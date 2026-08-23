#include "elf.h"

#include <stdexcept>

int main() {
    Elf elf;
    try {
        // catch invalid path
        elf = loadElf("invalid_path");
        return 1;
    } catch (std::runtime_error e) {
    }
    // should be a known ELF executable
    elf = loadElf("riscv-tests/isa/rv64ui-p-add");
    return 0;
}
