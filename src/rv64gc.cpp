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
        throw Exception(EXCEPTION_INSTR_ACCESS);
    }
    // A full instruction is 32 bits, but a compressed one sitting at the very
    // end of a region only has 16 bits behind it, so fetch what is there and
    // let the decoder decide how much of it is an instruction.
    u64 width = std::min<u64>(4, base + size - pc);
    if (width < 2) {
        throw Exception(EXCEPTION_INSTR_ACCESS);
    }
    u32 instr;
    for (u64 i = 0; i < width; ++i) {
        instr |= static_cast<u32>(data[pc - base + i]) << (8 * i);
    }
    return instr;
}

u64 MemRegion::read(u64 addr, u64 len) const {
    if (!contains(addr, len)) {
        throw Exception(EXCEPTION_LOAD_ACCESS);
    }
    u64 value = 0;
    for (u64 i = 0; i < len; ++i) {
        value |= static_cast<u64>(data[addr - base + i]) << (8 * i);
    }
    return value;
}

void MemRegion::write(u64 addr, u64 len, u64 value) {
    if (!contains(addr, len)) {
        throw Exception(EXCEPTION_STORE_ACCESS);
    }
    for (u64 i = 0; i < len; ++i) {
        data[addr - base + i] = static_cast<u8>(value >> (8 * i));
    }
}

u32 Memory::fetch(u64 pc) const {
    // rv64gc includes the C extension, so instructions are 16-bit aligned.
    if (pc & 1) {
        throw Exception(EXCEPTION_INSTR_MISALIGN);
    }
    for (const auto &[addr, region] : regions) {
        if (region.contains(pc)) {
            return region.fetch(pc);
        }
    }
    throw Exception(EXCEPTION_INSTR_ACCESS);
}

u64 Memory::read(u64 addr, u64 len) const {
    for (const auto &[regionBase, region] : regions) {
        if (region.contains(addr)) {
            return region.read(addr, len);
        }
    }
    throw Exception(EXCEPTION_LOAD_ACCESS);
}

void Memory::write(u64 addr, u64 len, u64 value) {
    for (auto &[regionBase, region] : regions) {
        if (region.contains(addr)) {
            region.write(addr, len, value);
            return;
        }
    }
    throw Exception(EXCEPTION_STORE_ACCESS);
}

Exception::Exception(u64 _code, bool _interrupt) : code{_code}, interrupt{_interrupt} {}

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

Hart::Hart() : memory(), pc{0}, regs{} {}

void Hart::loadElf(const Elf &elf) {
    memory.loadElf(elf);
}

void Hart::reset(u64 entry) {
    pc = entry;
    std::fill(std::begin(regs), std::end(regs), 0);
}

void Hart::step() {
    Instr instr = decode();
    (void)instr;
    throw Exception(EXCEPTION_ILLEGAL_INSTR);
}

Instr Hart::decode() {
    u32 raw = memory.fetch(pc);
    Instr instr{raw};
    return instr;
}

u32 Instr::opcode() const {
    return raw & 0x7f;
}

bool Instr::isCompressed() const {
    return (raw & 3);
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

i32 Instr::immI() const {
    u32 imm = (raw >> 20) & 0xfff;
    return sext<12>(imm);
}

i32 Instr::immS() const {
    u32 imm = ((raw >> 25) & 0x7f) << 5
        | ((raw << 7) & 0x1f);
    return sext<12>(imm);
}

i32 Instr::immB() const {
    u32 imm = ((raw >> 31) & 1) << 12
        | ((raw << 7) & 1) << 11
        | ((raw >> 25) & 0x3f) << 5
        | ((raw >> 8) & 0xf) << 1;
    return sext<13>(imm);
}

i32 Instr::immU() const {
    return static_cast<i32>(raw & 0xfffff000);
}

i32 Instr::immJ() const {
    u32 imm = ((raw >> 31) & 1) << 20
        | ((raw << 12) & 0xff) << 12
        | ((raw >> 20) & 1) << 11
        | ((raw >> 21) & 0x3ff) << 1;
    return sext<21>(imm);
}

