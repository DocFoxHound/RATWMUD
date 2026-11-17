// The talk ledger (RatwTalkLedger.h; Docs/Design/84 §8.7, Phase 5).
#include "RatwTalkLedger.h"

#include <algorithm>
#include <cmath>
#include <tuple>

namespace ratw
{
namespace
{
template <typename T> void cap(std::deque<T>& d, std::size_t most)
{
    while (d.size() > most)
        d.pop_front();
}
json::Value strings(const std::vector<std::string>& v)
{
    auto a = json::Value::array();
    for (const auto& s : v)
        a.push(s);
    return a;
}
std::vector<std::string> strings(const json::Value& v)
{
    std::vector<std::string> out;
    for (const auto& s : v.items())
        out.push_back(s.asString(""));
    return out;
}
} // namespace

std::string TalkLedger::pairKey(const std::string& a, const std::string& b) { return a < b ? a + "|" + b : b + "|" + a; }

void TalkLedger::trimNotes(std::deque<TalkNote>& list) const
{
    // The oldest heard past 8, the oldest out of hearing past 2, each kept in its own room (the newest is never dropped).
    std::size_t heard = 0, unheard = 0;
    for (const auto& n : list)
        ++(n.unheard ? unheard : heard);
    for (auto it = list.begin(); it != list.end() && (heard > NotesPerPair || unheard > UnheardPerPair) && std::next(it) != list.end();)
        if (it->unheard ? unheard > UnheardPerPair : heard > NotesPerPair)
        {
            --(it->unheard ? unheard : heard);
            it = list.erase(it);
        }
        else
            ++it;
}

TalkNote& TalkLedger::note(const std::string& a, const std::string& b, double day, bool unheard)
{
    now_ = std::max(now_, day);
    const auto key = pairKey(a, b);
    if (unheard)
        touchQuiet("n:" + key);
    else
        touch("n:" + key);
    auto& list = notes_[key];
    list.push_back({});
    list.back().day = day;
    list.back().opener = a;
    list.back().unheard = unheard;
    trimNotes(list);
    return list.back();
}

const std::deque<TalkNote>* TalkLedger::notes(const std::string& a, const std::string& b) const
{
    const auto key = pairKey(a, b);
    const auto found = notes_.find(key);
    if (found == notes_.end())
        return nullptr;
    touch("n:" + key);
    return &found->second;
}

TalkNote* TalkLedger::latest(const std::string& a, const std::string& b, bool heardOnly)
{
    const auto found = notes_.find(pairKey(a, b));
    if (found == notes_.end())
        return nullptr;
    for (auto it = found->second.rbegin(); it != found->second.rend(); ++it)
        if (!heardOnly || !it->unheard)
            return &*it;
    return nullptr;
}

const TalkNote* TalkLedger::latest(const std::string& a, const std::string& b, bool heardOnly) const
{
    const auto* list = notes(a, b);
    if (!list)
        return nullptr;
    for (auto it = list->rbegin(); it != list->rend(); ++it)
        if (!heardOnly || !it->unheard)
            return &*it;
    return nullptr;
}

const TalkNote* TalkLedger::recall(const std::string& a, const std::string& b, const std::string& topic) const
{
    const auto* list = notes(a, b);
    if (!list)
        return nullptr;
    for (auto it = list->rbegin(); it != list->rend(); ++it)
        if (std::find(it->topics.begin(), it->topics.end(), topic) != it->topics.end())
            return &*it;
    return nullptr;
}

bool TalkLedger::talkedOf(const std::string& a, const std::string& b, const std::string& topic, const std::string& key, double since) const
{
    const auto* list = notes(a, b);
    if (!list)
        return false;
    for (auto it = list->rbegin(); it != list->rend() && it->day >= since; ++it)
        for (std::size_t i = 0; i < it->topics.size(); ++i)
            if (it->topics[i] == topic && (key.empty() || (i < it->keys.size() && it->keys[i] == key)))
                return true;
    return false;
}

std::string TalkLedger::thread(const std::string& a, const std::string& b, double day) const
{
    const auto* n = latest(a, b, true);
    return n && !n->thread.empty() && day <= n->threadUntil ? n->thread : std::string();
}

void TalkLedger::closeThread(const std::string& a, const std::string& b)
{
    if (auto* n = latest(a, b, true))
        n->thread.clear();
}

void TalkLedger::told(const std::string& listener, const std::string& teller, const std::string& key, const std::string& kind,
                      double day, bool counted, bool heard)
{
    if (!enabled || listener.empty() || key.empty())
        return;
    now_ = std::max(now_, day);
    if (heard)
        touch("t:" + listener);
    else
        touchQuiet("t:" + listener);
    auto& list = told_[listener];
    for (auto& t : list)
        if (t.teller == teller && t.key == key)
        {
            t.day = day;                            // (Told again: one record, the latest day.)
            t.counted = t.counted || counted;
            if (!kind.empty())
                t.kind = kind;
            return;
        }
    list.push_back({teller, key, kind, day, counted});
    cap(list, ToldPerResident);
}

bool TalkLedger::counted(const std::string& listener, const std::string& teller, const std::string& key) const
{
    const auto found = told_.find(listener);
    if (found != told_.end())
        touch("t:" + listener);
    return found != told_.end() && std::any_of(found->second.begin(), found->second.end(), [&](const TalkTold& t) {
               return t.key == key && t.teller == teller && t.counted;
           });
}

bool TalkLedger::wasTold(const std::string& listener, const std::string& key) const
{
    const auto found = told_.find(listener);
    if (found == told_.end())
        return false;
    touch("t:" + listener);
    return std::any_of(found->second.begin(), found->second.end(), [&](const TalkTold& t) { return t.key == key; });
}

bool TalkLedger::toldBy(const std::string& listener, const std::string& teller, const std::string& key) const
{
    const auto found = told_.find(listener);
    if (found != told_.end())
        touch("t:" + listener);
    return found != told_.end() &&
           std::any_of(found->second.begin(), found->second.end(), [&](const TalkTold& t) { return t.key == key && t.teller == teller; });
}

std::string TalkLedger::toldMe(const std::string& listener, const std::string& teller) const
{
    const auto found = told_.find(listener);
    if (found == told_.end())
        return {};
    touch("t:" + listener);
    for (auto it = found->second.rbegin(); it != found->second.rend(); ++it)
        if (it->teller == teller && !it->kind.empty())
            return it->kind;
    return {};
}

void TalkLedger::said(const std::string& subject, const std::string& key, const std::string& value, const std::string& by, double until)
{
    if (!enabled || subject.empty() || key.empty())
        return;
    touch("s:" + subject);
    auto& list = said_[subject];
    list.erase(std::remove_if(list.begin(), list.end(), [&](const TalkSaid& s) { return s.key == key; }), list.end());
    list.push_back({key, value, by, until});
    cap(list, SaidPerSubject);
}

std::map<std::string, std::string> TalkLedger::saidOf(const std::string& subject, double day) const
{
    std::map<std::string, std::string> out;
    const auto found = said_.find(subject);
    if (found == said_.end())
        return out;
    touch("s:" + subject);
    for (const auto& s : found->second)
        if (s.until < 0 || day <= s.until)
            out[s.key] = s.value;
    return out;
}

void TalkLedger::heard(const std::string& resident, const std::string& entry)
{
    if (!enabled || resident.empty() || entry.empty())
        return;
    touch("h:" + resident);
    auto& list = heard_[resident];
    if (std::find(list.begin(), list.end(), entry) == list.end())
        list.push_back(entry);
    cap(list, HeardPerResident);
}

bool TalkLedger::hasHeard(const std::string& resident, const std::string& entry) const
{
    const auto found = heard_.find(resident);
    if (found != heard_.end())
        touch("h:" + resident);
    return found != heard_.end() && std::find(found->second.begin(), found->second.end(), entry) != found->second.end();
}

void TalkLedger::keep(const std::string& resident, const std::string& key, const std::string& value, double until)
{
    if (!enabled || resident.empty() || key.empty())
        return;
    touch("k:" + resident);
    kept_[resident][key] = {value, until};
}

std::map<std::string, std::string> TalkLedger::kept(const std::string& resident, double day) const
{
    std::map<std::string, std::string> out;
    const auto found = kept_.find(resident);
    if (found == kept_.end())
        return out;
    touch("k:" + resident);
    for (const auto& [k, v] : found->second)
        if (day <= v.second)
            out[k] = v.first;
    return out;
}

std::map<std::string, std::string> TalkLedger::pairKept(const std::string& a, const std::string& b, double day) const
{
    std::map<std::string, std::string> out;
    if (const auto* list = notes(a, b))
        for (auto it = list->rbegin(); it != list->rend(); ++it)
            for (const auto& [k, v] : it->kept)
                if (day <= v.second)
                    out.emplace(k, v.first);       // (The newest first.)
    return out;
}

void TalkLedger::placeTalk(const std::string& cell, const std::string& topic, const std::string& key, bool news, double day)
{
    if (!enabled || cell.empty())
        return;
    auto& p = places_[cell];
    p.last.push_back({topic, news});
    cap(p.last, PlaceLast);
    const auto today = std::int64_t(std::floor(day));
    if (p.day != today)
    {
        p.day = today;
        p.today.clear();
    }
    ++p.today[key.empty() ? topic : key];
}

int TalkLedger::todayCount(const std::string& cell, const std::string& key, double day) const
{
    const auto found = places_.find(cell);
    if (found == places_.end() || found->second.day != std::int64_t(std::floor(day)))
        return 0;
    const auto n = found->second.today.find(key);
    return n == found->second.today.end() ? 0 : n->second;
}

double TalkLedger::newsShare(const std::string& cell, int window) const
{
    const auto found = places_.find(cell);
    if (found == places_.end() || found->second.last.empty() || window <= 0)
        return -1;
    int seen = 0, news = 0;
    for (auto it = found->second.last.rbegin(); it != found->second.last.rend() && seen < window; ++it, ++seen)
        news += it->second;
    return double(news) / seen;
}

const TalkInvite* TalkLedger::lastInvite(const std::string& a, const std::string& b) const
{
    for (auto it = invites_.rbegin(); it != invites_.rend(); ++it)
        if (it->state != "open" && ((it->by == a && it->to == b) || (it->by == b && it->to == a)))
            return &*it;
    return nullptr;
}

const TalkPromise* TalkLedger::lastPromise(const std::string& by, const std::string& to) const
{
    for (auto it = promises_.rbegin(); it != promises_.rend(); ++it)
        if (it->by == by && it->to == to)
            return &*it;
    return nullptr;
}

void TalkLedger::drift(const std::string& resident, const std::string& issue, int steps, const std::string& event,
                       const std::string& eventId, double day)
{
    if (!enabled || resident.empty() || issue.empty() || steps == 0 || applied(resident, eventId + "|" + issue))
        return;
    now_ = std::max(now_, day);
    auto& d = drift_[resident][issue];
    d.steps = std::clamp(d.steps + steps, -2, 2);
    d.event = event;
    d.day = day;
    auto& done = applied_[resident];
    done.push_back(eventId + "|" + issue);
    cap(done, AppliedPerResident);
}

bool TalkLedger::applied(const std::string& resident, const std::string& eventId) const
{
    const auto found = applied_.find(resident);
    return found != applied_.end() && std::find(found->second.begin(), found->second.end(), eventId) != found->second.end();
}

std::map<std::string, std::pair<int, std::string>> TalkLedger::drifts(const std::string& resident) const
{
    std::map<std::string, std::pair<int, std::string>> out;
    if (const auto found = drift_.find(resident); found != drift_.end())
        for (const auto& [issue, d] : found->second)
            if (d.steps != 0)
                out[issue] = {d.steps, d.event};
    return out;
}

void TalkLedger::moodEvent(const std::string& resident, const std::string& kind, double day)
{
    if (!enabled || resident.empty() || kind.empty())
        return;
    now_ = std::max(now_, day);
    auto& list = moods_[resident];
    list.push_back({day, kind});
    cap(list, MoodsPerResident);
}

std::vector<std::pair<double, std::string>> TalkLedger::moodEvents(const std::string& resident, double since) const
{
    std::vector<std::pair<double, std::string>> out;
    if (const auto found = moods_.find(resident); found != moods_.end())
        for (const auto& e : found->second)
            if (e.first >= since)
                out.push_back(e);
    return out;
}

std::size_t TalkLedger::size() const
{
    std::size_t n = invites_.size() + promises_.size();
    for (const auto& [k, v] : notes_)
        n += v.size();
    for (const auto& [k, v] : told_)
        n += v.size();
    for (const auto& [k, v] : said_)
        n += v.size();
    for (const auto& [k, v] : heard_)
        n += v.size();
    for (const auto& [k, v] : kept_)
        n += v.size();
    return n;
}

void TalkLedger::prune(double day, const std::function<bool(const std::string&)>& gone)
{
    now_ = std::max(now_, day);
    // Those gone (dead, or out of the world) take their drift, its events, their moods and their own kept tags with them
    // (Slice 2's review: kept for good, they grew with every resident who ever lived).
    if (gone)
    {
        for (auto it = drift_.begin(); it != drift_.end();)
            it = gone(it->first) ? drift_.erase(it) : std::next(it);
        for (auto it = applied_.begin(); it != applied_.end();)
            it = gone(it->first) ? applied_.erase(it) : std::next(it);
        for (auto it = moods_.begin(); it != moods_.end();)
            it = gone(it->first) ? moods_.erase(it) : std::next(it);
        for (auto it = kept_.begin(); it != kept_.end();)
            it = gone(it->first) ? (touched_.erase("k:" + it->first), kept_.erase(it)) : std::next(it);
    }
    for (auto it = notes_.begin(); it != notes_.end();)
    {
        auto& list = it->second;
        while (!list.empty() && day - list.front().day > NoteDays)
            list.pop_front();
        it = list.empty() ? (touched_.erase("n:" + it->first), notes_.erase(it)) : std::next(it);
    }
    for (auto it = said_.begin(); it != said_.end();)
    {
        auto& list = it->second;
        list.erase(std::remove_if(list.begin(), list.end(), [&](const TalkSaid& s) { return s.until >= 0 && day > s.until; }), list.end());
        it = list.empty() ? (touched_.erase("s:" + it->first), said_.erase(it)) : std::next(it);
    }
    for (auto it = kept_.begin(); it != kept_.end();)
    {
        for (auto k = it->second.begin(); k != it->second.end();)
            k = day > k->second.second ? it->second.erase(k) : std::next(k);
        it = it->second.empty() ? (touched_.erase("k:" + it->first), kept_.erase(it)) : std::next(it);
    }
    for (auto it = told_.begin(); it != told_.end();)
    {
        auto& list = it->second;
        while (!list.empty() && day - list.front().day > NoteDays)
            list.pop_front();
        it = list.empty() ? (touched_.erase("t:" + it->first), told_.erase(it)) : std::next(it);
    }
    invites_.erase(std::remove_if(invites_.begin(), invites_.end(), [&](const TalkInvite& i) { return day - i.until > NoteDays; }),
                   invites_.end());
    promises_.erase(std::remove_if(promises_.begin(), promises_.end(), [&](const TalkPromise& p) { return day - p.until > NoteDays; }),
                    promises_.end());
    for (auto it = places_.begin(); it != places_.end();)
        it = it->second.day >= 0 && day - double(it->second.day) > 2 ? places_.erase(it) : std::next(it);
    for (auto it = moods_.begin(); it != moods_.end();)
    {
        auto& list = it->second;
        while (!list.empty() && day - list.front().first > MoodDays)
            list.pop_front();
        it = list.empty() ? moods_.erase(it) : std::next(it);
    }
    // Past the cap: the notes out of hearing first, the oldest first (World::gossip writes them again tomorrow); then whole
    // record sets, the least recently used first.
    auto total = size();
    if (total <= Records)
        return;
    {
        std::vector<std::tuple<double, std::string>> unheard;
        for (const auto& [pair, list] : notes_)
            for (const auto& n : list)
                if (n.unheard)
                    unheard.emplace_back(n.day, pair);
        std::sort(unheard.begin(), unheard.end());
        for (const auto& [d, pair] : unheard)
        {
            if (total <= Records)
                break;
            auto& list = notes_[pair];
            const auto at = std::find_if(list.begin(), list.end(), [&](const TalkNote& n) { return n.unheard && n.day == d; });
            if (at == list.end())
                continue;
            list.erase(at);
            --total;
            if (list.empty())
            {
                notes_.erase(pair);
                touched_.erase("n:" + pair);
            }
        }
        if (total <= Records)
            return;
    }
    std::vector<std::pair<double, std::string>> order;
    for (const auto& [k, t] : touched_)
        order.push_back({t, k});
    std::sort(order.begin(), order.end());
    for (const auto& [t, k] : order)
    {
        if (total <= Records)
            break;
        const auto id = k.substr(2);
        std::size_t dropped = 0;
        switch (k[0])
        {
        case 'n': if (auto f = notes_.find(id); f != notes_.end()) dropped = f->second.size(), notes_.erase(f); break;
        case 't': if (auto f = told_.find(id); f != told_.end()) dropped = f->second.size(), told_.erase(f); break;
        case 's': if (auto f = said_.find(id); f != said_.end()) dropped = f->second.size(), said_.erase(f); break;
        case 'h': if (auto f = heard_.find(id); f != heard_.end()) dropped = f->second.size(), heard_.erase(f); break;
        case 'k': if (auto f = kept_.find(id); f != kept_.end()) dropped = f->second.size(), kept_.erase(f); break;
        default: break;
        }
        touched_.erase(k);
        total -= std::min(total, dropped);
    }
}

void TalkLedger::clear()
{
    notes_.clear();
    told_.clear();
    said_.clear();
    heard_.clear();
    kept_.clear();
    places_.clear();
    invites_.clear();
    promises_.clear();
    drift_.clear();
    applied_.clear();
    moods_.clear();
    touched_.clear();
    now_ = 0;
}

json::Value TalkLedger::save() const
{
    // Days to a ten-thousandth (about nine seconds): the checkpoint's whole-written part every 30 s (Slice 2's review).
    const auto d = [](double v) { return std::round(v * 1e4) / 1e4; };
    auto out = json::Value::object();
    auto notes = json::Value::object();
    for (const auto& [pair, list] : notes_)
    {
        auto a = json::Value::array();
        for (const auto& n : list)
        {
            auto j = json::Value::object();
            j.add("day", d(n.day));
            j.add("by", n.opener);
            j.add("topics", strings(n.topics));
            j.add("keys", strings(n.keys));
            if (!n.outcome.empty())
                j.add("outcome", n.outcome);
            if (!n.souredBy.empty())
                j.add("soured_by", n.souredBy);
            if (!n.told.empty())
                j.add("told", strings(n.told));
            if (!n.claims.empty())
                j.add("claims", strings(n.claims));
            if (!n.kept.empty())
            {
                auto kept = json::Value::object();
                for (const auto& [k, v] : n.kept)
                {
                    auto pair = json::Value::array();
                    pair.push(v.first);
                    pair.push(d(v.second));
                    kept.add(k, pair);
                }
                j.add("kept", kept);
            }
            if (!n.thread.empty())
            {
                j.add("thread", n.thread);
                j.add("thread_until", d(n.threadUntil));
            }
            if (n.unheard)
                j.add("unheard", true);
            a.push(j);
        }
        notes.add(pair, a);
    }
    out.add("notes", notes);
    auto told = json::Value::object();
    for (const auto& [who, list] : told_)
    {
        auto a = json::Value::array();
        for (const auto& t : list)
        {
            auto row = json::Value::array();
            row.push(t.teller);
            row.push(t.key);
            row.push(t.kind);
            row.push(d(t.day));
            if (t.counted)
                row.push(true);
            a.push(row);
        }
        told.add(who, a);
    }
    out.add("told", told);
    auto said = json::Value::object();
    for (const auto& [who, list] : said_)
    {
        auto a = json::Value::array();
        for (const auto& s : list)
        {
            auto row = json::Value::array();
            row.push(s.key);
            row.push(s.value);
            row.push(s.by);
            row.push(s.until < 0 ? s.until : d(s.until));
            a.push(row);
        }
        said.add(who, a);
    }
    out.add("said", said);
    auto heard = json::Value::object();
    for (const auto& [who, list] : heard_)
        heard.add(who, strings(std::vector<std::string>(list.begin(), list.end())));
    out.add("heard", heard);
    auto kept = json::Value::object();
    for (const auto& [who, tags] : kept_)
    {
        auto o = json::Value::object();
        for (const auto& [k, v] : tags)
        {
            auto pair = json::Value::array();
            pair.push(v.first);
            pair.push(d(v.second));
            o.add(k, pair);
        }
        kept.add(who, o);
    }
    out.add("kept", kept);
    auto places = json::Value::object();
    for (const auto& [cell, p] : places_)
    {
        auto o = json::Value::object();
        o.add("day", double(p.day));
        auto last = json::Value::array();
        for (const auto& [topic, news] : p.last)
        {
            auto row = json::Value::array();
            row.push(topic);
            row.push(news);
            last.push(row);
        }
        o.add("last", last);
        auto today = json::Value::object();
        for (const auto& [k, n] : p.today)
            today.add(k, n);
        o.add("today", today);
        places.add(cell, o);
    }
    out.add("places", places);
    auto invites = json::Value::array();
    for (const auto& i : invites_)
    {
        auto j = json::Value::object();
        j.add("by", i.by); j.add("to", i.to); j.add("cell", i.cell); j.add("place", i.place); j.add("hour", i.hour);
        j.add("from", d(i.from)); j.add("until", d(i.until)); j.add("state", i.state);
        invites.push(j);
    }
    out.add("invites", invites);
    auto promises = json::Value::array();
    for (const auto& p : promises_)
    {
        auto j = json::Value::object();
        j.add("by", p.by); j.add("to", p.to); j.add("thing", p.thing); j.add("day", d(p.day)); j.add("until", d(p.until));
        j.add("state", p.state);
        promises.push(j);
    }
    out.add("promises", promises);
    // Phase 6: stances drifted, the events applied, the moods' events.
    auto drift = json::Value::object();
    for (const auto& [who, issues] : drift_)
    {
        auto o = json::Value::object();
        for (const auto& [issue, dr] : issues)
        {
            auto row = json::Value::array();
            row.push(dr.steps);
            row.push(dr.event);
            row.push(d(dr.day));
            o.add(issue, row);
        }
        drift.add(who, o);
    }
    out.add("drift", drift);
    auto applied = json::Value::object();
    for (const auto& [who, list] : applied_)
        applied.add(who, strings(std::vector<std::string>(list.begin(), list.end())));
    out.add("applied", applied);
    auto moods = json::Value::object();
    for (const auto& [who, list] : moods_)
    {
        auto a = json::Value::array();
        for (const auto& [day, kind] : list)
        {
            auto row = json::Value::array();
            row.push(d(day));
            row.push(kind);
            a.push(row);
        }
        moods.add(who, a);
    }
    out.add("moods", moods);
    auto touched = json::Value::object();
    for (const auto& [k, t] : touched_)
        touched.add(k, d(t));
    out.add("touched", touched);
    out.add("now", now_);
    return out;
}

void TalkLedger::load(const json::Value& v)
{
    clear();
    if (!v.isObject())
        return;
    for (const auto& [pair, list] : v.object("notes").fields())
        for (const auto& j : list.items())
        {
            TalkNote n;
            n.day = j.number("day");
            n.opener = j.string("by");
            n.topics = strings(j["topics"]);
            n.keys = strings(j["keys"]);
            n.outcome = j.string("outcome");
            n.souredBy = j.string("soured_by");
            n.told = strings(j["told"]);
            n.claims = strings(j["claims"]);
            for (const auto& [k, kv] : j.object("kept").fields())
                if (kv.isArray() && kv.size() == 2)
                    n.kept[k] = {kv.items()[0].asString(""), kv.items()[1].asNumber()};
            n.thread = j.string("thread");
            n.threadUntil = j.number("thread_until");
            n.unheard = j.boolean("unheard");
            auto& d = notes_[pair];
            d.push_back(std::move(n));
            trimNotes(d);
        }
    for (const auto& [who, list] : v.object("told").fields())
        for (const auto& row : list.items())
            if (row.isArray() && (row.size() == 4 || row.size() == 5))
            {
                const auto& r = row.items();
                auto& d = told_[who];
                d.push_back({r[0].asString(""), r[1].asString(""), r[2].asString(""), r[3].asNumber(), row.size() == 5 && r[4].asBool()});
                cap(d, ToldPerResident);
            }
    for (const auto& [who, list] : v.object("said").fields())
        for (const auto& row : list.items())
            if (row.isArray() && row.size() == 4)
            {
                const auto& r = row.items();
                auto& d = said_[who];
                d.push_back({r[0].asString(""), r[1].asString(""), r[2].asString(""), r[3].asNumber()});
                cap(d, SaidPerSubject);
            }
    for (const auto& [who, list] : v.object("heard").fields())
        for (const auto& e : list.items())
        {
            auto& d = heard_[who];
            d.push_back(e.asString(""));
            cap(d, HeardPerResident);
        }
    for (const auto& [who, tags] : v.object("kept").fields())
        for (const auto& [k, kv] : tags.fields())
            if (kv.isArray() && kv.size() == 2)
                kept_[who][k] = {kv.items()[0].asString(""), kv.items()[1].asNumber()};
    for (const auto& [cell, o] : v.object("places").fields())
    {
        auto& p = places_[cell];
        p.day = std::int64_t(o.number("day", -1));
        for (const auto& row : o.array("last"))
            if (row.isArray() && row.size() == 2)
                p.last.push_back({row.items()[0].asString(""), row.items()[1].asBool()});
        cap(p.last, PlaceLast);
        for (const auto& [k, n] : o.object("today").fields())
            p.today[k] = int(n.asNumber());
    }
    for (const auto& j : v.array("invites"))
    {
        TalkInvite i;
        i.by = j.string("by"); i.to = j.string("to"); i.cell = j.string("cell"); i.place = j.string("place");
        i.hour = int(j.number("hour")); i.from = j.number("from"); i.until = j.number("until"); i.state = j.string("state", "open");
        invites_.push_back(std::move(i));
    }
    for (const auto& j : v.array("promises"))
    {
        TalkPromise p;
        p.by = j.string("by"); p.to = j.string("to"); p.thing = j.string("thing"); p.day = j.number("day");
        p.until = j.number("until"); p.state = j.string("state", "open");
        promises_.push_back(std::move(p));
    }
    for (const auto& [who, issues] : v.object("drift").fields())
        for (const auto& [issue, row] : issues.fields())
            if (row.isArray() && row.size() == 3)
                drift_[who][issue] = {std::clamp(int(row.items()[0].asNumber()), -2, 2), row.items()[1].asString(""), row.items()[2].asNumber()};
    for (const auto& [who, list] : v.object("applied").fields())
    {
        auto& d = applied_[who];
        for (const auto& e : list.items())
            d.push_back(e.asString(""));
        cap(d, AppliedPerResident);
    }
    for (const auto& [who, list] : v.object("moods").fields())
    {
        auto& d = moods_[who];
        for (const auto& row : list.items())
            if (row.isArray() && row.size() == 2)
                d.push_back({row.items()[0].asNumber(), row.items()[1].asString("")});
        cap(d, MoodsPerResident);
    }
    for (const auto& [k, t] : v.object("touched").fields())
        touched_[k] = t.asNumber();
    now_ = v.number("now");
}
} // namespace ratw
