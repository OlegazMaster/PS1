#!/usr/bin/env bash
# Installs the decompilation toolchain into tools/toolchain/ and .venv/.
#
# Requirements (Debian/Ubuntu, also inside WSL on Windows):
#   sudo apt install binutils-mipsel-linux-gnu python3-venv git curl make
#
# Everything downloaded here is pinned to an exact release/commit and checked.
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/.." && pwd)
TC="$ROOT/tools/toolchain"

# PSY-Q compatible GCC builds: https://github.com/decompals/old-gcc
OLD_GCC_TAG=0.17
declare -A OLD_GCC_SHA256=(
    [2.7.2-psx]=500a459b3485e885a8d302cac23c2a4632f3900e03a09153f6190699fd723571
    [2.8.1-psx]=f6f6e883942d4d3289d048236c672e71ed410e546aaae8ff655952f1567e1be0
)
# maspsx: makes GCC output assemble like PSY-Q's ASPSX did.
MASPSX_COMMIT=4ffa8b5106f9780d5d4aa7609f184949433ddbbc
# m2c: MIPS -> C decompiler used to produce first drafts of functions.
M2C_COMMIT=c632946b8cfbe8fce442b522dba92dfa72f1f317
# asm-differ: compares our build against the original, function by function.
ASM_DIFFER_COMMIT=0dd09af8f8008f1f880327cf0aca3b26d2562ea2

say() { printf '\033[1;34m==>\033[0m %s\n' "$*"; }
die() { printf '\033[1;31mошибка:\033[0m %s\n' "$*" >&2; exit 1; }

check_deps() {
    local missing=()
    for tool in mipsel-linux-gnu-as mipsel-linux-gnu-ld mipsel-linux-gnu-objcopy \
                mipsel-linux-gnu-objdump python3 git curl make sha256sum cpp; do
        command -v "$tool" >/dev/null || missing+=("$tool")
    done
    if ((${#missing[@]})); then
        die "не найдены: ${missing[*]}
  Debian/Ubuntu/WSL: sudo apt install binutils-mipsel-linux-gnu python3-venv git curl make cpp"
    fi
    [[ $(uname -s) == Linux ]] || die "сборка поддерживается только на Linux (на Windows используйте WSL)"
}

install_gcc() {
    local version=$1 sha=${OLD_GCC_SHA256[$1]}
    local dir="$TC/gcc-$version"
    if [[ -x $dir/cc1 ]]; then return; fi
    say "GCC $version"
    local archive
    archive=$(mktemp)
    curl -sSLf -o "$archive" \
        "https://github.com/decompals/old-gcc/releases/download/$OLD_GCC_TAG/gcc-$version.tar.gz"
    echo "$sha  $archive" | sha256sum -c --quiet - || die "контрольная сумма gcc-$version не совпала"
    mkdir -p "$dir"
    tar -xzf "$archive" -C "$dir"
    rm -f "$archive"
}

install_git() {
    local name=$1 url=$2 commit=$3
    local dir="$TC/$name"
    if [[ -d $dir/.git ]] && [[ $(git -C "$dir" rev-parse HEAD) == "$commit" ]]; then return; fi
    say "$name"
    rm -rf "$dir"
    git init -q "$dir"
    git -C "$dir" fetch -q --depth 1 "$url" "$commit"
    git -C "$dir" checkout -q FETCH_HEAD
}

check_deps
mkdir -p "$TC"
for version in "${!OLD_GCC_SHA256[@]}"; do install_gcc "$version"; done
install_git maspsx https://github.com/mkst/maspsx "$MASPSX_COMMIT"
install_git m2c https://github.com/matt-kempster/m2c "$M2C_COMMIT"
install_git asm-differ https://github.com/simonlindholm/asm-differ "$ASM_DIFFER_COMMIT"

say "Python-окружение (.venv)"
[[ -x $ROOT/.venv/bin/python3 ]] || python3 -m venv "$ROOT/.venv"
"$ROOT/.venv/bin/pip" install -q --upgrade pip
"$ROOT/.venv/bin/pip" install -q -r "$ROOT/requirements.txt"

say "Готово. Дальше: положите исполняемый файл игры в disc/ и выполните 'make init'"
