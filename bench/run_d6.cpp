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
        int repetitions = 5;
        unsigned long long base_seed = 12345;
        const bool scale_only = argc > 1 && std::string(argv[1]) == "--scale-only";
        if (scale_only) {
            if (argc > 2) repetitions = std::stoi(argv[2]);
            if (argc > 3) base_seed = std::stoull(argv[3]);
            if (argc > 4) throw std::invalid_argument("Usage: run_d6 --scale-only [repetitions>=5] [seed]");
        } else {
            if (argc > 1) repetitions = std::stoi(argv[1]);
            if (argc > 2) base_seed = std::stoull(argv[2]);
            if (argc > 3) throw std::invalid_argument("Usage: run_d6 [repetitions>=5, default 5] [seed]");
        }
        if (repetitions < 5)
            throw std::invalid_argument("at least five repetitions are required");

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
        const std::string results_tmp = results + ".tmp";
        const std::string temp = prefix + "d6_run_tmp.csv";

        const std::array<BuildTarget, 3> targets{{
            {"A", "TRIAGE_A"}, {"B", "TRIAGE_B"}, {"oracle", "TRIAGE_ORACLE"}
        }};
        std::array<std::string, 3> binaries;
        if (!scale_only) {
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

        std::ofstream output(results_tmp.c_str(), std::ios::trunc);
        if (!output) throw std::runtime_error("cannot create temporary results CSV");
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
                            throw std::runtime_error("treatment checksum mismatch for " +
                                std::string(policy) + ", revision share " + std::to_string(share) +
                                "%, seed " + std::to_string(seed) + ", implementation " +
                                targets[i].name);
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
        std::remove(results.c_str());
        if (std::rename(results_tmp.c_str(), results.c_str()) != 0)
            throw std::runtime_error("cannot finalize measurements CSV");
        std::cout << "Wrote measurements to " << results << '\n';
        }

        // Separate 10^6-live-patient scale run. The fixed-size micro-workload
        // isolates build-heap, revision and extract costs without making the
        // linear-scan oracle's revision path prohibitively large.
        const std::string scale_source = prefix + "d6_scale_benchmark.cpp";
        const std::string scale_results = launched_from_bench
            ? "../results/d6_scale_measurements.csv" : "results/d6_scale_measurements.csv";
        const std::string scale_results_tmp = scale_results + ".tmp";
        const std::string scale_temp = prefix + "d6_scale_tmp.csv";
        const std::string scale_patients = "1000000";
        const int scale_share = 90;
        std::array<std::string, 3> scale_binaries;
        for (std::size_t i = 0; i < targets.size(); ++i) {
            scale_binaries[i] = prefix + "d6_scale_" + targets[i].name + ".exe";
            std::string command = "g++ -std=c++17 -O2 -DNDEBUG -Wall -Wextra -D";
            command += targets[i].define;
            command += " " + quote(scale_source) + " -o " + quote(scale_binaries[i]);
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

        std::ofstream scale_output(scale_results_tmp.c_str(), std::ios::trunc);
        if (!scale_output) throw std::runtime_error("cannot create temporary scale results CSV");
        bool wrote_scale_header = false;
        for (int repetition = 0; repetition < repetitions; ++repetition) {
            const unsigned long long seed = base_seed + repetitions + repetition;
            std::string scale_checksum;
            for (std::size_t i = 0; i < targets.size(); ++i) {
                std::string executable = scale_binaries[i];
#ifdef _WIN32
                executable = ".\\" + executable;
                for (char& ch : executable) if (ch == '/') ch = '\\';
#endif
                const std::string command = executable + " " + scale_patients + " " +
                    std::to_string(scale_share) + " " + std::to_string(seed) +
                    " > " + scale_temp;
                if (run_command(command) != 0) return 1;
                std::ifstream input(scale_temp.c_str());
                std::string header, row;
                if (!std::getline(input, header) || !std::getline(input, row))
                    throw std::runtime_error("missing scale benchmark output");
                if (!wrote_scale_header) {
                    scale_output << header << ",repetition\n";
                    wrote_scale_header = true;
                }
                const std::vector<std::string> fields = split_csv(row);
                if (fields.size() < 14)
                    throw std::runtime_error("unexpected scale benchmark CSV format");
                const std::string checksum = fields[13];
                if (scale_checksum.empty()) scale_checksum = checksum;
                else if (checksum != scale_checksum)
                    throw std::runtime_error("scale treatment checksums differ across implementations");
                scale_output << row << ',' << repetition << '\n';
                std::cout << "scale n=" << scale_patients << " rep=" << repetition << ' '
                          << targets[i].name << ": build " << fields[5]
                          << " ns, operations " << fields[8] << " ns\n";
            }
        }
        scale_output.close();
        std::remove(scale_temp.c_str());
        std::remove(scale_results.c_str());
        if (std::rename(scale_results_tmp.c_str(), scale_results.c_str()) != 0)
            throw std::runtime_error("cannot finalize scale measurements CSV");
        std::cout << "Wrote 10^6-scale measurements to " << scale_results << '\n';

#ifdef _WIN32
        const std::string analysis = launched_from_bench
            ? "python ../bench/analyze_d6.py" : "python bench/analyze_d6.py";
        const std::string analysis_fallback = launched_from_bench
            ? "py -3 ../bench/analyze_d6.py" : "py -3 bench/analyze_d6.py";
#else
        const std::string analysis = launched_from_bench
            ? "python3 ../bench/analyze_d6.py" : "python3 bench/analyze_d6.py";
        const std::string analysis_fallback = analysis;
#endif
        if (run_command(analysis) != 0 && run_command(analysis_fallback) != 0) {
            std::cerr << "Analysis step failed. Run the plotting/report script manually with Python 3 and ReportLab.\n";
            return 1;
        }
    } catch (const std::exception& error) {
        std::cerr << "D6 runner error: " << error.what() << '\n';
        return 2;
    }
    return 0;
}
