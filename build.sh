clang --target=riscv32 -march=rv32im -nostdlib -O3 chris.c -o chris.elf \
&& echo "done compiling"