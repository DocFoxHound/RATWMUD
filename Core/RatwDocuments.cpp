// The document store (Docs/Design/55-letters-gifts-favours.md, 1): see RatwDocuments.h.
#include "RatwDocuments.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

namespace ratw::documents
{
namespace
{
std::filesystem::path lettersFile()
{
    // Data/Social: RATW_DATA_DIR, else the working directory or one above it, else the source tree.
    namespace fs = std::filesystem;
    std::error_code ec;
    if (const char* dir = std::getenv("RATW_DATA_DIR"); dir && *dir)
        return fs::path(dir) / "Social" / "letters.json";
    for (auto at = fs::current_path(ec); !ec && !at.empty(); at = at.parent_path())
    {
        if (fs::exists(at / "Data" / "Social" / "letters.json", ec))
            return at / "Data" / "Social" / "letters.json";
        if (at == at.parent_path())
            break;
    }
#ifdef RATW_SOURCE_DIR
    return fs::path(RATW_SOURCE_DIR) / "Data" / "Social" / "letters.json";
#else
    return fs::path("Data") / "Social" / "letters.json";
#endif
}
} // namespace

Rules parse(const std::string& text)
{
    Rules r;
    json::Value doc;
    std::string error;
    if (text.empty() || !json::parse(text, doc, error) || !doc.isObject())
        return r;
    r.textMost = std::max(1, int(doc.number("textMost", r.textMost)));
    r.perDay = std::max(1, int(doc.number("perDay", r.perDay)));
    r.unreadMost = std::max(1, int(doc.number("unreadMost", r.unreadMost)));
    const auto& fee = doc.object("fee");
    r.feeTown = int(fee.number("town", r.feeTown));
    r.feeBetween = int(fee.number("between", r.feeBetween));
    r.feePerCells = std::max(1, int(fee.number("perCells", r.feePerCells)));
    const auto& hours = doc.object("hours");
    r.hoursTown = hours.number("town", r.hoursTown);
    r.cellsPerHour = std::max(.1, hours.number("cellsPerHour", r.cellsPerHour));
    r.mostHours = hours.number("mostHours", r.mostHours);
    r.scentDays = doc.number("scentDays", r.scentDays);
    r.sightFamiliarity = doc.number("sightFamiliarity", r.sightFamiliarity);
    r.sendOnFee = int(doc.number("sendOnFee", r.sendOnFee));
    return r;
}

const Rules& rules()
{
    static const Rules r = [] {
        std::ifstream in(lettersFile());
        std::stringstream text;
        text << in.rdbuf();
        if (!in)
            std::cerr << "[warn] RATW_LETTERS no Data/Social/letters.json: the placeholders stand\n";
        return parse(text.str());
    }();
    return r;
}

double courierHours(int cells, bool sameTown, const Rules& r)
{
    return sameTown ? r.hoursTown : std::min(r.mostHours, 1 + std::max(0, cells) / r.cellsPerHour);
}

int courierFee(int cells, bool sameTown, const Rules& r)
{
    return sameTown ? r.feeTown : r.feeBetween + std::max(0, cells) / r.feePerCells;
}

json::Value save(const Document& d)
{
    auto o = json::Value::object();
    o.add("id", d.id);
    o.add("kind", d.kind);
    o.add("author", d.author);
    if (!d.scent.empty())
        o.add("scent", d.scent);
    o.add("to", d.to);
    o.add("text", d.text);
    if (!d.sign.empty())
        o.add("sign", d.sign);
    o.add("fromTown", d.fromTown);
    o.add("postTown", d.postTown);
    o.add("state", d.state);
    o.add("written", d.written);
    o.add("deliverAt", d.deliverAt);
    if (d.readAt >= 0)
        o.add("readAt", d.readAt);
    if (d.kept)
        o.add("kept", true);
    if (d.viaCourier)
        o.add("viaCourier", true);
    if (d.answered)
        o.add("answered", true);
    if (!d.replyTo.empty())
        o.add("replyTo", d.replyTo);
    if (!d.facts.empty())
        o.add("facts", d.facts);
    if (!d.contract.empty())
        o.add("contract", d.contract);
    if (!d.occasion.empty())
        o.add("occasion", d.occasion), o.add("answer", d.answer);
    if (!d.pact.empty())
    {
        o.add("pact", d.pact);
        o.add("party", d.party);
        auto seals = json::Value::array(), witnesses = json::Value::array();
        for (const auto& s : d.seals)
            seals.push(s);
        for (const auto& w : d.witnesses)
            witnesses.push(w);
        o.add("seals", seals);
        o.add("witnesses", witnesses);
    }
    if (!d.escrow.empty())
    {
        auto e = json::Value::object();
        e.add("account", d.escrow);
        e.add("item", d.encItem);
        e.add("quantity", d.encQuantity);
        e.add("coins", d.encCoins);
        e.add("scented", d.scented);
        o.add("enclosure", e);
    }
    return o;
}

Document load(const json::Value& v)
{
    Document d;
    d.id = v.string("id").substr(0, 64);
    d.kind = v.string("kind", "letter").substr(0, 32);
    d.author = v.string("author").substr(0, 128);
    d.scent = v.string("scent").substr(0, 128);
    d.to = v.string("to").substr(0, 128);
    d.text = v.string("text").substr(0, std::size_t(rules().textMost) * 4);   // (Letters, not bytes: room for UTF-8.)
    d.sign = v.string("sign").substr(0, 80);
    d.fromTown = v.string("fromTown").substr(0, 128);
    d.postTown = v.string("postTown").substr(0, 128);
    d.state = v.string("state", "travelling");
    if (d.state != "travelling" && d.state != "waiting" && d.state != "delivered" && d.state != "read" && d.state != "returned" && d.state != "carried")
        d.state = "travelling";
    d.written = v.number("written");
    d.deliverAt = v.number("deliverAt");
    d.readAt = v.number("readAt", -1);
    d.kept = v.boolean("kept");
    d.viaCourier = v.boolean("viaCourier");
    d.answered = v.boolean("answered");
    d.replyTo = v.string("replyTo").substr(0, 64);
    d.facts = v.string("facts").substr(0, 200);
    d.contract = v.string("contract").substr(0, 64);
    d.occasion = v.string("occasion").substr(0, 128);
    d.answer = std::clamp(int(v.number("answer")), -1, 1);
    d.pact = v.string("pact").substr(0, 64);
    d.party = v.string("party").substr(0, 128);
    for (const auto& s : v.array("seals"))
        if (s.isString() && d.seals.size() < 8)
            d.seals.push_back(s.asString().substr(0, 128));
    for (const auto& w : v.array("witnesses"))
        if (w.isString() && d.witnesses.size() < 3)
            d.witnesses.push_back(w.asString().substr(0, 128));
    if (const auto& e = v.object("enclosure"); !e.string("account").empty())
    {
        d.escrow = e.string("account").substr(0, 80);
        d.encItem = e.string("item").substr(0, 128);
        d.encQuantity = std::max(0, int(e.number("quantity")));
        d.encCoins = std::max<std::int64_t>(0, std::int64_t(e.number("coins")));
        d.scented = e.boolean("scented");
    }
    return d;
}

Document& Store::add(Document d)
{
    if (d.id.empty())
        d.id = "doc-" + std::to_string(next++);
    erase(d.id);
    auto& at = docs_[d.id] = std::move(d);
    byReader_.emplace(at.to, at.id);
    byAuthor_.emplace(at.author, at.id);
    if (at.state == "travelling")
        queue_.insert({at.deliverAt, at.id});
    // (Ids from a save keep the counter ahead of them.)
    if (at.id.rfind("doc-", 0) == 0)
        if (const auto n = std::strtoull(at.id.c_str() + 4, nullptr, 10); n >= next)
            next = n + 1;
    return at;
}

Document* Store::find(const std::string& id)
{
    const auto it = docs_.find(id);
    return it == docs_.end() ? nullptr : &it->second;
}

const Document* Store::find(const std::string& id) const
{
    const auto it = docs_.find(id);
    return it == docs_.end() ? nullptr : &it->second;
}

void Store::erase(const std::string& id)
{
    const auto it = docs_.find(id);
    if (it == docs_.end())
        return;
    const auto drop = [&](std::multimap<std::string, std::string>& index, const std::string& key) {
        for (auto [a, b] = index.equal_range(key); a != b; ++a)
            if (a->second == id)
            {
                index.erase(a);
                break;
            }
    };
    drop(byReader_, it->second.to);
    drop(byAuthor_, it->second.author);
    queue_.erase({it->second.deliverAt, id});
    docs_.erase(it);
}

void Store::clear()
{
    docs_.clear();
    byReader_.clear();
    byAuthor_.clear();
    queue_.clear();
}

std::vector<const Document*> Store::forReader(const std::string& reader) const
{
    std::vector<const Document*> out;
    for (auto [a, b] = byReader_.equal_range(reader); a != b; ++a)
        if (const auto* d = find(a->second); d && d->state != "travelling" && d->state != "carried")
            out.push_back(d);
    std::sort(out.begin(), out.end(), [](const Document* x, const Document* y) {
        const bool ux = x->readAt < 0, uy = y->readAt < 0;
        return ux != uy ? ux : x->deliverAt > y->deliverAt;
    });
    return out;
}

int Store::unread(const std::string& reader) const
{
    int n = 0;
    for (auto [a, b] = byReader_.equal_range(reader); a != b; ++a)
        if (const auto* d = find(a->second); d && d->readAt < 0)
            ++n;
    return n;
}

int Store::writtenSince(const std::string& author, double day) const
{
    int n = 0;
    for (auto [a, b] = byAuthor_.equal_range(author); a != b; ++a)
        if (const auto* d = find(a->second); d && d->written >= day)
            ++n;
    return n;
}

std::vector<const Document*> Store::fromAuthor(const std::string& author) const
{
    std::vector<const Document*> out;
    for (auto [a, b] = byAuthor_.equal_range(author); a != b; ++a)
        if (const auto* d = find(a->second))
            out.push_back(d);
    return out;
}

std::vector<std::string> Store::due(double now) const
{
    std::vector<std::string> out;
    for (const auto& [at, id] : queue_)
    {
        if (at > now)
            break;
        out.push_back(id);
    }
    return out;
}

void Store::rescheduled(const std::string& id, double was)
{
    queue_.erase({was, id});
    if (const auto* d = find(id); d && d->state == "travelling")
        queue_.insert({d->deliverAt, id});
}
} // namespace ratw::documents
