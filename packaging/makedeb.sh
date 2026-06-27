#!/usr/bin/env bash
# BigBubbleMuff — build a Debian package (.deb) with no systemd dependency, so it
# installs cleanly on Devuan/sysvinit as well as Debian/Ubuntu and derivatives.
# Copyright (C) 2026  BigBubbleMuff contributors. GPL-3.0-or-later (see COPYING).
#
# The package ships the VST3 plugin (/usr/lib/vst3), the standalone app
# (/usr/bin/bigbubblemuff), a .desktop entry and icon. It declares only library
# Depends (no Pre-Depends, no services, no systemd units).
set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PKGDIR_SRC="$REPO/packaging"
MAINTAINER="${MAINTAINER:-rations <rations@users.noreply.github.com>}"

cmake -B "$REPO/build" -G Ninja -DCMAKE_BUILD_TYPE=Release -S "$REPO"
cmake --build "$REPO/build" --parallel "$(nproc)"

VERSION="$(grep -oP '(?<=project\(BigBubbleMuff VERSION )\S+' "$REPO/CMakeLists.txt")"
case "$(uname -m)" in
  x86_64) ARCH=amd64 ;;
  aarch64) ARCH=arm64 ;;
  armv7l) ARCH=armhf ;;
  *) ARCH="$(uname -m)" ;;
esac

ART="$REPO/build/BigBubbleMuff_artefacts/Release"
DEB="$(mktemp -d)/bigbubblemuff_${VERSION}_${ARCH}"
mkdir -p "$DEB/DEBIAN" \
         "$DEB/usr/lib/vst3" \
         "$DEB/usr/bin" \
         "$DEB/usr/share/applications" \
         "$DEB/usr/share/pixmaps" \
         "$DEB/usr/share/doc/bigbubblemuff"

# Payload --------------------------------------------------------------------
cp -r "$ART/VST3/BigBubbleMuff.vst3" "$DEB/usr/lib/vst3/"
find "$DEB/usr/lib/vst3/BigBubbleMuff.vst3" -name '*.so' -exec strip --strip-unneeded {} \;

if [ -f "$ART/Standalone/BigBubbleMuff" ]; then
  install -m 0755 "$ART/Standalone/BigBubbleMuff" "$DEB/usr/bin/bigbubblemuff"
  strip --strip-unneeded "$DEB/usr/bin/bigbubblemuff"
fi

cp "$REPO/gui/mufffbase.png" "$DEB/usr/share/pixmaps/bigbubblemuff.png"
sed -e "s|@BIN@|/usr/bin/bigbubblemuff|g" \
    -e "s|@ICON@|bigbubblemuff|g" \
    "$PKGDIR_SRC/bigbubblemuff.desktop" > "$DEB/usr/share/applications/bigbubblemuff.desktop"
cp "$REPO/COPYING" "$DEB/usr/share/doc/bigbubblemuff/copyright"

# Control metadata -----------------------------------------------------------
INSTALLED_KB="$(du -sk "$DEB" | cut -f1)"
cat > "$DEB/DEBIAN/control" <<EOF
Package: bigbubblemuff
Version: ${VERSION}
Section: sound
Priority: optional
Architecture: ${ARCH}
Maintainer: ${MAINTAINER}
Installed-Size: ${INSTALLED_KB}
Depends: libc6, libgcc-s1, libstdc++6, libasound2, libfreetype6, libfontconfig1, libx11-6, libxext6, libxinerama1, libxrandr2, libxcursor1
Description: Big Muff Pi fuzz/distortion — VST3 plugin and standalone app
 Native Linux emulation of the Russian "Bubble Font" Big Muff Pi, built on a
 full Wave Digital Filter circuit model derived from the schematic netlist.
 Installs a VST3 plugin and a standalone JACK/ALSA application. Audio-only,
 fully offline, with no systemd dependency.
EOF

# Refresh the desktop database after install/remove when the tool exists. POSIX
# sh, no systemd, safe on sysvinit systems.
cat > "$DEB/DEBIAN/postinst" <<'EOF'
#!/bin/sh
set -e
if command -v update-desktop-database >/dev/null 2>&1; then
  update-desktop-database -q /usr/share/applications || true
fi
exit 0
EOF
cp "$DEB/DEBIAN/postinst" "$DEB/DEBIAN/postrm"
chmod 0755 "$DEB/DEBIAN/postinst" "$DEB/DEBIAN/postrm"

# Build ----------------------------------------------------------------------
mkdir -p "$REPO/dist"
OUT="$REPO/dist/bigbubblemuff_${VERSION}_${ARCH}.deb"
if dpkg-deb --root-owner-group --build "$DEB" "$OUT" 2>/dev/null; then
  :
else
  fakeroot dpkg-deb --build "$DEB" "$OUT"
fi
rm -rf "$(dirname "$DEB")"

echo "Built: $OUT"
echo ""
dpkg-deb -I "$OUT"
echo "Contents:"
dpkg-deb -c "$OUT"
