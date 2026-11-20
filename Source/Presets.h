#pragma once
// The 26 voice presets, taken from DJZ-Speak (MushroomFleet/ComfyUI-DJZ-Speak).
// Each one is an eSpeak-NG voice + variant with speed (words per minute), pitch (0-99),
// amplitude and word gap (in units of 10 ms).

struct VoicePreset
{
    const char* id;
    const char* name;      // shown on the screen
    const char* language;  // eSpeak-NG voice
    const char* variant;   // eSpeak-NG variant (voices/!v)
    int speed, pitch, amplitude, gap;
};

static constexpr VoicePreset kVoicePresets[] =
{
    // retro computing
    { "sbaitso",           "DR. SBAITSO",       "en",         "m2", 160, 45, 110,  6 },
    { "vintage_computer",  "VINTAGE COMPUTER",  "en",         "m3", 130, 40, 100, 12 },
    { "dectalk",           "DECTALK",           "en",         "m1", 120, 25,  95, 10 },
    { "atari_sam",         "ATARI SAM",         "en",         "m3", 140, 50, 110,  8 },
    { "amiga_narrator",    "AMIGA NARRATOR",    "en",         "m2", 135, 48, 105,  9 },
    { "apple_ii",          "APPLE II",          "en",         "m3", 125, 45, 100, 12 },
    // cinematic AI
    { "hal9000",           "HAL 9000",          "en",         "m1", 100, 20,  85, 15 },
    { "c3po",              "C-3PO",             "en",         "m4", 150, 55, 105,  5 },
    { "terminator",        "TERMINATOR",        "en",         "m1", 110, 18,  90, 12 },
    { "glados",            "GLADOS",            "en",         "f2", 135, 50,  95,  8 },
    { "jarvis",            "JARVIS",            "en",         "m2", 155, 38, 100,  4 },
    { "robocop",           "ROBOCOP",           "en",         "m3", 125, 30, 105, 10 },
    { "wall_e",            "WALL-E",            "en",         "m4", 140, 60, 110,  6 },
    // modern and technical
    { "modern_ai",         "MODERN AI",         "en",         "m2", 160, 42,  95,  4 },
    { "classic_robot",     "CLASSIC ROBOT",     "en",         "m3", 140, 35, 100,  8 },
    { "navigation_system", "NAVIGATION SYSTEM", "en",         "f2", 150, 42, 100,  5 },
    { "space_station",     "SPACE STATION",     "en",         "m2", 140, 38,  95,  8 },
    { "computer_alert",    "COMPUTER ALERT",    "en",         "f1", 170, 45, 115,  3 },
    { "diagnostics",       "DIAGNOSTICS",       "en",         "m2", 130, 40,  95,  7 },
    { "countdown",         "COUNTDOWN",         "en",         "m1", 120, 35, 105, 15 },
    // character
    { "robotic_female",    "ROBOTIC FEMALE",    "en",         "f3", 145, 65, 100,  7 },
    { "robotic_child",     "ROBOTIC CHILD",     "en",         "f4", 160, 75, 105,  5 },
    { "robotic_elder",     "ROBOTIC ELDER",     "en",         "m1", 105, 28,  90, 18 },
    { "british_android",   "BRITISH ANDROID",   "en-gb-x-rp", "m3", 145, 40, 100,  6 },
    { "binary_whisper",    "BINARY WHISPER",    "en",         "f3", 180, 55,  70,  3 },
    { "heavy_metal",       "HEAVY METAL",       "en",         "m1", 145, 22, 120,  6 },
};

static constexpr int kNumVoices = (int) (sizeof (kVoicePresets) / sizeof (kVoicePresets[0]));
static constexpr int kDefaultVoice = 14; // CLASSIC ROBOT
static constexpr int kNumSlots = 20;
