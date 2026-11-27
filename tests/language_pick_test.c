/* Unit tests for games/lm3/runtime/language_pick.h (LM3's 13 supported languages and locale fallbacks).
 *   clang -O2 -Wall -I games/lm3/runtime tests/language_pick_test.c -o /tmp/language_pick_test && /tmp/language_pick_test */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "language_pick.h"

static void expect(const char *in, const char *want) {
    char got[16];
    language_pick(in, got);
    if (strcmp(got, want) != 0) {
        fprintf(stderr, "language_pick(%s): got \"%s\", want \"%s\"\n", in ? in : "(null)", got, want);
        assert(0);
    }
}

int main(void) {
    /* All 13 exact codes Luigi's Mansion 3 checks in main v1.4.0 (0x7100081970..0x7100081adc) */
    const char *exact[] = {"en-US", "en-GB", "es", "es-419", "fr", "fr-CA",
                           "de", "it", "nl", "ja", "ko", "zh-Hans", "zh-Hant"};
    for (size_t i = 0; i < sizeof exact / sizeof exact[0]; i++) expect(exact[i], exact[i]);

    /* Case-insensitive and POSIX underscore variants */
    expect("ES-419", "es-419");
    expect("es_419", "es-419");
    expect("en_gb", "en-GB");
    expect("FR_CA", "fr-CA");
    expect("zh_hant", "zh-Hant");
    expect("ZH-HANS", "zh-Hans");

    /* Spanish regional locales */
    expect("es-ES", "es");
    expect("es_ES", "es");
    expect("es-MX", "es-419");
    expect("es-AR", "es-419");
    expect("es-CO", "es-419");
    expect("es-CL", "es-419");
    expect("es-PE", "es-419");
    expect("es-US", "es-419");

    /* English regional locales */
    expect("en", "en-US");
    expect("en-CA", "en-US");
    expect("en-AU", "en-GB");
    expect("en-NZ", "en-GB");
    expect("en-IE", "en-GB");
    expect("en-ZA", "en-GB");

    /* French regional locales */
    expect("fr-FR", "fr");
    expect("fr-BE", "fr");
    expect("fr-CH", "fr");
    expect("fr-CA", "fr-CA");

    /* Other supported single-variant languages */
    expect("de-DE", "de");
    expect("de-AT", "de");
    expect("it-IT", "it");
    expect("nl-NL", "nl");
    expect("nl-BE", "nl");
    expect("ja-JP", "ja");
    expect("ko-KR", "ko");

    /* Chinese regional locales */
    expect("zh-CN", "zh-Hans");
    expect("zh-SG", "zh-Hans");
    expect("zh-TW", "zh-Hant");
    expect("zh-HK", "zh-Hant");
    expect("zh-MO", "zh-Hant");
    expect("zh-Hant-TW", "zh-Hant");
    expect("zh-Hans-CN", "zh-Hans");

    /* Unsupported languages & edge cases fall back to en-US (LM3 has no Russian or Portuguese) */
    expect("ru", "en-US");
    expect("ru-RU", "en-US");
    expect("pt-BR", "en-US");
    expect("pt", "en-US");
    expect("pl-PL", "en-US");
    expect("esp", "en-US");
    expect("", "en-US");
    expect(NULL, "en-US");

    printf("language_pick_test: all checks passed\n");
    return 0;
}
