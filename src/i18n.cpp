#include "i18n.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>

namespace {

Translations createEnglish() {
    Translations t{};

    // Language Metadata
    t.code = "en";
    t.displayName = "English";
    t.languageAuto = "Auto";

    // Dashboard & Window Header
    t.title = "Display Color Adjustment";
    t.dashboardName = "Display";

    // Preset Buttons
    t.builtinPresets = {"Standard", "Vivid", "Night"};
    t.emptySlotSuffix = " (Empty)";
    t.save = "Save";
    t.selectSlot = "Select Slot";
    t.reset = "Reset";
    t.resetConfirm = "Confirm?";

    // Adjustment Controls & Sliders
    t.panelBrightness = "Panel Brightness";
    t.adjustmentChannel = "Adjustment Channel";
    t.channelNames = {"All", "Red", "Green", "Blue"};
    t.brightness = "Brightness";
    t.saturation = "Saturation";
    t.contrast = "Contrast";
    t.gamma = "Gamma";
    t.sharpness = "Sharpness";
    t.compare = "Hold to Compare";
    t.compareHeld = "Original";
    t.autoPreset = "Auto Preset";
    t.standalone = "Standalone";
    t.streaming = "PC Streaming";
    t.thisApp = "This App";
    t.autoOff = "Off";
    t.noApp = "no app";
    t.nowPrefix = "Now: ";
    t.autoApplied = "Auto preset: ";
    t.autoRestored = "Restored the settings from before the auto preset";
    t.ruleSaved = "Rule saved. It applies when the mode or app changes";

    // Status & Feedback Messages
    t.errorSaveSettings = "Failed to save settings: ";
    t.applied = "Applied";
    t.errorSaveAdvanced = "Failed to save advanced settings";
    t.errorSavePreset = "Failed to save preset";
    t.slotEmpty = "This slot is empty";
    t.presetApplied = "Preset applied";
    t.promptSaveSlot = "Select slot P1-P3 to save";
    t.saveCanceled = "Save canceled";
    t.savedToSlotPrefix = "Saved to P";
    t.savedToSlotSuffix = "";

    return t;
}

Translations createJapanese() {
    Translations t{};

    // Language Metadata
    t.code = "ja";
    t.displayName = "日本語";
    t.languageAuto = "自動";

    // Dashboard & Window Header
    t.title = "ディスプレイ色調整";
    t.dashboardName = "ディスプレイ";

    // Preset Buttons
    t.builtinPresets = {"標準", "鮮やか", "夜間"};
    t.emptySlotSuffix = "（空）";
    t.save = "保存";
    t.selectSlot = "保存先を選択";
    t.reset = "リセット";
    t.resetConfirm = "確認？";

    // Adjustment Controls & Sliders
    t.panelBrightness = "パネル輝度";
    t.adjustmentChannel = "調整チャンネル";
    t.channelNames = {"全体", "赤", "緑", "青"};
    t.brightness = "明るさ";
    t.saturation = "彩度";
    t.contrast = "コントラスト";
    t.gamma = "ガンマ";
    t.sharpness = "シャープネス";
    t.compare = "長押しで比較";
    t.compareHeld = "補正なし";
    t.autoPreset = "自動プリセット";
    t.standalone = "単体";
    t.streaming = "PCストリーミング";
    t.thisApp = "このアプリ";
    t.autoOff = "オフ";
    t.noApp = "アプリなし";
    t.nowPrefix = "現在: ";
    t.autoApplied = "自動プリセット: ";
    t.autoRestored = "自動プリセット前の設定に戻しました";
    t.ruleSaved = "規則を保存しました．モードかアプリが変わると適用します";

    // Status & Feedback Messages
    t.errorSaveSettings = "設定の保存に失敗: ";
    t.applied = "適用しました";
    t.errorSaveAdvanced = "詳細設定を保存できません";
    t.errorSavePreset = "プリセットを保存できません";
    t.slotEmpty = "このスロットは未保存です";
    t.presetApplied = "プリセットを適用しました";
    t.promptSaveSlot = "保存先 P1〜P3 を選んでください";
    t.saveCanceled = "保存をキャンセルしました";
    t.savedToSlotPrefix = "P";
    t.savedToSlotSuffix = " に保存しました";

    return t;
}

Translations createKorean() {
    Translations t{};

    // Language Metadata
    t.code = "ko";
    t.displayName = "한국어";
    t.languageAuto = "자동";

    // Dashboard & Window Header
    t.title = "디스플레이 색 조정";
    t.dashboardName = "디스플레이";

    // Preset Buttons
    t.builtinPresets = {"표준", "선명", "야간"};
    t.emptySlotSuffix = " (비어 있음)";
    t.save = "저장";
    t.selectSlot = "저장할 슬롯 선택";
    t.reset = "초기화";
    t.resetConfirm = "확인?";

    // Adjustment Controls & Sliders
    t.panelBrightness = "패널 밝기";
    t.adjustmentChannel = "조정 채널";
    t.channelNames = {"전체", "빨강", "초록", "파랑"};
    t.brightness = "밝기";
    t.saturation = "채도";
    t.contrast = "대비";
    t.gamma = "감마";
    t.sharpness = "선명도";
    t.compare = "길게 눌러 비교";
    t.compareHeld = "보정 없음";
    t.autoPreset = "자동 프리셋";
    t.standalone = "독립형";
    t.streaming = "PC 스트리밍";
    t.thisApp = "이 앱";
    t.autoOff = "끔";
    t.noApp = "앱 없음";
    t.nowPrefix = "현재: ";
    t.autoApplied = "자동 프리셋: ";
    t.autoRestored = "자동 프리셋 전의 설정으로 되돌렸습니다";
    t.ruleSaved = "규칙을 저장했습니다. 모드나 앱이 바뀌면 적용됩니다";

    // Status & Feedback Messages
    t.errorSaveSettings = "설정을 저장하지 못했습니다: ";
    t.applied = "적용했습니다";
    t.errorSaveAdvanced = "고급 설정을 저장할 수 없습니다";
    t.errorSavePreset = "프리셋을 저장할 수 없습니다";
    t.slotEmpty = "이 슬롯은 비어 있습니다";
    t.presetApplied = "프리셋을 적용했습니다";
    t.promptSaveSlot = "저장할 슬롯 P1~P3을 고르세요";
    t.saveCanceled = "저장을 취소했습니다";
    t.savedToSlotPrefix = "P";
    t.savedToSlotSuffix = "에 저장했습니다";

    return t;
}

const Translations kTranslationsEn = createEnglish();
const Translations kTranslationsJa = createJapanese();
const Translations kTranslationsKo = createKorean();

const std::vector<Language> kLanguages = {
    Language::English,
    Language::Japanese,
    Language::Korean,
};

std::string trimmedLower(const std::string& code) {
    std::string s = code;
    s.erase(0, s.find_first_not_of(" \t\r\n"));
    s.erase(s.find_last_not_of(" \t\r\n") + 1);
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return s;
}

// The Steam client language ("english", "japanese", "koreana", ...) from the
// first "language" key in ~/.steam/registry.vdf, or "" if it cannot be read.
std::string steamClientLanguage() {
    const char* home = std::getenv("HOME");
    if (home == nullptr || *home == '\0') return {};
    std::ifstream in(std::filesystem::path(home) / ".steam/registry.vdf");
    for (std::string line; std::getline(in, line);) {
        const auto key = line.find("\"language\"");
        if (key == std::string::npos) continue;
        const auto open = line.find('"', key + 10);
        const auto close = open == std::string::npos ? open : line.find('"', open + 1);
        if (close != std::string::npos) return line.substr(open + 1, close - open - 1);
    }
    return {};
}

}  // namespace

std::string Translations::formatSavedToSlot(int slotNumber) const {
    return std::string(savedToSlotPrefix) + std::to_string(slotNumber) + savedToSlotSuffix;
}

const std::vector<Language>& availableLanguages() {
    return kLanguages;
}

const Translations& getTranslations(Language lang) {
    switch (lang) {
        case Language::Japanese:
            return kTranslationsJa;
        case Language::Korean:
            return kTranslationsKo;
        case Language::English:
        default:
            return kTranslationsEn;
    }
}

std::optional<Language> matchLanguage(const std::string& code) {
    const std::string s = trimmedLower(code);
    // Locale names such as "ko_KR.UTF-8" match on their language part.
    const auto is = [&s](const char* name) { return s == name || s.rfind(std::string(name) + "_", 0) == 0; };
    if (is("ja") || s == "japanese" || s == "jp") return Language::Japanese;
    if (is("ko") || s == "korean" || s == "koreana") return Language::Korean;
    if (is("en") || s == "english") return Language::English;
    return std::nullopt;
}

Language languageFromCode(const std::string& code) {
    return matchLanguage(code).value_or(Language::English);
}

std::string languageCode(Language lang) {
    switch (lang) {
        case Language::Japanese:
            return "ja";
        case Language::Korean:
            return "ko";
        case Language::English:
        default:
            return "en";
    }
}

Language detectLanguage() {
    if (const auto steam = matchLanguage(steamClientLanguage())) return *steam;
    for (const char* name : {"LC_ALL", "LC_MESSAGES", "LANG"}) {
        const char* value = std::getenv(name);
        if (value == nullptr || *value == '\0') continue;
        // The first variable that is set decides, as in POSIX locale lookup.
        return languageFromCode(value);
    }
    return Language::English;
}

namespace {

std::string savedLanguageCode(const std::filesystem::path& configDir) {
    std::ifstream in(configDir / "language.txt");
    std::string code;
    in >> code;
    return trimmedLower(code);
}

bool writeLanguageFile(const std::filesystem::path& configDir, const std::string& code) {
    std::error_code ec;
    std::filesystem::create_directories(configDir, ec);

    const auto path = configDir / "language.txt";
    const auto temp = configDir / "language.txt.tmp";

    std::ofstream out(temp, std::ios::trunc);
    if (!out) return false;
    out << code << '\n';
    out.close();

    std::filesystem::rename(temp, path, ec);
    if (ec) {
        std::filesystem::remove(temp, ec);
        return false;
    }
    return true;
}

}  // namespace

bool languageIsAuto(const std::filesystem::path& configDir) {
    const std::string code = savedLanguageCode(configDir);
    return code.empty() || code == "auto";
}

Language loadSavedLanguage(const std::filesystem::path& configDir) {
    if (languageIsAuto(configDir)) return detectLanguage();
    return languageFromCode(savedLanguageCode(configDir));
}

bool saveAutoLanguage(const std::filesystem::path& configDir) {
    return writeLanguageFile(configDir, "auto");
}

bool saveLanguage(const std::filesystem::path& configDir, Language lang) {
    return writeLanguageFile(configDir, languageCode(lang));
}
