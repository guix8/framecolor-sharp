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

uint64_t drmMemoryKb(const std::string& value) {
    // The kernel prints the size with the largest exact unit: none (bytes), KiB or MiB.
    std::istringstream in(value);
    uint64_t number = 0;
    std::string unit;
    if (!(in >> number)) return 0;
    in >> unit;
    if (unit == "KiB") return number;
    if (unit == "MiB") return number * 1024;
    if (unit == "GiB") return number * 1024 * 1024;
    return number / 1024;
}

DrmGpuUsage readDrmGpuUsage(const std::filesystem::path& procRoot, int pid) {
    DrmGpuUsage usage;
    if (pid < 0) return usage;
    std::vector<std::string> clients;  // a DRM client can be open on several fds
    std::error_code ec;
    for (std::filesystem::directory_iterator it(procRoot / std::to_string(pid) / "fdinfo", ec), end; !ec && it != end;
         it.increment(ec)) {
        std::ifstream in(it->path());
        std::string line, client;
        std::optional<uint64_t> engineNs;
        uint64_t residentKb = 0;
        while (std::getline(in, line)) {
            const auto colon = line.find(':');
            if (colon == std::string::npos) continue;
            const std::string key = line.substr(0, colon);
            const std::string rest = line.substr(colon + 1);
            std::istringstream value(rest);
            uint64_t number = 0;
            if (key == "drm-client-id") value >> client;
            else if (key == "drm-engine-gpu" && value >> number) engineNs = number;
            else if (key == "drm-resident-memory") residentKb = drmMemoryKb(rest);
        }
        if (client.empty() || std::find(clients.begin(), clients.end(), client) != clients.end()) continue;
        clients.push_back(client);
        if (engineNs) usage.engineNs[client] = *engineNs;
        usage.residentKb += residentKb;
    }
    return usage;
}

std::optional<uint64_t> drmEngineDeltaNs(const DrmGpuUsage& start, const DrmGpuUsage& end) {
    std::optional<uint64_t> total;
    for (const auto& [client, endNs] : end.engineNs) {
        const auto before = start.engineNs.find(client);
        if (before == start.engineNs.end() || endNs < before->second) continue;
        total = total.value_or(0) + (endNs - before->second);
    }
    return total;
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

std::vector<double> currentValues() {
    std::ifstream in(configDir() / "current.txt");
    std::vector<double> v;
    for (double x; in >> x;) v.push_back(x);
    return v;
}

bool compareActive() {
    const std::vector<double> v = currentValues();
    return v.size() >= 14 && v[13] > 0.5;
}

// What FrameColor was doing during the run, so results can be compared.
std::string settingsLine(bool compareSeen) {
    const auto layer = dataDir() / "layer";
    const char* layerState = std::filesystem::exists(layer / "vkBasalt.json") ? "enabled"
        : std::filesystem::exists(layer / "vkBasalt.json.disabled") ? "disabled" : "not installed";
    const std::vector<double> v = currentValues();
    const std::vector<double> neutral{0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1};
    bool colorNeutral = true;
    for (size_t i = 0; i < std::min(v.size(), neutral.size()); ++i) colorNeutral = colorNeutral && v[i] == neutral[i];
    char text[200];
    std::snprintf(text, sizeof(text), "Vulkan correction: %s | color: %s | sharpness: %.0f%% (%s)%s", layerState,
                  colorNeutral ? "neutral" : "adjusted", v.size() >= 13 ? v[12] * 100.0 : 0.0,
                  v.size() >= 15 && v[14] > 0.5 ? "whole image" : "center",
                  compareSeen ? " | COMPARE was on during the run (correction bypassed part of the time)" : "");
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
    const int compositorPid = findProcess("/proc", "vrcompositor");
    const DrmGpuUsage gpuStart = readDrmGpuUsage("/proc", compositorPid);
    const auto start = std::chrono::steady_clock::now();
    bool compareSeen = false;  // hold or auto compare switches the correction off part of the time
    // The compositor keeps a short history, so read it every 0.5 s.
    while (std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count() < seconds) {
        compareSeen = compareSeen || compareActive();
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
    const DrmGpuUsage gpuEnd = readDrmGpuUsage("/proc", compositorPid);
    const double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    vr::VR_Shutdown();

    const BenchSummary s = summarize(samples);
    std::printf("\nFrameColor bench (%.0f s, %s)\n", seconds, isStreaming() ? "PC streaming" : "standalone");
    std::printf("  %s\n", settingsLine(compareSeen || compareActive()).c_str());
    if (s.frames == 0) {
        std::printf("  No frame timing was returned. Is a scene running?\n");
        return 1;
    }
    std::printf("  frames                 %zu (%.1f Hz)\n", s.frames, s.refreshHz);
    // The per-process time includes FrameColor's pass; SteamVR's compositor GPU time does not.
    const std::optional<uint64_t> busy = drmEngineDeltaNs(gpuStart, gpuEnd);
    if (busy && elapsed > 0.0 && s.refreshHz > 0.0) {
        const double busyNs = static_cast<double>(*busy);
        const double frames = elapsed * s.refreshHz;
        std::printf("  vrcompositor GPU       %.2f ms/frame, %.1f%% busy (includes FrameColor)\n", busyNs / 1e6 / frames,
                    100.0 * busyNs / 1e9 / elapsed);
    } else {
        std::printf("  vrcompositor GPU       not available (no drm-engine-gpu in vrcompositor's fdinfo)\n");
    }
    std::printf("  compositor GPU ms      p50 %.2f  p95 %.2f  max %.2f (SteamVR; excludes FrameColor)\n", s.gpuP50,
                s.gpuP95, s.gpuMax);
    std::printf("  compositor CPU ms      p50 %.2f  p95 %.2f\n", s.cpuP50, s.cpuP95);
    std::printf("  dropped %u  mispresented %u  reprojected %u (%.1f%%)\n", s.dropped, s.mispresented, s.reprojected,
                100.0 * s.reprojected / s.frames);
    std::printf("Memory (RSS; GPU allocations may not be counted in it)\n");
    printMemory("vrcompositor", compositorPid);
    if (gpuEnd.residentKb > 0)
        std::printf("  vrcompositor GPU mem   %.1f MiB resident (sum over DRM clients)\n", gpuEnd.residentKb / 1024.0);
    printMemory("framecolor dashboard", findProcess("/proc", "framecolor", static_cast<int>(getpid())));
    return 0;
}
