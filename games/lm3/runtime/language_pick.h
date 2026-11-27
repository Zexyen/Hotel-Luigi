/* The nn::settings language codes, and the code nn::oe::GetDesiredLanguage answers for a system language name.
 * In the game's folder: which languages it takes is the game's. Included by runtime/nnmisc.c. */
#ifndef LANGUAGE_PICK_H
#define LANGUAGE_PICK_H
#include <ctype.h>
#include <string.h>

/* [doc] nn::settings::Language values and their codes (switchbrew "Settings services"); the
 * order agrees with what nnsdk.c already uses (4 = "it"). LanguageCode is 8 chars, returned in x0. */
static const char *const g_language_codes[] = {"ja", "en-US", "fr", "de", "it", "es", "zh-CN", "ko", "nl", "pt", "ru",
                                               "zh-TW", "en-GB", "fr-CA", "es-419", "zh-Hans", "zh-Hant", "pt-BR"};

static int lang_ieq(const char *a, const char *b) {
    for (; *a && *b; a++, b++) {
        int ca = *a == '_' ? '-' : tolower((unsigned char)*a);
        int cb = *b == '_' ? '-' : tolower((unsigned char)*b);
        if (ca != cb) return 0;
    }
    return *a == 0 && *b == 0;
}

static int lang_has_subtag(const char *name, const char *tag) {
    size_t tl = strlen(tag);
    for (const char *p = name; *p; ) {
        while (*p == '-' || *p == '_') p++;
        const char *q = p;
        while (*q && *q != '-' && *q != '_') q++;
        if ((size_t)(q - p) == tl) {
            size_t i = 0;
            for (; i < tl; i++) if (tolower((unsigned char)p[i]) != tolower((unsigned char)tag[i])) break;
            if (i == tl) return 1;
        }
        p = q;
    }
    return 0;
}

/* name = a system display language ("en-GB", "es-MX", "de-DE"), graphics.ini `language`, or HL_LANGUAGE;
 * pick = one of the 13 LanguageCodes Luigi's Mansion 3 (main v1.4.0, 0x7100081970..0x7100081adc) checks, else en-US. */
static void language_pick(const char *name, char pick[16]) {
    static const char *const supported[] = {"en-US", "en-GB", "es", "es-419", "fr", "fr-CA",
                                            "de", "it", "nl", "ja", "ko", "zh-Hans", "zh-Hant"};
    const size_t n = sizeof supported / sizeof supported[0];
    strcpy(pick, "en-US");
    if (!name || !name[0]) return;
    for (size_t i = 0; i < n; i++) if (lang_ieq(name, supported[i])) { strcpy(pick, supported[i]); return; }
    char two[3] = {(char)tolower((unsigned char)name[0]), name[1] ? (char)tolower((unsigned char)name[1]) : 0, 0};
    int sep = name[1] && (name[2] == 0 || name[2] == '-' || name[2] == '_');
    if (!sep) return;
    if (!strcmp(two, "zh")) {
        strcpy(pick, lang_has_subtag(name + 2, "TW") || lang_has_subtag(name + 2, "HK") ||
                     lang_has_subtag(name + 2, "MO") || lang_has_subtag(name + 2, "Hant") ? "zh-Hant" : "zh-Hans");
    } else if (!strcmp(two, "es")) {
        strcpy(pick, !name[2] || lang_has_subtag(name + 2, "ES") ? "es" : "es-419");
    } else if (!strcmp(two, "en")) {
        strcpy(pick, lang_has_subtag(name + 2, "GB") || lang_has_subtag(name + 2, "AU") ||
                     lang_has_subtag(name + 2, "NZ") || lang_has_subtag(name + 2, "IE") ||
                     lang_has_subtag(name + 2, "ZA") ? "en-GB" : "en-US");
    } else if (!strcmp(two, "fr")) {
        strcpy(pick, lang_has_subtag(name + 2, "CA") ? "fr-CA" : "fr");
    } else {
        for (size_t i = 0; i < n; i++) if (lang_ieq(two, supported[i])) { strcpy(pick, supported[i]); return; }
    }
}
#endif
