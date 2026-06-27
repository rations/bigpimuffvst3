#!/bin/sh
# BigBubbleMuff — distro-agnostic installer for the pre-built release tarball.
# Copyright (C) 2026  BigBubbleMuff contributors. GPL-3.0-or-later (see COPYING).
#
# POSIX sh on purpose: this must run under dash/busybox on minimal and non-systemd
# systems (Devuan/sysvinit included). It installs the VST3 plugin and the
# standalone app, and checks that the required runtime libraries are present —
# mapping any that are missing to the right package for apt, pacman, xbps or dnf.
#
# Usage:
#   ./install.sh            user install (~/.vst3, ~/.local/bin); root → system
#   ./install.sh --yes      don't prompt before installing missing packages
#   ./install.sh --no-deps  skip the runtime-dependency check entirely
set -eu

SELF_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)

APP_NAME="BigBubbleMuff"
BIN_NAME="bigbubblemuff"

VST3_SRC="$SELF_DIR/$APP_NAME.vst3"
STANDALONE_SRC="$SELF_DIR/$APP_NAME"
DESKTOP_SRC="$SELF_DIR/$BIN_NAME.desktop"
ICON_SRC="$SELF_DIR/$BIN_NAME.png"

ASSUME_YES=0
CHECK_DEPS=1
for arg in "$@"; do
  case "$arg" in
    --yes | -y) ASSUME_YES=1 ;;
    --no-deps) CHECK_DEPS=0 ;;
    -h | --help)
      sed -n '2,16p' "$0" | sed 's/^# \{0,1\}//'
      exit 0
      ;;
    *) echo "Unknown option: $arg" >&2; exit 2 ;;
  esac
done

# --- destinations: system-wide as root, otherwise per-user --------------------
if [ "$(id -u)" -eq 0 ]; then
  VST3_DIR="/usr/lib/vst3"
  BIN_DIR="/usr/local/bin"
  DESKTOP_DIR="/usr/share/applications"
  ICON_DIR="/usr/share/pixmaps"
else
  VST3_DIR="$HOME/.vst3"
  BIN_DIR="$HOME/.local/bin"
  DESKTOP_DIR="$HOME/.local/share/applications"
  ICON_DIR="$HOME/.local/share/icons"
fi

# --- runtime dependency check -------------------------------------------------
# Each row: <soname>:<apt>:<pacman>:<xbps>:<dnf>
DEP_TABLE="\
libasound.so.2:libasound2:alsa-lib:alsa-lib:alsa-lib
libfreetype.so.6:libfreetype6:freetype2:freetype:freetype
libfontconfig.so.1:libfontconfig1:fontconfig:fontconfig:fontconfig
libX11.so.6:libx11-6:libx11:libX11:libX11
libXext.so.6:libxext6:libxext:libXext:libXext
libXinerama.so.1:libxinerama1:libxinerama:libXinerama:libXinerama
libXrandr.so.2:libxrandr2:libxrandr:libXrandr:libXrandr
libXcursor.so.1:libxcursor1:libxcursor:libXcursor:libXcursor"

detect_pm() {
  for pm in apt-get pacman xbps-install dnf zypper; do
    if command -v "$pm" >/dev/null 2>&1; then
      echo "$pm"
      return 0
    fi
  done
  echo ""
}

lib_present() {
  if command -v ldconfig >/dev/null 2>&1 && ldconfig -p 2>/dev/null | grep -q "[[:space:]]$1"; then
    return 0
  fi
  for d in /lib /usr/lib /lib64 /usr/lib64 /usr/lib/x86_64-linux-gnu \
           /usr/lib/aarch64-linux-gnu /usr/local/lib; do
    [ -e "$d/$1" ] && return 0
  done
  return 1
}

pm_install_cmd() {
  case "$1" in
    apt-get) echo "sudo apt-get install -y" ;;
    pacman) echo "sudo pacman -S --needed" ;;
    xbps-install) echo "sudo xbps-install -Sy" ;;
    dnf) echo "sudo dnf install -y" ;;
    zypper) echo "sudo zypper install -y" ;;
    *) echo "" ;;
  esac
}

check_dependencies() {
  PM=$(detect_pm)
  missing_libs=""
  missing_pkgs=""

  # Feed the table via a heredoc (not a pipe) so the vars survive the loop.
  OLDIFS=$IFS
  while IFS=: read -r soname p_apt p_pacman p_xbps p_dnf; do
    [ -z "$soname" ] && continue
    if ! lib_present "$soname"; then
      missing_libs="$missing_libs $soname"
      case "$PM" in
        apt-get) missing_pkgs="$missing_pkgs $p_apt" ;;
        pacman) missing_pkgs="$missing_pkgs $p_pacman" ;;
        xbps-install) missing_pkgs="$missing_pkgs $p_xbps" ;;
        dnf | zypper) missing_pkgs="$missing_pkgs $p_dnf" ;;
      esac
    fi
  done <<EOF
$DEP_TABLE
EOF
  IFS=$OLDIFS

  if [ -z "$missing_libs" ]; then
    echo "All runtime libraries are present."
    return 0
  fi

  echo "Missing runtime libraries:$missing_libs" >&2
  if [ -z "$PM" ]; then
    echo "Could not detect a supported package manager (apt/pacman/xbps/dnf)." >&2
    echo "Please install the equivalents of the libraries above, then re-run." >&2
    return 0
  fi

  installer=$(pm_install_cmd "$PM")
  echo "Install them with:" >&2
  echo "    $installer$missing_pkgs" >&2

  if [ "$ASSUME_YES" -ne 1 ]; then
    if [ ! -t 0 ]; then
      echo "(Non-interactive shell; skipping automatic install.)" >&2
      return 0
    fi
    printf "Run this now? [y/N] " >&2
    read -r reply || reply=""
    case "$reply" in
      y | Y | yes | YES) ;;
      *) echo "Skipping dependency install." >&2; return 0 ;;
    esac
  fi

  # shellcheck disable=SC2086
  $installer $missing_pkgs
}

# --- install ------------------------------------------------------------------
if [ "$CHECK_DEPS" -eq 1 ]; then
  check_dependencies || true
fi

if [ ! -d "$VST3_SRC" ]; then
  echo "error: $VST3_SRC not found (run this from the extracted release dir)." >&2
  exit 1
fi

echo "Installing VST3 → $VST3_DIR/"
mkdir -p "$VST3_DIR"
rm -rf "$VST3_DIR/$APP_NAME.vst3"
cp -r "$VST3_SRC" "$VST3_DIR/"

if [ -f "$STANDALONE_SRC" ]; then
  echo "Installing standalone → $BIN_DIR/$BIN_NAME"
  mkdir -p "$BIN_DIR"
  cp "$STANDALONE_SRC" "$BIN_DIR/$BIN_NAME"
  chmod +x "$BIN_DIR/$BIN_NAME"

  if [ -f "$DESKTOP_SRC" ]; then
    mkdir -p "$DESKTOP_DIR" "$ICON_DIR"
    icon_target="$ICON_DIR/$BIN_NAME.png"
    [ -f "$ICON_SRC" ] && cp "$ICON_SRC" "$icon_target"
    sed -e "s|@BIN@|$BIN_DIR/$BIN_NAME|g" \
        -e "s|@ICON@|$icon_target|g" \
        "$DESKTOP_SRC" > "$DESKTOP_DIR/$BIN_NAME.desktop"
  fi

  case ":${PATH}:" in
    *":$BIN_DIR:"*) ;;
    *) echo "note: $BIN_DIR is not on your PATH; add it or launch from the app menu." ;;
  esac
fi

echo "Done. The plugin shows up as '$APP_NAME' in any VST3 host."
