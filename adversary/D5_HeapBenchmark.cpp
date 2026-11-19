#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

unsigned long long d5_comparisons = 0;
#ifdef TEST_STRUCTURE_A
#include "Structure_A.cpp"
#else
#include "Structure_B.cpp"
#endif

static std::vector<Patient> read_csv(const std::string& path) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("cannot open CSV: "+path);
    std::string line;
    if (!std::getline(in,line) || line!="id,severity,arrival_time,priority_key")
        throw std::runtime_error("unexpected CSV header");
    std::vector<Patient> patients;
    std::unordered_set<int> ids;
    int line_no=1;
    while (std::getline(in,line)) {
        ++line_no;
        if (line.empty()) continue;
        std::stringstream ss(line);
        std::string a,b,c,d,extra;
        if (!std::getline(ss,a,',') || !std::getline(ss,b,',') ||
            !std::getline(ss,c,',') || !std::getline(ss,d,',') ||
            std::getline(ss,extra,','))
            throw std::runtime_error("expected 4 fields at line "+std::to_string(line_no));
        Patient p{};
        try {
            std::size_t x,y,z,w;
            p.id=std::stoi(a,&x); p.severity=std::stoi(b,&y);
            p.arrival_time=std::stoi(c,&z); p.priority_key=std::stoll(d,&w);
            if(x!=a.size()||y!=b.size()||z!=c.size()||w!=d.size())
                throw std::runtime_error("bad integer");
        } catch (...) { throw std::runtime_error("invalid integer at line "+std::to_string(line_no)); }
        if (!ids.insert(p.id).second) throw std::runtime_error("duplicate patient ID");
        patients.push_back(p);
    }
    if (patients.empty()) throw std::runtime_error("no patient rows");
    return patients;
}
int main(int argc,char** argv) {
    if(argc!=2) { std::cerr<<"Usage: heap_benchmark <patients.csv>\n"; return 2; }
    try {
        auto patients=read_csv(argv[1]);
#ifdef TEST_STRUCTURE_A
        Min_Heap queue(patients);
        const char* name="Structure_A";
#else
        LazyTriageQueue queue;
        queue.build_heap(patients);
        const char* name="Structure_B";
#endif
        d5_comparisons=0; // Exclude construction; count only extract_min work.
        long long previous=std::numeric_limits<long long>::min();
        for(std::size_t i=0;i<patients.size();++i) {
            Patient p=queue.extract_min();
            if(p.priority_key<previous) throw std::runtime_error("queue returned out-of-order priority keys");
            previous=p.priority_key;
        }
        std::cout<<"implementation,n,extract_comparisons\n"
                 <<name<<','<<patients.size()<<','<<d5_comparisons<<'\n';
    } catch(const std::exception& e) {
        std::cerr<<"Benchmark error: "<<e.what()<<'\n'; return 1;
    }
}
