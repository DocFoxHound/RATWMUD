// What the talk library knows of a resident and of two together (Docs/Design/62-talk.md, phase 2): read from the
// world as it is now, every time someone is about to speak, and never stored. The facts and their values are listed in
// Data/Talk/facts.json; the appendix of doc 62 says where each comes from.
#include "RatwGame.h"
#include "RatwCalendar.h"
#include "RatwTroubles.h"

#include <algorithm>
#include <cmath>

namespace ratw::game
{
namespace
{
const char* yes(bool b) { return b ? "yes" : "no"; }

bool within(double start, double end, double hour)
{
    return start <= end ? hour >= start && hour < end : hour >= start || hour < end;   // (A shift may run past midnight.)
}
} // namespace

talk::Speaker Game::talkSpeaker(const Entity& e)
{
    talk::Speaker p;
    const auto& society = world_.society();
    const auto* spec = society.spec(e.id);
    const auto* post = society.jobOf(e.id);
    const auto* life = society.resident(e.id);
    auto role = spec ? spec->role : std::string("civilian");
    if (role != "merchant" && role != "guard")
        role = "civilian";
    const auto job = talk::jobCategory(post ? post->title : std::string(), spec ? spec->workLabel : std::string(), role, e.age);
    auto& f = p.facts;
    f = {{"id", e.id},
         {"name", e.name},
         {"sex", e.appearance->sex == "female" ? "female" : "male"},
         {"stage", lifeStageName(lifeStage(e.age))},
         {"age", std::to_string(e.age)},
         {"job", job},
         {"role", role}};
    if (job != "none")
        f["trade"] = job;
    talkRecall("person:" + e.id, "", f);            // (What rules remembered of them: phase 4.)
    if (!e.npc || !life)
        return p;                                   // (A player, or someone passing: who they are, and no more.)

    // Where they grew up and how they sound (phase 3): their dialect is a layer of their grammar, above plain.
    std::string from, accent;
    world_.talkOrigin(e.id, from, accent);
    if (!from.empty())
    {
        f["from"] = from;
        f["accent"] = accent;
        p.grammar.push_back(from);
    }

    // Means: the household's money and food in days of plain food for everyone in it (troubles::foodDays, as the
    // troubles read it), the dole's own test, debts, and how their business does.
    std::vector<std::string> household;
    if (Society::communalHome(life->homeCell) || life->homeCell.empty())
        household.push_back(e.id);
    else
        for (const auto& [id, other] : society.state().residents)
            if (other.homeCell == life->homeCell)
                if (const auto* o = world_.entity(id); o && !o->dead)
                    household.push_back(id);
    std::int64_t held = 0;
    const double days = troubles::foodDays(household, life->homeCell, troubleReads(), &held);
    f["wealth_days"] = std::to_string(int(std::floor(days)));
    // (Lines: under three days destitute; the society's own poor line; six weeks' food in hand is rich, about the top
    // twentieth of DEV's households when this was written. Placeholders, to tune by ear.)
    f["wealth"] = days < 3 ? "destitute" : days < Society::PoorDays ? "poor" : days < 42 ? "comfortable" : "rich";
    std::int64_t cash = 0;
    for (const auto& id : household)
        if (const auto* a = society.account(id))
            cash += a->cash;
    f["on_dole"] = yes(cash < std::int64_t(Society::PoorDays * Society::FoodADay * household.size()));
    bool owes = false;
    if (const auto* bonds = world_.bonds().of(e.id))
        for (const auto& [other, b] : *bonds)
            owes |= b.owed < 0;
    std::string business = "none";
    if (post && society.ownsTill(*post))
    {
        const auto till = society.tillOf(e.id);
        const bool loan = society.state().memory.loans.count(till) > 0;
        owes |= loan;
        const auto* account = society.account(till);
        const auto money = account ? account->cash : 0;
        const auto floor = society.floatOf(post->id);
        const auto propped = society.state().houses.propped.find(post->id);
        const bool low = propped != society.state().houses.propped.end() && propped->second.size() >= 3;
        business = loan || low || money < floor / 2 ? "failing" : money >= floor * 2 ? "thriving" : "steady";
    }
    f["owes"] = yes(owes);
    f["business"] = business;
    f["hungry"] = yes(life->hunger >= 55);
    f["homeless"] = yes(life->homeCell.empty() || job == "beggar");

    // Body and mood.
    f["injured"] = e.hurt >= 50 || e.downedLeft > 0 ? "bad" : e.hurt >= 1 ? "light" : "none";
    f["tired"] = yes(life->fatigue >= 60);
    std::string mourning = "no";
    if (const auto* m = society.mourning(e.id); m && m->until > world_.calendarDays())
        mourning = society.family(e.id, m->whom) ? "kin" : "friend";
    f["mourning"] = mourning;
    f["fighting"] = yes(world_.inBattle(e.id));
    bool fought = false, robbed = false;
    for (const auto& i : world_.crime().incidents)
    {
        if (world_.calendarDays() - i.day > 3)
            continue;
        fought |= i.kind == "assault" && (i.offender == e.id || i.victim == e.id) && world_.calendarDays() - i.day <= 2;
        robbed |= i.kind == "theft" && i.victim == e.id;
    }
    f["fought"] = yes(fought);
    f["robbed"] = yes(robbed);

    // Doing: at work or not, where in their working day, on the road, on an errand.
    const auto& task = life->task;
    static const std::set<std::string> Working{"trade", "patrol", "watch", "preaching", "selling",
                                               "shopping for the household", "minding the children"};
    const bool working = (post && task == post->title) || Working.count(task) > 0 || task.rfind("odd job", 0) == 0;
    f["working"] = yes(working);
    std::string shift = "none";
    if (post)
    {
        const auto plan = world_.dayPlan(world_.communityOf(e.cellId));
        const double hour = calendar::calendarAt(world_.calendarDays()).hour;
        const double start = post->startHour, end = post->endHour;
        if (role != "guard" && (plan.kind == "rest" || plan.kind == "festival"))
            shift = "day_off";
        else if (within(start, end, hour))
            shift = "during";
        else
        {
            // Off work: nearer the last shift's end (after) or the next one's start (before).
            const double since = std::fmod(hour - end + 24, 24), until = std::fmod(start - hour + 24, 24);
            shift = since < until ? "after" : "before";
        }
    }
    f["shift"] = shift;
    f["on_watch"] = yes(world_.guardOnDuty(e.id));
    f["travelling"] = yes((post && task == post->title && life->reason.rfind("On the road", 0) == 0) || task == "relocate" ||
                          task == "companion");
    static const std::vector<std::pair<const char*, const char*>> Errands{
        {"carrying a letter", "letter"}, {"guarding a caravan", "caravan"}, {"fetching goods", "goods"},
        {"delivering goods", "goods"},   {"renegotiating", "order"},          {"at a wedding", "wedding"},
        {"at a funeral", "funeral"}};
    std::string errand = "none";
    for (const auto& [prefix, kind] : Errands)
        if (e.activity.rfind(prefix, 0) == 0)
            errand = kind;
    f["errand"] = errand;

    // Who they are to others: married, widowed, a parent, learning a trade or teaching one, of a House.
    const auto* spouse = society.spouse(e.id);
    const auto* partner = spouse ? world_.entity(*spouse) : nullptr;
    f["married"] = yes(spouse && partner && !partner->dead);
    f["widowed"] = yes(spouse && (!partner || partner->dead));
    bool parent = false;
    for (const auto& [child, parents] : society.state().careers.parents)
        parent |= std::find(parents.begin(), parents.end(), e.id) != parents.end();
    f["parent"] = yes(parent);
    f["apprentice"] = yes(society.apprenticedTo(e.id) != nullptr);
    bool master = false;
    if (post)
        if (const auto state = society.state().careers.positions.find(post->id); state != society.state().careers.positions.end())
            master = !state->second.apprentice.empty();
    f["master"] = yes(master);
    // A House's: its head, kin of its head, or working at one of its businesses.
    for (const auto& h : society.houses())
    {
        const auto head = society.state().careers.positions.find(h.head);
        const auto& headHolder = head != society.state().careers.positions.end() ? head->second.holder : std::string();
        const bool of = (post && post->id == h.head) || (!headHolder.empty() && society.family(e.id, headHolder)) ||
                        (post && society.ownerOf(post->id) == h.id);
        if (of)
        {
            f["house"] = h.id.substr(h.id.find(':') + 1);
            break;
        }
    }
    const auto tone = voices_.toneOf(spec ? spec->personality : std::string());
    f["tone"] = tone.empty() ? "plain" : tone;      // (The router's tones; plain without the router, or a personality.)
    return p;
}

std::string Game::voiceCardOf(const std::string& id)
{
    const auto* e = world_.entity(id);
    return e && e->npc ? talk_.voiceCard(talkSpeaker(*e)) : std::string();
}

void Game::talkPair(const std::string& a, const std::string& b, talk::Facts& facts)
{
    // The two of them: who they are to each other, kin, who works for whom, who is the better off.
    const auto& society = world_.society();
    facts["pair.band"] = world_.pairBand(a, b);
    std::string kin = "no", elder;
    const auto& parents = society.state().careers.parents;
    const auto parentsOf = [&](const std::string& id) {
        const auto found = parents.find(id);
        return found == parents.end() ? std::vector<std::string>{} : found->second;
    };
    const auto pa = parentsOf(a), pb = parentsOf(b);
    if (const auto* s = society.spouse(a); s && *s == b)
        kin = "spouses";
    else if (std::find(pb.begin(), pb.end(), a) != pb.end())
        kin = "parent", elder = "a";
    else if (std::find(pa.begin(), pa.end(), b) != pa.end())
        kin = "parent", elder = "b";
    else if (std::any_of(pa.begin(), pa.end(), [&](const std::string& p) { return std::find(pb.begin(), pb.end(), p) != pb.end(); }))
        kin = "siblings";
    else if (society.family(a, b))
        kin = "family";
    else if (society.household(a, b))
        kin = "household";
    facts["pair.kin"] = kin;
    if (!elder.empty())
        facts["pair.elder"] = elder;
    // Who pays whom: a till's keeper, a household's purse, a House's head.
    const auto paidBy = [&](const std::string& worker, const std::string& boss) {
        const auto* job = society.jobOf(worker);
        const auto* e = world_.entity(worker);
        if (!job || !e)
            return false;
        const auto payer = society.payerOf(worker, *job, e->age);
        if (payer.account == boss)
            return true;
        if (payer.account.rfind("till:", 0) == 0)
            if (const auto s = society.state().careers.positions.find(payer.account.substr(5)); s != society.state().careers.positions.end())
                return s->second.holder == boss && boss != worker;
        return false;
    };
    facts["pair.boss"] = paidBy(b, a) ? "a" : paidBy(a, b) ? "b" : "no";
}

talk::Query Game::talkQuery(const std::string& a, const std::string& b, const std::string& concept,
                            const talk::Facts& facts)
{
    talk::Query q;
    q.concept = concept;
    q.alone = b.empty();
    world_.talkMoment(a, q.facts);
    world_.talkTown(a, q.facts);
    if (const auto* e = world_.entity(a))
        q.a = talkSpeaker(*e);
    if (const auto* e = q.alone ? nullptr : world_.entity(b))
    {
        q.b = talkSpeaker(*e);
        talkPair(a, b, q.facts);
        if (q.a.facts.count("from") && q.b.facts.count("from"))
            q.facts["pair.same_from"] = q.a.facts["from"] == q.b.facts["from"] ? "yes" : "no";
        const auto wa = q.a.facts.find("wealth_days"), wb = q.b.facts.find("wealth_days");
        if (wa != q.a.facts.end() && wb != q.b.facts.end())
        {
            const double x = std::stod(wa->second), y = std::stod(wb->second);
            q.facts["pair.richer"] = x > y * 1.5 + 1 ? "a" : y > x * 1.5 + 1 ? "b" : "same";
        }
    }
    if (!q.alone)
        talkRecall("pair:" + std::min(a, b) + "|" + std::max(a, b), "pair.", q.facts);
    if (const auto region = q.facts.find("here.region"); region != q.facts.end())
        talkRecall("town:" + region->second, "world.", q.facts);
    for (const auto& [k, v] : facts)
        if (!v.empty())
            q.facts[k] = v;
    return q;
}

void Game::talkRemember(const std::vector<talk::Effect>& effects)
{
    const double now = world_.calendarDays();
    for (const auto& e : effects)
    {
        auto& memos = talkMemory_[e.scope + ":" + e.owner];
        memos[e.key] = {e.value, e.days < 0 ? -1 : now + e.days};
        while (memos.size() > 32)
        {
            // The soonest to go goes first (those kept for good last).
            auto soonest = memos.begin();
            for (auto it = memos.begin(); it != memos.end(); ++it)
                if (it->second.until >= 0 && (soonest->second.until < 0 || it->second.until < soonest->second.until))
                    soonest = it;
            memos.erase(soonest);
        }
    }
}

void Game::talkRecall(const std::string& owner, const std::string& prefix, talk::Facts& into)
{
    const auto found = talkMemory_.find(owner);
    if (found == talkMemory_.end())
        return;
    const double now = world_.calendarDays();
    for (auto it = found->second.begin(); it != found->second.end();)
    {
        if (it->second.until >= 0 && it->second.until < now)
        {
            it = found->second.erase(it);
            continue;
        }
        into[prefix + (it->first.rfind("said:", 0) == 0 ? it->first : "mem." + it->first)] = it->second.value;
        ++it;
    }
    if (found->second.empty())
        talkMemory_.erase(found);
}

bool Game::react(const std::string& npc, const std::string& to, const std::string& concept)
{
    // One resident's reaction, if the library has something for it: said at once (one line), or, for two (a friend's
    // greeting and its answer), as an exchange the ambient voice paces.
    const double now = world_.time();
    const auto* e = world_.entity(npc);
    if (!e || !e->npc || e->dead || e->offstage || e->speakingUntil > now || e->posture == "lying" || pendingNpc_.count(npc) ||
        !talk_.has(concept))
        return false;
    for (const auto& t : ambient_)
        if (t.pick.teller == npc || t.pick.listener == npc || (!to.empty() && (t.pick.teller == to || t.pick.listener == to)))
            return false;
    const auto q = talkQuery(npc, to, concept);
    const auto said = talkPick(q, npc, "react:" + e->cellId, 4, std::uint64_t(now * 1000) ^ std::hash<std::string>{}(npc + concept), false);
    if (said.lines.empty())
        return false;
    if (said.lines.size() == 1)
    {
        ParsedPost post;
        post.ok = true;
        post.speech = true;
        post.segments.push_back({"speech", said.lines.front().second});
        publish(npc, post, Voice::Speak);
        npcLastSpeech_[npc] = now;
    }
    else
    {
        AmbientTalk talking;
        talking.id = ambientNext_++;
        talking.pick.teller = npc;
        talking.pick.listener = to;
        talking.pick.cell = e->cellId;
        talking.pick.topic.kind = concept;
        talking.lines.assign(said.lines.begin(), said.lines.end());
        talking.voiced = true;
        talking.nextAt = now;
        ambient_.push_back(talking);
    }
    voiced("react", "scene", npc);
    return true;
}

void Game::reactions(double dt)
{
    // Every two seconds, where players are: what has happened since the last look, and someone there reacting to it.
    if ((reactLookIn_ += dt) < 2)
        return;
    reactLookIn_ = 0;
    const double now = world_.time();
    std::set<std::string> watched;
    for (const auto* c : clients_)
        if (const auto* p = world_.entity(c->entityId); p && !p->dead)
            watched.insert(p->cellId);
    if (watched.empty())
        return;
    const auto due = [&](const std::string& key, double every) {
        const auto last = reactLast_.find(key);
        if (last != reactLast_.end() && now - last->second < every)
            return false;
        reactLast_[key] = now;
        return true;
    };
    // Someone in the place, not in `besides`, close enough to `at` to see it; the nearest first.
    const auto bystander = [&](const std::string& cell, Vec2 at, const std::set<std::string>& besides, double reach) {
        std::string best;
        double nearest = reach;
        for (const auto* o : world_.entitiesIn(cell))
            if (o && o->npc && !o->dead && !o->offstage && !besides.count(o->id) && world_.society().resident(o->id))
            {
                const double d = std::hypot(o->position.x - at.x, o->position.y - at.y);
                if (d < nearest)
                    nearest = d, best = o->id;
            }
        return best;
    };
    // A fight: one bystander remarks on it now and then; a fighter cries out when struck.
    for (const auto& b : world_.battles())
    {
        if (b.over || !watched.count(b.cellId))
            continue;
        std::set<std::string> in(b.observers.begin(), b.observers.end());
        for (const auto& f : b.fighters)
        {
            in.insert(f.id);
            const auto* e = world_.entity(f.id);
            if (!e || !e->npc)
                continue;
            auto& was = reactHurt_[f.id];
            if (e->hurt > was + 1 && due("struck:" + f.id, 20))
                react(f.id, {}, "react.struck");
            was = e->hurt;
        }
        const Vec2 middle{b.x0 + b.w / 2.0, b.y0 + b.h / 2.0};
        if (due("fight:" + b.id, 45))
            if (const auto who = bystander(b.cellId, middle, in, 16); !who.empty())
                react(who, {}, "react.fight");
    }
    for (auto it = reactHurt_.begin(); it != reactHurt_.end();)
        it = world_.inBattle(it->first) ? std::next(it) : reactHurt_.erase(it);
    // A theft, just now, where someone could see.
    double newest = reactIncidentTime_;
    for (const auto& i : world_.crime().incidents)
    {
        newest = std::max(newest, i.time);
        if (reactIncidentTime_ < 0 || i.time <= reactIncidentTime_ || i.kind.find("theft") == std::string::npos || !watched.count(i.cell))
            continue;
        const auto* victim = world_.entity(i.victim);
        const auto* offender = world_.entity(i.offender);
        const auto* near = victim ? victim : offender;
        if (near && due("theft:" + i.cell, 10))
            if (const auto who = bystander(i.cell, near->position, {i.offender}, 14); !who.empty())
                react(who, {}, "react.theft");
    }
    reactIncidentTime_ = newest;                    // (The first look only notes where things stand.)
    // Rain starting, outdoors.
    for (const auto& cellId : watched)
    {
        const auto* c = world_.cell(cellId);
        if (!c || !c->outdoors)
            continue;
        const auto before = reactWeather_.find(cellId);
        const bool wet = c->weather == Weather::Rain || c->weather == Weather::Storm;
        const bool started = before != reactWeather_.end() && wet && before->second != Weather::Rain && before->second != Weather::Storm;
        reactWeather_[cellId] = c->weather;
        if (started && due("rain:" + cellId, 120))
            for (const auto* client : clients_)
                if (const auto* p = world_.entity(client->entityId); p && p->cellId == cellId)
                {
                    if (const auto who = bystander(cellId, p->position, {}, 14); !who.empty())
                        react(who, {}, "react.rain");
                    break;
                }
    }
    for (auto it = reactWeather_.begin(); it != reactWeather_.end();)
        it = watched.count(it->first) ? std::next(it) : reactWeather_.erase(it);
    // A customer: a player walks into the room where someone keeps shop, who is there and awake.
    for (const auto* client : clients_)
    {
        const auto* p = world_.entity(client->entityId);
        if (!p || p->dead)
            continue;
        auto& last = reactPlayerCell_[p->id];
        const bool entered = !last.empty() && last != p->cellId;
        last = p->cellId;
        if (!entered)
            continue;
        for (const auto* o : world_.entitiesIn(p->cellId))
        {
            if (!o || !o->npc || o->dead)
                continue;
            // Their shop: where a merchant works, or a post's place of work.
            const auto* post = world_.society().jobOf(o->id);
            const auto* spec = world_.society().spec(o->id);
            const bool keeps = (post && post->work.cell == p->cellId) || (spec && spec->role == "merchant" && spec->work.cell == p->cellId);
            const auto* life = world_.society().resident(o->id);
            if (keeps && life && life->task != "sleep" && due("customer:" + o->id + "|" + p->id, 60))
            {
                react(o->id, p->id, "react.customer");
                break;
            }
        }
    }
    // Two friends passing each other, near a player.
    for (const auto& cellId : watched)
    {
        const auto& here = world_.entitiesIn(cellId);
        for (std::size_t i = 0; i < here.size(); ++i)
            for (std::size_t j = i + 1; j < here.size(); ++j)
            {
                const auto* x = here[i];
                const auto* y = here[j];
                if (!x || !y || !x->npc || !y->npc || x->dead || y->dead || !world_.society().resident(x->id) ||
                    !world_.society().resident(y->id) || std::hypot(x->position.x - y->position.x, x->position.y - y->position.y) > 3)
                    continue;
                const auto pair = std::min(x->id, y->id) + "|" + std::max(x->id, y->id);
                if (world_.pairBand(x->id, y->id) == "friends" && due("friend:" + pair, 1800))
                    react(x->id, y->id, "react.friend");
            }
    }
}
} // namespace ratw::game
