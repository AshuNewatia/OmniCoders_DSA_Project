#include "OracleQueue.h"

#include <iostream>
#include <stdexcept>

void check(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

int main() {
    try {
        OracleQueue queue;

        check(queue.empty(), "New queue should be empty");

        // Insert, decrease key, and delete.
        queue.insert({101, 7, 0, -7});
        queue.insert({102, 3, 1, -3});
        queue.insert({103, 9, 2, -9});

        queue.decrease_key(103, -15);
        queue.delete_patient(101);

        check(queue.size() == 2, "Queue size should be 2");
        check(queue.extract_min().id == 103, "Patient 103 should be first");
        check(queue.extract_min().id == 102, "Patient 102 should be second");
        check(queue.empty(), "Queue should be empty");

        // Equal keys: earlier arrival, then smaller ID.
        // This checks the tie rule currently implemented by the oracle.
        queue.build_heap({
            {3, 5, 10, -5},
            {2, 5, 0, -5},
            {1, 5, 0, -5}
        });

        check(queue.extract_min().id == 1, "ID tie-break failed");
        check(queue.extract_min().id == 2, "Arrival tie-break failed");
        check(queue.extract_min().id == 3, "Final tie extraction failed");

        // Building replaces old contents.
        queue.insert({9, 10, 0, -10});

        queue.build_heap({
            {4, 2, 0, -2}
        });

        check(queue.size() == 1, "Build should replace old contents");

        // An equal-key update is allowed.
        queue.decrease_key(4, -2);

        Patient patient = queue.extract_min();

        check(patient.id == 4, "Incorrect patient after build");
        check(patient.severity == 2, "Severity should remain unchanged");
        check(patient.priority_key == -2, "Incorrect priority key");
        check(patient.arrival_time == 0, "Incorrect arrival time");

        // Extracting from an empty queue should throw.
        bool caught = false;

        try {
            queue.extract_min();
        } catch (const std::out_of_range&) {
            caught = true;
        }

        check(caught, "Empty extraction should throw");

        // Deleting a missing patient should throw.
        caught = false;

        try {
            queue.delete_patient(999);
        } catch (const std::out_of_range&) {
            caught = true;
        }

        check(caught, "Deleting a missing patient should throw");

        // Updating a missing patient should throw.
        caught = false;

        try {
            queue.decrease_key(999, -10);
        } catch (const std::out_of_range&) {
            caught = true;
        }

        check(caught, "Updating a missing patient should throw");

        // Increasing a key is invalid for decrease_key.
        queue.insert({5, 2, 0, -2});

        caught = false;

        try {
            queue.decrease_key(5, 8);
        } catch (const std::invalid_argument&) {
            caught = true;
        }

        check(caught, "Increasing the key should throw");

        patient = queue.extract_min();

        check(patient.id == 5, "Incorrect patient after invalid update");
        check(
            patient.priority_key == -2,
            "Key changed after invalid update"
        );
        check(
            patient.severity == 2,
            "Severity changed after invalid update"
        );

        std::cout << "All oracle tests passed.\n";
    } catch (const std::exception& error) {
        std::cerr << "Oracle test failed: " << error.what() << '\n';
        return 1;
    }

    return 0;
}