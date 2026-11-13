#!/bin/sh
# playtest.sh <name> <seconds> [key=value ...] [VAR=value ...]
# One hidden, scripted run of the built game from the title into the lobby (docs/TESTING.md), with its own save folder
# and its own graphics.ini. Lower-case key=value pairs are graphics.ini keys (frame_rate=120 aspect_ratio=21:9
# resolution_scale=1.5); upper-case ones are environment variables of the run (WT_LPROF=240,15,lprof.txt).
# Output in build/playtest/<name>/: run.log, shots/frame_<n>.bmp (one every 15 s of game frames, quarter size), sheet.png
# when ImageMagick is installed. Prints the frame times of the whole run and of the lobby (from second 215).
# The script reaches the lobby at about second 210 at 30, 60 and 120 fps; run 300 s to measure there.
# from=<name>: start from the save an earlier run <name> left (its lobby autosave) and load it: the lobby from about
# second 50, so 90 s are enough.
set -eu
repo=$(cd "$(dirname "$0")/.." && pwd)
name=$1; secs=$2; shift 2
out=$repo/build/playtest/$name
rm -rf "$out"; mkdir -p "$out/shots" "$out/user" "$repo/build/run"
cp "$repo/config/graphics.ini" "$out/graphics.ini"
sed -i 's/^frame_time_overlay = .*/frame_time_overlay = on/' "$out/graphics.ini"
rate=30
envs=""
from=""; lobby=215
for a in "$@"; do
    k=${a%%=*}; v=${a#*=}
    case $k in
        from) from=$v; lobby=50; continue ;;
        *[a-z]*) grep -q "^$k = " "$out/graphics.ini" || { echo "playtest: graphics.ini has no key '$k'" >&2; exit 2; }
                 sed -i "s|^$k = .*|$k = $v|" "$out/graphics.ini"; [ "$k" = frame_rate ] && rate=$v ;;
        *) envs="$envs $a" ;;
    esac
done
# A through the title, the file select ("r" moves to Start), the save notice and the cutscene's prompts
script="22:Z 30:Z 37:Z 44:Z 50+0.4:r 53:Z 58:Z 63:Z"
t=70; while [ $t -le 290 ]; do script="$script $t:Z"; t=$((t + 6)); done
if [ -n "$from" ]; then
    [ -d "$repo/build/playtest/$from/user/save" ] || { echo "playtest: run '$from' left no save" >&2; exit 2; }
    cp -r "$repo/build/playtest/$from/user/save" "$out/user/save"
    script=""; t=20; while [ $t -le 75 ]; do script="$script $t:Z"; t=$((t + 5)); done      # title, the news, Story, the file, Load
fi
cd "$repo/build/run"
# shellcheck disable=SC2086
env WT_SCRIPT="$script" WT_SHOT_EVERY=$((rate * 15)) WT_SHOT_SCALE=4 $envs WT_GRAPHICS_CONFIG="$out/graphics.ini" WT_USERDATA="$out/user" \
    WT_SHOW=0 WT_SHOT_DIR="$out/shots" WT_ROMFS="$repo/targets/romfs" \
    timeout -s KILL $((secs + 40)) ../witch-time-game ../../targets/exefs/main.elf "$secs" > "$out/run.log" 2>&1 || true
stats() { sort -n | awk -v w="$1" '{a[NR]=$1} END{if (NR) printf "%s: median %.2f ms, p90 %.2f, worst %.2f (%d s)\n", w, a[int((NR+1)/2)], a[int(NR*9/10)+(NR<10)], a[NR], NR; else print w ": no frames"}'; }
grep "frame-time:" "$out/run.log" | awk '{print $2}' | stats "$name whole run"
grep "frame-time:" "$out/run.log" | awk -v l=$lobby 'NR>=l {print $2}' | stats "$name lobby    "
bad=$(grep -c "HOST SIGNAL\|guest trap\|not translated yet: [1-9]" "$out/run.log" || true)
[ "$bad" = 0 ] || { echo "$name: $bad crash / trap line(s) in $out/run.log"; grep -n "HOST SIGNAL\|guest trap" "$out/run.log" | head -3; }
if command -v magick >/dev/null 2>&1 && ls "$out"/shots/*.bmp >/dev/null 2>&1; then
    # shellcheck disable=SC2046
    (cd "$out/shots" && magick montage $(ls ./*.bmp | sort -t_ -k2 -n) -tile 6x -geometry 320x180+2+2 ../sheet.png) && echo "$name: $out/sheet.png"
fi
