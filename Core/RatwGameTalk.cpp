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

double Game::talkRichLine()
{
    const auto today = std::int64_t(std::floor(world_.calendarDays()));
    if (today == richLineDay_)
        return richLine_;
    richLineDay_ = today;
    const auto& society = world_.society();
    // (Households as talkSpeaker counts them: those sharing a home, or one alone in a communal one.)
    std::map<std::string, std::pair<std::string, std::vector<std::string>>> homes;
    for (const auto& [id, life] : society.state().residents)
        if (const auto* who = world_.entity(id); who && !who->dead)
        {
            auto& home = homes[Society::communalHome(life.homeCell) || life.homeCell.empty() ? "alone:" + id : life.homeCell];
            home.first = life.homeCell;
            home.second.push_back(id);
        }
    const auto reads = troubleReads();
    std::vector<double> days;
    for (const auto& [key, home] : homes)
        days.push_back(troubles::foodDays(home.second, home.first, reads));
    if (days.size() >= 20)
    {
        std::sort(days.begin(), days.end());
        richLine_ = std::max(42.0, days[days.size() * 19 / 20]);
        poorLine_ = std::max(double(Society::PoorDays), days[days.size() / 5]);
    }
    return richLine_;
}

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

    // Where they grew up and how they sound (phase 3): their dialect is a layer of their grammar, above plain. Their
    // row in live.npc_voice (phase 6) says otherwise where it says anything: origin, accent, quarter, traits, quirk, and
    // their own words, a layer above the dialect.
    std::string from, accent;
    world_.talkOrigin(e.id, from, accent);
    const auto voice = npcVoice_.find(e.id);
    if (voice != npcVoice_.end())
    {
        if (!voice->second.from.empty())
            from = voice->second.from;
        if (!voice->second.accent.empty())
            accent = voice->second.accent;
        if (!voice->second.quarter.empty())
            f["quarter"] = voice->second.quarter;
        if (!voice->second.quirk.empty())
            f["quirk"] = voice->second.quirk;
    }
    if (voiceLayers_.count(e.id))
        p.grammar.push_back(talk::Library::personalLayer(e.id));
    if (!from.empty())
    {
        f["from"] = from;
        f["accent"] = accent.empty() ? "light" : accent;
        p.grammar.push_back(from);
    }
    // Their people (decision 8), by name: to speak of ("#self.spouse#", "#self.priest#").
    for (const auto& [role, who] : peopleOf(e.id))
        if (const auto* them = world_.entity(who))
            f[role] = them->name;
    // Traits (decision 4): the middle of each axis unless their row says.
    static const std::map<std::string, std::string> Middle{{"temper", "even"}, {"talk", "plain"}, {"warmth", "reserved"},
                                                           {"humour", "none"}, {"honesty", "plain"}, {"piety", "ordinary"},
                                                           {"drive", "steady"}};
    for (const auto& [axis, middle] : Middle)
    {
        const auto set = voice == npcVoice_.end() ? nullptr : &voice->second.traits;
        f[axis] = set && set->count(axis) ? set->at(axis) : middle;
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
    // (Lines: under three days destitute; poor, the bottom fifth of the world's households (never under the society's
    // own poor line); rich, the top twentieth (never under six weeks' food). Standing is added below.)
    const double richLine = talkRichLine();
    int wealth = days < 3 ? 0 : days < poorLine_ ? 1 : days < richLine ? 2 : 3;
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
    // Standing (doc 62, phase 7; the user, 2026-10-13): food in hand doesn't measure a fortune. A great House's head
    // and the head's family are rich, whatever their own purse; the owner of a thriving business, and those who live
    // in the Heights or on an estate (not their servants), are comfortable at least; the Sump's are poor at most; the
    // homeless are destitute.
    for (const auto& h : society.houses())
    {
        const auto head = society.state().careers.positions.find(h.head);
        const auto& holder = head != society.state().careers.positions.end() ? head->second.holder : std::string();
        if ((post && post->id == h.head) || (!holder.empty() && (holder == e.id || society.family(e.id, holder))))
            wealth = 3;
    }
    const auto quarter = f.find("quarter");
    if (business == "thriving" ||
        (quarter != f.end() && (quarter->second == "heights" || quarter->second == "estate") && job != "servant"))
        wealth = std::max(wealth, 2);
    else if (quarter != f.end() && quarter->second == "sump")
        wealth = std::min(wealth, 1);           // (The Sump is poor, whatever's in the larder.)
    if (life->homeCell.empty() || job == "beggar")
        wealth = 0;
    static const char* const Wealth[] = {"destitute", "poor", "comfortable", "rich"};
    f["wealth"] = Wealth[wealth];
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
    // Neighbours, and those they deal with (decision 8): who buys from whom, who is whose priest.
    const auto near = neighboursOf(a);
    facts["pair.neighbours"] = yes(std::find(near.begin(), near.end(), b) != near.end());
    auto pa2 = peopleOf(a), pb2 = peopleOf(b);
    facts["pair.customer"] = pa2["shopkeeper"] == b ? "a" : pb2["shopkeeper"] == a ? "b" : "no";
    facts["pair.flock"] = pa2["priest"] == b ? "b" : pb2["priest"] == a ? "a" : "no";
    // Toward the other, player or resident (decision 9): a's knowledge of b, and a's feelings for b.
    facts["pair.named"] = yes(knowsName(a, b));
    if (const auto* k = known_.find(a, b))
        facts["pair.named_how"] = k->how;
    facts["pair.known_by"] = yes(knowsName(b, a));
    const Bond none{};
    const auto* bond = world_.bonds().find(a, b);
    const Bond& r = bond ? *bond : none;
    const auto number = [](double v) { return std::to_string(int(std::lround(v))); };
    facts["pair.affinity"] = number(r.affinity);
    facts["pair.trust"] = number(r.trust);
    facts["pair.respect"] = number(r.respect);
    facts["pair.familiarity"] = number(r.familiarity);
    facts["pair.fear"] = number(r.fear);
    // One word for it (placeholders: doc 62, "Players, as settled").
    facts["pair.disposition"] = r.affinity <= -50 || (r.affinity <= -25 && r.fear >= 30) ? "hostile"
                                : r.affinity <= -15 || r.trust <= -25                    ? "wary"
                                : r.affinity < -3                                        ? "cool"
                                : r.affinity >= 50 && r.familiarity >= 40                ? "fond"
                                : r.affinity >= 20                                       ? "warm"
                                                                                         : "neutral";
    if (bond && r.familiarity > 0)
        facts["pair.days_since"] = std::to_string(std::max(0, int(std::floor(world_.calendarDays() - r.lastContact))));
    facts["pair.owed"] = r.owed > 0 ? "b" : r.owed < 0 ? "a" : "no";
    std::string promise = "no";
    for (const auto& p : world_.promises())
        if (p.status == "open" && ((p.by == a && p.to == b) || (p.by == b && p.to == a)))
            promise = p.by == a ? "a" : "b";
    facts["pair.promise"] = promise;
    // The worst they believe of the other.
    static const std::vector<std::pair<const char*, const char*>> Rumours{{"violent", "violent"}, {"stole", "thief"},
                                                                        {"breaks promises", "breaks_promises"},
                                                                        {"keeps promises", "keeps_promises"},
                                                                        {"new in town", "newcomer"}};
    std::string rumour = "none";
    if (const auto* beliefs = world_.beliefsOf(a))
        for (const auto& [words, word] : Rumours)
            if (rumour == "none")
                for (const auto& belief : *beliefs)
                    if (belief.subject == b && belief.claim.find(words) != std::string::npos)
                    {
                        rumour = word;
                        break;
                    }
    facts["pair.rumour"] = rumour;
    if (const auto* other = world_.entity(b); other && !other->npc)
        facts["pair.famous"] = yes(!recognise(a, b, false).empty());
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
            // (Standing first: a House's lord is the richer of the two whatever is in the larder; doc 62, phase 7.)
            static const std::map<std::string, int> Rank{{"destitute", 0}, {"poor", 1}, {"comfortable", 2}, {"rich", 3}};
            const auto rank = [&](const talk::Speaker& p) {
                const auto w = p.facts.find("wealth");
                const auto r = w == p.facts.end() ? Rank.end() : Rank.find(w->second);
                return r == Rank.end() ? 1 : r->second;
            };
            const double x = std::stod(wa->second), y = std::stod(wb->second);
            const int ra = rank(q.a), rb = rank(q.b);
            q.facts["pair.richer"] = ra != rb ? (ra > rb ? "a" : "b") : x > y * 1.5 + 1 ? "a" : y > x * 1.5 + 1 ? "b" : "same";
        }
    }
    if (!q.alone)
        talkRecall("pair:" + std::min(a, b) + "|" + std::max(a, b), "pair.", q.facts);
    if (const auto region = q.facts.find("here.region"); region != q.facts.end())
        talkRecall("town:" + region->second, "world.", q.facts);
    // Figures not in the game yet (decision 8), known where the speaker is from, stands, or serves: by role.
    for (const auto& fig : figures_)
    {
        const auto& k = fig.knownIn;
        const auto of = [&](const talk::Facts& f, const char* key) {
            const auto found = f.find(key);
            return found == f.end() ? std::string("-") : found->second;
        };
        const bool known = k == "realm" || k == "region:" + of(q.a.facts, "from") || k == "region:" + of(q.facts, "here.region") ||
                           k == "house:" + of(q.a.facts, "house");
        if (known && !fig.role.empty())
            q.facts["world.figure_" + fig.role] = fig.name;
    }
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
        if (e.scope == "regard")
        {
            // A line that moves how one feels about the other (doc 62, decision 9): bounded as talk is.
            const auto bar = e.owner.find('|');
            BondChange change;
            const double n = std::atof(e.value.c_str());
            (e.key == "affinity" ? change.affinity : e.key == "trust" ? change.trust : e.key == "respect" ? change.respect
             : e.key == "fear" ? change.fear : change.familiarity) = n;
            if (bar != std::string::npos)
                regard(e.owner.substr(0, bar), e.owner.substr(bar + 1), change, "talk", "said " + e.why);
            continue;
        }
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
    talk::Facts facts;
    if (const auto* them = to.empty() ? nullptr : world_.entity(to); them && !them->npc && knowsName(npc, to))
        facts["topic.player"] = labelFor(npc, to);  // (Only a name they were given: doc 32.)
    const auto q = talkQuery(npc, to, concept, facts);
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

std::pair<std::string, Vec2> Game::entranceOf(const std::string& cell)
{
    if (world_.doors().size() != entranceDoors_)
    {
        entrances_.clear();
        entranceDoors_ = world_.doors().size();
    }
    if (const auto found = entrances_.find(cell); found != entrances_.end())
        return found->second;
    // Out through the door that leads in, and through the one before it (a flat upstairs, over a shop), to a street.
    std::pair<std::string, Vec2> at{cell, {}};
    std::string here = cell;
    for (int step = 0; step < 4; ++step)
    {
        const auto* c = world_.cell(here);
        if (!c || c->outdoors)
            break;
        const Door* in = nullptr;
        for (const auto& [id, d] : world_.doors())
            if (d.targetCell == here && d.cellId != here)
            {
                in = &d;
                break;
            }
        if (!in)
            break;
        at = {in->cellId, in->position};
        here = in->cellId;
    }
    return entrances_[cell] = at;
}

std::vector<std::string> Game::neighboursOf(const std::string& id)
{
    const auto& society = world_.society();
    const auto* life = society.resident(id);
    if (!life || life->homeCell.empty() || Society::communalHome(life->homeCell))
        return {};
    const auto mine = entranceOf(life->homeCell);
    // Households by how far their door is from ours: the same building is no distance at all.
    std::map<std::string, double> households;
    for (const auto& [other, theirs] : society.state().residents)
    {
        if (other == id || theirs.homeCell.empty() || theirs.homeCell == life->homeCell || Society::communalHome(theirs.homeCell))
            continue;
        if (const auto* e = world_.entity(other); !e || e->dead)
            continue;
        if (households.count(theirs.homeCell))
            continue;
        const auto at = entranceOf(theirs.homeCell);
        if (at.first != mine.first)
            continue;
        const double d = std::hypot(at.second.x - mine.second.x, at.second.y - mine.second.y);
        if (d <= 30)
            households[theirs.homeCell] = d;
    }
    std::vector<std::pair<double, std::string>> nearest;
    for (const auto& [home, d] : households)
        nearest.push_back({d, home});
    std::sort(nearest.begin(), nearest.end());
    if (nearest.size() > 5)
        nearest.resize(5);
    std::vector<std::string> out;
    for (const auto& [d, home] : nearest)
        for (const auto& [other, theirs] : society.state().residents)
            if (theirs.homeCell == home)
                if (const auto* e = world_.entity(other); e && !e->dead)
                    out.push_back(other);
    return out;
}

std::map<std::string, std::string> Game::peopleOf(const std::string& id)
{
    std::map<std::string, std::string> out;
    const auto& society = world_.society();
    const auto living = [&](const std::string& who) {
        const auto* e = world_.entity(who);
        return e && !e->dead;
    };
    if (const auto* s = society.spouse(id); s && living(*s))
        out["spouse"] = *s;
    int youngest = 1000;
    for (const auto& [child, parents] : society.state().careers.parents)
    {
        if (std::find(parents.begin(), parents.end(), id) != parents.end() && living(child) && world_.entity(child)->age < youngest)
            youngest = world_.entity(child)->age, out["child"] = child;
        if (child == id)
            for (const auto& p : parents)
                if (living(p))
                    out[world_.entity(p)->appearance->sex == "female" ? "mother" : "father"] = p;
    }
    if (const auto n = neighboursOf(id); !n.empty())
        out["neighbour"] = n.front();
    // Those they deal with in their town: the nearest shopkeeper to their door, their priest, their leader.
    const auto* life = society.resident(id);
    const auto* e = world_.entity(id);
    if (!life || !e)
        return out;
    const auto town = world_.communityOf(life->homeCell.empty() ? e->cellId : life->homeCell);
    const auto door = entranceOf(life->homeCell.empty() ? e->cellId : life->homeCell);
    double nearest = 1e18;
    for (const auto& p : society.positions())
    {
        const auto held = society.state().careers.positions.find(p.id);
        if (held == society.state().careers.positions.end() || held->second.holder.empty() || held->second.holder == id ||
            !living(held->second.holder) || world_.communityOf(p.work.cell) != town)
            continue;
        const auto& holder = held->second.holder;
        if (p.role == "merchant")
        {
            const auto at = entranceOf(p.work.cell);
            const double d = at.first == door.first ? std::hypot(at.second.x - door.second.x, at.second.y - door.second.y) : 1e9;
            if (d < nearest)
                nearest = d, out["shopkeeper"] = holder;
        }
        if (Society::clergy(p.title) && !out.count("priest"))
            out["priest"] = holder;
    }
    // Their leader: their own House's head, or the head of a House of their town.
    for (const auto& h : society.houses())
    {
        const auto head = society.state().careers.positions.find(h.head);
        if (head == society.state().careers.positions.end() || head->second.holder.empty() || head->second.holder == id ||
            !living(head->second.holder))
            continue;
        const auto* post = society.jobOf(id);
        const bool own = (post && society.ownerOf(post->id) == h.id) || society.family(id, head->second.holder);
        if (own || (!out.count("leader") && h.community == town))
            out["leader"] = head->second.holder;
        if (own)
            break;
    }
    return out;
}

BondChange Game::regard(const std::string& holder, const std::string& other, BondChange change, const std::string& source,
                        const std::string& reason, double lift)
{
    // Each source's bounds: a change at most `each`, at most `per` in each axis in its window (an hour, or a day).
    struct Bounds
    {
        double each, per;
        bool daily;
    };
    static const std::map<std::string, Bounds> Sources{{"model", {3, 6, false}}, {"talk", {1, 3, false}}, {"analysis", {5, 10, true}},
                                                       {"dm", {200, 1e9, true}}};
    const auto bounds = Sources.count(source) ? Sources.at(source) : Bounds{1, 3, false};
    auto& budget = regardBudget_[holder + "|" + other + "|" + source];
    const auto window = std::int64_t(std::floor(world_.calendarDays() * (bounds.daily ? 1 : 24)));
    if (budget.window != window)
        budget = {window, {0, 0, 0, 0, 0}};
    double* axes[5] = {&change.affinity, &change.trust, &change.familiarity, &change.fear, &change.respect};
    bool any = false;
    for (int i = 0; i < 5; ++i)
    {
        const double room = std::max(0.0, bounds.per - budget.used[i]);
        *axes[i] = std::clamp(*axes[i], -std::min(bounds.each, room), std::min(bounds.each, room));
        budget.used[i] += std::abs(*axes[i]);
        if (*axes[i] > 0 && (i == 0 || i == 1))
            *axes[i] *= lift;                       // (First impressions, doc 55: the model's warmth only.)
        any |= *axes[i] != 0;
    }
    if (!any)
        return change;
    world_.bonds().change(holder, other, change, world_.calendarDays());
    auto& ledger = regardLedger_[holder + "|" + other];
    ledger.push_back({world_.calendarDays(), source, reason.substr(0, 160), change});
    while (ledger.size() > 12)
        ledger.pop_front();
    return change;
}

const std::deque<Game::RegardEntry>* Game::regardLedger(const std::string& holder, const std::string& other) const
{
    const auto found = regardLedger_.find(holder + "|" + other);
    return found == regardLedger_.end() ? nullptr : &found->second;
}

void Game::loadVoices()
{
    if (options_.database.empty() || liveWorldId_.empty())
        return;
    std::string problem;
    int dialects = 0, refused = 0;
    // The dialects (decision 7): the database's over the files'.
    if (const auto rows = worldDb_.exec("SELECT name, grammar FROM live.dialects WHERE world_id = $1 ORDER BY position, name",
                                        {liveWorldId_});
        rows.ok)
        for (const auto& row : rows.rows)
            if (row.size() >= 2 && row[0] && row[1])
            {
                if (talk_.setGrammar(*row[0], *row[1], problem))
                    ++dialects;
                else
                    ++refused, note("warn", "RATW_TALK the dialect " + *row[0] + " was refused, the file's kept:\n" + problem);
            }
    // Each resident's voice (decisions 4 to 6, 10).
    std::map<std::string, NpcVoice> voices;
    std::set<std::string> layers, rules;
    if (const auto rows = worldDb_.exec("SELECT npc_id, home_region, accent, quarter, traits::text, quirk, persona, grammar, rules "
                                        "FROM live.npc_voice WHERE world_id = $1", {liveWorldId_});
        rows.ok)
        for (const auto& row : rows.rows)
        {
            if (row.size() < 9 || !row[0])
                continue;
            const auto& id = *row[0];
            const auto text = [&](std::size_t i) { return row[i] ? *row[i] : std::string(); };
            auto& v = voices[id];
            v.from = text(1), v.accent = text(2), v.quarter = text(3), v.quirk = text(5), v.persona = text(6);
            json::Value traits;
            std::string error;
            if (json::parse(text(4), traits, error) && traits.isObject())
                for (const auto& [axis, value] : traits.fields())
                    if (value.isString())
                        v.traits[axis] = value.asString("");
            // Their own words and lines: a person's layer, and rules only they may say.
            if (const auto grammar = text(7); !grammar.empty())
            {
                const auto layer = talk::Library::personalLayer(id);
                if (talk_.setGrammar(layer, "grammar " + layer + "\n" + grammar, problem))
                    layers.insert(id);
                else
                    ++refused, note("warn", "RATW_TALK " + id + "'s own words were refused:\n" + problem);
            }
            if (const auto own = text(8); !own.empty())
            {
                if (talk_.setCharacter(id, own, problem))
                    rules.insert(id);
                else
                    ++refused, note("warn", "RATW_TALK " + id + "'s own lines were refused:\n" + problem);
            }
        }
    // Those whose words or lines are gone since the last load lose them.
    for (const auto& id : voiceLayers_)
        if (!layers.count(id))
            talk_.setGrammar(talk::Library::personalLayer(id), "grammar " + talk::Library::personalLayer(id) + "\n", problem);
    for (const auto& id : voiceRules_)
        if (!rules.count(id))
            talk_.setCharacter(id, "", problem);
    npcVoice_ = std::move(voices);
    voiceLayers_ = std::move(layers);
    voiceRules_ = std::move(rules);
    // Figures (decision 8).
    figures_.clear();
    if (const auto rows = worldDb_.exec("SELECT id, name, title, role, known_in, about, coalesce(npc_id, '') FROM live.figures "
                                        "WHERE world_id = $1 ORDER BY position, id", {liveWorldId_});
        rows.ok)
        for (const auto& row : rows.rows)
            if (row.size() >= 7 && row[0] && row[1])
                figures_.push_back({*row[0], *row[1], row[2] ? *row[2] : "", row[3] ? *row[3] : "", row[4] ? *row[4] : "realm",
                                    row[5] ? *row[5] : "", row[6] ? *row[6] : ""});
    note("info", "RATW_TALK voices: " + std::to_string(npcVoice_.size()) + " residents (" + std::to_string(voiceLayers_.size()) +
                     " with words of their own, " + std::to_string(voiceRules_.size()) + " with lines), " + std::to_string(dialects) +
                     " dialects, " + std::to_string(figures_.size()) + " figures" + (refused ? ", " + std::to_string(refused) + " refused" : ""));
}
} // namespace ratw::game
