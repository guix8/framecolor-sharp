// Automatic preset selection by display mode (standalone / PC streaming) and by app.
#pragma once

#include <filesystem>
#include <map>
#include <string>

/** Preset indices: 0-2 are the built-in presets, 3-5 are P1-P3, -1 means no rule. */
struct AutoRules {
    int standalone = -1;
    int streaming = -1;
    std::map<std::string, int> apps;  ///< OpenVR app key -> preset index
};

/** What is on screen now. */
struct DisplayContext {
    bool streaming = false;
    std::string appKey;  ///< empty when the scene app has no OpenVR app key
    bool operator==(const DisplayContext& other) const {
        return streaming == other.streaming && appKey == other.appKey;
    }
    bool operator!=(const DisplayContext& other) const { return !(*this == other); }
};

/** Read auto.txt. A missing or broken file gives no rules. */
AutoRules loadAutoRules(const std::filesystem::path& path);

/** Write auto.txt atomically. */
bool saveAutoRules(const std::filesystem::path& path, const AutoRules& rules);

/** The app rule wins over the mode rule. Returns -1 when no rule applies. */
int resolveAutoPreset(const AutoRules& rules, const DisplayContext& context);

/**
 * Whether a PC stream is running: a process whose argv[0] is "vrlink".
 * (vrlink renames its main thread, so /proc/PID/comm does not match.)
 */
bool isStreaming(const std::filesystem::path& procRoot = "/proc");

/** Next choice when a rule button is pressed: -1, 0, 1, 2, then the valid P1-P3 slots. */
int nextPresetChoice(int current, const bool customValid[3]);
