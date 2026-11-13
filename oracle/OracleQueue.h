#ifndef ORACLE_QUEUE_H
#define ORACLE_QUEUE_H

#include "../app/ITriageQueue.h"

// Reference implementation: patients are stored in an unsorted array.
class OracleQueue : public ITriageQueue {
public:
    void build_heap(const std::vector<Patient>& patients) override;
    void insert(const Patient& patient) override;
    Patient extract_min() override;
    void decrease_key(int id, int new_severity) override;
    void delete_patient(int id) override;

private:
    std::vector<Patient> patients_;
};

#endif
