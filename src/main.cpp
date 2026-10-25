#define NOMINMAX
#include <windows.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <thread>
#include <atomic>
#include <string>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cstdlib>

constexpr int      STUDENT_ID      = 123;
constexpr int      BASE_OPERATIONS = 20;
constexpr unsigned SEED            = 1000u + (unsigned)STUDENT_ID * 97u;
constexpr int      OPERATIONS      = BASE_OPERATIONS + STUDENT_ID * 5;

struct Config {
    int readers = 3;
    int writers = 2;
    int ops = 10;
    int readMs = 50;
    int writeMs = 50;
    DWORD timeoutMs = 5000;
    bool verbose = true;
    std::string logPath = "events.log";
};

Config g_cfg;

std::atomic<int> g_activeReaders{ 0 };
std::atomic<int> g_activeWriters{ 0 };

CRITICAL_SECTION g_logCs;
std::ofstream    g_log;
LARGE_INTEGER    g_qpcFreq, g_qpcStart;

long long nowMs() {
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return (t.QuadPart - g_qpcStart.QuadPart) * 1000LL / g_qpcFreq.QuadPart;
}

void logEvent(int id, const char* role, const char* event) {
    if (!g_cfg.verbose) return;
    EnterCriticalSection(&g_logCs);
    g_log << "[" << std::setw(6) << std::setfill('0') << nowMs() << " ms] "
          << role << id << " " << event << "\n";
    g_log.flush();
    LeaveCriticalSection(&g_logCs);
}

struct PStats {
    int id = 0;
    const char* role = "";
    int ops = 0;
    long long totalWait = 0;
    long long maxWait   = 0;
    int timeouts = 0;
};

void addWait(PStats& s, long long w) {
    s.ops++;
    s.totalWait += w;
    if (w > s.maxWait) s.maxWait = w;
}

void reader(int id, PStats* st) {
    for (int op = 0; op < g_cfg.ops; ++op) {
        long long waitStart = nowMs();
        logEvent(id, "R", "WAITING_READ");

        // TODO (этап 3): waitOk(g_readTry), g_rmutex, g_resource
        logEvent(id, "R", "ACQUIRED_READ");

        long long w = nowMs() - waitStart;
        addWait(*st, w);

        g_activeReaders++;
        Sleep(g_cfg.readMs);
        g_activeReaders--;

        logEvent(id, "R", "RELEASED_READ");
        // TODO (этап 3): отпустить g_readTry, g_rmutex, g_resource
    }
    logEvent(id, "R", "FINISHED");
}

void writer(int id, PStats* st) {
    for (int op = 0; op < g_cfg.ops; ++op) {
        long long waitStart = nowMs();
        logEvent(id, "W", "WAITING_WRITE");

        // TODO (этап 3): g_wmutex, waitOk(g_readTry), g_resource
        logEvent(id, "W", "ACQUIRED_WRITE");

        long long w = nowMs() - waitStart;
        addWait(*st, w);

        g_activeWriters++;
        Sleep(g_cfg.writeMs);
        g_activeWriters--;

        logEvent(id, "W", "RELEASED_WRITE");
        // TODO (этап 3): отпустить g_resource, g_wmutex, g_readTry
    }
    logEvent(id, "W", "FINISHED");
}

bool parseArgs(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto next = [&](int& v) {
            if (i + 1 >= argc) return false;
            v = std::atoi(argv[++i]);
            return true;
        };
        auto nextD = [&](DWORD& v) {
            if (i + 1 >= argc) return false;
            v = (DWORD)std::strtoul(argv[++i], nullptr, 10);
            return true;
        };

        if (a == "--readers")         { if (!next(g_cfg.readers))  return false; }
        else if (a == "--writers")    { if (!next(g_cfg.writers))  return false; }
        else if (a == "--ops")        { if (!next(g_cfg.ops))      return false; }
        else if (a == "--read-ms")    { if (!next(g_cfg.readMs))   return false; }
        else if (a == "--write-ms")   { if (!next(g_cfg.writeMs))  return false; }
        else if (a == "--timeout-ms") { if (!nextD(g_cfg.timeoutMs)) return false; }
        else if (a == "--log") {
            if (i + 1 >= argc) return false;
            g_cfg.logPath = argv[++i];
        }
        else if (a == "--quiet") { g_cfg.verbose = false; }
        else if (a == "--help") {
            std::cout << "Options:\n"
                      << "  --readers N --writers N --ops N\n"
                      << "  --read-ms N --write-ms N --timeout-ms N\n"
                      << "  --log file.log --quiet\n";
            return false;
        }
        else {
            std::cerr << "Unknown argument: " << a << "\n";
            return false;
        }
    }
    return true;
}

int main(int argc, char** argv) {
    if (!parseArgs(argc, argv)) return 1;
    if (g_cfg.ops <= 0) g_cfg.ops = OPERATIONS;

    std::cout << "STUDENT_ID = " << STUDENT_ID
              << " | SEED = " << SEED
              << " | OPERATIONS = " << OPERATIONS << "\n";

    QueryPerformanceFrequency(&g_qpcFreq);
    QueryPerformanceCounter(&g_qpcStart);
    InitializeCriticalSection(&g_logCs);

    g_log.open(g_cfg.logPath);
    if (!g_log) {
        std::cerr << "Cannot open log file: " << g_cfg.logPath << "\n";
        DeleteCriticalSection(&g_logCs);
        return 1;
    }

    int total = g_cfg.readers + g_cfg.writers;
    std::vector<PStats> stats(total);
    std::vector<std::thread> threads;
    threads.reserve(total);

    int idx = 0;
    for (int i = 0; i < g_cfg.readers; ++i, ++idx) {
        stats[idx].id = idx + 1;
        stats[idx].role = "R";
        threads.emplace_back(reader, idx + 1, &stats[idx]);
    }
    for (int i = 0; i < g_cfg.writers; ++i, ++idx) {
        stats[idx].id = idx + 1;
        stats[idx].role = "W";
        threads.emplace_back(writer, idx + 1, &stats[idx]);
    }

    for (auto& t : threads) t.join();

    long long elapsed = nowMs();

    long long rOps = 0, rWait = 0, rMax = 0;
    long long wOps = 0, wWait = 0, wMax = 0;
    for (auto& s : stats) {
        if (std::string(s.role) == "R") {
            rOps += s.ops; rWait += s.totalWait;
            rMax = std::max(rMax, s.maxWait);
        } else {
            wOps += s.ops; wWait += s.totalWait;
            wMax = std::max(wMax, s.maxWait);
        }
    }

    std::cout << "=== SUMMARY ===\n";
    std::cout << "Readers: ops=" << rOps
              << " avg_wait_ms=" << (rOps ? rWait / rOps : 0)
              << " max_wait_ms=" << rMax << "\n";
    std::cout << "Writers: ops=" << wOps
              << " avg_wait_ms=" << (wOps ? wWait / wOps : 0)
              << " max_wait_ms=" << wMax << "\n";
    std::cout << "Total time, ms: " << elapsed << "\n";

    g_log.close();
    DeleteCriticalSection(&g_logCs);
    return 0;
}