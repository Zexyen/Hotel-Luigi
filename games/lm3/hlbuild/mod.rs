//! The game this checkout builds: what hlbuild knows about one game only. The dump it accepts, the addresses the lifter
//! patches or sends to host functions (../runtime/game_shim_eh.c), and the runtime sources only this game needs.
//! tools/hlbuild/src/main.rs includes this file as `crate::game`.
//!
//! Luigi's Mansion 3.

use crate::lifter::Image;
use crate::util::{u16_at, u32_at, u64_at};

/// the folder under games/
pub const NAME: &str = "lm3";

/// this game's own runtime sources (../runtime), built into the host runtime after the shared ones
pub const RUNTIME_SOURCES: &[&str] = &["lm3_imports.c", "prompts.c"];

/// data the game library exports to the runtime besides the tables every game has (../runtime/game_shim_eh.c)
pub const EXPORTS: &[&str] = &["game_fsr_w", "game_fsr_h", "game_fsr_phases"];

// ---- the one version this port handles
pub const VERSION: &str = "Luigi's Mansion 3 (0100DCA0064A6000), the build with main build ID 79e5950ffa85acf6 (nnSdk 7.3.2)";
/// the dump's exefs files: name, size, sha256, NSO build id. The first is main
pub const EXEFS: &[(&str, u64, &str, Option<&str>)] = &[
    ("main", 11830677, "0689854bc8af51316daaa1978a8aceecd45bb2ba19a4c39496739201946c6283", Some("79e5950ffa85acf63e28c9aec051eac27d6f7f8d")),
    ("sdk", 5310646, "754cf4b581d749fbe3d457cf66497b141ef994cf592d247919d64a37dcc30d27", Some("fcf93a41b08c5193a1410d262602dc621617c56f")),
    // multimedia.nss (nn::movie): main imports from it
    ("subsdk1", 3558582, "1778b9934561f24cbdae72314add6236645b906a411787f58f7cc328c7648c55", Some("95501e203c3f39918fcc7c25fee7cc84fe494a92")),
    ("rtld", 7435, "4e77d3fb96ed4808d4ede7a8680cdfdbdaf3a85b99f5971ad43cd1bbf5179986", Some("0b30bee8500b0002a171757e16f1e754ba0216a5")),
    ("main.npdm", 1508, "605570894f4d4d9c0491262d14cf91776cb4c0b1ce01b457aa1131f7b04cf1b9", None),
];
/// the files of EXEFS a dump must have
pub const REQUIRED: &[&str] = &["main", "sdk", "subsdk1", "main.npdm"];
/// a few RomFS files checked one by one: path, size, sha256
pub const ROMFS_FILES: &[(&str, u64, &str)] = &[
    ("init.dict", 3203, "d579362a9e6f4fb082f710cbc55c742660507a712af21975fcd07702120c2255"),
    ("flow.data", 7888, "ffe44cd87beb4977bd5423bdd1c449003036c9efbcbb3f917cfb1fc9a39f48bc"),
];
/// the whole RomFS: file count, bytes, sha256 of its manifest (the verify step prints the one it found)
pub const ROMFS_MANIFEST: (usize, u64, &str) = (239, 7551759103, "6852c4dee25d50d2ac582aceed3afda64f038696858965956cb72fe70b98a155");

/// the module path at the start of main's rodata (the second PT_LOAD) names the project
pub fn is_game(img: &[u8]) -> bool {
    let phoff = u64_at(img, 0x20) as usize;
    let ro = (0..u16_at(img, 0x38) as usize).map(|i| phoff + i * 0x38).filter(|&p| u32_at(img, p) == 1).nth(1);
    let Some(p) = ro else { return false };
    let off = u64_at(img, p + 8) as usize;
    let len = (u32_at(img, off + 4) as usize).min(0x200);
    img.get(off + 8..off + 8 + len).map_or(false, |s| s.ends_with(b"\\NeoPlumNX64_dist.nss"))
}

/// game functions whose direct calls (BL) go to a host function instead: (address, name) calls h_<name>(Ctx *c)
pub const HOST_OVERRIDES: &[(i64, &str)] = &[
    // the screen's width / height as a float (reached through a pointer only): graphics.ini aspect_ratio
    (0x7100067be0, "aspect"),
    // FrameTimer::Update (reached through a pointer only): at 60 fps and above the time a hitch takes is still stepped
    (0x710079f320, "frame_timer"),
];

/// instruction patches of main, checked against the words the binary has (im.patches), and its frame counters
pub fn patches(im: &mut Image) -> Result<(), String> {
    if !im.game {
        return Ok(());
    }
    im.fps_counters = counters();
    Ok(())
}

/// ../lifter/fps_counters.txt: the store address of every site of main whose add counts the game time due
pub fn counters() -> std::collections::HashSet<i64> {
    include_str!("../lifter/fps_counters.txt")
        .lines()
        .filter_map(|l| l.split_whitespace().next())
        .filter(|w| !w.starts_with('#'))
        .map(|w| i64::from_str_radix(w, 16).expect("fps_counters.txt: a hex store address"))
        .collect()
}
