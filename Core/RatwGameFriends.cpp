// Friends and private messages (Docs/Design/50-player-card-friends-safety.md, Phase 3; agreed, doc 48 §3.2-3.3).
// Friends are mutual and by account: a request by handle (or from a card) that the other accepts. Each side chooses
// whether its friend may see which character it is playing; a friend who shares shows their handle under their label.
// Private messages go between friends anywhere, out of character, named by handles (not "tells": that is a Gifted
// wolf's word, the user 2026-10-07). An offline friend's wait in an inbox until they next come: 50 at most, each gone
// after 14 days. The limits are in Data/Social/profile.json.
#include "RatwGame.h"
#include "RatwNames.h"

#include <algorithm>

namespace ratw::game
{
using json::Value;

namespace
{
constexpr double FormerHandleSeconds = 7 * 86400;   // Friends see a changed handle's old one a week (doc 50, 1).
constexpr double FriendsTendSeconds = 600;          // How often old requests and messages are cleared.
} // namespace

Connection* Game::onlineClient(const std::string& account) const
{
    const auto it = online_.find(account);
    return it == online_.end() ? nullptr : it->second;
}

std::string Game::handleOf(const std::string& account) const
{
    const auto it = people_.find(account);
    return it == people_.end() ? std::string() : it->second.handle;
}

std::string Game::accountByHandle(const std::string& handle) const
{
    const auto key = people::handleKey(people::clean(handle, std::size_t(people::rules().handleMax)));
    if (key.empty())
        return {};
    for (const auto& [account, a] : people_)
        if (people::handleKey(a.handle) == key)
            return account;
    return {};
}

bool Game::areFriends(const std::string& a, const std::string& b) const
{
    const auto one = accountKey(a), two = accountKey(b);
    const auto mine = friends_.find(one);
    return !one.empty() && one != two && mine != friends_.end() && mine->second.count(two);
}

std::string Game::sharedHandle(const std::string& viewerAccount, const std::string& target) const
{
    // A friend's handle under their label (doc 50, 4), only when they share their character with this viewer: a handle
    // under a label would otherwise give away the very character they chose not to share.
    const auto mine = friends_.find(viewerAccount);
    if (mine == friends_.end() || mine->second.empty())
        return {};
    const auto other = accountKey(target);
    if (other == viewerAccount || !mine->second.count(other))
        return {};
    const auto theirs = friends_.find(other);
    if (theirs == friends_.end())
        return {};
    const auto link = theirs->second.find(viewerAccount);
    return link != theirs->second.end() && link->second.shares ? handleOf(other) : std::string();
}

void Game::befriend(const std::string& one, const std::string& two)
{
    const double t = now();
    friends_[one][two] = {t, people::rules().shareByDefault};
    friends_[two][one] = {t, people::rules().shareByDefault};
    friendRequests_.erase(std::remove_if(friendRequests_.begin(), friendRequests_.end(),
                                         [&](const people::FriendRequest& r) {
                                             return (r.from == one && r.to == two) || (r.from == two && r.to == one);
                                         }),
                          friendRequests_.end());
}

void Game::unfriend(const std::string& one, const std::string& two)
{
    // A block ends a friendship and any request between the two (doc 50, 7): neither is told why.
    bool changed = false;
    if (auto it = friends_.find(one); it != friends_.end())
        changed |= it->second.erase(two) > 0;
    if (auto it = friends_.find(two); it != friends_.end())
        changed |= it->second.erase(one) > 0;
    const auto before = friendRequests_.size();
    friendRequests_.erase(std::remove_if(friendRequests_.begin(), friendRequests_.end(),
                                         [&](const people::FriendRequest& r) {
                                             return (r.from == one && r.to == two) || (r.from == two && r.to == one);
                                         }),
                          friendRequests_.end());
    if (changed || friendRequests_.size() != before)
    {
        sendFriends(one);
        sendFriends(two);
    }
}

void Game::sendFriends(const std::string& account, const std::string& toast)
{
    // One account's friends, as it may see them: each by handle (and the old one, a week after a change), whether they
    // are here, and the character they're playing only if they share it with this account. No places.
    auto* c = onlineClient(account);
    if (!c)
        return;
    auto e = Value::object();
    e.add("type", "friends");
    auto list = Value::array();
    if (const auto mine = friends_.find(account); mine != friends_.end())
    {
        std::vector<std::pair<std::string, people::FriendLink>> sorted(mine->second.begin(), mine->second.end());
        std::sort(sorted.begin(), sorted.end(), [&](const auto& a, const auto& b) {
            const bool ah = onlineClient(a.first), bh = onlineClient(b.first);
            return ah != bh ? ah : people::handleKey(handleOf(a.first)) < people::handleKey(handleOf(b.first));
        });
        for (const auto& [other, link] : sorted)
        {
            auto row = Value::object();
            row.add("handle", handleOf(other));
            if (const auto p = people_.find(other); p != people_.end() && !p->second.formerHandle.empty() &&
                                                    now() - p->second.handleChangedAt < FormerHandleSeconds)
                row.add("was", p->second.formerHandle);
            const auto* there = onlineClient(other);
            row.add("online", there != nullptr);
            if (there)
                if (const auto theirs = friends_.find(other); theirs != friends_.end())
                    if (const auto back = theirs->second.find(account); back != theirs->second.end() && back->second.shares)
                        if (const auto* wolf = world_.entity(there->entityId))
                            row.add("character", wolf->name);   // (Their true name: they chose to share it.)
            row.add("shares", link.shares);
            row.add("since", link.since);
            list.push(row);
        }
    }
    e.add("friends", list);
    auto incoming = Value::array(), outgoing = Value::array();
    for (const auto& r : friendRequests_)
    {
        auto row = Value::object();
        row.add("at", r.at);
        if (r.to == account)
        {
            row.add("handle", handleOf(r.from));
            // Asked in person, from a card: which wolf asked, as this one knows them.
            if (!r.character.empty() && !c->entityId.empty() && world_.entity(r.character))
                row.add("wolf", names::capitalised(labelFor(c->entityId, r.character)));
            incoming.push(row);
        }
        else if (r.from == account)
        {
            row.add("handle", handleOf(r.to));
            outgoing.push(row);
        }
    }
    e.add("incoming", incoming);
    e.add("outgoing", outgoing);
    if (!toast.empty())
        if (const auto p = people_.find(account); p == people_.end() || p->second.settings.toasts)
            e.add("toast", toast);
    send(c, e);
}

void Game::cameOrWent(Connection* c, bool here)
{
    // A wolf comes into the world or leaves it: its friends' lists change (and they hear of an arrival, if they want
    // to); one who comes gets its own list and the private messages kept for it.
    const auto account = accountKey(c);
    if (account.empty())
        return;
    if (here)
        online_[account] = c;
    else if (const auto it = online_.find(account); it != online_.end() && it->second == c)
        online_.erase(it);
    else
        return;
    if (here)
    {
        sendFriends(account);
        sendCircles(account);                         // (Their circles and invitations: doc 50, 6.)
        deliverInbox(c);
    }
    for (const auto& id : circlesOf(account))         // (Their circles' rosters show them here or away.)
        for (const auto& [who, m] : circles_[id].members)
            if (who != account)
                sendCircles(who);
    if (const auto mine = friends_.find(account); mine != friends_.end())
        for (const auto& [other, link] : mine->second)
            sendFriends(other, here && !handleOf(account).empty() ? handleOf(account) + " is here." : std::string());
}

bool Game::friendsCommand(Connection* c, const json::Value& j, Result& result)
{
    // {"type": "friends", "verb": "request" | "accept" | "decline" | "cancel" | "remove" | "share" | "list",
    //  "handle": "…" | "target": id, "on": bool}.
    const auto verb = j.string("verb");
    const auto mine = accountKey(c);
    if (c->entityId.empty() || mine.empty())
        return false;
    const auto& r = people::rules();
    if (verb == "list")
    {
        sendFriends(mine);
        result = {true, {}, {}};
        return true;
    }
    const bool inPerson = verb == "request" && !j.string("target").empty();
    std::string other;
    if (inPerson)
    {
        const auto* them = world_.entity(j.string("target"));
        if (them && !them->npc)
            other = accountKey(them->id);
    }
    else
        other = accountByHandle(j.string("handle"));
    const auto theirHandle = handleOf(other);
    const auto countWaiting = [&](bool to, const std::string& account) {
        return std::count_if(friendRequests_.begin(), friendRequests_.end(),
                             [&](const people::FriendRequest& q) { return (to ? q.to : q.from) == account; });
    };
    const auto findRequest = [&](const std::string& from, const std::string& to) {
        return std::find_if(friendRequests_.begin(), friendRequests_.end(),
                            [&](const people::FriendRequest& q) { return q.from == from && q.to == to; });
    };
    if (verb == "request")
    {
        // (Someone who has blocked you, or whom you blocked, gets the refusal an unknown handle gets: doc 50, 7.)
        const auto nobody = inPerson ? "They aren't taking friend requests." : "No one by that handle is taking friend requests.";
        if (handleOf(mine).empty())
            result = {false, "Choose a handle first: friends know you by it.", {}};
        else if (other == mine)
            result = {false, "Not yourself.", {}};
        else if (other.empty() || theirHandle.empty() || blockedAccounts(mine, other))
            result = {false, nobody, {}};
        else if (friends_[mine].count(other))
            result = {true, "You are friends with " + theirHandle + " already.", {}};
        else if (findRequest(other, mine) != friendRequests_.end())
        {
            // They asked first: asking back is accepting.
            befriend(mine, other);
            sendFriends(other, handleOf(mine) + " accepted your friend request.");
            result = {true, "You are now friends with " + theirHandle + ".", {}};
        }
        else if (findRequest(mine, other) != friendRequests_.end())
            result = {true, "Your request is waiting for " + theirHandle + ".", {}};
        else if (int(friends_[mine].size()) >= r.friendsMost)
            result = {false, "Your friends list is full.", {}};
        else if (int(friends_[other].size()) >= r.friendsMost)
            result = {false, nobody, {}};
        else if (countWaiting(false, mine) >= r.requestsWaiting)
            result = {false, "You have too many requests waiting for an answer.", {}};
        else if (countWaiting(true, other) >= r.requestsWaiting)
            result = {false, theirHandle + " has too many requests waiting; try again later.", {}};
        else
        {
            friendRequests_.push_back({mine, other, inPerson ? c->entityId : std::string(), now()});
            sendFriends(other, handleOf(mine) + " asked to be your friend.");
            result = {true, "Friend request sent to " + theirHandle + ".", {}};
        }
    }
    else if (verb == "accept" || verb == "decline")
    {
        const auto it = other.empty() ? friendRequests_.end() : findRequest(other, mine);
        if (it == friendRequests_.end())
            result = {false, "No request from them is waiting.", {}};
        else if (verb == "decline")
        {
            friendRequests_.erase(it);                    // (They aren't told: their request simply goes.)
            sendFriends(other);
            result = {true, "Declined.", {}};
        }
        else if (int(friends_[mine].size()) >= r.friendsMost)
            result = {false, "Your friends list is full.", {}};
        else
        {
            befriend(mine, other);
            sendFriends(other, handleOf(mine) + " accepted your friend request.");
            result = {true, "You are now friends with " + theirHandle + ".", {}};
        }
    }
    else if (verb == "cancel")
    {
        const auto it = other.empty() ? friendRequests_.end() : findRequest(mine, other);
        if (it == friendRequests_.end())
            result = {false, "No request of yours is waiting for them.", {}};
        else
        {
            friendRequests_.erase(it);
            sendFriends(other);
            result = {true, "Request withdrawn.", {}};
        }
    }
    else if (verb == "remove" || verb == "share")
    {
        const auto link = friends_[mine].find(other);
        if (other.empty() || link == friends_[mine].end())
            result = {false, "They aren't on your friends list.", {}};
        else if (verb == "remove")
        {
            friends_[mine].erase(other);
            friends_[other].erase(mine);
            sendFriends(other);
            result = {true, "Removed " + theirHandle + " from your friends.", {}};
        }
        else
        {
            link->second.shares = j.boolean("on");
            sendFriends(other);                           // (What they see of you changed; the map follows next snapshot.)
            result = {true, link->second.shares ? theirHandle + " can see which wolf you're playing." : theirHandle + " no longer sees which wolf you're playing.", {}};
        }
    }
    else
        return false;
    if (result.ok)
        saveSoon();
    sendFriends(mine);
    return true;
}

Result Game::privateMessage(Connection* c, const std::string& to, const std::string& written)
{
    // Out of character, to a friend anywhere (doc 50, 4). An offline friend's waits in their inbox. A friend who has
    // muted this wolf simply never gets it, and the sender isn't told.
    const auto mine = accountKey(c);
    const auto other = accountByHandle(to);
    const auto& r = people::rules();
    if (other.empty() || other == mine || !friends_[mine].count(other) || blockedAccounts(mine, other))
        return {false, "Private messages go to friends only.", {}};
    const auto text = people::clean(written, std::size_t(r.messageMost) + 1, true);
    if (text.empty())
        return {false, "Write something first.", {}};
    if (people::clean(written, std::size_t(r.messageMost), true).size() < text.size())
        return {false, "A private message is at most " + std::to_string(r.messageMost) + " characters.", {}};
    const auto theirHandle = handleOf(other), myHandle = handleOf(mine);
    if (const auto p = people_.find(other); p != people_.end() && !p->second.settings.messages)
        return {false, theirHandle + " isn't taking private messages.", {}};
    auto e = Value::object();
    e.add("type", "ooc");
    e.add("channel", "private");
    e.add("sequence", sequence_++);
    e.add("text", text);
    e.add("at", now());
    auto* there = onlineClient(other);
    if (!there && int(inbox_[other].size()) >= r.inboxMost)
        return {false, theirHandle + "'s messages are full until they are next here.", {}};
    if (there)
    {
        if (!hides(there->entityId, c->entityId))
        {
            e.add("speaker", myHandle);
            e.add("with", myHandle);
            send(there, e);
            heardLine(there->entityId, std::uint64_t(e.number("sequence")), c->entityId, "private", text);
        }
    }
    else
        inbox_[other].push_back({"pm-" + guid(), other, mine, c->entityId, text, now()});
    // The sender's own copy, in their PRIVATE feed.
    auto towards = Value::array();
    towards.push(theirHandle);
    e.set("speaker", myHandle);
    e.set("with", theirHandle);
    e.set("to", towards);
    e.set("outgoing", true);
    if (!there)
        e.add("away", true);
    send(c, e);
    saveSoon();
    return {true, there ? std::string() : theirHandle + " is away; it will reach them when they are next here.", {}};
}

void Game::deliverInbox(Connection* c)
{
    // What came while they were away (doc 50, 4), oldest first, each marked with when it was sent; then the inbox is
    // empty. Older than 14 days, from a wolf muted or an account blocked since: never delivered.
    const auto mine = accountKey(c);
    const auto it = inbox_.find(mine);
    if (it == inbox_.end())
        return;
    int delivered = 0;
    const double oldest = now() - people::rules().inboxDays * 86400.0;
    for (const auto& m : it->second)
    {
        if (m.at < oldest || hides(c->entityId, m.fromCharacter) || blockedAccounts(mine, m.from))
            continue;
        auto e = Value::object();
        e.add("type", "ooc");
        e.add("channel", "private");
        e.add("sequence", sequence_++);
        e.add("text", m.text);
        e.add("at", m.at);
        e.add("speaker", handleOf(m.from));
        e.add("with", handleOf(m.from));
        e.add("kept", true);
        send(c, e);
        heardLine(c->entityId, std::uint64_t(e.number("sequence")), m.fromCharacter, "private", m.text);
        ++delivered;
    }
    inbox_.erase(it);
    if (delivered)
        system(c, std::to_string(delivered) + (delivered == 1 ? " private message" : " private messages") +
                      " came while you were away: see PRIVATE.");
    saveSoon();
}

void Game::tendFriends()
{
    // Requests past 14 days, and private messages kept past 14 days, go (doc 50, 4).
    if (now() - friendsTendedAt_ < FriendsTendSeconds)
        return;
    friendsTendedAt_ = now();
    tendCircles();                                  // (Old invitations and nights past, too: doc 50, 6.)
    starBook_.prune(now());                         // (Stars past 30 days leave the recent list: doc 51.)
    tendBooks();                                    // (Books finished after three quiet days: doc 51, Phase 7.)
    const auto& r = people::rules();
    const double requestsFrom = now() - r.requestDays * 86400.0, messagesFrom = now() - r.inboxDays * 86400.0;
    std::set<std::string> touched;
    for (auto it = friendRequests_.begin(); it != friendRequests_.end();)
        if (it->at < requestsFrom)
        {
            touched.insert(it->from);
            touched.insert(it->to);
            it = friendRequests_.erase(it);
        }
        else
            ++it;
    for (auto it = inbox_.begin(); it != inbox_.end();)
    {
        auto& kept = it->second;
        kept.erase(std::remove_if(kept.begin(), kept.end(), [&](const people::PrivateMessage& m) { return m.at < messagesFrom; }), kept.end());
        it = kept.empty() ? inbox_.erase(it) : std::next(it);
    }
    for (const auto& account : touched)
        sendFriends(account);
}

void Game::friendsSave(json::Value& root) const
{
    // Three lists (game.friendships, game.friend_requests and game.private_inbox through game.sections).
    auto friends = Value::array();
    for (const auto& [account, links] : friends_)
        for (const auto& [other, link] : links)
        {
            auto e = Value::object();
            e.add("account", account);
            e.add("friend", other);
            e.add("since", link.since);
            e.add("shares", link.shares);
            friends.push(e);
        }
    auto requests = Value::array();
    for (const auto& q : friendRequests_)
    {
        auto e = Value::object();
        e.add("from", q.from);
        e.add("to", q.to);
        if (!q.character.empty())
            e.add("character", q.character);
        e.add("at", q.at);
        requests.push(e);
    }
    auto inbox = Value::array();
    for (const auto& [account, kept] : inbox_)
        for (const auto& m : kept)
            inbox.push(people::saveMessage(m));
    root.add("friends", friends);
    root.add("requests", requests);
    root.add("inbox", inbox);
}

void Game::friendsLoad(const json::Value& saved)
{
    // Only both sides of a friendship, between accounts the game knows; what has expired is dropped.
    friends_.clear();
    friendRequests_.clear();
    inbox_.clear();
    const auto known = [&](const std::string& account) {
        return !account.empty() && (accounts_.exists(account) || account.rfind("dev:", 0) == 0);
    };
    std::map<std::string, std::map<std::string, people::FriendLink>> sides;
    for (const auto& e : saved.array("friends"))
        if (known(e.string("account")) && known(e.string("friend")) && e.string("account") != e.string("friend"))
            sides[e.string("account")][e.string("friend")] = {e.number("since"), e.boolean("shares", true)};
    for (const auto& [account, links] : sides)
        for (const auto& [other, link] : links)
            if (const auto back = sides.find(other); back != sides.end() && back->second.count(account))
                friends_[account][other] = link;
    const auto& r = people::rules();
    for (const auto& e : saved.array("requests"))
        if (known(e.string("from")) && known(e.string("to")) && e.number("at") >= now() - r.requestDays * 86400.0)
            friendRequests_.push_back({e.string("from"), e.string("to"), e.string("character").substr(0, 80), e.number("at")});
    for (const auto& e : saved.array("inbox"))
    {
        auto m = people::loadMessage(e);
        if (known(m.to) && known(m.from) && !m.text.empty() && m.at >= now() - r.inboxDays * 86400.0 &&
            int(inbox_[m.to].size()) < r.inboxMost)
            inbox_[m.to].push_back(std::move(m));
    }
    for (auto& [account, kept] : inbox_)
        std::sort(kept.begin(), kept.end(), [](const auto& a, const auto& b) { return a.at < b.at; });
}
} // namespace ratw::game
