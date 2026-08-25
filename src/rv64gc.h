#pragma once

#include "common.h"
#include "elf.h"

#include <bitset>
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

constexpr u32 OPCODE_LOAD = 0b0000011;
constexpr u32 OPCODE_LOAD_FP = 0b0000111;
constexpr u32 OPCODE_MISC_MEM = 0b0001111;
constexpr u32 OPCODE_OP_IMM = 0b0010011;
constexpr u32 OPCODE_AUIPC = 0b0010111;
constexpr u32 OPCODE_OP_IMM_32 = 0b0011011;
constexpr u32 OPCODE_STORE = 0b0100011;
constexpr u32 OPCODE_STORE_FP = 0b0100111;
constexpr u32 OPCODE_AMO = 0b0101111;
constexpr u32 OPCODE_OP = 0b0110011;
constexpr u32 OPCODE_LUI = 0b0110111;
constexpr u32 OPCODE_OP_32 = 0b0111011;
constexpr u32 OPCODE_MADD = 0b1000011;
constexpr u32 OPCODE_MSUB = 0b1000111;
constexpr u32 OPCODE_OP_FP = 0b1010011;
constexpr u32 OPCODE_BRANCH = 0b1100011;
constexpr u32 OPCODE_JALR = 0b1100111;
constexpr u32 OPCODE_JAL = 0b1101111;
constexpr u32 OPCODE_SYSTEM = 0b1110011;

constexpr u32 LOAD_BYTE = 0b000;
constexpr u32 LOAD_HALF = 0b001;
constexpr u32 LOAD_WORD = 0b010;
constexpr u32 LOAD_DOUBLE = 0b011;
constexpr u32 LOAD_BYTE_UNSIGNED = 0b100;
constexpr u32 LOAD_HALF_UNSIGNED = 0b101;
constexpr u32 LOAD_WORD_UNSIGNED = 0b110;

constexpr u32 STORE_BYTE = 0b000;
constexpr u32 STORE_HALF = 0b001;
constexpr u32 STORE_WORD = 0b010;
constexpr u32 STORE_DOUBLE = 0b011;

constexpr u32 OP_ADD_SUB = 0b000;
constexpr u32 OP_SLL = 0b001;
constexpr u32 OP_SLT = 0b010;
constexpr u32 OP_SLTU = 0b011;
constexpr u32 OP_XOR = 0b100;
constexpr u32 OP_SRL_SRA = 0b101;
constexpr u32 OP_OR = 0b110;
constexpr u32 OP_AND = 0b111;

constexpr u32 FUNCT7_ADD = 0b0000000;
constexpr u32 FUNCT7_SUB = 0b0100000;
constexpr u32 FUNCT7_SRL = 0b0000000;
constexpr u32 FUNCT7_SRA = 0b0100000;

constexpr u32 OP_IMM_ADDI = 0b000;
constexpr u32 OP_IMM_SLTI = 0b010;
constexpr u32 OP_IMM_SLTIU = 0b011;
constexpr u32 OP_IMM_XORI = 0b100;
constexpr u32 OP_IMM_ORI = 0b110;
constexpr u32 OP_IMM_ANDI = 0b111;
constexpr u32 OP_IMM_SLLI = 0b001;
constexpr u32 OP_IMM_SRLI_SRAI = 0b101;

constexpr u32 OP_IMM_32_ADDIW = 0b000;
constexpr u32 OP_IMM_32_SLLIW = 0b001;
constexpr u32 OP_IMM_32_SRLIW_SRAIW = 0b101;

constexpr u32 OP_32_ADDW_SUBW = 0b000;
constexpr u32 OP_32_SLLW = 0b001;
constexpr u32 OP_32_SRLW_SRAW = 0b101;

constexpr u32 FUNCT7_SRLI = 0b0000000;
constexpr u32 FUNCT7_SRAI = 0b0100000;

constexpr u32 FUNCT6_SRLI = 0b000000;
constexpr u32 FUNCT6_SRAI = 0b010000;

constexpr u32 BRANCH_BEQ = 0b000;
constexpr u32 BRANCH_BNE = 0b001;
constexpr u32 BRANCH_BLT = 0b100;
constexpr u32 BRANCH_BGE = 0b101;
constexpr u32 BRANCH_BLTU = 0b110;
constexpr u32 BRANCH_BGEU = 0b111;

constexpr u32 SYSTEM_ECALL = 0;
constexpr u32 SYSTEM_EBREAK = 1;
constexpr u32 SYSTEM_MRET = 0x302;

constexpr unsigned CSR_REGS = 4096;

constexpr u32 SYSTEM_CSRRW = 0b001;
constexpr u32 SYSTEM_CSRRS = 0b010;
constexpr u32 SYSTEM_CSRRC = 0b011;
constexpr u32 SYSTEM_CSRRWI = 0b101;
constexpr u32 SYSTEM_CSRRSI = 0b110;
constexpr u32 SYSTEM_CSRRCI = 0b111;

constexpr u64 PRIV_U = 0b00;
constexpr u64 PRIV_S = 0b01;
constexpr u64 PRIV_M = 0b11;

constexpr unsigned MVENDORID = 0xf11;
constexpr unsigned MARCHID = 0xf12;
constexpr unsigned MIMPID = 0xf13;
constexpr unsigned MHARTID = 0xf14;
constexpr unsigned MSTATUS = 0x300;
constexpr unsigned MISA = 0x301;
constexpr unsigned MEDELEG = 0x302;
constexpr unsigned MIDELEG = 0x303;
constexpr unsigned MIE = 0x304;
constexpr unsigned MTVEC = 0x305;
constexpr unsigned MSCRATCH = 0x340;
constexpr unsigned MEPC = 0x341;
constexpr unsigned MCAUSE = 0x342;
constexpr unsigned MTVAL = 0x343;
constexpr unsigned SATP = 0x180;
constexpr unsigned PMPCFG0 = 0x3a0;
constexpr unsigned PMPADDR0 = 0x3b0;
constexpr unsigned MNSTATUS = 0x744;
constexpr unsigned TSELECT = 0x7a0;
constexpr unsigned TDATA1 = 0x7a1;
constexpr unsigned TDATA2 = 0x7a2;
constexpr unsigned SSTATUS = 0x100;

template <unsigned bits>
i64 sext(u64 value) {
    assert(bits < 64);
    i64 shift = 64 - bits;
    return (static_cast<i64>(value) << shift) >> shift;
}

struct Exception {
    u64 code;
    u64 val;
    bool interrupt;

    Exception(u64 code, u64 val, bool interrupt = false);
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
    u32 func12() const;

    i64 immI() const;
    i64 immS() const;
    i64 immB() const;
    i64 immU() const;
    i64 immJ() const;
};

struct Registers {
private:
    u64 regs[NUM_REGS];
    u64 trashReg;

public:
    void clear();

    u64& operator[](size_t index) {
        if (index >= NUM_REGS) {
            throw std::out_of_range("Register index out of bounds");
        }
        if (index == 0) {
            // This trashes anything that would set the x0 register
            trashReg = 0;
            return trashReg;
        }
        return regs[index];
    }

    const u64& operator[](size_t index) const {
        if (index >= NUM_REGS) {
            throw std::out_of_range("Register index out of bounds");
        }
        return regs[index];
    }
};

struct CsrEntry {
    u64 resetValue;
    u64 writeMask;
    u64 readMask;
    u16 addr;
};

struct Csrs {
private:
    std::bitset<CSR_REGS> implemented;
    std::map<u16, CsrEntry> table;

    void implement(const CsrEntry &entry);

public:
    u64 regs[CSR_REGS];
    u64 privilege = PRIV_M;

    Csrs();

    void reset();

    u64 read(unsigned addr);
    void write(unsigned addr, u64 value);
    void bitset(unsigned addr, u64 mask);
    void bitclear(unsigned addr, u64 mask);
};

struct Hart {
    Memory memory;
    u64 pc;
    Registers regs;
    Csrs csrs;
    bool halted = false;

    // What the last step() did with the instruction it ran: `trapped` is true
    // when the instruction faulted instead of retiring, and `lastTrap` then
    // describes the fault that was taken.
    bool trapped = false;
    Exception lastTrap{0, 0};

    Hart();

    void loadElf(const Elf &elf);

    Instr decode();
    u64 execute();

    void trapEntry(const Exception &e);
    void trapExit();

    // Puts the hart back into its post-reset state with the PC at `entry`.
    void reset(u64 entry);

    // Executes a single instruction, throwing an Exception on a fault.
    void step();
};

