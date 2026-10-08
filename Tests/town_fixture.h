// A strip of three towns for game tests (Upper Accord, Ser Ferro, Ridgemere, each with an inn by its market), a test
// client, and a game around them (`Town`): shared by letters_tests and gathering_tests. Included inside a test file's
// anonymous namespace, after its `expect`.
json::Value parsed(const std::string& text)
{
    json::Value v;
    std::string error;
    json::parse(text, v, error);
    return v;
}
struct Client final : game::Connection
{
    std::vector<json::Value> events, snapshots;
    sections::Cache cache;
    void event(const std::string& text) override { events.push_back(parsed(text)); }
    void snapshot(const std::string& text) override
    {
        auto v = parsed(text);
        sections::fill(v, cache);
        snapshots.push_back(v);
    }
    void motion(const json::Value&) override {}
    bool allowsLocalCredentials() const override { return true; }
    const json::Value* last(const std::string& type) const
    {
        for (auto it = events.rbegin(); it != events.rend(); ++it)
            if (it->string("type") == type)
                return &*it;
        return nullptr;
    }
    std::string said() const                        // Everything the world told it, since the last call.
    {
        std::string out;
        for (const auto& e : events)
            if (e.string("type") == "system")
                out += e.string("text") + "\n";
        return out;
    }
};
std::string cmd(std::initializer_list<std::pair<const char*, json::Value>> fields)
{
    auto o = json::Value::object();
    for (const auto& [k, v] : fields)
        o.add(k, v);
    return json::dump(o);
}
bool contains(const std::string& text, const std::string& part) { return text.find(part) != std::string::npos; }

// Nine cells in a row: Upper Accord (the capital) in the first two, wild country, Ser Ferro, Ridgemere at the far end.
// Each town's first cell is where its people live, its second its market, with an inn there.
constexpr int Side = 16;
std::string cellId(int i) { return "c_" + std::to_string(i) + "_0"; }
std::string resident(const std::string& who, const std::string& name, const std::string& role, const std::string& work, int home,
                     double hx, int at, double wx)
{
    std::ostringstream r;
    r << "resident \"" << who << "\" \"" << name << "\" \"" << role << "\" \"" << work << "\" \"Someone.\" \"Hello.\" 30 \"timber\" "
      << "\"female\" \"average\" \"saddle\" 3 1 5 1 1 8 17 \"-\" 40 0 1 \"" << cellId(home) << "\" " << hx << " 4.5 \"" << cellId(at)
      << "\" " << wx << " 8.5 \"" << cellId(home) << "\" " << hx << " 5.5\n";
    return r.str();
}
std::string writeStrip(const fs::path& root, bool upstairs = false, bool stalls = false, bool tables = false, bool archive = false)
{
    const std::string layout = "UU..SS.RR";
    fs::remove_all(root);
    fs::create_directories(root / "cells");
    fs::create_directories(root / "seams");
    std::map<std::string, std::string> seams;
    std::ostringstream m;
    m << "RATW_WORLD 3\n";
    const int cells = int(layout.size());
    int seam = 0;
    for (int i = 0; i < cells; ++i)
    {
        std::ofstream cell(root / "cells" / (cellId(i) + ".cell"));
        cell << "id: " << cellId(i) << "\nname: Stretch " << i << "\ndescription: Open ground.\nworld: " << i * Side
             << " 0 0\noutdoors: true\nweather: clear\nsize: " << Side << ' ' << Side << "\ngrid:\n";
        for (int y = 0; y < Side; ++y)
        {
            std::string row(Side, '.');
            if (stalls && y == 5 && (i == 1 || i == 5 || i == 8))   // (Built stalls on each market: doc 39.)
                row[5] = row[7] = row[9] = 'u';
            if (tables && y == 11 && (i == 1 || i == 5 || i == 8))   // (A table in each inn's common room: doc 54, 5.)
                row[12] = 'T';
            cell << row << '\n';
        }
        m << "area \"" << cellId(i) << "\"\n";
        if (i + 1 < cells)
            for (int k = 0; k < Side; ++k)
            {
                const std::string a = "seam_" + std::to_string(seam) + "_a", b = "seam_" + std::to_string(seam) + "_b";
                std::ostringstream one, two;
                one << "door \"" << a << "\" \"Open boundary\" \"" << cellId(i) << "\" " << Side - .5 << ' ' << k + .5 << " \""
                    << cellId(i + 1) << "\" .5 " << k + .5 << " \"" << b << "\" 1 0 1 1 \"E\"\n";
                two << "door \"" << b << "\" \"Open boundary\" \"" << cellId(i + 1) << "\" .5 " << k + .5 << " \"" << cellId(i)
                    << "\" " << Side - .5 << ' ' << k + .5 << " \"" << a << "\" 1 0 1 1 \"W\"\n";
                seams[cellId(i)] += one.str();
                seams[cellId(i + 1)] += two.str();
                ++seam;
            }
    }
    for (const auto& [id, text] : seams)
        std::ofstream(root / "seams" / id) << text;
    const auto region = [](char c) { return c == 'U' ? "upper_accord" : c == 'S' ? "ser_ferro" : c == 'R' ? "ridgemere" : "wilds"; };
    for (int i = 0; i < cells; ++i)
    {
        m << "exits \"" << cellId(i) << "\" " << ((i > 0) + (i + 1 < cells));
        if (i > 0)
            m << " \"" << cellId(i - 1) << '"';
        if (i + 1 < cells)
            m << " \"" << cellId(i + 1) << '"';
        m << '\n';
        m << "territory \"" << cellId(i) << "\" \"" << region(layout[std::size_t(i)]) << "\" \"-\" 0\n";
    }
    m << "spawn \"" << cellId(0) << "\" 8.5 8.5\n";
    m << "economy 20000 100 50 10 12\n";
    for (const auto& [c, tag] : std::map<char, std::string>{{'U', "u"}, {'S', "s"}, {'R', "r"}})
    {
        const int home = int(layout.find(c));
        m << resident(tag + "m", "Merchant " + tag, "merchant", "keeping the stall", home, 2.5, home + 1, 8.5);
        m << resident(tag + "i", "Keeper " + tag, "merchant", "keeping the inn", home, 12.5, home + 1, 12.5);
        for (int n = 1; n <= 3; ++n)
            m << resident(tag + std::to_string(n), "Neighbour " + tag + std::to_string(n), "civilian", "working", home, 3.5 + n, home, 3.5 + n);
    }
    if (archive)                                    // (A records clerk at Upper Accord's market: doc 54, 7.)
        m << resident("ua", "Clerk u", "civilian", "copying the city rolls", 0, 6.5, 1, 4.5);
    if (upstairs)
    {
        // Upper Accord's inn's upstairs (doc 54's beds): a room with four beds, reached by placing wolves there.
        std::ofstream cell(root / "cells" / "c_up.cell");
        cell << "id: c_up\nname: Stretch 1, upstairs\ndescription: Small rooms and beds.\nworld: 0 100 0\noutdoors: false\nweather: clear\nsize: 10 6\ngrid:\n"
             << "..........\n.b..b.....\n..........\n.b..b.....\n..........\n..........\n";
        m << "area \"c_up\"\nterritory \"c_up\" \"upper_accord\" \"-\" 0\n";
        std::ofstream(root / "seams" / "c_up") << "";
    }
    std::ofstream(root / "world.ratw") << m.str();
    return root.string();
}

struct Town
{
    game::Game g;
    Client ash, bo, cy;
    Town(const std::string& world, const std::string& save, double performQuiet = 120, bool voice = false, double awayBreak = 3 * 86400) : g([&] {
          game::Options o;
          o.hiddenNames = true;
          o.devIdentity = true;
          o.forkSnapshots = false;
          o.tiesOptional = true;
          o.worldExport = world;
          o.savePath = save;
          o.performQuietSeconds = performQuiet;
          o.awayBreakSeconds = awayBreak;
#ifdef RATW_SOURCE_DIR
          if (voice)
              o.voiceData = std::string(RATW_SOURCE_DIR) + "/Data/Voice";   // (The router and the written scenes.)
#endif
          return o;
      }())
    {
        std::string problem;
        expect(g.start(problem), "starts: " + problem);
        run(1);
    }
    void enter(Client& c, int n, const char* id, const char* name, int cell, double x)
    {
        c.id = n;
        g.connect(&c);
        g.command(&c, cmd({{"type", "hello"}, {"id", id}, {"name", name}}));
        place(c, cell, x);
        g.world().society().shift("treasury", c.entityId, "", 0, 50, "test purse");
    }
    void place(Client& c, int cell, double x, double y = 8.5)
    {
        auto* e = g.world().entity(c.entityId);
        e->cellId = cellId(cell);
        e->position = {x, y};
    }
    void run(double seconds)
    {
        for (double t = 0; t < seconds; t += .25)
        {
            g.tick(.25);
            for (auto* c : {&ash, &bo, &cy})
                if (!c->snapshots.empty())
                    g.acknowledge(c, c->snapshots.back().number("revision"), false);
        }
    }
    void hours(double h)                            // Game hours pass (the courier's), then a moment for the tick.
    {
        g.world().advanceCalendar(h / 24);
        run(1.5);
    }
    Result letter(Client& c, const std::string& verb, std::initializer_list<std::pair<const char*, json::Value>> more = {})
    {
        auto o = json::Value::object();
        o.add("type", "letter");
        o.add("verb", verb);
        for (const auto& [k, v] : more)
            o.add(k, v);
        c.events.clear();
        g.command(&c, json::dump(o));
        const auto said = c.said();
        return {said.find("can't") == std::string::npos && said.find("know no") == std::string::npos && said.find("are written") == std::string::npos &&
                    said.find("isn't") == std::string::npos && said.find("more than") == std::string::npos && said.find("full") == std::string::npos &&
                    said.find("no such") == std::string::npos && said.find("already") == std::string::npos && said.find("as many") == std::string::npos && said.find("more than you have") == std::string::npos,
                said, {}};
    }
    json::Value caseOf(Client& c)
    {
        g.command(&c, cmd({{"type", "letters"}}));
        const auto* e = c.last("letters");
        return e ? *e : json::Value{};
    }
    json::Value selfOf(Client& c)
    {
        run(.5);
        return c.snapshots.empty() ? json::Value{} : c.snapshots.back()["self"];
    }
};
