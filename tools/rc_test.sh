#!/bin/bash
# Recompiles nd_asm to C (tools/recomp.py), builds the C for MIPS and checks
# it against the original in the emulator with the tests of src/nd.c.
# usage: tools/rc_test.sh [count] [seed] [all|funcs|fuzz]
# funcs: the generators of tools/nd_difftest.py; fuzz: tools/rc_fuzz.py (random
# states from every label of the segment: count cases, plus the jumps whose
# delay slot loads a register their target reads)
set -e
cd "$(dirname "$0")/.."
PY=.venv/bin/python3
OUT=build/us/rc
mkdir -p $OUT
$PY -c "import unicorn" 2>/dev/null || .venv/bin/pip install -q unicorn
python3 tools/recomp.py asm/us/nd_asm.s $OUT/nd_recomp.c nd 2>$OUT/load_delays.txt
clang --target=mipsel-unknown-none-elf -march=mips1 -mabi=32 -msoft-float -G0 -fno-pic -mno-abicalls \
    -O0 -ffreestanding -fno-strict-aliasing -fno-builtin -DRC_MIPS -DRC_TEST=nd_exec -DRC_RETURN=0x80000100u ${RC_TRACE:+-DRC_TRACE} -Iinclude \
    -Wno-unused-label -c $OUT/nd_recomp.c -o $OUT/nd_recomp.o
DEFS="--defsym rc_gte_op=0x801FF000 --defsym rc_step=0x801FF100"
for s in $(mipsel-linux-gnu-nm -u $OUT/nd_recomp.o | awk '{print $2}'); do
    [ "$s" = rc_gte_op -o "$s" = rc_step ] && continue
    a=$(mipsel-linux-gnu-nm build/us/main.elf | awk -v s=$s '$3==s{print $1}' | head -1)
    DEFS="$DEFS --defsym $s=0x$a"
done
mipsel-linux-gnu-ld -EL -nostdlib -e 0x80200000 -Ttext=0x80200000 $DEFS -o $OUT/nd_recomp.elf $OUT/nd_recomp.o
WHAT=${3:-all}
if [ $WHAT != fuzz ]; then
    RC=1 $PY -u tools/nd_difftest.py build/us/SCUS_942.44 $OUT/nd_recomp.elf "${1:-2000}" "${2:-1}"
fi
if [ $WHAT != funcs ]; then
    $PY -u tools/rc_fuzz.py build/us/SCUS_942.44 $OUT/nd_recomp.elf asm/us/nd_asm.s "${1:-2000}" "${2:-1}"
fi
