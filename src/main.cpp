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

    std::cout << "STUDENT_ID = " << STUDENT_ID
              << " | SEED = " << SEED
              << " | OPERATIONS = " << OPERATIONS << "\n";

    std::cout << "=== CONFIG ===\n"
              << "Readers:  " << g_cfg.readers  << "\n"
              << "Writers:  " << g_cfg.writers  << "\n"
              << "Ops:      " << g_cfg.ops      << "\n"
              << "Read ms:  " << g_cfg.readMs   << "\n"
              << "Write ms: " << g_cfg.writeMs  << "\n"
              << "Timeout:  " << g_cfg.timeoutMs<< "\n"
              << "Log:      " << g_cfg.logPath  << "\n";

    std::cout << "(Stage 1 stub: synchronization is not implemented yet)\n";
    return 0;
}