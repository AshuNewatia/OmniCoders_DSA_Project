#include <iostream>
#include <vector>
#include <stdexcept>
#include <string>
#include <algorithm>

#include "locator_for_structure_A.cpp"
#include "../../app/ITriageQueue.h"

class Min_Heap : public ITriageQueue {
private:
    LookUp table;
    std::vector<Patient> Heap;
    int count = 0;

    void Sift_Down(int index) {
        if (2 * index + 1 >= count) {
            return;
        }

        if (2 * index + 2 >= count) {
            int left = 2 * index + 1;

            if (Heap[left].priority_key < Heap[index].priority_key) {
                std::swap(Heap[left], Heap[index]);
                table.interchange(Heap[index].id, Heap[left].id);
            }

            return;
        }

        int left = 2 * index + 1;
        int right = 2 * index + 2;

        int smallest =
            Heap[right].priority_key < Heap[left].priority_key
                ? right
                : left;

        if (Heap[index].priority_key > Heap[smallest].priority_key) {
            std::swap(Heap[index], Heap[smallest]);

            table.interchange(
                Heap[index].id,
                Heap[smallest].id
            );

            Sift_Down(smallest);
        }
    }

    void heapify() {
        for (int i = count / 2 - 1; i >= 0; i--) {
            Sift_Down(i);
        }
    }

    void Insertify(int index) {
        if (index <= 0) {
            return;
        }

        int parent = (index - 1) / 2;

        if (Heap[parent].priority_key > Heap[index].priority_key) {
            std::swap(Heap[parent], Heap[index]);

            table.interchange(
                Heap[index].id,
                Heap[parent].id
            );

            Insertify(parent);
        }
    }

public:
    Min_Heap() = default;

    Min_Heap(const std::vector<Patient>& patients) {
        build_heap(patients);
    }

    void build_heap(
        const std::vector<Patient>& patients
    ) override {
        table = LookUp();
        Heap = patients;
        count = static_cast<int>(Heap.size());

        for (int i = 0; i < count; i++) {
            table.Insert(twin(Heap[i].id, i));
        }

        heapify();
    }

    void insert(const Patient& patient) override {
        Heap.push_back(patient);
        count = static_cast<int>(Heap.size());

        table.Insert(twin(patient.id, count - 1));

        Insertify(count - 1);
    }

    Patient peek() {
        if (count == 0) {
            throw std::out_of_range(
                "No patients are currently waiting for triage."
            );
        }

        return Heap[0];
    }

    Patient extract_min() override {
        if (count == 0) {
            throw std::out_of_range(
                "No patients are currently waiting for triage."
            );
        }

        Patient minimum = Heap[0];

        std::swap(Heap[0], Heap[count - 1]);

        table.interchange(
            Heap[count - 1].id,
            Heap[0].id
        );

        table.Delete(Heap[count - 1].id);

        Heap.pop_back();
        count--;

        Sift_Down(0);

        return minimum;
    }

    void delete_patient(int id) override {
        int index = table.find(id);

        std::swap(Heap[index], Heap[count - 1]);

        table.interchange(
            Heap[index].id,
            Heap[count - 1].id
        );

        table.Delete(Heap[count - 1].id);

        Heap.pop_back();
        count--;

        Sift_Down(index);
    }

    void decrease_key(int id, long long new_key) override {
        int index = table.find(id);

        Heap[index].priority_key = new_key;

        Insertify(index);
    }
};