#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

#include "../app/ITriageQueue.h"

#if defined(TRIAGE_A)
#include "../src/structA/structure_A.cpp"
using Queue = Min_Heap;
static const char* implementation = "A";
#elif defined(TRIAGE_B)
#include "../src/structB/structureB.cpp"
using Queue = LazyTriageQueue;
static const char* implementation = "B";
#elif defined(TRIAGE_ORACLE)
#include "../oracle/OracleQueue.h"
using Queue = OracleQueue;
static const char* implementation = "oracle";
#else
#error Define TRIAGE_A, TRIAGE_B, or TRIAGE_ORACLE
#endif

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
static long long now_ticks() {
    LARGE_INTEGER value;
    QueryPerformanceCounter(&value);
    return value.QuadPart;
}
static long long tick_frequency() {
    static const long long value = [] { LARGE_INTEGER f; QueryPerformanceFrequency(&f); return f.QuadPart; }();
    return value;
}
static unsigned long long ns(long long ticks) {
    return static_cast<unsigned long long>(ticks) * 1000000000ULL /
           static_cast<unsigned long long>(tick_frequency());
}
static std::size_t peak_memory() {
    PROCESS_MEMORY_COUNTERS_EX counters{};
    counters.cb = sizeof(counters);
    if (GetProcessMemoryInfo(GetCurrentProcess(),
        reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters), sizeof(counters)))
        return static_cast<std::size_t>(counters.PeakWorkingSetSize);
    return 0;
}
#else
using Clock = std::chrono::steady_clock;
static long long now_ticks() { return Clock::now().time_since_epoch().count(); }
static unsigned long long ns(long long ticks) {
    return static_cast<unsigned long long>(std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::duration(ticks)).count());
}
static std::size_t peak_memory() { return 0; }
#endif

int main(int argc, char** argv) {
    if (argc != 4) {
        std::cerr << "Usage: d6_scale_benchmark <patient-count> <revision-share-percent> <seed>\n";
        return 2;
    }
    const int n = std::stoi(argv[1]);
    const int share = std::stoi(argv[2]);
    const std::uint64_t seed = std::stoull(argv[3]);
    if (n < 100 || share < 0 || share > 90)
        throw std::invalid_argument("patient count must be >= 100 and revision share must be 0..90");

    constexpr int extracts = 16;
    const int revisions = static_cast<int>((static_cast<long long>(share) * extracts + (100 - share) / 2) / (100 - share));
    if (n < revisions)
        throw std::invalid_argument("patient count must be at least the number of revision targets");
    std::mt19937_64 rng(seed);
    std::uniform_int_distribution<int> severity(1, 50);
    std::vector<Patient> patients;
    patients.reserve(static_cast<std::size_t>(n));
    for (int id = 1; id <= n; ++id) {
        Patient p{};
        p.id = id;
        p.severity = severity(rng);
        p.arrival_time = 0;
        p.priority_key = -p.severity;
        patients.push_back(p);
    }
    std::vector<int> revision_ids(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) revision_ids[static_cast<std::size_t>(i)] = i + 1;
    std::shuffle(revision_ids.begin(), revision_ids.end(), rng);

    Queue queue;
    long long start = now_ticks();
    queue.build_heap(patients);
    const unsigned long long build_ns = ns(now_ticks() - start);

    start = now_ticks();
    for (int i = 0; i < revisions; ++i) {
        const Patient& p = patients[static_cast<std::size_t>(revision_ids[static_cast<std::size_t>(i)] - 1)];
        queue.decrease_key(p.id, p.priority_key - 1);
    }
    const unsigned long long revision_ns = ns(now_ticks() - start);

    start = now_ticks();
    unsigned long long checksum = 0;
    for (int i = 0; i < extracts; ++i) {
        const Patient p = queue.extract_min();
        checksum = checksum * 1315423911ULL + static_cast<unsigned>(p.id);
    }
    const unsigned long long extract_ns = ns(now_ticks() - start);
    const unsigned long long operation_ns = revision_ns + extract_ns;
    const double actual_share = revisions == 0 ? 0.0 : 100.0 * revisions / (revisions + extracts);

    std::cout << "implementation,patient_count,requested_revision_pct,actual_revision_pct,seed,build_ns,revision_ns,extract_ns,operation_ns,total_including_build_ns,revision_count,extract_count,peak_working_set_bytes,treatment_checksum\n";
    std::cout << implementation << ',' << n << ',' << share << ',' << actual_share << ',' << seed << ','
              << build_ns << ',' << revision_ns << ',' << extract_ns << ',' << operation_ns << ','
              << build_ns + operation_ns << ',' << revisions << ',' << extracts << ','
              << peak_memory() << ',' << checksum << '\n';
}
