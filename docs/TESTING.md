# Testing the port

What is checked, how, and what counts as a pass. Everything here runs from a built checkout
(`tools/hlbuild/target/release/hlbuild setup --dump <dump>`; after a change to `runtime/` or `games/lm3/runtime/`
use `hlbuild setup --from link`, a plain `setup` does not notice it).

## 1. Without the game (seconds; every change)

| Check | Command | Pass |
|---|---|---|
| Build tool and lifter | `cd tools/hlbuild && cargo test --release` | all pass, except `lift_words_digest` (fails on `main` too; its golden file was not updated with the lifter) |
| Golden lift of the stand-in main | part of the above (`lift_fake_main`) | unchanged, or re-blessed on purpose with `HLBUILD_BLESS=1` after a lifter change |

A change to the lifter that is meant to alter generated code is the only reason to bless.

## 2. Scripted playthrough (5 minutes; every change to the runtime, the lifter or a game option)

`scripts/playtest.sh <name> 300 [graphics.ini key=value ...] [VAR=value ...]`

One hidden run with its own save folder: title, Story, new game, the opening cutscene (A is pressed every 6 s), then
the hotel lobby from about second 210. It writes `build/playtest/<name>/`: `run.log`, a screenshot every 15 s of game
frames and `sheet.png` with all of them, and prints the frame times of the whole run and of the lobby.

A quick variant for the lobby alone: `scripts/playtest.sh <name> 90 from=<earlier run> [...]` loads the save that run
left (title, the news, Story, the file, Load) and is in the lobby from about second 50. It also covers loading a save.

Pass, for every configuration in the matrix:
- no `HOST SIGNAL` / `guest trap` line (the script prints them);
- `sheet.png` ends in the lobby with Luigi on the red carpet, and no picture is black, torn or missing its 3D scene;
- the lobby's median frame time is the target of its `frame_rate` (table below) and its worst second is under twice that.

| Configuration | Arguments | Lobby target |
|---|---|---|
| As made | (none) | 33.4 ms |
| 60 fps | `frame_rate=60` | 16.7 ms |
| 120 fps | `frame_rate=120` | 8.3 ms |
| Ultrawide | `frame_rate=60 aspect_ratio=21:9` | 16.7 ms; the scene shows more at the sides, squeezed in the screenshots (they are the game's 16:9 image) |
| Resolution scale | `frame_rate=60 resolution_scale=2.0` | 16.7 ms; `run.log` has `nvn_vk_scale: internal resolution scale 2.000` |
| Half scale | `resolution_scale=0.5` | 33.4 ms; picture complete, only softer |

Game speed: the lobby must be reached at the same second (within one screenshot, 15 s) at 30, 60 and 120. A frame-rate
change that makes the cutscene finish sooner or later has changed the game's clock.

## 3. Performance (when a change is meant to make it faster, or might make it slower)

1. Baseline first: run the configuration before the change, note the lobby line.
2. `scripts/playtest.sh prof 300 frame_rate=120 WT_LPROF=240,15,$PWD/build/playtest/lprof.txt`, then
   `tools/hlbuild/target/release/hlbuild lprof build/playtest/lprof.txt`: per thread, where the time goes. The first
   thread listed with `render_thread` in its `incl:` list is the renderer; `f_71...` entries are the game's own code.
3. In `run.log`, the last `nvn_vk_perf:` line splits the renderer's frame (batch replay, draw preparation, wait for the
   GPU) and the last `nvn_jobwait:` line shows how long the game waited for it (`Finish`).
4. Run one configuration at a time when measuring: three runs side by side cost each other 10-20 %.

## 4. By hand (before a release; what a script cannot judge)

- A real window: `build/witch-time-game targets/exefs/main.elf` from `build/run` with `WT_ROMFS=$PWD/../../targets/romfs`.
- Controller: move, Poltergust, the menu; rumble.
- Sound in the cutscene and the lobby; no crackle at 60 and 120.
- `window = borderless` on an ultrawide monitor with `aspect_ratio = auto`: the picture fills the screen, the HUD is
  stretched (as with the 21:9 mod), nothing is cut off.
- Quit and start again: the save made by the first run loads, with its picture in the file select.

Measured on 2026-11-12 (RX 9070 XT, Linux), lobby, one run at a time: as made 33.40 ms flat; 60 and 60 + scale 2.0 +
21:9 16.67 ms with a worst second of 16.67; 120: 8.33 ms, worst second 8.34 (120.0 fps, 1 % low 116.7).

## Known gaps (2026-11-12)

- **The save thumbnail is dark at 60 and above.** The picture in the file select is the lobby at about a fifth of its
  brightness (mean 0.06 against 0.34 at 30 fps) once the game's own frame limiter is off; with the limiter left on
  (`LM3_KEEP_LIMITER=1`) it is 0.22. Whatever the game waits for there is not found yet; the save itself is fine.
- **Only the first room is covered.** The script cannot play; rooms with ghosts, the elevator and co-op need section 4.
- The OpenGL fallback renderer is not tested at all.
