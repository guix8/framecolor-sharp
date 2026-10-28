#include "draw.h"
#include "openvr.h"
#include "vr_overlay.h"

#include <cairo.h>
#include <signal.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace {

constexpr int kWidth = 1200;
constexpr int kHeight = 790;
constexpr int kThumb = 256;
constexpr const char* kFont = "/usr/share/fonts/noto-cjk/NotoSansCJK-Regular.ttc";
constexpr const char* kBold = "/usr/share/fonts/noto-cjk/NotoSansCJK-Bold.ttc";
constexpr double kTrackX = 405;
constexpr double kTrackW = 625;
constexpr Color kBg{0.051, 0.067, 0.090};
constexpr Color kCard{0.086, 0.106, 0.133};
constexpr Color kControl{0.129, 0.149, 0.176};
constexpr Color kHover{0.188, 0.208, 0.239};
constexpr Color kBorder{0.431, 0.463, 0.506};
constexpr Color kText{0.902, 0.929, 0.953};
constexpr Color kMuted{0.569, 0.596, 0.631};
constexpr Color kAccent{0.886, 0.490, 0.992};
constexpr Color kGreen{0.247, 0.725, 0.314};
constexpr Color kRed{0.973, 0.318, 0.286};

volatile sig_atomic_t gStop = 0;
void stopHandler(int) { gStop = 1; }

double clamp(double value, double lo, double hi) { return std::max(lo, std::min(hi, value)); }

struct Values {
    double panel = 1.0;
    double contrast = 0.0;
    double red = 1.0;
    double green = 1.0;
    double blue = 1.0;
    std::array<double, 3> channelContrast{{0.0, 0.0, 0.0}};
    std::array<double, 3> gamma{{1.0, 1.0, 1.0}};
    std::array<double, 3> saturation{{1.0, 1.0, 1.0}};
    double sharpen = 0.0;
};

struct Preset { Values values; bool valid = false; };

enum class HitKind { Slider, ChannelMenu, ChannelChoice, Preset, SaveMode, Reset };
struct Hit { double x, y, w, h; HitKind kind; int arg; };

struct UiState {
    Values values;
    std::array<Preset, 3> custom{};
    int channel = 0;       // 0=all, 1=red, 2=green, 3=blue
    int activeSlider = -1;
    bool channelOpen = false;
    bool saveMode = false;
    bool lookDirty = false;
    bool vulkanLayerActive = false;
    std::string message;
};

const std::array<Preset, 3> kBuiltins{{
    {{1.00, 0.00, 1.00, 1.00, 1.00, {0.00, 0.00, 0.00}, {1.00, 1.00, 1.00}, {1.00, 1.00, 1.00}}, true},
    {{1.10, 0.30, 1.05, 1.05, 1.05, {0.30, 0.30, 0.30}, {1.08, 1.08, 1.08}, {1.00, 1.00, 1.00}}, true},
    {{0.70, 0.15, 1.05, 0.95, 0.82, {0.15, 0.15, 0.15}, {0.92, 0.92, 0.92}, {1.00, 1.00, 1.00}}, true},
}};

float getSetting(const char* key, float fallback) {
    vr::EVRSettingsError error = vr::VRSettingsError_None;
    const float value = vr::VRSettings()->GetFloat("steamvr", key, &error);
    return error == vr::VRSettingsError_None ? value : fallback;
}

void readValues(Values& v) {
    v.panel = getSetting("analogGain", 1.0f);
}

double average(const std::array<double, 3>& values);
bool setSetting(const char* key, double value, UiState& ui);

double average(const std::array<double, 3>& values) {
    return (values[0] + values[1] + values[2]) / 3.0;
}

bool setSetting(const char* key, double value, UiState& ui) {
    vr::EVRSettingsError error = vr::VRSettingsError_None;
    vr::VRSettings()->SetFloat("steamvr", key, static_cast<float>(value), &error);
    if (error != vr::VRSettingsError_None) {
        ui.message = std::string("設定の保存に失敗: ") + vr::VRSettings()->GetSettingsErrorNameFromEnum(error);
        return false;
    }
    ui.message = "適用しました";
    return true;
}

std::filesystem::path presetPath() {
    const char* config = std::getenv("XDG_CONFIG_HOME");
    if (config != nullptr && config[0] != '\0') return std::filesystem::path(config) / "framecolor" / "presets.txt";
    const char* home = std::getenv("HOME");
    return std::filesystem::path(home != nullptr ? home : "/tmp") / ".config" / "framecolor" / "presets.txt";
}

std::filesystem::path currentPath() { return presetPath().parent_path() / "current.txt"; }

void neutralizeLegacyStreamGains(UiState& ui) {
    // Earlier releases wrote these stream-only gains. Leave them neutral so
    // that existing settings cannot double-apply the Vulkan LUT correction.
    for (int channel = 0; channel < 3; ++channel) {
        setSetting(channel == 0 ? "hmdDisplayColorGainR" : (channel == 1 ? "hmdDisplayColorGainG" : "hmdDisplayColorGainB"), 1.0, ui);
    }
}

void loadAdvancedValues(Values& values) {
    std::ifstream in(currentPath());
    std::vector<double> stored;
    for (double value; in >> value;) stored.push_back(value);
    if (stored.size() >= 6) {
        std::copy_n(stored.begin(), 3, values.channelContrast.begin());
        std::copy_n(stored.begin() + 3, 3, values.gamma.begin());
        if (stored.size() >= 9) {
            values.red = stored[6];
            values.green = stored[7];
            values.blue = stored[8];
        }
        if (stored.size() >= 12) {
            for (int channel = 0; channel < 3; ++channel)
                values.saturation[channel] = clamp(stored[9 + channel], 0.0, 2.0);
        } else if (stored.size() >= 10) {
            values.saturation.fill(clamp(stored[9], 0.0, 2.0));
        }
        if (stored.size() >= 13) values.sharpen = clamp(stored[12], 0.0, 1.0);
    } else {
        values.channelContrast = {values.contrast, values.contrast, values.contrast};
        values.gamma = {1.0, 1.0, 1.0};
    }
    values.contrast = average(values.channelContrast);
}

bool saveAdvancedValues(UiState& ui) {
    const auto path = currentPath();
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    const auto temporary = path.string() + ".tmp";
    std::ofstream out(temporary, std::ios::trunc);
    if (!out) { ui.message = "詳細設定を保存できません"; return false; }
    out << std::setprecision(9)
        << ui.values.channelContrast[0] << ' ' << ui.values.channelContrast[1] << ' ' << ui.values.channelContrast[2] << ' '
        << ui.values.gamma[0] << ' ' << ui.values.gamma[1] << ' ' << ui.values.gamma[2] << ' '
        << ui.values.red << ' ' << ui.values.green << ' ' << ui.values.blue << ' '
        << ui.values.saturation[0] << ' ' << ui.values.saturation[1] << ' '
        << ui.values.saturation[2] << ' ' << ui.values.sharpen << '\n';
    out.close();
    std::filesystem::rename(temporary, path, ec);
    if (ec) { std::filesystem::remove(temporary); ui.message = "詳細設定を保存できません"; return false; }
    return true;
}

void loadCustomPresets(UiState& ui) {
    std::ifstream in(presetPath());
    for (Preset& preset : ui.custom) {
        std::string line;
        if (!std::getline(in, line)) break;
        std::istringstream row(line);
        int valid = 0;
        if (!(row >> valid >> preset.values.panel >> preset.values.contrast >> preset.values.red
                  >> preset.values.green >> preset.values.blue)) break;
        if (!(row >> preset.values.channelContrast[0] >> preset.values.channelContrast[1] >> preset.values.channelContrast[2]
                  >> preset.values.gamma[0] >> preset.values.gamma[1] >> preset.values.gamma[2])) {
            preset.values.channelContrast = {preset.values.contrast, preset.values.contrast, preset.values.contrast};
            preset.values.gamma = {1.0, 1.0, 1.0};
        }
        std::vector<double> storedSaturation;
        for (double value; row >> value;) storedSaturation.push_back(value);
        if (storedSaturation.size() >= 3) {
            for (int channel = 0; channel < 3; ++channel)
                preset.values.saturation[channel] = clamp(storedSaturation[channel], 0.0, 2.0);
        } else if (!storedSaturation.empty()) {
            preset.values.saturation.fill(clamp(storedSaturation[0], 0.0, 2.0));
        }
        if (storedSaturation.size() >= 4) preset.values.sharpen = clamp(storedSaturation[3], 0.0, 1.0);
        preset.valid = valid != 0;
    }
}

bool saveCustomPresets(UiState& ui) {
    const auto path = presetPath();
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    const auto temporary = path.string() + ".tmp";
    std::ofstream out(temporary, std::ios::trunc);
    if (!out) { ui.message = "プリセットを保存できません"; return false; }
    out << std::setprecision(9);
    for (const Preset& preset : ui.custom) {
        const Values& v = preset.values;
        out << (preset.valid ? 1 : 0) << ' ' << v.panel << ' ' << v.contrast << ' '
            << v.red << ' ' << v.green << ' ' << v.blue << ' '
            << v.channelContrast[0] << ' ' << v.channelContrast[1] << ' ' << v.channelContrast[2] << ' '
            << v.gamma[0] << ' ' << v.gamma[1] << ' ' << v.gamma[2] << ' '
            << v.saturation[0] << ' ' << v.saturation[1] << ' ' << v.saturation[2] << ' '
            << v.sharpen << '\n';
    }
    out.close();
    std::filesystem::rename(temporary, path, ec);
    if (ec) { std::filesystem::remove(temporary); ui.message = "プリセットを保存できません"; return false; }
    return true;
}

void applyPreset(const Preset& preset, UiState& ui) {
    if (!preset.valid) { ui.message = "このスロットは未保存です"; return; }
    ui.values = preset.values;
    setSetting("analogGain", ui.values.panel, ui);
    ui.lookDirty = true;
    saveAdvancedValues(ui);
    readValues(ui.values);
    if (ui.message == "適用しました" || ui.message == "プリセットを適用しました") ui.message.clear();
}

void fillRect(const Pen& p, double x, double y, double w, double h, Color c, double radius = 12) {
    p.roundedRect(x, y, w, h, radius); p.color(c); cairo_fill(p.cr);
}

void strokeRect(const Pen& p, double x, double y, double w, double h, Color c, double radius = 12) {
    p.roundedRect(x, y, w, h, radius); p.color(c); cairo_set_line_width(p.cr, 2); cairo_stroke(p.cr);
}

bool inside(double px, double py, double x, double y, double w, double h) {
    return px >= x && px <= x + w && py >= y && py <= y + h;
}

void button(const Pen& p, std::vector<Hit>& hits, double x, double y, double w, double h,
            const std::string& label, HitKind kind, int arg, bool selected, bool usable,
            double pointerX, double pointerY) {
    const bool hovered = usable && inside(pointerX, pointerY, x, y, w, h);
    fillRect(p, x, y, w, h, selected ? kAccent : (hovered ? kHover : kControl));
    strokeRect(p, x, y, w, h, hovered ? kAccent : kBorder);
    const Color color = usable ? (selected ? kBg : kText) : kMuted;
    const double tw = p.measure(label, 21, true);
    p.text(x + (w - tw) / 2, y + h / 2 + 8, label, 21, color, true);
    if (usable) hits.push_back({x, y, w, h, kind, arg});
}

std::string percent(double value) {
    char text[32]; std::snprintf(text, sizeof(text), "%.0f%%", value * 100.0); return text;
}

void slider(const Pen& p, std::vector<Hit>& hits, double y, const std::string& title,
            double value, double lo, double hi, int index, bool usable) {
    fillRect(p, 45, y, 1110, 78, kCard);
    p.text(70, y + 49, title, 25, usable ? kText : kMuted, true);
    const double cy = y + 39;
    p.color(usable ? kBorder : kControl);
    cairo_set_line_width(p.cr, 10); cairo_set_line_cap(p.cr, CAIRO_LINE_CAP_ROUND);
    cairo_move_to(p.cr, kTrackX, cy); cairo_line_to(p.cr, kTrackX + kTrackW, cy); cairo_stroke(p.cr);
    const double fraction = clamp((value - lo) / (hi - lo), 0.0, 1.0);
    const double knobX = kTrackX + fraction * kTrackW;
    if (usable) {
        p.color(kAccent); cairo_set_line_width(p.cr, 10);
        cairo_move_to(p.cr, kTrackX, cy); cairo_line_to(p.cr, knobX, cy); cairo_stroke(p.cr);
    }
    p.color(usable ? kAccent : kMuted); cairo_arc(p.cr, knobX, cy, 16, 0, 2 * M_PI); cairo_fill(p.cr);
    std::string shown = percent(value);
    if (index == 3) { char text[24]; std::snprintf(text, sizeof(text), "%.2f", value); shown = text; }
    p.text(1120, y + 49, shown, 24, usable ? kText : kMuted, true, true);
    if (usable) hits.push_back({kTrackX - 20, y + 8, kTrackW + 40, 62, HitKind::Slider, index});
}

double selectedGain(const UiState& ui) {
    if (ui.channel == 1) return ui.values.red;
    if (ui.channel == 2) return ui.values.green;
    if (ui.channel == 3) return ui.values.blue;
    return (ui.values.red + ui.values.green + ui.values.blue) / 3.0;
}

double selectedArrayValue(const std::array<double, 3>& values, int channel) {
    return channel == 0 ? average(values) : values[channel - 1];
}

void setSelectedArrayValue(std::array<double, 3>& values, int channel, double next, double lo, double hi) {
    if (channel == 0) {
        const double delta = next - average(values);
        for (double& value : values) value = clamp(value + delta, lo, hi);
    } else {
        values[channel - 1] = next;
    }
}

void updateSlider(UiState& ui, int index, double pointerX) {
    const double fraction = clamp((pointerX - kTrackX) / kTrackW, 0.0, 1.0);
    if (index == 0) {
        ui.values.panel = std::round((0.40 + fraction * 0.85) * 100.0) / 100.0;
        setSetting("analogGain", ui.values.panel, ui);
    } else if (index == 1) {
        const double next = std::round((0.50 + fraction) * 100.0) / 100.0;
        if (ui.channel == 0) {
            const double delta = next - selectedGain(ui);
            ui.values.red = clamp(ui.values.red + delta, 0.50, 1.50);
            ui.values.green = clamp(ui.values.green + delta, 0.50, 1.50);
            ui.values.blue = clamp(ui.values.blue + delta, 0.50, 1.50);
        } else {
            double* value = ui.channel == 1 ? &ui.values.red : (ui.channel == 2 ? &ui.values.green : &ui.values.blue);
            *value = next;
        }
        ui.lookDirty = true;
    } else if (index == 2) {
        const double next = std::round(fraction * 100.0) / 100.0;
        setSelectedArrayValue(ui.values.channelContrast, ui.channel, next, 0.0, 1.0);
        ui.lookDirty = true;
    } else if (index == 3) {
        const double next = std::round((0.50 + fraction * 2.0) * 100.0) / 100.0;
        setSelectedArrayValue(ui.values.gamma, ui.channel, next, 0.50, 2.50);
        ui.lookDirty = true;
    } else if (index == 4) {
        const double next = std::round(fraction * 200.0) / 100.0;
        setSelectedArrayValue(ui.values.saturation, ui.channel, next, 0.0, 2.0);
        ui.lookDirty = true;
    } else if (index == 5) {
        // Sharpening applies to all channels, like panel brightness.
        ui.values.sharpen = std::round(fraction * 100.0) / 100.0;
        ui.lookDirty = true;
    }
}

void render(const FontSet& fonts, const UiState& ui, double pointerX, double pointerY,
            std::vector<uint8_t>& rgba, std::vector<Hit>& hits) {
    cairo_surface_t* surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, kWidth, kHeight);
    cairo_t* cr = cairo_create(surface); Pen p{cr, &fonts};
    cairo_set_source_rgb(cr, kBg.r, kBg.g, kBg.b); cairo_paint(cr);
    p.text(45, 51, "Display Color Adjustment", 34, kText, true);
    hits.clear();

    const std::array<std::string, 6> names{{"標準", "鮮やか", "夜間", "P1", "P2", "P3"}};
    for (int i = 0; i < 6; ++i) {
        const bool valid = i < 3 || ui.custom[i - 3].valid;
        std::string label = names[i];
        if (i >= 3 && !valid) label += "（空）";
        button(p, hits, 45 + i * 137, 101, 125, 48, label, HitKind::Preset, i, false,
               !ui.saveMode || i >= 3, pointerX, pointerY);
    }
    button(p, hits, 885, 101, 125, 48, ui.saveMode ? "保存先を選択" : "保存", HitKind::SaveMode, 0,
           ui.saveMode, true, pointerX, pointerY);
    button(p, hits, 1025, 101, 130, 48, "リセット", HitKind::Reset, 0, false, true, pointerX, pointerY);

    static const std::array<const char*, 4> channelNames{{"全体", "赤", "緑", "青"}};
    slider(p, hits, 174, "パネル輝度", ui.values.panel, 0.40, 1.25, 0, true);

    p.text(45, 289, "調整チャンネル", 22, kText, true);
    button(p, hits, 885, 251, 270, 54, std::string(channelNames[ui.channel]) + "    v",
           HitKind::ChannelMenu, 0, ui.channelOpen, true, pointerX, pointerY);

    slider(p, hits, 320, "明るさ", selectedGain(ui), 0.50, 1.50, 1, ui.vulkanLayerActive);
    slider(p, hits, 410, "彩度", selectedArrayValue(ui.values.saturation, ui.channel),
           0.0, 2.0, 4, ui.vulkanLayerActive);
    slider(p, hits, 500, "コントラスト",
           selectedArrayValue(ui.values.channelContrast, ui.channel), 0.0, 1.0, 2, ui.vulkanLayerActive);
    slider(p, hits, 590, "ガンマ",
           selectedArrayValue(ui.values.gamma, ui.channel), 0.50, 2.50, 3, ui.vulkanLayerActive);
    slider(p, hits, 680, "シャープネス", ui.values.sharpen, 0.0, 1.0, 5, ui.vulkanLayerActive);

    if (!ui.message.empty()) p.text(45, 770, ui.message, 20, ui.message.find("失敗") == std::string::npos ? kGreen : kRed);

    if (ui.channelOpen) {
        fillRect(p, 885, 307, 270, 224, kBg, 10); strokeRect(p, 885, 307, 270, 224, kAccent, 10);
        for (int i = 0; i < 4; ++i) {
            button(p, hits, 895, 317 + i * 51, 250, 43, channelNames[i], HitKind::ChannelChoice, i,
                   ui.channel == i, true, pointerX, pointerY);
        }
    }

    surfaceToRgba(surface, rgba); cairo_destroy(cr); cairo_surface_destroy(surface);
}

void thumbnail(const FontSet& fonts, std::vector<uint8_t>& rgba) {
    cairo_surface_t* surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, kThumb, kThumb);
    cairo_t* cr = cairo_create(surface); Pen p{cr, &fonts};
    cairo_set_source_rgb(cr, kBg.r, kBg.g, kBg.b); cairo_paint(cr);
    fillRect(p, 30, 44, 196, 150, kCard, 26); strokeRect(p, 30, 44, 196, 150, kAccent, 26);
    p.text(128, 137, "RGB", 57, kAccent, true, true);
    p.text(128, 224, "Display", 25, kText, true, true);
    surfaceToRgba(surface, rgba); cairo_destroy(cr); cairo_surface_destroy(surface);
}

const Hit* hitTest(const std::vector<Hit>& hits, double x, double y) {
    for (auto it = hits.rbegin(); it != hits.rend(); ++it) if (inside(x, y, it->x, it->y, it->w, it->h)) return &*it;
    return nullptr;
}

void activate(const Hit& hit, double x, UiState& ui) {
    switch (hit.kind) {
        case HitKind::Slider: ui.activeSlider = hit.arg; ui.channelOpen = false; updateSlider(ui, hit.arg, x); break;
        case HitKind::ChannelMenu: ui.channelOpen = !ui.channelOpen; break;
        case HitKind::ChannelChoice: ui.channel = hit.arg; ui.channelOpen = false; ui.message.clear(); break;
        case HitKind::SaveMode: ui.saveMode = !ui.saveMode; ui.message = ui.saveMode ? "保存先 P1〜P3 を選んでください" : "保存をキャンセルしました"; break;
        case HitKind::Preset:
            if (ui.saveMode) {
                if (hit.arg >= 3) {
                    ui.custom[hit.arg - 3] = {ui.values, true};
                    if (saveCustomPresets(ui)) ui.message = "P" + std::to_string(hit.arg - 2) + " に保存しました";
                    ui.saveMode = false;
                }
            } else {
                applyPreset(hit.arg < 3 ? kBuiltins[hit.arg] : ui.custom[hit.arg - 3], ui);
            }
            ui.channelOpen = false;
            break;
        case HitKind::Reset: applyPreset(kBuiltins[0], ui); ui.channelOpen = false; ui.saveMode = false; break;
    }
}

} // namespace

int main() {
    signal(SIGINT, stopHandler); signal(SIGTERM, stopHandler);
    VrOverlay overlay; std::string error;
    while (!gStop) {
        const auto result = overlay.connect(kWidth, kHeight, error);
        if (result == VrOverlay::ConnectResult::Ok) break;
        if (result == VrOverlay::ConnectResult::Error) { std::fprintf(stderr, "%s\n", error.c_str()); return 1; }
        std::this_thread::sleep_for(std::chrono::seconds(3));
    }
    if (gStop) return 0;
    FontSet fonts; fonts.load(kFont, kBold);
    std::vector<uint8_t> pixels; thumbnail(fonts, pixels); overlay.submitThumbnail(pixels.data(), kThumb);
    UiState ui; readValues(ui.values); loadAdvancedValues(ui.values); loadCustomPresets(ui);
    const char* dataHome = std::getenv("XDG_DATA_HOME");
    const char* home = std::getenv("HOME");
    const std::filesystem::path dataRoot = dataHome && *dataHome ? dataHome :
        std::filesystem::path(home ? home : "/tmp") / ".local/share";
    ui.vulkanLayerActive = std::filesystem::exists(dataRoot / "framecolor/layer/vkBasalt.json");
    setSetting("contrast", 0.0, ui);
    neutralizeLegacyStreamGains(ui);
    auto lastLookSave = std::chrono::steady_clock::now();
    std::vector<Hit> hits; double px = -1, py = -1; bool dirty = true; bool wasVisible = false;
    while (!gStop && overlay.steamVrAlive()) {
        const VrEvents events = overlay.pollEvents();
        if (events.quit || events.closeRequested) break;
        for (const auto& e : events.pointer) {
            if (e.type == PointerInput::Type::Move) {
                px = e.x; py = e.y;
                if (ui.activeSlider >= 0) updateSlider(ui, ui.activeSlider, px);
                dirty = true;
            } else if (e.type == PointerInput::Type::Leave) {
                px = py = -1; ui.activeSlider = -1; dirty = true;
            } else if (e.type == PointerInput::Type::Down) {
                if (const Hit* hit = hitTest(hits, e.x, e.y)) activate(*hit, e.x, ui);
                else ui.channelOpen = false;
                dirty = true;
            } else if (e.type == PointerInput::Type::Up) {
                ui.activeSlider = -1; readValues(ui.values);
                if (saveAdvancedValues(ui)) ui.lookDirty = false;
                dirty = true;
            }
        }
        const auto now = std::chrono::steady_clock::now();
        if (ui.vulkanLayerActive && ui.lookDirty &&
            (ui.activeSlider < 0 || now - lastLookSave >= std::chrono::milliseconds(150))) {
            if (saveAdvancedValues(ui)) {
                ui.lookDirty = false;
                lastLookSave = now;
            }
        }
        const bool visible = overlay.panelVisible();
        if (visible && (!wasVisible || dirty)) {
            render(fonts, ui, px, py, pixels, hits); overlay.submitPanel(pixels.data()); dirty = false;
        }
        wasVisible = visible;
        std::this_thread::sleep_for(std::chrono::milliseconds(visible ? 33 : 250));
    }
    overlay.shutdown();
    return 0;
}
