#!/usr/bin/env bash
# BigBubbleMuff — build a distributable release tarball (VST3 + Standalone +
# install/uninstall scripts), mirroring the layout users extract and run.
# Copyright (C) 2026  BigBubbleMuff contributors. GPL-3.0-or-later (see COPYING).
set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PKGDIR_SRC="$REPO/packaging"

cmake -B "$REPO/build" -G Ninja -DCMAKE_BUILD_TYPE=Release -S "$REPO"
cmake --build "$REPO/build" --parallel "$(nproc)"

VERSION="$(grep -oP '(?<=project\(BigBubbleMuff VERSION )\S+' "$REPO/CMakeLists.txt")"
ARCH="$(uname -m)"
STAGE="$(mktemp -d)"
PKG="$STAGE/BigBubbleMuff-${VERSION}"
mkdir -p "$PKG"

ART="$REPO/build/BigBubbleMuff_artefacts/Release"

# VST3 bundle — strip the shared object inside the bundle.
BUNDLE="$ART/VST3/BigBubbleMuff.vst3"
find "$BUNDLE" -name '*.so' -exec strip --strip-unneeded {} \;
cp -r "$BUNDLE" "$PKG/"

# Standalone binary (built when FORMATS includes Standalone).
STANDALONE="$ART/Standalone/BigBubbleMuff"
if [ -f "$STANDALONE" ]; then
  strip --strip-unneeded "$STANDALONE"
  cp "$STANDALONE" "$PKG/"
fi

# Installer scripts, desktop entry, icon and licence.
cp "$PKGDIR_SRC/install.sh" "$PKGDIR_SRC/uninstall.sh" "$PKG/"
chmod +x "$PKG/install.sh" "$PKG/uninstall.sh"
cp "$PKGDIR_SRC/bigbubblemuff.desktop" "$PKG/"
cp "$REPO/gui/mufffbase.png" "$PKG/bigbubblemuff.png"
cp "$REPO/COPYING" "$PKG/"

mkdir -p "$REPO/dist"
TARBALL="$REPO/dist/BigBubbleMuff-${VERSION}-linux-${ARCH}.tar.gz"
tar -czf "$TARBALL" -C "$STAGE" "BigBubbleMuff-${VERSION}"
rm -rf "$STAGE"

echo "Packaged: $TARBALL"
echo ""
echo "Contents:"
tar -tzf "$TARBALL"
