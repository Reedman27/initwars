#!/bin/sh
# Smoke tests for the real binary: CLI, audio fallbacks, and (if available) a real
# Window Maker session on a private virtual X server (Xvfb).
#   make smoke
# Prints PASS / FAIL / SKIP per check. Needs: Xvfb. Optional (all needed for the Window Maker part): wmaker, xdotool, ImageMagick (convert+import), xwininfo, xprop, bc.
# Nothing here uses your real display, audio device, or home directory.

BIN=${BIN:-./build/init-wars}
TMP=$(mktemp -d /tmp/initwars-smoke-XXXXXX)
export INITWARS_DATA_DIR="$TMP/data"
pass=0; fail=0
ok()   { echo "PASS  $1"; pass=$((pass+1)); }
bad()  { echo "FAIL  $1"; fail=$((fail+1)); }
skip() { echo "SKIP  $1"; }
check() { if [ "$1" = 0 ]; then ok "$2"; else bad "$2"; fi; }
cleanup() { [ -n "$WMPID" ] && kill "$WMPID" 2>/dev/null; [ -n "$XPID" ] && kill "$XPID" 2>/dev/null; rm -rf "$TMP"; }
trap cleanup EXIT

[ -x "$BIN" ] || { echo "build first: make"; exit 2; }

# ---- CLI ----
$BIN --version >/dev/null; check $? "--version exits 0"
$BIN --help >/dev/null; check $? "--help exits 0"
$BIN --bogus >/dev/null 2>&1; [ $? = 2 ]; check $? "unknown option exits 2"
$BIN --windowed-size 10x10 >/dev/null 2>&1; [ $? = 2 ]; check $? "too-small --windowed-size rejected"

# ---- X server ----
if ! command -v Xvfb >/dev/null 2>&1; then
    skip "Xvfb not installed: all window/audio checks skipped"; echo "$pass passed, $fail failed"; [ $fail = 0 ]; exit $?
fi
D=:$((90 + $$ % 9))
Xvfb $D -screen 0 1280x800x24 >/dev/null 2>&1 & XPID=$!
sleep 1
export DISPLAY=$D

run_for() {   # run_for SECONDS args... ; echoes exit status of the game after SIGTERM
    secs=$1; shift
    "$BIN" --no-save "$@" >"$TMP/out.log" 2>&1 & p=$!
    sleep "$secs"
    kill -0 $p 2>/dev/null || { wait $p; echo "died:$?"; return; }
    kill -TERM $p; wait $p; echo "exit:$?"
}

# ---- audio paths ----
[ "$(SDL_AUDIODRIVER=dummy run_for 2)" = "exit:0" ]; check $? "starts and quits cleanly (dummy audio driver)"
[ "$(SDL_AUDIODRIVER=nonexistent run_for 2)" = "exit:0" ]; check $? "audio driver unavailable: runs silent, quits cleanly"
grep -q "running silent" "$TMP/out.log"; check $? "audio fallback message printed"
[ "$(run_for 2 --no-audio)" = "exit:0" ]; check $? "--no-audio works"
SDL_AUDIODRIVER=disk SDL_DISKAUDIOFILE="$TMP/audio.raw" run_for 3 >/dev/null
SZ=$(wc -c < "$TMP/audio.raw" 2>/dev/null || echo 0)
[ "$SZ" -gt 1000 ]; check $? "audio device opened and fed samples (disk driver wrote $SZ bytes)"
if command -v python3 >/dev/null 2>&1; then
    python3 -I - "$TMP/audio.raw" <<'PY'
import sys, struct
d = open(sys.argv[1], 'rb').read()
n = len(d) // 2
s = struct.unpack('<%dh' % n, d[:n*2])
nz = sum(1 for x in s if x)
sys.exit(0 if nz > 0 else 1)
PY
    check $? "audio output is not pure silence (menu chime was synthesised)"
fi
mkdir -p "$TMP/assets/sfx"; echo "not a wav" > "$TMP/assets/sfx/shoot.wav"
[ "$(INITWARS_ASSETS=$TMP/assets SDL_AUDIODRIVER=dummy run_for 2)" = "exit:0" ]; check $? "corrupt optional WAV: falls back, no crash"
[ "$(INITWARS_ASSETS=/nonexistent SDL_AUDIODRIVER=dummy run_for 2)" = "exit:0" ]; check $? "missing asset dir: no crash"
[ "$(INITWARS_DATA_DIR=/proc/nope/x SDL_AUDIODRIVER=dummy run_for 2)" = "exit:0" ]; check $? "unwritable score dir: no crash"

# ---- window manager session ----
for t in wmaker xdotool convert import xwininfo xprop bc; do
    command -v $t >/dev/null 2>&1 || { skip "$t missing: Window Maker session checks skipped"; echo "$pass passed, $fail failed"; [ $fail = 0 ]; exit $?; }
done
wmaker --no-dock --no-clip >"$TMP/wm.log" 2>&1 & WMPID=$!
sleep 4
SDL_AUDIODRIVER=dummy "$BIN" --no-save --seed 3 >"$TMP/game.log" 2>&1 & GP=$!
sleep 3
# absolute screen positions of the 26x26 title-bar buttons, left to right
title_buttons() {   # buttons sit in the title bar directly above the client window
    wy=$(xwininfo -id $W | awk '/Absolute upper-left Y/{print $4}')
    xwininfo -root -tree | awk -v ty=$((wy - 26)) '/ 26x26\+/ {n=split($NF,a,"+"); if (n>=3 && a[3]==ty && a[2]>=0 && a[2]<1280) print a[2], a[3]}' | sort -n
}
W=$(xdotool search --name "INIT WARS" | head -1)
[ -n "$W" ]; check $? "game window appears under Window Maker"
xprop -id $W WM_PROTOCOLS | grep -q WM_DELETE_WINDOW; check $? "window accepts WM_DELETE_WINDOW (close button)"
FRAME=$(title_buttons | wc -l)
[ "$FRAME" -ge 2 ]; check $? "window is decorated (title bar with buttons)"
size() { xwininfo -id $W | awk '/Width/{w=$2}/Height/{h=$2}END{print w"x"h}'; }
[ "$(size)" = "800x600" ]; check $? "initial size 800x600"
xdotool windowsize $W 1000 500; sleep 1; [ "$(size)" = "1000x500" ]; check $? "WM resize to 1000x500"
xdotool windowsize $W 320 240; sleep 1; [ "$(size)" = "320x240" ]; check $? "WM resize to minimum 320x240"
xdotool windowsize $W 800 600; sleep 1

# get into a wave: skip boot, Start, select, skip story
xdotool mousemove 640 400 click 1; sleep 0.3
for k in space space Return Return Escape; do xdotool key $k; sleep 0.5; done
sleep 3
lum() { import -window $W png:- 2>/dev/null | convert png:- -colorspace Gray -format "%[fx:mean*1000]" info: 2>/dev/null; }
L1=$(lum)
# minimise via the left title-bar button
BTN=$(title_buttons | head -1)
BX=$(echo $BTN | cut -d' ' -f1); BY=$(echo $BTN | cut -d' ' -f2)
xdotool mousemove $((BX+13)) $((BY+13)) click 1; sleep 1.5
xprop -id $W WM_STATE | grep -q Iconic; check $? "minimise via title-bar button (window iconic)"
kill -0 $GP 2>/dev/null; check $? "game keeps running while minimised"
xdotool windowactivate $W 2>/dev/null; sleep 1.5
xprop -id $W WM_STATE | grep -q Normal; check $? "window restores"
L2=$(lum)
if [ -n "$L1" ] && [ -n "$L2" ]; then
    [ "$(echo "$L2 < $L1 * 0.7" | bc -l 2>/dev/null || echo 1)" = 1 ]; check $? "auto-pause: restored window is dimmed pause screen (mean lum $L1 -> $L2)"
fi
# resume with Esc
xdotool mousemove 640 400 click 1; sleep 0.3
xdotool key Escape; sleep 1
L3=$(lum)
[ "$(echo "$L3 > $L2 * 1.25" | bc -l 2>/dev/null || echo 1)" = 1 ]; check $? "Esc resumes (screen bright again: mean lum $L2 -> $L3)"

# fullscreen round trips must not shrink the window (Window Maker restore quirk)
for i in 1 2 3; do
    xdotool key f; sleep 2; [ "$(size)" = "1280x800" ] || { bad "fullscreen #$i enters (size $(size))"; break; }
    xdotool key f; sleep 2; [ "$(size)" = "800x600" ] || { bad "fullscreen #$i restores 800x600 (size $(size))"; break; }
done
[ "$i" = 3 ] && [ "$(size)" = "800x600" ]; check $? "3 fullscreen round trips (key F) restore exactly 800x600"

# close via the right title-bar button (WM_DELETE_WINDOW -> SDL_QUIT)
BTN=$(title_buttons | tail -1)
BX=$(echo $BTN | cut -d' ' -f1); BY=$(echo $BTN | cut -d' ' -f2)
xdotool mousemove $((BX+13)) $((BY+13)) click 1; sleep 2
kill -0 $GP 2>/dev/null && bad "close button ends the game" || ok "close button ends the game"
wait $GP 2>/dev/null; [ $? = 0 ]; check $? "process exit status 0 after window close"

echo
echo "$pass passed, $fail failed"
[ $fail = 0 ]
