#include "OracleQueue.h"

#include <stdexcept>
#include <unordered_set>

bool OracleQueue::comes_before(
    const Patient& left,
    const Patient& right
) {
    if (left.priority_key != right.priority_key) {
        return left.priority_key < right.priority_key;
    }

    if (left.arrival_time != right.arrival_time) {
        return left.arrival_time < right.arrival_time;
    }

    return left.id < right.id;
}

void OracleQueue::build_heap(const std::vector<Patient>& patients) {
    // Validate first so invalid input cannot replace the existing queue.
    std::unordered_set<int> ids;
    ids.reserve(patients.size());
    for (const Patient& patient : patients) {
        if (!ids.insert(patient.id).second) {
            throw std::invalid_argument("build_heap contains duplicate patient IDs");
        }
    }
    patients_ = patients;
}

void OracleQueue::insert(const Patient& patient) {
    for (const Patient& existing : patients_) {
        if (existing.id == patient.id) {
            throw std::invalid_argument("Patient ID is already live");
        }
    }
    patients_.push_back(patient);
}

Patient OracleQueue::extract_min() {
    if (patients_.empty()) {
        throw std::out_of_range(
            "Cannot extract from an empty oracle queue"
        );
    }

    std::size_t best = 0;

    for (std::size_t i = 1; i < patients_.size(); ++i) {
        if (comes_before(patients_[i], patients_[best])) {
            best = i;
        }
    }

    Patient result = patients_[best];

    patients_[best] = patients_.back();
    patients_.pop_back();

    return result;
}

void OracleQueue::decrease_key(int id, long long new_key) {
    for (std::size_t i = 0; i < patients_.size(); ++i) {
        if (patients_[i].id == id) {
            if (new_key > patients_[i].priority_key) {
                throw std::invalid_argument(
                    "decrease_key cannot increase the key"
                );
            }

            patients_[i].priority_key = new_key;
            return;
        }
    }

    throw std::out_of_range(
        "Patient ID not found in oracle queue"
    );
}

void OracleQueue::delete_patient(int id) {
    for (std::size_t i = 0; i < patients_.size(); ++i) {
        if (patients_[i].id == id) {
            patients_[i] = patients_.back();
            patients_.pop_back();
            return;
        }
    }

    throw std::out_of_range(
        "Patient ID not found in oracle queue"
    );
}
