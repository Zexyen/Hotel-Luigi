# Hotel-Luigi

A native PC port of Luigi's Mansion 3, made by static recompilation: the Switch program's ARM64 code is translated to C
on your PC, compiled, and linked against a runtime that stands in for the console (Vulkan for the GPU, SDL3 for
window, sound and controllers). It is not an emulator, and it contains nothing of the game: you build it from your own
dump.

Linux and Windows, x86-64.

## Status

Early. It boots to the title, the menus work, the opening cutscene plays, and the first room (the hotel lobby) is
playable; saves are written and load again. Nothing after the lobby has been checked: rooms with ghosts, the elevator
and co-op are untested. `docs/TESTING.md` lists what is checked and the known gaps.

Beyond the console's picture (all off by default, in `config/graphics.ini` or the settings screens):

- 60 to 120 fps at the game's own speed (`frame_rate`)
- 21:9, 32:9 or any other picture shape, drawn natively (`aspect_ratio`)
- internal resolution scale from 0.25 to 3.0 (`resolution_scale`), FSR 3.1 as anti-aliasing or upscaler (`upscaler`)
- highest model detail at any distance (`model_lod`), film grain off (`film_grain`)
- Xbox, PlayStation and keyboard button prompts, remappable pads and keys (`config/controllers.ini`)
- emulator-style code mods (`.pchtxt`), patched in at build time

## What you extract from your own console

One folder with the decrypted, extracted contents of your own copy:

```
<dump>/
  exefs/    main, sdk, subsdk0, rtld, main.npdm    (plain NSO files)
  romfs/    *.pkz, movie/, ...                     (239 files, about 7.5 GB)
```

Only one version is supported: title `0100DCA0064A6000`, the build whose `main` has build ID `79e5950ffa85acf6`
(nnSdk 7.3.2). The first build step checks every file by SHA-256 and says what differs. Encrypted packages (`.nsp`,
`.xci`, `.nca`, `.nsz`, `.xcz`) are refused, and no keys are used or needed.

The game is not distributed here in any form, and will not be. Do not ask for it in issues.

## Play

Start `recomp-launcher` (Windows: `recomp-launcher.exe`), point it at the dump folder, and let it build. The build has
five rows, of which compiling is the long one; it is resumable, so closing the launcher loses nothing finished. The
game then runs in the launcher's own window. Settings, mods and updates are on the launcher's screens.

A release archive brings everything the build needs (the launcher, the build tool, the runtime already built, clang
and lld), so the PC needs no compiler or Python. There is no release yet: until the first one, build from source.

## Build from source

Needs: Rust, clang and lld, CMake and Ninja, and the development files of SDL3 (3.2 or newer), FreeType, Vulkan
headers, glslang, FFmpeg and OpenGL. On Windows the toolchain is llvm-mingw.

```sh
# the build tool
cargo build --release --manifest-path tools/wtbuild/Cargo.toml

# the game, from your dump (resumable; --status shows where it is)
tools/wtbuild/target/release/wtbuild setup --dump <dump>

# the launcher (fetches RmlUi 6.3), then start it
cmake -S launcher -B build/launcher -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/launcher
build/recomp-launcher
```

`wtbuild setup` runs these steps, each with its log in `build/logs/`:

| Step | What it does |
|---|---|
| `deps` | finds the compiler and the libraries |
| `verify` | checks that the dump is the supported version |
| `convert` | NSO files to ELF, the ExeFS and RomFS placed under `targets/` |
| `mods` | patches code mods into `main.elf` |
| `tables` | reads what the runtime needs to know from the dump's SDK |
| `lift` | translates the ARM64 code to C |
| `compile` | compiles it (the longest step) |
| `link` | links the game library and, in a checkout, the runtime |
| `check` | a self-test of the result |

`--from <step>` repeats a step and those after it, `--only <step>` just that one. After a change to `runtime/` or
`games/lm3/runtime/` use `--from link`: a plain `setup` does not notice it.

clang is taken from `$LLVM_ROOT/bin`, then `deps/llvm/bin`, then `PATH`.

## Layout

| Path | What |
|---|---|
| `tools/wtbuild/` | the build tool (Rust): dump check, NSO to ELF, the lifter, compile, link |
| `runtime/` | the console's side on the PC: the OS and SDK services, the NVN to Vulkan renderer, audio, pads, the PC settings menu |
| `games/lm3/` | everything specific to this game: the supported version's hashes, code patches, its runtime files |
| `launcher/` | the launcher (C++, SDL3 + RmlUi) |
| `config/` | default `graphics.ini` and `controllers.ini`; every key is documented in the file |
| `tests/` | tests of runtime parts that need no game |
| `tools/lifter-check/` | a developer's check of the lifter against an ARM64 emulator (unicorn) |
| `scripts/` | the scripted playtest, Ghidra export scripts |
| `docs/TESTING.md` | what to run before a change goes in, and what counts as a pass |

## Testing

`cargo test --release` in `tools/wtbuild` needs no game. `scripts/playtest.sh <name> 300` plays a hidden, scripted run
from the title into the lobby and reports frame times and screenshots. `docs/TESTING.md` has the matrix and the pass
criteria.

## Credits and licence

GPL-3.0 (`LICENSE`). The runtime and the lifter come from [ssbu-recomp](https://github.com/Denkishi/ssbu-recomp) by
Denkishi, by way of B2recomp by [K0NL](https://github.com/K0NL) and
[Witch Time](https://github.com/Zexyen/Witch-Time), whose shared base this repository is built on. `NOTICE.md` lists
every copied file, its origin and licence, and the third-party components.

Not affiliated with or endorsed by Nintendo or Next Level Games. Luigi's Mansion is a trademark of Nintendo. This
repository contains no code, data, assets or keys of theirs.
