#!/bin/bash
# Checks src/goolvm.c (the GOOL interpreter in C) against the original
# hand-written interpreter (gool_asm): both run the same random GOOL programs
# in a MIPS emulator (unicorn) and must give the same results.
#
# usage: tools/goolvm_test.sh [count] [seed]   (after a matching 'make')
set -e
cd "$(dirname "$0")/.."
T=tools/toolchain
PY=.venv/bin/python3
OUT=build/us/goolvm_test
mkdir -p $OUT
$PY -c "import unicorn" 2>/dev/null || .venv/bin/pip install -q unicorn
cpp -undef -nostdinc -Iinclude -DVERSION_US -DTARGET_PSX src/goolvm.c \
    | $T/gcc-2.8.1-psx/cc1 -quiet -O2 -G0 -mips1 -mcpu=3000 -mgas -msoft-float -mno-abicalls -fgnu-linker -funsigned-char \
    | $PY $T/maspsx/maspsx.py --aspsx-version=2.56 --expand-div -G0 \
    | mipsel-linux-gnu-as -EL -Iinclude -march=r3000 -mtune=r3000 -no-pad-sections -G0 -msoft-float -o $OUT/goolvm.o
# the symbols it uses, at their addresses in the built executable
DEFS=""
for s in $(mipsel-linux-gnu-nm -u $OUT/goolvm.o | awk '{print $2}'); do
    a=$(mipsel-linux-gnu-nm build/us/main.elf | awk -v s=$s '$3==s{print $1}' | head -1)
    DEFS="$DEFS --defsym $s=0x$a"
done
mipsel-linux-gnu-ld -EL -nostdlib -e func_80038E28 -Ttext=0x80180000 $DEFS -o $OUT/goolvm.elf $OUT/goolvm.o
$PY tools/goolvm_difftest.py build/us/SCUS_942.44 $OUT/goolvm.elf "${1:-500}" "${2:-1}"
