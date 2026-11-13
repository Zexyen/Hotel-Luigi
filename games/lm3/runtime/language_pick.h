/* The nn::settings language codes, and the code nn::oe::GetDesiredLanguage answers for a system language name.
 * In the game's folder: which languages it takes is the game's. Included by runtime/nnmisc.c. */
#ifndef LANGUAGE_PICK_H
#define LANGUAGE_PICK_H
#include <string.h>

/* [doc] nn::settings::Language values and their codes (switchbrew "Settings services"); the
 * order agrees with what nnsdk.c already uses (4 = "it"). LanguageCode is 8 chars, returned in x0. */
static const char *const g_language_codes[] = {"ja", "en-US", "fr", "de", "it", "es", "zh-CN", "ko", "nl", "pt", "ru",
                                               "zh-TW", "en-GB", "fr-CA", "es-419", "zh-Hans", "zh-Hant", "pt-BR"};

/* name = a Windows display language ("en-GB", "de-DE") or WT_LANGUAGE; pick = one of the codes this game takes, else en-US.
 * `supported` is the game's own list (its control data, or what its language tables accept): the stand-in takes these. */
static void language_pick(const char *name, char pick[16]) {
    static const char *const supported[] = {"ja", "en-US", "fr", "de", "it", "es", "zh-Hans", "zh-Hant", "ko", "ru"};
    const size_t n = sizeof supported / sizeof supported[0];
    strcpy(pick, "en-US");
    for (size_t i = 0; i < n; i++) if (!_stricmp(name, supported[i])) { strcpy(pick, supported[i]); return; }
    if (!_strnicmp(name, "zh", 2)) strcpy(pick, strstr(name, "TW") || strstr(name, "HK") || strstr(name, "MO") || strstr(name, "Hant") ? "zh-Hant" : "zh-Hans");
    else {
        char two[3] = {name[0], name[1], 0};
        for (size_t i = 0; i < n; i++) if (!_stricmp(two, supported[i])) strcpy(pick, supported[i]);
    }
}
#endif
