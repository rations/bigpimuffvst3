#!/bin/sh
# BigBubbleMuff — uninstaller (mirror of install.sh).
# Copyright (C) 2026  BigBubbleMuff contributors. GPL-3.0-or-later (see COPYING).
#
# Removes the VST3 plugin, the standalone binary, its .desktop entry and icon.
# As root it cleans the system locations; otherwise the per-user ones.
set -eu

APP_NAME="BigBubbleMuff"
BIN_NAME="bigbubblemuff"

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

remove() {
  if [ -e "$1" ]; then
    rm -rf "$1"
    echo "Removed $1"
  fi
}

remove "$VST3_DIR/$APP_NAME.vst3"
remove "$BIN_DIR/$BIN_NAME"
remove "$DESKTOP_DIR/$BIN_NAME.desktop"
remove "$ICON_DIR/$BIN_NAME.png"

echo "Done."
