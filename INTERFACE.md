# Common Interface and Structural Invariants

This document defines the common interface and structural invariants for the Emergency Triage Queue simulator. It must be strictly followed by Structure A, Structure B, and the Oracle.

## 1. The Common Interface
Structure A, Structure B, and the Oracle must all implement this common C++ interface to ensure they can be used interchangeably by the triage simulator application and the differential test harness.

```cpp
#ifndef TRIAGE_QUEUE_INTERFACE_H
#define TRIAGE_QUEUE_INTERFACE_H

#include <vector>

// Represents a patient in the triage casualty ward
struct Patient {
    int id;             // Unique patient identifier
    int severity;       // Base severity score
    int arrival_time;   // Arrival timestamp for aging policies
};

// The common priority queue interface
class ITriageQueue {
public:
    virtual ~ITriageQueue() = default;

    // Constructs the initial heap from a given set of patients in O(n) time
    virtual void build_heap(const std::vector<Patient>& patients) = 0;

    // Inserts a new patient into the queue
    virtual void insert(const Patient& p) = 0;

    // Removes and returns the patient with the highest priority (minimum key)
    virtual Patient extract_min() = 0;

    // Raises a waiting patient's severity by lowering their priority key
    virtual void decrease_key(int id, int new_severity) = 0;

    // Removes a specific patient who leaves before receiving treatment
    virtual void delete_patient(int id) = 0; 
};

#endif // TRIAGE_QUEUE_INTERFACE_H