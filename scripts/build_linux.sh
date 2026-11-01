#!/usr/bin/env bash
# Configure and build the `space` binary on Linux.
#
# This script installs nothing. Install the build dependencies first
# (see "Building on Linux" in README.md), then run from anywhere:
#
#   ./scripts/build_linux.sh [build-dir]
#
# The build directory defaults to "build". To use Clang:
#
#   CC=clang CXX=clang++ ./scripts/build_linux.sh build-clang
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${1:-build}"

# Accept both relative and absolute build-dir arguments.
case "$build_dir" in
  /*) ;;
  *) build_dir="$repo_root/$build_dir" ;;
esac

cmake -S "$repo_root" -B "$build_dir"
cmake --build "$build_dir" --parallel

echo
echo "Build finished: $build_dir/space"
echo "Run it from the repository root (assets are loaded relative to the"
echo "working directory):"
echo
printf '  cd %q && %q\n' "$repo_root" "$build_dir/space"
