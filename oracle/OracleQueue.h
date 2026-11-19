#ifndef ORACLE_QUEUE_H
#define ORACLE_QUEUE_H

#include <cstddef>
#include <vector>
#include "../app/ITriageQueue.h"

class OracleQueue : public ITriageQueue {
public:
    void build_heap(const std::vector<Patient>& patients) override;

    void insert(const Patient& patient) override;

    Patient extract_min() override;

    void decrease_key(int id, long long new_key) override;

    void delete_patient(int id) override;

    bool empty() const {
        return patients_.empty();
    }

    std::size_t size() const {
        return patients_.size();
    }

private:
    static bool comes_before(
        const Patient& left,
        const Patient& right
    );

    std::vector<Patient> patients_;
};

#endif