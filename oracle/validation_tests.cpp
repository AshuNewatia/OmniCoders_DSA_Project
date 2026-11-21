#include "OracleQueue.h"

#include <algorithm>
#include <climits>
#include <cstdint>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <map>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

// This reference uses map storage and sorting, not the oracle's scan/comparator.
using Reference = std::map<int, Patient>;
std::vector<std::string> trace;

std::string describe(const Patient& p) {
    std::ostringstream out;
    out << "{id=" << p.id << ",severity=" << p.severity
        << ",arrival=" << p.arrival_time << ",key=" << p.priority_key << '}';
    return out.str();
}

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

void same_patient(const Patient& actual, const Patient& expected) {
    require(actual.id == expected.id && actual.severity == expected.severity &&
            actual.arrival_time == expected.arrival_time &&
            actual.priority_key == expected.priority_key,
            "expected " + describe(expected) + ", actual " + describe(actual));
}

template<class Error, class Action>
void expect_error(Action action) {
    bool caught = false;
    try { action(); }
    catch (const Error&) { caught = true; }
    require(caught, "expected exception was not thrown");
    // Exceptions of a different type propagate to the named test runner.
}

std::vector<Patient> sorted_reference(const Reference& reference) {
    std::vector<Patient> result;
    for (const auto& item : reference) result.push_back(item.second);
    std::sort(result.begin(), result.end(), [](const Patient& a, const Patient& b) {
        return std::tie(a.priority_key, a.arrival_time, a.id) <
               std::tie(b.priority_key, b.arrival_time, b.id);
    });
    return result;
}

// Drain a COPY after each generated operation. The original oracle stays intact.
// This checks the complete observable state, not just the next returned ID.
void check_state(const OracleQueue& original, const Reference& reference) {
    require(original.size() == reference.size(), "live size mismatch");
    require(original.empty() == reference.empty(), "empty() mismatch");
    OracleQueue copy = original;
    for (const Patient& expected : sorted_reference(reference)) {
        same_patient(copy.extract_min(), expected);
    }
    require(copy.empty(), "extra patient remains after draining");
    expect_error<std::out_of_range>([&] { copy.extract_min(); });
}

void check_order(OracleQueue& queue, const std::vector<int>& ids) {
    for (int id : ids) {
        Patient actual = queue.extract_min();
        require(actual.id == id, "expected ID " + std::to_string(id) +
                ", actual " + describe(actual));
    }
    require(queue.empty(), "unexpected patient remains");
}

// 3 possible keys x 3 arrival times for each of 3 patients = 729 data sets.
// Test every input permutation (6): 4,374 builds and full extraction sequences.
void exhaustive_small() {
    for (int code = 0; code < 729; ++code) {
        int digits = code;
        std::vector<Patient> patients;
        Reference reference;
        for (int id = 1; id <= 3; ++id) {
            int key = digits % 3 - 1; digits /= 3;
            int arrival = digits % 3; digits /= 3;
            Patient p{id, 50 - id, arrival, key};
            patients.push_back(p); reference.emplace(id, p);
        }
        std::vector<int> permutation{0, 1, 2};
        do {
            trace = {"small-state code=" + std::to_string(code),
                     "permutation=" + std::to_string(permutation[0]) + "," +
                     std::to_string(permutation[1]) + "," + std::to_string(permutation[2])};
            OracleQueue queue;
            std::vector<Patient> input;
            for (int index : permutation) input.push_back(patients[index]);
            queue.build_heap(input);
            check_state(queue, reference);
        } while (std::next_permutation(permutation.begin(), permutation.end()));
    }
}

void random_sequence(std::uint64_t seed, int steps) {
    std::mt19937_64 rng(seed);
    OracleQueue queue;
    Reference reference;
    int next_id = 1;
    trace.clear();
    trace.push_back("seed=" + std::to_string(seed) + ", steps=" + std::to_string(steps));

    auto new_patient = [&]() {
        return Patient{next_id++, static_cast<int>(rng() % 51),
                       static_cast<int>(rng() % 31),
                       static_cast<long long>(rng() % 21) - 10};
    };

    for (int step = 0; step < steps; ++step) {
        int op = static_cast<int>(rng() % 9);
        // Bound live occupancy; keep IDs unique across resets and removals.
        if (reference.size() >= 64 && op == 0) op = 4;
        std::string prefix = "step " + std::to_string(step) + ": ";
        if (op == 0) {
            Patient p = new_patient();
            trace.push_back(prefix + "insert " + describe(p));
            queue.insert(p); reference.emplace(p.id, p);
        } else if (op == 1) {
            std::vector<Patient> input;
            int count = static_cast<int>(rng() % 16);
            reference.clear();
            trace.push_back(prefix + "build replacement, count=" + std::to_string(count));
            for (int i = 0; i < count; ++i) {
                Patient p = new_patient(); input.push_back(p); reference.emplace(p.id, p);
                trace.push_back("  " + describe(p));
            }
            queue.build_heap(input);
        } else if (op >= 2 && op <= 4 && !reference.empty()) {
            auto item = reference.begin();
            std::advance(item, static_cast<int>(rng() % reference.size()));
            int id = item->first;
            if (op == 2) {
                long long key = item->second.priority_key - static_cast<long long>(rng() % 6);
                trace.push_back(prefix + "decrease " + std::to_string(id) + " to " + std::to_string(key));
                queue.decrease_key(id, key); item->second.priority_key = key;
            } else if (op == 3) {
                trace.push_back(prefix + "delete " + std::to_string(id));
                queue.delete_patient(id); reference.erase(id);
            } else {
                Patient expected = sorted_reference(reference).front();
                trace.push_back(prefix + "extract, expected " + describe(expected));
                same_patient(queue.extract_min(), expected); reference.erase(expected.id);
            }
        } else if (op == 5 && !reference.empty()) {
            Patient p = reference.begin()->second;
            trace.push_back(prefix + "invalid increase for " + std::to_string(p.id));
            expect_error<std::invalid_argument>([&] { queue.decrease_key(p.id, p.priority_key + 1); });
        } else if (op == 6) {
            trace.push_back(prefix + "delete missing ID 0");
            expect_error<std::out_of_range>([&] { queue.delete_patient(0); });
        } else if (op == 7) {
            trace.push_back(prefix + "decrease missing ID 0");
            expect_error<std::out_of_range>([&] { queue.decrease_key(0, -100); });
        } else if (reference.empty()) {
            trace.push_back(prefix + "extract empty queue");
            expect_error<std::out_of_range>([&] { queue.extract_min(); });
        } else {
            trace.push_back(prefix + "observe without mutation");
        }
        check_state(queue, reference);
    }
}

int main(int argc, char** argv) {
    // Optional single-seed replay: validation_tests <seed> <steps>.
    if (argc != 1 && argc != 3) {
        std::cerr << "Usage: validation_tests [seed steps]\n";
        return 2;
    }
    std::vector<std::pair<std::string, std::function<void()>>> tests;
    auto add = [&](const std::string& name, std::function<void()> test) {
        tests.push_back({name, test});
    };

    add("new_queue_empty", [] {
        OracleQueue q; require(q.empty() && q.size() == 0, "new queue not empty");
    });
    add("empty_extract_throws", [] {
        OracleQueue q; expect_error<std::out_of_range>([&] { q.extract_min(); });
    });
    add("single_patient_full_record", [] {
        OracleQueue q; Patient p{101, 7, 10, -7}; q.insert(p);
        require(q.size() == 1, "insert size"); same_patient(q.extract_min(), p);
        require(q.empty(), "single extraction size");
    });
    add("minimum_is_key_not_severity_or_id", [] {
        OracleQueue q; q.build_heap({{1, 100, 0, 8}, {2, 1, 20, -9}, {3, 90, 1, 0}});
        check_order(q, {2, 3, 1});
    });
    add("equal_key_earlier_arrival", [] {
        OracleQueue q; q.build_heap({{1, 8, 20, -8}, {2, 8, 10, -8}});
        check_order(q, {2, 1});
    });
    add("equal_key_and_arrival_smaller_id", [] {
        OracleQueue q; q.build_heap({{103, 5, 10, -5}, {101, 5, 10, -5}, {102, 5, 10, -5}});
        check_order(q, {101, 102, 103});
    });
    add("priority_overrides_arrival", [] {
        OracleQueue q; q.build_heap({{1, 2, 0, -2}, {2, 9, 50, -9}});
        check_order(q, {2, 1});
    });
    add("build_replaces_and_copies_input", [] {
        OracleQueue q; q.insert({1, 4, 0, -4});
        std::vector<Patient> input{{2, 9, 10, -9}, {3, 2, 20, -2}};
        q.build_heap(input); input[0].priority_key = 999; input.clear();
        same_patient(q.extract_min(), Patient{2, 9, 10, -9}); check_order(q, {3});
    });
    add("build_empty_clears_queue", [] {
        OracleQueue q; q.insert({1, 1, 0, -1}); q.build_heap({});
        require(q.empty() && q.size() == 0, "empty build failed");
    });
    add("decrease_changes_selection_only_key", [] {
        OracleQueue q; q.build_heap({{1, 7, 0, -7}, {2, 3, 10, -3}});
        q.decrease_key(2, -10);
        same_patient(q.extract_min(), Patient{2, 3, 10, -10}); check_order(q, {1});
    });
    add("equal_key_update_allowed", [] {
        OracleQueue q; q.insert({1, 7, 12, -7}); q.decrease_key(1, -7);
        same_patient(q.extract_min(), Patient{1, 7, 12, -7});
    });
    add("repeated_decreases_and_ties", [] {
        OracleQueue q; q.build_heap({{1, 1, 0, -10}, {2, 2, 1, 0}});
        for (int key = -1; key >= -10; --key) q.decrease_key(2, key);
        check_order(q, {1, 2});
    });
    add("increasing_key_rejected_without_mutation", [] {
        OracleQueue q; Patient p{1, 9, 2, -9}; q.insert(p);
        expect_error<std::invalid_argument>([&] { q.decrease_key(1, -8); });
        same_patient(q.extract_min(), p);
    });
    add("missing_decrease_rejected_without_mutation", [] {
        OracleQueue q; Patient p{1, 9, 2, -9}; q.insert(p);
        expect_error<std::out_of_range>([&] { q.decrease_key(999, -10); });
        same_patient(q.extract_min(), p);
    });
    add("missing_delete_rejected_without_mutation", [] {
        OracleQueue q; Patient p{1, 9, 2, -9}; q.insert(p);
        expect_error<std::out_of_range>([&] { q.delete_patient(999); });
        same_patient(q.extract_min(), p);
    });
    add("missing_operations_on_empty", [] {
        OracleQueue q;
        expect_error<std::out_of_range>([&] { q.delete_patient(1); });
        expect_error<std::out_of_range>([&] { q.decrease_key(1, -1); });
        require(q.empty(), "failed operation mutated empty queue");
    });
    for (int position = 0; position < 3; ++position) {
        add("delete_storage_position_" + std::to_string(position), [position] {
            OracleQueue q;
            Reference r{{1, {1, 2, 0, -2}}, {2, {2, 9, 0, -9}}, {3, {3, 5, 0, -5}}};
            q.build_heap({r.at(1), r.at(2), r.at(3)});
            q.delete_patient(position + 1); r.erase(position + 1); check_state(q, r);
        });
    }
    add("delete_only_patient", [] {
        OracleQueue q; q.insert({1, 4, 0, -4}); q.delete_patient(1);
        require(q.empty(), "delete only patient failed");
        expect_error<std::out_of_range>([&] { q.delete_patient(1); });
    });
    add("removed_ids_are_missing", [] {
        OracleQueue q; q.insert({1, 4, 0, -4}); q.extract_min();
        expect_error<std::out_of_range>([&] { q.decrease_key(1, -10); });
        expect_error<std::out_of_range>([&] { q.delete_patient(1); });
    });
    add("long_long_key_boundaries", [] {
        OracleQueue q; q.build_heap({{1, 1, 0, LLONG_MAX}, {2, 2, 0, LLONG_MIN}, {3, 3, 0, 0}});
        check_order(q, {2, 3, 1});
        q.insert({4, 4, 0, LLONG_MAX}); q.decrease_key(4, LLONG_MIN);
        same_patient(q.extract_min(), Patient{4, 4, 0, LLONG_MIN});
    });
    add("large_positive_ids", [] {
        OracleQueue q; q.build_heap({{INT_MAX, 1, 0, 0}, {INT_MAX - 1, 1, 0, 0}});
        q.delete_patient(INT_MAX); check_order(q, {INT_MAX - 1});
    });
    add("duplicate_live_insert_rejected", [] {
        OracleQueue q;
        Reference r{{1, {1, 4, 0, -4}}, {2, {2, 4, 0, -4}}, {3, {3, 9, 20, -9}}};
        q.build_heap({r.at(1), r.at(2), r.at(3)});
        for (int id = 1; id <= 3; ++id) {
            expect_error<std::invalid_argument>([&] { q.insert({id, 50, 99, -100}); });
            check_state(q, r);
        }
    });
    add("duplicate_build_rejected", [] {
        OracleQueue q;
        Patient original{10, 5, 1, -5};
        q.insert(original);
        expect_error<std::invalid_argument>([&] {
            q.build_heap({{1, 4, 0, -4}, {2, 4, 0, -4}, {1, 9, 20, -9}});
        });
        check_state(q, Reference{{original.id, original}});
    });
    add("exhaustive_4374_small_builds", exhaustive_small);
    add("large_tie_heavy_batch_2000", [] {
        OracleQueue q; Reference r; std::vector<Patient> input;
        for (int id = 2000; id >= 1; --id) {
            Patient p{id, id % 50, id % 7, id % 11 - 5};
            input.push_back(p); r.emplace(id, p);
        }
        q.build_heap(input); check_state(q, r);
    });

    try {
        if (argc == 3) {
            std::string seed_arg = argv[1], steps_arg = argv[2];
            std::size_t a = 0, b = 0;
            std::uint64_t seed = std::stoull(seed_arg, &a);
            int steps = std::stoi(steps_arg, &b);
            if (seed_arg.empty() || seed_arg[0] == '-' || a != seed_arg.size() ||
                b != steps_arg.size() || steps < 1 || steps > 1000000) {
                throw std::invalid_argument("invalid seed/steps (steps must be 1..1000000)");
            }
            add("random_seed_" + std::to_string(seed), [seed, steps] { random_sequence(seed, steps); });
        } else {
            for (std::uint64_t seed = 1; seed <= 20; ++seed) {
                add("random_seed_" + std::to_string(seed), [seed] { random_sequence(seed, 2000); });
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Invalid arguments: " << e.what() << '\n'; return 2;
    }

    int passed = 0, failed = 0;
    for (const auto& test : tests) {
        trace.clear();
        try {
            test.second();
            ++passed; std::cout << "PASS " << test.first << '\n';
        } catch (const std::exception& error) {
            ++failed;
            std::cout << "FAIL " << test.first << " | " << error.what() << '\n';
            if (!trace.empty()) {
                std::string path = "failure_" + test.first + ".txt";
                std::ofstream log(path);
                for (const std::string& line : trace) log << line << '\n';
                log << "FAILURE: " << error.what() << '\n';
                std::cout << "TRACE " << path << '\n';
            }
        }
    }
    std::cout << "SUMMARY total=" << tests.size() << " passed=" << passed
              << " failed=" << failed << '\n';
    return failed == 0 ? 0 : 1;
}
