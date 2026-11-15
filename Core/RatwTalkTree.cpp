// Dialogue trees (RatwTalk.h, Tree; Docs/Design/83-dialogue-trees.md): a .tree file read into nodes and choices, its
// NPC lines compiled into the talk library's rules (concept dlg.<tree>.<node>), and what a node offers worked out.
// The game runs a conversation (Core/RatwGameDialogue.cpp); nothing here knows the world.
#include "RatwTalk.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <sstream>

namespace ratw::talk
{
namespace
{
constexpr std::size_t MostShown = 9;          // Choices a window shows (the keys 1 to 9).

std::string trimmed(const std::string& s)
{
    const auto a = s.find_first_not_of(" \t\r");
    if (a == std::string::npos)
        return {};
    return s.substr(a, s.find_last_not_of(" \t\r") - a + 1);
}

std::vector<std::string> tokens(const std::string& s)
{
    std::vector<std::string> out;
    std::istringstream in(s);
    for (std::string w; in >> w;)
        out.push_back(w);
    return out;
}

bool name(const std::string& s)
{
    return !s.empty() && s.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_") == std::string::npos;
}

// The items an `each` may go through, each with its facts as #<item>.x# (doc 83): a job (a contract or a mission), a
// place (the way to it), a trouble (what may be done about one).
const std::set<std::string>& itemNames()
{
    static const std::set<std::string> Items{"job", "place", "trouble"};
    return Items;
}

// "#job.title#" in a tree is the bound item's fact, topic.job_title (the library's namespaces are fixed).
std::string bound(std::string text)
{
    for (const auto& item : itemNames())
    {
        const auto from = "#" + item + ".", to = "#topic." + item + "_";
        for (std::size_t at = text.find(from); at != std::string::npos; at = text.find(from, at + to.size()))
            text.replace(at, from.size(), to);
    }
    return text;
}

// Two `for`s that can never both hold: one fact each tests for values the other's can't take.
bool disjoint(const std::vector<Criterion>& a, const std::vector<Criterion>& b)
{
    for (const auto& x : a)
        for (const auto& y : b)
        {
            if (x.ns != y.ns || x.name != y.name || x.full != y.full || x.op != Criterion::Op::In || y.op != Criterion::Op::In)
                continue;
            if (std::none_of(x.values.begin(), x.values.end(), [&](const std::string& v) { return y.values.count(v) > 0; }))
                return true;
        }
    return false;
}
} // namespace

const std::set<std::string>& Library::treeActions()
{
    // Doc 83: the shop window, the free-talk field and the end (Phase 1); a giver's work: taking it, declining it,
    // handing it in, giving it back; a faction official's standing, paid report and tithe; and `open <entry>`, the
    // conversation going on in another tree at its entry (the keeper's "Any work going?" into the work tree) (Phase 2).
    // The other roles (Phase 3): the watch's report and fine, an inn's beds (lodge), learning a trade (apprentice), a
    // trouble's remedy (doc 57), a companion's orders (doc 32), and an offering to a priest (doc 55's gift of coins).
    static const std::set<std::string> Actions{"shop", "free", "end", "take", "decline", "deliver", "give_back",
                                               "standing", "buy_report", "tithe", "open", "report", "pay_fine", "lodge",
                                               "apprentice", "trouble", "order", "offering"};
    return Actions;
}

const std::set<std::string>& Library::faceMoods()
{
    // Doc 83, Phase 5: the moods the dialogue window's face knows (a tint and a frame now; painted expressions later).
    // None (calm, unmarked) is the face at rest. The game puts the NPC Mind's free-worded emotions into these too.
    static const std::set<std::string> Moods{"calm", "warm", "pleased", "keen", "formal", "stern", "wary", "cross",
                                             "sad", "worried", "tired", "sleepy", "hurt"};
    return Moods;
}

const std::set<std::string>& Library::treeLists()
{
    // here.work: contracts to be had near; here.missions: a faction official's board; here.mine: work the player has
    // taken that this resident gave out (a market's contracts, an official's missions); here.deliveries: what the player
    // carries that this resident receives (a letter, a mission's goods or message).
    // here.places: places in town worth asking the way to (the inn, the church, the shops...), nearest first;
    // here.troubles: what this wolf may do, here, about a trouble it has heard of (doc 57).
    static const std::set<std::string> Lists{"here.work", "here.missions", "here.mine", "here.deliveries", "here.places",
                                             "here.troubles"};
    return Lists;
}

const Tree* Library::tree(const std::string& id) const
{
    const auto f = trees_.find(id);
    return f == trees_.end() ? nullptr : &f->second;
}

bool Library::holdsAll(const std::vector<Criterion>& when, const Query& q, int speaker) const
{
    return std::all_of(when.begin(), when.end(), [&](const Criterion& c) { return holds(c, q, speaker); });
}

const Tree* Library::treeFor(const std::string& entry, const Query& q) const
{
    const Tree* best = nullptr;
    for (const auto& [id, t] : trees_)
    {
        if (!t.entries.count(entry) || !holdsAll(t.who, q, 0))
            continue;
        if (!best || t.priority > best->priority || (t.priority == best->priority && t.who.size() > best->who.size()))
            best = &t;
    }
    return best;
}

std::vector<ShownChoice> Library::choices(const TreeNode& node, const Query& q, std::uint64_t seed,
                                          const std::function<std::vector<Facts>(const std::string&)>& items) const
{
    std::vector<ShownChoice> out;
    const auto offer = [&](const TreeChoice& c, std::size_t index, const Query& with, int item, const Facts& facts) {
        if (out.size() >= MostShown)
            return;
        const bool ok = holdsAll(c.when, with, 0);
        if (!ok && c.locked.empty())
            return;
        ShownChoice shown;
        if (!expand(c.words, with, 1, seed + index * 131 + std::uint64_t(item + 1), shown.text) || shown.text.empty())
            return;
        shown.n = int(out.size()) + 1;
        shown.index = index;
        shown.item = item;
        shown.bound = facts;
        shown.locked = !ok;
        if (!ok && !expand(c.locked, with, 1, seed, shown.why))
            shown.why = c.locked;
        out.push_back(std::move(shown));
    };
    for (std::size_t i = 0; i < node.choices.size(); ++i)
    {
        const auto& c = node.choices[i];
        if (c.each.empty())
        {
            offer(c, i, q, -1, {});
            continue;
        }
        const auto list = items ? items(c.each) : std::vector<Facts>{};
        for (std::size_t k = 0; k < list.size() && int(k) < c.max; ++k)
        {
            Query with = q;
            for (const auto& [key, v] : list[k])
                with.facts[key] = v;
            offer(c, i, with, int(k), list[k]);
        }
    }
    return out;
}

bool Library::parseTree(const std::string& text, const std::string& file, std::string& problem)
{
    std::vector<std::string> errors;
    Tree t;
    t.file = file;
    TreeNode* node = nullptr;
    std::string gen;                                 // The rules the tree's lines (and criteria) are written as.
    std::vector<int> genLines;                       // The tree's line for each line of gen.
    const auto emit = [&](const std::string& line, int from) {
        gen += line + "\n";
        genLines.push_back(from);
    };
    std::string forTokens;
    int forLine = 0;
    struct Each
    {
        std::string list, item;
        int max = 0;
        int line = 0;
    } each;
    bool eachOpen = false;
    int number = 0;
    std::istringstream in(text);
    std::string raw;
    const auto fail = [&](const std::string& message) { errors.push_back(file + ":" + std::to_string(number) + ": " + message); };
    std::vector<std::pair<std::string, std::string>> concepts;   // (concept, node)
    std::set<std::string> says, plainSays;           // Nodes with a say, and with one that always holds.
    while (std::getline(in, raw))
    {
        ++number;
        const std::string line = trimmed(raw);
        if (line.empty() || line[0] == '#')
            continue;
        const auto first = line.substr(0, line.find_first_of(" \t["));
        const auto rest = trimmed(line.substr(first.size()));
        if (first == "tree")
        {
            if (!t.id.empty())
                fail("one tree to a file");
            else if (!name(rest))
                fail("a tree's ID is lower-case letters, digits and _");
            else if (trees_.count(rest))
                fail("tree " + rest + " is defined twice");
            t.id = rest;
            continue;
        }
        if (t.id.empty())
        {
            fail("expected 'tree <id>' first");
            continue;
        }
        if (first == "for")
        {
            forTokens = rest;
            forLine = number;
            continue;
        }
        if (first == "priority" && !node)
        {
            char* end = nullptr;
            const double n = std::strtod(rest.c_str(), &end);
            if (rest.empty() || *end || n != double(int(n)) || n < 0 || n > 100)
                fail("a tree's priority is a whole number from 0 to 100");
            else
                t.priority = int(n);
            continue;
        }
        if (first == "entry")
        {
            const auto w = tokens(rest);
            if (w.size() != 2 || !name(w[0]) || !name(w[1]))
                fail("an entry is 'entry <name> <node>'");
            else if (t.entries.count(w[0]))
                fail("entry " + w[0] + " is given twice");
            else
                t.entries[w[0]] = w[1];
            continue;
        }
        if (first == "node")
        {
            if (eachOpen)
                fail("an each needs its choice on the next line");
            eachOpen = false;
            if (!name(rest))
            {
                fail("a node's ID is lower-case letters, digits and _");
                node = nullptr;
                continue;
            }
            if (t.nodes.count(rest))
                fail("node " + rest + " is defined twice");
            node = &t.nodes[rest];
            node->id = rest;
            node->concept = "dlg." + t.id + "." + rest;
            node->line = number;
            t.order.push_back(rest);
            concepts.emplace_back(node->concept, rest);
            continue;
        }
        if (!node)
        {
            fail("expected 'node <id>' before '" + first + "'");
            continue;
        }
        if (first == "say")
        {
            // say[criteria] words | words: a rule for the node's concept; priority=N among the criteria is its priority.
            std::string criteria, words = rest;
            if (line.size() > 3 && line[3] == '[')
            {
                const auto close = line.find(']');
                if (close == std::string::npos)
                {
                    fail("a say's criteria end with ]");
                    continue;
                }
                criteria = line.substr(4, close - 4);
                words = trimmed(line.substr(close + 1));
            }
            std::string when, priority;
            for (const auto& token : tokens(criteria))
                if (token.rfind("priority=", 0) == 0)
                    priority = token.substr(9);
                else
                    when += " " + token;
            if (words.empty())
            {
                fail("a say with no words");
                continue;
            }
            says.insert(node->id);
            if (trimmed(when).empty())
                plainSays.insert(node->id);
            emit("rule " + node->concept + "." + std::to_string(number), number);
            emit("when concept=" + node->concept + when, number);
            if (!priority.empty())
                emit("priority " + priority, number);
            emit("say " + bound(words), number);
            continue;
        }
        if (first == "ask")
        {
            if (!settings_.concepts.count(rest))
                fail("'" + rest + "' is not a concept (settings.json)");
            node->ask = rest;
            continue;
        }
        if (first == "free" && rest.empty())
        {
            node->free = true;
            continue;
        }
        if (first == "end" && rest.empty())
            continue;                               // (A node with no choices ends the conversation: written to be seen.)
        if (first == "mood")
        {
            if (!name(rest))
                fail("a mood is one word");
            else if (!faceMoods().count(rest))
                fail("'" + rest + "' is not a mood the face knows (Library::faceMoods)");
            node->mood = rest;
            continue;
        }
        if (first == "each")
        {
            // each <item> in <list> max N
            const auto w = tokens(rest);
            char* end = nullptr;
            const long n = w.size() == 5 ? std::strtol(w[4].c_str(), &end, 10) : 0;
            if (w.size() != 5 || w[1] != "in" || w[3] != "max" || !name(w[0]) || !end || *end || n < 1 || n > 9)
                fail("an each is 'each <item> in <list> max <1-9>'");
            else if (!treeLists().count(w[2]))
                fail("'" + w[2] + "' is not a list a tree may go through");
            else if (!itemNames().count(w[0]))
                fail("an each's item is job, place or trouble (its facts are #job.x#...)");
            each = {w[2], w[0], int(n), number};
            eachOpen = true;
            continue;
        }
        if (first == "choice")
        {
            // choice <words> [criteria] [locked "why"] -> <node> | end | do <action> [args] [; then <node>] [; refused <node>]
            TreeChoice c;
            c.line = number;
            if (eachOpen)
            {
                c.each = each.list;
                c.item = each.item;
                c.max = each.max;
                eachOpen = false;
            }
            const auto arrow = rest.rfind("->");
            if (arrow == std::string::npos)
            {
                fail("a choice is 'choice <words> [criteria] -> <target>'");
                continue;
            }
            std::string head = trimmed(rest.substr(0, arrow));
            const std::string target = trimmed(rest.substr(arrow + 2));
            // locked "why"
            if (const auto lock = head.rfind(" locked \""); lock != std::string::npos && head.back() == '"')
            {
                c.locked = head.substr(lock + 9, head.size() - lock - 10);
                head = trimmed(head.substr(0, lock));
                if (c.locked.empty())
                    fail("a locked choice says why");
            }
            std::string criteria;
            if (!head.empty() && head.back() == ']')
                if (const auto open = head.rfind(" ["); open != std::string::npos)
                {
                    criteria = head.substr(open + 2, head.size() - open - 3);
                    head = trimmed(head.substr(0, open));
                }
            c.words = bound(head);
            c.locked = bound(c.locked);
            if (c.words.empty())
                fail("a choice with no words");
            // The target.
            if (target.rfind("do ", 0) == 0)
            {
                std::vector<std::string> parts;
                std::istringstream clauses(target.substr(3));
                for (std::string clause; std::getline(clauses, clause, ';');)
                    parts.push_back(trimmed(clause));
                const auto act = tokens(parts.empty() ? std::string() : parts[0]);
                if (act.empty() || !treeActions().count(act[0]))
                    fail("'" + (act.empty() ? std::string() : act[0]) + "' is not an action a tree may do");
                else
                {
                    c.action = act[0];
                    c.args.assign(act.begin() + 1, act.end());
                }
                for (std::size_t i = 1; i < parts.size(); ++i)
                {
                    const auto w = tokens(parts[i]);
                    if (w.size() == 2 && w[0] == "then")
                        c.then = w[1];
                    else if (w.size() == 2 && w[0] == "refused")
                        c.refused = w[1];
                    else
                        fail("after an action: '; then <node>' or '; refused <node>'");
                }
                for (const auto& a : c.args)
                    if (a.rfind("job.", 0) == 0 && a != "job.id")
                        fail("'" + a + "': an action is given the bound job as job.id");
                if ((c.action == "take" || c.action == "deliver" || c.action == "give_back") &&
                    (c.args.size() != 1 || c.args[0] != "job.id"))
                    fail("'" + c.action + "' is given the bound job: 'do " + c.action + " job.id'");
                if (c.action == "open" && (c.args.size() != 1 || !name(c.args[0])))
                    fail("'open' names an entry: 'do open <entry>'");
                if (c.action == "trouble" && (c.args.size() != 1 || c.args[0] != "trouble.id"))
                    fail("'trouble' is given the bound trouble: 'do trouble trouble.id'");
                // lodge bed night|week, lodge whole (an inn's whole upstairs for a night); order follow|wait|home|dismiss;
                // offering <pennies, 1 to 100>.
                if (c.action == "lodge" &&
                    !(c.args.size() == 2 && c.args[0] == "bed" && (c.args[1] == "night" || c.args[1] == "week")) &&
                    !(c.args.size() == 1 && c.args[0] == "whole"))
                    fail("'lodge' is 'do lodge bed night', 'do lodge bed week' or 'do lodge whole'");
                if (c.action == "order" && (c.args.size() != 1 || !(c.args[0] == "follow" || c.args[0] == "wait" ||
                                                                    c.args[0] == "home" || c.args[0] == "dismiss")))
                    fail("'order' is follow, wait, home or dismiss");
                if (c.action == "offering")
                {
                    char* end = nullptr;
                    const long n = c.args.size() == 1 ? std::strtol(c.args[0].c_str(), &end, 10) : 0;
                    if (c.args.size() != 1 || !end || *end || n < 1 || n > 100)
                        fail("'offering' gives 1 to 100 pennies: 'do offering 5'");
                }
                if ((c.action == "report" || c.action == "pay_fine" || c.action == "apprentice") && !c.args.empty())
                    fail("'" + c.action + "' takes nothing more");
            }
            else if (name(target))
                c.go = target;
            else
                fail("a choice goes to a node, 'end', or 'do <action>'");
            if (!criteria.empty())
            {
                // Its criteria, as a rule's (read by the library's own parser, then taken off it).
                emit("rule ~c." + node->concept + "." + std::to_string(node->choices.size()), number);
                emit("when concept=" + node->concept + " " + criteria, number);
                emit("say -", number);
            }
            node->choices.push_back(std::move(c));
            continue;
        }
        fail("'" + first + "' is not part of a tree (tree, for, priority, entry, node, say, ask, free, mood, each, choice, end)");
    }
    if (eachOpen)
        fail("an each needs its choice on the next line");
    if (t.id.empty())
        errors.push_back(file + ": no 'tree <id>'");
    if (t.entries.empty() && !t.id.empty())
        errors.push_back(file + ": tree " + t.id + " has no entry");
    if (!forTokens.empty())
    {
        emit("rule ~for." + t.id, forLine);
        emit("when concept=" + (concepts.empty() ? std::string("bark") : concepts.front().first) + " " + forTokens, forLine);
        emit("say -", forLine);
    }
    // Every node has its words, every target is a node, every node can be reached.
    for (const auto& [id, n] : t.nodes)
    {
        if (n.ask.empty() && !says.count(id))
            errors.push_back(file + ":" + std::to_string(n.line) + ": node " + id + " says nothing (say ...)");
        else if (n.ask.empty() && !plainSays.count(id))
            errors.push_back(file + ":" + std::to_string(n.line) + ": node " + id + " needs a say without criteria");
        for (const auto& c : n.choices)
            for (const auto* target : {&c.go, &c.then, &c.refused})
                if (!target->empty() && *target != "end" && !t.nodes.count(*target))
                    errors.push_back(file + ":" + std::to_string(c.line) + ": no node '" + *target + "'");
    }
    for (const auto& [entry, n] : t.entries)
        if (!t.nodes.count(n))
            errors.push_back(file + ": entry " + entry + " goes to no node '" + n + "'");
    {
        std::set<std::string> seen;
        std::vector<std::string> open;
        for (const auto& [entry, n] : t.entries)
            open.push_back(n);
        if (t.nodes.count("bye"))
            open.push_back("bye");                  // (Esc says goodbye: the tree's bye.)
        while (!open.empty())
        {
            const auto at = open.back();
            open.pop_back();
            const auto f = t.nodes.find(at);
            if (f == t.nodes.end() || !seen.insert(at).second)
                continue;
            for (const auto& c : f->second.choices)
                for (const auto* target : {&c.go, &c.then, &c.refused})
                    if (!target->empty())
                        open.push_back(*target);
        }
        for (const auto& id : t.order)
            if (!seen.count(id))
                errors.push_back(file + ":" + std::to_string(t.nodes[id].line) + ": node " + id + " can't be reached");
    }
    if (!errors.empty())
    {
        problem.clear();
        for (std::size_t i = 0; i < errors.size() && i < 20; ++i)
            problem += (i ? "\n" : "") + errors[i];
        return false;
    }
    // The lines and criteria, read as rules by the library's own parser: their concepts said only to players, at their
    // best tier, and said again on every visit.
    const auto keep = settings_;
    const auto before = entries_.size();
    for (const auto& [concept, n] : concepts)
    {
        settings_.concepts.insert(concept);
        settings_.bestOnly.insert(concept);
        settings_.repeat.insert(concept);
    }
    std::string trouble;
    if (!parse(gen, file, trouble))
    {
        // Its line numbers are the generated text's: put back the tree's.
        std::string mapped;
        std::istringstream lines(trouble);
        for (std::string one; std::getline(lines, one);)
        {
            const auto prefix = file + ":";
            if (one.rfind(prefix, 0) == 0)
            {
                const auto colon = one.find(':', prefix.size());
                const auto digits = colon == std::string::npos ? std::string() : one.substr(prefix.size(), colon - prefix.size());
                if (!digits.empty() && digits.find_first_not_of("0123456789") == std::string::npos)
                {
                    const auto at = std::size_t(std::atoi(digits.c_str()));
                    if (at >= 1 && at <= genLines.size())
                        one = prefix + std::to_string(genLines[at - 1]) + one.substr(colon);
                }
            }
            mapped += (mapped.empty() ? "" : "\n") + one;
        }
        settings_ = keep;
        problem = mapped;
        return false;
    }
    // The choices' and the `for`'s criteria: taken off the rules that carried them.
    for (std::size_t i = before; i < entries_.size();)
    {
        const auto& e = entries_[i];
        if (e.id.rfind("~", 0) != 0)
        {
            ++i;
            continue;
        }
        if (e.id.rfind("~for.", 0) == 0)
            t.who = e.when;
        else
        {
            const auto dot = e.id.rfind('.');
            const auto concept = e.id.substr(3, dot - 3);
            const auto index = std::size_t(std::atoi(e.id.c_str() + dot + 1));
            for (auto& [id, n] : t.nodes)
                if (n.concept == concept && index < n.choices.size())
                    n.choices[index].when = e.when;
        }
        entries_.erase(entries_.begin() + std::ptrdiff_t(i));
    }
    // The choices' words: facts known, symbols in plain, short enough.
    for (const auto& [id, n] : t.nodes)
        for (const auto& c : n.choices)
        {
            std::size_t longest = 0;
            const auto where = file + ":" + std::to_string(c.line);
            if (checkText(c.words, errors, where, &longest) && longest > 160)
                errors.push_back(where + ": a choice of more than 160 characters");
            if (!c.locked.empty())
                checkText(c.locked, errors, where, nullptr);
        }
    // No two trees for the same entry that could both hold, at the same priority.
    for (const auto& [id, o] : trees_)
        for (const auto& [entry, n] : t.entries)
            if (o.entries.count(entry) && o.priority == t.priority && !disjoint(o.who, t.who))
                errors.push_back(file + ": tree " + t.id + " and tree " + id + " both answer '" + entry +
                                 "' for the same residents: give one a priority");
    if (!errors.empty())
    {
        entries_.resize(before);
        reindex();
        settings_ = keep;
        problem.clear();
        for (std::size_t i = 0; i < errors.size() && i < 20; ++i)
            problem += (i ? "\n" : "") + errors[i];
        return false;
    }
    reindex();
    trees_[t.id] = std::move(t);
    return true;
}
} // namespace ratw::talk
