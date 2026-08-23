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

