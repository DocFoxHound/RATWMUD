#include "RatwPg.h"

#include <algorithm>
#include <mutex>
#include <type_traits>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace ratw
{
namespace
{
// The parts of libpq's C interface the server uses (libpq-fe.h), declared here
// so no PostgreSQL headers are needed to build.
struct PgNotify
{
    char* relname;
    int be_pid;
    char* extra;
    PgNotify* next;
};
constexpr int ConnectionOk = 0;                        // CONNECTION_OK
constexpr int CommandOk = 1, TuplesOk = 2;             // PGRES_COMMAND_OK, PGRES_TUPLES_OK

struct Api
{
    void* (*connectdb)(const char*) = nullptr;
    int (*status)(const void*) = nullptr;
    char* (*errorMessage)(const void*) = nullptr;
    void (*finish)(void*) = nullptr;
    void (*reset)(void*) = nullptr;
    void* (*execParams)(void*, const char*, int, const unsigned*, const char* const*, const int*, const int*, int) = nullptr;
    int (*resultStatus)(const void*) = nullptr;
    char* (*resultErrorMessage)(const void*) = nullptr;
    int (*ntuples)(const void*) = nullptr;
    int (*nfields)(const void*) = nullptr;
    char* (*getvalue)(const void*, int, int) = nullptr;
    int (*getlength)(const void*, int, int) = nullptr;
    int (*getisnull)(const void*, int, int) = nullptr;
    void (*clear)(void*) = nullptr;
    int (*consumeInput)(void*) = nullptr;
    PgNotify* (*notifies)(void*) = nullptr;
    void (*freemem)(void*) = nullptr;
    std::string problem;
};

const Api& api()
{
    static Api loaded;
    static std::once_flag once;
    std::call_once(once, [] {
#if defined(_WIN32)
        HMODULE library = LoadLibraryA("libpq.dll");
        auto symbol = [&](const char* name) { return library ? reinterpret_cast<void*>(GetProcAddress(library, name)) : nullptr; };
#elif defined(__APPLE__)
        void* library = dlopen("libpq.5.dylib", RTLD_NOW | RTLD_LOCAL);
        auto symbol = [&](const char* name) { return library ? dlsym(library, name) : nullptr; };
#else
        void* library = dlopen("libpq.so.5", RTLD_NOW | RTLD_LOCAL);
        auto symbol = [&](const char* name) { return library ? dlsym(library, name) : nullptr; };
#endif
        if (!library)
        {
            loaded.problem = "The PostgreSQL client library (libpq) is not installed.";
            return;
        }
        bool complete = true;
        auto bind = [&](auto& target, const char* name) {
            target = reinterpret_cast<std::remove_reference_t<decltype(target)>>(symbol(name));
            complete = complete && target;
        };
        bind(loaded.connectdb, "PQconnectdb");
        bind(loaded.status, "PQstatus");
        bind(loaded.errorMessage, "PQerrorMessage");
        bind(loaded.finish, "PQfinish");
        bind(loaded.reset, "PQreset");
        bind(loaded.execParams, "PQexecParams");
        bind(loaded.resultStatus, "PQresultStatus");
        bind(loaded.resultErrorMessage, "PQresultErrorMessage");
        bind(loaded.ntuples, "PQntuples");
        bind(loaded.nfields, "PQnfields");
        bind(loaded.getvalue, "PQgetvalue");
        bind(loaded.getlength, "PQgetlength");
        bind(loaded.getisnull, "PQgetisnull");
        bind(loaded.clear, "PQclear");
        bind(loaded.consumeInput, "PQconsumeInput");
        bind(loaded.notifies, "PQnotifies");
        bind(loaded.freemem, "PQfreemem");
        if (!complete)
            loaded.problem = "The installed libpq is missing functions the server needs.";
    });
    return loaded;
}

std::string trimmed(const char* message)
{
    std::string text = message ? message : "";
    while (!text.empty() && (text.back() == '\n' || text.back() == ' '))
        text.pop_back();
    return text;
}
} // namespace

bool PgClient::available(std::string& error)
{
    error = api().problem;
    return error.empty();
}

PgClient::~PgClient()
{
    close();
}

bool PgClient::connect(const std::string& conninfo, std::string& error)
{
    close();
    if (!available(error))
        return false;
    conn_ = api().connectdb(conninfo.c_str());
    if (!conn_ || api().status(conn_) != ConnectionOk)
    {
        error = conn_ ? trimmed(api().errorMessage(conn_)) : "Out of memory.";
        close();
        return false;
    }
    return true;
}

bool PgClient::connected() const
{
    return conn_ && api().status(conn_) == ConnectionOk;
}

void PgClient::close()
{
    if (conn_)
        api().finish(conn_);
    conn_ = nullptr;
    listening_.clear();
}

PgResult PgClient::exec(const std::string& sql, const std::vector<PgValue>& params)
{
    perf::Scope timed(meter, perf::Database);
    PgResult out;
    if (!conn_)
    {
        out.error = "Not connected to the database.";
        return out;
    }
    std::vector<const char*> values;
    std::vector<int> lengths, formats;
    for (const auto& p : params)
    {
        values.push_back(p ? p->c_str() : nullptr);
        lengths.push_back(p ? static_cast<int>(p->size()) : 0);
        formats.push_back(0);
    }
    for (int attempt = 0; attempt < 2; ++attempt)
    {
        if (api().status(conn_) != ConnectionOk)
        {
            api().reset(conn_);                   // Reconnect after a dropped connection (server restart, network).
            if (api().status(conn_) != ConnectionOk)
            {
                out.error = trimmed(api().errorMessage(conn_));
                return out;
            }
            std::string ignored;
            const auto channels = listening_;
            listening_.clear();
            for (const auto& channel : channels)
                listen(channel, ignored);
        }
        void* result = api().execParams(conn_, sql.c_str(), static_cast<int>(params.size()), nullptr, values.data(),
                                        lengths.data(), formats.data(), 0);
        const int status = result ? api().resultStatus(result) : -1;
        if (status == CommandOk || status == TuplesOk)
        {
            const int rows = api().ntuples(result), fields = api().nfields(result);
            out.rows.reserve(static_cast<size_t>(rows));
            for (int r = 0; r < rows; ++r)
            {
                std::vector<PgValue> row;
                for (int f = 0; f < fields; ++f)
                    row.push_back(api().getisnull(result, r, f)
                                      ? PgValue{}
                                      : PgValue{std::string(api().getvalue(result, r, f),
                                                            static_cast<size_t>(api().getlength(result, r, f)))});
                out.rows.push_back(std::move(row));
            }
            out.ok = true;
            api().clear(result);
            return out;
        }
        out.error = trimmed(result ? api().resultErrorMessage(result) : api().errorMessage(conn_));
        if (result)
            api().clear(result);
        if (api().status(conn_) == ConnectionOk)
            return out;                            // A real SQL error: do not retry.
    }
    return out;
}

bool PgClient::listen(const std::string& channel, std::string& error)
{
    if (channel.empty() || channel.size() > 63 ||
        !std::all_of(channel.begin(), channel.end(), [](char c) { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_'; }))
    {
        error = "Invalid channel name.";
        return false;
    }
    const auto result = exec("LISTEN " + channel);
    error = result.error;
    if (result.ok && std::find(listening_.begin(), listening_.end(), channel) == listening_.end())
        listening_.push_back(channel);
    return result.ok;
}

std::vector<std::pair<std::string, std::string>> PgClient::notifications()
{
    std::vector<std::pair<std::string, std::string>> out;
    if (!conn_ || !api().consumeInput(conn_))
        return out;
    while (PgNotify* note = api().notifies(conn_))
    {
        out.emplace_back(note->relname ? note->relname : "", note->extra ? note->extra : "");
        api().freemem(note);
    }
    return out;
}
} // namespace ratw
