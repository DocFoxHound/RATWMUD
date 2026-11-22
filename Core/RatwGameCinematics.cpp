// Scene playback is owned by the game. Each instance's cast, stage, audience, branch and deadline
// live in the native story book. The player's physical location and the resident's schedule are independent.
#include "RatwGame.h"
#include "RatwWire.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <functional>

namespace ratw::game
{
namespace
{
using json::Value;

double sceneTime()
{
    return std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count();
}

bool identifier(const std::string& s, std::size_t limit = 80)
{
    return !s.empty() && s.size() <= limit && s.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_.:-") == s.npos;
}

bool words(const Value& v, std::size_t limit)
{
    if (!v.isString()) return false;
    const auto& s = v.asString();
    if (s.empty() || s.find_first_not_of(" \t\r\n") == s.npos) return false;
    std::size_t offset = 0, letters = 0;
    while (offset < s.size())
    {
        const auto n = json::utf8Length(s, offset);
        if (n == 0 || ++letters > limit || (static_cast<unsigned char>(s[offset]) < 32 && s[offset] != '\n')) return false;
        offset += n;
    }
    return true;
}

const Value& beatOf(const Value& state)
{
    for (const auto& beat : state["doc"].array("beats"))
        if (beat.string("id") == state.string("beat")) return beat;
    static const Value none;
    return none;
}

bool tile(const Value& p)
{
    return identifier(p.string("cell")) && story::Region::tile(p["x"]) && story::Region::tile(p["y"]);
}

bool validScene(const Value& d)
{
    const auto scope = d.string("visibility");
    if (d.string("format") != "ratw-scene" || d.number("version") != 1 || !identifier(d.string("id")) ||
        !words(d["title"], 100) || !d["lockMovement"].isBool() || (scope != "public" && scope != "individual" && scope != "party") ||
        ((scope == "public" || d.has("area")) && !story::Region::read(d["area"])) || (d.has("camera") && !tile(d["camera"])))
        return false;
    const auto& cast = d.array("cast");
    const auto& beats = d.array("beats");
    if (cast.empty() || cast.size() > 16 || beats.empty() || beats.size() > 64) return false;
    std::set<std::string> roles;
    for (const auto& part : cast)
    {
        const auto& selector = part["selector"];
        if (!identifier(part.string("id")) || !roles.insert(part.string("id")).second || part.has("npc") == part.has("selector"))
            return false;
        if (part.has("npc") ? !identifier(part.string("npc")) :
            (!selector.isObject() || (!selector.has("role") && !selector.has("cell")) ||
             (selector.has("cell") && !identifier(selector.string("cell"))) || (selector.has("role") && !words(selector["role"], 80)))) return false;
    }
    std::map<std::string, std::vector<std::string>> edges;
    for (const auto& beat : beats)
    {
        const auto id = beat.string("id"), type = beat.string("type");
        if (!identifier(id) || !roles.count(beat.string("speaker")) || edges.count(id)) return false;
        auto& follow = edges[id];
        if (beat.has("next"))
        {
            if (type=="choice") return false;
            follow.push_back(beat.string("next"));
        }
        if (type == "line" || type == "choice" || type == "response")
        {
            if (!words(beat["text"], 400) || !beat["seconds"].isNumber() || beat.number("seconds") < 1 || beat.number("seconds") > 600 ||
                std::floor(beat.number("seconds")) != beat.number("seconds")) return false;
            if (beat.has("ai") && (!words(beat["ai"]["intent"], 600) || !words(beat["ai"]["personality"], 600))) return false;
            if (type == "choice")
            {
                const auto& options = beat.array("options");
                if ((beat.string("mode") != "first" && beat.string("mode") != "majority") || options.size() < 2 || options.size() > 8) return false;
                std::set<std::string> ids;
                for (const auto& o : options)
                {
                    if (!identifier(o.string("id")) || !ids.insert(o.string("id")).second || !words(o["label"], 80)) return false;
                    if (o.has("next")) follow.push_back(o.string("next"));
                }
                if (!ids.count(beat.string("default"))) return false;
            }
            if (type == "response")
            {
                if (beat.string("mode") != "all" && beat.string("mode") != "first") return false;
                if (beat.has("timeoutNext")) follow.push_back(beat.string("timeoutNext"));
            }
        }
        else if (type == "move")
        {
            if (!tile(beat) || (beat.string("pace") != "walk" && beat.string("pace") != "run")) return false;
        }
        else if (type == "pose")
        {
            const auto p = beat.string("posture");
            if (p != "standing" && p != "sitting" && p != "lying") return false;
        }
        else return false;
    }
    std::set<std::string> done, active;
    std::function<bool(const std::string&)> check = [&](const std::string& id) {
        if (done.count(id)) return true;
        if (!edges.count(id) || !active.insert(id).second) return false;
        for (const auto& next : edges.at(id)) if (!check(next)) return false;
        active.erase(id); done.insert(id);
        return true;
    };
    return check(d.string("start")) && done.size() == edges.size();
}

// Whether any later beat reachable along this branch can use the collected answers.
bool needsResponses(const Value& doc, const Value& current)
{
    const auto hasAI = [](const Value& b) { return b.has("ai"); };
    if (std::none_of(doc.array("beats").begin(), doc.array("beats").end(), hasAI)) return false;
    std::map<std::string, const Value*> byId;
    for (const auto& b : doc.array("beats")) byId[b.string("id")] = &b;
    std::set<std::string> seen;
    std::vector<const Value*> pending{&current};
    while (!pending.empty())
    {
        const auto* beat = pending.back();
        pending.pop_back();
        if (!seen.insert(beat->string("id")).second) continue;
        if (beat != &current && hasAI(*beat)) return true;
        auto follow = [&](const std::string& next) {
            auto found = byId.find(next);
            if (found != byId.end()) pending.push_back(found->second);
        };
        follow(beat->string("next"));
        follow(beat->string("timeoutNext"));
        for (const auto& option : beat->array("options")) follow(option.string("next"));
    }
    return false;
}

// Hold a shared private instance until all of its captured participants have finished any earlier private scene.
// Public instances are synchronized with the world and keep playing for other viewers.
bool waitsForPrivateScene(const storylines::Book& book, const Value& state)
{
    if (state["doc"].string("visibility") == "public") return false;
    for (const auto& run : book.playingCinematics())
        for (const auto& [id, other] : book.find(run)->cinematics)
        {
            if (id == state.string("id") || other.boolean("failed") || other["doc"].string("visibility") == "public") continue;
            if (std::make_pair(other.number("began"), id) > std::make_pair(state.number("began"), state.string("id"))) continue;
            for (const auto& [who, member] : state["members"].fields())
                if (other["members"].has(who)) return true;
        }
    return false;
}

Value point(const std::string& cell, Vec2 p, double seconds)
{
    auto j = Value::object();
    j.add("cell", cell); j.add("x", p.x); j.add("y", p.y); j.add("at", seconds);
    return j;
}

Value pathFor(World& world, const Value& actor, const Value& beat)
{
    const auto cells = world.routeBetween(actor.string("cell"), beat.string("cell"));
    if (cells.empty()) return {};
    Vec2 here{actor.number("x"), actor.number("y")};
    const double speed = beat.string("pace") == "run" ? 5.0 : 2.0;
    double seconds = 0;
    auto path = Value::array();
    path.push(point(cells.front(), here, seconds));
    for (std::size_t i = 0; i < cells.size(); ++i)
    {
        auto& cell = cells[i];
        if (!world.ensureLoaded(cell).ok) return {};
        std::vector<Vec2> leg;
        const Door* crossing = nullptr;
        if (i + 1 == cells.size())
            leg = world.cinematicPath(cell, here, {beat.number("x") + .5, beat.number("y") + .5});
        else
        {
            std::vector<const Door*> doors;
            for (auto* d : world.doorsIn(cell))
                if (d->portal && !d->locked && d->targetCell == cells[i+1]) doors.push_back(d);
            std::stable_sort(doors.begin(), doors.end(), [&](const auto* a, const auto* b) {
                return std::hypot(a->position.x-here.x,a->position.y-here.y) < std::hypot(b->position.x-here.x,b->position.y-here.y);
            });
            for (const auto* d : doors)
            {
                leg = world.cinematicPath(cell, here, d->position);
                if (!leg.empty()) { crossing = d; break; }
            }
        }
        if (leg.empty() || path.size() + leg.size() > 8192) return {};
        for (auto pos : leg)
        {
            seconds += std::hypot(pos.x-here.x, pos.y-here.y) / speed;
            path.push(point(cell, pos, seconds));
            here = pos;
        }
        if (crossing)
        {
            here = crossing->arrival;
            path.push(point(crossing->targetCell, here, seconds));
        }
    }
    return path;
}

// A private scene's audience takes part from its first beat: the time each joined bounds how long it can be held.
void joinMembers(Value& state, double wall)
{
    if (!state.has("joined")) state.set("joined", Value::object());
    auto& joined = *state.find("joined");
    for (const auto& [who, member] : state["members"].fields())
        if (!joined.has(who)) joined.set(who, wall);
}

void project(Value& actor, const Value& state, double wall)
{
    const auto& path = state.array("path");
    if (path.empty()) return;
    const double elapsed = std::max(0., wall - state.number("since"));
    auto next = std::upper_bound(path.begin(), path.end(), elapsed, [](double t, const Value& p) { return t < p.number("at"); });
    const auto& from = next == path.begin() ? path.front() : *std::prev(next);
    const auto& to = next == path.end() ? from : *next;
    const double d = to.number("at")-from.number("at");
    const double f = d > 0 && from.string("cell")==to.string("cell") ? std::clamp((elapsed-from.number("at"))/d,0.,1.) : 0;
    actor.set("x", from.number("x") + f*(to.number("x")-from.number("x")));
    actor.set("y", from.number("y") + f*(to.number("y")-from.number("y")));
    actor.set("cell", from.string("cell"));
}
} // namespace

Result Game::startCinematic(const std::string& run, const Value& payload)
{
    auto* story = storylines_.find(run);
    const auto& doc = payload["scene"];
    const auto cue = payload.string("cue"), player = payload.string("player"), visibility = doc.string("visibility");
    if (!story || story->source != "storykeeper" || !story->live() || payload.number("visit",-1) != std::strtod(story->sourceRef.c_str(),nullptr))
        return {false,"The cinematic belongs to a different visit.",run};
    if (!identifier(cue,64) || !validScene(doc) || (player == "world" ? visibility != "public" : !story->takesPart(player)))
        return {false,"Invalid scene, cue, or initiating character.",run};
    const auto id = guid();
    Value state = Value::object();
    state.add("id",id); state.add("run",run); state.add("visit",payload["visit"]); state.add("doc",doc);
    state.add("player",player=="world"?"":player); state.add("cue",cue); state.add("began",sceneTime());
    auto members = Value::object();
    if (visibility != "public")
    {
        members.add(player,true);
        if (visibility=="party") if (const auto* party=parties_.of(player))
            for (const auto& mate : party->members) members.set(mate,true);
    }
    state.add("members",members);
    state.add("joined",Value::object());
    state.add("responses",Value::array());
    state.add("questions",Value::array());
    auto stage = Value::object();
    std::set<std::string> cast;
    for (const auto& role : doc.array("cast"))
    {
        const Entity* npc = nullptr;
        if (role.has("npc")) npc = world_.entity(role.string("npc"));
        else
        {
            const auto& sel = role["selector"];
            for (const auto& [who,e] : world_.entities())
            {
                const auto* spec=world_.society().spec(who);
                if (spec && e.npc && !e.dead && !cast.count(who) && (!sel.has("role") || sel.string("role")==spec->role) &&
                    (!sel.has("cell") || sel.string("cell")==e.cellId) && (!npc || who<npc->id)) npc=&e;
            }
        }
        if (!npc || !npc->npc || npc->dead || !world_.society().resident(npc->id)) { state.set("failed",true); break; }
        cast.insert(npc->id);
        auto a=Value::object();
        a.add("id",npc->id); a.add("role",role.string("id")); a.add("name",npc->name); a.add("appearance",wire::appearance(npc->appearance));
        a.add("cell",npc->cellId); a.add("x",npc->position.x); a.add("y",npc->position.y); a.add("posture",npc->posture);
        stage.add(role.string("id"),a);
    }
    state.add("stage",stage);
    auto& saved = story->cinematics.emplace(id,std::move(state)).first->second;
    storylines_.touch(); // Opening a public ballot uses the same per-viewer selection as the snapshot.
    if (!saved.boolean("failed"))
    {
        if (waitsForPrivateScene(storylines_, saved)) saved.set("queued",true);
        else
        {
            joinMembers(saved,sceneTime());
            openCinematicBeat(saved,doc.string("start"),sceneTime());
        }
    }
    record(StoryBook);
    return {true,"Cinematic accepted.",id};
}

bool Game::cinematicViewer(const Value& state,const std::string& who) const
{
    if (state["doc"].string("visibility")!="public") return state["members"].has(who);
    const auto* e=world_.entity(who);
    auto area=story::Region::read(state["doc"]["area"]);
    if (!e || e->npc || !area) return false;
    auto [x,y]=world_.eventTile(*e);
    return area->contains(e->cellId,x,y);
}

const Value* Game::visibleCinematic(const std::string& who) const
{
    const Value* best=nullptr;
    for (const auto& run : storylines_.playingCinematics())
        for (const auto& [id, c] : storylines_.find(run)->cinematics)
        {
            if (c.boolean("failed") || c.boolean("queued") || !cinematicViewer(c,who)) continue;
            const bool privateScene=c["doc"].string("visibility")!="public";
            if (!best || (privateScene && best->operator[]("doc").string("visibility")=="public") ||
                (privateScene==(best->operator[]("doc").string("visibility")!="public") && c.number("began")<best->number("began"))) best=&c;
        }
    return best;
}

bool Game::sceneHolds(const Value& state,const std::string& who) const
{
    // A fight outranks the shot: a viewer set upon (a story's bandits, a wild wolf) moves and acts in the fight as anyone
    // does, or it could only stand while it was beaten. The scene still plays for it, and locks again once it is over.
    // Only a participant who joined is held: a passer-by walking into a public ceremony sees it and walks on. And no
    // one is held for longer than kSceneLockSeconds, however long the scene runs.
    if (!state["doc"].boolean("lockMovement") || world_.inBattle(who)) return false;
    const auto& joined=state["joined"];
    return joined.has(who) && sceneTime()-joined.number(who) < kSceneLockSeconds;
}

bool Game::cinematicLocks(const std::string& who) const
{
    auto* state=visibleCinematic(who);
    return state && sceneHolds(*state,who);
}

void Game::noteSceneLine(const std::string& instance,SceneLine line)
{
    line.seq=++sceneLineSeq_;
    auto& said=sceneLines_[instance];
    said.push_back(std::move(line));
    while (said.size()>8) said.pop_front();
}

Value Game::cinematicView(const std::string& who) const
{
    auto* ptr=visibleCinematic(who);
    if (!ptr) return {};
    const auto& s=*ptr;
    const auto& beat=beatOf(s);
    const auto& doc=s["doc"];
    const double wall=sceneTime();
    auto out=Value::object();
    out.add("instance",s["id"]); out.add("beat",s["beat"]); out.add("title",doc["title"]); out.add("type",beat["type"]);
    auto cast=Value::array();
    for (const auto& [role,pin] : s["stage"].fields())
    {
        auto actor=pin;
        if (role==beat.string("speaker"))
        {
            if (beat.string("type")=="move") project(actor,s,wall);
            out.add("speaker",actor);
        }
        cast.push(actor);
    }
    const bool pending=s.boolean("generating"), voted=s["inputs"].has(who);
    out.add("actors",cast); out.add("text",pending ? "" : s.string("text")); out.add("lockMovement",sceneHolds(s,who));
    // Each speaker's own panel (the user, 2026-11-20): the scene's last few lines, residents' and players', newest last.
    // A player's face and name go only to a viewer who is shown that player, as with any line heard (addSpeaker).
    {
        std::vector<SceneLine> said;
        const auto found=sceneLines_.find(s.string("id"));
        if (found!=sceneLines_.end()) said.assign(found->second.begin(),found->second.end());
        const auto type=beat.string("type");
        const bool spoken=type=="line" || type=="choice" || type=="response";
        if (spoken && !pending && !s.string("text").empty() &&
            std::none_of(said.begin(),said.end(),[&](const SceneLine& l) {
                return l.role==beat.string("speaker") && l.beat==s.string("beat") && l.since==s.number("since"); }))
        {
            // After a restart the current line is known only from the saved beat: it came before any reply since.
            const auto* pin=s["stage"].find(beat.string("speaker"));
            said.insert(said.begin(),SceneLine{0,pin ? pin->string("id") : std::string(),beat.string("speaker"),s.string("beat"),
                                               s.number("since"),s.string("text")});
        }
        if (said.size()>4) said.erase(said.begin(),said.end()-4);
        auto lines=Value::array();
        for (const auto& l : said)
        {
            auto row=Value::object();
            row.add("key",l.seq ? std::to_string(l.seq) : "beat:"+l.beat);
            row.add("text",l.text);
            if (!l.role.empty())
            {
                const auto* pin=s["stage"].find(l.role);
                if (!pin) continue;
                row.add("kind","resident"); row.add("name",(*pin)["name"]); row.add("speakerId",(*pin)["id"]);
                row.add("appearance",(*pin)["appearance"]);
            }
            else
            {
                const auto* e=world_.entity(l.speaker);
                row.add("kind","player");
                addSpeaker(row,who,l.speaker,true);
                row.add("name",e && row.has("speakerId") ? e->name : std::string("Someone"));
                row.add("self",l.speaker==who);
            }
            lines.push(row);
        }
        out.add("lines",lines);
    }
    out.add("remainingMs",std::max(0.,std::ceil((s.number("end")-wall)*1000)));
    out.add("generating",pending); out.add("voted",voted); out.add("mode",beat.string("mode"));
    out.add("canRespond", !pending && !voted && wall<s.number("end") && s["electorate"].has(who));
    out.add("usesAI",s.boolean("usesAI"));
    if (doc.has("camera")) out.add("camera",doc["camera"]);
    if (beat.string("type")=="choice")
    {
        auto opts=Value::array();
        for (const auto& o:beat.array("options")) { auto row=Value::object();row.add("id",o["id"]);row.add("label",o["label"]);opts.push(row); }
        out.add("options",opts);
    }
    return out;
}

void Game::openCinematicBeat(Value& state,const std::string& id,double since)
{
    state.set("beat",id);
    state.set("since",since);
    state.set("generating",false);
    state.erase("generated");
    state.erase("path");
    state.set("inputs",Value::object());
    const auto& beat=beatOf(state);
    const auto type=beat.string("type");
    const auto duration=beat.number("seconds");
    const auto role=beat.string("speaker");
    state.erase("inputChanged");
    auto* cast=state.find("stage");
    auto* actor=cast ? cast->find(role) : nullptr;
    if (!actor) { state.set("failed",true); return; }
    if (type=="move")
    {
        auto path=pathFor(world_,*actor,beat);
        if (path.items().empty()) { state.set("failed",true); return; }
        actor->set("posture","standing");
        state.set("end",since+std::max(.25,path.items().back().number("at")));
        state.set("path",std::move(path));
    }
    else if (type=="pose")
    {
        actor->set("posture",beat["posture"]);
        state.set("end",since+1.);
    }
    else state.set("end",since+duration);
    state.set("text",beat.string("text"));
    auto electorate=Value::object();
    if (type=="choice" || type=="response")
    {
        if (state["doc"].string("visibility")!="public") electorate=state["members"];
        else for (const auto* c : clients_)
            if (!c->entityId.empty() && visibleCinematic(c->entityId) == &state) electorate.set(c->entityId,true);
    }
    state.set("electorate",std::move(electorate));
    const auto recentResponses=state["responses"];
    const auto recentQuestions=state["questions"];
    const bool keepAnswers=needsResponses(state["doc"],beat);
    state.set("usesAI",keepAnswers);
    if (!keepAnswers)
    {
        state.set("responses",Value::array());
        state.set("questions",Value::array());
    }
    const auto spoken=[&] {
        if (type=="line" || type=="choice" || type=="response")
            noteSceneLine(state.string("id"),{0,actor->string("id"),role,id,since,state.string("text")});
    };
    if (!beat.has("ai") || !mind_.live()) { spoken(); return; }
    // The live Mind writes a line only for players who can hear it: a ceremony in an empty square, or a private scene
    // whose audience has all signed out, plays its authored line and spends nothing.
    if (std::none_of(clients_.begin(), clients_.end(),
                     [&](const Connection* c) { return !c->entityId.empty() && cinematicViewer(state, c->entityId); }))
    {
        spoken();
        return;
    }
    auto context=Value::object();
    context.add("npcId",(*actor)["id"]); context.add("npc",(*actor)["name"]); context.add("personality",beat["ai"]["personality"]);
    context.add("intent",beat["ai"]["intent"]); context.add("fallback",beat["text"]); context.add("responses",recentResponses);
    if (recentQuestions.isArray() && recentQuestions.size()==recentResponses.size()) context.add("questions",recentQuestions);
    context.add("title",state["doc"]["title"]); context.add("beat",id);
    state.set("generating",true);
    state.set("end",sceneTime()+8.);
    // The game commits the attempt before submitting private dialogue to the external Mind. On a
    // restart between commit and callback the authored fallback becomes available at the deadline.
    state.set("aiPending",std::move(context));
}

// Return true when this instance has ended. Every outcome is recorded together with the checkpoint that removes
// the instance or advances its beat. A saved ballot cannot be re-applied after a restart.
bool Game::advanceCinematic(Value& s,double wall)
{
    const auto fact=[&](const char* kind,const std::string& detail) {
        world_.recordEvent({kind,s.string("player"),s.string("run"),{},0,0,{},int(s.number("visit")),0,detail});
    };
    if (s.boolean("failed")) { fact("scene failed",s.string("cue")); return true; }
    const auto& beat=beatOf(s);
    const auto type=beat.string("type"),mode=beat.string("mode");
    const auto end=s.number("end");
    if (s.boolean("generating"))
    {
        if (!s.has("generated") && wall<end) return false;
        if (!s.string("generated").empty()) s.set("text",s["generated"]);
        s.erase("generated");
        s.set("generating",false); s.set("since",wall); s.set("end",wall+beat.number("seconds"));
        if (const auto* pin=s["stage"].find(beat.string("speaker")))
            noteSceneLine(s.string("id"),{0,pin->string("id"),beat.string("speaker"),s.string("beat"),wall,s.string("text")});
        return false;
    }
    std::string next=beat.string("next");
    const auto& ballots=s["inputs"];
    const auto required=s["electorate"].size();
    const bool timedOut = wall>=end;
    if (type=="choice")
    {
        std::map<std::string,std::size_t> counts;
        for (const auto& [who,ballot] : ballots.fields()) ++counts[ballot.asString()];
        auto chosen=beat.string("default");
        std::size_t most=0;
        bool tied=false;
        for (const auto& [option,n]:counts)
        {
            if (n>most) { most=n; chosen=option; tied=false; }
            else if (n==most) tied=true;
        }
        if (tied) chosen=beat.string("default");
        if (!timedOut && (!most || (mode=="majority" && most<=required/2))) return false;
        for (const auto& option:beat.array("options")) if (option.string("id")==chosen) next=option.string("next");
        fact("scene choice",s.string("cue")+"."+chosen);
    }
    else if (type=="response")
    {
        const bool ready=ballots.size() && (mode=="first" || ballots.size()==required);
        if (!ready && !timedOut) return false;
        fact("scene response",s.string("cue")+"."+beat.string("id")+(ready?".submitted":".timeout"));
        if (!ready && beat.has("timeoutNext")) next=beat.string("timeoutNext");
    }
    else if (!timedOut) return false;
    if (type=="move")
    {
        auto* actor=s.find("stage")->find(beat.string("speaker"));
        project(*actor,s,end);
    }
    if (next.empty()) { fact("scene finished",s.string("cue")); return true; }
    // Use the previous deadline for catch-up. A live input that resolves a beat early starts the next one immediately.
    openCinematicBeat(s,next,timedOut?end:wall);
    return false;
}

void Game::tickCinematics()
{
    if (storylines_.playingCinematics().empty())
    {
        sceneLines_.clear();
        return;
    }
    const double wall=sceneTime();
    struct Request { std::string run,instance,beat; Value context; };
    std::vector<Request> requests;
    // The game thread owns the event buffer during this pass. Include trimmed entries so a full buffer
    // cannot hide a new choice, response or terminal event when we decide how to persist the advance.
    const auto worldEvents = [&] { return world_.droppedEvents() + world_.recentEvents().size(); };
    const auto eventsBefore = worldEvents();
    bool changed=false;
    for (const auto& run:storylines_.playingCinematics())
    {
        auto& instances=storylines_.find(run)->cinematics;
        for (auto it=instances.begin();it!=instances.end();)
        {
            auto& s=it->second;
            if (s.boolean("queued"))
            {
                if (waitsForPrivateScene(storylines_,s)) { ++it; continue; }
                s.erase("queued");
                joinMembers(s,wall);
                openCinematicBeat(s,s["doc"].string("start"),wall);
                changed=true;
            }
            if (s.has("aiPending"))
            {
                if (mind_.live() && wall<s.number("end")) requests.push_back({run,it->first,s.string("beat"),s["aiPending"]});
                else s.set("generated","");
                s.erase("aiPending");
                changed=true;
            }
            if (!s.boolean("failed") && !s.boolean("inputChanged") && !s.has("generated") && wall < s.number("end"))
            {
                ++it;
                continue;
            }
            s.erase("inputChanged");
            // A recovered clock can cover a whole scene. The contract is acyclic with at most 64 beats.
            bool ended=false;
            for (unsigned i=0;i<=64;++i)
            {
                const auto id=s.string("beat");
                const bool generating=s.boolean("generating");
                ended=advanceCinematic(s,wall);
                if (!ended && id==s.string("beat") && generating==s.boolean("generating")) break;
                changed=true;
                if (ended) break;
            }
            if (ended) { sceneLines_.erase(it->first); it=instances.erase(it); } else ++it;
        }
    }
    if (changed)
    {
        storylines_.touch();
        ++revision_;
        // Commit the run and its world facts together, and persist a consumed AI request before dispatch: in an early
        // save on the store's worker (doc 85, Phase 7: never a whole save waited for on the game thread, which stopped a
        // big world for as long as it took), whose checkpoint carries the outcome's events with the state they belong
        // with; a crash before it loses both, and the beat is played again. Intermediate beats only change the story
        // book. Their stage, deadline and text replay from the journal.
        if (worldEvents() != eventsBefore || !requests.empty()) saveSoon(StoryOutcomeSeconds);
        else record(StoryBook);
    }
    // (A request is sent once the save that consumed it is stored: dispatchSceneRequests, each tick.)
    for (auto& request : requests)
        if (storageReady_)
            sceneRequestsAfterSave_.push_back({revision_, request.run, request.instance, request.beat, std::move(request.context)});
    dispatchSceneRequests();
}

void Game::dispatchSceneRequests()
{
    if (sceneRequestsAfterSave_.empty())
        return;
    const std::uint64_t stored = store_ ? store_->storedRevision() : UINT64_MAX;
    for (auto it = sceneRequestsAfterSave_.begin(); it != sceneRequestsAfterSave_.end();)
    {
        if (storageReady_ && stored < it->revision)
        {
            ++it;
            continue;
        }
        if (storageReady_)
        {
            const auto run=it->run, instance=it->instance, id=it->beat;
            mind_.sceneLine(it->context,[this,run,instance,id](const std::string& line) {
                auto* story=storylines_.find(run);
                if (!story || !story->live()) return;
                const auto found=story->cinematics.find(instance);
                if (found==story->cinematics.end()) return;
                auto& active=found->second;
                if (active.string("beat")==id && active.boolean("generating") && sceneTime()<active.number("end"))
                    active.set("generated",words(Value(line),600) ? line : "");
            });
        }
        it = sceneRequestsAfterSave_.erase(it);
    }
}

Result Game::submitCinematicInput(const std::string& who,const std::string& instance,const std::string& id,
                                 const std::string& input,bool reply)
{
    tickCinematics();
    const auto* visible=visibleCinematic(who);
    if (!visible || visible->string("id")!=instance || visible->string("beat")!=id || !visible->operator[]("electorate").has(who) ||
        visible->boolean("generating") || visible->operator[]("inputs").has(who) || sceneTime()>=visible->number("end"))
        return {false,"This prompt is closed or you have already answered.",{}};
    auto& s=storylines_.find(visible->string("run"))->cinematics.at(instance);
    const auto& beat=beatOf(s);
    if (reply)
    {
        if (beat.string("type")!="response" || !words(Value(input),280))
            return {false,"A reply needs 1–280 characters.",{}};
        if (s.boolean("usesAI") && mind_.live())
        {
            // A ballot only needs the submitter. Keep a bounded, unattributed tail of prose solely
            // for this instance's authored AI lines, and discard it with the instance.
            auto answers=s["responses"];
            auto questions=s["questions"];
            // Older checkpoints can contain answers without the corresponding prompts. Omit
            // questions from the AI request until those entries have aged out of the window.
            if (answers.size()>=8)
            {
                if (questions.size()==answers.size()) questions.items().erase(questions.items().begin());
                answers.items().erase(answers.items().begin());
            }
            answers.push(input); questions.push(s["text"]);
            s.set("responses",std::move(answers)); s.set("questions",std::move(questions));
        }
        s.find("inputs")->set(who,true);
        // The reply is said aloud in the scene: in the player's own panel, for its audience (never in the event log).
        noteSceneLine(instance,{0,who,"","",sceneTime(),input});
    }
    else
    {
        if (beat.string("type")!="choice" || std::none_of(beat.array("options").begin(),beat.array("options").end(),
                [&](const auto& option){return option.string("id")==input;}))
            return {false,"Choose an offered option.",{}};
        s.find("inputs")->set(who,input);
    }
    // Answering is taking part: a public scene with a movement lock holds this viewer from now (sceneHolds).
    if (!s.has("joined")) s.set("joined",Value::object());
    if (!s["joined"].has(who)) s.find("joined")->set(who,sceneTime());
    // Persist immediately when this input resolves the beat and creates an outcome. Otherwise journal the new ballot.
    const auto run=s.string("run");
    s.set("inputChanged",true);
    tickCinematics();
    auto& running=storylines_.find(run)->cinematics;
    const auto it=running.find(instance);
    if (it!=running.end() && it->second.string("beat")==id) { storylines_.touch(); record(StoryBook); }
    return {true,reply?"Reply received.":"Vote received.",{}};
}

Result Game::cinematicVote(const std::string& who,const std::string& instance,const std::string& beat,const std::string& option)
{
    return submitCinematicInput(who,instance,beat,option,false);
}

Result Game::cinematicReply(const std::string& who,const std::string& instance,const std::string& beat,const std::string& text)
{
    return submitCinematicInput(who,instance,beat,text,true);
}
} // namespace ratw::game
