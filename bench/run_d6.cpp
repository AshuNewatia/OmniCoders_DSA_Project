#include <array>
#include <cstdlib>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

struct BuildTarget {
    const char* name;
    const char* define;
};

static std::string quote(const std::string& path) {
    return "\"" + path + "\"";
}

static std::vector<std::string> split_csv(const std::string& line) {
    std::vector<std::string> fields;
    std::size_t start = 0;
    for (;;) {
        const std::size_t comma = line.find(',', start);
        if (comma == std::string::npos) {
            fields.push_back(line.substr(start));
            return fields;
        }
        fields.push_back(line.substr(start, comma - start));
        start = comma + 1;
    }
}

static int run_command(const std::string& command) {
    std::cout << command << '\n' << std::flush;
    const int result = std::system(command.c_str());
    if (result != 0) {
        std::cerr << "Command failed with status " << result << '\n';
    }
    return result;
}

static bool file_exists(const std::string& path) {
    std::ifstream input(path.c_str());
    return input.good();
}

int main(int argc, char** argv) {
    try {
        int repetitions = 3;
        unsigned long long base_seed = 12345;
        if (argc > 1) repetitions = std::stoi(argv[1]);
        if (argc > 2) base_seed = std::stoull(argv[2]);
        if (argc > 3 || repetitions < 1)
            throw std::invalid_argument("Usage: run_d6 [repetitions] [seed]");

        // Accept either the repository root or its bench/ directory as CWD.
        // Relative paths avoid requiring <filesystem> from older MinGW builds.
        const bool launched_from_bench = !file_exists("bench/d6_benchmark.cpp") &&
                                         file_exists("d6_benchmark.cpp");
        if (!launched_from_bench && !file_exists("bench/d6_benchmark.cpp"))
            throw std::runtime_error("run this program from the repository root or bench directory");
        const std::string prefix = launched_from_bench ? "" : "bench/";
        const std::string source = prefix + "d6_benchmark.cpp";
        const std::string results = launched_from_bench
            ? "../results/d6_measurements.csv" : "results/d6_measurements.csv";
        const std::string temp = prefix + "d6_run_tmp.csv";

        const std::array<BuildTarget, 3> targets{{
            {"A", "TRIAGE_A"}, {"B", "TRIAGE_B"}, {"oracle", "TRIAGE_ORACLE"}
        }};
        std::array<std::string, 3> binaries;
        for (std::size_t i = 0; i < targets.size(); ++i) {
            binaries[i] = prefix + "d6_" + targets[i].name + ".exe";
            std::string command = "g++ -std=c++17 -O2 -DNDEBUG -Wall -Wextra -D";
            command += targets[i].define;
            command += " " + quote(source) + " -o " + quote(binaries[i]);
            if (std::string(targets[i].name) == "oracle") {
                const std::string oracle_source = launched_from_bench
                    ? "../oracle/OracleQueue.cpp" : "oracle/OracleQueue.cpp";
                command += " " + quote(oracle_source);
            }
#ifdef _WIN32
            command += " -lpsapi";
#endif
            if (run_command(command) != 0) return 1;
        }

        std::ofstream output(results.c_str(), std::ios::trunc);
        if (!output) throw std::runtime_error("cannot create results CSV");
        bool wrote_header = false;
        for (const char* policy : {"linear", "stepwise"}) {
            for (int share = 0; share <= 90; share += 10) {
                for (int repetition = 0; repetition < repetitions; ++repetition) {
                    const unsigned long long seed = base_seed + repetition;
                    std::string expected_checksum;
                    for (std::size_t i = 0; i < targets.size(); ++i) {
                        // MinGW's system() routes through cmd.exe on Windows.
                        // Starting a quoted bare executable name is parsed
                        // inconsistently by cmd, so use an explicit .\ path.
                        std::string executable = binaries[i];
#ifdef _WIN32
                        executable = ".\\" + executable;
                        for (char& ch : executable)
                            if (ch == '/') ch = '\\';
#endif
                        std::string command = executable + " " + policy + " " +
                            std::to_string(share) + " " + std::to_string(seed) + " > " + temp;
                        if (run_command(command) != 0) return 1;
                        std::ifstream input(temp.c_str());
                        std::string header, row;
                        if (!std::getline(input, header) || !std::getline(input, row))
                            throw std::runtime_error("missing benchmark output");
                        if (!wrote_header) {
                            output << header << ",repetition\n";
                            wrote_header = true;
                        }
                        const std::vector<std::string> fields = split_csv(row);
                        if (fields.size() < 24)
                            throw std::runtime_error("unexpected benchmark CSV format");
                        const std::string checksum = fields[23];
                        if (expected_checksum.empty()) expected_checksum = checksum;
                        else if (checksum != expected_checksum) {
                            std::cerr << "WARNING: treatment checksum mismatch for " << policy
                                      << ", revision share " << share << "%, seed " << seed
                                      << ", implementation " << targets[i].name
                                      << "; do not treat this as an apples-to-apples comparison.\n";
                        }
                        output << row << ',' << repetition << '\n';
                        std::cout << policy << ' ' << share << "% rep=" << repetition << ' '
                                  << targets[i].name << ": " << fields[16]
                                  << " ns, peak working set " << fields[22] << " bytes\n";
                    }
                }
            }
        }
        output.close();
        std::remove(temp.c_str());
        std::cout << "Wrote measurements to " << results << '\n';
    } catch (const std::exception& error) {
        std::cerr << "D6 runner error: " << error.what() << '\n';
        return 2;
    }
    return 0;
}
