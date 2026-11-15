#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

/*
 * D5 Oracle benchmark.
 * Reads CSV: id,severity,arrival_time
 * Counts calls to the same three-field ordering used by OracleQueue.
 *
 * Build:
 *   g++ -std=c++17 -O2 -Wall -Wextra -Wpedantic AdversaryBenchmark.cpp -o benchmark
 * Run:
 *   ./benchmark patients.csv [results.csv]
 *
 * The count is comparator INVOCATIONS, not primitive integer comparisons.
 */

struct Patient {
    int id;
    int severity;
    int arrival_time;
};

static bool comes_before(const Patient& a, const Patient& b,
                         unsigned long long& comparator_calls) {
    ++comparator_calls;
    if (a.severity != b.severity) return a.severity < b.severity;
    if (a.arrival_time != b.arrival_time)
        return a.arrival_time < b.arrival_time;
    return a.id < b.id;
}

static std::vector<Patient> read_csv(const std::string& path) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("cannot open input CSV: " + path);

    std::string line;
    if (!std::getline(in, line))
        throw std::runtime_error("CSV is empty");

    std::vector<Patient> patients;
    int line_number = 1;
    while (std::getline(in, line)) {
        ++line_number;
        if (line.empty()) continue;
        std::stringstream ss(line);
        std::string a, b, c, extra;
        if (!std::getline(ss, a, ',') || !std::getline(ss, b, ',') ||
            !std::getline(ss, c, ',') || std::getline(ss, extra, ',')) {
            throw std::runtime_error("expected exactly 3 columns at line " +
                                     std::to_string(line_number));
        }
        try {
            std::size_t p1, p2, p3;
            Patient p{std::stoi(a, &p1), std::stoi(b, &p2), std::stoi(c, &p3)};
            if (p1 != a.size() || p2 != b.size() || p3 != c.size())
                throw std::runtime_error("invalid integer");
            patients.push_back(p);
        } catch (...) {
            throw std::runtime_error("invalid integer at line " +
                                     std::to_string(line_number));
        }
    }
    return patients;
}

int main(int argc, char** argv) {
    if (argc < 2 || argc > 3) {
        std::cerr << "Usage: " << argv[0] << " <patients.csv> [results.csv]\n";
        return 2;
    }

    try {
        auto patients = read_csv(argv[1]);
        const std::size_t n = patients.size();
        unsigned long long comparisons = 0;

        // Mirror OracleQueue::extract_min: scan, compare against current best,
        // replace the selected slot with the final element, then pop_back.
        while (!patients.empty()) {
            std::size_t best = 0;
            for (std::size_t i = 1; i < patients.size(); ++i) {
                if (comes_before(patients[i], patients[best], comparisons))
                    best = i;
            }
            patients[best] = patients.back();
            patients.pop_back();
        }

        const unsigned long long expected =
            static_cast<unsigned long long>(n) * (n == 0 ? 0 : n - 1) / 2;

        std::cout << "n=" << n << "\n"
                  << "comparator_calls=" << comparisons << "\n"
                  << "expected_calls=" << expected << "\n";

        if (comparisons != expected) {
            std::cerr << "Unexpected comparator count; check benchmark logic.\n";
            return 1;
        }

        if (argc == 3) {
            std::ofstream out(argv[2]);
            if (!out) throw std::runtime_error("cannot open output CSV");
            out << "n,comparator_calls,expected_calls\n"
                << n << ',' << comparisons << ',' << expected << '\n';
        }
    } catch (const std::exception& ex) {
        std::cerr << "Benchmark error: " << ex.what() << '\n';
        return 1;
    }
    return 0;
}
