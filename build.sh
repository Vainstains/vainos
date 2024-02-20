#! /bin/bash

CUR_DIR=$(pwd)
BIN="$CUR_DIR"/bin/misc
BINFINAL="$CUR_DIR"/bin
echo "Running build script..."

rm -rf bin
mkdir -p bin/misc

cd src || exit

# Compile all .c files in the directories

find "./" -type f \( -name "*.c" \) -print0 |
while IFS= read -r -d '' file; do
    printf "\n================================[ %s ]======================\n\n" "$file"
    if [[ "$file" == *".c" ]]; then
        flat_filename=OUT$(echo "$file" | tr '/' '__' | tr '.' '_')
        i386-elf-gcc -ffreestanding -c "$file" -o "$BIN"/"${flat_filename%.c}.o"
        printf "\n"
        echo " output to file:  $BIN"/"${flat_filename%.c}.o"
    fi
done

# Assemble bootsector
nasm -f bin boot/bootsect.asm -o "$BIN"/bootsect.bin
nasm boot/kernel_entry.asm -f elf -o "$BIN"/kernel_entry.o
# Link and create image binary
printf "\n================================[ Linking ]======================\n\n"
i386-elf-ld -o "$BIN"/kernel.bin -Ttext 0x1000 -e 0x0 "$BIN"/*.o --oformat binary
cat "$BIN"/bootsect.bin "$BIN"/kernel.bin > "$BINFINAL"/os-image.bin

cd "$CUR_DIR" || exit

# Run image
qemu-system-i386 -fda bin/os-image.bin \
                 -D ./log.txt \
                 -d cpu_reset \
                 -no-reboot \
                 -vga std
