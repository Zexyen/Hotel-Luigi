//! The player's Nintendo Switch packages, read with their own keys: an .xci (game card) or .nsp (eShop) of the game and
//! the .nsp of its update, unpacked to the exefs/ and romfs/ folders the rest of `setup` takes (hlbuild setup --dump
//! <file> --update <file> --keys <prod.keys>). Only what that needs: PFS0 and HFS0 containers, NCA3 headers (AES-XTS),
//! the key area or a ticket's title key (AES-ECB), sections in AES-CTR, the update's RomFS through its BKTR tables
//! (AES-CTR-EX, and the relocation table onto the game's own RomFS), and the RomFS file tree. Layouts as hactool
//! (github.com/SciresM/hactool) reads them. No keys are shipped or kept: the player's file is read where it is.

use aes::cipher::{generic_array::GenericArray, BlockDecrypt, BlockEncrypt, KeyInit};
use aes::Aes128;
use rayon::prelude::*;
use std::collections::HashMap;
use std::fs::{self, File};
use std::io::Write;
use std::path::Path;
use std::sync::atomic::{AtomicUsize, Ordering};
use std::sync::Arc;

type R<T> = Result<T, String>;

fn u32le(b: &[u8], o: usize) -> u32 {
    u32::from_le_bytes(b[o..o + 4].try_into().unwrap())
}
fn u64le(b: &[u8], o: usize) -> u64 {
    u64::from_le_bytes(b[o..o + 8].try_into().unwrap())
}
fn hex(b: &[u8]) -> String {
    b.iter().map(|x| format!("{:02x}", x)).collect()
}

// ---------------------------------------------------------------------------------------------- keys
/// prod.keys: `name = hex` lines
pub struct Keys(HashMap<String, Vec<u8>>);

impl Keys {
    pub fn read(p: &Path) -> R<Keys> {
        let text = fs::read_to_string(p).map_err(|e| format!("{}: {}", p.display(), e))?;
        let mut m = HashMap::new();
        for l in text.lines() {
            if let Some((k, v)) = l.split_once('=') {
                let v = v.trim();
                if v.len() % 2 == 0 && v.bytes().all(|c| c.is_ascii_hexdigit()) {
                    let b = (0..v.len()).step_by(2).map(|i| u8::from_str_radix(&v[i..i + 2], 16).unwrap()).collect();
                    m.insert(k.trim().to_lowercase(), b);
                }
            }
        }
        if !m.contains_key("header_key") {
            return Err(format!("{}: no header_key in it; give your console's prod.keys", p.display()));
        }
        Ok(Keys(m))
    }
    fn get(&self, name: &str, len: usize) -> R<&[u8]> {
        match self.0.get(name) {
            Some(k) if k.len() == len => Ok(k),
            _ => Err(format!("your keys file has no {} (dumped from an older console firmware?)", name)),
        }
    }
}

// ---------------------------------------------------------------------------------------------- AES
fn aes(key: &[u8]) -> Aes128 {
    Aes128::new(GenericArray::from_slice(key))
}

fn ecb_decrypt(key: &[u8], data: &mut [u8]) {
    let c = aes(key);
    data.chunks_exact_mut(16).for_each(|b| c.decrypt_block(GenericArray::from_mut_slice(b)));
}

/// AES-XTS with the Switch's tweak: the sector number big-endian (the standard one is little-endian)
fn xts_decrypt(key: &[u8], data: &mut [u8], sector_size: usize) {
    let (k1, k2) = (aes(&key[..16]), aes(&key[16..32]));
    for (n, sector) in data.chunks_mut(sector_size).enumerate() {
        let mut t = GenericArray::clone_from_slice(&(n as u128).to_be_bytes());
        k2.encrypt_block(&mut t);
        for b in sector.chunks_exact_mut(16) {
            b.iter_mut().zip(t.iter()).for_each(|(x, y)| *x ^= y);
            k1.decrypt_block(GenericArray::from_mut_slice(b));
            b.iter_mut().zip(t.iter()).for_each(|(x, y)| *x ^= y);
            let mut carry = 0;
            for x in t.iter_mut() {
                let c = *x >> 7;
                *x = (*x << 1) | carry;
                carry = c;
            }
            if carry != 0 {
                t[0] ^= 0x87;
            }
        }
    }
}

// ---------------------------------------------------------------------------------------------- storages
trait Storage: Send + Sync {
    fn read(&self, off: u64, buf: &mut [u8]) -> R<()>;
}

/// a part of the package file, from `base`
struct Slice {
    f: Arc<File>,
    base: u64,
}

impl Storage for Slice {
    fn read(&self, off: u64, mut buf: &mut [u8]) -> R<()> {
        let mut at = self.base + off;
        while !buf.is_empty() {
            #[cfg(unix)]
            let n = std::os::unix::fs::FileExt::read_at(&*self.f, buf, at);
            #[cfg(windows)]
            let n = std::os::windows::fs::FileExt::seek_read(&*self.f, buf, at);
            match n {
                Ok(0) => return Err("the package ends early: an incomplete or damaged file".into()),
                Ok(n) => {
                    buf = &mut buf[n..];
                    at += n as u64;
                }
                Err(e) => return Err(e.to_string()),
            }
        }
        Ok(())
    }
}

/// an NCA section in AES-CTR: the counter is the section's 8 bytes and the offset in the NCA / 16, big-endian. CTR-EX
/// (an update's RomFS): `ex` lists from which section offset the counter's second word is another value.
struct Ctr {
    inner: Slice,
    cipher: Aes128,
    nca_off: u64,
    iv: [u8; 8],
    ex: Vec<(u64, u32)>,
}

impl Storage for Ctr {
    fn read(&self, off: u64, buf: &mut [u8]) -> R<()> {
        let start = off & !15;
        let end = (off + buf.len() as u64 + 15) & !15;
        let mut data = vec![0u8; (end - start) as usize];
        self.inner.read(start, &mut data)?;
        let mut ks: Vec<GenericArray<u8, _>> = Vec::with_capacity(data.len() / 16);
        let mut i = self.ex.partition_point(|e| e.0 <= start);
        for n in 0..data.len() as u64 / 16 {
            let at = start + n * 16;
            while i < self.ex.len() && self.ex[i].0 <= at {
                i += 1;
            }
            let mut b = [0u8; 16];
            b[..8].copy_from_slice(&self.iv);
            if i > 0 {
                b[4..8].copy_from_slice(&self.ex[i - 1].1.to_be_bytes());
            }
            b[8..].copy_from_slice(&((self.nca_off + at) >> 4).to_be_bytes());
            ks.push(GenericArray::from(b));
        }
        self.cipher.encrypt_blocks(&mut ks);
        for (d, k) in data.chunks_exact_mut(16).zip(&ks) {
            d.iter_mut().zip(k.iter()).for_each(|(x, y)| *x ^= y);
        }
        let skip = (off - start) as usize;
        buf.copy_from_slice(&data[skip..skip + buf.len()]);
        Ok(())
    }
}

/// an update's RomFS section as the game sees it: runs of the update's own data and of the game's RomFS section
struct Indirect {
    base: Arc<dyn Storage>,
    patch: Arc<dyn Storage>,
    map: Vec<(u64, u64, bool)>, // virtual offset, physical offset, from the update
    end: u64,
}

impl Storage for Indirect {
    fn read(&self, mut off: u64, mut buf: &mut [u8]) -> R<()> {
        while !buf.is_empty() {
            let i = self.map.partition_point(|e| e.0 <= off);
            if i == 0 || off >= self.end {
                return Err(format!("the update's RomFS has no data at {:#x}", off));
            }
            let (virt, phys, patch) = self.map[i - 1];
            let next = self.map.get(i).map(|e| e.0).unwrap_or(self.end);
            let n = buf.len().min((next - off) as usize);
            let (now, rest) = buf.split_at_mut(n);
            if patch { &self.patch } else { &self.base }.read(phys + (off - virt), now)?;
            buf = rest;
            off += n as u64;
        }
        Ok(())
    }
}

/// a storage seen from `base` on
struct Sub(Arc<dyn Storage>, u64);

impl Storage for Sub {
    fn read(&self, off: u64, buf: &mut [u8]) -> R<()> {
        self.0.read(self.1 + off, buf)
    }
}

// ---------------------------------------------------------------------------------------------- containers
/// a PFS0 (.nsp, an ExeFS) or HFS0 (the partitions of an .xci): name, offset in `s`, size
fn container(s: &dyn Storage, at: u64) -> R<Vec<(String, u64, u64)>> {
    let mut h = [0u8; 16];
    s.read(at, &mut h)?;
    let size = match &h[..4] {
        b"PFS0" => 0x18,
        b"HFS0" => 0x40,
        _ => return Err("not an .nsp or .xci (no PFS0 or HFS0 header)".into()),
    };
    let (n, strings) = (u32le(&h, 4) as usize, u32le(&h, 8) as usize);
    if n > 100_000 || strings > 1 << 24 {
        return Err("a damaged package (its file table is too large)".into());
    }
    let mut t = vec![0u8; n * size + strings];
    s.read(at + 16, &mut t)?;
    let data = at + 16 + t.len() as u64;
    (0..n)
        .map(|i| {
            let e = &t[i * size..];
            let name = &t[n * size + u32le(e, 16) as usize..];
            let name = String::from_utf8_lossy(&name[..name.iter().position(|&c| c == 0).unwrap_or(name.len())]).into_owned();
            Ok((name, data + u64le(e, 0), u64le(e, 8)))
        })
        .collect()
}

/// the files of an .nsp, or of an .xci's secure partition (where its NCAs are), with their offsets in the file
fn package_files(f: &Arc<File>) -> R<Vec<(String, u64, u64)>> {
    let s = Slice { f: f.clone(), base: 0 };
    let mut h = [0u8; 4];
    s.read(0, &mut h)?;
    if &h == b"PFS0" {
        return container(&s, 0);
    }
    for skip in [0, 0x1000] {
        // a card image with its key area in front starts 0x1000 later
        let mut x = [0u8; 0x40];
        s.read(skip + 0x100, &mut x)?;
        if &x[..4] == b"HEAD" {
            let root = skip + u64le(&x, 0x30);
            let secure = container(&s, root)?
                .into_iter()
                .find(|p| p.0 == "secure")
                .ok_or("the .xci has no secure partition")?;
            return container(&s, secure.1);
        }
    }
    Err("not an .nsp or .xci (no PFS0 or HFS0 header)".into())
}

// ---------------------------------------------------------------------------------------------- NCA
struct Nca {
    f: Arc<File>,
    at: u64, // in the package file
    header: Vec<u8>,
    key: [u8; 16], // the sections' AES-CTR key
}

impl Nca {
    fn open(f: &Arc<File>, at: u64, keys: &Keys, tickets: &HashMap<String, Vec<u8>>) -> R<Option<Nca>> {
        let mut header = vec![0u8; 0xC00];
        Slice { f: f.clone(), base: at }.read(0, &mut header)?;
        xts_decrypt(keys.get("header_key", 32)?, &mut header, 0x200);
        if &header[0x200..0x204] != b"NCA3" {
            return Err("an NCA header does not decrypt (another console's keys, or a damaged package)".into());
        }
        if header[0x205] != 0 {
            return Ok(None); // not the program: meta, control, manual or data
        }
        let rev = header[0x206].max(header[0x220]).saturating_sub(1);
        let rights = &header[0x230..0x240];
        let mut key = [0u8; 16];
        if rights.iter().any(|&b| b != 0) {
            let tik = tickets
                .get(&hex(rights))
                .ok_or_else(|| format!("the package has no ticket {}.tik for its title key", hex(rights)))?;
            let body = 4 + match u32le(tik, 0) {
                0x10000 | 0x10003 => 0x200 + 0x3C,
                0x10001 | 0x10004 => 0x100 + 0x3C,
                _ => 0x3C + 0x40,
            };
            if tik.len() < body + 0x150 || tik[body + 0x141] != 0 {
                return Err("the ticket is personalised (tied to one console): give a package with a common ticket".into());
            }
            key.copy_from_slice(&tik[body + 0x40..body + 0x50]);
            ecb_decrypt(keys.get(&format!("titlekek_{:02x}", rev), 16)?, &mut key);
        } else {
            let kind = ["application", "ocean", "system"].get(header[0x207] as usize).ok_or("an unknown key area")?;
            let mut area = header[0x300..0x340].to_vec();
            ecb_decrypt(keys.get(&format!("key_area_key_{}_{:02x}", kind, rev), 16)?, &mut area);
            key.copy_from_slice(&area[0x20..0x30]);
        }
        Ok(Some(Nca { f: f.clone(), at, header, key }))
    }

    fn size(&self) -> u64 {
        u64le(&self.header, 0x208)
    }

    /// section i's header (0x200 bytes) and its storage, decrypted as its header says
    fn section(&self, i: usize, ex: Vec<(u64, u32)>) -> R<(&[u8], Arc<dyn Storage>)> {
        let start = u32le(&self.header, 0x240 + i * 0x10) as u64 * 0x200;
        let h = &self.header[0x400 + i * 0x200..0x600 + i * 0x200];
        let inner = Slice {
            f: self.f.clone(),
            base: self.at + start,
        };
        let s: Arc<dyn Storage> = match h[4] {
            1 => Arc::new(inner),
            3 | 4 => {
                let mut iv = [0u8; 8];
                iv.iter_mut().zip(h[0x140..0x148].iter().rev()).for_each(|(a, b)| *a = *b);
                Arc::new(Ctr {
                    inner,
                    cipher: aes(&self.key),
                    nca_off: start,
                    iv,
                    ex,
                })
            }
            e => return Err(format!("section {} is in an encryption this does not read ({})", i, e)),
        };
        Ok((h, s))
    }

    /// the section of the given kind (0 RomFS, 1 PFS0) that holds the program's ExeFS / RomFS
    fn find(&self, fs_type: u8) -> R<usize> {
        (0..4)
            .filter(|&i| u32le(&self.header, 0x244 + i * 0x10) != 0 && self.header[0x402 + i * 0x200] == fs_type)
            .find(|&i| fs_type == 0 || self.exefs(i).map(|v| v.iter().any(|f| f.0 == "main")).unwrap_or(false))
            .ok_or_else(|| format!("the program has no {}", if fs_type == 0 { "RomFS" } else { "ExeFS" }))
    }

    fn exefs(&self, i: usize) -> R<Vec<(String, u64, u64)>> {
        let (h, s) = self.section(i, vec![])?;
        container(&*s, u64le(h, 0x40)) // the PFS0's offset in the section, after its hash table
    }
}

/// the RomFS level of a section's IVFC header (its last level)
fn romfs_level(h: &[u8]) -> R<u64> {
    if &h[8..12] != b"IVFC" {
        return Err("the RomFS section has no IVFC header".into());
    }
    Ok(u64le(h, 0x18 + 5 * 0x18))
}

/// the BKTR table at section offset `table` (offset, size, its entry size), flattened over its buckets
fn bktr_entries(s: &dyn Storage, h: &[u8], entry: usize) -> R<(Vec<Vec<u8>>, u64)> {
    let (off, size) = (u64le(h, 0), u64le(h, 8));
    if &h[0x10..0x14] != b"BKTR" || size > 1 << 30 {
        return Err("the update's RomFS has no BKTR tables".into());
    }
    let mut t = vec![0u8; size as usize];
    s.read(off, &mut t)?;
    let (buckets, total) = (u32le(&t, 4) as usize, u64le(&t, 8));
    let mut out = Vec::new();
    for b in 0..buckets {
        let k = &t.get(0x4000 + b * 0x4000..0x8000 + b * 0x4000).ok_or("a damaged BKTR table")?;
        let n = u32le(k, 4) as usize;
        out.extend((0..n.min((0x4000 - 0x10) / entry)).map(|i| k[0x10 + i * entry..0x10 + (i + 1) * entry].to_vec()));
    }
    Ok((out, total))
}

/// the program NCA of a package
fn program(path: &Path, keys: &Keys) -> R<Nca> {
    let f = Arc::new(File::open(path).map_err(|e| format!("{}: {}", path.display(), e))?);
    let files = package_files(&f)?;
    let mut tickets = HashMap::new();
    for (n, o, l) in &files {
        if let Some(id) = n.strip_suffix(".tik") {
            let mut t = vec![0u8; (*l).min(0x1000) as usize];
            Slice { f: f.clone(), base: *o }.read(0, &mut t)?;
            tickets.insert(id.to_lowercase(), t);
        }
    }
    let mut best: Option<Nca> = None;
    for (n, o, _) in files.iter().filter(|f| f.0.ends_with(".nca") && !f.0.ends_with(".cnmt.nca")) {
        if let Some(nca) = Nca::open(&f, *o, keys, &tickets).map_err(|e| format!("{}: {}: {}", path.display(), n, e))? {
            if best.as_ref().map_or(true, |b| nca.size() > b.size()) {
                best = Some(nca);
            }
        }
    }
    best.ok_or_else(|| format!("{}: no program in it (an .nsp of a DLC, or not a game?)", path.display()))
}

// ---------------------------------------------------------------------------------------------- RomFS
/// the files of a RomFS: path, offset in `s`, size
fn romfs_files(s: &dyn Storage) -> R<Vec<(String, u64, u64)>> {
    let mut h = [0u8; 0x50];
    s.read(0, &mut h)?;
    if u64le(&h, 0) != 0x50 {
        return Err("the RomFS header is not readable (wrong keys, or the update does not belong to this game)".into());
    }
    let table = |o: usize| -> R<Vec<u8>> {
        let (off, size) = (u64le(&h, o), u64le(&h, o + 8));
        if size > 1 << 28 {
            return Err("a damaged RomFS".into());
        }
        let mut t = vec![0u8; size as usize];
        s.read(off, &mut t)?;
        Ok(t)
    };
    let (dirs, files, data) = (table(0x18)?, table(0x38)?, u64le(&h, 0x48));
    let name = |t: &[u8], e: usize, at: usize| -> R<String> {
        let n = u32le(t, e + at - 4) as usize;
        t.get(e + at..e + at + n)
            .map(|b| String::from_utf8_lossy(b).into_owned())
            .ok_or_else(|| "a damaged RomFS".to_string())
    };
    let mut out = Vec::new();
    let mut todo = vec![(0usize, String::new())];
    let mut seen = 0;
    while let Some((d, path)) = todo.pop() {
        seen += 1;
        if seen > 1_000_000 || d + 0x18 > dirs.len() {
            return Err("a damaged RomFS directory table".into());
        }
        let mut c = u32le(&dirs, d + 8);
        while c != u32::MAX {
            let c_ = c as usize;
            todo.push((c_, format!("{}{}/", path, name(&dirs, c_, 0x18)?)));
            c = u32le(&dirs, c_ + 4);
        }
        let mut f = u32le(&dirs, d + 12);
        while f != u32::MAX {
            let f_ = f as usize;
            if f_ + 0x20 > files.len() || out.len() > 1_000_000 {
                return Err("a damaged RomFS file table".into());
            }
            out.push((
                format!("{}{}", path, name(&files, f_, 0x20)?),
                data + u64le(&files, f_ + 8),
                u64le(&files, f_ + 16),
            ));
            f = u32le(&files, f_ + 4);
        }
    }
    Ok(out)
}

fn write_out(s: &dyn Storage, at: u64, size: u64, to: &Path) -> R<()> {
    if let Some(p) = to.parent() {
        fs::create_dir_all(p).map_err(|e| format!("{}: {}", p.display(), e))?;
    }
    let mut w = std::io::BufWriter::with_capacity(1 << 20, File::create(to).map_err(|e| format!("{}: {}", to.display(), e))?);
    let mut buf = vec![0u8; 8 << 20];
    let mut done = 0;
    while done < size {
        let n = (size - done).min(buf.len() as u64) as usize;
        s.read(at + done, &mut buf[..n])?;
        w.write_all(&buf[..n]).map_err(|e| format!("{}: {}", to.display(), e))?;
        done += n as u64;
    }
    w.flush().map_err(|e| format!("{}: {}", to.display(), e))
}

/// `game` (.xci or .nsp) and its `update` (.nsp) to out/exefs and out/romfs, as an extracted dump has them.
/// say: a line for the log now and then (the RomFS is about 7.5 GB).
pub fn unpack(game: &Path, update: Option<&Path>, keys: &Path, out: &Path, say: &(dyn Fn(String) + Sync)) -> R<()> {
    let keys = Keys::read(keys)?;
    let base = program(game, &keys)?;
    let upd = update.map(|u| program(u, &keys)).transpose()?;
    // the ExeFS: the update's when there is one (an update's program is whole), else the game's
    let exe = upd.as_ref().unwrap_or(&base);
    let i = exe.find(1)?;
    let (_, s) = exe.section(i, vec![])?;
    let pfs = u64le(&exe.header[0x400 + i * 0x200..], 0x40);
    for (n, o, l) in container(&*s, pfs)? {
        write_out(&*s, o, l, &out.join("exefs").join(&n))?;
    }
    // the RomFS: the game's, or the update's view of it
    let b = base.find(0)?;
    let (bh, bs) = base.section(b, vec![])?;
    let romfs: Arc<dyn Storage> = match &upd {
        None => Arc::new(Sub(bs, romfs_level(bh)?)),
        Some(u) => {
            let p = u.find(0)?;
            let (ph, plain) = u.section(p, vec![])?;
            if ph[4] != 4 {
                return Err(format!("{}: not an update of a game (its RomFS has no BKTR patch)", update.unwrap().display()));
            }
            let (subs, _) = bktr_entries(&*plain, &ph[0x120..0x140], 0x10)?;
            let mut ex: Vec<(u64, u32)> = subs.iter().map(|e| (u64le(e, 0), u32le(e, 12))).collect();
            ex.push((u64le(ph, 0x100), u32le(ph, 0x140))); // the tables themselves: the section's own counter
            let (_, patch) = u.section(p, ex)?;
            let (rel, end) = bktr_entries(&*patch, &ph[0x100..0x120], 0x14)?;
            let map = rel.iter().map(|e| (u64le(e, 0), u64le(e, 8), u32le(e, 16) != 0)).collect();
            Arc::new(Sub(Arc::new(Indirect { base: bs, patch, map, end }), romfs_level(ph)?))
        }
    };
    let files = romfs_files(&*romfs)?;
    let total: u64 = files.iter().map(|f| f.2).sum();
    say(format!("  unpacking {} RomFS files, {:.1} GB", files.len(), total as f64 / 1e9));
    let done = AtomicUsize::new(0);
    files.par_iter().try_for_each(|(n, o, l)| -> R<()> {
        write_out(&*romfs, *o, *l, &out.join("romfs").join(n))?;
        let d = done.fetch_add(1, Ordering::Relaxed) + 1;
        if d % 40 == 0 {
            say(format!("  {} of {} RomFS files", d, files.len()));
        }
        Ok(())
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    struct Mem(Vec<u8>);
    impl Storage for Mem {
        fn read(&self, off: u64, buf: &mut [u8]) -> R<()> {
            let o = off as usize;
            buf.copy_from_slice(self.0.get(o..o + buf.len()).ok_or("past the end")?);
            Ok(())
        }
    }

    #[test]
    fn pfs0_and_romfs_tree() {
        // PFS0: two files, "a" (3 bytes) and "bc" (2 bytes)
        let mut p = b"PFS0".to_vec();
        p.extend(2u32.to_le_bytes());
        p.extend(8u32.to_le_bytes());
        p.extend(0u32.to_le_bytes());
        for (off, size, name) in [(0u64, 3u64, 0u32), (3, 2, 2)] {
            p.extend(off.to_le_bytes());
            p.extend(size.to_le_bytes());
            p.extend(name.to_le_bytes());
            p.extend(0u32.to_le_bytes());
        }
        p.extend(b"a\0bc\0\0\0\0xyzuv");
        let v = container(&Mem(p), 0).unwrap();
        assert_eq!(v, vec![("a".into(), 0x10 + 0x30 + 8, 3), ("bc".into(), 0x10 + 0x30 + 8 + 3, 2)]);

        // RomFS: / holds f (4 bytes) and d/, and d/ holds g (2 bytes)
        let entry = |words: &[u32], name: &str| {
            let mut e: Vec<u8> = words.iter().flat_map(|w| w.to_le_bytes()).collect();
            e.extend((name.len() as u32).to_le_bytes());
            e.extend(name.as_bytes());
            while e.len() % 4 != 0 {
                e.push(0);
            }
            e
        };
        let none = u32::MAX;
        let mut dirs = entry(&[0, none, 0x18, 0, 0], ""); // root: child dir at 0x18, child file at 0
        dirs.extend(entry(&[0, none, none, 0x24, 0], "d")); // g follows f, whose name is padded to 4
        let file = |data: u64, size: u64, name: &str| {
            let mut e = [0u32, none].iter().flat_map(|w| w.to_le_bytes()).collect::<Vec<u8>>();
            e.extend(data.to_le_bytes());
            e.extend(size.to_le_bytes());
            e.extend(0u32.to_le_bytes());
            e.extend((name.len() as u32).to_le_bytes());
            e.extend(name.as_bytes());
            while e.len() % 4 != 0 {
                e.push(0);
            }
            e
        };
        let mut files = file(0, 4, "f");
        files.extend(file(4, 2, "g"));
        let (d_at, f_at, data_at) = (0x50u64, 0x50 + dirs.len() as u64, 0x50 + (dirs.len() + files.len()) as u64);
        let mut img = Vec::new();
        for v in [0x50, 0, 0, d_at, dirs.len() as u64, 0, 0, f_at, files.len() as u64, data_at] {
            img.extend((v as u64).to_le_bytes());
        }
        img.extend(&dirs);
        img.extend(&files);
        img.extend(b"ffffgg");
        let mut got = romfs_files(&Mem(img)).unwrap();
        got.sort();
        assert_eq!(got, vec![("d/g".into(), data_at + 4, 2), ("f".into(), data_at, 4)]);
    }

    /// a real package, against an extracted dump of it (diff -r them after):
    /// HL_NX_GAME=<.xci|.nsp> [HL_NX_UPDATE=<.nsp>] HL_NX_KEYS=<prod.keys> HL_NX_OUT=<folder> cargo test --release real_package -- --ignored
    #[test]
    #[ignore]
    fn real_package() {
        let v = |n: &str| std::env::var(n).ok();
        let update = v("HL_NX_UPDATE");
        unpack(
            Path::new(&v("HL_NX_GAME").unwrap()),
            update.as_deref().map(Path::new),
            Path::new(&v("HL_NX_KEYS").unwrap()),
            Path::new(&v("HL_NX_OUT").unwrap()),
            &|l| println!("{}", l),
        )
        .unwrap();
    }

    #[test]
    fn xts_round_trip_with_the_big_endian_tweak() {
        // encrypt two sectors by hand (the definition), then decrypt with xts_decrypt
        let key: Vec<u8> = (0..32).collect();
        let plain: Vec<u8> = (0..0x400).map(|i| (i * 7) as u8).collect();
        let (k1, k2) = (aes(&key[..16]), aes(&key[16..]));
        let mut data = plain.clone();
        for (n, sector) in data.chunks_mut(0x200).enumerate() {
            let mut t = GenericArray::clone_from_slice(&(n as u128).to_be_bytes());
            k2.encrypt_block(&mut t);
            for b in sector.chunks_exact_mut(16) {
                b.iter_mut().zip(t.iter()).for_each(|(x, y)| *x ^= y);
                k1.encrypt_block(GenericArray::from_mut_slice(b));
                b.iter_mut().zip(t.iter()).for_each(|(x, y)| *x ^= y);
                let carry = t[15] >> 7;
                for j in (1..16).rev() {
                    t[j] = (t[j] << 1) | (t[j - 1] >> 7);
                }
                t[0] = (t[0] << 1) ^ if carry != 0 { 0x87 } else { 0 };
            }
        }
        xts_decrypt(&key, &mut data, 0x200);
        assert_eq!(data, plain);
    }
}
