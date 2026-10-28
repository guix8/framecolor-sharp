// framecolor --bench: measure compositor frame timing and memory while FrameColor runs.
#pragma once

#include <cstdint>
#include <filesystem>
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

/** Run the benchmark for the given seconds and print the report. Returns the exit code. */
int runBench(double seconds);
