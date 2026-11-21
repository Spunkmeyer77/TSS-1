#pragma once
// The 26 voice presets, taken from DJZ-Speak (MushroomFleet/ComfyUI-DJZ-Speak).
// Each one is an eSpeak-NG voice + variant with speed (words per minute), pitch (0-99),
// amplitude and word gap (in units of 10 ms).

struct VoicePreset
{
    const char* id;
    const char* name;      // shown on the screen
    const char* language;  // eSpeak-NG voice
    const char* variant;   // eSpeak-NG variant (voices/!v): each preset uses a different one
    int speed, pitch, amplitude, gap;
    // the character of the preset beyond the voice: intonation and the effects it loads with
    int range;             // 0 = monotone
    float amount, harmonic, crush;
    bool filter;
};

// Speed, pitch, volume and gap after DJZ-Speak; variants, range and effects chosen per preset so
// that each one is a clearly different machine (Klatt-engine voices, Robosoft robots, whisper...).
static constexpr VoicePreset kVoicePresets[] =
{
    // retro computing
    { "sbaitso",           "DR. SBAITSO",       "en",         "klatt3",    160, 45, 110,  6, 30, 1.1f, 1.2f, 0.45f, true  },
    { "vintage_computer",  "VINTAGE COMPUTER",  "en",         "robosoft4", 130, 40, 100, 12, 20, 1.0f, 1.3f, 0.30f, true  },
    { "dectalk",           "DECTALK",           "en-us",      "klatt",     120, 25,  95, 10, 50, 0.8f, 1.0f, 0.00f, false },
    { "atari_sam",         "ATARI SAM",         "en-us",      "klatt2",    140, 50, 110,  8, 35, 1.3f, 1.2f, 0.70f, true  },
    { "amiga_narrator",    "AMIGA NARRATOR",    "en-us",      "klatt4",    135, 48, 105,  9, 45, 1.0f, 1.1f, 0.25f, true  },
    { "apple_ii",          "APPLE II",          "en-us",      "robosoft7", 125, 45, 100, 12, 25, 1.2f, 1.2f, 0.60f, true  },
    // cinematic AI
    { "hal9000",           "HAL 9000",          "en-us",      "m1",        100, 20,  85, 15, 25, 0.6f, 1.0f, 0.00f, false },
    { "c3po",              "C-3PO",             "en-gb-x-rp", "m4",        150, 55, 105,  5, 70, 1.0f, 1.3f, 0.00f, true  },
    { "terminator",        "TERMINATOR",        "en-us",      "robosoft",  110, 18,  90, 12,  5, 1.4f, 1.8f, 0.10f, true  },
    { "glados",            "GLADOS",            "en-us",      "robosoft8", 135, 50,  95,  8, 10, 1.2f, 1.4f, 0.00f, true  },
    { "jarvis",            "JARVIS",            "en-gb-x-rp", "m2",        155, 38, 100,  4, 55, 0.6f, 1.0f, 0.00f, false },
    { "robocop",           "ROBOCOP",           "en-us",      "UniRobot",  125, 30, 105, 10, 10, 1.3f, 1.5f, 0.20f, true  },
    { "wall_e",            "WALL-E",            "en-us",      "robosoft3", 140, 60, 110,  6, 60, 1.2f, 1.2f, 0.50f, true  },
    // modern and technical
    { "modern_ai",         "MODERN AI",         "en-us",      "f5",        160, 42,  95,  4, 50, 0.5f, 1.0f, 0.00f, false },
    { "classic_robot",     "CLASSIC ROBOT",     "en",         "robosoft2", 140, 35, 100,  8,  0, 1.2f, 1.4f, 0.15f, true  },
    { "navigation_system", "NAVIGATION SYSTEM", "en-us",      "f2",        150, 42, 100,  5, 40, 0.6f, 1.0f, 0.00f, true  },
    { "space_station",     "SPACE STATION",     "en-us",      "announcer", 140, 38,  95,  8, 30, 1.0f, 1.2f, 0.00f, true  },
    { "computer_alert",    "COMPUTER ALERT",    "en-us",      "klatt6",    170, 45, 115,  3, 20, 1.3f, 1.3f, 0.20f, true  },
    { "diagnostics",       "DIAGNOSTICS",       "en-us",      "robosoft5", 130, 40,  95,  7, 10, 1.1f, 1.2f, 0.10f, true  },
    { "countdown",         "COUNTDOWN",         "en",         "m7",        120, 35, 105, 15, 15, 1.0f, 1.3f, 0.00f, true  },
    // character
    { "robotic_female",    "ROBOTIC FEMALE",    "en",         "f3",        145, 65, 100,  7, 10, 1.3f, 1.6f, 0.10f, true  },
    { "robotic_child",     "ROBOTIC CHILD",     "en",         "robosoft6", 160, 75, 105,  5, 40, 1.1f, 1.2f, 0.30f, true  },
    { "robotic_elder",     "ROBOTIC ELDER",     "en",         "grandpa",   105, 28,  90, 18, 30, 0.9f, 1.2f, 0.00f, true  },
    { "british_android",   "BRITISH ANDROID",   "en-gb-x-rp", "klatt5",    145, 40, 100,  6, 35, 1.0f, 1.3f, 0.00f, true  },
    { "binary_whisper",    "BINARY WHISPER",    "en",         "whisper",   180, 55,  70,  3, 20, 1.0f, 1.0f, 0.20f, true  },
    { "heavy_metal",       "HEAVY METAL",       "en",         "croak",     145, 22, 120,  6, 10, 1.6f, 2.0f, 0.30f, true  },
};

static constexpr int kNumVoices = (int) (sizeof (kVoicePresets) / sizeof (kVoicePresets[0]));
static constexpr int kDefaultVoice = 14; // CLASSIC ROBOT
static constexpr int kNumSlots = 20;
