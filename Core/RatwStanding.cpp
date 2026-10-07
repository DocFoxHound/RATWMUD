#include "RatwStanding.h"

#include "RatwJsonDoc.h"
#include "RatwPractice.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>

namespace ratw::standing
{
namespace
{
Thresholds build()
{
    // Data/Progression/standing.json, found as practice finds its files: RATW_DATA_DIR, the working directory or one
    // above it, else the source tree.
    namespace fs = std::filesystem;
    Thresholds t;
    fs::path path;
    std::error_code ec;
    if (const char* dir = std::getenv("RATW_DATA_DIR"); dir && *dir)
        path = fs::path(dir) / "Progression" / "standing.json";
    else
    {
        for (auto at = fs::current_path(ec); !ec && !at.empty(); at = at.parent_path())
        {
            if (fs::exists(at / "Data" / "Progression" / "standing.json", ec))
            {
                path = at / "Data" / "Progression" / "standing.json";
                break;
            }
            if (at == at.parent_path())
                break;
        }
#ifdef RATW_SOURCE_DIR
        if (path.empty())
            path = fs::path(RATW_SOURCE_DIR) / "Data" / "Progression" / "standing.json";
#endif
    }
    std::ifstream in(path, std::ios::binary);
    std::stringstream text;
    text << in.rdbuf();
    json::Value doc;
    std::string error;
    if (!in || !json::parse(text.str(), doc, error))
        return t;
    const auto& u = doc.object("unlocks");
    const auto& g = u.object("gifted");
    const auto& q = u.object("quickened");
    t.giftedLevel = int(g.number("socialLevel", t.giftedLevel));
    t.giftedScenes = int(g.number("normalScenes", t.giftedScenes));
    t.quickenedLevel = int(q.number("socialLevel", t.quickenedLevel));
    t.quickenedStars = int(q.number("stars", t.quickenedStars));
    t.quickenedGivers = int(q.number("starGivers", t.quickenedGivers));
    t.quickenedStories = int(q.number("closedStories", t.quickenedStories));
    t.reportDays = int(q.number("reportDays", t.reportDays));
    t.givers = int(u.number("givers", t.givers));
    return t;
}
} // namespace

const Thresholds& thresholds()
{
    static std::once_flag once;
    static Thresholds t;
    std::call_once(once, [] { t = build(); });
    return t;
}

bool meetsGifted(const Measures& m, const Thresholds& t)
{
    return m.socialLevel >= t.giftedLevel && m.normalScenes >= t.giftedScenes;
}

bool meetsQuickened(const Measures& m, const Thresholds& t)
{
    return m.socialLevel >= t.quickenedLevel && m.stars >= t.quickenedStars && m.starGivers >= t.quickenedGivers &&
           m.closedStories >= t.quickenedStories && m.upheldReports == 0;
}

bool open(const Record& r, const std::string& tier)
{
    return tier == "normal" || (tier == "gifted" && r.giftedAt >= 0) || (tier == "quickened" && r.quickenedAt >= 0);
}

std::vector<Progress> progress(const std::string& tier, const Measures& m, const Thresholds& t)
{
    std::vector<Progress> out;
    const auto line = [&](const char* measure, int have, int need, const std::string& words) {
        out.push_back({measure, have, need, words + " " + std::to_string(std::min(have, need)) + " of " + std::to_string(need)});
    };
    if (tier == "gifted")
    {
        line("socialLevel", m.socialLevel, t.giftedLevel, "Social level");
        line("normalScenes", m.normalScenes, t.giftedScenes, "Scenes as a Normal wolf:");
    }
    else if (tier == "quickened")
    {
        line("socialLevel", m.socialLevel, t.quickenedLevel, "Social level");
        line("stars", m.stars, t.quickenedStars, "Stars:");
        line("starGivers", m.starGivers, t.quickenedGivers, "Different wolves who gave them:");
        line("closedStories", m.closedStories, t.quickenedStories, "Stories closed:");
        if (m.upheldReports > 0)
            out.push_back({"upheldReports", m.upheldReports, 0, "No upheld reports in the last " + std::to_string(t.reportDays) + " days"});
    }
    return out;
}

std::string lockedMessage(const std::string& tier, const Measures& m, const Thresholds& t)
{
    std::string still;
    for (const auto& p : progress(tier, m, t))
        if (p.measure == "upheldReports" || p.have < p.need)
            still += (still.empty() ? "" : "; ") + p.label;
    const auto name = tier == "quickened" ? std::string("Quickened") : std::string("Gifted");
    if (tier == "quickened" && still.empty())
        still = "Gifted must open first";
    return name + " isn't open to your account yet: " + (still.empty() ? std::string("keep roleplaying") : still) + ".";
}
} // namespace ratw::standing
