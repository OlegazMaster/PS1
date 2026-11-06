#!/bin/bash
# tryfn.sh FILE FUNC < code : put the function, rebuild, compare with the original
S="$(cd "$(dirname "$0")" && pwd)"
cd "$(dirname "$0")/../.."
python3 $S/putfn.py "$1" "$2" || exit 1
rm -f build/us/main.elf
timeout 300 make build/us/main.elf >/tmp/try_build.log 2>&1
if [ ! -f build/us/main.elf ]; then echo "BUILD FAIL $2"; grep -iE "error" /tmp/try_build.log | head -3; exit 1; fi
$S/odiff.sh $2 0 >/dev/null
if diff -q /tmp/od_a.txt /tmp/od_b.txt >/dev/null; then echo "MATCH $2"; else echo "no $2 ($(diff /tmp/od_a.txt /tmp/od_b.txt | grep -c '^[<>]'))"; fi
