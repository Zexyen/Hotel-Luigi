# Logo

The Hotel Luigi logo: a front-desk bell whose lid is tipped open, with two eyes looking out from underneath. It uses the launcher's colors (blue #0a86d8, edge #06305c, gold #f5c04a, yellow #ffe55c).

| File | Use |
|---|---|
| `hotel-luigi-mark.svg` | The bell alone, on a transparent background |
| `hotel-luigi-icon.svg` | The app icon: the bell on a night-blue rounded tile |
| `hotel-luigi-lockup.svg` | The bell with the name, for headers and banners |

`launcher/ui/img/icon.png` (256 px) and `launcher/win32/launcher.ico` (16 to 256 px) are exported from `hotel-luigi-icon.svg`. So is `launcher/ui/img/icon.bmp` (128 px, 32 bit), the window's icon. `launcher/ui/img/logo.bmp` (192 px) is `hotel-luigi-mark.svg` as the launcher reads pictures (8 bit, alpha in the palette, premultiplied): the bell beside the name on the title screen.

The letters in the lockup are outlines of Russo One (SIL Open Font License 1.1, `launcher/ui/fonts/OFL-RussoOne.txt`), so the SVG needs no font installed.
