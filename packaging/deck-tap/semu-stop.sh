#!/usr/bin/env bash
# semu-stop — cleanly stop a Semu-launched emulator in Steam Game Mode.
#
# THIS IS THE ONLY SANCTIONED WAY TO STOP A GAME. Do NOT pkill -9 broad patterns.
#
# What broke the Deck before (all forbidden):
#   pkill -9 -f org.kde.Sdk   -> SIGKILLs the bwrap sandbox parent -> gamescope holds a dead
#                                child surface -> black screen, wedged input.
#   pkill -9 -f reaper        -> kills Steam's per-game supervisor -> Steam stuck "in game".
#   pkill -f steamwebhelper   -> kills the Game Mode UI renderer -> black during respawn.
#   systemctl restart xdg-desktop-portal-gamescope -> destabilizes the live compositor session.
#
# This script touches ONLY the emulator's own leaf process, by EXACT process name (comm),
# with SIGTERM first (clean shutdown -> the sandbox unwinds itself -> Steam returns to the
# library). SIGKILL is a bounded last resort on that same leaf PID only — never the sandbox,
# runtime, reaper, gamescope, steamwebhelper, or any session/portal service.
#
# Usage: semu-stop [name ...]      (default: retroarch)
#   e.g. semu-stop retroarch       semu-stop pcsx2 ryujinx
set -u

NAMES="${*:-retroarch}"

stop_one() {
    local n="$1" pids i
    pids=$(pgrep -x "$n" 2>/dev/null || true)        # -x: exact comm match (the emulator, NOT bwrap/reaper)
    if [ -z "$pids" ]; then echo "semu-stop: '$n' not running"; return 0; fi
    echo "semu-stop: SIGTERM $n -> $pids"
    # shellcheck disable=SC2086
    kill -TERM $pids 2>/dev/null || true
    for i in $(seq 1 24); do                          # up to 12s for a clean exit
        pids=$(pgrep -x "$n" 2>/dev/null || true)
        if [ -z "$pids" ]; then echo "semu-stop: $n exited cleanly"; return 0; fi
        sleep 0.5
    done
    pids=$(pgrep -x "$n" 2>/dev/null || true)         # last resort: SIGKILL the leaf only
    if [ -n "$pids" ]; then
        echo "semu-stop: $n ignored SIGTERM; SIGKILL leaf only -> $pids"
        # shellcheck disable=SC2086
        kill -KILL $pids 2>/dev/null || true
    fi
}

for n in $NAMES; do stop_one "$n"; done
echo "semu-stop: done (gamescope / steam / reaper / bwrap / portals untouched)"
