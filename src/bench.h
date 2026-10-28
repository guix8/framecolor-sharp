// framecolor --bench: measure compositor frame timing and memory while FrameColor runs.
#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

/** One compositor frame, copied from vr::Compositor_FrameTiming. */
struct FrameSample {
    uint32_t index = 0;
    double systemTime = 0.0;     ///< seconds
    float compositorGpuMs = 0.0f;
    float compositorCpuMs = 0.0f;
    uint32_t dropped = 0;        ///< extra scan-outs of the previous frame
    uint32_t mispresented = 0;
    bool reprojected = false;    ///< async or motion reprojection
};

struct BenchSummary {
    size_t frames = 0;
    double refreshHz = 0.0;      ///< from the median frame interval
    double gpuP50 = 0.0, gpuP95 = 0.0, gpuMax = 0.0;
    double cpuP50 = 0.0, cpuP95 = 0.0;
    uint32_t dropped = 0, mispresented = 0, reprojected = 0;
};

/** Nearest-rank percentile; p in 0..1. Returns 0 for no values. */
double percentile(std::vector<double> values, double p);

/** Sort by frame index, drop duplicates and summarize. */
BenchSummary summarize(std::vector<FrameSample> samples);

/** PID of the first process whose /proc/PID/comm equals name (not excludePid), or -1. */
int findProcess(const std::filesystem::path& procRoot, const std::string& name, int excludePid = -1);

/** VmRSS of a process in KiB, or -1. */
long readRssKb(const std::filesystem::path& procRoot, int pid);

/**
 * GPU use of one process from the DRM fdinfo files in /proc/PID/fdinfo, summed over
 * its DRM clients. Unlike the compositor GPU time from SteamVR, this includes work that
 * a Vulkan layer adds inside the process, such as FrameColor's pass in vrcompositor.
 */
struct DrmGpuUsage {
    std::map<std::string, uint64_t> engineNs;  ///< drm-engine-gpu busy time per drm-client-id, ns
    uint64_t residentKb = 0;      ///< drm-resident-memory summed over clients, KiB (shared memory may count twice)
};

/** Read DrmGpuUsage for a process; each drm-client-id is counted once. */
DrmGpuUsage readDrmGpuUsage(const std::filesystem::path& procRoot, int pid);

/**
 * GPU busy time between two readings, summed over the clients present in both, so a
 * client that opens or closes during the run does not distort it. Empty if none are.
 */
std::optional<uint64_t> drmEngineDeltaNs(const DrmGpuUsage& start, const DrmGpuUsage& end);

/** A DRM fdinfo memory value such as "880204 KiB", "10 MiB" or "4096" (bytes), in KiB. */
uint64_t drmMemoryKb(const std::string& value);

/** Means of paired measurements (a, b) and the mean difference b - a with its 95% half-interval. */
struct PairStats {
    size_t pairs = 0;
    double meanA = 0.0, meanB = 0.0, meanDiff = 0.0, ci95 = 0.0;
};
PairStats pairedStats(const std::vector<std::pair<double, double>>& samples);

/** Two-sided 95% critical value of Student's t for df degrees of freedom (0 for df 0). */
double studentT95(size_t df);

/**
 * current.txt with the sharpness (13th value) replaced, or nothing if the text is not a
 * current.txt with at least 12 values (the dashboard writes 15).
 */
std::optional<std::string> currentWithSharpness(const std::string& text, double sharpness);

/**
 * Compare two sharpness values in the running scene: switch between them every 2 s and
 * compare vrcompositor's GPU time in neighbouring intervals. Restores current.txt at the end.
 */
int runBenchCompare(double sharpnessA, double sharpnessB, int pairs);

/** Run the benchmark for the given seconds and print the report. Returns the exit code. */
int runBench(double seconds);
