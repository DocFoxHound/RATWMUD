// Storylines (Docs/Design/58-player-storytellers.md, 1): see RatwStorylines.h.
#include "RatwStorylines.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

namespace ratw::storylines
{
using json::Value;

namespace
{
std::filesystem::path dataFile(const std::string& name)
{
    // Data/Storylines: RATW_DATA_DIR, else the working directory or one above it, else the source tree.
    namespace fs = std::filesystem;
    std::error_code ec;
    if (const char* dir = std::getenv("RATW_DATA_DIR"); dir && *dir)
        return fs::path(dir) / "Storylines" / name;
    for (auto at = fs::current_path(ec); !ec && !at.empty(); at = at.parent_path())
    {
        if (fs::exists(at / "Data" / "Storylines" / name, ec))
            return at / "Data" / "Storylines" / name;
        if (at == at.parent_path())
            break;
    }
#ifdef RATW_SOURCE_DIR
    return fs::path(RATW_SOURCE_DIR) / "Data" / "Storylines" / name;
#else
    return fs::path("Data") / "Storylines" / name;
#endif
}

std::string readFile(const std::string& name)
{
    std::ifstream in(dataFile(name));
    std::stringstream text;
    text << in.rdbuf();
    if (!in)
        std::cerr << "[warn] RATW_STORYLINES no Data/Storylines/" << name << "\n";
    return text.str();
}

std::string filled(std::string text, const std::map<std::string, std::string>& cast)
{
    for (const auto& [role, id] : cast)
    {
        const auto blank = "{" + role + "}";
        for (auto at = text.find(blank); at != std::string::npos; at = text.find(blank, at + id.size()))
            text.replace(at, blank.size(), id);
    }
    return text;
}

double apart(double x1, double y1, double x2, double y2) { return std::hypot(x1 - x2, y1 - y2); }
} // namespace

bool Step::done() const
{
    return !objectives.empty() && std::all_of(objectives.begin(), objectives.end(), [](const Objective& o) { return o.done(); });
}

std::size_t Storyline::current() const
{
    for (std::size_t i = 0; i < steps.size(); ++i)
        if (!steps[i].done())
            return i;
    return steps.size();
}

bool Storyline::takesPart(const std::string& who) const
{
    const auto p = participants.find(who);
    return p != participants.end() && p->second.active();
}

Rules parseRules(const std::string& text)
{
    Rules r;
    Value doc;
    std::string error;
    if (text.empty() || !json::parse(text, doc, error) || !doc.isObject())
        return r;
    const auto num = [&](const char* key, int& v) { v = int(doc.number(key, v)); };
    const auto dbl = [&](const char* key, double& v) { v = doc.number(key, v); };
    const auto pair = [&](const char* key, int (&v)[2]) {
        if (const auto& a = doc.array(key); a.size() == 2)
            v[0] = int(a[0].asNumber(v[0])), v[1] = int(a[1].asNumber(v[1]));
    };
    num("personalAtOnce", r.personalAtOnce);
    num("talesAtOnce", r.talesAtOnce);
    num("runningTales", r.runningTales);
    num("participants", r.participants);
    num("cast", r.cast);
    pair("personalSteps", r.personalSteps);
    pair("taleSteps", r.taleSteps);
    dbl("idleDays", r.idleDays);
    dbl("placeEvery", r.placeEvery);
    dbl("narrateEvery", r.narrateEvery);
    num("narrateLetters", r.narrateLetters);
    num("castName", r.castName);
    num("visitorsAtOnce", r.visitorsAtOnce);
    pair("visitorMinutes", r.visitorMinutes);
    num("visitorReach", r.visitorReach);
    num("diceMost", r.diceMost);
    num("diceBonus", r.diceBonus);
    num("applyLevel", r.applyLevel);
    num("reportDays", r.reportDays);
    num("keepTextDays", r.keepTextDays);
    num("creditsDays", r.creditsDays);
    num("creditsStars", r.creditsStars);
    return r;
}

const Rules& rules()
{
    static const Rules r = parseRules(readFile("rules.json"));
    return r;
}

std::vector<Template> parseTemplates(const std::string& text)
{
    std::vector<Template> out;
    Value doc;
    std::string error;
    if (text.empty() || !json::parse(text, doc, error) || !doc.isObject())
        return out;
    for (const auto& t : doc.array("templates"))
        if (!t.string("id").empty() && t.array("steps").size() >= 1)
            out.push_back({t.string("id"), t.string("title"), t.string("source", "dm"), t["steps"]});
    return out;
}

const std::vector<Template>& templates()
{
    static const std::vector<Template> t = parseTemplates(readFile("templates.json"));
    return t;
}

const Template* findTemplate(const std::string& id)
{
    for (const auto& t : templates())
        if (t.id == id)
            return &t;
    return nullptr;
}

Storyline* Book::find(const std::string& id)
{
    const auto found = all_.find(id);
    return found == all_.end() ? nullptr : &found->second;
}

const Storyline* Book::find(const std::string& id) const
{
    const auto found = all_.find(id);
    return found == all_.end() ? nullptr : &found->second;
}

int Book::counting(const std::string& who, const std::string& kind) const
{
    int n = 0;
    for (const auto& [id, s] : all_)
        n += s.kind == kind && (s.state == "running" || s.state == "paused" || s.state == "draft") && s.takesPart(who);
    return n;
}

Storyline* Book::begin(const Template& t, const std::string& owner, const std::vector<std::string>& also,
                       const std::map<std::string, std::string>& cast, const std::string& source, const std::string& sourceRef,
                       double now, const MarkerFor& markerFor, std::string& why)
{
    const auto& r = rules();
    if (counting(owner, "personal") >= r.personalAtOnce)
    {
        why = "Your journal holds as many stories as you can follow at once.";
        return nullptr;
    }
    Storyline s;
    s.kind = "personal";
    s.source = source;
    s.sourceRef = sourceRef;
    s.templateId = t.id;
    s.owner = owner;
    s.title = filled(t.title, cast).substr(0, std::size_t(r.title));
    for (const auto& st : t.steps.items())
    {
        Step step;
        step.title = filled(st.string("title"), cast).substr(0, std::size_t(r.stepTitle));
        step.text = filled(st.string("text"), cast).substr(0, std::size_t(r.stepText));
        step.brief = filled(st.string("brief"), cast).substr(0, 300);
        step.distinct = st.boolean("distinct");
        if (const auto role = st.string("marker"); !role.empty() && markerFor)
        {
            const auto id = cast.count(role) ? cast.at(role) : role;
            step.marker = markerFor(role, id);
        }
        for (const auto& o : st.array("objectives"))
        {
            Objective obj;
            obj.kind = o.string("kind");
            obj.target = filled(o.string("target"), cast);
            obj.cell = filled(o.string("cell"), cast);
            obj.to = filled(o.string("to"), cast);
            obj.x = o.number("x", -1), obj.y = o.number("y", -1), obj.radius = o.number("radius", 0);
            obj.count = std::max(1, int(o.number("count", 2)));
            obj.line = filled(o.string("line"), cast).substr(0, 160);
            obj.suits = o.string("suits");
            if (obj.kind.empty() || obj.target.find('{') != std::string::npos || obj.to.find('{') != std::string::npos)
            {
                why = "That story's cast isn't complete.";
                return nullptr;
            }
            step.objectives.push_back(std::move(obj));
        }
        if (step.objectives.empty())
            step.objectives.push_back(Objective{"told"});
        s.steps.push_back(std::move(step));
    }
    if (int(s.steps.size()) < r.personalSteps[0] || int(s.steps.size()) > r.personalSteps[1])
    {
        why = "A personal story has 3 to 7 steps.";
        return nullptr;
    }
    // Endowed progress (doc 48, 8.2): the first step is already done ("Arrived in Upper Accord").
    for (auto& o : s.steps.front().objectives)
        o.doneBy = owner, o.doneAt = now;
    s.participants[owner] = {now, -1};
    for (const auto& other : also)
        if (!other.empty() && other != owner)
            s.participants[other] = {now, -1};
    s.began = s.lastActivity = now;
    s.tracking.insert(owner);
    auto& kept = add(std::move(s));
    return &kept;
}

Storyline& Book::add(Storyline s)
{
    if (s.id.empty())
        s.id = (s.kind == "tale" ? "tale-" : s.kind == "world" ? "world-" : "story-") + std::to_string(next_++);
    auto& kept = all_[s.id];
    kept = std::move(s);
    touch();
    return kept;
}

void Book::erase(const std::string& id)
{
    all_.erase(id);
    touch();
}

void Book::reindex()
{
    waiting_.clear();
    for (const auto& [id, s] : all_)
    {
        if (!s.live())
            continue;
        const auto at = s.current();
        if (at >= s.steps.size())
            continue;
        for (const auto& o : s.steps[at].objectives)
            if (!o.done())
                waiting_[o.kind].insert(id);
    }
}

bool Book::waiting(const std::string& kind) const
{
    const auto found = waiting_.find(kind);
    return found != waiting_.end() && !found->second.empty();
}

std::set<std::string> Book::waitingFor(const std::string& kind) const
{
    std::set<std::string> out;
    if (const auto found = waiting_.find(kind); found != waiting_.end())
        for (const auto& id : found->second)
            if (const auto* s = find(id))
                for (const auto& [who, p] : s->participants)
                    if (p.active())
                        out.insert(who);
    return out;
}

std::vector<Progress> Book::settle(Storyline& s, double now)
{
    // After an objective: the step done, the next begun; the last done, the storyline done.
    std::vector<Progress> out;
    const auto at = s.current();
    s.lastActivity = now;
    if (at >= s.steps.size() && s.state == "running")
    {
        s.state = "done";
        s.ended = now;
        out.push_back({s.id, {}, "done", s.steps.empty() ? 0 : s.steps.size() - 1, 0, false});
    }
    return out;
}

std::vector<Progress> Book::happen(const Event& e, double now)
{
    std::vector<Progress> out;
    const auto ids = waiting_.find(e.kind);
    if (ids == waiting_.end())
        return out;
    const std::vector<std::string> candidates(ids->second.begin(), ids->second.end());
    for (const auto& id : candidates)
    {
        auto* s = find(id);
        if (!s || !s->live())
            continue;
        const auto at = s->current();
        if (at >= s->steps.size())
            continue;
        auto& step = s->steps[at];
        const bool wasDone = step.done();
        for (std::size_t i = 0; i < step.objectives.size(); ++i)
        {
            auto& o = step.objectives[i];
            if (o.done() || o.kind != e.kind)
                continue;
            // Who did it: the actor, taking part; for a scene, enough of the storyline's wolves in it.
            std::string actor = e.actor;
            if (!s->takesPart(actor))
                continue;
            if (step.distinct && std::any_of(step.objectives.begin(), step.objectives.end(),
                                             [&](const Objective& other) { return &other != &o && other.doneBy == actor; }))
                continue;                           // (Two angles: a wolf can't do two parts of one step.)
            if (!o.cell.empty() && o.cell != e.cell)
                continue;
            bool fits = false;
            if (e.kind == "place")
                fits = e.cell == o.target && (o.radius <= 0 || apart(e.x, e.y, o.x, o.y) <= o.radius);
            else if (e.kind == "talk" || e.kind == "hunt" || e.kind == "gift")
                fits = o.target.empty() || o.target == e.target;
            else if (e.kind == "contract")
                fits = (o.target.empty() || o.target == e.target) && (o.to.empty() || o.to == e.to);
            else if (e.kind == "deliver")
                fits = (o.target.empty() || o.target == e.target) && (o.to.empty() || o.to == e.to);
            else if (e.kind == "fight")
                fits = true;
            else if (e.kind == "scene")
                fits = int(std::count_if(e.together.begin(), e.together.end(), [&](const std::string& w) { return s->takesPart(w); })) >= o.count;
            if (!fits)
                continue;
            o.doneBy = actor;
            o.doneAt = now;
            out.push_back({s->id, actor, "objective", at, i, false});
            if (step.distinct)
                break;                              // (One part of a step a wolf, at a time.)
        }
        if (!wasDone && step.done())
            out.push_back({s->id, e.actor, "step", at, 0, false});
        const auto ended = settle(*s, now);
        out.insert(out.end(), ended.begin(), ended.end());
    }
    if (!out.empty())
        touch();
    return out;
}

std::vector<Progress> Book::tick(const std::string& id, std::size_t step, std::size_t objective, const std::string& by, double now,
                                 bool byHand)
{
    std::vector<Progress> out;
    auto* s = find(id);
    if (!s || !s->live() || step != s->current() || step >= s->steps.size() || objective >= s->steps[step].objectives.size())
        return out;
    auto& o = s->steps[step].objectives[objective];
    if (o.done())
        return out;
    o.doneBy = by;
    o.doneAt = now;
    o.byHand = byHand || o.kind != "told";
    out.push_back({s->id, by, "objective", step, objective, o.byHand});
    if (s->steps[step].done())
        out.push_back({s->id, by, "step", step, 0, o.byHand});
    const auto ended = settle(*s, now);
    out.insert(out.end(), ended.begin(), ended.end());
    touch();
    return out;
}

bool Book::take(const std::string& id, std::size_t step, std::size_t objective, const std::string& who)
{
    auto* s = find(id);
    if (!s || !s->live() || !s->takesPart(who) || step != s->current() || step >= s->steps.size() ||
        objective >= s->steps[step].objectives.size() || s->steps[step].objectives[objective].done())
        return false;
    for (auto& o : s->steps[step].objectives)
        if (o.takenBy == who)
            o.takenBy.clear();                      // (One part at a time.)
    s->steps[step].objectives[objective].takenBy = who;
    touch();
    return true;
}

void Book::end(const std::string& id, const std::string& state, double now)
{
    if (auto* s = find(id); s && (s->state == "running" || s->state == "paused" || s->state == "draft"))
    {
        s->state = state;
        s->ended = now;
        touch();
    }
}

std::vector<const Storyline*> Book::of(const std::string& who) const
{
    std::vector<const Storyline*> out;
    for (const auto& [id, s] : all_)
        if (s.participants.count(who) || s.invited.count(who) || s.asked.count(who))
            out.push_back(&s);
    return out;
}

Value saveStoryline(const Storyline& s)
{
    auto o = Value::object();
    o.add("id", s.id); o.add("kind", s.kind); o.add("source", s.source); o.add("sourceRef", s.sourceRef);
    o.add("template", s.templateId); o.add("owner", s.owner); o.add("authorAccount", s.authorAccount);
    o.add("author", s.authorCharacter); o.add("title", s.title); o.add("premise", s.premise); o.add("chapter", s.chapter);
    o.add("state", s.state); o.add("began", s.began); o.add("ended", s.ended); o.add("lastActivity", s.lastActivity);
    auto steps = Value::array();
    for (const auto& st : s.steps)
    {
        auto j = Value::object();
        j.add("title", st.title); j.add("text", st.text); j.add("brief", st.brief); j.add("distinct", st.distinct);
        if (st.marker.set())
        {
            auto m = Value::object();
            m.add("cell", st.marker.cell); m.add("label", st.marker.label); m.add("x", st.marker.x); m.add("y", st.marker.y);
            m.add("radius", st.marker.radius);
            j.add("marker", m);
        }
        auto objectives = Value::array();
        for (const auto& ob : st.objectives)
        {
            auto k = Value::object();
            k.add("kind", ob.kind); k.add("target", ob.target); k.add("cell", ob.cell); k.add("to", ob.to); k.add("x", ob.x);
            k.add("y", ob.y); k.add("radius", ob.radius); k.add("count", ob.count); k.add("line", ob.line); k.add("suits", ob.suits);
            k.add("doneBy", ob.doneBy); k.add("takenBy", ob.takenBy); k.add("doneAt", ob.doneAt); k.add("byHand", ob.byHand);
            objectives.push(k);
        }
        j.add("objectives", objectives);
        steps.push(j);
    }
    o.add("steps", steps);
    auto who = Value::object();
    for (const auto& [id, p] : s.participants)
    {
        auto j = Value::object();
        j.add("joined", p.joined);
        j.add("left", p.left);
        who.add(id, j);
    }
    o.add("participants", who);
    auto cast = Value::array();
    for (const auto& c : s.cast)
    {
        auto j = Value::object();
        j.add("name", c.name);
        j.add("looks", c.looks);
        cast.push(j);
    }
    o.add("cast", cast);
    const auto list = [](const std::set<std::string>& set) {
        auto a = Value::array();
        for (const auto& x : set)
            a.push(x);
        return a;
    };
    o.add("tracking", list(s.tracking));
    o.add("invited", list(s.invited));
    o.add("asked", list(s.asked));
    o.add("admitted", list(s.admitted));
    if (s.calledOn)
    {
        o.add("calledTown", s.calledTown);
        o.add("callText", s.callText);
    }
    return o;
}

Storyline loadStoryline(const Value& o)
{
    Storyline s;
    s.id = o.string("id"); s.kind = o.string("kind", "personal"); s.source = o.string("source"); s.sourceRef = o.string("sourceRef");
    s.templateId = o.string("template"); s.owner = o.string("owner"); s.authorAccount = o.string("authorAccount");
    s.authorCharacter = o.string("author"); s.title = o.string("title"); s.premise = o.string("premise"); s.chapter = o.string("chapter");
    s.state = o.string("state", "running"); s.began = o.number("began"); s.ended = o.number("ended", -1);
    s.lastActivity = o.number("lastActivity");
    for (const auto& j : o.array("steps"))
    {
        Step st;
        st.title = j.string("title"); st.text = j.string("text"); st.brief = j.string("brief"); st.distinct = j.boolean("distinct");
        if (const auto& m = j.object("marker"); m.isObject())
            st.marker = {m.string("cell"), m.string("label"), m.number("x", -1), m.number("y", -1), m.number("radius")};
        for (const auto& k : j.array("objectives"))
        {
            Objective ob;
            ob.kind = k.string("kind"); ob.target = k.string("target"); ob.cell = k.string("cell"); ob.to = k.string("to");
            ob.x = k.number("x", -1); ob.y = k.number("y", -1); ob.radius = k.number("radius"); ob.count = int(k.number("count", 2));
            ob.line = k.string("line"); ob.suits = k.string("suits"); ob.doneBy = k.string("doneBy"); ob.takenBy = k.string("takenBy");
            ob.doneAt = k.number("doneAt", -1); ob.byHand = k.boolean("byHand");
            st.objectives.push_back(std::move(ob));
        }
        s.steps.push_back(std::move(st));
    }
    for (const auto& [id, p] : o.object("participants").fields())
        s.participants[id] = {p.number("joined"), p.number("left", -1)};
    for (const auto& c : o.array("cast"))
        s.cast.push_back({c.string("name"), c.string("looks")});
    const auto set = [&](const char* key, std::set<std::string>& into) {
        for (const auto& x : o.array(key))
            if (x.isString())
                into.insert(x.asString());
    };
    set("tracking", s.tracking);
    set("invited", s.invited);
    set("asked", s.asked);
    set("admitted", s.admitted);
    s.calledTown = o.string("calledTown");
    s.callText = o.string("callText");
    s.calledOn = !s.calledTown.empty();
    return s;
}

Value Book::save() const
{
    auto list = Value::array();
    for (const auto& [id, s] : all_)
        list.push(saveStoryline(s));
    return list;
}

void Book::load(const Value& saved)
{
    all_.clear();
    next_ = 1;
    for (const auto& o : saved.items())
    {
        auto s = loadStoryline(o);
        if (s.id.empty() || s.id.size() > 60)
            continue;
        const auto dash = s.id.rfind('-');
        if (dash != std::string::npos)
            next_ = std::max<std::uint64_t>(next_, std::strtoull(s.id.c_str() + dash + 1, nullptr, 10) + 1);
        all_[s.id] = std::move(s);
    }
    touch();
}
} // namespace ratw::storylines
