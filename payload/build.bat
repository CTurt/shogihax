mips64-elf-gcc stage2.c -Os -G 0 -flto -fomit-frame-pointer -nostartfiles -nostdlib -o stage2.elf -Ttext=0x801bd91c -mtune=vr4300 -march=vr4300 -Wl,-z,max-page-size=0x1
mips64-elf-objcopy -O binary --only-section=.text --only-section=.rodata --only-section=.data stage2.elf stage2.bin -Wl,-z,max-page-size=0x1

cd stage3s/homebrewChannel
mips64-elf-gcc -flto -G 0 ../common/crt0.s homebrewChannel.c -I../common -Os -fomit-frame-pointer -nostartfiles -nostdlib -o homebrewChannel.elf -Ttext=0x80000400 -mtune=vr4300 -march=vr4300 -Wl,-z,max-page-size=0x1
mips64-elf-objcopy -O binary --only-section=.text --only-section=.rodata --only-section=.data homebrewChannel.elf homebrewChannel.bin -Wl,-z,max-page-size=0x1
"../../lzss" -evo homebrewChannel.bin

pause
