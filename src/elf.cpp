#include "common.h"
#include "elf.h"

#include <fstream>
#include <iostream>

constexpr unsigned HEADER_SIZE = 64;
constexpr unsigned SHT_STRTAB = 3;

ElfHalf pack2(const uint8_t *value) {
    return static_cast<ElfHalf>(value[0])
        | static_cast<ElfHalf>(value[1] << 8);
}

ElfWord pack4(const uint8_t *value) {
    return static_cast<ElfWord>(value[0])
        | static_cast<ElfWord>(value[1] << 8)
        | static_cast<ElfWord>(value[2] << 16)
        | static_cast<ElfWord>(value[3] << 24);
}

ElfXword pack8(const uint8_t *value) {
    ElfXword lo = pack4(value);
    ElfXword hi = pack4(value + 4);
    return lo | (hi << 32);
}

Elf loadElf(const char *path) {
    std::ifstream file{path, std::ios_base::binary | std::ios_base::ate};
    if (!file) throw std::runtime_error("Failed to open file");

    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);

    if (size < HEADER_SIZE) {
        throw std::runtime_error("Invalid ELF file size");
    }

    std::vector<uint8_t> buf(size);
    if (!file.read(reinterpret_cast<char*>(buf.data()), size)) {
        throw std::runtime_error("Failed to read file");
    }

    Elf elf;
    ElfHeader header;
    std::copy(buf.begin(), buf.begin() + 16, header.ident);
    if (header.ident[0] != 0x7f
            || std::strncmp("ELF", reinterpret_cast<const char *>(&header.ident[1]), 3) != 0) {
        throw std::runtime_error("Invalid ELF header ident");
    }
    if (header.ident[4] != ELFCLASS64) {
        throw std::runtime_error("Not a 64-bit ELF file");
    }
    if (header.ident[5] != ELFDATA2LSB) {
        throw std::runtime_error("Not a little-endian ELF file");
    }

    header.type = pack2(&buf[16]);
    header.machine = pack2(&buf[18]);
    header.version = pack4(&buf[20]);
    header.entry = pack8(&buf[24]);
    header.phoff = pack8(&buf[32]);
    header.shoff = pack8(&buf[40]);
    header.flags = pack4(&buf[48]);
    header.ehsize = pack2(&buf[52]);
    header.phentsize = pack2(&buf[54]);
    header.phnum = pack2(&buf[56]);
    header.shentsize = pack2(&buf[58]);
    header.shnum = pack2(&buf[60]);
    header.shstrndx = pack2(&buf[62]);
    elf.header = header;

    if (header.phoff >= HEADER_SIZE) {
        if ((buf.size() - header.phoff) / header.phentsize < header.phnum) {
            throw std::runtime_error("ELF file has invalid segment count");
        }
        for (int i = 0; i < header.phnum; ++i) {
            ProgramHeader header;
            size_t offset = elf.header.phoff + i * elf.header.phentsize;
            header.type = pack4(&buf[offset]);
            header.flags = pack4(&buf[offset + 4]);
            header.offset = pack8(&buf[offset + 8]);
            header.vaddr = pack8(&buf[offset + 16]);
            header.paddr = pack8(&buf[offset + 24]);
            header.filesz = pack8(&buf[offset + 32]);
            header.memsz = pack8(&buf[offset + 40]);
            header.align = pack8(&buf[offset + 48]);
            Segment segment;
            segment.header = header;
            segment.data = std::vector<uint8_t>((header.memsz > header.filesz) ? header.memsz : header.filesz);
            if (buf.size() - header.offset >= header.filesz)
                std::copy(buf.begin() + header.offset, buf.begin() + header.offset + header.filesz, segment.data.begin());
            elf.segments.push_back(segment);
        }
    }

    if (header.shoff >= HEADER_SIZE) {
        if ((buf.size() - header.shoff) / header.shentsize < header.shnum) {
            throw std::runtime_error("ELF file has invalid section count");
        }
        for (int i = 0; i < header.shnum; ++i) {
            SectionHeader header;
            size_t offset = elf.header.shoff + i * elf.header.shentsize;
            header.name = pack4(&buf[offset]);
            header.type = pack4(&buf[offset + 4]);
            header.flags = pack8(&buf[offset + 8]);
            header.addr = pack8(&buf[offset + 16]);
            header.offset = pack8(&buf[offset + 24]);
            header.size = pack8(&buf[offset + 32]);
            header.link = pack4(&buf[offset + 40]);
            header.info = pack4(&buf[offset + 44]);
            header.addralign = pack8(&buf[offset + 48]);
            header.entsize = pack8(&buf[offset + 56]);
            Section section;
            section.header = header;
            section.data = std::vector<uint8_t>(header.size);
            if (buf.size() - header.offset >= header.size)
                std::copy(buf.begin() + header.offset, buf.begin() + header.offset + header.size, section.data.begin());
            elf.sections.push_back(section);
            if (header.type == SHT_STRTAB) {
                if (i == elf.header.shstrndx) {
                    elf.shstrtab = section;
                } else {
                    elf.strtab = section;
                }
            }
        }
        for (auto &section : elf.sections) {
            section.name = std::string(reinterpret_cast<const char *>(&elf.shstrtab.data[section.header.name]));
        }
    }

    return elf;
}

void Elf::printDetails() const {
    std::cout << "Type: " << header.type << std::endl;
    std::cout << "Machine: " << header.machine << std::endl;
    std::cout << "Version: " << header.version << std::endl;
    std::cout << "Entry: 0x" << std::hex << header.entry << std::endl;
    std::cout << "PhOff: " << std::dec << header.phoff << std::endl;
    std::cout << "ShOff: " << header.shoff << std::endl;
    std::cout << "Flags: " << header.flags << std::endl;
    std::cout << "EhSize: " << header.ehsize << std::endl;
    std::cout << "PhEntSize: " << header.phentsize << std::endl;
    std::cout << "PhNum: " << header.phnum << std::endl;
    std::cout << "ShEntSize: " << header.shentsize << std::endl;
    std::cout << "ShNum: " << header.shnum << std::endl;
    std::cout << "ShStrNdx: " << header.shstrndx << std::endl;

    for (size_t i = 0; i < segments.size(); ++i) {
        const Segment &segment = segments[i];
        std::cout << "Segment " << i << std::endl;
        std::cout << "  Type: " << segment.header.type << std::endl;
        std::cout << "  Offset: " << segment.header.offset << std::endl;
        std::cout << "  Vaddr: 0x" << std::hex << segment.header.vaddr << std::endl;
        std::cout << "  Paddr: 0x" << segment.header.paddr << std::endl;
        std::cout << "  File Size: " << std::dec << segment.header.filesz << std::endl;
        std::cout << "  Mem Size: " << segment.header.memsz << std::endl;
        std::cout << "  Flags: " << segment.header.flags << std::endl;
        std::cout << "  Align: " << segment.header.align << std::endl;
    }

    for (size_t i = 0; i < sections.size(); ++i) {
        const Section &section = sections[i];
        std::cout << "Section " << i << std::endl;
        std::cout << "  Name Idx: " << section.header.name << std::endl;
        std::cout << "  Name: " << section.name << std::endl;
        std::cout << "  Type: " << section.header.type << std::endl;
        std::cout << "  Flags: " << section.header.flags << std::endl;
        std::cout << "  Addr: 0x" << std::hex << section.header.addr << std::endl;
        std::cout << "  Offset: " << std::dec << section.header.offset << std::endl;
        std::cout << "  Size: " << section.header.size << std::endl;
        std::cout << "  Link: " << section.header.link << std::endl;
        std::cout << "  Info: " << section.header.info << std::endl;
        std::cout << "  Addr Align: " << section.header.addralign << std::endl;
        std::cout << "  Entry Size: " << section.header.entsize << std::endl;
    }
}
