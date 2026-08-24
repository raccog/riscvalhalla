#include "rv64gc.h"

MemRegion::MemRegion(u64 _base, u64 _size) : base{_base}, size{_size} {
    data.resize(size, 0);
}

bool MemRegion::contains(u64 addr) const {
    return addr >= base && addr < (base + size);
}

Memory::Memory() : regions{} {}

u64 MemRegion::fetch(u64 pc) const {
    if (contains(pc)) {
        return data[pc - base];
    }
    throw Exception(EXCEPTION_INSTR_ACCESS);
}

u64 Memory::fetch(u64 pc) const {
    if (pc & 3) {
        throw Exception(EXCEPTION_INSTR_MISALIGN);
    }
    for (const auto &[addr, region] : regions) {
        if (region.contains(pc)) {
            return region.fetch(pc);
        }
    }
    throw Exception(EXCEPTION_INSTR_ACCESS);
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

Hart::Hart() : memory() {}

void Hart::loadElf(const Elf &elf) {
    memory.loadElf(elf);
}

