#include <iostream>
#include <fstream>
#include <string>
#include <random>
#include <unordered_map>
#include <iterator>
#include <cstdint>
// #include "../src/structA/structure_A.cpp"
// #include "../src/structB/StructureB.cpp"
#include "../oracle/OracleQueue.h"
#include "ITriageQueue.h"

using namespace std;

enum class AgingPolicy {
    NONE,
    LINEAR,
    STEP_WISE
};

struct WaitStats {
    long long total_wait_time = 0;
    int patient_count = 0;
};

long long calculate_key(
    const Patient& patient,
    AgingPolicy policy,
    int current_time,
    int step_interval,
    int linear_c,
    int step_amount
) {
    long long key = -static_cast<long long>(patient.severity);

    if (policy == AgingPolicy::LINEAR) {
        key += static_cast<long long>(linear_c) * patient.arrival_time;
    } else if (policy == AgingPolicy::STEP_WISE) {
        int wait_time = current_time - patient.arrival_time;
        int steps = wait_time / step_interval;

        key -= static_cast<long long>(step_amount) * steps;
    }

    return key;
}

void simulation(
    ITriageQueue* queue,
    AgingPolicy policy,
    uint64_t seed,
    const string& log_filename
) {
    mt19937_64 rng(seed);

    cout << "Starting simulation with RNG seed: " << seed << '\n';

    uniform_int_distribution<int> arrival_dist(1, 20);
    uniform_int_distribution<int> severity_dist(1, 50);
    uniform_int_distribution<int> treatment_dist(5, 10);
    uniform_int_distribution<int> leave_dist(1, 200);

    uniform_int_distribution<int> revision_dist(1, 100);
    uniform_int_distribution<int> revision_increase_dist(1, 5);

    const int TOTAL_MINS = 30 * 24 * 60;
    const int STEP_INTERVAL = 30;
    const int LINEAR_C = 1;
    const int STEP_AMOUNT = 5;

    int doctor_free_at = 0;
    int patient_id_counter = 1;

    unordered_map<int, Patient> waiting_room;
    unordered_map<int, WaitStats> severity_stats;

    ofstream log_file(log_filename);

    if (!log_file) {
        cerr << "Cannot open log file: " << log_filename << '\n';
        return;
    }

    log_file << "minute,event,patient_id,wait_time,severity,priority_key\n";

    for (int curr = 0; curr < TOTAL_MINS; curr++) {
        if (arrival_dist(rng) == 1) {
            Patient p;

            p.id = patient_id_counter++;
            p.arrival_time = curr;
            p.severity = severity_dist(rng);

            p.priority_key = calculate_key(
                p,
                policy,
                curr,
                STEP_INTERVAL,
                LINEAR_C,
                STEP_AMOUNT
            );

            waiting_room[p.id] = p;
            queue->insert(p);

            log_file
                << curr << ",ARRIVE,"
                << p.id << ",0,"
                << p.severity << ","
                << p.priority_key << '\n';
        }

        if (policy == AgingPolicy::STEP_WISE) {
            for (auto& entry : waiting_room) {
                Patient& p = entry.second;
                int wait_time = curr - p.arrival_time;

                if (wait_time > 0 && wait_time % STEP_INTERVAL == 0) {
                    long long new_key = calculate_key(
                        p,
                        policy,
                        curr,
                        STEP_INTERVAL,
                        LINEAR_C,
                        STEP_AMOUNT
                    );

                    queue->decrease_key(p.id, new_key);
                    p.priority_key = new_key;

                    log_file
                        << curr << ",ESCALATE,"
                        << p.id << ","
                        << wait_time << ","
                        << p.severity << ","
                        << p.priority_key << '\n';
                }
            }
        }

        if (!waiting_room.empty() && revision_dist(rng) == 1) {
            auto it = waiting_room.begin();
            advance(it, rng() % waiting_room.size());

            Patient& p = it->second;

            p.severity += revision_increase_dist(rng);

            long long new_key = calculate_key(
                p,
                policy,
                curr,
                STEP_INTERVAL,
                LINEAR_C,
                STEP_AMOUNT
            );

            queue->decrease_key(p.id, new_key);
            p.priority_key = new_key;

            log_file
                << curr << ",REVISE,"
                << p.id << ","
                << curr - p.arrival_time << ","
                << p.severity << ","
                << p.priority_key << '\n';
        }

        if (!waiting_room.empty() && leave_dist(rng) == 1) {
            auto it = waiting_room.begin();
            advance(it, rng() % waiting_room.size());

            Patient p = it->second;

            queue->delete_patient(p.id);

            log_file
                << curr << ",ABANDON,"
                << p.id << ","
                << curr - p.arrival_time << ","
                << p.severity << ","
                << p.priority_key << '\n';

            waiting_room.erase(it);
        }

        if (curr >= doctor_free_at && !waiting_room.empty()) {
            Patient treated = queue->extract_min();

            auto it = waiting_room.find(treated.id);

            if (it == waiting_room.end()) {
                cerr << "Queue returned an unknown patient ID: "
                     << treated.id << '\n';
                return;
            }

            Patient current_patient = it->second;
            int wait_time = curr - current_patient.arrival_time;

            log_file
                << curr << ",TREAT,"
                << current_patient.id << ","
                << wait_time << ","
                << current_patient.severity << ","
                << current_patient.priority_key << '\n';

            WaitStats& stats = severity_stats[current_patient.severity];
            stats.total_wait_time += wait_time;
            stats.patient_count++;

            waiting_room.erase(it);

            doctor_free_at = curr + treatment_dist(rng);
        }
    }

    cout << "\n--- Waiting-Time Statistics Per Severity Class ---\n";
    cout << "Severity\tTotal Patients\tAverage Wait (mins)\n";

    for (const auto& entry : severity_stats) {
        int severity = entry.first;
        const WaitStats& stats = entry.second;

        double average_wait =
            static_cast<double>(stats.total_wait_time) / stats.patient_count;

        cout << severity << "\t\t"
             << stats.patient_count << "\t\t"
             << average_wait << '\n';
    }

    cout << "\nPatients still waiting after 30 days: "
         << waiting_room.size() << '\n';

    cout << "Event log saved to: " << log_filename << '\n';
}

int main() {
    std::cout << "Run the simulator through the dashboard.\n";
    return 0;
}