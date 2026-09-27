// The game server's PostgreSQL client: parameters, NULLs, errors and
// notifications, against a real database named by RATW_TEST_DATABASE_URL.
#include "RatwPg.h"
#include "Runtime/RatwCellPrefetch.h"

#include <chrono>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

using namespace ratw;
namespace
{
int checks = 0;
void expect(bool condition, const std::string& message)
{
    ++checks;
    if (!condition)
        throw std::runtime_error(message);
}
} // namespace

int main()
{
    const char* url = std::getenv("RATW_TEST_DATABASE_URL");
    std::string error;
    if (!url || !*url || !PgClient::available(error))
    {
        std::cout << "PostgreSQL client tests skipped: " << (error.empty() ? "RATW_TEST_DATABASE_URL is not set." : error) << '\n';
        return 0;
    }
    try
    {
        PgClient bad;
        expect(!bad.connect("host=127.0.0.1 port=1 connect_timeout=2", error) && !error.empty(), "A bad address is refused with a reason");
        expect(!bad.exec("SELECT 1").ok, "No statements without a connection");

        PgClient db;
        expect(db.connect(url, error), "Connects: " + error);
        auto r = db.exec("SELECT $1::int + 1, $2::text, $3::text", {std::string("41"), std::string("it's \"quoted\"; DROP TABLE x"), PgValue{}});
        expect(r.ok && r.rows.size() == 1, "Parameters are sent separately: " + r.error);
        expect(r.rows[0][0] == PgValue{"42"} && r.rows[0][1] == PgValue{"it's \"quoted\"; DROP TABLE x"} && !r.rows[0][2],
               "Values, text that looks like SQL, and NULL come back exactly");
        const std::string big(3 * 1024 * 1024, 'x');
        r = db.exec("SELECT length($1)", {big});
        expect(r.ok && r.rows[0][0] == PgValue{std::to_string(big.size())}, "Multi-megabyte values survive");
        r = db.exec("SELECT nope FROM nowhere");
        expect(!r.ok && r.error.find("nowhere") != std::string::npos, "SQL errors are reported, not thrown");
        expect(db.exec("SELECT 1").ok, "The connection stays usable after an error");

        expect(!db.listen("Bad Channel; DROP", error), "Channel names are checked");
        expect(db.listen("ratw_test_channel", error), "LISTEN: " + error);
        PgClient sender;
        expect(sender.connect(url, error), "Second connection");
        expect(sender.exec("SELECT pg_notify('ratw_test_channel', $1)", {std::string("{\"release\": 7}")}).ok, "NOTIFY");
        std::vector<std::pair<std::string, std::string>> got;
        for (int i = 0; i < 50 && got.empty(); ++i)
        {
            got = db.notifications();
            if (got.empty())
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        expect(got.size() == 1 && got[0].first == "ratw_test_channel" && got[0].second == "{\"release\": 7}",
               "The notification arrives with its payload");
        expect(db.notifications().empty(), "Each notification arrives once");

        // Fetching cells ahead: a query that names what it was asked for stands in for world.build_cells.
        const std::string echo = "SELECT 'file of ' || $2, 'seams of ' || $2 || ' in build ' || $1";
        const auto waitFor = [](const std::function<bool()>& done) {
            for (int i = 0; i < 250 && !done(); ++i)
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            return done();
        };
        {
            FRatwCellPrefetch prefetch;
            expect(prefetch.Start(url, "7", error, echo), "The prefetcher connects: " + error);
            std::string body, seams;
            expect(!prefetch.Take("north", body, seams), "Nothing is ready before it is wanted");
            prefetch.Want({"north", "south"});
            expect(waitFor([&] { return prefetch.ReadyCount() == 2; }), "Wanted cells are fetched in the background");
            expect(prefetch.Take("north", body, seams) && body == "file of north" && seams == "seams of north in build 7",
                   "A fetched cell is handed over with its seams");
            expect(!prefetch.Take("north", body, seams), "A cell is handed over once");
            std::vector<std::string> many;
            for (int i = 0; i < int(FRatwCellPrefetch::Kept) + 20; ++i)
                many.push_back("cell" + std::to_string(i));
            prefetch.Want(many);
            expect(waitFor([&] { return prefetch.ReadyCount() == FRatwCellPrefetch::Kept; }),
                   "At most so many cells are kept");
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            expect(prefetch.ReadyCount() == FRatwCellPrefetch::Kept, "The oldest are let go to stay within the limit");
            expect(prefetch.Take(many.back(), body, seams) && body == "file of " + many.back(), "The newest are kept");
        }                                                   // Stops its thread on the way out.
        {
            FRatwCellPrefetch broken;
            expect(broken.Start(url, "7", error, "SELECT nonsense FROM nowhere WHERE $1 = $2"), "Connects");
            broken.Want({"x"});
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            std::string body, seams;
            expect(!broken.Take("x", body, seams), "A failed fetch leaves the cell to be loaded as before");
        }
    }
    catch (const std::exception& failure)
    {
        std::cerr << "FAILED after " << checks << " checks: " << failure.what() << '\n';
        return 1;
    }
    std::cout << "PostgreSQL client tests passed: " << checks << " checks.\n";
    return 0;
}
