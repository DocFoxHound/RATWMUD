#include "RatwSermons.h"

#include "RatwJsonDoc.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <sstream>

namespace ratw::sermons
{
namespace
{
std::filesystem::path file()
{
    namespace fs = std::filesystem;
    std::error_code ec;
    if (const char* dir = std::getenv("RATW_DATA_DIR"); dir && *dir)
        return fs::path(dir) / "Voice" / "sermons.json";
    for (auto at = fs::current_path(ec); !ec && !at.empty(); at = at.parent_path())
    {
        if (fs::exists(at / "Data" / "Voice" / "sermons.json", ec))
            return at / "Data" / "Voice" / "sermons.json";
        if (at == at.parent_path())
            break;
    }
#ifdef RATW_SOURCE_DIR
    return fs::path(RATW_SOURCE_DIR) / "Data" / "Voice" / "sermons.json";
#else
    return fs::path("Data") / "Voice" / "sermons.json";
#endif
}

std::vector<Sermon> load()
{
    std::vector<Sermon> out;
    std::ifstream in(file(), std::ios::binary);
    if (!in)
        return out;
    std::ostringstream text;
    text << in.rdbuf();
    json::Value doc;
    std::string error;
    if (!json::parse(text.str(), doc, error) || doc.string("format") != "ratw-sermons")
        return out;
    for (const auto& s : doc.array("sermons"))
    {
        Sermon sermon{s.string("id"), s.string("title"), s.string("theme"), {}, {}};
        for (const auto& p : s.array("places"))
            sermon.places.push_back(p.asString(""));
        for (const auto& l : s.array("lines"))
            if (!l.asString("").empty())
                sermon.lines.push_back(l.asString(""));
        if (!sermon.id.empty() && !sermon.lines.empty())
            out.push_back(std::move(sermon));
    }
    return out;
}
} // namespace

const std::vector<Sermon>& all()
{
    static const std::vector<Sermon> loaded = load();
    return loaded;
}

const Sermon* forWeek(const std::string& community, long long week)
{
    std::vector<const Sermon*> fit;
    for (const auto& s : all())
        if (s.places.empty() || std::find(s.places.begin(), s.places.end(), community) != s.places.end())
            fit.push_back(&s);
    if (fit.empty())
        return nullptr;
    // In turn, from a place of its own in the list: no sermon twice in four weeks while there are four or more.
    const auto start = std::hash<std::string>{}(community) % fit.size();
    return fit[(start + std::size_t(week < 0 ? 0 : week)) % fit.size()];
}
} // namespace ratw::sermons
