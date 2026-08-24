#pragma once

#include "common.h"
#include "elf.h"

#include <map>
#include <vector>

constexpr u64 EXCEPTION_INSTR_MISALIGN = 0;
constexpr u64 EXCEPTION_INSTR_ACCESS = 1;
constexpr u64 EXCEPTION_ILLEGAL_INSTR = 2;
constexpr u64 EXCEPTION_BREAKPOINT = 3;
constexpr u64 EXCEPTION_LOAD_MISALIGN = 4;
constexpr u64 EXCEPTION_LOAD_ACCESS = 5;
constexpr u64 EXCEPTION_STORE_MISALIGN = 6;
constexpr u64 EXCEPTION_STORE_ACCESS = 7;
constexpr u64 EXCEPTION_U_ECALL = 8;
constexpr u64 EXCEPTION_S_ECALL = 9;
constexpr u64 EXCEPTION_M_ECALL = 11;
constexpr u64 EXCEPTION_INSTR_PAGE = 12;
constexpr u64 EXCEPTION_LOAD_PAGE = 13;
constexpr u64 EXCEPTION_STORE_PAGE = 15;
constexpr u64 EXCEPTION_DOUBLE_TRAP = 16;

struct Exception {
    u64 code;
    bool interrupt;

    Exception(u64 code, bool interrupt = false);
};

struct MemRegion {
    u64 base;
    u64 size;
    std::vector<u8> data;

    MemRegion(u64 base, u64 size);

    bool contains(u64 addr) const;
    u64 fetch(u64 pc) const;
};

struct Memory {
    std::map<u64, MemRegion> regions;

    Memory();

    u64 fetch(u64 pc) const;
    void addRegion(u64 base, u64 size);
    void addRegion(MemRegion region);
    void loadElf(const Elf &elf);
};

struct Hart {
    Memory memory;

    Hart();

    void loadElf(const Elf &elf);
};

