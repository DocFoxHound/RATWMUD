// What the talk library knows of a resident and of two together (Docs/Design/62-talk.md, phase 2): read from the
// world as it is now, every time someone is about to speak, and never stored. The facts and their values are listed in
// Data/Talk/facts.json; the appendix of doc 62 says where each comes from.
#include "RatwGame.h"
#include "RatwCalendar.h"
#include "RatwItems.h"
#include "RatwTroubles.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>

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
    f["first"] = e.name.substr(0, e.name.find(' '));   // (How one introduces oneself: doc 84 §7.11, Phase 5.)
    if (job != "none")
        f["trade"] = job;
    talkRecall("person:" + e.id, "", f);            // (What rules remembered of them: phase 4.)
    p.player = !e.npc;                              // (Never given a line by the engine: doc 84 §13, Slice 2's review.)
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
    // Their mantle (doc 84 §9.4, Phase 9): its words a layer between their own and their dialect, its lines theirs.
    const talk::Mantle* worn = nullptr;
    if (const auto m = mantleOf_.find(e.id); m != mantleOf_.end())
        if ((worn = talk_.mantle(m->second)))
        {
            f["mantle"] = worn->id;
            p.grammar.push_back(talk::mantleLayer(worn->id));
        }
    if (!from.empty())
    {
        f["from"] = from;
        f["accent"] = accent.empty() ? "light" : accent;
        p.grammar.push_back(talk_.dialectOf(from));   // (A town with no dialect of its own speaks another's: doc 83.)
    }
    // Their people (decision 8), by name: to speak of ("#self.spouse#", "#self.priest#").
    for (const auto& [role, who] : peopleOf(e.id))
        if (const auto* them = world_.entity(who))
        {
            f[role] = them->name;
            f[role + "_sex"] = them->appearance->sex == "female" ? "female" : "male";   // (#self.spouse_their#: Phase 6.)
        }
    // §11's slots (doc 84, Phase 10): the surname, husband or wife, where they grew up by name and whether that is here,
    // and the grammar they speak.
    if (const auto space = e.name.find_last_of(' '); space != std::string::npos)
        f["surname"] = e.name.substr(space + 1);
    if (const auto s = f.find("spouse_sex"); s != f.end())
        f["spouse_word"] = s->second == "female" ? "wife" : "husband";
    if (!from.empty())
    {
        auto town = from;
        std::replace(town.begin(), town.end(), '_', ' ');
        for (std::size_t i = 0; i < town.size(); ++i)
            if (i == 0 || town[i - 1] == ' ')
                town[i] = char(std::toupper(static_cast<unsigned char>(town[i])));
        f["from_town"] = town;
        f["away"] = yes(from != world_.townIdOf(life->homeCell.empty() ? e.cellId : life->homeCell));
        const auto d = talk_.dialectOf(from);
        static const std::set<std::string> Dialects{"coldwater", "frosthollow", "larkfield", "mirebridge", "oakhaven", "southmeadow", "saltwell"};
        f["dialect"] = Dialects.count(d) ? d : std::string("plain");
    }
    // Traits (decision 4): the middle of each axis unless their row says.
    static const std::map<std::string, std::string> Middle{{"temper", "even"}, {"talk", "plain"}, {"warmth", "reserved"},
                                                           {"humour", "none"}, {"honesty", "plain"}, {"piety", "ordinary"},
                                                           {"drive", "steady"}};
    for (const auto& [axis, middle] : Middle)
    {
        const auto set = voice == npcVoice_.end() ? nullptr : &voice->second.traits;
        f[axis] = set && set->count(axis) ? set->at(axis) : middle;
    }
    // The mantle's talk-only traits and its quirk over the seeded ones; never honesty, which drives who turns thief and
    // stays the placed resident's own (§9.5; a newborn takes the card's whole: Phase 10).
    if (worn)
    {
        for (const auto& [axis, value] : worn->traits)
            if (axis != "honesty" && Middle.count(axis))
                f[axis] = value;
        if (!worn->quirk.empty())
            f["quirk"] = worn->quirk;
    }

    // Means (doc 79, 11; Phase 8): the household's band, worked out once a day with its guarantees (6.3), and class a
    // fact beside it: no scan of every household for the lines. A resident in no band (a world without the staged
    // households: a demo roster) falls back on its household's food and money in days (troubles::foodDays,
    // `wealth_days`) against the society's lines: under three days destitute, under a week poor, six weeks rich.
    const auto* standing = society.statusOf(e.id);
    int wealth = 2;
    if (standing)
    {
        wealth = std::clamp(standing->band, 0, 3);
        f["class"] = standing->cls == "homeless" ? std::string("lower") : standing->cls;
    }
    else
    {
        std::vector<std::string> household;
        if (Society::communalHome(life->homeCell) || life->homeCell.empty())
            household.push_back(e.id);
        else
            for (const auto& [id, other] : society.state().residents)
                if (other.homeCell == life->homeCell)
                    if (const auto* o = world_.entity(id); o && !o->dead)
                        household.push_back(id);
        const double days = troubles::foodDays(household, life->homeCell, troubleReads());
        f["wealth_days"] = std::to_string(int(std::floor(days)));
        wealth = days < 3 ? 0 : days < 7 ? 1 : days < 42 ? 2 : 3;
    }
    // The Restday dole and the shop's business (doc 79, 10.2-10.3; Phase 6): from the society's street life.
    f["on_dole"] = yes(society.onDole(e.id));
    f["lifted"] = yes(society.liftedNow(e.id));    // (Living on a player's help: 6.7, T15. Phase 8 repair.)
    bool owes = false;
    if (const auto* bonds = world_.bonds().of(e.id))
        for (const auto& [other, b] : *bonds)
            owes |= b.owed < 0;
    const std::string business = society.businessOf(e.id);
    f["owes"] = yes(owes);
    f["business"] = business;
    // Standing for one in no band (doc 62, phase 7; the user, 2026-10-13): a great House's head and the head's family are
    // rich; those who live in the Heights or on an estate (not their servants) comfortable at least; the Sump's poor at
    // most; the homeless destitute. (A band has these as its guarantees: doc 79, 6.3.)
    for (const auto& h : society.houses())
    {
        if (standing)
            break;
        const auto head = society.state().careers.positions.find(h.head);
        const auto& holder = head != society.state().careers.positions.end() ? head->second.holder : std::string();
        if ((post && post->id == h.head) || (!holder.empty() && (holder == e.id || society.family(e.id, holder))))
            wealth = 3;
    }
    const auto quarter = f.find("quarter");
    if (!standing && quarter != f.end() && (quarter->second == "heights" || quarter->second == "estate") && job != "servant")
        wealth = std::max(wealth, 2);
    else if (!standing && quarter != f.end() && quarter->second == "sump")
        wealth = std::min(wealth, 1);           // (The Sump is poor, whatever's in the larder.)
    if (!standing && (life->homeCell.empty() || job == "beggar"))
        wealth = 0;
    static const char* const Wealth[] = {"destitute", "poor", "comfortable", "rich"};
    f["wealth"] = Wealth[wealth];
    // (A household a player lifted has no hunger tags while lifted: doc 79, 6.7.)
    f["hungry"] = yes(life->hunger >= 55 && !society.liftedNow(e.id));
    f["homeless"] = yes(life->homeCell.empty() || job == "beggar");
    // A destitute wolf out begging (doc 79, 10.2) speaks as a beggar does (the `job=beggar` barks), whatever its home.
    if (e.activity.rfind("begging", 0) == 0)
        f["job"] = "beggar";

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
    // A carter driving a wagon or a farm's cart (doc 79, 8.1-8.2): "on the Coldwater run", "driving the wagon home to
    // Ridgemere", "taking the cart to Larkfield".
    if (e.activity.rfind("driving the wagon", 0) == 0 ||
        (e.activity.rfind("on the ", 0) == 0 && e.activity.find(" run") != std::string::npos && e.activity.find(" run") < e.activity.find(" — ")))
        errand = "caravan";
    else if (e.activity.rfind("taking the cart", 0) == 0)
        errand = "goods";
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
    // A House's: its head, or kin of its head.
    for (const auto& h : society.houses())
    {
        const auto head = society.state().careers.positions.find(h.head);
        const auto& headHolder = head != society.state().careers.positions.end() ? head->second.holder : std::string();
        const bool of = (post && post->id == h.head) || (!headHolder.empty() && society.family(e.id, headHolder));
        if (of)
        {
            f["house"] = h.id.substr(h.id.find(':') + 1);
            break;
        }
    }
    const auto tone = voices_.toneOf(spec ? spec->personality : std::string());
    f["tone"] = tone.empty() ? "plain" : tone;      // (The router's tones; plain without the router, or a personality.)
    // Doc 84 §8.8 (Phase 6): their mood from the last days, their stances (and the steps events moved them: the talk
    // ledger's drift), their likes and dislikes (stable: drawn from who they are).
    talkMood(e, f);
    auto& daily = talkDaily_[e.id];
    if (const auto today = std::int64_t(std::floor(world_.calendarDays())); daily.day != today)
    {
        auto worked = f;
        talk_.stances(worked, world_.talkLedger().drifts(e.id));
        talk_.likes(worked);
        if (worn)
            talk_.wear(worked, worn->id);           // (Its opinions, likes, dislikes, fears and hopes: doc 84 §8.8.)
        daily.facts.clear();
        for (auto& [k, v] : worked)
            if (k.rfind("op", 0) == 0 || k == "likes" || k == "dislikes" || k == "dislike_kind" || k == "fears" || k == "hopes" ||
                (k.size() > 6 && k.compare(k.size() - 6, 6, "_words") == 0))
                daily.facts.emplace(k, std::move(v));
        talkSlots(e, post, spec, daily.facts);      // (§11's people and places: doc 84, Phase 10.)
        daily.day = today;
    }
    for (const auto& [k, v] : daily.facts)
        f[k] = v;
    return p;
}

void Game::talkSlots(const Entity& e, const Position* post, const ResidentSpec* spec, talk::Facts& f)
{
    // Doc 84 §11's slots, worked out once a game day: the kin last mourned (dead), the closest friend (affinity 40 or more)
    // and the worst rival (-25 or less) among the living residents, over and under at work (a shop's keeper and the help
    // at its premises), the work's place and the post's words, and the street home opens onto.
    const auto& society = world_.society();
    const auto put = [&](const char* k, const Entity* who) {
        if (who)
            f[k] = who->name, f[std::string(k) + "_sex"] = who->appearance->sex == "female" ? "female" : "male";
    };
    if (const auto* m = society.mourning(e.id))
        if (const auto* dead = world_.entity(m->whom); dead && dead->dead)
        {
            const auto& c = society.state().careers;
            const auto in = [](const std::map<std::string, std::vector<std::string>>& of, const std::string& x, const std::string& y) {
                const auto found = of.find(x);
                return found != of.end() && std::find(found->second.begin(), found->second.end(), y) != found->second.end();
            };
            const auto spouse = c.spouses.find(e.id);
            if ((spouse != c.spouses.end() && spouse->second == m->whom) || in(c.parents, e.id, m->whom) || in(c.parents, m->whom, e.id))
                put("late", dead);
        }
    const Entity *friendOf = nullptr, *rivalOf = nullptr;
    double best = 39.999, worst = -25.001;
    if (const auto* bonds = world_.bonds().of(e.id))
        for (const auto& [other, b] : *bonds)
            if (const auto* o = world_.entity(other); o && o->npc && !o->dead && other != e.id)
            {
                if (b.affinity > best)
                    best = b.affinity, friendOf = o;
                if (b.affinity < worst)
                    worst = b.affinity, rivalOf = o;
            }
    put("friend", friendOf);
    put("rival", rivalOf);
    if (post)
    {
        const Entity *boss = nullptr, *hand = nullptr;
        for (const auto& [pid, ps] : society.state().careers.positions)
            if (const auto* p = society.position(pid); p && !ps.holder.empty() && ps.holder != e.id && p->work.cell == post->work.cell &&
                                                       !post->work.cell.empty())
                if (const auto* o = world_.entity(ps.holder); o && !o->dead)
                {
                    if (p->role == "merchant" && post->role != "merchant" && !boss)
                        boss = o;
                    else if (post->role == "merchant" && p->role != "merchant" && !hand)
                        hand = o;
                }
        put("boss", boss);
        put("hand", hand);
        if (const auto* place = world_.cell(post->work.cell); place && !place->name.empty())
            f["workplace"] = place->name;
        f["trade_words"] = post->title;
    }
    else if (spec && !spec->workLabel.empty() && spec->workLabel != "-")
        f["trade_words"] = spec->workLabel;
    if (const auto* life = society.resident(e.id); life && !life->homeCell.empty())
        if (const auto& [street, at] = entranceOf(life->homeCell); !street.empty())
            if (const auto* c = world_.cell(street); c && !c->name.empty())
                f["street"] = c->name;
}

void Game::talkMood(const Entity& e, talk::Facts& f)
{
    // §8.8: low, sore, bright or even, from the last three days' events, fading: mourning, hunger, a bad hurt, want (low);
    // robbed, attacked, a quarrel (sore); a wedding, a pup born, a new post, praise, good trade, a festival (bright). The
    // strongest pull of 2 or more wins (low before sore before bright); else even.
    const double day = world_.calendarDays();
    double low = 0, sore = 0, bright = 0;
    std::string lowWhy, soreWhy, brightWhy;
    const auto add = [](double& at, std::string& why, double w, const char* what) {
        if (w > at)
            at = w, why = what;
    };
    const auto fade = [&](double when) { return std::clamp(1 - (day - when) / 3.0, 0.0, 1.0); };
    const auto is = [&](const char* k, const char* v) {
        const auto found = f.find(k);
        return found != f.end() && found->second == v;
    };
    if (is("mourning", "kin"))
        add(low, lowWhy, 3, "mourning");
    else if (is("mourning", "friend"))
        add(low, lowWhy, 2, "mourning");
    if (is("hungry", "yes"))
        add(low, lowWhy, 2, "hungry");
    if (is("injured", "bad"))
        add(low, lowWhy, 2.5, "hurt");
    if (is("wealth", "destitute") || is("homeless", "yes"))
        add(low, lowWhy, 2, "poor");
    for (const auto& i : world_.crime().incidents)
        if (day - i.day <= 3 && i.victim == e.id)
        {
            if (i.kind == "theft")             // (Robbed: something was taken; an attempt is a fright, not a loss.)
                add(sore, soreWhy, 2.5 * fade(i.day) + .5, "robbed");
            else if (i.kind == "assault")
                add(sore, soreWhy, 2.5 * fade(i.day) + .5, "attacked");
        }
    if (const auto* news = world_.ownNews(e.id))
        for (const auto& n : *news)
        {
            if (day - n.day > 3)
                continue;
            if (n.kind == "marriage")
                add(bright, brightWhy, 2.5 * fade(n.day) + .5, "wed");
            else if (n.kind == "birth")
                add(bright, brightWhy, 2.5 * fade(n.day) + .5, "newborn");
            else if (n.kind == "work")
                add(bright, brightWhy, 2 * fade(n.day) + .3, "work");
            else if (n.kind == "loss")
                add(low, lowWhy, 2.5 * fade(n.day) + .5, "loss");
        }
    for (const auto& [when, kind] : world_.talkLedger().moodEvents(e.id, day - 3))
    {
        if (kind == "quarrel")
            add(sore, soreWhy, 1.8 * fade(when) + .3, "quarrel");     // (Sore a few hours, unless more befalls them.)
        else if (kind == "praised")
            add(bright, brightWhy, 1.7 * fade(when) + .3, "praised");
    }
    if (is("business", "thriving"))
        add(bright, brightWhy, 2, "trade");
    if (world_.dayPlan(world_.communityOf(e.cellId)).kind == "festival")
        add(bright, brightWhy, 2, "festival");
    std::string mood = "even", why;
    if (low >= 2 && low >= sore && low >= bright)
        mood = "low", why = lowWhy;
    else if (sore >= 2 && sore >= bright)
        mood = "sore", why = soreWhy;
    else if (bright >= 2)
        mood = "bright", why = brightWhy;
    f["mood"] = mood;
    if (!why.empty())
        f["mood_why"] = why;
}

void Game::talkDrift()
{
    // Doc 84 §7.6 (Phase 6): once a game day, the events that move a stance a step (opinions.json's drift), each applied
    // once (the talk ledger keeps which): a theft from a resident closed unsolved (cold) or charged, the Restday dole
    // feeding them, kin of theirs under a warrant, a wedding of their own.
    auto& ledger = world_.talkLedger();
    const auto today = std::int64_t(std::floor(world_.calendarDays()));
    if (!ledger.enabled || today == talkDriftDay_ || talk_.issues().empty())
        return;
    talkDriftDay_ = today;
    talkDaily_.clear();                             // (Stances worked out again, with today's drift.)
    const double day = world_.calendarDays();
    const auto& society = world_.society();
    const auto apply = [&](const std::string& who, const std::string& event, const std::string& id) {
        for (const auto& i : talk_.issues())
            if (const auto d = i.drift.find(event); d != i.drift.end())
                ledger.drift(who, i.name, d->second, event, id, day);
    };
    for (const auto& i : world_.crime().incidents)
    {
        if (day - i.day > 30 || i.victim.empty() || !society.resident(i.victim) || (i.kind != "theft" && i.kind != "attempted theft"))
            continue;
        if (i.status == "cold")
            apply(i.victim, "robbed_uncaught", i.id);
        else if (i.status == "charged")
            apply(i.victim, "robbed_caught", i.id);
    }
    for (const auto& w : world_.crime().warrants)
        for (const auto& [id, life] : society.state().residents)
            if (id != w.person && (society.family(id, w.person) || (society.spouse(id) && *society.spouse(id) == w.person)))
                apply(id, "kin_charged", "warrant:" + w.person);
    for (const auto& [id, life] : society.state().residents)
    {
        if (society.onDole(id))
            apply(id, "fed_by_dole", "dole");
        if (const auto* news = world_.ownNews(id))
            for (const auto& n : *news)
                if (n.kind == "marriage" && day - n.day <= 3)
                    apply(id, "wed", "wed:" + std::to_string(std::int64_t(n.day)));
    }
}

std::string Game::voiceCardOf(const std::string& id)
{
    const auto* e = world_.entity(id);
    return e && e->npc ? talk_.voiceCard(talkSpeaker(*e)) : std::string();
}

void Game::notEachOthers(talk::Query& q)
{
    // One's people by name (#self.spouse#, #self.child#...) are never the one spoken to: "Ridge cooked last night" isn't
    // said to Ridge, nor "Tam made me breakfast" to Tam (doc 84, Phase 5: the talk town's T3). Lines that need them don't
    // fit between the two.
    static const char* Roles[] = {"spouse", "child", "mother", "father", "neighbour", "shopkeeper", "priest", "leader"};
    for (auto [self, other] : {std::pair{&q.a, &q.b}, std::pair{&q.b, &q.a}})
        if (const auto name = other->facts.find("name"); name != other->facts.end())
            for (const char* role : Roles)
                if (const auto f = self->facts.find(role); f != self->facts.end() && f->second == name->second)
                    self->facts.erase(f);
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
    // Under one roof (kin or not): a house's own complaints are said to one who lives elsewhere (the slice-end review).
    facts["pair.roof"] = society.household(a, b) ? "yes" : "no";
    if (!elder.empty())
        facts["pair.elder"] = elder;
    // Who works for whom: a shop's keeper and the help at its premises (nobody pays a wage since doc 66's Phase 5; the
    // shop is still the keeper's).
    const auto paidBy = [&](const std::string& worker, const std::string& boss) {
        const auto* job = society.jobOf(worker);
        const auto* own = society.jobOf(boss);
        return job && own && worker != boss && own->role == "merchant" && job->role != "merchant" && job->work.cell == own->work.cell;
    };
    facts["pair.boss"] = paidBy(b, a) ? "a" : paidBy(a, b) ? "b" : "no";
    // Neighbours, and those they deal with (decision 8): who buys from whom, who is whose priest.
    const auto near = neighboursOf(a);
    facts["pair.neighbours"] = yes(std::find(near.begin(), near.end(), b) != near.end());
    auto pa2 = peopleOf(a), pb2 = peopleOf(b);
    facts["pair.customer"] = pa2["shopkeeper"] == b ? "a" : pb2["shopkeeper"] == a ? "b" : "no";
    facts["pair.flock"] = pa2["priest"] == b ? "b" : pb2["priest"] == a ? "a" : "no";
    // Toward the other, player or resident (decision 9): a's knowledge of b, and a's feelings for b.
    const Bond none{};
    const auto* bond = world_.bonds().find(a, b);
    const Bond& r = bond ? *bond : none;
    // Between residents (doc 84 §7.11, defect 11): they know each other's names at familiarity 10 or more, or as kin,
    // housemates or workmates. A player's name is known only as it was given (knowsName: doc 32).
    const auto* ea = world_.entity(a);
    const auto* eb = world_.entity(b);
    const auto residentsKnow = [&](const std::string& x, const std::string& y, const Bond* xy) {
        if (xy && xy->familiarity >= 10)
            return true;
        if (society.family(x, y) || society.household(x, y))
            return true;
        const auto* jx = society.jobOf(x);
        const auto* jy = society.jobOf(y);
        return jx && jy && jx->work.cell == jy->work.cell && !jx->work.cell.empty();
    };
    const bool residents = ea && eb && ea->npc && eb->npc;
    facts["pair.named"] = yes(residents ? residentsKnow(a, b, bond) : knowsName(a, b));
    if (const auto* k = known_.find(a, b))
        facts["pair.named_how"] = k->how;
    facts["pair.known_by"] = yes(residents ? residentsKnow(b, a, world_.bonds().find(b, a)) : knowsName(b, a));
    talkBond(a, b, facts);
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
    // An echo (doc 84 §12.4, Phase 10): one of the two wears a mantle last worn by kin of theirs whom the other knew well
    // (familiarity 40 or more): "You've your grandmother's laugh, you know that?"
    std::string echo = "no", echoOf;
    for (const auto& [wearer, other] : {std::pair{a, b}, std::pair{b, a}})
        if (const auto* m = society.mantleOf(wearer); m && !m->echoOf.empty() && m->releasedDay < 0)
            if (const auto* knew = world_.bonds().find(other, m->echoOf); knew && knew->familiarity >= 40)
                if (const auto* dead = world_.entity(m->echoOf); dead && other != m->echoOf)
                    echo = "yes", echoOf = dead->name;
    facts["pair.echo"] = echo;
    if (!echoOf.empty())
        facts["pair.echo_of"] = echoOf;
    if (const auto* other = world_.entity(b); other && !other->npc)
        facts["pair.famous"] = yes(!recognise(a, b, false).empty());
}

void Game::talkBond(const std::string& a, const std::string& b, talk::Facts& facts)
{
    // a's feelings for b (decision 9), in numbers and one word: what a talk itself moves, so asked again every line of
    // one (doc 84 §8.2).
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
}

talk::Query Game::talkQuery(const std::string& a, const std::string& b, const std::string& concept,
                            const talk::Facts& facts)
{
    talk::Query q;
    q.concept = concept;
    q.alone = b.empty();
    // Who a line's `binds x=<slot>` names (doc 84 §7.1): one of the speaker's or the other's people (peopleOf).
    // topic.subject is the exchange's own (exchangeQuery).
    q.bind = [this, a, b](const std::string& slot, int speaker, talk::Facts& out) {
        const auto dot = slot.find('.');
        if (dot == std::string::npos)
            return false;
        const auto& self = speaker == 0 ? a : b;
        const auto& whose = slot.compare(0, dot, "self") == 0 ? self : slot.compare(0, dot, "other") == 0 ? (speaker == 0 ? b : a) : std::string();
        if (whose.empty())
            return false;
        const auto people = peopleOf(whose);
        const auto found = people.find(slot.substr(dot + 1));
        // (Never the speaker, nor the one spoken to: "Fell's family are coming" is never said to Fell. Doc 84, Phase 5.)
        const auto& other = speaker == 0 ? b : a;
        return found != people.end() && found->second != self && found->second != other && talkThird(self, found->second, out);
    };
    world_.talkMoment(a, q.facts);
    world_.talkTown(a, q.facts);
    if (const auto region = q.facts.find("here.region"); region != q.facts.end())
        q.facts["here.fishers"] = std::to_string(townFishers(region->second));
    // (A price fact only for the goods the library lists: world.<good>_price, doc 79, 11. The rest are the topic's.)
    for (auto it = q.facts.begin(); it != q.facts.end();)
        it = it->first.rfind("world.", 0) == 0 && it->first.size() > 12 && it->first.compare(it->first.size() - 6, 6, "_price") == 0 &&
                     !talk_.sharedFacts().count(it->first)
                 ? q.facts.erase(it)
                 : std::next(it);
    if (const auto* e = world_.entity(a))
        q.a = talkSpeaker(*e);
    if (const auto* e = q.alone ? nullptr : world_.entity(b))
    {
        q.b = talkSpeaker(*e);
        notEachOthers(q);
        talkPair(a, b, q.facts);
        // What each calls the other (doc 84 §11, #other.called#; Phase 10): between residents who know each other's names,
        // the first name, or a Master or Mistress and the surname across a class gap in Ridgemere; strangers nothing (the
        // line says the speaker's #address# instead).
        if (q.facts["pair.named"] == "yes" && q.a.facts.count("first") && q.b.facts.count("first") && !q.a.player && !q.b.player)
        {
            const bool ridgemere = q.facts.count("here.region") && q.facts.at("here.region") == "ridgemere";
            const auto calls = [&](const talk::Speaker& who, const talk::Speaker& whom) {
                const auto of = [](const talk::Speaker& p, const char* k) {
                    const auto f = p.facts.find(k);
                    return f == p.facts.end() ? std::string() : f->second;
                };
                const auto gap = (of(who, "class") == "upper") != (of(whom, "class") == "upper");
                if (ridgemere && gap && !of(whom, "surname").empty() && q.facts["pair.kin"] == "no" &&
                    std::atoi(q.facts["pair.affinity"].c_str()) < 40)
                    return std::string(of(whom, "sex") == "female" ? "Mistress " : "Master ") + of(whom, "surname");
                return of(whom, "first");
            };
            q.b.facts["called"] = calls(q.a, q.b);
            q.a.facts["called"] = calls(q.b, q.a);
        }
        if (q.a.facts.count("from") && q.b.facts.count("from"))
            q.facts["pair.same_from"] = q.a.facts["from"] == q.b.facts["from"] ? "yes" : "no";
        // Who is the better off: the band first (a House's lord is the richer of the two whatever is in the larder; doc 62,
        // phase 7), then within a band the household's score (doc 79, 6.3: half a point apart), or for those in no band
        // their food in days (half again more).
        static const std::map<std::string, int> Rank{{"destitute", 0}, {"poor", 1}, {"comfortable", 2}, {"rich", 3}};
        const auto rank = [&](const talk::Speaker& p) {
            const auto w = p.facts.find("wealth");
            const auto r = w == p.facts.end() ? Rank.end() : Rank.find(w->second);
            return r == Rank.end() ? 1 : r->second;
        };
        const int ra = rank(q.a), rb = rank(q.b);
        const auto* sa = world_.society().statusOf(a);
        const auto* sb = world_.society().statusOf(b);
        const auto wa = q.a.facts.find("wealth_days"), wb = q.b.facts.find("wealth_days");
        std::string richer = ra != rb ? (ra > rb ? "a" : "b") : "same";
        if (ra == rb && sa && sb)
            richer = sa->score >= sb->score + .5 ? "a" : sb->score >= sa->score + .5 ? "b" : "same";
        else if (ra == rb && wa != q.a.facts.end() && wb != q.b.facts.end())
        {
            const double x = std::stod(wa->second), y = std::stod(wb->second);
            richer = x > y * 1.5 + 1 ? "a" : y > x * 1.5 + 1 ? "b" : "same";
        }
        if (q.b.facts.count("wealth") && q.a.facts.count("wealth"))
            q.facts["pair.richer"] = richer;
    }
    // Whether each grew up where they stand (self.home_here; Slice 2's review): a city-born wolf in the city is never told
    // he's a long way from home.
    if (const auto region = q.facts.find("here.region"); region != q.facts.end())
        for (auto* s : {&q.a, &q.b})
            if (const auto from = s->facts.find("from"); from != s->facts.end())
                s->facts["home_here"] = from->second == region->second ? "yes" : "no";
    if (!q.alone)
    {
        talkRecall("pair:" + std::min(a, b) + "|" + std::max(a, b), "pair.", q.facts);
        talkLedgerFacts(a, b, q);                   // (The talk ledger's: doc 84 §8.7, Phase 5.)
    }
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
    // Whether each speaker deals in the topic's good (doc 79, 11; Phase 8 repair: "my shelf of nails" only from a keeper
    // who sells them), and whether its shop has none of it left (`sold_out`: "cleared out of bread" only from a keeper
    // whose bread is gone, never after a dozen loaves off a full shelf).
    if (const auto good = q.facts.find("topic.good"); good != q.facts.end())
    {
        const auto deals = [&](const std::string& id, talk::Facts& of) {
            of["deals"] = of["sold_out"] = "no";
            const auto& s = world_.society();
            if (!s.merchant(id))
                return;
            const auto* till = s.account(s.tillOf(id));
            int left = 0;
            for (const auto& w : s.wares(id))
                if (items::baseOf(w) == good->second)
                {
                    of["deals"] = "yes";
                    left += till ? Society::stock(*till, w) : 0;
                }
            if (of["deals"] == "yes" && left <= 0)
                of["sold_out"] = "yes";
        };
        deals(a, q.a.facts);
        if (!q.alone)
            deals(b, q.b.facts);
    }
    return q;
}

int Game::townFishers(const std::string& town)
{
    if (const auto today = std::int64_t(std::floor(world_.calendarDays())); today != townFishersDay_)
    {
        townFishersDay_ = today;
        townFishers_.clear();
        const auto& society = world_.society();
        for (const auto& [id, life] : society.state().residents)
        {
            const auto* e = world_.entity(id);
            if (!e || e->dead || life.homeCell.empty())
                continue;
            const auto* spec = society.spec(id);
            const auto* post = society.jobOf(id);
            const auto role = spec ? spec->role : std::string("civilian");
            const auto job = talk::jobCategory(post ? post->title : std::string(), spec ? spec->workLabel : std::string(),
                                               role == "merchant" || role == "guard" ? role : "civilian", e->age);
            if (job == "fisher" || job == "sailor")
                ++townFishers_[world_.townIdOf(life.homeCell)];
        }
    }
    const auto found = townFishers_.find(town);
    return found == townFishers_.end() ? 0 : found->second;
}

void Game::talkLedgerFacts(const std::string& a, const std::string& b, talk::Query& q)
{
    // Doc 84 §7.11, §8.7 (Phase 5): what the ledger keeps of the two, as the library's facts. The pair's: days since they
    // last talked (pair.last_talk) and how it went, a thread left open, each topic they talked of (pair.talked.<topic>,
    // days; pair.recall.<topic>, how it went), tags kept (pair.was.<k>), an invitation kept or missed. Each one's: their
    // own kept tags (was.<k>), the claim kind they last passed the other (told_me), their talk-only promise to the other
    // (promised, promise_what).
    const auto& ledger = world_.talkLedger();
    const double day = world_.calendarDays();
    const auto days = [](double d) {
        char buf[32];
        std::snprintf(buf, sizeof buf, "%.2f", std::max(0.0, d));
        return std::string(buf);
    };
    if (const auto* notes = ledger.notes(a, b))    // (Once, the newest first: every line asks: §16, B1.)
    {
        bool last = false;
        for (auto it = notes->rbegin(); it != notes->rend(); ++it)
        {
            if (!last && !it->unheard)
            {
                last = true;
                q.facts["pair.last_talk"] = days(day - it->day);
                if (!it->outcome.empty())
                    q.facts["pair.last_outcome"] = it->outcome;
                // Who soured it (Slice 2's review): the one who says sorry (frames.json's mend: self.soured=yes).
                if (!it->souredBy.empty())
                    for (auto* s : {&q.a, &q.b})
                        if (const auto id = s->facts.find("id"); id != s->facts.end())
                            s->facts["soured"] = id->second == it->souredBy ? "yes" : "no";
                if (!it->thread.empty() && day <= it->threadUntil)
                    q.facts["pair.thread"] = it->thread;
            }
            for (const auto& topic : it->topics)
                if (q.facts.emplace("pair.talked." + topic, days(day - it->day)).second && !it->outcome.empty() && !it->unheard)
                    q.facts["pair.recall." + topic] = it->outcome;
            for (const auto& [k, v] : it->kept)
                if (day <= v.second)
                    q.facts.emplace(k.rfind("agree.", 0) == 0 ? "pair." + k : "pair.was." + k, v.first);   // (Phase 6: pair.agree.<issue>.)
        }
    }
    if (const auto* invite = ledger.lastInvite(a, b); invite && (invite->state == "kept" || invite->state == "missed"))
        q.facts["pair.kept_invite"] = invite->state == "kept" ? "yes" : "no";
    for (auto [self, other] : {std::pair{&q.a, b}, std::pair{&q.b, a}})
    {
        const auto id = self->facts.count("id") ? self->facts.at("id") : std::string();
        if (id.empty())
            continue;
        for (const auto& [k, v] : ledger.kept(id, day))
            self->facts["was." + k] = v;
        if (const auto kind = ledger.toldMe(other, id); !kind.empty())
            self->facts["told_me"] = kind;
        if (const auto* p = ledger.lastPromise(id, other); p && (p->state == "open" || p->state == "lapsed"))
        {
            self->facts["promised"] = p->state;
            self->facts["promise_what"] = p->thing;
        }
    }
}

void Game::talkRemember(const std::vector<talk::Effect>& effects)
{
    const double now = world_.calendarDays();
    for (const auto& e : effects)
    {
        if (e.scope == "said")
        {
            // What talk has said of a third party (remember x: k=v Nd; doc 84 §8.7): the ledger's person facts, x.said.k.
            const auto bar = e.owner.find('|');
            world_.talkLedger().said(e.owner.substr(0, bar), e.key, e.value, bar == std::string::npos ? std::string() : e.owner.substr(bar + 1),
                                     e.days < 0 ? -1 : now + e.days);
            continue;
        }
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
        if (e.key.rfind("said:", 0) == 0)
        {
            talkSaid_[e.scope + ":" + e.owner][e.key] = e.days < 0 ? -1 : now + e.days;   // (Never evicted: defect 6.)
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
    const double now = world_.calendarDays();
    if (const auto said = talkSaid_.find(owner); said != talkSaid_.end())
    {
        for (auto it = said->second.begin(); it != said->second.end();)
        {
            if (it->second >= 0 && it->second < now)
            {
                it = said->second.erase(it);
                continue;
            }
            into[prefix + it->first] = "yes";
            ++it;
        }
        if (said->second.empty())
            talkSaid_.erase(said);
    }
    const auto found = talkMemory_.find(owner);
    if (found == talkMemory_.end())
        return;
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
        if (t.pick.teller == npc || t.pick.listener == npc || (!to.empty() && (t.pick.teller == to || t.pick.listener == to)) ||
            std::find(t.people.begin(), t.people.end(), npc) != t.people.end())
            return false;                           // (Nor one who joined a talk: doc 84 §8.9.)
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
        if (options_.logTalkWords)
            voicedWords("react", npc, to, said.lines.front().second, said.info.empty() ? nullptr : &said.info.front());
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
        if (said.info.size() == said.lines.size())    // (Where each line came from: doc 84 §15.)
            talking.info.assign(said.info.begin(), said.info.end());
        talking.chain = said.chain;
        talking.route = "react";
        talking.written = int(said.lines.size());
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
    if (world_.doorsKey() != entranceDoors_)        // (Not the doors' count: seams stream in and out: doorsKey.)
    {
        entrances_.clear();
        entranceDoors_ = world_.doorsKey();
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

void Game::talkPeopleFresh()
{
    // Who a resident's people are (their neighbours, shopkeeper, priest, leader) walks the town's homes and posts: kept
    // five world minutes and while the doors are the same (doc 84 §16, B1: a talk's facts are asked every line, and the
    // director asks them of the same few in a busy place look after look). Homes, posts and kin change far slower. (30 s
    // until Phase 5, whose longer talks crossed it, and with it a walk of every home, in most talks.)
    const double now = world_.time();
    if (peopleMemoAt_ < 0 || now < peopleMemoAt_ || now - peopleMemoAt_ >= 300 || world_.doorsKey() != peopleMemoDoors_)
    {
        peopleMemo_.clear();
        neighboursMemo_.clear();
        peopleMemoAt_ = now;
        peopleMemoDoors_ = world_.doorsKey();
    }
}

std::vector<std::string> Game::neighboursOf(const std::string& id)
{
    talkPeopleFresh();
    if (const auto found = neighboursMemo_.find(id); found != neighboursMemo_.end())
        return found->second;
    return neighboursMemo_[id] = neighboursNow(id);
}

std::vector<std::string> Game::neighboursNow(const std::string& id)
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

bool Game::talkThird(const std::string& speaker, const std::string& who, talk::Facts& out)
{
    const auto* e = world_.entity(who);
    if (!e || e->dead)
        return false;
    out = talkSpeaker(*e).facts;
    for (const auto& [k, v] : world_.talkLedger().saidOf(who, world_.calendarDays()))
        out["said." + k] = v;                       // (What talk has said of them: x.said.k, doc 84 §8.7.)
    const Bond none{};
    const auto* bond = world_.bonds().find(speaker, who);
    const Bond& r = bond ? *bond : none;
    out["affinity"] = std::to_string(int(std::lround(r.affinity)));
    // (The same one word as pair.disposition: talkPair.)
    out["disposition"] = r.affinity <= -50 || (r.affinity <= -25 && r.fear >= 30) ? "hostile"
                         : r.affinity <= -15 || r.trust <= -25                    ? "wary"
                         : r.affinity < -3                                        ? "cool"
                         : r.affinity >= 50 && r.familiarity >= 40                ? "fond"
                         : r.affinity >= 20                                       ? "warm"
                                                                                  : "neutral";
    return true;
}

std::map<std::string, std::string> Game::peopleOf(const std::string& id)
{
    talkPeopleFresh();
    if (const auto found = peopleMemo_.find(id); found != peopleMemo_.end())
        return found->second;
    auto out = peopleNow(id);
    talkPeopleFresh();                              // (Asked again: neighboursOf may have begun the memo afresh.)
    return peopleMemo_[id] = std::move(out);
}

std::map<std::string, std::string> Game::peopleNow(const std::string& id)
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
    double nearest = 1e18, nearestChurch = 1e18;
    // Where a door is in the world (its street's place on the map), for the church nearest home: the priest of their own
    // district, not the town's first (doc 84, defect 12: one priest was 598 residents' of 773).
    const auto onMap = [&](const std::pair<std::string, Vec2>& at) {
        const auto* c = world_.cell(at.first);
        return c ? Vec2{c->worldX + at.second.x, c->worldY + at.second.y} : Vec2{1e9, 1e9};
    };
    const auto home = onMap(door);
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
        if (Society::clergy(p.title))
        {
            const auto at = onMap(entranceOf(p.work.cell));
            if (const double d = std::hypot(at.x - home.x, at.y - home.y); d < nearestChurch)
                nearestChurch = d, out["priest"] = holder;
        }
    }
    // Their leader: their own House's head; else the head of the House whose district they live in; else of a House of
    // their town (doc 84, defect 12).
    const auto district = world_.districtHouse(life->homeCell.empty() ? e->cellId : life->homeCell);
    int rank = 0;                                   // 3 their own House, 2 their district's, 1 their town's.
    for (const auto& h : society.houses())
    {
        const auto head = society.state().careers.positions.find(h.head);
        if (head == society.state().careers.positions.end() || head->second.holder.empty() || head->second.holder == id ||
            !living(head->second.holder))
            continue;
        const int r = society.family(id, head->second.holder) ? 3 : !district.empty() && h.name == "House " + district ? 2
                      : h.community == town ? 1 : 0;
        if (r > rank)
            rank = r, out["leader"] = head->second.holder;
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
    ledger.push_back({world_.calendarDays(), source, json::utf8Prefix(reason, 160), change});
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
    {
        loadMantles({});
        return;
    }
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
    // Whether the world has mantles (migration 0066): a world before it has no binding columns either.
    if (const auto rows = worldDb_.exec("SELECT to_regclass('live.mantles') IS NOT NULL", {}); rows.ok && !rows.rows.empty() &&
        !rows.rows.front().empty() && rows.rows.front()[0])
        mantleTables_ = *rows.rows.front()[0] == "t" || *rows.rows.front()[0] == "true";
    // Each resident's voice (decisions 4 to 6, 10).
    std::map<std::string, NpcVoice> voices;
    std::set<std::string> layers, rules;
    std::map<std::string, std::string> bindings;
    rowBindings_.clear();
    if (const auto rows = worldDb_.exec(std::string("SELECT npc_id, home_region, accent, quarter, traits::text, quirk, persona, grammar, rules") +
                                            (mantleTables_ ? ", coalesce(mantle_id, ''), released_day IS NULL, bound_how, "
                                                             "coalesce(bound_day, 0), coalesce(released_day, -1)" : "") +
                                            " FROM live.npc_voice WHERE world_id = $1",
                                        {liveWorldId_});
        rows.ok)
        for (const auto& row : rows.rows)
        {
            if (row.size() < 9 || !row[0])
                continue;
            const auto& id = *row[0];
            const auto text = [&](std::size_t i) { return i < row.size() && row[i] ? *row[i] : std::string(); };
            auto& v = voices[id];
            v.from = text(1), v.accent = text(2), v.quarter = text(3), v.quirk = text(5), v.persona = text(6);
            json::Value traits;
            std::string error;
            if (json::parse(text(4), traits, error) && traits.isObject())
                for (const auto& [axis, value] : traits.fields())
                    if (value.isString())
                        v.traits[axis] = value.asString("");
            // The mantle they wear (doc 84 §17.7): a binding not released (their death releases it, Phase 10).
            if (row.size() >= 11 && !text(9).empty() && (text(10) == "t" || text(10) == "true"))
                bindings[id] = v.mantle = text(9);
            // Every binding, worn or released (doc 84 Phase 10): the ledger's truth in a database world.
            if (row.size() >= 14 && !text(9).empty())
                rowBindings_[id] = {text(9), text(11), std::atof(text(12).c_str()), std::atof(text(13).c_str())};
            // Their own words and lines: a person's layer, and rules only they may say (an override: never passed on).
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
    voicesChanged();
    voiceLayers_ = std::move(layers);
    voiceRules_ = std::move(rules);
    loadMantles(bindings);
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
                     " with words of their own, " + std::to_string(voiceRules_.size()) + " with lines, " +
                     std::to_string(mantleOf_.size()) + " with a mantle), " + std::to_string(dialects) + " dialects, " +
                     std::to_string(figures_.size()) + " figures" + (refused ? ", " + std::to_string(refused) + " refused" : ""));
}

void Game::voicesChanged()
{
    // The quarters (the Heights, the Sump, an estate): the Market Board's district factor (doc 79, 7.1) and the bands'
    // score and guarantees (6.3), one map for both.
    std::map<std::string, std::string> quarters;
    for (const auto& [id, v] : npcVoice_)
        if (!v.quarter.empty())
            quarters[id] = v.quarter;
    world_.society().setQuarters(std::move(quarters));
    // Their honesty (frank, plain, guarded, sly): who turns thief among the day's thieves (doc 79, 10.1).
    std::map<std::string, std::string> honesty;
    for (const auto& [id, v] : npcVoice_)
        if (const auto trait = v.traits.find("honesty"); trait != v.traits.end() && !trait->second.empty())
            honesty[id] = trait->second;
    world_.society().setHonesty(std::move(honesty));
    // Their origin and accent (Slice 2's review): the world's talk (the accent topic) as their own facts have it.
    std::map<std::string, std::pair<std::string, std::string>> origins;
    for (const auto& [id, v] : npcVoice_)
        if (!v.from.empty() || !v.accent.empty())
            origins[id] = {v.from, v.accent};
    world_.setTalkOrigins(std::move(origins));
}

namespace
{
// A mantle's card from its live.mantles row (the columns the queries below select, from `at`), as talk::mantleText reads it.
talk::Mantle mantleCard(const std::vector<PgValue>& row, std::size_t at)
{
    const auto text = [&](std::size_t i) { return at + i < row.size() && row[at + i] ? *row[at + i] : std::string(); };
    talk::Mantle m;
    m.id = text(0), m.dialect = text(1), m.tier = text(2), m.state = text(3);
    m.format = std::max(1, std::atoi(text(4).c_str()));
    std::istringstream accents(text(5));
    m.accents.clear();
    for (std::string a; accents >> a;)
        m.accents.push_back(a);
    m.title = text(6), m.writtenFor = text(7), m.fits = text(8), m.prefers = text(9);
    json::Value traits, card;
    std::string error;
    if (json::parse(text(10), traits, error) && traits.isObject())
        for (const auto& [k, v] : traits.fields())
            if (v.isString())
                m.traits[k] = v.asString("");
    m.quirk = text(11);
    if (json::parse(text(12), card, error) && card.isObject())
    {
        const auto list = [&](const char* key, std::vector<std::string>& into) {
            if (const auto* v = card.find(key); v && v->isArray())
                for (const auto& w : v->items())
                    if (w.isString())
                        into.push_back(w.asString(""));
        };
        list("likes", m.likes), list("dislikes", m.dislikes), list("fears", m.fears), list("hopes", m.hopes), list("marks", m.marks);
        if (const auto* v = card.find("topics"); v && v->isArray())
            for (const auto& pair : v->items())
                if (pair.isArray() && pair.items().size() == 2 && pair.items()[0].isString())
                    m.topics.push_back({pair.items()[0].asString(""), pair.items()[1].asNumber(0)});
        if (const auto* v = card.find("styles"); v && v->isObject())
            for (const auto& [k, w] : v->fields())
                if (w.isString())
                    m.styles[k] = w.asString("");
    }
    m.persona = text(13);
    m.revision = std::atoi(text(14).c_str());
    m.writtenBy = text(15);
    return m;
}
constexpr const char* MantleColumns = "m.id, m.dialect, m.tier, m.state, m.format, array_to_string(m.accents, ' '), m.title, "
                                      "coalesce(m.written_for, ''), m.fits, m.prefers, m.traits::text, m.quirk, m.card::text, "
                                      "m.persona, m.revision, m.written_by";
constexpr const char* LineColumns = "l.mantle_id, l.key, l.section, l.kind, l.move, l.position, l.body";
talk::MantleItem mantleItem(const std::vector<PgValue>& row)
{
    const auto text = [&](std::size_t i) { return i < row.size() && row[i] ? *row[i] : std::string(); };
    talk::MantleItem item;
    item.key = text(1), item.section = text(2), item.kind = text(3), item.move = text(4);
    item.position = std::atoi(text(5).c_str());
    item.body = text(6);
    return item;
}
bool readText(const std::filesystem::path& path, std::string& out)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
        return false;
    std::ostringstream all;
    all << in.rdbuf();
    out = all.str();
    return true;
}
} // namespace

void Game::loadMantles(const std::map<std::string, std::string>& bindings)
{
    const auto started = std::chrono::steady_clock::now();
    std::vector<talk::MantleText> texts;
    std::vector<PoolMantle> cards;                    // Every ready one's card, for the pools (Phase 10)
    std::map<std::string, std::string> wearers;       // Resident -> mantle
    std::size_t rows = 0;
    const auto living = [&](const std::string& npc) {
        const auto* e = world_.entity(npc);
        return e && e->npc && !e->dead;
    };
    if (!options_.mantlesFrom.empty())
    {
        // Files (an export, or the test fixture: doc 84 §17.5-17.6), each worn by the resident it was written for.
        mantleFiles_.clear();
        std::error_code error;
        std::vector<std::filesystem::path> files;
        for (std::filesystem::recursive_directory_iterator it(options_.mantlesFrom, error), end; !error && it != end; it.increment(error))
            if (it->is_regular_file() && it->path().extension() == ".mantle")
                files.push_back(it->path());
        std::sort(files.begin(), files.end());
        for (const auto& path : files)
        {
            std::string text;
            if (!readText(path, text))
                continue;
            talk::Mantle head;
            std::vector<std::string> problems;
            talk::readMantle(text, path.string(), head, problems);
            if (head.id.empty() || head.state != "ready")
                continue;
            mantleFiles_[head.id] = path.string();
            if (!head.writtenFor.empty() && living(head.writtenFor) && !wearers.count(head.writtenFor))
                wearers[head.writtenFor] = head.id;
            texts.push_back({head.id, text, path.string()});
            cards.push_back({head.id, head.dialect, head.fits, head.writtenFor, head.quirk, head.accents, head.marks, head.traits});
        }
        if (error)
            note("warn", "RATW_MANTLE the mantles under " + options_.mantlesFrom + " could not all be read: " + error.message());
    }
    else if (mantleTables_ && !liveWorldId_.empty())
    {
        // The rows (doc 84 §9.4): the ready mantles worn by a living wolf, card and items, in one pass each.
        std::set<std::string> worn;
        for (const auto& [npc, mantle] : bindings)
            if (living(npc))
                wearers[npc] = mantle, worn.insert(mantle);
        std::map<std::string, talk::Mantle> wornCards;
        std::map<std::string, std::vector<talk::MantleItem>> items;
        const std::string bound = " FROM live.mantles m WHERE m.world_id = $1 AND m.state = 'ready' AND m.id IN "
                                  "(SELECT v.mantle_id FROM live.npc_voice v WHERE v.world_id = $1 AND v.mantle_id IS NOT NULL "
                                  "AND v.released_day IS NULL)";
        if (const auto got = worldDb_.exec(std::string("SELECT ") + MantleColumns + bound, {liveWorldId_}); got.ok)
            for (const auto& row : got.rows)
                if (auto card = mantleCard(row, 0); worn.count(card.id))
                    wornCards[card.id] = std::move(card);
        if (const auto got = worldDb_.exec(std::string("SELECT ") + LineColumns + " FROM live.mantle_lines l WHERE l.world_id = $1 AND "
                                               "l.mantle_id IN (SELECT m.id" + bound + ")",
                                           {liveWorldId_});
            got.ok)
            for (const auto& row : got.rows)
                if (!row.empty() && row[0] && wornCards.count(*row[0]))
                    items[*row[0]].push_back(mantleItem(row)), ++rows;
        for (auto& [id, card] : wornCards)
            texts.push_back({id, talk::mantleText(card, std::move(items[id])), "db:" + id});
        // Every ready mantle's card (§9.4: what the pools need; Phase 10 draws from them), worn or not.
        if (const auto got = worldDb_.exec(std::string("SELECT ") + MantleColumns + " FROM live.mantles m WHERE m.world_id = $1 AND m.state = 'ready'",
                                           {liveWorldId_});
            got.ok)
            for (const auto& row : got.rows)
                if (const auto m = mantleCard(row, 0); !m.id.empty())
                    cards.push_back({m.id, m.dialect, m.fits, m.writtenFor, m.quirk, m.accents, m.marks, m.traits});
    }
    mantlePoolCards_ = std::move(cards);
    std::vector<talk::Library::MantleRefusal> refused;
    const auto loaded = talk_.setMantles(texts, refused);
    for (const auto& r : refused)
        note("warn", "RATW_MANTLE refused " + r.id + ": " + r.why);
    mantleOf_.clear();
    for (const auto& [npc, mantle] : wearers)
        if (talk_.mantle(mantle))
            mantleOf_[npc] = mantle;
    talkDaily_.clear();
    favourites_.clear();
    syncMantleLedger();                             // (Doc 84 Phase 10: the ledger, and who wears what by it.)
    const auto ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
    if (!texts.empty() || !refused.empty() || mantleTables_)
        note("info", "RATW_MANTLE loaded " + std::to_string(loaded) + " mantles (" + std::to_string(talk_.mantleEntries()) +
                         " rules and scenes" + (rows ? ", " + std::to_string(rows) + " rows" : "") + ") worn by " +
                         std::to_string(mantleOf_.size()) + " residents in " + std::to_string(int(ms)) + " ms" +
                         (refused.empty() ? "" : ", " + std::to_string(refused.size()) + " refused") +
                         (options_.mantlesFrom.empty() ? "" : " (from " + options_.mantlesFrom + ")"));
}

std::string Game::mantleRows(const std::string& id, std::string& state, int& revision)
{
    state.clear();
    revision = 0;
    if (!mantleTables_ || liveWorldId_.empty())
        return {};
    const auto got = worldDb_.exec(std::string("SELECT ") + MantleColumns + " FROM live.mantles m WHERE m.world_id = $1 AND m.id = $2",
                                   {liveWorldId_, id});
    if (!got.ok || got.rows.empty())
        return {};
    auto card = mantleCard(got.rows.front(), 0);
    state = card.state;
    revision = card.revision;
    std::vector<talk::MantleItem> items;
    if (const auto lines = worldDb_.exec(std::string("SELECT ") + LineColumns + " FROM live.mantle_lines l WHERE l.world_id = $1 AND l.mantle_id = $2",
                                         {liveWorldId_, id});
        lines.ok)
        for (const auto& row : lines.rows)
            items.push_back(mantleItem(row));
    return talk::mantleText(card, std::move(items));
}

Result Game::loadMantle(const std::string& id)
{
    // mantle.sync <id> (doc 84 §17.2): one mantle's rows (or file) read again and swapped whole; if its new revision
    // fails, the one loaded stays and the reasons are the answer. B3: 50 ms or less.
    const auto started = std::chrono::steady_clock::now();
    std::string text, state, where;
    int revision = 0;
    if (!options_.mantlesFrom.empty())
    {
        auto file = mantleFiles_.count(id) ? mantleFiles_[id] : std::string();
        if (file.empty())
        {
            std::error_code error;
            for (std::filesystem::recursive_directory_iterator it(options_.mantlesFrom, error), end; !error && it != end; it.increment(error))
                if (it->path().filename() == id + ".mantle")
                    file = it->path().string();
        }
        if (file.empty() || !readText(file, text))
            return {false, "No mantle " + id + " under " + options_.mantlesFrom + ".", id};
        talk::Mantle head;
        std::vector<std::string> problems;
        talk::readMantle(text, file, head, problems);
        state = head.state, revision = head.revision, where = file;
        mantleFiles_[id] = file;
    }
    else
    {
        text = mantleRows(id, state, revision);
        where = "db:" + id;
        if (text.empty())
            return {false, "No mantle " + id + " in the world database.", id};
    }
    const auto* was = talk_.mantle(id);
    const int before = was ? was->revision : 0;
    if (state != "ready")
    {
        // A draft is never worn; a retired one leaves at once (§17.2.4). Its wearer speaks from the library.
        talk_.dropMantle(id);
        for (auto it = mantleOf_.begin(); it != mantleOf_.end();)
            it = it->second == id ? mantleOf_.erase(it) : std::next(it);
        talkDaily_.clear();
        return {true, id + " is " + state + ": not worn" + (before ? " (revision " + std::to_string(before) + " unloaded)" : "") + ".", id};
    }
    std::string problem;
    if (!talk_.setMantle({id, text, where}, problem))
    {
        note("warn", "RATW_MANTLE refused " + id + " revision " + std::to_string(revision) + ": " + problem);
        return {false, id + " revision " + std::to_string(revision) + " was refused" +
                           (before ? "; revision " + std::to_string(before) + " is still in use" : "; it is not worn") + ":\n" + problem,
                id};
    }
    // Who wears it now: its binding in the rows (a living resident's, not released), or the one it was written for.
    std::string wearer;
    if (options_.mantlesFrom.empty())
    {
        if (const auto got = worldDb_.exec("SELECT npc_id FROM live.npc_voice WHERE world_id = $1 AND mantle_id = $2 AND released_day IS NULL",
                                           {liveWorldId_, id});
            got.ok && !got.rows.empty() && got.rows.front()[0])
            wearer = *got.rows.front()[0];
    }
    else if (const auto* m = talk_.mantle(id))
        wearer = m->writtenFor;
    for (auto it = mantleOf_.begin(); it != mantleOf_.end();)
        it = it->second == id && it->first != wearer ? mantleOf_.erase(it) : std::next(it);
    if (const auto* e = world_.entity(wearer); e && e->npc && !e->dead)
    {
        mantleOf_[wearer] = id;
        if (auto v = npcVoice_.find(wearer); v != npcVoice_.end())
            v->second.mantle = id;
    }
    talkDaily_.erase(wearer);
    favourites_.erase(wearer);
    const auto ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
    const auto* now = talk_.mantle(id);
    return {true, id + " revision " + std::to_string(now ? now->revision : revision) + " is loaded (" +
                      std::to_string(now ? now->entries : 0) + " rules and scenes" + (wearer.empty() ? ", worn by nobody living" : ", worn by " + wearer) +
                      ", " + std::to_string(int(ms + .5)) + " ms).",
            id};
}

Result Game::loadVoice(const std::string& npc)
{
    // voice.sync <npc> (doc 84 §9.4): one resident's voice row, their own words and lines, and their mantle read again.
    if (options_.database.empty() || liveWorldId_.empty())
    {
        if (const auto m = mantleOf_.find(npc); m != mantleOf_.end())
            return loadMantle(m->second);
        return {true, "No voice rows here: nothing to read for " + npc + ".", npc};
    }
    const auto rows = worldDb_.exec(std::string("SELECT npc_id, home_region, accent, quarter, traits::text, quirk, persona, grammar, rules") +
                                        (mantleTables_ ? ", coalesce(mantle_id, ''), released_day IS NULL" : "") +
                                        " FROM live.npc_voice WHERE world_id = $1 AND npc_id = $2",
                                    {liveWorldId_, npc});
    if (!rows.ok)
        return {false, "The voice of " + npc + " could not be read.", npc};
    std::string problem, said;
    const auto layer = talk::Library::personalLayer(npc);
    if (rows.rows.empty())
    {
        npcVoice_.erase(npc);
        talk_.setGrammar(layer, "grammar " + layer + "\n", problem);
        talk_.setCharacter(npc, "", problem);
        voiceLayers_.erase(npc), voiceRules_.erase(npc), mantleOf_.erase(npc);
        voicesChanged();
        talkDaily_.erase(npc);
        return {true, npc + " has no voice row: they speak from the library.", npc};
    }
    const auto& row = rows.rows.front();
    const auto text = [&](std::size_t i) { return i < row.size() && row[i] ? *row[i] : std::string(); };
    NpcVoice v;
    v.from = text(1), v.accent = text(2), v.quarter = text(3), v.quirk = text(5), v.persona = text(6);
    json::Value traits;
    std::string error;
    if (json::parse(text(4), traits, error) && traits.isObject())
        for (const auto& [axis, value] : traits.fields())
            if (value.isString())
                v.traits[axis] = value.asString("");
    if (row.size() >= 11 && !text(9).empty() && (text(10) == "t" || text(10) == "true"))
        v.mantle = text(9);
    bool ok = true;
    if (const auto grammar = text(7); !grammar.empty())
    {
        if (talk_.setGrammar(layer, "grammar " + layer + "\n" + grammar, problem))
            voiceLayers_.insert(npc);
        else
            ok = false, said += " Their own words were refused: " + problem;
    }
    else if (voiceLayers_.erase(npc))
        talk_.setGrammar(layer, "grammar " + layer + "\n", problem);
    if (const auto own = text(8); !own.empty())
    {
        if (talk_.setCharacter(npc, own, problem))
            voiceRules_.insert(npc);
        else
            ok = false, said += " Their own lines were refused: " + problem;
    }
    else if (voiceRules_.erase(npc))
        talk_.setCharacter(npc, "", problem);
    const auto mantle = v.mantle;
    npcVoice_[npc] = std::move(v);
    voicesChanged();
    talkDaily_.erase(npc);
    favourites_.erase(npc);
    if (mantle.empty())
        mantleOf_.erase(npc);
    else
    {
        const auto loaded = loadMantle(mantle);
        ok = ok && loaded.ok;
        said += " " + loaded.message;
    }
    return {ok, npc + "'s voice is read again." + said, npc};
}
} // namespace ratw::game
