#pragma once

#include <cstdint>
#include <string>
#include <vector>

typedef uint64_t ElfAddr;
typedef uint64_t ElfOff;
typedef uint32_t ElfWord;
typedef int32_t ElfSword;
typedef uint64_t ElfXword;
typedef int64_t ElfSxword;
typedef uint16_t ElfHalf;

constexpr ElfHalf ET_EXEC = 2;
constexpr ElfWord PT_LOAD = 1;
constexpr unsigned EI_NIDENT = 16;
constexpr unsigned char ELFCLASS64 = 2;
constexpr unsigned char ELFDATA2LSB = 1;
constexpr ElfHalf EM_RISCV = 243;

struct ElfHeader {
    unsigned char ident[EI_NIDENT];
    ElfHalf type;
    ElfHalf machine;
    ElfWord version;
    ElfAddr entry;
    ElfOff phoff;
    ElfOff shoff;
    ElfWord flags;
    ElfHalf ehsize;
    ElfHalf phentsize;
    ElfHalf phnum;
    ElfHalf shentsize;
    ElfHalf shnum;
    ElfHalf shstrndx;
};

struct ProgramHeader {
    ElfWord type;
    ElfWord flags;
    ElfOff offset;
    ElfAddr vaddr;
    ElfAddr paddr;
    ElfXword filesz;
    ElfXword memsz;
    ElfXword align;
};

struct Segment {
    ProgramHeader header;
    std::vector<uint8_t> data;
};

struct SectionHeader {
    ElfWord name;
    ElfWord type;
    ElfXword flags;
    ElfAddr addr;
    ElfOff offset;
    ElfXword size;
    ElfWord link;
    ElfWord info;
    ElfXword addralign;
    ElfXword entsize;
};

struct Section {
    SectionHeader header;
    std::string name;
    std::vector<uint8_t> data;
};

struct Elf {
    ElfHeader header;
    std::vector<Segment> segments;
    std::vector<Section> sections;
    Section strtab;
    Section shstrtab;

    void printDetails() const;
};

Elf loadElf(const char *path);
