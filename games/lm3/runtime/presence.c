/* Discord Rich Presence for Luigi's Mansion 3: the floor, the room and the money, read from the game every 5 s and sent
 * to a running Discord when they changed. Nothing happens without a Discord application ID (APP_ID, or
 * HL_DISCORD_APP_ID=<id>); HL_DISCORD=0 switches it off. No Discord running: a new try every 30 s.
 *
 * What is read (main 79e5950f, offsets from its base):
 *   - money: UserProfileManager (pointer at +0x19d0940) +0x38f8, the u32 the save writes as mMoneyBalance.
 *   - room: the room Luigi is in (pointer at +0x1c68a90), +0x40 the id of its name in the game's text table (pointer at
 *     +0x1c90180: +0 a header whose +4 is the count, +8 {u32 id, u32 offset} sorted by id, +0x10 UTF-16 text), so the
 *     name comes in the game's language, as the file select shows it.
 *   - floor: the game's own function (+0x8a1c0): 0 = B2, 1 = B1, 2 = 1F ... 16 = 15F, anything else is no floor. It
 *     goes by Luigi's object (+0x356e90 on the pointer at +0x1bd2cd8) without asking whether there is one, so that is
 *     asked here first, as the game's room-name function (+0x10d500) does.
 *
 * The pictures are the application's Rich Presence art assets, uploaded there: "hotel-luigi" (docs/logo/hotel-luigi.png)
 * and one a floor, "floor_b2" ... "floor_15f" (screenshots of the game, not in this repository).
 *
 * Discord's local protocol: a pipe (Windows: \\.\pipe\discord-ipc-N; else a Unix socket discord-ipc-N in the runtime
 * folder) of frames {u32 opcode, u32 length, JSON}: opcode 0 the handshake, 1 a command. Answers are read and dropped. */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "runtime.h"
#include "hostrt.h"
#ifndef _WIN32
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#endif

static const char APP_ID[] = "1571944462840496240";       /* the project's Discord application (discord.com/developers/applications): its name is the "Playing ..." line */

#define G(off) (GUEST_BASE + (uint64_t)(off))
#define OBJECTS G(0x18868e0)
static uint64_t r64(uint64_t a) { return a ? *(uint64_t *)(uintptr_t)a : 0; }
static uint32_t r32(uint64_t a) { return a ? *(uint32_t *)(uintptr_t)a : 0; }
static uint64_t gcall(Ctx *c, uint64_t fn, uint64_t a0) {      /* as debug_menu.c gcall */
    Ctx keep = *c;
    c->x[0] = a0;
    call_guest(c, fn);
    uint64_t r = c->x[0];
    *c = keep;
    return r;
}

/* ---- the game's side ---- */
/* the text behind an id as UTF-8 with JSON's escapes, "" when the table has none */
static void game_text(uint32_t id, char *out, size_t n) {
    uint64_t db = r64(G(0x1c90180)), e = r64(db + 8), pool = r64(db + 0x10);
    uint32_t lo = 0, hi = r32(r64(db) + 4);
    size_t k = 0;
    while (e && lo < hi) { uint32_t m = (lo + hi) / 2; if (r32(e + m * 8ull) < id) lo = m + 1; else hi = m; }
    const uint16_t *s = e && pool && lo < r32(r64(db) + 4) && r32(e + lo * 8ull) == id ? (const uint16_t *)(uintptr_t)(pool + r32(e + lo * 8ull + 4) * 2ull) : NULL;
    for (; s && *s && k + 7 < n; s++) {
        uint32_t u = *s;
        if (u >= 0xd800 && u < 0xdc00 && s[1] >= 0xdc00 && s[1] < 0xe000) u = 0x10000 + ((u - 0xd800) << 10) + (*++s - 0xdc00);
        if (u < 0x20) out[k++] = ' ';
        else if (u == '"' || u == '\\') out[k++] = '\\', out[k++] = (char)u;
        else if (u < 0x80) out[k++] = (char)u;
        else if (u < 0x800) out[k++] = (char)(0xc0 | u >> 6), out[k++] = (char)(0x80 | (u & 0x3f));
        else if (u < 0x10000) out[k++] = (char)(0xe0 | u >> 12), out[k++] = (char)(0x80 | (u >> 6 & 0x3f)), out[k++] = (char)(0x80 | (u & 0x3f));
        else out[k++] = (char)(0xf0 | u >> 18), out[k++] = (char)(0x80 | (u >> 12 & 0x3f)), out[k++] = (char)(0x80 | (u >> 6 & 0x3f)), out[k++] = (char)(0x80 | (u & 0x3f));
    }
    out[k] = 0;
}
/* "details", "state" and "assets" of the activity, as JSON members; in a level: the floor and the room, and the money */
static void describe(Ctx *c, char *out, size_t n) {
    static const char *const FLOOR[] = {"B2", "B1", "1F", "2F", "3F", "4F", "5F", "6F", "7F", "8F", "9F", "10F", "11F", "12F", "13F", "14F", "15F"};
    uint64_t gsm = r64(G(0x1cea920)), room = r64(G(0x1c68a90)), profile = r64(G(0x19d0940)), luigi = r64(G(0x1bd2cd8));
    uint32_t states = r32(gsm);
    if (!states || r64(r64(gsm + 8) + (states - 1) * 8ull) != r64(G(0x1c69280)) || !room || !profile) { snprintf(out, n, "\"details\":\"In the menus\",\"assets\":{\"large_image\":\"hotel-luigi\",\"large_text\":\"Hotel-Luigi\"}"); return; }      /* not GameplayState */
    uint64_t obj = luigi ? r64(OBJECTS + (gcall(c, G(0x356e90), luigi) & 0x1ffff) * 8) : 0;
    uint64_t floor = obj >> 32 && r64(obj + 0xc8) ? gcall(c, G(0x8a1c0), 0) : ~0ull;
    char name[160], money[16], with_commas[24];
    game_text(r32(room + 0x40), name, sizeof name);
    int digits = snprintf(money, sizeof money, "%u", r32(profile + 0x38f8)), k = 0;
    for (int i = 0; i < digits; i++) { if (i && (digits - i) % 3 == 0) with_commas[k++] = ','; with_commas[k++] = money[i]; }
    with_commas[k] = 0;
    const char *f = floor < sizeof FLOOR / sizeof FLOOR[0] ? FLOOR[floor] : "";
    char key[8] = "";                                      /* the floor's picture: floor_b2 ... floor_15f */
    for (int i = 0; f[i]; i++) key[i] = (char)(f[i] | 0x20), key[i + 1] = 0;
    snprintf(out, n, "\"details\":\"%s%s%s\",\"state\":\"Money: %s G\",\"assets\":{\"large_image\":\"%s%s\",\"large_text\":\"%s\",\"small_image\":\"hotel-luigi\",\"small_text\":\"Hotel-Luigi\"}",
             f, *f && *name ? ": " : "", *f || *name ? name : "Somewhere in the hotel", with_commas, *f ? "floor_" : "hotel-luigi", key, *f ? f : "Hotel-Luigi");
}

/* ---- Discord's side ---- */
#ifdef _WIN32
static HANDLE g_pipe = INVALID_HANDLE_VALUE;
static int ipc_open(void) {
    for (int i = 0; i < 10 && g_pipe == INVALID_HANDLE_VALUE; i++) {
        char path[64];
        snprintf(path, sizeof path, "\\\\.\\pipe\\discord-ipc-%d", i);
        g_pipe = CreateFileA(path, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    }
    return g_pipe != INVALID_HANDLE_VALUE;
}
static void ipc_close(void) { if (g_pipe != INVALID_HANDLE_VALUE) CloseHandle(g_pipe); g_pipe = INVALID_HANDLE_VALUE; }
static int ipc_write(const void *p, size_t n) {
    char drop[1024]; DWORD got = 0, have = 0;
    while (PeekNamedPipe(g_pipe, NULL, 0, NULL, &have, NULL) && have && ReadFile(g_pipe, drop, sizeof drop, &got, NULL) && got) ;
    return WriteFile(g_pipe, p, (DWORD)n, &got, NULL) && got == n;
}
#else
static int g_pipe = -1;
static int ipc_open(void) {
    static const char *const sub[] = {"", "app/com.discordapp.Discord/", "snap.discord/"};      /* the package's own, Flatpak's, Snap's */
    const char *dir = getenv("XDG_RUNTIME_DIR");
    for (int i = 0; i < 30 && g_pipe < 0; i++) {
        struct sockaddr_un a = {.sun_family = AF_UNIX};
        snprintf(a.sun_path, sizeof a.sun_path, "%s/%sdiscord-ipc-%d", dir ? dir : "/tmp", sub[i / 10], i % 10);
        int s = socket(AF_UNIX, SOCK_STREAM, 0);
        if (s >= 0 && !connect(s, (struct sockaddr *)&a, sizeof a)) { fcntl(s, F_SETFL, O_NONBLOCK); g_pipe = s; }
        else if (s >= 0) close(s);
    }
    return g_pipe >= 0;
}
static void ipc_close(void) { if (g_pipe >= 0) close(g_pipe); g_pipe = -1; }
static int ipc_write(const void *p, size_t n) {
    char drop[1024];
    while (recv(g_pipe, drop, sizeof drop, 0) > 0) ;
    return send(g_pipe, p, n, MSG_NOSIGNAL) == (ssize_t)n;
}
#endif
static int ipc_frame(uint32_t op, const char *json) {
    char buf[1024];
    uint32_t n = (uint32_t)strlen(json);
    if (n + 8 > sizeof buf) return 0;
    memcpy(buf, &op, 4); memcpy(buf + 4, &n, 4); memcpy(buf + 8, json, n);
    return ipc_write(buf, n + 8);
}

/* at player 1's pad read, on the game's main thread (lm3_imports.c) */
void lm3_presence_tick(Ctx *c) {
    static const char *id;
    static uint64_t next, retry;
    static long long started;
    static char sent[512];
    static unsigned nonce;
    uint64_t now = GetTickCount64();
    if (now < next) return;
    next = now + 5000;
    if (!id) { const char *off = getenv("HL_DISCORD"), *e = getenv("HL_DISCORD_APP_ID"); id = off && !atoi(off) ? "" : e ? e : APP_ID; started = (long long)time(NULL); }
    if (!*id || strspn(id, "0123456789") != strlen(id)) return;
    char what[512], json[900];
    describe(c, what, sizeof what);
#ifdef _WIN32
    int open = g_pipe != INVALID_HANDLE_VALUE;
#else
    int open = g_pipe >= 0;
#endif
    if (!open) {
        if (now < retry) return;
        retry = now + 30000;
        snprintf(json, sizeof json, "{\"v\":1,\"client_id\":\"%s\"}", id);
        if (!ipc_open()) return;
        if (!ipc_frame(0, json)) { ipc_close(); return; }
        printf("lm3: Discord Rich Presence: connected\n");
        sent[0] = 0;
    }
    if (!strcmp(what, sent)) return;
    snprintf(json, sizeof json, "{\"cmd\":\"SET_ACTIVITY\",\"args\":{\"pid\":%u,\"activity\":{%s,\"timestamps\":{\"start\":%lld}}},\"nonce\":\"%u\"}",
             (unsigned)GetCurrentProcessId(), what, started, ++nonce);
    if (ipc_frame(1, json)) snprintf(sent, sizeof sent, "%s", what);
    else ipc_close();                                      /* Discord was closed: connect again in 30 s */
}
