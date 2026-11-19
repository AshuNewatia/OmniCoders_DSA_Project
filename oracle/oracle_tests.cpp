#include "OracleQueue.h"

#include <iostream>
#include <stdexcept>

void check(bool condition) {
    if (!condition) {
        throw std::runtime_error("Oracle test failed");
    }
}

int main() {
    OracleQueue queue;
    check(queue.empty());

    queue.insert({101, 7, 0});
    queue.insert({102, 3, 1});
    queue.insert({103, 9, 2});
    queue.decrease_key(103, 1);
    queue.delete_patient(101);
    check(queue.size() == 2);
    check(queue.extract_min().id == 103);
    check(queue.extract_min().id == 102);
    check(queue.empty());

    // Equal keys: earlier arrival wins, then smaller ID.
    queue.build_heap({{3, 5, 10}, {2, 5, 0}, {1, 5, 0}});
    check(queue.extract_min().id == 1);
    check(queue.extract_min().id == 2);
    check(queue.extract_min().id == 3);

    // Building replaces old contents; negative keys are allowed.
    queue.insert({9, 10, 0});
    queue.build_heap({{4, -2, 0}});
    queue.decrease_key(4, -2); // An unchanged key is allowed.
    check(queue.size() == 1);
    Patient patient = queue.extract_min();
    check(patient.id == 4 && patient.severity == -2 && patient.arrival_time == 0);

    bool caught = false;
    try { queue.extract_min(); }
    catch (const std::out_of_range&) { caught = true; }
    check(caught);

    caught = false;
    try { queue.delete_patient(999); }
    catch (const std::out_of_range&) { caught = true; }
    check(caught);

    caught = false;
    try { queue.decrease_key(999, 1); }
    catch (const std::out_of_range&) { caught = true; }
    check(caught);

    queue.insert({5, 2, 0});
    caught = false;
    try { queue.decrease_key(5, 8); }
    catch (const std::invalid_argument&) { caught = true; }
    check(caught);
    check(queue.extract_min().severity == 2);

    // Delete the final element and reuse its ID after removal.
    queue.insert({6, 4, 0});
    queue.delete_patient(6);
    check(queue.empty());
    queue.insert({6, 1, 5});
    check(queue.extract_min().arrival_time == 5);

    std::cout << "All oracle tests passed.\n";
}
