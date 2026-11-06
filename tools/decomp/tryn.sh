#!/bin/bash
# tryn.sh FILE FUNC < code : like tryfn.sh on a clean copy of FILE (git HEAD), ignoring moved %lo() immediates
S="$(cd "$(dirname "$0")" && pwd)"
cd "$(dirname "$0")/../.."
cp "$1" /tmp/tryn_keep.c
if [ -n "$BASE" ]; then cp "$BASE" "$1"; else git show HEAD:"$1" > "$1"; fi
out=$($S/tryfn.sh "$1" "$2")
case "$out" in BUILD*) echo "$out"; exit 1;; esac
sed -i -E 's/(addiu [a-z0-9]+,[a-z0-9]+,)-?[0-9]{4,}$/\1LO/; s/(l[wbh]u? [a-z0-9]+,)-?[0-9]{4,}\(/\1LO(/' /tmp/od_a.txt /tmp/od_b.txt
echo "$2 $(diff /tmp/od_a.txt /tmp/od_b.txt | grep -c '^[<>]')"
