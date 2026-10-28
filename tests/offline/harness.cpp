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
    { auto v = readCurrent(); assert(v.size() == 15 && v[12] == 0.5 && v[13] == 0.0 && v[14] == 0.0); }
    Values loaded; loadAdvancedValues(loaded); assert(loaded.sharpen == 0.5);
    { std::ofstream(currentPath()) << "0 0 0 1 1 1 1 1 1 1 1 1\n"; }
    Values fresh; loadAdvancedValues(fresh); assert(fresh.sharpen == 0.0);
    assert(!loadSharpenWhole());  // older files sharpen the centre of each eye

    // --- Sharpen area button: toggles the 15th value; presets do not change it ---
    render(fonts, ui, -1, -1, rgba, hits);
    hit = hitTest(hits, 232 + 74, 697 + 22);
    assert(hit && hit->kind == HitKind::SharpenArea);
    assert(hitTest(hits, kTrackX - 20, 719)->kind == HitKind::Slider);  // the button stays clear of the track
    {
        activate(*hit, 0, ui, nullptr);
    }
    assert(ui.sharpenWhole);
    { auto v = readCurrent(); assert(v.size() == 15 && v[14] == 1.0); }
    assert(loadSharpenWhole());
    ui.sharpenWhole = false; assert(saveAdvancedValues(ui) && !loadSharpenWhole());

    // --- Auto compare: starts on the original image, stops on its own ---
    render(fonts, ui, -1, -1, rgba, hits);
    hit = hitTest(hits, 1155 - 160 - 15 - 220 - 15 - 100, 42);
    assert(hit && hit->kind == HitKind::AutoCompare);
    {
        activate(*hit, 0, ui, nullptr);
        assert(ui.autoCompare && ui.autoOriginal);
        { auto v = readCurrent(); assert(v[13] == 1.0); }
        assert(ui.autoCompareEnd - ui.autoCompareTurnEnd == kAutoCompareLength - kAutoCompareTurn);
        activate(*hit, 0, ui, nullptr);
        assert(!ui.autoCompare && !ui.autoOriginal);
        { auto v = readCurrent(); assert(v[13] == 0.0); }

        // The timeline: a switch every 3 s, one switch after a stall, off at 60 s
        ui.saveMode = true;
        activate(*hit, 0, ui, nullptr);
        assert(!ui.saveMode);  // starting auto compare leaves save mode
        const auto t0 = ui.autoCompareEnd - kAutoCompareLength;
        assert(!tickAutoCompare(ui, t0 + std::chrono::milliseconds(2900)) && ui.autoOriginal);
        assert(tickAutoCompare(ui, t0 + kAutoCompareTurn) && !ui.autoOriginal);
        assert(!tickAutoCompare(ui, t0 + std::chrono::milliseconds(5900)) && !ui.autoOriginal);
        // After a 10 s stall it switches once and waits a full turn again
        const auto late = t0 + std::chrono::seconds(16);
        assert(tickAutoCompare(ui, late) && ui.autoOriginal);
        assert(!tickAutoCompare(ui, late + std::chrono::milliseconds(250)) && ui.autoOriginal);
        assert(ui.autoCompareTurnEnd == late + kAutoCompareTurn);
        assert(tickAutoCompare(ui, t0 + kAutoCompareLength) && !ui.autoCompare && !ui.autoOriginal);
        assert(!tickAutoCompare(ui, t0 + kAutoCompareLength + kAutoCompareTurn));
        assert(saveAdvancedValues(ui));
        { auto v = readCurrent(); assert(v[13] == 0.0); }
    }

    // --- Compare button: hit area, bypass flag in the 14th value ---
    render(fonts, ui, -1, -1, rgba, hits);
    hit = hitTest(hits, 1155 - 160 - 15 - 110, 42);
    assert(hit && hit->kind == HitKind::Compare);
    activate(*hit, 0, ui, nullptr);
    assert(ui.compare);
    { auto v = readCurrent(); assert(v.size() == 15 && v[13] == 1.0); }
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
    // DRM fdinfo, as the msm driver writes it: each client id counts once, other fds are skipped
    std::filesystem::create_directories(proc / "200" / "fdinfo");
    std::ofstream(proc / "200" / "fdinfo" / "0") << "pos:\t0\nflags:\t02\n";
    std::ofstream(proc / "200" / "fdinfo" / "67")
        << "drm-driver:\tmsm\ndrm-client-id:\t116\ndrm-engine-gpu:\t0 ns\ndrm-cycles-gpu:\t0\n";
    std::ofstream(proc / "200" / "fdinfo" / "69")
        << "drm-driver:\tmsm\ndrm-client-id:\t118\ndrm-engine-gpu:\t171714985445 ns\ndrm-cycles-gpu:\t154256749949\n"
           "drm-maxfreq-gpu:\t903000000 Hz\ndrm-total-memory:\t880204 KiB\ndrm-resident-memory:\t880204 KiB\n";
    std::ofstream(proc / "200" / "fdinfo" / "70")  // a second fd for client 118
        << "drm-driver:\tmsm\ndrm-client-id:\t118\ndrm-engine-gpu:\t171714985445 ns\ndrm-cycles-gpu:\t154256749949\n"
           "drm-resident-memory:\t880204 KiB\n";
    std::ofstream(proc / "200" / "fdinfo" / "71")  // a client without engine time; memory in MiB
        << "drm-driver:\tmsm\ndrm-client-id:\t119\ndrm-resident-memory:\t10 MiB\n";
    const DrmGpuUsage drm = readDrmGpuUsage(proc, 200);
    assert(drm.engineNs.size() == 2 && drm.engineNs.at("118") == 171714985445ull && drm.engineNs.at("116") == 0);
    assert(drm.residentKb == 880204 + 10 * 1024);
    assert(readDrmGpuUsage(proc, 201).engineNs.empty() && readDrmGpuUsage(proc, -1).engineNs.empty());
    assert(drmMemoryKb("880204 KiB") == 880204 && drmMemoryKb("10 MiB") == 10240 && drmMemoryKb("4096") == 4);
    assert(drmMemoryKb("0") == 0 && drmMemoryKb("") == 0);
    // Deltas count only clients present at both readings
    DrmGpuUsage before, after;
    before.engineNs = {{"116", 100}, {"118", 1000}, {"120", 50}};
    after.engineNs = {{"116", 150}, {"118", 1600}, {"121", 99999}};
    assert(drmEngineDeltaNs(before, after) == 650u);
    assert(!drmEngineDeltaNs(DrmGpuUsage{}, after));

    // --- bench --compare: paired statistics and the current.txt it writes ---
    {
        const PairStats p = pairedStats({{2.0, 2.4}, {1.8, 2.3}, {2.2, 2.5}});
        assert(p.pairs == 3 && std::abs(p.meanA - 2.0) < 1e-9 && std::abs(p.meanB - 2.4) < 1e-9);
        assert(std::abs(p.meanDiff - 0.4) < 1e-9);
        // differences 0.4, 0.5, 0.3: sample sd 0.1; 95% interval t(2) * 0.1 / sqrt(3)
        assert(std::abs(p.ci95 - 4.303 * 0.1 / std::sqrt(3.0)) < 1e-9);
        assert(pairedStats({}).pairs == 0 && pairedStats({{1.0, 2.0}}).ci95 == 0.0);
        assert(studentT95(0) == 0.0 && studentT95(11) == 2.201 && studentT95(30) == 2.042);
        assert(studentT95(59) < 2.042 && studentT95(59) > 1.98);
        // Command-line numbers: whole, in range, no leading zeros
        assert(parseIntArg("0", 0, 100) == 0 && parseIntArg("60", 0, 100) == 60 && parseIntArg("100", 0, 100) == 100);
        for (const char* bad : {"", "-5", "101", "010", "0100", "0.6", "6a", "99999999999999999999"})
            assert(!parseIntArg(bad, 0, 100));
        assert(!parseIntArg("4", 5, 60) && parseIntArg("5", 5, 60) == 5 && !parseIntArg(nullptr, 0, 100));
        const auto with = currentWithSharpness("0 0 0 1.12 1.12 1.12 1 1 1 1.2 1.2 1.2 0.6 0 1\n", 0.35);
        assert(with && *with == "0 0 0 1.12 1.12 1.12 1 1 1 1.2 1.2 1.2 0.35 0 1\n");
        assert(*currentWithSharpness("0 0 0 1 1 1 1 1 1 1 1 1\n", 1.0) == "0 0 0 1 1 1 1 1 1 1 1 1 1\n");
        assert(!currentWithSharpness("0 0 0 1 1 1\n", 0.5) && !currentWithSharpness("", 0.5));
        assert(!currentWithSharpness("0 0 0 1 1 1 1 1 1 1 1 nan\n", 0.5));
    }

    // --- Language codes and automatic detection ---
    assert(matchLanguage("koreana") == Language::Korean);
    assert(matchLanguage("ko_KR.UTF-8") == Language::Korean);
    assert(matchLanguage("ja_JP.UTF-8") == Language::Japanese);
    assert(matchLanguage("english") == Language::English);
    assert(!matchLanguage("french") && !matchLanguage("C.UTF-8") && !matchLanguage("kor"));
    assert(languageFromCode("schinese") == Language::English);
    {
        const std::filesystem::path home = std::filesystem::temp_directory_path() / "framecolor-lang-test";
        std::filesystem::remove_all(home);
        std::filesystem::create_directories(home / ".steam");
        const std::string oldHome = std::getenv("HOME") ? std::getenv("HOME") : "";
        setenv("HOME", home.c_str(), 1);
        unsetenv("LC_ALL");
        unsetenv("LC_MESSAGES");
        setenv("LANG", "en_US.UTF-8", 1);
        // Steam language wins over LANG
        std::ofstream(home / ".steam/registry.vdf")
            << "\"Registry\"\n{\n\t\"HKCU\"\n\t{\n\t\t\"Software\"\n\t\t{\n\t\t\t\"Valve\"\n\t\t\t{\n"
               "\t\t\t\t\"Steam\"\n\t\t\t\t{\n\t\t\t\t\t\"language\"\t\t\"koreana\"\n\t\t\t\t}\n\t\t\t}\n\t\t}\n\t}\n}\n";
        assert(detectLanguage() == Language::Korean);
        // An unsupported Steam language falls back to the locale
        std::ofstream(home / ".steam/registry.vdf") << "\t\t\t\t\t\"language\"\t\t\"french\"\n";
        setenv("LANG", "ja_JP.UTF-8", 1);
        assert(detectLanguage() == Language::Japanese);
        setenv("LC_ALL", "ko_KR.UTF-8", 1);
        assert(detectLanguage() == Language::Korean);
        unsetenv("LC_ALL");
        std::filesystem::remove(home / ".steam/registry.vdf");
        setenv("LANG", "C.UTF-8", 1);
        assert(detectLanguage() == Language::English);

        // language.txt: missing or "auto" follows detection, a code is kept
        const auto config = home / "config";
        std::filesystem::create_directories(config);
        setenv("LANG", "ko_KR.UTF-8", 1);
        assert(languageIsAuto(config) && loadSavedLanguage(config) == Language::Korean);
        assert(saveLanguage(config, Language::Japanese));
        assert(!languageIsAuto(config) && loadSavedLanguage(config) == Language::Japanese);
        assert(saveAutoLanguage(config));
        assert(languageIsAuto(config) && loadSavedLanguage(config) == Language::Korean);

        setenv("HOME", oldHome.c_str(), 1);
        unsetenv("LANG");
        std::filesystem::remove_all(home);
    }
    // Every string of every language is set (a null one would crash std::string)
    for (Language language : availableLanguages()) {
        const auto& t = getTranslations(language);
        assert(t.code == languageCode(language));
        const std::vector<const char*> strings{
            t.code, t.displayName, t.languageAuto, t.title, t.dashboardName, t.builtinPresets[0],
            t.builtinPresets[1], t.builtinPresets[2], t.emptySlotSuffix, t.save, t.selectSlot, t.reset,
            t.resetConfirm, t.panelBrightness, t.adjustmentChannel, t.channelNames[0], t.channelNames[1],
            t.channelNames[2], t.channelNames[3], t.brightness, t.saturation, t.contrast, t.gamma, t.sharpness,
            t.compare, t.compareHeld, t.autoCompare, t.autoCompareShowingOriginal, t.autoCompareShowingCorrected,
            t.autoCompareOn, t.autoCompareOff, t.autoCompareEnded, t.sharpenCenter, t.sharpenWhole, t.autoPreset,
            t.standalone, t.streaming, t.thisApp, t.autoOff, t.noApp, t.nowPrefix, t.autoApplied, t.autoRestored,
            t.ruleSaved, t.errorSaveSettings, t.applied, t.errorSaveAdvanced, t.errorSavePreset, t.slotEmpty,
            t.presetApplied, t.promptSaveSlot, t.saveCanceled, t.savedToSlotPrefix, t.savedToSlotSuffix};
        for (const char* s : strings) assert(s != nullptr);
        // The struct has no other members: a new field must be added to the list above
        static_assert(sizeof(Translations) == 54 * sizeof(const char*), "update the string list");
    }

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

    // Korean, with the language menu open (Auto is selected)
    shot.language = Language::Korean;
    shot.languageAuto = true;
    shot.languageOpen = true;
    shot.compare = false;
    shot.context = {false, "steam.app.620980"};
    shot.appName = "Beat Saber";
    shot.message = getTranslations(shot.language).ruleSaved;
    render(fonts, shot, -1, -1, rgba, hits);
    writeRgba(outDir + "/panel_ko.rgba", rgba);
    shot.languageOpen = false;

    // Korean, auto compare running and whole-image sharpening
    shot.autoCompare = true;
    shot.sharpenWhole = true;
    shot.message = getTranslations(shot.language).autoCompareOn;
    render(fonts, shot, -1, -1, rgba, hits);
    writeRgba(outDir + "/panel_ko_auto.rgba", rgba);

    // Japanese labels are the longest; check they fit (showing the original this time)
    shot.language = Language::Japanese;
    shot.autoOriginal = true;
    shot.message = getTranslations(shot.language).autoCompareOn;
    render(fonts, shot, -1, -1, rgba, hits);
    writeRgba(outDir + "/panel_ja_auto.rgba", rgba);

    // English has the longest auto compare message
    shot.language = Language::English;
    shot.message = getTranslations(shot.language).autoCompareOn;
    render(fonts, shot, -1, -1, rgba, hits);
    writeRgba(outDir + "/panel_en_auto.rgba", rgba);

    std::cout << "harness OK, panel " << kWidth << "x" << kHeight << "\n";
    return 0;
}
