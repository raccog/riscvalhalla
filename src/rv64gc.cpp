#include "rv64gc.h"

MemRegion::MemRegion(u64 _base, u64 _size) : base{_base}, size{_size} {
    data.resize(size, 0);
}

bool MemRegion::contains(u64 addr) const {
    return addr >= base && addr < (base + size);
}

bool MemRegion::contains(u64 addr, u64 len) const {
    return len != 0 && addr >= base && (addr - base) <= (size - len);
}

Memory::Memory() : regions{} {}

u32 MemRegion::fetch(u64 pc) const {
    if (!contains(pc)) {
        throw Exception(EXCEPTION_INSTR_ACCESS, pc);
    }
    // A full instruction is 32 bits, but a compressed one sitting at the very
    // end of a region only has 16 bits behind it, so fetch what is there and
    // let the decoder decide how much of it is an instruction.
    u64 width = std::min<u64>(4, base + size - pc);
    if (width < 2) {
        throw Exception(EXCEPTION_INSTR_ACCESS, pc);
    }
    u32 instr = 0;
    for (u64 i = 0; i < width; ++i) {
        instr |= static_cast<u32>(data[pc - base + i]) << (8 * i);
    }
    return instr;
}

u64 MemRegion::read(u64 addr, u64 len) const {
    if (!contains(addr, len)) {
        throw Exception(EXCEPTION_LOAD_ACCESS, addr);
    }
    u64 value = 0;
    for (u64 i = 0; i < len; ++i) {
        value |= static_cast<u64>(data[addr - base + i]) << (8 * i);
    }
    return value;
}

void MemRegion::write(u64 addr, u64 len, u64 value) {
    if (!contains(addr, len)) {
        throw Exception(EXCEPTION_STORE_ACCESS, addr);
    }
    for (u64 i = 0; i < len; ++i) {
        data[addr - base + i] = static_cast<u8>(value >> (8 * i));
    }
}

u32 Memory::fetch(u64 pc) const {
    // rv64gc includes the C extension, so instructions are 16-bit aligned.
    if (pc & 1) {
        throw Exception(EXCEPTION_INSTR_MISALIGN, pc);
    }
    for (const auto &[addr, region] : regions) {
        if (region.contains(pc)) {
            return region.fetch(pc);
        }
    }
    throw Exception(EXCEPTION_INSTR_ACCESS, pc);
}

u64 Memory::read(u64 addr, u64 len) const {
    for (const auto &[regionBase, region] : regions) {
        if (region.contains(addr)) {
            return region.read(addr, len);
        }
    }
    throw Exception(EXCEPTION_LOAD_ACCESS, addr);
}

void Memory::write(u64 addr, u64 len, u64 value) {
    for (auto &[regionBase, region] : regions) {
        if (region.contains(addr)) {
            region.write(addr, len, value);
            return;
        }
    }
    throw Exception(EXCEPTION_STORE_ACCESS, addr);
}

Exception::Exception(u64 _code, u64 _val, bool _interrupt) : code{_code}, val(_val), interrupt{_interrupt} {}

void Memory::addRegion(u64 base, u64 size) {
    regions.insert({base, MemRegion(base, size)});
}

void Memory::addRegion(MemRegion region) {
    regions.insert({region.base, region});
}

void Memory::loadElf(const Elf &elf) {
    if (elf.header.type != ET_EXEC) {
        throw std::runtime_error("Tried to load a non-executable ELF file");
    }
    for (const auto &segment : elf.segments) {
        if (segment.header.type == PT_LOAD) {
            MemRegion region = MemRegion(segment.header.paddr, segment.header.memsz);
            region.data = segment.data;
            addRegion(region);
        }
    }
}

u32 Instr::opcode() const {
    return raw & 0x7f;
}

bool Instr::isCompressed() const {
    return ((raw & 3) != 3);
}

u32 Instr::rd() const {
    return (raw >> 7) & 0x1f;
}

u32 Instr::rs1() const {
    return (raw >> 15) & 0x1f;
}

u32 Instr::rs2() const {
    return (raw >> 20) & 0x1f;
}

u32 Instr::funct3() const {
    return (raw >> 12) & 7;
}

u32 Instr::funct7() const {
    return (raw >> 25) & 0x7f;
}

u32 Instr::func12() const {
    return (raw >> 20) & 0xfff;
}

i64 Instr::immI() const {
    u32 imm = (raw >> 20) & 0xfff;
    return sext<12>(imm);
}

i64 Instr::immS() const {
    u32 imm = ((raw >> 25) & 0x7f) << 5
        | ((raw >> 7) & 0x1f);
    return sext<12>(imm);
}

i64 Instr::immB() const {
    u32 imm = ((raw >> 31) & 1) << 12
        | ((raw >> 7) & 1) << 11
        | ((raw >> 25) & 0x3f) << 5
        | ((raw >> 8) & 0xf) << 1;
    return sext<13>(imm);
}

i64 Instr::immU() const {
    // Bit 31 is the sign bit: the 32-bit result has to be widened as signed so
    // AUIPC and LUI see a negative offset rather than a 4GiB-ish positive one.
    return sext<32>(raw & 0xfffff000);
}

i64 Instr::immJ() const {
    u32 imm = ((raw >> 31) & 1) << 20
        | ((raw >> 12) & 0xff) << 12
        | ((raw >> 20) & 1) << 11
        | ((raw >> 21) & 0x3ff) << 1;
    return sext<21>(imm);
}

void Registers::clear() {
    std::fill(std::begin(regs), std::end(regs), 0);
}

Csrs::Csrs() {
    implement({(2ull << 62), 0, ~0ull, MISA});
    implement({0, 0, ~0ull, MHARTID});
    implement({0, ~0ull, ~0ull, MTVEC});
    implement({0, ~0ull, ~0ull, MTVAL});
    implement({0, 0x1888, 0x1888, MSTATUS});
    implement({0, ~0ull, ~0ull, MEPC});
    implement({0, ~0ull, 0x800000000000001f, MCAUSE});
    implement({0, 0x888, 0x888, MIE});
    implement({0, 0, ~0ull, TSELECT});
    implement({0, 0, ~0ull, TDATA1});
    implement({0, ~0ull, ~0ull, TDATA2});
    reset();
}

void Csrs::implement(const CsrEntry &entry) {
    implemented.set(entry.addr);
    table[entry.addr] = entry;
}

void Csrs::reset() {
    std::fill(std::begin(regs), std::end(regs), 0);
    for (const auto &[addr, entry] : table) {
        regs[addr] = entry.resetValue;
    }
    privilege = PRIV_M;
}

u64 Csrs::read(unsigned addr) {
    // Throw exception for reading without appropriate privilege
    if (privilege < ((addr >> 8) & 0b11)) {
        throw Exception(EXCEPTION_ILLEGAL_INSTR, 0);
    }
    // Throw exception for accessing debug registers
    if ((addr & 0xff0) == 0x7b0) {
        throw Exception(EXCEPTION_ILLEGAL_INSTR, 0);
    }
    // Throw exception if not implemented
    if (!implemented.test(addr)) {
        throw Exception(EXCEPTION_ILLEGAL_INSTR, 0);
    }
    const CsrEntry &entry = table[addr];
    return regs[addr] & entry.readMask;
}

void Csrs::write(unsigned addr, u64 value) {
    // Throw exception for writing to read-only CSRs
    if ((addr >> 10) == 0b11) {
        throw Exception(EXCEPTION_ILLEGAL_INSTR, 0);
    }
    // Throw exception for writing without appropriate privilege
    if (privilege < ((addr >> 8) & 0b11)) {
        throw Exception(EXCEPTION_ILLEGAL_INSTR, 0);
    }
    // Throw exception for accessing debug registers
    if ((addr & 0xff0) == 0x7b0) {
        throw Exception(EXCEPTION_ILLEGAL_INSTR, 0);
    }
    // Throw exception if not implemented
    if (!implemented.test(addr)) {
        throw Exception(EXCEPTION_ILLEGAL_INSTR, 0);
    }
    const CsrEntry &entry = table[addr];
    regs[addr] = (value & entry.writeMask) | (regs[addr] & ~entry.writeMask);
}

void Csrs::bitset(unsigned addr, u64 mask) {
    write(addr, read(addr) | mask);
}

void Csrs::bitclear(unsigned addr, u64 mask) {
    write(addr, read(addr) & ~mask);
}

Hart::Hart() : pc{0} {}

void Hart::loadElf(const Elf &elf) {
    memory.loadElf(elf);
}

void Hart::reset(u64 entry) {
    pc = entry;
    halted = false;
    trapped = false;
    regs.clear();
    csrs.reset();
}

Instr Hart::decode() {
    u32 raw = memory.fetch(pc);
    return Instr{raw};
}

u64 Hart::execute() {
    Instr instr = decode();
    u64 next_pc = pc + 4;
    switch (instr.opcode()) {
    case OPCODE_JAL:
        if (((pc + instr.immJ()) & 3) != 0) {
            throw Exception(EXCEPTION_INSTR_MISALIGN, pc + instr.immJ());
        }
        regs[instr.rd()] = pc + 4;
        next_pc = pc + instr.immJ();
        break;
    case OPCODE_JALR: {
        // JALR clears bit 0 of the target, but not bit 1: misa.C is hardwired
        // off, so IALIGN is 32 and a target that is 2 mod 4 still traps. The
        // exception is reported on the jalr itself, so rd must stay untouched.
        u64 target = (regs[instr.rs1()] + instr.immI()) & ~1ull;
        if ((target & 3) != 0) {
            throw Exception(EXCEPTION_INSTR_MISALIGN, target);
        }
        regs[instr.rd()] = pc + 4;
        next_pc = target;
        break;
    }
    case OPCODE_LUI:
        regs[instr.rd()] = instr.immU();
        break;
    case OPCODE_AUIPC:
        regs[instr.rd()] = pc + instr.immU();
        break;
    case OPCODE_LOAD:
        switch (instr.funct3()) {
        case LOAD_BYTE:
            regs[instr.rd()] = sext<8>(memory.read(regs[instr.rs1()] + instr.immI(), 1));
            break;
        case LOAD_HALF:
            regs[instr.rd()] = sext<16>(memory.read(regs[instr.rs1()] + instr.immI(), 2));
            break;
        case LOAD_WORD:
            regs[instr.rd()] = sext<32>(memory.read(regs[instr.rs1()] + instr.immI(), 4));
            break;
        case LOAD_DOUBLE:
            regs[instr.rd()] = memory.read(regs[instr.rs1()] + instr.immI(), 8);
            break;
        case LOAD_BYTE_UNSIGNED:
            regs[instr.rd()] = memory.read(regs[instr.rs1()] + instr.immI(), 1);
            break;
        case LOAD_HALF_UNSIGNED:
            regs[instr.rd()] = memory.read(regs[instr.rs1()] + instr.immI(), 2);
            break;
        case LOAD_WORD_UNSIGNED:
            regs[instr.rd()] = memory.read(regs[instr.rs1()] + instr.immI(), 4);
            break;
        default:
            throw Exception(EXCEPTION_ILLEGAL_INSTR, instr.raw);
        }
        break;
    case OPCODE_STORE:
        switch (instr.funct3()) {
        case STORE_BYTE:
            memory.write(regs[instr.rs1()] + instr.immS(), 1, regs[instr.rs2()] & 0xff);
            break;
        case STORE_HALF:
            memory.write(regs[instr.rs1()] + instr.immS(), 2, regs[instr.rs2()] & 0xffff);
            break;
        case STORE_WORD:
            memory.write(regs[instr.rs1()] + instr.immS(), 4, 
                    regs[instr.rs2()] & 0xffffffff);
            break;
        case STORE_DOUBLE:
            memory.write(regs[instr.rs1()] + instr.immS(), 8, regs[instr.rs2()]);
            break;
        default:
            throw Exception(EXCEPTION_ILLEGAL_INSTR, instr.raw);
        }
        break;
    case OPCODE_OP:
        switch (instr.funct3()) {
        case OP_ADD_SUB:
            switch (instr.funct7()) {
            case FUNCT7_ADD:
                regs[instr.rd()] = regs[instr.rs1()] + regs[instr.rs2()];
                break;
            case FUNCT7_SUB:
                regs[instr.rd()] = regs[instr.rs1()] - regs[instr.rs2()];
                break;
            default:
                throw Exception(EXCEPTION_ILLEGAL_INSTR, instr.raw);
            }
            break;
        case OP_SLL:
            regs[instr.rd()] = regs[instr.rs1()] << regs[instr.rs2()];
            break;
        case OP_SLT:
            regs[instr.rd()] = (static_cast<i64>(regs[instr.rs1()])
                    < static_cast<i64>(regs[instr.rs2()])) ? 1 : 0;
            break;
        case OP_SLTU:
            regs[instr.rd()] = (regs[instr.rs1()] < regs[instr.rs2()]) ? 1 : 0;
            break;
        case OP_XOR:
            regs[instr.rd()] = regs[instr.rs1()] ^ regs[instr.rs2()];
            break;
        case OP_OR:
            regs[instr.rd()] = regs[instr.rs1()] | regs[instr.rs2()];
            break;
        case OP_AND:
            regs[instr.rd()] = regs[instr.rs1()] & regs[instr.rs2()];
            break;
        case OP_SRL_SRA:
            switch (instr.funct7()) {
            case FUNCT7_SRL:
                regs[instr.rd()] = regs[instr.rs1()] >> regs[instr.rs2()];
                break;
            case FUNCT7_SRA:
                regs[instr.rd()] = static_cast<i64>(regs[instr.rs1()]) 
                    >> regs[instr.rs2()];
                break;
            default:
                throw Exception(EXCEPTION_ILLEGAL_INSTR, instr.raw);
            }
            break;
        default:
            throw Exception(EXCEPTION_ILLEGAL_INSTR, instr.raw);
        }
        break;
    case OPCODE_OP_IMM:
        switch (instr.funct3()) {
        case OP_IMM_ADDI:
            regs[instr.rd()] = regs[instr.rs1()] + instr.immI();
            break;
        case OP_IMM_ORI:
            regs[instr.rd()] = regs[instr.rs1()] | instr.immI();
            break;
        case OP_IMM_XORI:
            regs[instr.rd()] = regs[instr.rs1()] ^ instr.immI();
            break;
        case OP_IMM_ANDI:
            regs[instr.rd()] = regs[instr.rs1()] & instr.immI();
            break;
        case OP_IMM_SLTI:
            regs[instr.rd()] = (static_cast<i64>(regs[instr.rs1()])
                    < instr.immI()) ? 1 : 0;
            break;
        case OP_IMM_SLTIU:
            regs[instr.rd()] = (regs[instr.rs1()]
                    < static_cast<u64>(instr.immI())) ? 1 : 0;
            break;
        case OP_IMM_SLLI: {
            u64 shift = instr.immI() & 0x3f;
            if (shift >= 64) {
                throw Exception(EXCEPTION_ILLEGAL_INSTR, instr.raw);
            }
            regs[instr.rd()] = regs[instr.rs1()] << shift;
            break;
        }
        case OP_IMM_SRLI_SRAI: {
            u64 shift = instr.immI() & 0x3f;
            if (shift >= 64) {
                throw Exception(EXCEPTION_ILLEGAL_INSTR, instr.raw);
            }
            switch (instr.immI() >> 6) {
            case FUNCT6_SRLI:
                regs[instr.rd()] = regs[instr.rs1()] >> shift;
                break;
            case FUNCT6_SRAI:
                regs[instr.rd()] = static_cast<i64>(regs[instr.rs1()]) >> shift;
                break;
            default:
                throw Exception(EXCEPTION_ILLEGAL_INSTR, instr.raw);
            }
            break;
        }
        default:
            throw Exception(EXCEPTION_ILLEGAL_INSTR, instr.raw);
        }
        break;
    case OPCODE_OP_IMM_32:
        switch (instr.funct3()) {
        case OP_IMM_32_ADDIW:
            regs[instr.rd()] = sext<32>(regs[instr.rs1()] + instr.immI());
            break;
        case OP_IMM_32_SLLIW:
            regs[instr.rd()] = sext<32>(regs[instr.rs1()] << instr.immI());
            break;
        case OP_IMM_32_SRLIW_SRAIW:
            switch (instr.funct7()) {
            case FUNCT7_SRLI:
                regs[instr.rd()] = sext<32>(static_cast<u32>(regs[instr.rs1()])
                   >> instr.immI());
                break;
            case FUNCT7_SRAI:
                regs[instr.rd()] = sext<32>(static_cast<i32>(regs[instr.rs1()])
                       >> instr.immI());
                break;
            default:
                throw Exception(EXCEPTION_ILLEGAL_INSTR, instr.raw);
            }
            break;
        default:
            throw Exception(EXCEPTION_ILLEGAL_INSTR, instr.raw);
        }
        break;
    case OPCODE_OP_32:
        switch (instr.funct3()) {
        case OP_ADD_SUB:
            switch (instr.funct7()) {
            case FUNCT7_ADD:
                regs[instr.rd()] = sext<32>(regs[instr.rs1()] + regs[instr.rs2()]);
                break;
            case FUNCT7_SUB:
                regs[instr.rd()] = sext<32>(regs[instr.rs1()] - regs[instr.rs2()]);
                break;
            default:
                throw Exception(EXCEPTION_ILLEGAL_INSTR, instr.raw);
            }
            break;
        case OP_SLL:
            regs[instr.rd()] = sext<32>(static_cast<u32>(regs[instr.rs1()])
                    << regs[instr.rs2()]);
            break;
        case OP_SRL_SRA:
            switch (instr.funct7()) {
            case FUNCT7_SRL:
                regs[instr.rd()] = sext<32>(static_cast<u32>(regs[instr.rs1()])
                        >> regs[instr.rs2()]);
                break;
            case FUNCT7_SRA:
                regs[instr.rd()] = sext<32>(static_cast<i32>(regs[instr.rs1()])
                       >> regs[instr.rs2()]);
                break;
            default:
                throw Exception(EXCEPTION_ILLEGAL_INSTR, instr.raw);
            }
            break;
        default:
            throw Exception(EXCEPTION_ILLEGAL_INSTR, instr.raw);
        }
        break;
    case OPCODE_BRANCH:
        switch (instr.funct3()) {
        case BRANCH_BNE:
            if (regs[instr.rs1()] != regs[instr.rs2()]) {
                if (((pc + instr.immB()) & 3) != 0) {
                    throw Exception(EXCEPTION_INSTR_MISALIGN, pc + instr.immB());
                }
                next_pc = pc + instr.immB();
            }
            break;
        case BRANCH_BEQ:
            if (regs[instr.rs1()] == regs[instr.rs2()]) {
                if (((pc + instr.immB()) & 3) != 0) {
                    throw Exception(EXCEPTION_INSTR_MISALIGN, pc + instr.immB());
                }
                next_pc = pc + instr.immB();
            }
            break;
        case BRANCH_BLT:
            if (static_cast<i64>(regs[instr.rs1()])
                    < static_cast<i64>(regs[instr.rs2()])) {
                if (((pc + instr.immB()) & 3) != 0) {
                    throw Exception(EXCEPTION_INSTR_MISALIGN, pc + instr.immB());
                }
                next_pc = pc + instr.immB();
            }
            break;
        case BRANCH_BGE:
            if (static_cast<i64>(regs[instr.rs1()])
                    >= static_cast<i64>(regs[instr.rs2()])) {
                if (((pc + instr.immB()) & 3) != 0) {
                    throw Exception(EXCEPTION_INSTR_MISALIGN, pc + instr.immB());
                }
                next_pc = pc + instr.immB();
            }
            break;
        case BRANCH_BLTU:
            if (regs[instr.rs1()] < regs[instr.rs2()]) {
                if (((pc + instr.immB()) & 3) != 0) {
                    throw Exception(EXCEPTION_INSTR_MISALIGN, pc + instr.immB());
                }
                next_pc = pc + instr.immB();
            }
            break;
        case BRANCH_BGEU:
            if (regs[instr.rs1()] >= regs[instr.rs2()]) {
                if (((pc + instr.immB()) & 3) != 0) {
                    throw Exception(EXCEPTION_INSTR_MISALIGN, pc + instr.immB());
                }
                next_pc = pc + instr.immB();
            }
            break;
        default:
            throw Exception(EXCEPTION_ILLEGAL_INSTR, instr.raw);
        }
        break;
    case OPCODE_SYSTEM:
        switch (instr.funct3()) {
        case 0b000:
            switch (instr.func12()) {
            case SYSTEM_ECALL: {
                if (csrs.privilege == PRIV_M) {
                    throw Exception(EXCEPTION_M_ECALL, 0);
                } else if (csrs.privilege == PRIV_S) {
                    throw Exception(EXCEPTION_S_ECALL, 0);
                } else if (csrs.privilege == PRIV_U) {
                    throw Exception(EXCEPTION_U_ECALL, 0);
                }
                throw Exception(EXCEPTION_ILLEGAL_INSTR, instr.raw);
                break;
            }
            case SYSTEM_EBREAK:
                break;
            case SYSTEM_MRET:
                trapExit();
                next_pc = pc;
                break;
            default:
                throw Exception(EXCEPTION_ILLEGAL_INSTR, instr.raw);
            }
            break;
        case SYSTEM_CSRRW: {
            try {
                u64 old = (instr.rd() != 0) ? csrs.read(instr.func12()) : 0;
                csrs.write(instr.func12(), regs[instr.rs1()]);
                if (instr.rd() != 0)
                    regs[instr.rd()] = old;
                break;
            } catch (Exception &e) {
                e.val = instr.raw;
                throw e;
            }
        }
        case SYSTEM_CSRRS: {
            try {
                u64 old = csrs.read(instr.func12());
                if (instr.rs1() != 0)
                    csrs.bitset(instr.func12(), regs[instr.rs1()]);
                regs[instr.rd()] = old;
                break;
            } catch (Exception &e) {
                e.val = instr.raw;
                throw e;
            }
        }
        case SYSTEM_CSRRC: {
            try {
                u64 old = csrs.read(instr.func12());
                if (instr.rs1() != 0)
                    csrs.bitclear(instr.func12(), regs[instr.rs1()]);
                regs[instr.rd()] = old;
                break;
            } catch (Exception &e) {
                e.val = instr.raw;
                throw e;
            }
        }
        case SYSTEM_CSRRWI: {
            try {
                u64 old = (instr.rd() != 0) ? csrs.read(instr.func12()) : 0;
                csrs.write(instr.func12(), instr.rs1());
                if (instr.rd() != 0)
                    regs[instr.rd()] = old;
                break;
            } catch (Exception &e) {
                e.val = instr.raw;
                throw e;
            }
        }
        case SYSTEM_CSRRSI: {
            try {
                u64 old = csrs.read(instr.func12());
                if (instr.rs1() != 0)
                    csrs.bitset(instr.func12(), instr.rs1());
                regs[instr.rd()] = old;
                break;
            } catch (Exception &e) {
                e.val = instr.raw;
                throw e;
            }
        }
        case SYSTEM_CSRRCI: {
            try {
                u64 old = csrs.read(instr.func12());
                if (instr.rs1() != 0)
                    csrs.bitclear(instr.func12(), instr.rs1());
                regs[instr.rd()] = old;
                break;
            } catch (Exception &e) {
                e.val = instr.raw;
                throw e;
            }
        }
        default:
            throw Exception(EXCEPTION_ILLEGAL_INSTR, instr.raw);
        }
        break;
    case OPCODE_MISC_MEM:
        // Fence instructions do nothing yet.
        // But they do not throw illegal instruction exceptions
        break;
    default:
        throw Exception(EXCEPTION_ILLEGAL_INSTR, instr.raw);
    }
    return next_pc;
}

void Hart::trapEntry(const Exception &e) {
    // Save PC to MEPC
    csrs.regs[MEPC] = pc;
    // Save cause and interrupt bit to MCAUSE
    csrs.regs[MCAUSE] = (e.code & 0x3f) | (e.interrupt ? (1ull << 63) : 0);
    // Save val to MTVAL
    csrs.regs[MTVAL] = e.val;

    u64 mstatus = csrs.regs[MSTATUS];
    // Save privilege
    mstatus = (mstatus & ~(3ull << 11)) | (csrs.privilege << 11);
    // Save interrupt enable bit
    mstatus = (mstatus & ~(1ull << 7)) | (((mstatus >> 3) & 1) << 7);
    // Disable interrupts
    mstatus &= ~(1ull << 3);
    csrs.regs[MSTATUS] = mstatus;

    csrs.privilege = PRIV_M;
    // Jump to trap handler
    pc = csrs.regs[MTVEC];
}

void Hart::trapExit() {
    // Restore interrupts
    csrs.regs[MSTATUS] &= ~(1ull << 3);
    csrs.regs[MSTATUS] |= ((csrs.regs[MSTATUS] >> 7) & 1) << 3;
    csrs.regs[MSTATUS] |= (1ull << 7);
    // Restore privilege
    csrs.privilege = (csrs.regs[MSTATUS] >> 11) & PRIV_M;
    // Clear MPP
    csrs.regs[MSTATUS] &= ~(PRIV_M << 11);

    // Jump to return address
    pc = csrs.regs[MEPC];
}

void Hart::step() {
    trapped = false;
    try {
        u64 next_pc = execute();
        regs[0] = 0;
        if (!halted)
            pc = next_pc;
    } catch (const Exception& e) {
        trapped = true;
        lastTrap = e;
        trapEntry(e);
    }
}

