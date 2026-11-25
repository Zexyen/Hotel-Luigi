# Hotel-Luigi

<p align="center"><img src="docs/logo/hotel-luigi-lockup.svg" alt="Hotel Luigi logo" width="440"></p>

<p align="center"><a href="https://recomplauncher.com"><picture><source media="(prefers-color-scheme: dark)" srcset="https://recomplauncher.com/assets/press/launcher/badge-light.svg"><img alt="Get it on RecompLauncher" src="https://recomplauncher.com/assets/press/launcher/badge-dark.svg" height="48"></picture></a></p>

A native PC port of *Luigi's Mansion 3*, built by static recompilation: the Switch binary's ARM64 code is translated to C
on your PC, compiled, and linked against a native runtime (Vulkan 1.3 for the GPU; SDL3 for windowing, audio, and
controllers). It is not an emulator and contains nothing from the game—you build it locally from your own copy.

**Platforms:** Linux (x86-64, glibc 2.35+) and Windows (10/11, x86-64), with a Vulkan 1.3–capable GPU and driver.

## Status

Early / in active development. The game boots to the title screen, menus and the opening cutscene work, the hotel lobby is
playable, and saves write and load cleanly. Automated testing currently covers the title through the lobby
([docs/TESTING.md](docs/TESTING.md)); later floors, ghost fights, and multiplayer modes (co-op and ScreamPark) are still
being tested and fixed.

PC enhancements (configurable in `config/graphics.ini`, `config/controllers.ini`, the launcher, or the in-game settings
menu):

- **High frame rates:** 60 to 120 fps at the game's original speed (`frame_rate`, default `30`), with cutscene timing kept in sync
- **Native aspect ratios:** `auto` (monitor shape), 21:9, 32:9, or custom aspect ratios (`aspect_ratio`, default `16:9`)
- **Internal resolution scaling:** 0.25× to 3.0× (`resolution_scale`, default `1.0`), with dynamic resolution scaling disabled
- **HDR output:** automatic HDR10 / scRGB output following your desktop HDR setting (`hdr`, default `auto`), with configurable white level and peak brightness
- **Controllers and prompts:** auto-switching Xbox, PlayStation, Switch, and keyboard/mouse prompts (`button_prompts`), menu confirm/back swap (`swap_confirm`), rumble and gyro support, and remappable pads and keys
- **In-game settings & performance overlay:** press `F1` (or `Start + Select` on controller 1) to adjust settings in-game or toggle the FPS/frame-time overlay (`frame_time_overlay`)
- **Mods:** emulator-style code patches (`.pchtxt`, `.ips`) applied at build time and data mods (`romfs/` folders) loaded at startup
- **Discord Rich Presence:** shows your current floor, room, and money when Discord is running (`HL_DISCORD=0` disables it; `HL_DISCORD_APP_ID=<id>` uses a custom Discord app)

## What you dump from your own console

You need either your console's packages and keys:

```text
game.xci or game.nsp     the base game (1.0.0 is fine)
update.nsp               the 1.4.0 update (v327680; omit if your package is already 1.4.0)
prod.keys                your console's keys, used to read both
```

or a single folder with the decrypted, extracted contents of your copy:

```text
<dump>/
  exefs/    main, sdk, subsdk1, main.npdm (plus optional rtld)   (decrypted NSOs and NPDM)
  romfs/    *.pkz, movie/, ...                                   (239 files, ~7.5 GB)
```

Only **version 1.4.0** is supported (title `0100DCA0064A6000`, `main` build ID `79e5950ffa85acf6`, nnSdk 7.3.2). The
`verify` step checks ExeFS SHA-256 hashes and the RomFS manifest and reports anything that differs. Packages are
unpacked into `build/dump` first (a few seconds) and then moved into `targets/`; your keys are read in place and never
copied. Compressed packages (`.nsz`, `.xcz`) are not supported—decompress them first.

The game is not distributed here in any form, and will not be. Do not ask for it in issues.

## Play

### Using RecompLauncher (recommended)

[RecompLauncher](https://recomplauncher.com) handles setup end to end: download Hotel Luigi, point it at your copy (an
`.xci`/`.nsp` with update and `prod.keys`, or an extracted dump folder), let it build, and play.

### Using a standalone release

Release archives bundle everything needed to build and play (the launcher, `hlbuild`, the prebuilt runtime, `clang`,
and `lld`), so no compiler or Python installation is required.

1. Download the archive for your OS from the [releases page](https://github.com/Zexyen/Hotel-Luigi/releases):
   - **Windows:** unpack the `.zip` and run `hotel-luigi-game.exe`.
   - **Linux:** unpack the `.tar.gz` and run `./hotel-luigi-game`, or run the `.AppImage` directly.
2. Point the launcher at your extracted dump folder and start the build. The build has five stages (compiling is the
   longest) and is resumable—closing the launcher keeps completed work.
3. Once built, play directly from the launcher (or pass `--play` to skip the launcher screens, e.g. for Steam or
   Playnite shortcuts). Settings, mods, and self-updates are available in the launcher.

> **Building from `.xci` / `.nsp` packages without RecompLauncher:** the launcher's setup screen currently takes an
> extracted dump folder. To build from packages, run the headless prepare step once from a terminal in the release
> folder, then start the launcher as usual:
>
> ```sh
> # Linux (or run the .AppImage with the same flags)
> ./hotel-luigi-game --prepare --dump=game.xci --update=update.nsp --keys=prod.keys
>
> # Windows
> hotel-luigi-game.exe --prepare --dump=game.xci --update=update.nsp --keys=prod.keys
> ```

### Where files are stored

- **Unpacked release folder:** the build (~8–9 GB including game data), `config/`, `mods/`, saves (`build/userdata`),
  caches, and logs (`build/run`) live inside the release folder.
- **Linux `.AppImage`:** when run directly as a single file, writable data is stored in `~/.local/share/hotel-luigi`.
- **Custom location:** pass `--data <folder>` (or set `HL_DATA=<folder>`) every time you start the launcher to keep all
  writable files in that folder instead.

## Build from source

**Requirements:** Rust, `clang` and `lld`, CMake (3.20+), Ninja, and development packages for SDL3 (3.2+), FreeType,
Vulkan headers (1.3+), glslang, FFmpeg, and OpenGL. On Windows, build with `llvm-mingw`.

```sh
# 1. Build the build tool
cargo build --release --manifest-path tools/hlbuild/Cargo.toml

# 2. Build the game from your dump (resumable; --status shows progress)
tools/hlbuild/target/release/hlbuild setup --dump <dump>
# Or from packages:
# tools/hlbuild/target/release/hlbuild setup --dump <game.xci|nsp> --update <update.nsp> --keys <prod.keys>

# 3. Build the launcher (fetches RmlUi 6.3, or pass -DRMLUI_SOURCE_DIR=<path> offline) and start it
cmake -S launcher -B build/launcher -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/launcher
build/hotel-luigi-game
```

`hlbuild setup` runs these steps in order, writing per-step logs to `build/logs/`:

| Step | What it does |
|---|---|
| `deps` | Locates `clang`, `lld`, and required libraries |
| `verify` | Unpacks `.xci`/`.nsp` packages (`nx.rs`) if given, then verifies the dump matches 1.4.0 |
| `convert` | Converts NSOs to ELF and places ExeFS and RomFS under `targets/` |
| `mods` | Patches enabled `.pchtxt` / `.ips` code mods into `targets/exefs/main.elf` |
| `tables` | Extracts runtime tables from your dump's `sdk` and `main` |
| `lift` | Translates ARM64 code to C (`build/gen_eh/`) |
| `compile` | Compiles lifted C files (the longest step) |
| `link` | Links the game library and, in a source checkout, builds the runtime |
| `check` | Reserved / skipped (see `tools/lifter-check/` for the developer Unicorn check) |

Useful `hlbuild setup` options:

- `--from <step>` reruns from `<step>` onward; `--only <step>` runs just that step; `--status` prints completed steps.
- After editing `runtime/` or `games/lm3/runtime/`, run `--from link` (or `hlbuild link`) to rebuild the runtime
  directly—a plain `setup` either reruns from `tables` (for `runtime/` changes) or does not detect `games/lm3/runtime/`
  changes.
- `--move-dump` moves RomFS into `targets/` instead of hard-linking or copying it.
- `clang` is searched in `--llvm <folder>`, `deps/llvm/bin`, `$LLVM_ROOT/bin`, and then `PATH`.

## Layout

| Path | What |
|---|---|
| [`tools/hlbuild/`](tools/hlbuild) | Rust build tool: package unpacking, dump verification, NSO-to-ELF, mod patching, ARM64 lifter, compile, link |
| [`runtime/`](runtime) | Native runtime: OS/SDK services, NVN-to-Vulkan renderer, audio, input, and the in-game PC settings menu |
| [`games/lm3/`](games/lm3) | Game-specific code: supported version hashes, patches, imports, button prompts, debug menu, Discord presence |
| [`launcher/`](launcher) | Desktop launcher (C++, SDL3 + RmlUi) |
| [`config/`](config) | Default `graphics.ini` and `controllers.ini` (every key is documented inline) |
| [`tests/`](tests) | Standalone runtime unit tests (no game dump needed) |
| [`tools/lifter-check/`](tools/lifter-check) | Developer verification of the ARM64 lifter against Unicorn |
| [`scripts/`](scripts) | Headless scripted playtest and Ghidra export scripts |
| [`docs/TESTING.md`](docs/TESTING.md) | Test matrix, debug menu (`LM3_DEBUG=1`), and pass criteria |

## Testing

Without a game dump:

```sh
cargo test --release --manifest-path tools/hlbuild/Cargo.toml
```

Additional standalone C tests in `tests/` and the Unicorn lifter check in `tools/lifter-check/` are listed in
[`.github/workflows/hlbuild.yml`](.github/workflows/hlbuild.yml).

With a built game, `scripts/playtest.sh <name> 300` runs a headless scripted playthrough from the title screen into the
hotel lobby and reports frame times and screenshots. See [docs/TESTING.md](docs/TESTING.md) for the full test matrix and
pass criteria.

## Credits and licence

GPL-3.0 ([`LICENSE`](LICENSE)). The runtime and lifter descend from [ssbu-recomp](https://github.com/Denkishi/ssbu-recomp)
by Denkishi, by way of B2recomp by [K0NL](https://github.com/K0NL) and
[Witch Time](https://github.com/Zexyen/Witch-Time), whose shared base this repository is built on.
[`NOTICE.md`](NOTICE.md) lists copied files, origins, licences, and third-party components.

Not affiliated with or endorsed by Nintendo or Next Level Games. *Luigi's Mansion* is a trademark of Nintendo. This
repository contains no code, data, assets, or keys of theirs.
