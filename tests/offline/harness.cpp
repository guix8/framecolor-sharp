// Off-device check: render the dashboard and test file I/O and auto preset logic.
// framecolor_main.cpp keeps everything in an anonymous namespace, so include it.
// OpenVR symbols are left unresolved, so nothing here may call applyPreset/setSetting.
#define main framecolor_original_main
#include "framecolor_main.cpp"
#undef main

#include <cassert>
#include <iostream>

namespace {

void writeRgba(const std::string& path, const std::vector<uint8_t>& rgba) {
    std::ofstream(path, std::ios::binary).write(reinterpret_cast<const char*>(rgba.data()), rgba.size());
}

std::vector<double> readCurrent() {
    std::ifstream in(currentPath());
    std::vector<double> v;
    for (double x; in >> x;) v.push_back(x);
    return v;
}

void fakeProcess(const std::filesystem::path& root, const char* pid, const std::string& cmdline) {
    std::filesystem::create_directories(root / pid);
    std::ofstream(root / pid / "cmdline", std::ios::binary) << cmdline;
}

}  // namespace

int main(int argc, char** argv) {
    const std::string outDir = argc > 1 ? argv[1] : ".";
    FontSet fonts; fonts.load(kFont, kBold);
    std::vector<uint8_t> rgba; std::vector<Hit> hits;

    // --- Sharpness slider and current.txt (13th value) ---
    UiState ui;
    ui.vulkanLayerActive = true;
    render(fonts, ui, -1, -1, rgba, hits);
    const Hit* hit = hitTest(hits, kTrackX + kTrackW, 680 + 39);
    assert(hit && hit->kind == HitKind::Slider && hit->arg == 5);
    updateSlider(ui, 5, kTrackX + kTrackW); assert(ui.values.sharpen == 1.0);
    updateSlider(ui, 5, kTrackX - 50); assert(ui.values.sharpen == 0.0);
    ui.channel = 2; updateSlider(ui, 5, kTrackX + 0.5 * kTrackW); assert(ui.values.sharpen == 0.5);
    ui.channel = 0;
    assert(saveAdvancedValues(ui));
    { auto v = readCurrent(); assert(v.size() == 14 && v[12] == 0.5 && v[13] == 0.0); }
    Values loaded; loadAdvancedValues(loaded); assert(loaded.sharpen == 0.5);
    { std::ofstream(currentPath()) << "0 0 0 1 1 1 1 1 1 1 1 1\n"; }
    Values fresh; loadAdvancedValues(fresh); assert(fresh.sharpen == 0.0);

    // --- Compare button: hit area, bypass flag in the 14th value ---
    render(fonts, ui, -1, -1, rgba, hits);
    hit = hitTest(hits, 1155 - 160 - 15 - 110, 42);
    assert(hit && hit->kind == HitKind::Compare);
    VrOverlay* noOverlay = nullptr;
    activate(*hit, 0, ui, *noOverlay);  // Compare does not touch the overlay
    assert(ui.compare);
    { auto v = readCurrent(); assert(v.size() == 14 && v[13] == 1.0); }
    ui.compare = false; assert(saveAdvancedValues(ui));
    { auto v = readCurrent(); assert(v[13] == 0.0); }
    UiState noLayer;  // compare is disabled when the Vulkan layer is not installed
    render(fonts, noLayer, -1, -1, rgba, hits);
    hit = hitTest(hits, 1155 - 160 - 15 - 110, 42);
    assert(!hit || hit->kind != HitKind::Compare);

    // --- Presets keep sharpness; old rows load with it off ---
    ui.values.sharpen = 0.5;
    ui.custom[0] = {ui.values, true};
    assert(saveCustomPresets(ui));
    {
        std::string first;
        { std::ifstream in(presetPath()); std::getline(in, first); }
        std::ofstream(presetPath()) << first << "\n1 1 0 1 1 1 0 0 0 1 1 1 1 1 1\n";
    }
    UiState reloaded; loadCustomPresets(reloaded);
    assert(reloaded.custom[0].valid && reloaded.custom[0].values.sharpen == 0.5);
    assert(reloaded.custom[1].valid && reloaded.custom[1].values.sharpen == 0.0);
    for (const Preset& preset : kBuiltins) assert(preset.values.sharpen == 0.0);

    // --- Auto rules: file round trip, priority, cycling ---
    AutoRules rules;
    rules.standalone = 1; rules.streaming = -1; rules.apps["steam.app.620"] = 3;
    assert(saveAutoRules(autoPath(), rules));
    AutoRules back = loadAutoRules(autoPath());
    assert(back.standalone == 1 && back.streaming == -1 && back.apps.at("steam.app.620") == 3);
    rules.apps["system.generated.my game.exe"] = 4;   // app keys may contain spaces
    assert(saveAutoRules(autoPath(), rules));
    back = loadAutoRules(autoPath());
    assert(back.apps.size() == 2 && back.apps.at("system.generated.my game.exe") == 4);
    rules.apps.erase("system.generated.my game.exe");
    { std::ofstream(autoPath()) << "standalone 9\nstreaming x\napp -1 k\napp 2\nbogus 2\n"; }
    back = loadAutoRules(autoPath());
    assert(back.standalone == -1 && back.streaming == -1 && back.apps.empty());
    assert(!std::filesystem::exists(autoPath().string() + ".tmp"));
    assert(resolveAutoPreset(rules, {false, "steam.app.620"}) == 3);   // app wins
    assert(resolveAutoPreset(rules, {false, "other"}) == 1);           // falls back to mode
    assert(resolveAutoPreset(rules, {true, ""}) == -1);
    const bool onlyP2[3] = {false, true, false};
    assert(nextPresetChoice(-1, onlyP2) == 0);
    assert(nextPresetChoice(2, onlyP2) == 4);
    assert(nextPresetChoice(4, onlyP2) == -1);

    UiState autoUi;
    autoUi.custom[0].valid = true;
    autoUi.context = {false, "steam.app.620"};
    autoUi.contextKnown = false;  // no apply here (applyPreset needs OpenVR)
    cycleRule(autoUi, 2);
    assert(autoUi.autoRules.apps.at("steam.app.620") == 0);
    assert(loadAutoRules(autoPath()).apps.at("steam.app.620") == 0);
    for (int i = 0; i < 4; ++i) cycleRule(autoUi, 2);   // 0 -> 1 -> 2 -> 3 (P1) -> off (P2/P3 empty)
    assert(autoUi.autoRules.apps.count("steam.app.620") == 0);
    autoUi.autoRules.apps["steam.app.620"] = 1;
    assert(governingRule(autoUi) == 2);
    autoUi.context.appKey.clear();
    assert(governingRule(autoUi) == 0);
    autoUi.context.streaming = true;
    assert(governingRule(autoUi) == 1);

    // --- Streaming detection from /proc ---
    const std::filesystem::path proc = "/tmp/fakeproc";
    std::filesystem::remove_all(proc);
    fakeProcess(proc, "100", std::string("bash") + '\0');
    fakeProcess(proc, "101", std::string("/usr/bin/vrlinkx") + '\0');
    std::filesystem::create_directories(proc / "self");
    assert(!isStreaming(proc));
    fakeProcess(proc, "102", std::string("/opt/steamvr/bin/vrlink") + '\0' + "--serve" + '\0');
    assert(isStreaming(proc));
    assert(!isStreaming("/tmp/does-not-exist"));

    // --- Bench statistics and /proc readers ---
    assert(percentile({}, 0.5) == 0.0);
    assert(percentile({3, 1, 2}, 0.5) == 2.0);
    assert(percentile({1, 2, 3, 4, 5, 6, 7, 8, 9, 10}, 0.95) == 10.0);
    {
        std::vector<FrameSample> s;
        for (uint32_t i = 1; i <= 90; ++i)
            s.push_back({i, i / 90.0, i == 90 ? 6.0f : 3.0f, 1.0f, i == 45 ? 1u : 0u, 0, i % 30 == 0});
        s.push_back(s[10]);                 // duplicates from overlapping reads
        std::swap(s[0], s[50]);             // out of order
        const BenchSummary b = summarize(s);
        assert(b.frames == 90);
        assert(b.gpuP50 == 3.0 && b.gpuMax == 6.0);
        assert(b.dropped == 1 && b.reprojected == 3);
        assert(b.refreshHz > 89.9 && b.refreshHz < 90.1);
    }
    assert(summarize({}).frames == 0 && summarize({}).refreshHz == 0.0);
    std::filesystem::create_directories(proc / "200");
    std::ofstream(proc / "200" / "comm") << "vrcompositor\n";
    std::ofstream(proc / "200" / "status") << "Name:\tvrcompositor\nVmRSS:\t  409600 kB\nThreads:\t12\n";
    std::filesystem::create_directories(proc / "201");
    std::ofstream(proc / "201" / "comm") << "framecolor\n";
    assert(findProcess(proc, "vrcompositor") == 200);
    assert(findProcess(proc, "framecolor", 201) == -1);
    assert(findProcess(proc, "framecolor") == 201);
    assert(readRssKb(proc, 200) == 409600);
    assert(readRssKb(proc, 201) == -1 && readRssKb(proc, -1) == -1);

    // --- Screenshots ---
    UiState shot;
    shot.vulkanLayerActive = true;
    shot.values.sharpen = 0.35;
    shot.custom[0].valid = true;
    shot.contextKnown = true;
    shot.context = {false, "steam.app.546560"};
    shot.appName = "Half-Life: Alyx";
    shot.autoRules.standalone = 1;
    shot.autoRules.streaming = -1;
    shot.autoRules.apps["steam.app.546560"] = 3;
    shot.message = std::string(getTranslations(shot.language).autoApplied) + "P1";
    render(fonts, shot, -1, -1, rgba, hits);
    writeRgba(outDir + "/panel_en.rgba", rgba);

    shot.language = Language::Japanese;
    shot.compare = true;
    shot.context = {true, ""};
    shot.appName.clear();
    shot.message = getTranslations(shot.language).applied;
    render(fonts, shot, -1, -1, rgba, hits);
    writeRgba(outDir + "/panel_ja.rgba", rgba);

    std::cout << "harness OK, panel " << kWidth << "x" << kHeight << "\n";
    return 0;
}
