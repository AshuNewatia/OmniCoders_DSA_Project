#include "../app/ITriageQueue.h"
#include "OracleQueue.h"

#include <algorithm>
#include <climits>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

// Build each queue in a separate executable: A/B define conflicting helper names.
#if defined(DIFF_QUEUE_A)
#include "../src/structA/structure_A.cpp"
using SelectedQueue = Min_Heap;
#elif defined(DIFF_QUEUE_B)
#include "../src/structB/structureB.cpp"
using SelectedQueue = LazyTriageQueue;
#else
using SelectedQueue = OracleQueue;
#endif

Patient read_patient(std::istringstream& input) {
    Patient p{};
    if (!(input >> p.id >> p.severity >> p.arrival_time >> p.priority_key)) {
        throw std::runtime_error("Malformed patient record in trace");
    }
    return p;
}

void write_patient(std::ostream& out, const Patient& p) {
    out << p.id << ' ' << p.severity << ' ' << p.arrival_time << ' ' << p.priority_key;
}

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: differential_driver <trace file>\n";
        return 2;
    }
    std::ifstream file(argv[1]);
    if (!file) {
        std::cerr << "Cannot open trace\n";
        return 2;
    }
    SelectedQueue queue;
    std::string line;
    int line_number = 0;
    while (std::getline(file, line)) {
        ++line_number;
        if (line.empty()) {
            std::cerr << "Empty trace line " << line_number << '\n';
            return 2;
        }
        std::istringstream input(line);
        std::string operation;
        input >> operation;
        // Parse before executing, so malformed traces cannot be mistaken for
        // exceptions thrown by a queue implementation.
        Patient patient{};
        std::vector<Patient> patients;
        int id = 0, count = 0;
        long long key = 0;
        try {
            if (operation == "BUILD") {
                if (!(input >> count) || count < 0 || count > 1000000)
                    throw std::runtime_error("Invalid build size");
                for (int i = 0; i < count; ++i) patients.push_back(read_patient(input));
            } else if (operation == "INSERT") {
                patient = read_patient(input);
            } else if (operation == "DECREASE") {
                if (!(input >> id >> key)) throw std::runtime_error("Invalid decrease");
            } else if (operation == "DELETE") {
                if (!(input >> id)) throw std::runtime_error("Invalid deletion");
            } else if (operation == "SNAPSHOT") {
                if (!(input >> count) || count < 0 || count > 1000000)
                    throw std::runtime_error("Invalid snapshot size");
            } else if (operation != "EXTRACT") {
                throw std::runtime_error("Unknown operation");
            }
            std::string extra;
            if (input >> extra) throw std::runtime_error("Extra trace fields");
        } catch (const std::exception& error) {
            std::cerr << "Trace line " << line_number << ": " << error.what() << '\n';
            return 2;
        }

        std::ostringstream result;
        try {
            if (operation == "BUILD") {
                queue.build_heap(patients); result << "OK BUILD";
            } else if (operation == "INSERT") {
                queue.insert(patient); result << "OK INSERT";
            } else if (operation == "DECREASE") {
                queue.decrease_key(id, key); result << "OK DECREASE";
            } else if (operation == "DELETE") {
                queue.delete_patient(id); result << "OK DELETE";
            } else if (operation == "EXTRACT") {
                Patient answer = queue.extract_min();
                result << "PATIENT "; write_patient(result, answer);
            } else {
                // Copy and drain for observation; never drain the real queue.
                SelectedQueue copy = queue;
                result << "STATE " << count;
                for (int i = 0; i < count; ++i) {
                    Patient answer = copy.extract_min();
                    result << " | "; write_patient(result, answer);
                }
                try {
                    Patient extra = copy.extract_min();
                    result << " | EXTRA "; write_patient(result, extra);
                } catch (const std::out_of_range&) {
                    result << " | EMPTY";
                }
            }
            std::cout << result.str() << '\n';
        } catch (const std::out_of_range&) {
            std::cout << "ERROR out_of_range\n";
        } catch (const std::invalid_argument&) {
            std::cout << "ERROR invalid_argument\n";
        } catch (const std::exception&) {
            std::cout << "ERROR other_std_exception\n";
        } catch (...) {
            std::cout << "ERROR unknown_exception\n";
        }
        // Preserve the last completed response if a subsequent operation crashes.
        std::cout.flush();
    }
    return 0;
}
