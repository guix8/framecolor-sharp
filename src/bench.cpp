#include "bench.h"

#include "auto_profile.h"
#include "openvr.h"

#include <fcntl.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <cerrno>
#include <cmath>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iterator>
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

PairStats pairedStats(const std::vector<std::pair<double, double>>& samples) {
    PairStats s;
    s.pairs = samples.size();
    if (samples.empty()) return s;
    for (const auto& [a, b] : samples) {
        s.meanA += a;
        s.meanB += b;
    }
    s.meanA /= s.pairs;
    s.meanB /= s.pairs;
    s.meanDiff = s.meanB - s.meanA;
    if (s.pairs >= 2) {
        double variance = 0.0;
        for (const auto& [a, b] : samples) variance += std::pow((b - a) - s.meanDiff, 2);
        variance /= s.pairs - 1;
        s.ci95 = studentT95(s.pairs - 1) * std::sqrt(variance / s.pairs);
    }
    return s;
}

double studentT95(size_t df) {
    // Two-sided 95% critical values of Student's t; with few pairs 2 * SE would be far too narrow.
    static const double table[] = {0,     12.706, 4.303, 3.182, 2.776, 2.571, 2.447, 2.365, 2.306, 2.262, 2.228,
                                   2.201, 2.179,  2.160, 2.145, 2.131, 2.120, 2.110, 2.101, 2.093, 2.086, 2.080,
                                   2.074, 2.069,  2.064, 2.060, 2.056, 2.052, 2.048, 2.045, 2.042};
    if (df == 0) return 0.0;
    if (df <= 30) return table[df];
    return df <= 40 ? 2.030 : (df <= 60 ? 2.010 : 1.990);
}

std::optional<std::string> currentWithSharpness(const std::string& text, double sharpness) {
    std::istringstream in(text);
    std::vector<double> values;
    for (double value; in >> value;) values.push_back(value);
    if (values.size() < 12 || !std::all_of(values.begin(), values.end(), [](double v) { return std::isfinite(v); }))
        return std::nullopt;
    if (values.size() < 13) values.resize(13, 0.0);
    values[12] = sharpness;
    std::ostringstream out;
    out << std::setprecision(9);
    for (size_t i = 0; i < values.size(); ++i) out << (i ? " " : "") << values[i];
    out << '\n';
    return out.str();
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
    // GetFrameTimings keeps returning its history when the headset is off: compare the
    // newest frame index of the first and the last read.
    std::optional<uint32_t> newestFirst;
    uint32_t newestLast = 0;
    // The compositor keeps a short history, so read it every 0.5 s.
    while (std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count() < seconds) {
        compareSeen = compareSeen || compareActive();
        vr::Compositor_FrameTiming timing[128];
        for (auto& t : timing) t.m_nSize = sizeof(vr::Compositor_FrameTiming);
        const uint32_t count = compositor->GetFrameTimings(timing, 128);
        uint32_t newest = 0;
        for (uint32_t i = 0; i < count; ++i) newest = std::max(newest, timing[i].m_nFrameIndex);
        if (!newestFirst) newestFirst = newest;
        newestLast = newest;
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
    if (!newestFirst || newestLast <= *newestFirst) {
        std::printf("  No frames were presented during the run (headset off?). SteamVR's frame timing below is from before the run.\n");
    } else if (busy && elapsed > 0.0 && s.refreshHz > 0.0) {
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

namespace {

volatile std::sig_atomic_t gCompareStop = 0;
void compareStopHandler(int) { gCompareStop = 1; }

std::optional<std::string> readFile(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return std::nullopt;
    std::ostringstream text;
    text << in.rdbuf();
    return text.str();
}

// Write to a temporary file, flush it to disk, then rename it over path, so the layer and
// the dashboard never see a partial file and a full disk cannot leave an empty one.
bool writeFileAtomically(const std::filesystem::path& path, const std::string& text) {
    const std::string temporary = path.string() + ".bench.tmp";
    const int fd = ::open(temporary.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
    if (fd < 0) return false;
    bool ok = true;
    for (size_t done = 0; ok && done < text.size();) {
        const ssize_t n = ::write(fd, text.data() + done, text.size() - done);
        if (n > 0) done += static_cast<size_t>(n);
        else ok = n < 0 && errno == EINTR;
    }
    ok = ok && ::fsync(fd) == 0;
    ok = ::close(fd) == 0 && ok;
    std::error_code ec;
    if (ok) std::filesystem::rename(temporary, path, ec);
    if (!ok || ec) {
        std::filesystem::remove(temporary, ec);
        return false;
    }
    return true;
}

// Newest frame index in the compositor's timing history (0 if there is none).
uint32_t newestFrame(vr::IVRCompositor* compositor) {
    vr::Compositor_FrameTiming timing[128];
    for (auto& t : timing) t.m_nSize = sizeof(vr::Compositor_FrameTiming);
    const uint32_t count = compositor->GetFrameTimings(timing, 128);
    uint32_t newest = 0;
    for (uint32_t i = 0; i < count; ++i) newest = std::max(newest, timing[i].m_nFrameIndex);
    return newest;
}

enum class WindowResult { Ok, Stopped, ChangedByOther, WriteFailed, NoFrames, NoGpuTime };

}  // namespace

int runBenchCompare(double sharpnessA, double sharpnessB, int pairs) {
    const auto path = configDir() / "current.txt";
    const auto backup = configDir() / "current.txt.bench-backup";
    if (std::filesystem::exists(backup)) {
        std::fprintf(stderr,
                     "A previous bench --compare did not finish. Your settings from before it are in\n  %s\n"
                     "Check them, then restore them with: mv %s %s\n(or delete that file to keep the current ones).\n",
                     backup.c_str(), backup.c_str(), path.c_str());
        return 1;
    }
    const std::optional<std::string> original = readFile(path);
    if (!original || !currentWithSharpness(*original, sharpnessA)) {
        std::fprintf(stderr, "%s is missing or old. Open the dashboard once so it saves the current format.\n",
                     path.c_str());
        return 1;
    }
    if (compareActive()) {
        std::fprintf(stderr, "Compare or auto compare is on. Turn it off first.\n");
        return 1;
    }

    vr::EVRInitError error = vr::VRInitError_None;
    vr::VR_Init(&error, vr::VRApplication_Background);  // never starts SteamVR
    if (error != vr::VRInitError_None) {
        std::fprintf(stderr, "SteamVR is not available: %s\n", vr::VR_GetVRInitErrorAsEnglishDescription(error));
        return 1;
    }
    vr::IVRCompositor* compositor = vr::VRCompositor();
    const int compositorPid = findProcess("/proc", "vrcompositor");
    // FrameColor's layer only runs while the compositor presents frames. GetFrameTimings keeps
    // returning its history when the headset is off, so the newest frame index and
    // vrcompositor's GPU time must both advance.
    bool presenting = false;
    if (compositor != nullptr) {
        const DrmGpuUsage gpuBefore = readDrmGpuUsage("/proc", compositorPid);
        const uint32_t frameBefore = newestFrame(compositor);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        const auto busy = drmEngineDeltaNs(gpuBefore, readDrmGpuUsage("/proc", compositorPid));
        if (!busy) {
            std::fprintf(stderr, "vrcompositor has no drm-engine-gpu in its fdinfo; cannot measure.\n");
            vr::VR_Shutdown();
            return 1;
        }
        presenting = newestFrame(compositor) > frameBefore && *busy > 0;
    }
    if (!presenting) {
        std::fprintf(stderr, "No frames are being presented. Put the headset on and start a scene.\n");
        vr::VR_Shutdown();
        return 1;
    }
    // Keep the settings from before the run on disk, so an unclean exit can be undone.
    if (!writeFileAtomically(backup, *original)) {
        std::fprintf(stderr, "Could not write %s; nothing was changed.\n", backup.c_str());
        vr::VR_Shutdown();
        return 1;
    }

    std::printf("Comparing sharpness %.0f%% and %.0f%%: %d pairs, 2 s per value (about %d s).\n"
                "Keep the headset on and the scene still, and do not touch the dashboard.\n",
                sharpnessA * 100.0, sharpnessB * 100.0, pairs, pairs * 5);
    std::fflush(stdout);
    gCompareStop = 0;
    const int signals[] = {SIGINT, SIGTERM, SIGHUP, SIGQUIT};
    std::vector<void (*)(int)> oldHandlers;
    for (int s : signals) oldHandlers.push_back(std::signal(s, compareStopHandler));

    std::string lastWritten = *original;
    // One window: check that nothing else (the dashboard) changed the file, set the sharpness,
    // let the layer pick it up (it rereads the file every 50 ms), then measure for 2 s.
    auto window = [&](double sharpness, double& msPerFrame) -> WindowResult {
        if (gCompareStop) return WindowResult::Stopped;
        if (readFile(path) != lastWritten) return WindowResult::ChangedByOther;
        const std::string text = *currentWithSharpness(*original, sharpness);
        if (!writeFileAtomically(path, text)) return WindowResult::WriteFailed;
        lastWritten = text;
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        const DrmGpuUsage before = readDrmGpuUsage("/proc", compositorPid);
        const uint32_t frameBefore = newestFrame(compositor);
        std::this_thread::sleep_for(std::chrono::seconds(2));
        const DrmGpuUsage after = readDrmGpuUsage("/proc", compositorPid);
        const uint32_t frameAfter = newestFrame(compositor);
        if (gCompareStop) return WindowResult::Stopped;
        if (frameAfter <= frameBefore) return WindowResult::NoFrames;
        const auto busy = drmEngineDeltaNs(before, after);
        if (!busy) return WindowResult::NoGpuTime;
        msPerFrame = *busy / 1e6 / (frameAfter - frameBefore);  // per frame actually presented
        return WindowResult::Ok;
    };
    std::vector<std::pair<double, double>> results;
    WindowResult result = WindowResult::Ok;
    for (int i = 0; i < pairs && result == WindowResult::Ok; ++i) {
        // Alternate the order (AB, BA, ...) so slow drift does not favour either value.
        double a = 0.0, b = 0.0;
        result = i % 2 == 0 ? window(sharpnessA, a) : window(sharpnessB, b);
        if (result == WindowResult::Ok) result = i % 2 == 0 ? window(sharpnessB, b) : window(sharpnessA, a);
        if (result == WindowResult::Ok) results.emplace_back(a, b);
    }

    // Restore, unless something else wrote the file in the meantime: then its content wins.
    bool restored = false;
    const bool changedByOther = readFile(path) != lastWritten;
    if (!changedByOther) restored = writeFileAtomically(path, *original);
    if (restored || (changedByOther && lastWritten == *original)) std::filesystem::remove(backup);
    for (size_t i = 0; i < std::size(signals); ++i) std::signal(signals[i], oldHandlers[i]);
    vr::VR_Shutdown();

    switch (result) {
        case WindowResult::Stopped: std::printf("Stopped early.\n"); break;
        case WindowResult::ChangedByOther:
            std::fprintf(stderr, "Something else (the dashboard?) changed %s during the run, so the run "
                                 "stopped.\n", path.c_str());
            break;
        case WindowResult::WriteFailed: std::fprintf(stderr, "Could not write %s.\n", path.c_str()); break;
        case WindowResult::NoFrames:
            std::fprintf(stderr, "Frames stopped being presented (headset taken off?), so the run stopped.\n");
            break;
        case WindowResult::NoGpuTime: std::fprintf(stderr, "Could not read vrcompositor's GPU time.\n"); break;
        case WindowResult::Ok: break;
    }
    if (changedByOther && lastWritten != *original)
        std::fprintf(stderr, "Left %s as the other program wrote it. Your settings from before the run are in %s.\n",
                     path.c_str(), backup.c_str());
    else if (!changedByOther && !restored)
        std::fprintf(stderr, "Could not restore %s. Your settings from before the run are in %s.\n", path.c_str(),
                     backup.c_str());

    const PairStats s = pairedStats(results);
    if (s.pairs == 0) return 1;
    std::printf("\nFrameColor bench --compare (%zu pairs%s)\n", s.pairs,
                result == WindowResult::Ok ? "" : ", incomplete");
    std::printf("  sharpness %3.0f%%       %.2f ms/frame (vrcompositor GPU, includes FrameColor)\n",
                sharpnessA * 100.0, s.meanA);
    std::printf("  sharpness %3.0f%%       %.2f ms/frame\n", sharpnessB * 100.0, s.meanB);
    if (s.pairs >= 2)
        std::printf("  difference          %+.2f ms/frame (95%% interval ±%.2f)\n", s.meanDiff, s.ci95);
    else
        std::printf("  difference          %+.2f ms/frame (one pair; no interval)\n", s.meanDiff);
    const std::vector<double> now = currentValues();
    std::printf("  sharpen area        %s\n", now.size() >= 15 && now[14] > 0.5 ? "whole image" : "center");
    return result == WindowResult::Ok && restored ? 0 : 1;
}
