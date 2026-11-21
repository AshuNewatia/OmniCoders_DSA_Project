#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <unordered_map>
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
#endif

#ifdef _WIN32
static long long clock_ticks() {
    LARGE_INTEGER value;
    QueryPerformanceCounter(&value);
    return value.QuadPart;
}
static long long clock_frequency() {
    static const long long frequency = [] {
        LARGE_INTEGER value;
        QueryPerformanceFrequency(&value);
        return value.QuadPart;
    }();
    return frequency;
}
static unsigned long long ticks_to_ns(long long ticks) {
    return static_cast<unsigned long long>(ticks) * 1000000000ULL /
           static_cast<unsigned long long>(clock_frequency());
}
#else
using Clock = std::chrono::steady_clock;
static long long clock_ticks() { return Clock::now().time_since_epoch().count(); }
static unsigned long long ticks_to_ns(long long ticks) {
    return static_cast<unsigned long long>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::duration(ticks)).count());
}
#endif
static constexpr int SIM_MINUTES = 30 * 24 * 60;
static constexpr int STEP_MINUTES = 30;
static constexpr int ARRIVAL_EVERY = 5;
static constexpr int TREAT_EVERY = 8;
static constexpr int DEPART_EVERY = 120;

struct Totals { long long ticks = 0; unsigned long long count = 0; };
struct Workload { int arrivals = 0, treatments = 0, departures = 0; };

static long long key_for(const Patient& p, const std::string& policy, int now) {
    if (policy == "linear") {
        // min-key form of the linear aging priority, with C = 1.
        return -static_cast<long long>(p.severity) + p.arrival_time;
    }
    const int steps = (now - p.arrival_time) / STEP_MINUTES;
    return -static_cast<long long>(p.severity) - 5LL * steps;
}

static Workload count_base_workload() {
    Workload result;
    int live = 0;
    for (int minute = 0; minute < SIM_MINUTES; ++minute) {
        if (minute % ARRIVAL_EVERY == 0) { ++live; ++result.arrivals; }
        if (minute > 0 && minute % DEPART_EVERY == 0 && live > 0) {
            --live; ++result.departures;
        }
        if (minute % TREAT_EVERY == 0 && live > 0) {
            --live; ++result.treatments;
        }
    }
    return result;
}

static std::size_t peak_working_set_bytes() {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS_EX counters{};
    counters.cb = sizeof(counters);
    if (GetProcessMemoryInfo(GetCurrentProcess(),
            reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters), sizeof(counters)))
        return static_cast<std::size_t>(counters.PeakWorkingSetSize);
#endif
    return 0; // Unsupported platform: caller records zero and must state limitation.
}

int main(int argc, char** argv) {
    if (argc != 4) {
        std::cerr << "Usage: d6_benchmark <linear|stepwise> <revision-share-percent> <seed>\n";
        return 2;
    }
    const std::string policy = argv[1];
    const int share_pct = std::stoi(argv[2]);
    const std::uint64_t seed = std::stoull(argv[3]);
    if ((policy != "linear" && policy != "stepwise") || share_pct < 0 || share_pct > 90)
        throw std::invalid_argument("invalid policy or revision share");

    const Workload base = count_base_workload();
    const int base_ops = base.arrivals + base.treatments + base.departures;
    const int revision_target = share_pct == 0 ? 0 :
        static_cast<int>((static_cast<long long>(share_pct) * base_ops + (100 - share_pct) / 2) /
                         (100 - share_pct));

    std::mt19937_64 rng(seed);
    std::uniform_int_distribution<int> severity_dist(1, 50);
    std::uniform_int_distribution<int> increase_dist(1, 5);
    Queue queue;
    std::unordered_map<int, Patient> waiting;
    std::vector<int> active;
    active.reserve(base.arrivals);
    std::vector<int> treatments;
    treatments.reserve(base.treatments);
    Totals insert_t, extract_t, revision_t, delete_t, escalation_t;
    int next_id = 1;
    int revisions_done = 0;
    int pending_revisions = 0;
    int escalations = 0;
    int peak_live = 0;

    auto timed = [](Totals& total, auto&& operation) {
        const long long start = clock_ticks();
        operation();
        total.ticks += clock_ticks() - start;
        ++total.count;
    };
    auto apply_pending_revisions = [&](int now) {
        while (pending_revisions > 0 && !active.empty()) {
            const std::size_t pick = static_cast<std::size_t>(rng() % active.size());
            Patient& p = waiting.at(active[pick]);
            ++p.severity;
            p.priority_key = key_for(p, policy, now);
            timed(revision_t, [&] { queue.decrease_key(p.id, p.priority_key); });
            --pending_revisions;
            ++revisions_done;
        }
    };

    int prior_revision_due = 0;
    for (int minute = 0; minute < SIM_MINUTES; ++minute) {
        if (minute > 0 && minute % (10 * 24 * 60) == 0) {
            std::cerr << implementation << ' ' << policy << ' '
                      << share_pct << "%: simulated day " << minute / (24 * 60)
                      << " / 30\n";
        }
        if (minute % ARRIVAL_EVERY == 0) {
            Patient p{};
            p.id = next_id++;
            p.severity = severity_dist(rng);
            p.arrival_time = minute;
            p.priority_key = key_for(p, policy, minute);
            timed(insert_t, [&] { queue.insert(p); });
            waiting.emplace(p.id, p);
            active.push_back(p.id);
            apply_pending_revisions(minute);
        }

        if (policy == "stepwise" && minute > 0 && minute % STEP_MINUTES == 0) {
            for (int id : active) {
                Patient& p = waiting.at(id);
                if ((minute - p.arrival_time) % STEP_MINUTES == 0) {
                    p.priority_key = key_for(p, policy, minute);
                    timed(escalation_t, [&] { queue.decrease_key(id, p.priority_key); });
                    ++escalations;
                }
            }
        }

        const int due = static_cast<int>((static_cast<long long>(minute + 1) * revision_target) /
                                         SIM_MINUTES);
        pending_revisions += due - prior_revision_due;
        prior_revision_due = due;
        apply_pending_revisions(minute);

        if (minute > 0 && minute % DEPART_EVERY == 0 && !active.empty()) {
            const std::size_t pick = static_cast<std::size_t>(rng() % active.size());
            const int id = active[pick];
            timed(delete_t, [&] { queue.delete_patient(id); });
            waiting.erase(id);
            active[pick] = active.back();
            active.pop_back();
        }

        if (minute % TREAT_EVERY == 0 && !active.empty()) {
            Patient treated{};
            timed(extract_t, [&] { treated = queue.extract_min(); });
            if (waiting.find(treated.id) == waiting.end()) {
                std::cerr << "queue returned missing patient id " << treated.id << '\n';
                return 3;
            }
            treatments.push_back(treated.id);
            waiting.erase(treated.id);
            const auto it = std::find(active.begin(), active.end(), treated.id);
            if (it == active.end()) { std::cerr << "active-set mismatch\n"; return 4; }
            *it = active.back();
            active.pop_back();
        }
        peak_live = std::max(peak_live, static_cast<int>(active.size()));
    }

    // Any revision event due at the final minute is already processed above.
    const double actual_share = revisions_done == 0 ? 0.0 :
        100.0 * revisions_done / (base_ops + revisions_done);
    const unsigned long long insert_ns = ticks_to_ns(insert_t.ticks);
    const unsigned long long extract_ns = ticks_to_ns(extract_t.ticks);
    const unsigned long long revision_ns = ticks_to_ns(revision_t.ticks);
    const unsigned long long delete_ns = ticks_to_ns(delete_t.ticks);
    const unsigned long long escalation_ns = ticks_to_ns(escalation_t.ticks);
    const unsigned long long total_ns = insert_ns + extract_ns + revision_ns +
                                        delete_ns + escalation_ns;
    std::cout << "implementation,policy,requested_revision_pct,actual_revision_pct,seed,"
                 "arrivals,treatments,departures,revisions,escalations,peak_live,"
                 "insert_ns,extract_ns,revision_ns,delete_ns,escalation_ns,total_queue_ns,"
                 "insert_count,extract_count,revision_count,delete_count,escalation_count,"
                 "peak_working_set_bytes,treatment_id_checksum\n";
    unsigned long long checksum = 0;
    for (int id : treatments) checksum = checksum * 1315423911ULL + static_cast<unsigned>(id);
    std::cout << implementation << ',' << policy << ',' << share_pct << ',' << actual_share << ',' << seed
              << ',' << base.arrivals << ',' << extract_t.count << ',' << delete_t.count
              << ',' << revisions_done << ',' << escalations << ',' << peak_live
              << ',' << insert_ns << ',' << extract_ns << ',' << revision_ns
              << ',' << delete_ns << ',' << escalation_ns << ',' << total_ns
              << ',' << insert_t.count << ',' << extract_t.count << ',' << revision_t.count
              << ',' << delete_t.count << ',' << escalation_t.count
              << ',' << peak_working_set_bytes() << ',' << checksum << '\n';
    return 0;
}
