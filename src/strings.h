#ifndef STR_H
#define STR_H

enum VoiceActors {
    VA_AL,
    VA_HELGA,
    VA_QWARK,
    VA_RANGER,
    VA_SASHA,
    VA_SKIDD,
    VA_COUNT
};

const int VoiceLinesCounts[VA_COUNT] {
    16, /*Al*/
    22, /*Helga*/
    30, /*Qwark*/
    15, /*Ranger*/
    26, /*Sasha*/
    17, /*Skidd*/
};

#define VLC_MAX_COUNT 30

//Total : 126

enum Languages {
    LANG_EN, //English
    LANG_FR, //French
    LANG_DE, //German (Deutch)
    LANG_IT, //Italian
    LANG_SP, //Spanish
    LANGUAGES_COUNT
};

const char* languageCodes[LANGUAGES_COUNT] = { "en", "fr", "de", "it", "sp" };
const char* actorCodes[VA_COUNT] = { "al", "helga", "qwark", "ranger", "sasha", "skidd" };

char* VoiceLinesNames[LANGUAGES_COUNT][VA_COUNT][VLC_MAX_COUNT] = { 0 };
char* VoiceActorsNames[LANGUAGES_COUNT][VA_COUNT] = { 0 };
char languages[LANGUAGES_COUNT][32] = { 0 };


#define currentVANSet VoiceActorsNames[currentLanguage]
#define currentVLNSet VoiceLinesNames[currentLanguage]

#endif