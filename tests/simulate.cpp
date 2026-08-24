// Testbench: runs the riscv-tests ISA suite against the simulated Hart.
//
// Every riscv-test reports its result through the HTIF "tohost" word, which the
// test linker script places at the start of the .tohost section:
//
//   tohost == 0              still running
//   tohost == 1              passed
//   tohost odd, > 1          failed self-test number (tohost >> 1)
//   tohost even, non-zero    HTIF device request (not modelled here)
//
// Usage:
//   simulate                  run every rv64 test found in riscv-tests/isa
//   simulate <elf>            run a single ELF file and report it in detail
//   simulate --isa-dir DIR    take the test suite from somewhere else
//   simulate --max-steps N    step budget before a test counts as a timeout
//   simulate -v               print a line for every test, not just failures

#include "elf.h"
#include "rv64gc.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

constexpr const char *DEFAULT_ISA_DIR = "riscv-tests/isa";
constexpr u64 DEFAULT_MAX_STEPS = 10000000;
constexpr u64 TOHOST_PASS = 1;

enum class Status { Pass, Fail, Timeout, Error };

struct Result {
    Status status = Status::Error;
    std::string detail;
    u64 steps = 0;
    bool started = false;   // false when the ELF never made it into the hart
};

static const char *REG_NAMES[NUM_REGS] = {
    "zero", "ra", "sp",  "gp",  "tp", "t0", "t1", "t2",
    "s0",   "s1", "a0",  "a1",  "a2", "a3", "a4", "a5",
    "a6",   "a7", "s2",  "s3",  "s4", "s5", "s6", "s7",
    "s8",   "s9", "s10", "s11", "t3", "t4", "t5", "t6",
};

static const char *statusText(Status status) {
    switch (status) {
    case Status::Pass:    return "PASS";
    case Status::Fail:    return "FAIL";
    case Status::Timeout: return "TIMEOUT";
    case Status::Error:   return "ERROR";
    }
    return "?";
}

static const char *exceptionName(u64 code) {
    switch (code) {
    case EXCEPTION_INSTR_MISALIGN: return "instruction address misaligned";
    case EXCEPTION_INSTR_ACCESS:   return "instruction access fault";
    case EXCEPTION_ILLEGAL_INSTR:  return "illegal instruction";
    case EXCEPTION_BREAKPOINT:     return "breakpoint";
    case EXCEPTION_LOAD_MISALIGN:  return "load address misaligned";
    case EXCEPTION_LOAD_ACCESS:    return "load access fault";
    case EXCEPTION_STORE_MISALIGN: return "store address misaligned";
    case EXCEPTION_STORE_ACCESS:   return "store access fault";
    case EXCEPTION_U_ECALL:        return "environment call from U-mode";
    case EXCEPTION_S_ECALL:        return "environment call from S-mode";
    case EXCEPTION_M_ECALL:        return "environment call from M-mode";
    case EXCEPTION_INSTR_PAGE:     return "instruction page fault";
    case EXCEPTION_LOAD_PAGE:      return "load page fault";
    case EXCEPTION_STORE_PAGE:     return "store page fault";
    case EXCEPTION_DOUBLE_TRAP:    return "double trap";
    }
    return "unknown exception";
}

static std::string hex(u64 value) {
    std::ostringstream os;
    os << "0x" << std::hex << value;
    return os.str();
}

// The tohost word lives at the start of the .tohost section.
static bool findTohost(const Elf &elf, u64 &addr) {
    for (const auto &section : elf.sections) {
        if (section.name == ".tohost") {
            addr = section.header.addr;
            return true;
        }
    }
    return false;
}

// Loads `path` into `hart` and runs it until it reports through tohost, faults,
// or burns through its step budget.
static Result runElf(const fs::path &path, u64 maxSteps, Hart &hart) {
    Result result;

    Elf elf;
    try {
        elf = loadElf(path.c_str());
    } catch (const std::runtime_error &e) {
        result.detail = std::string("could not load ELF: ") + e.what();
        return result;
    }

    u64 tohostAddr = 0;
    if (!findTohost(elf, tohostAddr)) {
        result.detail = "ELF has no .tohost section";
        return result;
    }

    try {
        hart.loadElf(elf);
    } catch (const std::runtime_error &e) {
        result.detail = std::string("could not map ELF: ") + e.what();
        return result;
    }
    hart.reset(elf.header.entry);
    result.started = true;

    for (result.steps = 0; result.steps < maxSteps; ++result.steps) {
        u64 tohost = 0;
        try {
            tohost = hart.memory.read(tohostAddr, 8);
        } catch (const Exception &) {
            result.detail = "tohost at " + hex(tohostAddr) + " is not mapped";
            return result;
        }

        if (tohost != 0) {
            if (tohost == TOHOST_PASS) {
                result.status = Status::Pass;
            } else if (tohost & 1) {
                result.status = Status::Fail;
                result.detail = "test " + std::to_string(tohost >> 1) + " failed";
            } else {
                result.detail = "unsupported HTIF request (tohost=" + hex(tohost) + ")";
            }
            return result;
        }

        try {
            hart.step();
        } catch (const Exception &e) {
            result.detail = std::string(exceptionName(e.code)) + " at pc=" + hex(hart.pc);
            return result;
        }
    }

    result.status = Status::Timeout;
    result.detail = "no result after " + std::to_string(maxSteps) + " steps";
    return result;
}

// Test binaries sit alongside their .dump disassembly in the isa directory.
// This is an RV64GC hart, so the rv32 suites (wrong ELF class) and the
// hypervisor suites (they need the H extension) are left out.
static std::vector<fs::path> discoverTests(const fs::path &dir) {
    std::vector<fs::path> tests;
    for (const auto &entry : fs::directory_iterator(dir)) {
        if (!entry.is_regular_file()) continue;

        const fs::path &path = entry.path();
        if (path.has_extension()) continue;

        const std::string name = path.filename().string();
        if (name.rfind("rv64", 0) != 0) continue;
        if (name.find("-p-") == std::string::npos
                && name.find("-v-") == std::string::npos) {
            continue;
        }
        tests.push_back(path);
    }
    std::sort(tests.begin(), tests.end());
    return tests;
}

// "rv64ui-p-add" belongs to the "rv64ui-p" suite.
static std::string suiteName(const std::string &name) {
    size_t p = name.find("-p-");
    size_t v = name.find("-v-");
    size_t end = std::min(p == std::string::npos ? name.size() : p + 2,
                          v == std::string::npos ? name.size() : v + 2);
    return name.substr(0, end);
}

static void dumpRegs(const Hart &hart) {
    std::cout << "  registers:\n";
    for (usize i = 0; i < NUM_REGS; i += 4) {
        std::cout << "   ";
        for (usize j = i; j < i + 4; ++j) {
            std::cout << " " << std::setw(4) << std::right << REG_NAMES[j] << "="
                      << std::setw(16) << std::setfill('0') << std::right
                      << std::hex << hart.regs[j] << std::setfill(' ');
        }
        std::cout << std::dec << "\n";
    }
}

static int runSingle(const fs::path &path, u64 maxSteps) {
    Hart hart;
    Result result = runElf(path, maxSteps, hart);

    std::cout << path.string() << ": " << statusText(result.status);
    if (!result.detail.empty()) {
        std::cout << " (" << result.detail << ")";
    }
    std::cout << "\n";
    if (result.started) {
        std::cout << "  steps: " << std::dec << result.steps << "\n";
        std::cout << "  pc:    " << hex(hart.pc) << "\n";
        if (result.status != Status::Pass) {
            dumpRegs(hart);
        }
    }
    return result.status == Status::Pass ? 0 : 1;
}

struct Tally {
    u64 passed = 0;
    u64 total = 0;
};

static int runAll(const fs::path &isaDir, u64 maxSteps, bool verbose) {
    std::vector<fs::path> tests;
    try {
        tests = discoverTests(isaDir);
    } catch (const fs::filesystem_error &e) {
        std::cerr << "Could not read " << isaDir << ": " << e.what() << "\n"
                  << "Build the suite first with: make riscv-tests\n";
        return 2;
    }

    if (tests.empty()) {
        std::cerr << "No rv64 tests found in " << isaDir << "\n"
                  << "Build the suite first with: make riscv-tests\n";
        return 2;
    }

    std::cout << "Running " << tests.size() << " tests from " << isaDir.string()
              << "\n\n";

    std::map<std::string, Tally> suites;
    u64 counts[4] = {0, 0, 0, 0};
    usize width = 0;
    for (const auto &path : tests) {
        width = std::max(width, path.filename().string().size());
    }

    for (const auto &path : tests) {
        Hart hart;
        Result result = runElf(path, maxSteps, hart);

        const std::string name = path.filename().string();
        Tally &tally = suites[suiteName(name)];
        tally.total += 1;
        if (result.status == Status::Pass) tally.passed += 1;
        counts[static_cast<int>(result.status)] += 1;

        if (verbose || result.status != Status::Pass) {
            std::cout << "  " << std::setw(static_cast<int>(width)) << std::left
                      << name << "  " << std::setw(7) << std::left
                      << statusText(result.status);
            if (!result.detail.empty()) {
                std::cout << "  " << result.detail;
            }
            std::cout << "\n";
        }
    }

    std::cout << "\n";
    for (const auto &[suite, tally] : suites) {
        std::cout << "  " << std::setw(24) << std::left << suite
                  << tally.passed << "/" << tally.total << "\n";
    }

    const u64 passed = counts[static_cast<int>(Status::Pass)];
    std::cout << "\n  " << passed << "/" << tests.size() << " passed"
              << "   " << counts[static_cast<int>(Status::Fail)] << " failed"
              << "   " << counts[static_cast<int>(Status::Timeout)] << " timed out"
              << "   " << counts[static_cast<int>(Status::Error)] << " errored"
              << "\n";

    return passed == tests.size() ? 0 : 1;
}

static void usage(const char *program) {
    std::cout
        << "Usage: " << program << " [options] [elf]\n\n"
        << "Runs the riscv-tests ISA suite on the simulated Hart. With an ELF\n"
        << "path, runs just that file and reports it in detail.\n\n"
        << "Options:\n"
        << "  --isa-dir DIR    directory holding the tests (default "
        << DEFAULT_ISA_DIR << ")\n"
        << "  --max-steps N    steps before a test times out (default "
        << DEFAULT_MAX_STEPS << ")\n"
        << "  -v, --verbose    print a line for every test, not just failures\n"
        << "  -h, --help       show this message\n";
}

int main(int argc, char **argv) {
    fs::path isaDir = DEFAULT_ISA_DIR;
    u64 maxSteps = DEFAULT_MAX_STEPS;
    bool verbose = false;
    fs::path single;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            usage(argv[0]);
            return 0;
        } else if (arg == "-v" || arg == "--verbose") {
            verbose = true;
        } else if (arg == "--isa-dir" || arg == "--max-steps") {
            if (i + 1 >= argc) {
                std::cerr << arg << " needs an argument\n";
                return 2;
            }
            if (arg == "--isa-dir") {
                isaDir = argv[++i];
            } else {
                maxSteps = std::strtoull(argv[++i], nullptr, 0);
            }
        } else if (arg.size() > 1 && arg[0] == '-') {
            std::cerr << "Unknown option " << arg << "\n";
            usage(argv[0]);
            return 2;
        } else if (single.empty()) {
            single = arg;
        } else {
            std::cerr << "Only one ELF file may be given\n";
            return 2;
        }
    }

    if (!single.empty()) {
        return runSingle(single, maxSteps);
    }
    return runAll(isaDir, maxSteps, verbose);
}
