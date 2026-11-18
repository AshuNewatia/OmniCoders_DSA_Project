#ifndef TRIAGE_QUEUE_INTERFACE_H
#define TRIAGE_QUEUE_INTERFACE_H

#include <vector>

struct Patient {
    int id;
    int severity;
    int arrival_time;
    long long priority_key;
};

class ITriageQueue {
public:
    virtual ~ITriageQueue() = default;

    virtual void build_heap(const std::vector<Patient>& patients) = 0;

    virtual void insert(const Patient& patient) = 0;

    virtual Patient extract_min() = 0;

    virtual void decrease_key(int id, long long new_key) = 0;

    virtual void delete_patient(int id) = 0;
};

#endif