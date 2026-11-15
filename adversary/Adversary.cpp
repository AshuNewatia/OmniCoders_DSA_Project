struct Patient {
    int id;
    int severity;
    int arrival_time;
};

static std::uint64_t reverse_bits(std::uint64_t x, unsigned bits) {
    std::uint64_t result = 0;
    for (unsigned i = 0; i < bits; ++i) {
        result = (result << 1U) | (x & 1U);
        x >>= 1U;
    }
    return result;
}

static std::vector<Patient> generate_patients(
    int n, const std::string& pattern, std::uint64_t seed) {

    if (n <= 0) {
        throw std::invalid_argument("n must be positive");
    }

    std::vector<int> values(static_cast<std::size_t>(n));
    std::iota(values.begin(), values.end(), 1);

    if (pattern == "ascending") {
        // Severity values are 1, 2, ..., n.
    } else if (pattern == "descending") {
        std::reverse(values.begin(), values.end());
    } else if (pattern == "alternating") {
        // Low, high, next-low, next-high, ...
        std::vector<int> arranged;
        arranged.reserve(values.size());
        int lo = 1, hi = n;
        while (lo <= hi) {
            arranged.push_back(lo++);
            if (lo <= hi) arranged.push_back(hi--);
        }
        values.swap(arranged);
    } else if (pattern == "sawtooth") {
        // Repeatedly rises and falls in short blocks.
        constexpr int block = 16;
        for (int i = 0; i < n; ++i) {
            int offset = i % block;
            int cycle = i / block;
            values[static_cast<std::size_t>(i)] =
                cycle * block + (offset < block / 2
                    ? offset + 1
                    : block - offset);
        }
        // Keep values unique and in [1,n] by sorting the generated ranks
        // and replacing them with their rank order.
        std::vector<int> order(n);
        std::iota(order.begin(), order.end(), 0);
        std::stable_sort(order.begin(), order.end(), [&](int a, int b) {
            if (values[a] != values[b]) return values[a] < values[b];
            return a < b;
        });
        std::vector<int> ranked(n);
        for (int rank = 0; rank < n; ++rank)
            ranked[static_cast<std::size_t>(order[rank])] = rank + 1;
        values.swap(ranked);
    } else if (pattern == "bit-reversal") {
        unsigned bits = 0;
        while ((std::uint64_t(1) << bits) < static_cast<std::uint64_t>(n))
            ++bits;
        std::vector<std::pair<std::uint64_t, int>> keyed;
        keyed.reserve(static_cast<std::size_t>(n));
        for (int i = 0; i < n; ++i)
            keyed.emplace_back(reverse_bits(static_cast<std::uint64_t>(i), bits), i);
        std::stable_sort(keyed.begin(), keyed.end());
        for (int rank = 0; rank < n; ++rank)
            values[static_cast<std::size_t>(rank)] = keyed[rank].second + 1;
    } else if (pattern == "random") {
        std::mt19937_64 rng(seed);
        std::shuffle(values.begin(), values.end(), rng);
    } else if (pattern == "equal-reverse-arrival") {
        // Equal severity makes the comparator resolve priority by arrival,
        // then ID. Reverse arrival order stresses tie handling deterministically.
    } else {
        throw std::invalid_argument(
            "unknown pattern; use ascending, descending, alternating, "
            "sawtooth, bit-reversal, random, or equal-reverse-arrival");
    }

    std::vector<Patient> patients;
    patients.reserve(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) {
        Patient p{};
        p.id = i + 1;
        if (pattern == "equal-reverse-arrival") {
            p.severity = 25;
            p.arrival_time = n - i;
        } else {
            p.severity = values[static_cast<std::size_t>(i)];
            p.arrival_time = i;
        }
        patients.push_back(p);
    }
    return patients;
}

int main(int argc, char** argv) {
    if (argc < 3 || argc > 4) {
        std::cerr
            << "Usage: " << argv[0] << " <n> <pattern> [seed]\n"
            << "Patterns: ascending descending alternating sawtooth "
               "bit-reversal random equal-reverse-arrival\n";
        return 2;
    }

    try {
        const int n = std::stoi(argv[1]);
        const std::string pattern = argv[2];
        const std::uint64_t seed =
            argc == 4 ? std::stoull(argv[3]) : 12345ULL;

        const auto patients = generate_patients(n, pattern, seed);

        std::cout << "id,severity,arrival_time\n";
        for (const Patient& p : patients) {
            std::cout << p.id << ',' << p.severity << ','
                      << p.arrival_time << '\n';
        }
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << '\n';
        return 1;
    }
