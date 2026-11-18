#include <iostream>
#include <vector>
#include <stdexcept>
#include <string>
#include <algorithm>
#include "locator_for_structure_A.cpp"
#include "../../app/ITriageQueue.h"


//Insert
//Revision
//Access Min
//Pop Min 
//Delete ID
//O(1) access to records.


class Min_Heap: public ITriageQueue {
    private: 
    LookUp table;
    std::vector<Patient> Heap;
    int count = 0;
    //Heapify - The Floyd's Method
    void Sift_Down(int sweet){
        if(2*sweet+1 >= count) return;
        else if (2*sweet + 2>= count){
            if(Heap[2*sweet+1].priority_key < Heap[sweet].priority_key){
                std::swap(Heap[2*sweet+1], Heap[sweet]);
                table.interchange(Heap[sweet].id, Heap[2*sweet+1].id);
            }
        }
        else{
            int min = Heap[2*sweet+2].priority_key < Heap[2*sweet+1].priority_key ? 2*sweet+2 : 2*sweet + 1;
            if(Heap[sweet].priority_key>Heap[min].priority_key){
                std::swap(Heap[sweet], Heap[min]);
                table.interchange(Heap[sweet].id, Heap[min].id);
                Sift_Down(min);
            }
        }
    }
    void build_heap() override {
        for(int i = (count-1)/2 -1; i>=0; i--){
            Sift_Down(i);
        }
    }
    //Insertion Drama
    void Insertify(int kill){
        if( (kill-1)/2  < 0) return;
        else {
            if(Heap[(kill-1)/2].priority_key >Heap[kill].priority_key){
                std::swap(Heap[(kill-1)/2], Heap[kill]);
                table.interchange(Heap[kill].id, Heap[(kill-1)/2].id);
                Insertify((kill-1)/2);
            }
        }
    }


    public:

    Min_Heap()=default;
    Min_Heap(std::vector<Patient> &tri){
        // Insert(tri);
        Heap.resize(tri.size());
        count = tri.size();
        for(int i = 0; i<tri.size(); i++){
            table.Insert(twin(tri[i].id, i));
            Heap[i] = tri[i];
        }
        build_heap();
    }

    void insert(const Patient& patient) override {
        Heap.emplace_back(patient);
        count = Heap.size();
        twin tw(patient.id, count-1);
        table.Insert(tw);
        Insertify(count-1);
    }
    void insert(std::vector<Patient> &tri){
        int n = tri.size();
        for(int i = 0; i<n; i++){
            Heap.emplace_back(tri[i]);
            count = Heap.size();
            twin tw(tri[i].id, count-1);
            table.Insert(tw);
            Insertify(count-1);
        }
    }

    Patient peek(){
        if(count == 0){
            throw std::out_of_range("No patients are currently waiting for triage.");;
        }
        return Heap[0];
    }
    Patient extract_min() override {
        if(count == 0){
            throw std::out_of_range("No patients are currently waiting for triage.");;
        }
        Patient minnie = Heap[0];
        std::swap(Heap[0], Heap[count-1]);
        table.interchange(Heap[count-1].id, Heap[0].id);
        table.Delete(Heap[count-1].id);
        count--;
        Heap.pop_back();
        Sift_Down(0);
        return minnie;
    }

    void delete_patient(int id) override {
        int val = table.find(id);
        std::swap(Heap[val], Heap[count-1]);
        table.interchange(Heap[val].id, Heap[count-1].id);
        table.Delete(Heap[count-1].id);
        count--;
        Heap.pop_back();
        Sift_Down(val);
    }

    void decrease_key(int id, long long new_key) override {
        int val = table.find(id);
        Heap[val].priority_key = new_key;
        Insertify(val);
    }
    
};
