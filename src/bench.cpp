#include "bench.h"

#include "auto_profile.h"
#include "openvr.h"

#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <thread>

double percentile(std::vector<double> values, double p) {
    if (values.empty()) return 0.0;
    std::sort(values.begin(), values.end());
    const size_t i = std::min(values.size() - 1, static_cast<size_t>(p * (values.size() - 1) + 0.5));
    return values[i];
}

BenchSummary summarize(std::vector<FrameSample> samples) {
    std::sort(samples.begin(), samples.end(), [](const FrameSample& a, const FrameSample& b) { return a.index < b.index; });
    samples.erase(std::unique(samples.begin(), samples.end(),
                              [](const FrameSample& a, const FrameSample& b) { return a.index == b.index; }),
                  samples.end());
    BenchSummary s;
    s.frames = samples.size();
    std::vector<double> gpu, cpu, interval;
    for (size_t i = 0; i < samples.size(); ++i) {
        const FrameSample& f = samples[i];
        gpu.push_back(f.compositorGpuMs);
        cpu.push_back(f.compositorCpuMs);
        s.gpuMax = std::max(s.gpuMax, static_cast<double>(f.compositorGpuMs));
        s.dropped += f.dropped;
        s.mispresented += f.mispresented;
        if (f.reprojected) ++s.reprojected;
        if (i > 0 && f.index > samples[i - 1].index)
            interval.push_back((f.systemTime - samples[i - 1].systemTime) / (f.index - samples[i - 1].index));
    }
    s.gpuP50 = percentile(gpu, 0.50);
    s.gpuP95 = percentile(gpu, 0.95);
    s.cpuP50 = percentile(cpu, 0.50);
    s.cpuP95 = percentile(cpu, 0.95);
    const double medianInterval = percentile(interval, 0.50);
    s.refreshHz = medianInterval > 0.0 ? 1.0 / medianInterval : 0.0;
    return s;
}

int findProcess(const std::filesystem::path& procRoot, const std::string& name, int excludePid) {
    std::error_code ec;
    for (std::filesystem::directory_iterator it(procRoot, ec), end; !ec && it != end; it.increment(ec)) {
        const std::string pidText = it->path().filename().string();
        if (pidText.empty() || pidText.find_first_not_of("0123456789") != std::string::npos) continue;
        const int pid = std::atoi(pidText.c_str());
        if (pid == excludePid) continue;
        std::ifstream comm(it->path() / "comm");
        std::string value;
        if (std::getline(comm, value) && value == name) return pid;
    }
    return -1;
}

long readRssKb(const std::filesystem::path& procRoot, int pid) {
    if (pid < 0) return -1;
    std::ifstream status(procRoot / std::to_string(pid) / "status");
    std::string line;
    while (std::getline(status, line)) {
        if (line.rfind("VmRSS:", 0) == 0) return std::atol(line.c_str() + 6);
    }
    return -1;
}

namespace {

std::filesystem::path configDir() {
    const char* config = std::getenv("XDG_CONFIG_HOME");
    if (config != nullptr && config[0] != '\0') return std::filesystem::path(config) / "framecolor";
    const char* home = std::getenv("HOME");
    return std::filesystem::path(home != nullptr ? home : "/tmp") / ".config" / "framecolor";
}

std::filesystem::path dataDir() {
    const char* data = std::getenv("XDG_DATA_HOME");
    if (data != nullptr && data[0] != '\0') return std::filesystem::path(data) / "framecolor";
    const char* home = std::getenv("HOME");
    return std::filesystem::path(home != nullptr ? home : "/tmp") / ".local" / "share" / "framecolor";
}

// What FrameColor was doing during the run, so results can be compared.
std::string settingsLine() {
    const auto layer = dataDir() / "layer";
    const char* layerState = std::filesystem::exists(layer / "vkBasalt.json") ? "enabled"
        : std::filesystem::exists(layer / "vkBasalt.json.disabled") ? "disabled" : "not installed";
    std::ifstream in(configDir() / "current.txt");
    std::vector<double> v;
    for (double x; in >> x;) v.push_back(x);
    const std::vector<double> neutral{0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1};
    bool colorNeutral = true;
    for (size_t i = 0; i < std::min(v.size(), neutral.size()); ++i) colorNeutral = colorNeutral && v[i] == neutral[i];
    char text[160];
    std::snprintf(text, sizeof(text), "Vulkan correction: %s | color: %s | sharpness: %.0f%%%s", layerState,
                  colorNeutral ? "neutral" : "adjusted", v.size() >= 13 ? v[12] * 100.0 : 0.0,
                  v.size() >= 14 && v[13] > 0.5 ? " | COMPARE HELD (correction bypassed)" : "");
    return text;
}

void printMemory(const char* label, int pid) {
    const long kb = readRssKb("/proc", pid);
    if (kb < 0) std::printf("  %-22s not running\n", label);
    else std::printf("  %-22s %.1f MiB RSS (pid %d)\n", label, kb / 1024.0, pid);
}

}  // namespace

int runBench(double seconds) {
    vr::EVRInitError error = vr::VRInitError_None;
    vr::VR_Init(&error, vr::VRApplication_Background);  // never starts SteamVR
    if (error != vr::VRInitError_None) {
        std::fprintf(stderr, "SteamVR is not available: %s\n", vr::VR_GetVRInitErrorAsEnglishDescription(error));
        return 1;
    }
    vr::IVRCompositor* compositor = vr::VRCompositor();
    if (compositor == nullptr) {
        std::fprintf(stderr, "IVRCompositor is not available\n");
        vr::VR_Shutdown();
        return 1;
    }

    std::printf("Measuring for %.0f s. Keep the headset on and the scene still; close the dashboard.\n", seconds);
    std::fflush(stdout);
    std::vector<FrameSample> samples;
    const auto start = std::chrono::steady_clock::now();
    // The compositor keeps a short history, so read it every 0.5 s.
    while (std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count() < seconds) {
        vr::Compositor_FrameTiming timing[128];
        for (auto& t : timing) t.m_nSize = sizeof(vr::Compositor_FrameTiming);
        const uint32_t count = compositor->GetFrameTimings(timing, 128);
        for (uint32_t i = 0; i < count; ++i) {
            const auto& t = timing[i];
            samples.push_back({t.m_nFrameIndex, t.m_flSystemTimeInSeconds, t.m_flCompositorRenderGpuMs,
                               t.m_flCompositorRenderCpuMs, t.m_nNumDroppedFrames, t.m_nNumMisPresented,
                               (t.m_nReprojectionFlags & (vr::VRCompositor_ReprojectionAsync |
                                                          vr::VRCompositor_ReprojectionMotion)) != 0});
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    vr::VR_Shutdown();

    const BenchSummary s = summarize(samples);
    std::printf("\nFrameColor bench (%.0f s, %s)\n", seconds, isStreaming() ? "PC streaming" : "standalone");
    std::printf("  %s\n", settingsLine().c_str());
    if (s.frames == 0) {
        std::printf("  No frame timing was returned. Is a scene running?\n");
        return 1;
    }
    std::printf("  frames                 %zu (%.1f Hz)\n", s.frames, s.refreshHz);
    std::printf("  compositor GPU ms      p50 %.2f  p95 %.2f  max %.2f\n", s.gpuP50, s.gpuP95, s.gpuMax);
    std::printf("  compositor CPU ms      p50 %.2f  p95 %.2f\n", s.cpuP50, s.cpuP95);
    std::printf("  dropped %u  mispresented %u  reprojected %u (%.1f%%)\n", s.dropped, s.mispresented, s.reprojected,
                100.0 * s.reprojected / s.frames);
    std::printf("Memory (GPU allocations may not be counted in RSS)\n");
    printMemory("vrcompositor", findProcess("/proc", "vrcompositor"));
    printMemory("framecolor dashboard", findProcess("/proc", "framecolor", static_cast<int>(getpid())));
    return 0;
}
