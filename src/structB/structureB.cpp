#include "../../app/ITriageQueue.h"
#include <vector>
#include <stdexcept>
#include <algorithm>

using namespace std;

struct HeapEntry {
    Patient p;
    int version;
};

struct twin {
    int ID;
    int version; 
    Patient p_data;
    
    twin(int id = -1, int v = -1, Patient patient = {-1, -1, -1, -1}) 
        : ID(id), version(v), p_data(patient) {}
};

class LookUp {
private:
    int CAP = 1024;
    vector<twin> tab;
    int state_count = 0;
    twin e{-1, -1, {-1, -1, -1, -1}}; 

    long long hashie(int input) {
        return (long long)input * 2654435761 % CAP; 
    }

    void resize() {
        vector<twin> old_tab = tab;
        CAP *= 2;
        tab.assign(CAP, e);
        state_count = 0;
        for (const auto& item : old_tab) {
            if (item.ID != -1) {
                Insert(item);
            }
        }
    }

public:
    LookUp() {
        tab.assign(CAP, e); 
    }

    void Insert(twin in) {
        if (state_count > CAP / 2) resize();

        int put = hashie(in.ID) % CAP;
        int start = put;
        
        while (tab[put].ID != -1) {
            if (tab[put].ID == in.ID) {
                tab[put].version = in.version;
                tab[put].p_data = in.p_data;
                return;
            }
            put = (put + 1) % CAP;
            if (put == start) break; 
        }
        
        put = hashie(in.ID) % CAP;
        while (tab[put].ID != -1) {
            put = (put + 1) % CAP;
        }
        tab[put] = in;
        state_count++;
    }

    int find_version(int ID) {
        int ch = hashie(ID) % CAP;
        int start = ch;
        while (tab[ch].ID != -1) {
            if (tab[ch].ID == ID) {
                return tab[ch].version;
            }
            ch = (ch + 1) % CAP;
            if (ch == start) break; 
        }
        return -1; 
    }

    Patient get_patient(int ID) {
        int ch = hashie(ID) % CAP;
        while (tab[ch].ID != -1) {
            if (tab[ch].ID == ID) return tab[ch].p_data;
            ch = (ch + 1) % CAP;
        }
        return {-1, -1, -1, -1};
    }

    void clear() {
        CAP = 1024;
        tab.assign(CAP, e);
        state_count = 0;
    }
};

class LazyTriageQueue : public ITriageQueue {
private:
    vector<HeapEntry> Heap;
    LookUp Table; 

    int stale_c = 0;
    int live_c = 0;

    int parent(int i) { return (i - 1) / 2; }
    int Right_C(int i) { return 2 * i + 2; } 
    int Left_C(int i) { return 2 * i + 1; }  

    bool comes_before(const Patient& a, const Patient& b) const {
        #ifdef D5_COUNT_COMPARISONS
              ++d5_comparisons;
        #endif
        if (a.priority_key != b.priority_key)
            return a.priority_key < b.priority_key;
        if (a.arrival_time != b.arrival_time)
            return a.arrival_time < b.arrival_time;
        return a.id < b.id;
    }

    void build_up(int i) {
        while (i > 0 && comes_before(Heap[i].p, Heap[parent(i)].p)) {
            swap(Heap[parent(i)], Heap[i]);
            i = parent(i);
        }
    }

    void build_down(int i) {
        int n = Heap.size();
        while (Left_C(i) < n) {
            int min_idx = Left_C(i);
            int right = Right_C(i);

            if (right < n && comes_before(Heap[right].p, Heap[min_idx].p)) { 
                min_idx = right;
            }
            if (comes_before(Heap[i].p, Heap[min_idx].p)) { 
                break;
            }
            swap(Heap[min_idx], Heap[i]);
            i = min_idx;
        }
    }

    void internal_build_heap() {
        if (Heap.empty()) return;
        for (int i = Heap.size() / 2 - 1; i >= 0; i--) { 
            build_down(i); 
        }
    }

    void Compact() {
        vector<HeapEntry> newHeap;

        for (const auto& node : Heap) {
            if (node.version == Table.find_version(node.p.id)) { 
                newHeap.push_back(node);
            }
        }

        Heap = move(newHeap);
        stale_c = 0;
        internal_build_heap();
    }

    void CheckComp() {
        if (stale_c > live_c) {
            Compact();
        }
    }

public:
    LazyTriageQueue() = default;

    void build_heap(const vector<Patient>& patients) override {
        Heap.clear();
        Table.clear();
        live_c = 0;
        stale_c = 0;
        
        for (const auto& patient : patients) {
            
            if (Table.find_version(patient.id) != -1) {
                throw invalid_argument("build_heap input contains duplicate IDs.");
            }
            Table.Insert(twin(patient.id, 1, patient));
            Heap.push_back({patient, 1});
            live_c++;
        }
        internal_build_heap();
    }

    void insert(const Patient& patient) override {
        
        if (Table.find_version(patient.id) != -1) {
            throw invalid_argument("Cannot insert: Patient ID is already live.");
        }
        
        Table.Insert(twin(patient.id, 1, patient)); 
        Heap.push_back({patient, 1});
        build_up(Heap.size() - 1);
        live_c++;
    }

    void decrease_key(int id, long long new_key) override {
        int current_version = Table.find_version(id);
        
        if (current_version == -1) {
            throw out_of_range("decrease_key: Patient ID is not live.");
        }
        
        Patient p = Table.get_patient(id);
        
        if (new_key > p.priority_key) {
            throw invalid_argument("decrease_key: new_key is larger than current key.");
        }
        
        p.priority_key = new_key;          
        
        int new_version = current_version + 1;
        Table.Insert(twin(id, new_version, p)); 
        
        Heap.push_back({p, new_version});
        build_up(Heap.size() - 1);
        
        stale_c++; 
        CheckComp(); 
    }

    void delete_patient(int id) override {
        int current_version = Table.find_version(id);
        
        if (current_version == -1) {
            throw out_of_range("delete_patient: Patient ID is not live.");
        }

        Table.Insert(twin(id, -1)); 
        live_c--;
        stale_c++;
        
        CheckComp();
    }

    Patient extract_min() override {
        
        if (live_c == 0) {
            throw out_of_range("extract_min: Triage queue is empty.");
        }

        while (!Heap.empty()) {
            HeapEntry root = Heap[0];
            
            Heap[0] = Heap.back();
            Heap.pop_back();
            if (!Heap.empty()) {
                build_down(0); 
            }

            if (root.version == Table.find_version(root.p.id)) {
                Table.Insert(twin(root.p.id, -1)); 
                live_c--;
                return root.p; 
            } else {
                stale_c--;
            }
        }
        throw out_of_range("extract_min: Heap structurally empty despite live_c count.");
    }
};
