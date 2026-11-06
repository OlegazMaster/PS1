#!/bin/bash
# Checks the functions of src/nd.c (nd_asm rewritten in C) against the
# original assembly in a MIPS emulator. usage: tools/nd_test.sh [count] [seed]
set -e
cd "$(dirname "$0")/.."
T=tools/toolchain
PY=.venv/bin/python3
OUT=build/us/nd_test
mkdir -p $OUT
$PY -c "import unicorn" 2>/dev/null || .venv/bin/pip install -q unicorn
cpp -undef -nostdinc -Iinclude -DVERSION_US -DTARGET_PSX src/nd.c \
    | $T/gcc-2.8.1-psx/cc1 -quiet -O2 -G0 -mips1 -mcpu=3000 -mgas -msoft-float -mno-abicalls -fgnu-linker -funsigned-char \
    | $PY $T/maspsx/maspsx.py --aspsx-version=2.56 --expand-div -G0 \
    | mipsel-linux-gnu-as -EL -Iinclude -march=r3000 -mtune=r3000 -no-pad-sections -G0 -msoft-float -o $OUT/nd.o
DEFS=""
for s in $(mipsel-linux-gnu-nm -u $OUT/nd.o | awk '{print $2}'); do
    a=$(mipsel-linux-gnu-nm build/us/main.elf | awk -v s=$s '$3==s{print $1}' | head -1)
    DEFS="$DEFS --defsym $s=0x$a"
done
mipsel-linux-gnu-ld -EL -nostdlib -e 0x80180000 -Ttext=0x80180000 $DEFS -o $OUT/nd.elf $OUT/nd.o
$PY tools/nd_difftest.py build/us/SCUS_942.44 $OUT/nd.elf "${1:-2000}" "${2:-1}"
