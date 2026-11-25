//! [witchtime] where a program's files go (issue 115). Program 0 is Bayonetta 3, as always: targets/, build/, runtime/gen/.
//! Program N of the application (1 = the Old Picture Book's story, the Bayonetta Origins prologue, which the game starts
//! with nn::oe::ExecuteProgram): targets/programN/, build/programN/, build/programN/gen/. The runtime sources, lifter/
//! and prebuilt/ are the checkout's for every program.
//!
//! A data folder (HL_DATA, or --data <folder>; the launcher passes it) takes everything a build writes, so the checkout
//! can be read-only (a mounted AppImage): <data>/targets/, <data>/build/ with the tables in <data>/build/gen/, and
//! <data>/config/ and <data>/mods/ (data_root). Without one, all of it stays in the checkout as before.
//!
//! shortcut: one program per hlbuild process at a time (set_program before the steps); pass it down if two must build
//! at once

use std::path::{Path, PathBuf};
use std::sync::atomic::{AtomicU32, Ordering};
use std::sync::OnceLock;

static PROGRAM: AtomicU32 = AtomicU32::new(0);
static DATA: OnceLock<PathBuf> = OnceLock::new();

/// once, before any step (main.rs): HL_DATA or --data
pub fn set_data(d: PathBuf) {
    let _ = DATA.set(d);
}

/// where the build, the settings and the mods go: the data folder, else the checkout
pub fn data_root(repo: &Path) -> PathBuf {
    DATA.get().cloned().unwrap_or_else(|| repo.to_path_buf())
}

pub fn has_data() -> bool {
    DATA.get().is_some()
}

pub fn set_program(p: u32) {
    PROGRAM.store(p, Ordering::Relaxed)
}

pub fn program() -> u32 {
    PROGRAM.load(Ordering::Relaxed)
}

/// "" for program 0, else "programN"
pub fn sub() -> String {
    sub_of(program())
}

fn sub_of(p: u32) -> String {
    if p == 0 {
        String::new()
    } else {
        format!("program{}", p)
    }
}

/// (targets, build, gen) of program p
pub fn folders(repo: &Path, p: u32) -> (PathBuf, PathBuf, PathBuf) {
    folders_in(repo, DATA.get().map(|d| d.as_path()), p)
}

fn folders_in(repo: &Path, data: Option<&Path>, p: u32) -> (PathBuf, PathBuf, PathBuf) {
    let d = data.unwrap_or(repo);
    if p == 0 {
        let gen = if data.is_some() { d.join("build").join("gen") } else { repo.join("runtime").join("gen") };
        return (d.join("targets"), d.join("build"), gen);
    }
    let b = d.join("build").join(sub_of(p));
    (d.join("targets").join(sub_of(p)), b.clone(), b.join("gen"))
}

/// the game's own folder, games/<name>: its runtime sources, lifter lists and docs
pub fn game(repo: &Path) -> PathBuf {
    repo.join("games").join(crate::game::NAME)
}

pub fn targets(repo: &Path) -> PathBuf {
    folders(repo, program()).0
}

pub fn build(repo: &Path) -> PathBuf {
    folders(repo, program()).1
}

/// the generated tables (runtime/gen for program 0: the folder every older build used)
pub fn gen(repo: &Path) -> PathBuf {
    folders(repo, program()).2
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn program_folders() {
        let r = Path::new("/w");
        assert_eq!(folders(r, 0), ("/w/targets".into(), "/w/build".into(), "/w/runtime/gen".into()));
        assert_eq!(
            folders(r, 1),
            ("/w/targets/program1".into(), "/w/build/program1".into(), "/w/build/program1/gen".into())
        );
        let d = Some(Path::new("/d"));
        assert_eq!(folders_in(r, d, 0), ("/d/targets".into(), "/d/build".into(), "/d/build/gen".into()));
        assert_eq!(folders_in(r, d, 1).1, PathBuf::from("/d/build/program1"));
    }
}
