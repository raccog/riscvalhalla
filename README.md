# riscvalhalla

## Prerequisites

    riscv64-elf-gcc riscv64-elf-binutils verilator yosys

## Setup

Initialize riscv-tests.

    git submodule update --init --recursive

## Building

    make tests     # build and run the ELF loader test
