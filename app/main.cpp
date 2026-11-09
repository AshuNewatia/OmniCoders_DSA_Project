#include <iostream>
#include <fstream>
#include <vector>
#include <random>
#include <unordered_map>
#include "../ITriageQueue.h"
using namespace std;

enum class AgingPolicy { NONE, LINEAR, STEP_WISE };

struct WaitStats {
    long long total_wait_time = 0;
    int patient_count = 0;
};

void simulation(ITriageQueue* queue, AgingPolicy policy) {
    unsigned int generated_seed = random_device{}();
    mt19937_64 rng(generated_seed);
    cout << "Starting simulation with RNG seed: " << generated_seed << endl;

    uniform_int_distribution<int> arrival_dist(1, 20);
    uniform_int_distribution<int> severity_dist(1, 50);
    uniform_int_distribution<int> treatment_dist(5, 10);
    uniform_int_distribution<int> leave_dist(1, 200);

    const int TOTAL_MINS = 30 * 24 * 60;
    const int STEP_INTERVAL = 20;
    const int LINEAR_C = 1;

    int doctor_free_at = 0;
    int patient_id_counter = 1;

    unordered_map<int, Patient> waiting_room;

    unordered_map<int, WaitStats> severity_stats;

    std::ofstream log_file("results/simulation_log.csv");
    log_file << "minute,event,patient_id,wait_time\n";

    for (int curr = 0; curr < TOTAL_MINS; curr++) {
        
        if (arrival_dist(rng) == 1) {
            Patient p;
            p.id = patient_id_counter++;
            p.arrival_time = curr;
            p.severity = severity_dist(rng);

            waiting_room[p.id] = p;

            if (policy == AgingPolicy::LINEAR) {
                p.severity = p.severity - (LINEAR_C * p.arrival_time);
            }
            queue->insert(p);
            log_file << curr << ",ARRIVE," << p.id << ",0\n";
        }

        if (policy == AgingPolicy::STEP_WISE) {
            for (auto& [id, p] : waiting_room) {
                int wait_time = curr - p.arrival_time;
                if (wait_time > 0 && wait_time % STEP_INTERVAL == 0) {
                    int new_severity = p.severity - 5;
                    queue->decrease_key(id, new_severity);
                    p.severity = new_severity;
                    log_file << curr << ",ESCALATE," << id << "," << wait_time << "\n";
                }
            }
        }

        if (!waiting_room.empty() && leave_dist(rng) == 1) {
            auto it = waiting_room.begin();
            std::advance(it, rng() % waiting_room.size());
            int id_to_leave = it->first;
            
            queue->delete_patient(id_to_leave);
            
            log_file << curr << ",ABANDON," << id_to_leave << "," << (curr - it->second.arrival_time) << "\n";
            waiting_room.erase(it);
        }

        if (curr >= doctor_free_at && !waiting_room.empty()) {
            Patient treated = queue->extract_min();
            int wait_time = curr - treated.arrival_time;
            
            log_file << curr << ",TREAT," << treated.id << "," << wait_time << "\n";

            severity_stats[treated.severity].total_wait_time += wait_time;
            severity_stats[treated.severity].patient_count++;
            
            waiting_room.erase(treated.id);
            doctor_free_at = curr + treatment_dist(rng);
        }
    }
    
    log_file.close();

    cout << "\n--- Waiting-Time Statistics Per Severity Class ---\n";
    cout << "Severity\tTotal Patients\tAverage Wait (mins)\n";
    for (const auto& [severity, stats] : severity_stats) {
        double avg_wait = static_cast<double>(stats.total_wait_time) / stats.patient_count;
        cout << severity << "\t\t" << stats.patient_count << "\t\t" << avg_wait << "\n";
    }
}

int main() {
    cout << "Simulator engine ready. Awaiting linked queues." << endl;
    return 0;
}