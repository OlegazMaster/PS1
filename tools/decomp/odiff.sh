#!/bin/bash
# usage: odiff.sh FUNC  -> original (from disc exe, by nonmatchings size) vs build object
f=$1; cd "$(dirname "$0")/../.."
s=$(find asm/us/nonmatchings asm/us/matchings -name "$f.s" | head -1)
start=$(grep -A3 "glabel $f$" $s | grep -m1 -oE "/\* [0-9A-F]+ [0-9A-F]{8} [0-9A-F]{8} \*/" | awk "{print \$3}")
size=$(grep -m1 "nonmatching $f," $s | grep -oE "0x[0-9A-F]+$")
end=$(printf "0x%X" $((0x$start + size)))
mipsel-linux-gnu-objdump -D -b binary -m mips:3000 -EL --adjust-vma=0x8000F800 --start-address=0x$start --stop-address=$end --no-show-raw-insn disc/SCUS_942.44 | grep -E "^\s*[0-9a-f]+:" | sed -E 's/^\s*[0-9a-f]+:\s*//; s/\s+/ /g; s/0x[0-9a-f]+ <[^>]*>|[0-9a-f]+ <[^>]*>/<tgt>/; s/0x8[0-9a-f]{7}/<tgt>/' > /tmp/od_a.txt
mipsel-linux-gnu-objdump -d --no-show-raw-insn build/us/main.elf | grep -vE "^[0-9a-f]+ <(LM|\$L|L[0-9])" | awk -v f="<$f>:" '$2==f{p=1;next} p&&/^[0-9a-f]+ </{p=0} p' | grep -E "^\s*[0-9a-f]+:" | sed -E 's/^\s*[0-9a-f]+:\s*//; s/\s+/ /g; s/0x[0-9a-f]+ <[^>]*>|[0-9a-f]+ <[^>]*>/<tgt>/; s/0x8[0-9a-f]{7}/<tgt>/' > /tmp/od_b.txt
echo "== $f: original $(wc -l < /tmp/od_a.txt), ours $(wc -l < /tmp/od_b.txt)"; diff /tmp/od_a.txt /tmp/od_b.txt | head -${2:-40}
