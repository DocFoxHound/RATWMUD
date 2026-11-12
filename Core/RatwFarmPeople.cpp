// The farms' people (Docs/Design/71-the-country-s-industries.md, Phase C; the user, 2026-11-03): real NPCs replace the
// farms' virtual crews. Every farm's area is on (doc 79, 17: the switch, the tills and the crews' money went with Phase
// 3), and each of its posts is filled from the unplaced NPCs (doc 66, 2), by their old trade (doc 71, 3's table), deterministically: its keeper the oldest suitable
// adult of its trades with family among the unplaced where one has, and that family (spouse, children, parents) with
// them at the farmhouse, nobody else; its hands of working age from its own trades first (a family member of them among
// them), then the fallback trades; the hands housed in the farm's community (Data/Economy/communities.json, at most
// HouseholdMost a home) or, near the city, as doc 68's arrivals are. A community's market (its store, inn or alehouse,
// bakehouse) is staffed when its first farm is, its keepers of the towns' trades and shops. Each post filled is a
// position of its own (its holder its founder, as every authored resident's is) at the farm's work room. Its purse is
// the staged economy's (doc 79, 6.5: set nightly to its household's band). Nobody is ever made: the host places each one
// (a "farm" request: arrive).
#include "RatwItems.h"
#include "RatwSociety.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <tuple>

namespace ratw
{
namespace
{
std::uint64_t fnv(const std::string& text)
{
    std::uint64_t h = 1469598103934665603ULL;
    for (const unsigned char c : text)
        h = (h ^ c) * 1099511628211ULL;
    return h;
}
// A lot from 0 to 1, fixed by its words (the same on every build).
double lot(const std::string& words)
{
    return double(fnv(words) % 1000003ULL) / 1000003.;
}
std::string lowered(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    return s;
}
std::string filled(std::string text, const std::string& key, const std::string& value)
{
    for (auto at = text.find(key); at != std::string::npos; at = text.find(key, at + value.size()))
        text.replace(at, key.size(), value);
    return text;
}
int wordsIn(const std::string& label, const std::vector<std::string>& words)
{
    const auto l = lowered(label);
    int n = 0;
    for (const auto& w : words)
        n += !w.empty() && l.find(w) != std::string::npos;
    return n;
}
} // namespace

int Society::farmOfPost(const Position& p, bool* keeper) const
{
    if (keeper)
        *keeper = false;
    const auto at = farmWorkplace_.find(p.work.cell);
    if (at == farmWorkplace_.end() || at->second >= farms_.size())
        return -1;
    const auto& f = farms_[at->second];
    const bool keeps = p.title == farmLabel(f, 0);
    if (f.outdoors && !keeps && p.title != farmLabel(f, 1))
        return -1;                                  // (Another's post in a works' cell out of doors: doc 72.)
    if (keeper)
        *keeper = keeps;
    return int(at->second);
}

std::string Society::reachOf(const Position& p) const
{
    const int farm = farmOfPost(p);
    if (farm < 0 || !farms_[std::size_t(farm)].outdoors)
        return p.work.cell;
    const auto& people = items::farmPeople();
    const auto* site = people.farm(farms_[std::size_t(farm)].def->id);
    if (!site)
        return p.work.cell;
    if (const auto* community = site->community == "ridgemere" ? nullptr : people.community(site->community))
        return community->homes.empty() ? p.work.cell : community->homes.front().area;
    for (const auto& place : people.farms)
        if (!place.works && place.community == "ridgemere" && !place.workplaces.empty())
            return place.workplaces[0].area;      // (The city's farms' work room, as staffFarms judges the city's homes.)
    return p.work.cell;
}

std::string Society::farmLabel(const FarmNow& f, int post) const
{
    const auto& people = items::farmPeople();
    const auto* place = people.farm(f.def->id);
    const auto kind = people.kinds.find(f.def->kind);
    if (!place || kind == people.kinds.end())
        return {};
    const auto& k = kind->second;
    const auto& text = post == 0 ? k.keeper : post == 2 && !k.second.empty() ? k.second : k.hand;
    return filled(text, "{place}", people.placeName(*place));
}

void Society::countFarmPosts()
{
    for (auto& f : farms_)
        f.postKeepers = f.postHands = f.heldKeepers = f.heldHands = 0;
    if (farmWorkplace_.empty())
        return;
    for (const auto& p : positions_)
    {
        bool keeper = false;
        const int farm = farmOfPost(p, &keeper);
        if (farm < 0)
            continue;
        auto& f = farms_[std::size_t(farm)];
        const auto held = state_.careers.positions.find(p.id);
        const bool filled = held != state_.careers.positions.end() && !held->second.holder.empty() &&
                            !dead_.count(held->second.holder) && state_.residents.count(held->second.holder);
        ++(keeper ? f.postKeepers : f.postHands);
        if (filled)
            ++(keeper ? f.heldKeepers : f.heldHands);
    }
}

void Society::staffFarms(const CareerWorld& world, std::vector<CareerNote>& notes)
{
    const auto& people = items::farmPeople();
    if (roster_ != Roster::Authored || !people.loaded || !world.place || farms_.empty() || authored_.unplaced.empty())
        return;
    // (The last day's placements not yet carried out by the host: wait for them.)
    if (std::any_of(requests_.begin(), requests_.end(), [](const ResidentRequest& r) { return r.kind == "farm"; }))
        return;
    countFarmPosts();
    // The pool: the unplaced NPCs not already sent for, nor protected, nor of the Heights' class.
    std::set<std::string> taken;
    for (const auto& r : requests_)
        taken.insert(r.templateId);
    std::map<std::string, const ResidentSpec*> pool;
    std::map<std::string, std::string> tradeOf;
    for (const auto& r : authored_.unplaced)
        if (!r.protectedNpc && !taken.count(r.id))
        {
            pool[r.id] = &r;
            tradeOf[r.id] = people.tradeOf(r.workLabel);
        }
    // The pool by trade, each in id order as the pool is (choose looks only at a trade's own: a fresh world's first
    // staffing looked over the whole pool for each of 206 placements, about 90 ms in one tick; the repair after doc 79's
    // Phases 4 and 5 merged). Kept with the pool: placing someone takes them out of both.
    std::map<std::string, std::map<std::string, const ResidentSpec*>> poolOf;
    for (const auto& [id, r] : pool)
        poolOf[tradeOf[id]].emplace_hint(poolOf[tradeOf[id]].end(), id, r);
    // Their families among the unplaced (doc 62's kin): spouses, children, parents.
    std::map<std::string, std::vector<std::string>> spouses, children, parents;
    for (const auto& k : authored_.kin)
    {
        if (!pool.count(k.a) || !pool.count(k.b))
            continue;
        if (k.kind == "spouse")
            spouses[k.a].push_back(k.b), spouses[k.b].push_back(k.a);
        else if (k.kind == "parent")
            children[k.a].push_back(k.b), parents[k.b].push_back(k.a);
    }
    // The Heights' own (a House's family: its class upper, its trade the Heights', or a spouse, parent or child of one)
    // keep to the Heights: no farm or market post is theirs.
    // (Each one's worked out once: their classes and kin don't move while the farms are staffed, and a fresh world's
    // first staffing asked it of the whole pool for each placement.)
    std::map<std::string, bool> heightsOf, upperOf;
    const auto heights = [&](const std::string& id) {
        if (const auto found = heightsOf.find(id); found != heightsOf.end())
            return found->second;
        const bool is = classOf(id) == "upper" || tradeOf[id] == "the Heights";
        heightsOf.emplace(id, is);
        return is;
    };
    const auto upper = [&](const std::string& id) {
        if (const auto found = upperOf.find(id); found != upperOf.end())
            return found->second;
        bool is = heights(id);
        for (const auto* kin : {&spouses, &children, &parents})
            if (const auto found = kin->find(id); !is && found != kin->end())
                for (const auto& other : found->second)
                    if (heights(other))
                    {
                        is = true;
                        break;
                    }
        upperOf.emplace(id, is);
        return is;
    };
    const auto working = [&](const ResidentSpec& r) { return r.age >= 16 && r.age < retireAge(r.id); };
    // Who among the pool is free, the best of a list of trades by a score (highest wins; the lot of the farm last).
    const auto choose = [&](const std::vector<std::string>& trades, const std::string& key,
                            const std::function<std::tuple<int, int, int>(const ResidentSpec&)>& score,
                            const std::function<bool(const ResidentSpec&)>& fits) -> const ResidentSpec* {
        for (const auto& trade : trades)
        {
            const ResidentSpec* best = nullptr;
            std::tuple<int, int, int, double> top{};
            const auto ofTrade = poolOf.find(trade);
            if (ofTrade == poolOf.end())
                continue;
            for (const auto& [id, r] : ofTrade->second)
            {
                if (!working(*r) || upper(id) || !fits(*r))
                    continue;
                const auto [a, b, c] = score(*r);
                const std::tuple<int, int, int, double> s{a, b, c, lot(id + "|" + key)};
                if (!best || s > top)
                    best = r, top = s;
            }
            if (best)
                return best;
        }
        return nullptr;
    };
    const auto familyOf = [&](const std::string& id) {
        // Spouse, then children (the eldest first), then parents: the owner's own and its spouse's, each once.
        std::vector<std::string> out;
        const auto add = [&](const std::string& who) {
            if (who != id && pool.count(who) && std::find(out.begin(), out.end(), who) == out.end())
                out.push_back(who);
        };
        for (const auto& s : spouses[id])
            add(s);
        std::vector<std::string> kids;
        for (const auto& c : children[id])
            if (pool.count(c))
                kids.push_back(c);
        for (const auto& s : spouses[id])
            for (const auto& c : children[s])
                if (pool.count(c))
                    kids.push_back(c);
        std::sort(kids.begin(), kids.end(), [&](const std::string& a, const std::string& b) {
            return std::make_tuple(-pool.at(a)->age, a) < std::make_tuple(-pool.at(b)->age, b);
        });
        for (const auto& c : kids)
            add(c);
        for (const auto& p : parents[id])
            add(p);
        return out;
    };
    // Who lives where now (the living), and who is to: a home's household size.
    std::map<std::string, int> living;
    std::map<std::string, std::vector<std::string>> members;
    for (const auto& [id, life] : state_.residents)
        if (!dead_.count(id) && (!world.alive || world.alive(id)) && !life.homeCell.empty())
        {
            ++living[life.homeCell];
            members[life.homeCell].push_back(id);
        }
    const int most = std::min(HouseholdMost, people.householdMost);
    const auto place = [&](const ResidentSpec& r, const Spot& home, const Spot& work, const std::string& label, const std::string& role,
                           double start, double end, std::int64_t purse, const std::string& farm, const std::string& what) {
        ResidentRequest q;
        q.kind = "farm";
        q.templateId = r.id;
        q.name = r.name;
        q.age = r.age;
        q.home = home;
        q.work = label == "-" ? home : work;
        q.workLabel = label;
        q.role = role;
        q.startHour = start;
        q.endHour = end;
        q.purse = std::max<std::int64_t>(0, purse);
        q.farm = farm;
        requests_.push_back(q);
        if (const auto t = tradeOf.find(r.id); t != tradeOf.end())
            if (const auto list = poolOf.find(t->second); list != poolOf.end())
                list->second.erase(r.id);
        pool.erase(r.id);
        ++living[home.cell];
        ++farmPlaced_[what];
        notes.push_back({"farm placement", r.id, farm, label == "-" ? "family, at " + home.cell : label});
    };
    const auto bedOf = [&](const items::PlaceRoom& room) {
        const auto& s = room.spots[std::size_t(living[room.area]) % room.spots.size()];
        return Spot{room.area, s.first, s.second};
    };
    // A home in a community for one more: the first of its homes with room (at most `most`, and its capacity).
    // (A business's flat: the one `for` its name, or, a country workshop's (doc 72, Phase 5), its building's `<area>_flat`.)
    const auto communityHome = [&](const items::CommunityPlace& c, const std::string& flatFor,
                                   const std::string& flatArea = {}) -> Spot {
        if (!flatFor.empty())
            for (const auto& flat : c.flats)
                if ((flat.forName == flatFor || (!flatArea.empty() && flat.area == flatArea)) && world.place(flat.area) &&
                    living[flat.area] < std::min(most, flat.capacity))
                    return bedOf(flat);
        for (const auto& home : c.homes)
            if (world.place(home.area) && living[home.area] < std::min(most, home.capacity))
                return bedOf(home);
        return {};
    };
    // A home near the city (doc 68's arrivals): with a household of its class within reach of the work that has room,
    // the smallest first; else the communal lodgings within reach. Nowhere: none.
    // A household's class, by most of its members (each home's worked out once, and again when someone is placed there:
    // a fresh world's first staffing worked out every home's for each of its city hands, about 65 ms in one tick; the
    // repair after doc 79's Phases 4 and 5 merged).
    std::map<std::string, std::string> topOf;
    const auto topClassOf = [&](const std::string& cell, const std::vector<std::string>& ids) -> const std::string& {
        if (const auto found = topOf.find(cell); found != topOf.end())
            return found->second;
        std::map<std::string, int> count;
        for (const auto& m : ids)
            ++count[classOf(m)];
        std::string top;
        for (const auto& [c, n] : count)
            if (top.empty() || n > count[top])
                top = c;
        return topOf.emplace(cell, std::move(top)).first->second;
    };
    const auto cityHome = [&](const std::string& cls, const std::string& workCell, const std::string& who) -> Spot {
        Spot best;
        std::pair<int, double> roomiest{0, 0};
        for (const auto& [cell, ids] : members)
        {
            if (communalHome(cell) || living[cell] >= most || (world.near && !world.near(workCell, cell)) ||
                cell.rfind("fs_", 0) == 0)
                continue;                           // (A farmhouse is its keeper's family's alone.)
            if (topClassOf(cell, ids) != cls)
                continue;
            const std::pair<int, double> score{-living[cell], lot(cell + "|" + who)};
            if (best.cell.empty() || score > roomiest)
                if (const auto* first = resident(ids.front()))
                    roomiest = score, best = {cell, first->homeX, first->homeY};
        }
        if (best.cell.empty() && world.lodgings)
            if (const auto spots = world.lodgings(workCell); !spots.empty())
                best = spots.front();
        return best;
    };
    // Where a works' crew that lives in the city is near (doc 72): its work is out of doors in the realm's own region, so
    // the city's homes are judged from the city's farms' work rooms (their region the city's), as those farms' hands are.
    std::string cityWork;
    for (const auto& place : people.farms)
        if (!place.works && place.community == "ridgemere" && !place.workplaces.empty() && world.place(place.workplaces[0].area))
        {
            cityWork = place.workplaces[0].area;
            break;
        }
    const auto nearCity = [&](const items::FarmPlace& site) {
        return site.works && !cityWork.empty() ? cityWork : site.workplaces[0].area;
    };
    std::set<std::string> markets;                  // Communities whose farms are staffed (now or before).
    // Two passes over the farms whose posts are wanted: every keeper (and family) first, so the most experienced of a
    // trade keep its farms; then the hands. What each still wants is kept between them.
    struct Wants
    {
        int keepers = 0, hands = 0, second = 0;
        int nextSpot[2] = {0, 0};
        bool begun = false;
    };
    std::map<std::size_t, Wants> wants;
    for (int pass = 0; pass < 2; ++pass)
    for (auto& f : farms_)
    {
        const auto* site = people.farm(f.def->id);
        const auto kind = people.kinds.find(f.def->kind);
        if (!site || kind == people.kinds.end() ||
            (!site->works && !world.place(site->home.area)) ||
            std::any_of(site->workplaces.begin(), site->workplaces.end(), [&](const items::PlaceRoom& w) { return !world.place(w.area); }))
            continue;
        const auto& k = kind->second;
        const auto* community = site->community == "ridgemere" ? nullptr : people.community(site->community);
        if (community)
            markets.insert(community->id);
        auto& want = wants[std::size_t(&f - farms_.data())];
        if (!want.begun)
        {
            want.begun = true;
            want.keepers = f.keepers - f.postKeepers;
            want.hands = f.hands - f.postHands;
            // The posts at its second work room (a mill, a dairy, a cellar) already there.
            int atSecond = 0;
            if (site->workplaces.size() > 1)
                for (const auto& p : positions_)
                    if (p.work.cell == site->workplaces[1].area)
                        ++atSecond;
            want.second = site->workplaces.size() > 1 ? std::max(0, std::min(k.secondCount - atSecond, want.hands)) : 0;
            // The posts at each work room in use count against its spots, so the newcomers take the next ones.
            for (const auto& p : positions_)
                for (int room = 0; room < 2 && room < int(site->workplaces.size()); ++room)
                    if (p.work.cell == site->workplaces[std::size_t(room)].area)
                        ++want.nextSpot[room];
        }
        int& needKeepers = want.keepers;
        int& needHands = want.hands;
        int& needSecond = want.second;
        if (needKeepers <= 0 && needHands <= 0)
            continue;
        const auto workSpot = [&](int room) {
            const auto& w = site->workplaces[std::size_t(room)];
            if (w.spots.empty())
                return Spot{w.area, 1.5, 1.5};
            const auto& s = w.spots[std::size_t(want.nextSpot[room]++) % w.spots.size()];
            return Spot{w.area, s.first, s.second};
        };
        const auto hire = [&](const ResidentSpec& r, bool second, const Spot& home, const std::string& what) {
            const auto label = farmLabel(f, second ? 2 : 1);
            place(r, home, workSpot(second ? 1 : 0), label, "civilian", people.farmStart, people.farmEnd, 0, f.def->id, what);
            --needHands;
            if (second)
                --needSecond;
        };
        // Its keeper: of its trades, the most experienced (the oldest) of working age, with family among the unplaced
        // where one has; then the fallback trades. With them, their family at the farmhouse (nobody else lives there):
        // one of working age and of the farm's trades works as one of its hands. A works (doc 72) has no farmhouse: its
        // keeper, of those with no family among the unplaced first (doc 72, 4.8), comes alone and lives with the hands.
        const bool farmhouseEmpty = !site->works && living[site->home.area] == 0;
        if (pass == 0 && needKeepers > 0 && site->works)
        {
            const auto score = [&](const ResidentSpec& r) {
                return std::make_tuple(wordsIn(r.workLabel, k.words) > 0 ? 1 : 0, familyOf(r.id).empty() ? 1 : 0, r.age);
            };
            const auto adult = [](const ResidentSpec& r) { return r.age >= 18; };
            const auto* owner = choose(k.trades, f.def->id + "|keeper", score, adult);
            if (!owner)
                owner = choose(people.fallback, f.def->id + "|keeper", score, adult);
            if (owner)
            {
                const auto ownerSpec = *owner;
                const auto cls = classOf(ownerSpec.id).empty() ? std::string("lower") : classOf(ownerSpec.id);
                // (In its community, the keeper's house `for` the works where the map built one: Frosthollow's.)
                const auto home = community ? communityHome(*community, site->steading) : cityHome(cls, nearCity(*site), ownerSpec.id);
                if (!home.cell.empty())
                {
                    place(ownerSpec, home, workSpot(0), farmLabel(f, 0), "civilian", people.farmStart, people.farmEnd, 0, f.def->id,
                          "keeper");
                    --needKeepers;
                    if (!community)
                    {
                        members[home.cell].push_back(ownerSpec.id);
                        topOf.erase(home.cell);
                    }
                }
            }
        }
        if (pass == 0 && needKeepers > 0 && !site->works)
        {
            const auto score = [&](const ResidentSpec& r) {
                return std::make_tuple(wordsIn(r.workLabel, k.words) > 0 ? 1 : 0, farmhouseEmpty && !familyOf(r.id).empty() ? 1 : 0, r.age);
            };
            const auto adult = [](const ResidentSpec& r) { return r.age >= 18; };
            const auto* owner = choose(k.trades, f.def->id + "|keeper", score, adult);
            if (!owner)
                owner = choose(people.fallback, f.def->id + "|keeper", score, adult);
            if (owner)
            {
                const auto family = farmhouseEmpty ? familyOf(owner->id) : std::vector<std::string>{};
                const auto ownerSpec = *owner;      // (The pool's pointer goes with it.)
                place(ownerSpec, bedOf(site->home), workSpot(0), farmLabel(f, 0), "civilian", people.farmStart, people.farmEnd, 0, f.def->id,
                      "keeper");
                --needKeepers;
                const int room = std::min(most, site->home.capacity);
                for (const auto& id : family)
                {
                    if (living[site->home.area] >= room)
                        break;
                    const auto found = pool.find(id);
                    if (found == pool.end())
                        continue;
                    const auto member = *found->second;
                    const bool ofTrade = std::find(k.trades.begin(), k.trades.end(), tradeOf[id]) != k.trades.end();
                    if (needHands > 0 && working(member) && ofTrade && !upper(id))
                        hire(member, needSecond > 0 && wordsIn(member.workLabel, k.secondWords) > 0, bedOf(site->home), "family hand");
                    else
                        place(member, bedOf(site->home), {}, "-", "civilian", 0, 0, 0, f.def->id, "family");
                }
            }
        }
        // Its hands: the second work room's first, those whose labels hold its words first; then the main room's; of the
        // farm's own trades, then the fallback; housed in its community, or near the city as arrivals are.
        while (pass == 1 && needHands > 0)
        {
            const bool second = needSecond > 0;
            const auto& words = second ? k.secondWords : k.words;
            const auto score = [&](const ResidentSpec& r) {
                return std::make_tuple(wordsIn(r.workLabel, words) > 0 ? 1 : 0, (spouses[r.id].empty() && children[r.id].empty()) ? 1 : 0, 0);
            };
            const auto any = [](const ResidentSpec&) { return true; };
            const auto key = f.def->id + "|hand|" + std::to_string(needHands);
            const auto* hand = choose(k.trades, key, score, any);
            if (!hand)
                hand = choose(people.fallback, key, score, any);
            if (!hand)
                break;
            const auto spec = *hand;
            const auto work = site->workplaces[second ? 1 : 0].area;
            const auto home = community ? communityHome(*community, {}) : cityHome("lower", second ? work : nearCity(*site), spec.id);
            if (home.cell.empty())
                break;                              // (No room for them: not today.)
            hire(spec, second, home, std::string("hand ") + (community ? community->id : "ridgemere"));
            if (!community)
            {
                members[home.cell].push_back(spec.id);   // (Counted with that household for the next.)
                topOf.erase(home.cell);
            }
        }
    }
    // The communities' markets (communities.json `market`): a post at each of their businesses not yet there, its keeper
    // of the towns' trades and shops (the post's words first, the most experienced), its hands of service and labour;
    // the keeper in the flat over the business where it has one, the rest in the community's houses. Their purses as
    // arrivals' (doc 79, 6.5).
    for (const auto& id : markets)
    {
        const auto* c = people.community(id);
        if (!c)
            continue;
        // (Its square's stand last: the community's carters, out of doors by the wagon: doc 79, 8.1.)
        std::vector<const items::MarketPlace*> businesses;
        for (const auto& m : c->market)
            businesses.push_back(&m);
        if (!c->stand.posts.empty())
            businesses.push_back(&c->stand);
        for (const auto* business : businesses)
        {
            const auto& m = *business;
            if (!world.place(m.area))
                continue;
            int spot = 0;
            bool kept = false;                      // (A keeper there, or sent for: a country workshop's hands wait for one.)
            for (const auto& p : positions_)
            {
                spot += p.work.cell == m.area;
                kept = kept || (p.work.cell == m.area && p.role == "merchant");
            }
            const auto* workshop = people.workshop(m.area);
            for (const auto& [post, count] : m.posts)
            {
                const auto rules = people.market.find(post);
                if (rules == people.market.end())
                    continue;
                const auto& rule = rules->second;
                if (workshop && rule.role != "merchant" && !kept)
                    continue;
                const auto label = filled(rule.label, "{name}", m.name);
                int have = 0;
                for (const auto& p : positions_)
                    have += p.work.cell == m.area && p.title == label;
                for (int n = have; n < count; ++n)
                {
                    const bool keeper = rule.role == "merchant";
                    // (One who kept a shop of the town of the same name on the old map first: Larkfield General's
                    // shopkeeper for Larkfield Stores; one of another community's name last, kept for theirs: staffed
                    // first, Fenshore took Frosthollow Stores' shopkeeper, and Frosthollow then Larkfield's.)
                    const auto town = lowered(c->name);
                    const auto namesake = [&](const ResidentSpec& r) {
                        const auto l = lowered(r.workLabel);
                        if (l.find(town) != std::string::npos)
                            return 2;
                        for (const auto& other : people.communities)
                            if (other.id != c->id && l.find(lowered(other.name)) != std::string::npos)
                                return 0;
                        return 1;
                    };
                    const auto score = [&](const ResidentSpec& r) {
                        return std::make_tuple(wordsIn(r.workLabel, rule.words) > 0 ? 1 : 0, namesake(r), keeper ? r.age : 0);
                    };
                    const auto* who = choose(rule.trades, m.area + "|" + post + "|" + std::to_string(n), score,
                                             [](const ResidentSpec& r) { return r.age >= 18; });
                    if (!who)
                        break;
                    const auto spec = *who;
                    const auto home = communityHome(*c, keeper ? m.name : std::string(), keeper ? m.area + "_flat" : std::string());
                    if (home.cell.empty())
                        break;
                    const auto& s = m.work[std::size_t(spot++) % m.work.size()];
                    place(spec, home, {m.area, s.first, s.second}, label, rule.role, people.marketStart, people.marketEnd,
                          std::min<std::int64_t>(spec.purse, ArrivalPurse), "market:" + c->id,
                          workshop ? "workshop " + m.area : "market " + c->id);
                    kept = kept || keeper;
                }
            }
        }
    }
}
} // namespace ratw
