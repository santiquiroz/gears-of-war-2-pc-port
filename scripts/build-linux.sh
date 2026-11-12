#!/usr/bin/env bash
# Native Linux (x86_64) build: the ReXGlue SDK from source at the pinned commit with every
# sdk/patches/*.patch applied, codegen from your game/default.xex, then gow2 and a .tar.gz with
# everything it loads beside it. See docs/LINUX.md.
#
#   scripts/build-linux.sh             prepare the SDK if needed, build, package
#   scripts/build-linux.sh --no-package
#
# Environment: CC/CXX pick the compiler (default: the newest clang-NN found, 20 or later);
# GOW2_LINUX_SDK overrides the SDK checkout folder (default out/sdk-linux-src).
set -euo pipefail

root=$(cd "$(dirname "$0")/.." && pwd)
sdk_url=https://github.com/rexglue/rexglue-sdk.git
sdk_commit=923c1a5
sdk=${GOW2_LINUX_SDK:-$root/out/sdk-linux-src}
marker=$sdk.stamp
build=$root/out/linux
package=1
[ "${1:-}" = "--no-package" ] && package=0

fail() { echo "build-linux: $*" >&2; exit 1; }
step() { echo; echo "==> $*"; }

pick_compiler() {
  if [ -n "${CXX:-}" ]; then
    CC=${CC:-${CXX/clang++/clang}}
    return
  fi
  for v in 30 29 28 27 26 25 24 23 22 21 20; do
    if command -v "clang++-$v" >/dev/null && command -v "clang-$v" >/dev/null; then
      CXX=clang++-$v CC=clang-$v
      return
    fi
  done
  command -v clang++ >/dev/null || fail "no clang found; install clang 20 or later (see docs/LINUX.md)"
  CXX=clang++ CC=clang
}

patch_stamp() {
  local stamp=$sdk_commit p
  for p in "$root"/sdk/patches/*.patch; do
    stamp+="|$(basename "$p"):$(sha256sum "$p" | cut -d' ' -f1)"
  done
  echo "$stamp"
}

# Clones once; (re)applies the series whenever the commit or a patch changes.
prepare_sdk() {
  local stamp
  stamp=$(patch_stamp)
  if [ ! -d "$sdk/.git" ]; then
    step "Cloning the ReXGlue SDK ($sdk_commit) into $sdk"
    git clone --filter=blob:none "$sdk_url" "$sdk"
  fi
  if [ -f "$marker" ] && [ "$(cat "$marker")" = "$stamp" ]; then
    echo "SDK source current: $sdk"
    return
  fi
  step "Preparing $sdk ($sdk_commit + $(ls "$root"/sdk/patches/*.patch | wc -l) patches)"
  rm -f "$marker"
  git -C "$sdk" checkout -q -f "$sdk_commit"
  git -C "$sdk" clean -fdq -e out
  git -C "$sdk" submodule update --init --recursive --depth 1 --jobs 8
  local p
  for p in "$root"/sdk/patches/*.patch; do
    echo "  applying $(basename "$p")"
    git -C "$sdk" apply --ignore-whitespace --whitespace=nowarn "$p" || fail "$(basename "$p") does not apply"
  done
  echo "$stamp" > "$marker"
}

common_cmake_args() {
  echo -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_COMPILER="$CC" -DCMAKE_CXX_COMPILER="$CXX" \
    -DCMAKE_C_FLAGS=-march=x86-64-v2 -DCMAKE_CXX_FLAGS=-march=x86-64-v2 \
    -DCMAKE_LINKER_TYPE=LLD
}

# CMakeLists.txt includes generated/rexglue.cmake, which codegen writes: a fresh checkout builds the
# SDK's rexglue tool on its own first and runs codegen once (the build re-runs it when inputs change).
first_codegen() {
  [ -f "$root/generated/rexglue.cmake" ] && return
  local tools=$sdk/out/linux-tools rexglue
  step "Building the rexglue codegen tool in $tools"
  # shellcheck disable=SC2046
  cmake -S "$sdk" -B "$tools" $(common_cmake_args)
  cmake --build "$tools" --target rexglue
  rexglue=$(find "$sdk/out" -type f -name rexglue -perm -u+x -newer "$marker" | head -1)
  [ -n "$rexglue" ] || fail "rexglue was built but not found under $sdk/out"
  step "Running codegen on game/default.xex"
  mkdir -p "$root/out/logs"
  (cd "$root" && "$rexglue" --log-file out/logs/codegen.log codegen gow2_manifest.toml) ||
    fail "codegen failed (see out/logs/codegen.log)"
}

configure_and_build() {
  step "Configuring $build with $CXX"
  # shellcheck disable=SC2046
  cmake -S "$root" -B "$build" $(common_cmake_args) -DREXSDK_DIR="$sdk"
  step "Building gow2 (codegen runs first when generated/ is missing or stale)"
  cmake --build "$build" --target gow2
}

game_version() {
  sed -n 's/^set(GOW2_VERSION "\([^"]*\)".*/\1/p' "$root/CMakeLists.txt"
}

make_package() {
  local name dir
  name=gow2-recomp-$(game_version)-linux-x86_64
  dir=$build/package/$name
  step "Packaging $name"
  rm -rf "$build/package"
  mkdir -p "$dir"
  cp "$build/gow2" "$build"/lib*.so "$dir/"
  cp "$build/gamecontrollerdb.txt" "$build/gow2.default.toml" "$dir/"
  cp "$root/LICENSE" "$root/THIRD_PARTY_NOTICES.md" "$dir/"
  cp "$root/docs/LINUX.md" "$dir/README-LINUX.md"
  install -m 755 "$root/scripts/linux/gow2.sh" "$dir/gow2.sh"
  tar -C "$build/package" -czf "$build/package/$name.tar.gz" "$name"
  (cd "$build/package" && sha256sum "$name.tar.gz")
  echo "Package: $build/package/$name.tar.gz"
}

[ -f "$root/game/default.xex" ] || [ -f "$root/generated/default/sources.cmake" ] ||
  fail "game/default.xex not found: codegen needs your own copy of the game's XEX"
command -v cmake >/dev/null || fail "cmake not found"
command -v ninja >/dev/null || fail "ninja not found"
prepare_sdk
pick_compiler
first_codegen
configure_and_build
[ "$package" = 1 ] && make_package
exit 0
