#include "RatwWorld.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <tuple>

using namespace ratw;
namespace fs = std::filesystem;
namespace
{
int checks = 0;
void expect(bool condition, const std::string& message)
{
    ++checks;
    if (!condition) throw std::runtime_error(message);
}
bool near(double a, double b) { return std::abs(a - b) < 1e-7; }
double separation(Vec2 a, Vec2 b) { return std::hypot(a.x - b.x, a.y - b.y); }

std::string societySignature(const Society& society)
{
    std::ostringstream out;
    out << std::setprecision(17);
    const auto& state = society.state();
    out << state.enabled << ' ' << state.minted << ' ' << state.sunk << ' ' << state.nextEntry << ' '
        << state.budgetDay << ' ' << state.exportsRemaining << ' ' << state.importsRemaining << ' '
        << state.herbPatch << ' ' << state.decisionRemainder;
    for (const auto& account : state.accounts)
    {
        out << "|A " << account.first << ' ' << account.second.cash;
        for (const auto& item : account.second.stock) out << ' ' << item.first << ':' << item.second;
    }
    for (const auto& resident : state.residents)
    {
        const auto& life = resident.second;
        out << "|R " << resident.first << ' ' << life.role << ' ' << life.task << ' ' << life.reason << ' '
            << life.hunger << ' ' << life.fatigue << ' ' << life.progress << ' ' << life.goalCell << ' '
            << life.goalX << ' ' << life.goalY << ' ' << life.wagesToday << ' ' << life.homeCell << ' '
            << life.homeX << ' ' << life.homeY << ' ' << life.relocationCell << ' ' << life.relocationX << ' '
            << life.relocationY;
    }
    for (const auto& entry : state.ledger)
        out << "|L " << entry.sequence << ' ' << entry.day << ' ' << entry.coins << ' ' << entry.kind << ' '
            << entry.from << ' ' << entry.to << ' ' << entry.item << ' ' << entry.quantity;
    return out.str();
}

std::string worldSignature(const World& world)
{
    std::ostringstream out;
    out << std::setprecision(17) << world.time() << ' ' << world.calendarDays() << societySignature(world.society());
    for (const auto& entry : world.factions()) out << "|F " << entry.first << ' ' << entry.second.name << ' ' << entry.second.color;
    for (const auto& entry : world.chapters()) out << "|C " << entry.first << ' ' << entry.second.name;
    for (const auto& entry : world.cells())
    {
        const auto& cell = entry.second;
        out << "|W " << cell.id << ' ' << cell.name << ' ' << cell.width << ' ' << cell.height << ' '
            << cell.region << ' ' << cell.chapter << ' ' << int(cell.weather);
        for (const auto& claim : cell.factionClaims) out << ' ' << claim;
        for (const auto& tile : cell.tiles) out << tile.glyph << tile.height << tile.solid;
    }
    for (const auto& entry : world.entities())
    {
        const auto& e = entry.second;
        out << "|E " << e.id << ' ' << e.cellId << ' ' << e.position.x << ' ' << e.position.y << ' '
            << e.age << ' ' << e.posture << ' ' << e.leaderId << ' ' << e.activity << ' ' << e.input.x << ' '
            << e.input.y << ' ' << e.velocity.x << ' ' << e.velocity.y;
        for (const auto& point : e.path) out << ' ' << point.x << ',' << point.y;
    }
    for (const auto& entry : world.doors()) out << "|D " << entry.first << ' ' << entry.second.open << ' ' << entry.second.locked;
    return out.str();
}

const std::string BaseManifest = "RATW_WORLD 1\n"
    "cell \"west\" \"west.cell\"\n"
    "cell \"east\" \"east.cell\"\n"
    "cell \"room\" \"room.cell\"\n"
    "spawn \"west\" 2.5 3.5\n";
const std::string Politics =
    "faction \"north\" \"North Wardens\" \"#6688AA\"\n"
    "faction \"south\" \"South Wardens\" \"#cc9988\"\n"
    "chapter \"hearth\" \"Hearth Chapter\"\n"
    "territory \"west\" \"north_reach\" \"hearth\" 2 \"south\" \"north\"\n"
    "territory \"room\" \"north_reach\" \"hearth\" 1 \"north\"\n";

struct Fixture
{
    fs::path dir;
    Fixture()
    {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        for (int suffix = 0; suffix < 100; ++suffix)
        {
            const auto candidate = fs::temp_directory_path() / ("ratw-director-" + std::to_string(stamp) + "-" + std::to_string(suffix));
            if (fs::create_directory(candidate)) { dir = candidate; break; }
        }
        if (dir.empty()) throw std::runtime_error("Cannot create an isolated director fixture directory.");
        for (const auto* id : {"west", "east", "room"})
        {
            std::ostringstream out;
            out << "id: " << id << "\nname: " << id << "\ndescription: Political authoring fixture.\nworld: "
                << (std::string(id) == "east" ? 8 : 0) << " 0 0\noutdoors: "
                << (std::string(id) == "room" ? "false" : "true") << "\nweather: clear\ngrid:\n";
            for (int row = 0; row < 8; ++row) out << "........\n";
            write(std::string(id) + ".cell", out.str());
        }
    }
    ~Fixture()
    {
        std::error_code ignored;
        // Remove only this exact fixture-created, unique temporary directory.
        fs::remove_all(dir, ignored);
    }
    void write(const std::string& name, const std::string& contents)
    {
        std::ofstream file(dir / name, std::ios::binary | std::ios::trunc);
        file << contents;
        if (!file) throw std::runtime_error("Cannot write director test fixture.");
    }
    Result load(World& world, const std::string& manifest)
    {
        write("world.ratw", manifest);
        return world.loadWorldFile((dir / "world.ratw").string());
    }
};

void politicalImport()
{
    Fixture fixture;
    World world;
    expect(fixture.load(world, BaseManifest + Politics).ok, "Political manifest imports with declarations after cells.");
    expect(world.factions().size() == 2 && world.chapters().size() == 1, "Both catalogs retained.");
    expect(world.factions().at("south").color == "#cc9988" && world.chapters().at("hearth").name == "Hearth Chapter",
           "Catalog labels and hex colors round trip.");
    expect(world.cell("west")->region == "north_reach" && world.cell("west")->chapter == "hearth" &&
        world.cell("west")->factionClaims == std::vector<std::string>{"north", "south"}, "Contested claims sort without erasing Chapter sites.");
    expect(world.cell("room")->chapter == "hearth" && world.cell("room")->factionClaims == std::vector<std::string>{"north"},
           "Detached interiors retain political associations.");
    expect(world.cell("east")->region == "unassigned" && world.cell("east")->chapter.empty() && world.cell("east")->factionClaims.empty(),
           "Omitted territory uses the legacy neutral default.");
    expect(world.cell("west")->tiles.size() == 64 && world.doors().empty() && world.entities().empty(),
           "Political metadata does not spawn actors or change terrain/portal topology.");
    world.addPlayer("player-director", "Existing character");
    world.tick(.5);
    const auto before = worldSignature(world);
    const std::vector<std::string> malformed = {
        "faction north \"Bad\" \"#112233\"\n", "faction \"bad/id\" \"Bad\" \"#112233\"\n",
        "faction \"test\" \"Bad\" \"#abc\"\n", "faction \"test\" \"Bad\" \"#11223z\"\n",
        "faction \"test\" \"Bad\" \"1122334\"\n", "faction \"test\" \"Bad\" \"#112233\" extra\n",
        "faction \"test\" \"\" \"#112233\"\n", "faction \"test\" \"   \" \"#112233\"\n",
        "faction \"test\" \"" + std::string(121, 'x') + "\" \"#112233\"\n",
        "faction \"test\" \"bad\xC0\xAF\" \"#112233\"\n",
        "faction \"north\" \"Duplicate\" \"#112233\"\n",
        "chapter \"hearth\" \"Duplicate\"\n", "chapter \"bad/id\" \"Bad\"\n", "chapter \"new\" \"   \"\n",
        "chapter \"new\" \"" + std::string(121, 'x') + "\"\n", "chapter \"new\" \"Extra\" 1\n",
        "territory \"west\" \"repeat\" \"-\" 0\n", "territory \"missing\" \"reach\" \"-\" 0\n",
        "territory \"east\" \"bad/id\" \"-\" 0\n", "territory \"east\" \"reach\" \"missing\" 0\n",
        "territory \"east\" \"reach\" \"\" 0\n", "territory \"east\" \"reach\" \"-\" -1\n",
        "territory \"east\" \"reach\" \"-\" 65\n", "territory \"east\" \"reach\" \"-\" 1.0 \"north\"\n",
        "territory \"east\" \"reach\" \"-\" 1\n", "territory \"east\" \"reach\" \"-\" 1 \"missing\"\n",
        "territory \"east\" \"reach\" \"-\" 2 \"north\" \"north\"\n",
        "territory \"east\" \"reach\" \"-\" 0 \"north\"\n",
        "territory \"east\" \"reach\" \"-\" 1 \"bad/id\"\n",
        "territory \"east\" \"" + std::string(49, 'a') + "\" \"-\" 0\n"
    };
    for (const auto& bad : malformed)
    {
        expect(!fixture.load(world, BaseManifest + Politics + bad).ok, "Malformed political record rejected: " + bad);
        expect(worldSignature(world) == before, "Rejected political import preserves live world atomically.");
    }
    expect(fixture.load(world, BaseManifest + "faction \"west\" \"Same ID\" \"#112233\"\nchapter \"west\" \"Same ID\"\n"
        "territory \"west\" \"west\" \"west\" 1 \"west\"\n").ok, "Catalog and cell namespaces remain independent.");
    expect(fixture.load(world, BaseManifest).ok && world.factions().empty() && world.chapters().empty(), "Legacy reload clears previous catalogs.");
    for (const auto& cell : world.cells())
        expect(cell.second.region == "unassigned" && cell.second.factionClaims.empty() && cell.second.chapter.empty(), "Legacy cells have neutral territory.");

    std::ostringstream bounded;
    bounded << BaseManifest;
    for (int index = 0; index < 64; ++index) bounded << "faction \"f_" << index << "\" \"Faction\" \"#123456\"\n";
    for (int index = 0; index < 128; ++index) bounded << "chapter \"c_" << index << "\" \"Chapter\"\n";
    bounded << "territory \"west\" \"reach\" \"c_127\" 64";
    for (int index = 0; index < 64; ++index) bounded << " \"f_" << index << "\"";
    bounded << '\n';
    expect(fixture.load(world, bounded.str()).ok && world.cell("west")->factionClaims.size() == 64, "Exact64-faction/128-Chapter bounds accepted.");
    const auto atLimit = worldSignature(world);
    expect(!fixture.load(world, bounded.str() + "faction \"overflow\" \"Overflow\" \"#123456\"\n").ok &&
        worldSignature(world) == atLimit, "Faction overflow rejected atomically.");
    expect(!fixture.load(world, bounded.str() + "chapter \"overflow\" \"Overflow\"\n").ok &&
        worldSignature(world) == atLimit, "Chapter overflow rejected atomically.");
}

void finiteOperatorTransfers()
{
    Society society;
    society.addPlayer("player-recipient");
    const auto supply = society.moneySupply(), minted = society.state().minted, sunk = society.state().sunk;
    const auto treasury = *society.account("treasury"), recipient = *society.account("player-recipient");
    const auto sequence = society.state().nextEntry;
    expect(society.operatorTransfer("treasury", "player-recipient", "meal", 3, 17).ok, "Combined finite goods and money transfer succeeds.");
    expect(society.account("treasury")->cash == treasury.cash - 17 && society.account("player-recipient")->cash == recipient.cash + 17 &&
        Society::stock(*society.account("treasury"), "meal") == Society::stock(treasury, "meal") - 3 &&
        Society::stock(*society.account("player-recipient"), "meal") == Society::stock(recipient, "meal") + 3, "Both source and recipient move existing assets exactly.");
    const auto& entry = society.state().ledger.back();
    expect(entry.sequence == sequence && entry.kind == "operator transfer" && entry.from == "treasury" &&
        entry.to == "player-recipient" && entry.item == "meal" && entry.quantity == 3 && entry.coins == 17, "Operator transfer emits an attributable ledger record.");
    expect(society.operatorTransfer("player-recipient", "npc_scout", "", 0, 4).ok, "Cash-only existing-account transfer succeeds.");
    expect(society.operatorTransfer("treasury", "npc_scout", "herbs", 2, 0).ok, "Goods-only transfer succeeds without requiring money.");
    expect(society.moneySupply() == supply && society.state().minted == minted && society.state().sunk == sunk && society.conserved(),
           "Operator movements never mint, sink, or replenish money.");
    const auto before = societySignature(society);
    using Request = std::tuple<std::string, std::string, std::string, int, std::int64_t>;
    for (const Request& request : std::vector<Request>{
        {"missing", "npc_scout", "", 0, 1}, {"treasury", "missing", "", 0, 1},
        {"treasury", "treasury", "meal", 1, 1}, {"treasury", "npc_scout", "", 0, -1},
        {"treasury", "npc_scout", "", 0, 1000001}, {"treasury", "npc_scout", "", 0, std::numeric_limits<std::int64_t>::max()},
        {"treasury", "npc_scout", "meal", -1, 0}, {"treasury", "npc_scout", "meal", 100, 0},
        {"treasury", "npc_scout", "meal", std::numeric_limits<int>::max(), 0},
        {"treasury", "npc_scout", "junk", 1, 0}, {"treasury", "npc_scout", "", 1, 0},
        {"treasury", "npc_scout", "meal", 0, 1}, {"treasury", "npc_scout", "", 0, 0},
        {"npc_scout", "treasury", "meal", 99, 0}, {"npc_scout", "treasury", "meal", 1, 1000}})
    {
        expect(!society.operatorTransfer(std::get<0>(request), std::get<1>(request), std::get<2>(request),
            std::get<3>(request), std::get<4>(request)).ok, "Invalid/broke/excessive operator transfer is refused.");
        expect(societySignature(society) == before, "Rejected transfer changes no account, goods, needs, or ledger.");
    }
    auto state = society.state();
    state.accounts["player-recipient"].stock["meal"] = 9999;
    expect(society.restore(state), "Stock-capacity fixture restores.");
    const auto beforeCapacity = societySignature(society);
    expect(!society.operatorTransfer("treasury", "player-recipient", "meal", 2, 5).ok &&
        societySignature(society) == beforeCapacity, "Insufficient item capacity also prevents the cash side of a combined transfer.");
    expect(society.operatorTransfer("treasury", "player-recipient", "meal", 1, 5).ok &&
        Society::stock(*society.account("player-recipient"), "meal") == 10000, "Exact item capacity is accepted.");

    Society large(false);
    large.addPlayer("player-cap");
    auto capacity = large.state();
    capacity.minted = 1000000000;
    capacity.accounts["treasury"].cash = 1000000;
    capacity.accounts["treasury"].stock["herbs"] = 99;
    capacity.accounts["player-cap"].cash = 999000000;
    expect(large.restore(capacity), "Conserved billion-penny capacity fixture restores.");
    expect(large.operatorTransfer("treasury", "player-cap", "herbs", 99, 1000000).ok &&
        large.account("player-cap")->cash == 1000000000 && large.account("treasury")->cash == 0 && large.conserved(),
        "Exact per-call quantity/coin and recipient cash limits remain safe.");
    const auto exact = societySignature(large);
    expect(!large.operatorTransfer("treasury", "player-cap", "meal", 1, 1).ok && societySignature(large) == exact,
           "Capacity-bound empty source cannot produce another penny or partially transfer goods.");
    capacity = large.state();
    capacity.accounts["player-cap"].cash++;
    expect(!large.restore(capacity) && societySignature(large) == exact, "An over-capacity forged account cannot enter the transfer system.");
    Society restored;
    expect(restored.restore(society.state()) && societySignature(restored) == societySignature(society), "Finite intervention balances and ledger survive restore exactly.");
}

void relocationGuards()
{
    World world;
    world.addPlayer("player-director", "Player");
    const auto before = worldSignature(world);
    for (const auto* id : {"missing", "player-director", "npc_keeper", "npc_cook", "npc_porter"})
        expect(!world.relocateResident(id, "exterior", 20.5, 14.5).ok && worldSignature(world) == before,
               "Absent actors, players, and essential economic jobs cannot be relocated.");
    for (const auto& target : std::vector<std::tuple<std::string, double, double>>{
        {"missing", 2.5, 2.5}, {"exterior", -.1, 4.5}, {"exterior", 40, 4.5}, {"exterior", 4.5, 28},
        {"exterior", std::numeric_limits<double>::quiet_NaN(), 4.5}, {"exterior", 4.5, std::numeric_limits<double>::infinity()},
        {"exterior", 8.5, 5.5}})
        expect(!world.relocateResident("npc_scout", std::get<0>(target), std::get<1>(target), std::get<2>(target)).ok &&
            worldSignature(world) == before, "Invalid, nonfinite, missing, and solid homes are rejected atomically.");
    world.entity("npc_scout")->leaderId = "player-director";
    auto recruited = worldSignature(world);
    expect(!world.relocateResident("npc_scout", "exterior", 20.5, 14.5).ok && worldSignature(world) == recruited, "Recruited residents cannot be diverted.");
    world.entity("npc_scout")->leaderId.clear(); world.entity("npc_scout")->state = "following";
    expect(!world.relocateResident("npc_scout", "exterior", 20.5, 14.5).ok, "Following state also protects recruited residents.");
    world.entity("npc_scout")->state.clear();
    // An isolated destination tile in the same room has no physical route.
    auto* tavern = world.cell("tavern");
    for (const auto& offset : std::vector<Vec2>{{0, -1}, {1, 0}, {0, 1}, {-1, 0}})
        tavern->tile(18 + int(offset.x), 20 + int(offset.y))->solid = true;
    const auto isolated = worldSignature(world);
    expect(!world.relocateResident("npc_scout", "tavern", 18.5, 20.5).ok && worldSignature(world) == isolated,
           "Unreachable same-cell homes are rejected without relocating the body.");
}

void relocationJourneyAndPersistence()
{
    World world;
    const std::string id = "npc_scout", destination = "exterior";
    const Vec2 home{20.5, 14.5};
    const auto original = *world.entity(id);
    const auto originalLife = *world.society().resident(id);
    const auto minted = world.society().state().minted;
    const auto nextEntry = world.society().state().nextEntry;
    expect(world.relocateResident(id, destination, home.x, home.y).ok, "Resident relocation accepted across an authored closed doorway.");
    expect(world.entity(id)->cellId == original.cellId && separation(world.entity(id)->position, original.position) == 0 &&
        world.society().resident(id)->homeCell == originalLife.homeCell, "Accepting relocation does not teleport or prematurely change the home.");
    expect(world.society().state().minted == minted && world.society().state().nextEntry == nextEntry && world.society().conserved(),
           "Accepting relocation cannot grant money or create an economic transfer.");
    const auto pending = worldSignature(world);
    expect(!world.relocateResident(id, "loft", 8.5, 6.5).ok && worldSignature(world) == pending, "A second intervention cannot silently overwrite an in-flight relocation.");
    world.tick(2);
    expect(world.entity(id)->cellId == original.cellId && separation(world.entity(id)->position, original.position) > .1 &&
        separation(world.entity(id)->position, original.position) < 4 && world.society().resident(id)->homeCell == originalLife.homeCell,
        "First seconds physically walk through the source cell instead of teleporting.");
    const auto mid = world.save();
    World loaded;
    expect(loaded.restore(mid).ok, "Mid-journey checkpoint restores.");
    expect(loaded.entity(id)->cellId == world.entity(id)->cellId && separation(loaded.entity(id)->position, world.entity(id)->position) < 1e-7 &&
        loaded.society().resident(id)->relocationCell == destination && loaded.society().resident(id)->homeCell == originalLife.homeCell,
        "Mid-journey restart retains body position, target, and old home.");
    bool crossed = false, arrived = false;
    int walkFrames = 0;
    for (int frame = 0; frame < 9000; ++frame)
    {
        const auto previous = *loaded.entity(id);
        loaded.tick(1. / 30.);
        const auto& current = *loaded.entity(id);
        if (current.cellId == previous.cellId)
        {
            if (separation(previous.position, current.position) > .0001) ++walkFrames;
            if (separation(previous.position, current.position) > .5) throw std::runtime_error("Resident moved discontinuously inside a cell.");
        }
        else
        {
            bool viaPortal = false;
            for (const auto& entry : loaded.doors())
            {
                const auto& door = entry.second;
                viaPortal = viaPortal || (door.portal && door.cellId == previous.cellId && door.targetCell == current.cellId &&
                    separation(previous.position, door.position) <= door.reach + .2 && separation(current.position, door.arrival) < .5);
            }
            expect(viaPortal, "Cross-cell movement uses an actual nearby authored portal arrival.");
            crossed = true;
        }
        const auto* life = loaded.society().resident(id);
        if (life->relocationCell.empty())
        {
            arrived = current.cellId == destination && separation(current.position, home) <= .35;
            break;
        }
        if (life->homeCell != originalLife.homeCell) throw std::runtime_error("Home changed before physical arrival.");
    }
    expect(crossed && arrived && walkFrames > 30, "Restored resident physically crosses cells and reaches the chosen home.");
    expect(loaded.society().resident(id)->homeCell == destination && near(loaded.society().resident(id)->homeX, home.x) &&
        near(loaded.society().resident(id)->homeY, home.y), "Only physical arrival commits new home coordinates.");
    expect(loaded.society().conserved(), "Physical relocation retains conserved accounting while independent settlement trades continue.");
    World arrivedReload;
    expect(arrivedReload.restore(loaded.save()).ok && arrivedReload.society().resident(id)->relocationCell.empty() &&
        arrivedReload.society().resident(id)->homeCell == destination, "Committed new home survives a second restart without replaying the relocation.");
    expect(arrivedReload.setTimeOfDay(23).ok, "Night-time sleep check uses the normal clock.");
    arrivedReload.tick(3);
    expect(arrivedReload.society().resident(id)->task == "sleep" && arrivedReload.society().resident(id)->goalCell == destination &&
        arrivedReload.entity(id)->cellId == destination && arrivedReload.entity(id)->posture == "lying", "Resident rests at the new home rather than returning to the old bed.");

    const auto stable = arrivedReload.save();
    const auto before = worldSignature(arrivedReload);
    const std::vector<std::function<void(ResidentLife&)>> damage = {
        [](ResidentLife& life) { life.homeCell = "missing"; },
        [](ResidentLife& life) { life.homeCell = "tavern"; life.homeX = 23.5; life.homeY = 2.5; },
        [](ResidentLife& life) { life.homeX = -.5; },
        [](ResidentLife& life) { life.homeY = 28.5; },
        [](ResidentLife& life) { life.homeX = std::numeric_limits<double>::quiet_NaN(); },
        [](ResidentLife& life) { life.relocationCell = "missing"; life.relocationX = 2.5; life.relocationY = 2.5; },
        [](ResidentLife& life) { life.relocationCell = "exterior"; life.relocationX = 8.5; life.relocationY = 5.5; },
        [](ResidentLife& life) { life.relocationCell = "exterior"; life.relocationX = std::numeric_limits<double>::infinity(); }
    };
    for (const auto& corrupt : damage)
    {
        auto broken = stable; corrupt(broken.society.residents.at(id));
        expect(!arrivedReload.restore(broken).ok && worldSignature(arrivedReload) == before, "Bad saved home/target rejects the entire checkpoint atomically.");
    }
    auto forgedWorker = stable;
    forgedWorker.society.residents.at("npc_cook").relocationCell = "exterior";
    forgedWorker.society.residents.at("npc_cook").relocationX = home.x;
    forgedWorker.society.residents.at("npc_cook").relocationY = home.y;
    expect(!arrivedReload.restore(forgedWorker).ok && worldSignature(arrivedReload) == before, "Forged essential-worker relocation cannot bypass runtime protection through a save.");
    World legacy;
    auto legacyState = legacy.save();
    for (auto& resident : legacyState.society.residents)
    { resident.second.homeCell.clear(); resident.second.homeX = resident.second.homeY = 0; }
    expect(legacy.restore(legacyState).ok && legacy.society().resident("npc_scout")->homeCell == "tavern",
           "Legacy life records without home fields receive their original home defaults.");
}
} // namespace

int main()
{
    try
    {
        politicalImport();
        finiteOperatorTransfers();
        relocationGuards();
        relocationJourneyAndPersistence();
        std::cout << "PASS " << checks << " director core checks\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL after " << checks << " director core checks: " << error.what() << '\n';
        return 1;
    }
}
