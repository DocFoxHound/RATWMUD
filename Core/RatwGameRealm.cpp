// The realm's map, whole (RatwRealmMap.h): made once a server, off the game thread, and sent to a client that asks.
#include "RatwGame.h"
#include "RatwRealmMap.h"

#include <chrono>
#include <cmath>

namespace ratw::game
{
void Game::startRealmMap()
{
    // What the map is made from, gathered here on the game thread: the outdoor cells at the world's own level (the
    // commonest height among them), each building's door into its interior, and what its workers do there.
    std::map<double, int> levels;
    for (const auto& [id, c] : world_.cells())
        if (c.outdoors)
            ++levels[std::round(c.worldZ * 100) / 100];
    if (levels.empty())
        return;
    const double level = std::max_element(levels.begin(), levels.end(), [](const auto& a, const auto& b) { return a.second < b.second; })->first;
    realm::Input input;
    input.build = loadedBuild_ ? "build-" + std::to_string(loadedBuild_) : !options_.worldExport.empty() ? options_.worldExport
                  : !options_.worldFile.empty() ? options_.worldFile : std::string("world");
    const bool streamed = world_.streamed();
    std::set<std::string> outdoor;
    for (const auto& [id, c] : world_.cells())
    {
        if (!c.outdoors || std::abs(c.worldZ - level) > .01 || c.width <= 0 || c.height <= 0)
            continue;
        outdoor.insert(id);
        realm::CellIn in;
        in.id = id, in.name = c.name, in.region = c.region, in.width = c.width, in.height = c.height, in.x = c.worldX, in.y = c.worldY;
        if (!streamed && c.loaded)
        {
            in.rows.assign(std::size_t(c.height), std::string(std::size_t(c.width), ' '));
            for (int y = 0; y < c.height; ++y)
                for (int x = 0; x < c.width; ++x)
                    if (const auto* t = c.tile(x, y))
                        in.rows[y][x] = t->glyph;
        }
        input.cells.push_back(std::move(in));
    }
    std::map<std::string, std::string> workWords;
    for (const auto& p : world_.society().positions())
        if (!p.title.empty())
            workWords[p.work.cell] += " " + p.title;
    for (const auto& [id, d] : world_.doors())
    {
        if (!d.portal || d.boundary || d.passage || !outdoor.count(d.cellId))
            continue;
        const auto* inside = world_.cell(d.targetCell);
        if (!inside || inside->outdoors)
            continue;
        realm::DoorIn in;
        in.cell = d.cellId, in.interior = d.targetCell, in.name = inside->name, in.town = inside->region;
        in.words = inside->name + workWords[d.targetCell];
        in.x = d.position.x, in.y = d.position.y, in.innerWidth = inside->width, in.innerHeight = inside->height;
        input.doors.push_back(std::move(in));
    }
    // The cells' rows: a world export's files are at hand; a database build's are read on the thread's own
    // connection, a cell at a time (241 for the realm: a few seconds, no tick waits on it).
    std::string conninfo = streamed && exportCells_.empty() ? options_.conninfo : std::string();
    const auto build = std::to_string(loadedBuild_);
    const auto* exported = exportCells_.empty() ? nullptr : &exportCells_;
    realmThread_ = std::thread([this, input = std::move(input), conninfo, build, exported]() mutable {
        const auto started = std::chrono::steady_clock::now();
        PgClient pg;
        bool connected = false;
        if (!conninfo.empty())
        {
            std::string problem;
            connected = pg.connect(conninfo, problem);
        }
        const auto fetch = [&](const std::string& id, std::string& body) {
            if (exported)
            {
                const auto found = exported->find(id);
                if (found == exported->end())
                    return false;
                body = found->second.first;
                return true;
            }
            if (!connected)
                return false;
            const auto cell = pg.exec("SELECT body FROM world.build_cells WHERE build_id = $1 AND cell_id = $2", {build, id});
            if (!cell.ok || cell.rows.empty() || !cell.rows[0][0])
                return false;
            body = *cell.rows[0][0];
            return true;
        };
        auto made = realm::build(std::move(input), fetch, [this] { return realmStop_.load(); });
        if (made.empty())
            return;
        std::lock_guard<std::mutex> guard(realmLock_);
        realmJson_ = std::make_shared<const std::string>(std::move(made));
        realmSeconds_ = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
    });
}

void Game::realmMapCommand(Connection* c)
{
    std::shared_ptr<const std::string> made;
    {
        std::lock_guard<std::mutex> guard(realmLock_);
        made = realmJson_;
    }
    if (!made)
    {
        auto e = json::Value::object();
        e.add("type", "realmMap");
        e.add("pending", true);
        send(c, e);
        return;
    }
    // Once a connection, and a few times more for a page that lost it: never a stream of it.
    if (++realmSent_[c->id] > 3)
        return;
    if (!realmNoted_)
    {
        realmNoted_ = true;
        double seconds = 0;
        {
            std::lock_guard<std::mutex> guard(realmLock_);
            seconds = realmSeconds_;
        }
        note("info", "RATW_REALM_MAP made in " + std::to_string(seconds) + " s off the game thread; " + std::to_string(made->size()) +
                         " bytes (before the link's compression), sent to a client on asking");
    }
    c->event(*made);
}
} // namespace ratw::game
