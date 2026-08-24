#pragma once

#include "common.h"
#include "elf.h"

#include <cassert>
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

template <unsigned bits>
i32 sext(u32 value) {
    assert(bits < 32);
    i32 shift = 32 - bits;
    return (static_cast<i32>(value) << shift) >> shift;
}

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
    bool contains(u64 addr, u64 len) const;
    u32 fetch(u64 pc) const;
    u64 read(u64 addr, u64 len) const;
    void write(u64 addr, u64 len, u64 value);
};

struct Memory {
    std::map<u64, MemRegion> regions;

    Memory();

    u32 fetch(u64 pc) const;
    u64 read(u64 addr, u64 len) const;
    void write(u64 addr, u64 len, u64 value);
    void addRegion(u64 base, u64 size);
    void addRegion(MemRegion region);
    void loadElf(const Elf &elf);
};

constexpr usize NUM_REGS = 32;

struct Instr {
    u32 raw;
    
    bool isCompressed() const;

    u32 opcode() const;

    u32 rd() const;
    u32 rs1() const;
    u32 rs2() const;

    u32 funct3() const;
    u32 funct7() const;

    i32 immI() const;
    i32 immS() const;
    i32 immB() const;
    i32 immU() const;
    i32 immJ() const;
};

struct Hart {
    Memory memory;
    u64 pc;
    u64 regs[NUM_REGS];

    Hart();

    void loadElf(const Elf &elf);

    Instr decode();

    // Puts the hart back into its post-reset state with the PC at `entry`.
    void reset(u64 entry);

    // Executes a single instruction, throwing an Exception on a fault.
    void step();
};

