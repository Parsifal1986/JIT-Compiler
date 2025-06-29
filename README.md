# JIT-Compiler

## How to use it?

The JIT compiler is target at RISC V 64 plantform, so you have to use qemu with riscv tool chain to first compile a riscv-64 linux kernel. Then due to that the JIT compiler uses LLVM library, you have to install LLVM library for riscv or manually compile it on riscv-64 linux kernel.

It is tested that there do exist a LLVM package you can install directly by apt on riscv-64 qemu, but you have to build a Debian rootfs first. There is a release version of Debian rootfs you can download on the official site of Debian.

The last thing you have to do is to move the src to a directory on the file system of qemu and then make the project by CMake(You can also download a CMake package by apt as well!) or a makefile written by yourself. Then paste a LLVM IR program to a file and run ./naive_ir_runner <path/to/your/file> to run the JIT compiler!