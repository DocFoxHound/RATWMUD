// Cheaper NPC voices, in the game (Docs/Design/28-ai-cost.md): what a player asks that the world can answer (a
// greeting, a price, the hours, a way, the watch, the day) is answered from the world, in the NPC's tone, without a
// model; and a ledger line for every NPC line said, recording who answered it but none of the words.
#include "RatwGame.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <ctime>

namespace ratw::game
{
namespace
{
constexpr double RepeatWithin = 600;                // World seconds: the same thing asked again is answered "as I said".
std::string lower(std::string s)
{
    for (auto& c : s)
        c = char(std::tolower(static_cast<unsigned char>(c)));
    return s;
}
std::string plainWords(const std::string& s)
{
    std::string out;
    for (const char c : lower(s))
        out += std::isalnum(static_cast<unsigned char>(c)) ? c : ' ';
    std::string squeezed;
    for (const char c : out)
        if (c != ' ' || (!squeezed.empty() && squeezed.back() != ' '))
            squeezed += c;
    while (!squeezed.empty() && squeezed.back() == ' ')
        squeezed.pop_back();
    return squeezed;
}
// "eight in the morning", "noon", "five in the afternoon".
std::string hourWords(double hour)
{
    static const char* const numbers[] = {"twelve", "one", "two", "three", "four", "five", "six", "seven", "eight",
                                          "nine", "ten", "eleven"};
    const int h = int(std::floor(std::fmod(hour + 24, 24)));
    if (h == 0)
        return "midnight";
    if (h == 12)
        return "noon";
    return std::string(numbers[h % 12]) + (h < 12 ? " in the morning" : h < 18 ? " in the afternoon" : " in the evening");
}
std::string compass(double dx, double dy)
{
    static const char* const points[] = {"east", "south-east", "south", "south-west", "west", "north-west", "north", "north-east"};
    const double angle = std::atan2(dy, dx);
    return points[int(std::lround(angle / (3.14159265358979 / 4)) + 8) % 8];
}
} // namespace

void Game::voiced(const char* kind, const char* route, const std::string& npcId)
{
    if (!voiceLog_.is_open())
        return;
    // The ledger tools/ai_cost.py reads: no words, only when, what kind, who, and which route answered.
    auto line = json::Value::object();
    line.add("t", double(std::time(nullptr)));
    line.add("kind", std::string(kind));
    line.add("route", std::string(route));
    line.add("npc", npcId);
    voiceLog_ << json::dump(line) << '\n';
    voiceLog_.flush();
}

std::string Game::gameAnswer(const std::string& npcId, const std::string& playerId, const std::string& heard, bool identified)
{
    if (!voices_.routes())
        return {};
    const auto* npc = world_.entity(npcId);
    const auto* player = world_.entity(playerId);
    if (!npc || !player)
        return {};
    const auto& society = world_.society();
    const auto* spec = society.spec(npcId);
    const auto tone = voices_.toneOf(spec ? spec->personality : std::string());
    const auto* bond = identified ? world_.bonds().find(npcId, playerId) : nullptr;
    const std::string band = bond && bond->familiarity >= 25 ? "known" : "stranger";
    const auto key = npcId + "|" + (identified ? playerId : std::string("?"));
    const auto asked = voice::Rules::normalise(heard, npc->name);
    const auto seed = std::hash<std::string>{}(key) + voiceSeed_++;
    auto& last = lastSaid_[key];
    const auto remember = [&](const std::string& answer, const std::string& line) {
        last = {asked, answer, line, world_.time()};
        return line;
    };
    // The same thing asked again, soon after: "as I said".
    if (!asked.empty() && last.asked == asked && world_.time() - last.at < RepeatWithin && !last.answer.empty())
        if (const auto line = voices_.line("repeat", band, tone, {{"answer", last.answer}}, seed, last.line); !line.empty())
            return remember(last.answer, line);
    const auto intent = voices_.intent(heard, npc->name);
    if (intent.id.empty())
        return {};
    // A wolf lying hurt, or held in the gaol, isn't met with a doorstep greeting: a model answers what they said.
    if ((player->downedLeft > 0 || player->hurt >= 50 || world_.custodyOf(playerId)) &&
        (intent.id == "greet" || intent.id == "thanks" || intent.id == "bye"))
        return {};
    std::map<std::string, std::string> facts{{"name", npc->name}};
    if (identified && knowsName(npcId, playerId))
        facts["player"] = labelFor(npcId, playerId);     // Only a name they were given (doc 32).
    // Their own greeting, the first time they meet.
    if (spec && !spec->greeting.empty() && (!identified || memories_.recall(npcId, playerId).empty()))
        facts["greeting"] = spec->greeting;
    std::string answer;
    const auto* job = society.jobOf(npcId);
    const auto placeName = [&](const std::string& cellId) {
        const auto* c = world_.cell(cellId);
        return c ? c->name : cellId;
    };
    if (intent.id == "trade")
    {
        if (society.merchant(npcId))
        {
            const auto* stock = society.account(npcId);
            std::vector<std::string> goods;
            for (const auto* item : {"meal", "herbs"})
            {
                const int held = stock ? Society::stock(*stock, item) : 0;
                const auto quote = society.quote(playerId, npcId, item, 1, true);
                const std::string what = std::string(item) == "meal" ? "A meal" : "A bundle of herbs";
                if (held > 0 && quote.unitPrice > 0)
                    goods.push_back(what + " is " + std::to_string(quote.unitPrice) + (quote.unitPrice == 1 ? " penny" : " pennies") +
                                    " (" + std::to_string(held) + " left)");
                else
                    goods.push_back(std::string(item) == "meal" ? "I'm out of meals" : "I'm out of herbs");
            }
            answer = goods[0] + ". " + goods[1] + ".";
        }
        else
        {
            // Not a trader: send them to the nearest one in town.
            const auto community = world_.communityOf(npc->cellId);
            for (const auto& p : society.positions())
                if (p.role == "merchant" && world_.communityOf(p.work.cell) == community)
                    if (const auto held = society.state().careers.positions.find(p.id);
                        held != society.state().careers.positions.end() && !held->second.holder.empty())
                        if (const auto* who = world_.entity(held->second.holder); who && !who->dead)
                        {
                            answer = "I've nothing to sell. " + who->name + " keeps shop at " + placeName(p.work.cell) + ".";
                            break;
                        }
        }
    }
    else if (intent.id == "hours" && job)
    {
        const auto workWord = society.merchant(npcId) ? "I keep shop" : "I work";
        answer = std::string(workWord) + " from " + hourWords(job->startHour) + " until " + hourWords(job->endHour) + ".";
        if (society.merchant(npcId))
            answer += " Mornings only on Restday, and on Marketday you'll find me at a stall in the market.";
        if (job->role == "guard")
            answer = "I keep the watch from " + hourWords(job->startHour) + " until " + hourWords(job->endHour) + ".";
    }
    else if (intent.id == "job")
    {
        std::string doing = spec ? spec->workLabel : std::string();
        if (!willName(npcId, playerId))                    // Not to them, not yet (doc 32).
            answer = !doing.empty() ? "My name's my own business. I spend my days " + doing + "." : "My name's my own business.";
        else if (job && job->role == "guard")
            answer = "I'm " + npc->name + ", of the watch.";
        else if (const auto* master = society.apprenticedTo(npcId))
            answer = "I'm " + npc->name + ". I'm learning the trade: " + master->title + ".";
        else if (!doing.empty())
            answer = "I'm " + npc->name + ". I spend my days " + doing + ".";
        else
            answer = "I'm " + npc->name + ".";
    }
    else if (intent.id == "where" && !intent.target.empty())
    {
        const auto target = plainWords(intent.target);
        // Someone they'd know of: a resident, by name (never where a player is).
        for (const auto& [id, e] : world_.entities())
        {
            if (!e.npc || e.transient || id == npcId)
                continue;
            const auto name = plainWords(e.name);
            const auto first = name.substr(0, name.find(' '));
            if (name != target && first != target)
                continue;
            if (e.dead)
                answer = e.name + "? " + e.name + " is dead.";
            else if (const auto* theirs = society.jobOf(id))
                answer = e.name + " works at " + placeName(theirs->work.cell) + ".";
            else if (const auto* life = society.resident(id))
                answer = e.name + " lives at " + placeName(life->homeCell) + ".";
            break;
        }
        // Or a place, by its name: which way, from here.
        if (answer.empty())
        {
            const auto where = [&](const std::string& cellId, Vec2 p, bool& known) {
                // A place's position in the world; an interior by the door that leads into it.
                known = false;
                const auto* c = world_.cell(cellId);
                if (!c)
                    return Vec2{};
                if (c->outdoors)
                {
                    known = true;
                    return Vec2{c->worldX + p.x, c->worldY + p.y};
                }
                for (const auto& [id, d] : world_.doors())
                    if (d.targetCell == cellId)
                        if (const auto* out = world_.cell(d.cellId); out && out->outdoors)
                        {
                            known = true;
                            return Vec2{out->worldX + d.position.x, out->worldY + d.position.y};
                        }
                return Vec2{};
            };
            for (const auto& [id, c] : world_.cells())
            {
                const auto name = plainWords(c.name);
                if (name != target && name != "the " + target && plainWords(id) != target)
                    continue;
                if (id == npc->cellId)
                {
                    answer = "You're in " + c.name + " now.";
                    break;
                }
                bool fromKnown = false, toKnown = false;
                const auto from = where(npc->cellId, npc->position, fromKnown);
                const auto to = where(id, {c.width / 2.0, c.height / 2.0}, toKnown);
                if (!fromKnown || !toKnown)
                    break;
                const double dx = to.x - from.x, dy = to.y - from.y, far = std::hypot(dx, dy);
                answer = c.name + " is " + (far < 40 ? "close by" : far < 200 ? "a short walk" : "a long way") + " to the " +
                         compass(dx, dy) + ".";
                break;
            }
        }
    }
    else if (intent.id == "wanted")
    {
        if (!job || job->role != "guard")
            return {};                              // Only the watch knows; anyone else, a model answers.
        if (const auto* warrant = world_.warrantFor(playerId))
        {
            std::string charges;
            for (const auto& inc : world_.crime().incidents)
                if (std::find(warrant->incidents.begin(), warrant->incidents.end(), inc.id) != warrant->incidents.end())
                    charges += (charges.empty() ? "" : ", ") + inc.kind;
            answer = "You're wanted for " + charges + ". " + std::to_string(world_.owedBy(*warrant)) +
                     " pennies settles it, with any of the watch.";
        }
        else
            answer = "Not that I know of. Keep it that way.";
    }
    else if (intent.id == "day")
    {
        const auto plan = world_.dayPlan(world_.communityOf(npc->cellId));
        answer = "It's " + calendar::weekdayName(calendar::weekdayOf(world_.calendarDays()));
        answer += plan.kind == "festival" ? ", and " + plan.name + " today: the town gathers at the market from noon."
                  : plan.kind == "market" ? ": market day. The stalls are out until two."
                  : plan.kind == "rest"   ? ", the day of rest."
                                          : ".";
    }
    else if (intent.id != "greet" && intent.id != "thanks" && intent.id != "bye")
        return {};
    if (intent.id != "greet" && intent.id != "thanks" && intent.id != "bye")
    {
        if (answer.empty())
            return {};                              // Recognised, but nothing true to say: a model answers.
        facts["answer"] = answer;
    }
    const auto line = voices_.line(intent.id, band, tone, facts, seed, last.line);
    return line.empty() ? std::string() : remember(answer.empty() ? line : answer, line);
}
} // namespace ratw::game
