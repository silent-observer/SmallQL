#include "GroupDataSequence.h"

GroupifierDS::GroupifierDS(const IntermediateType& type, vector<bool> isGrouped,
    DataSequence* source) 
    : source(source)
    , data(make_unique<vector<ValueArray>>())
    , GroupDataSequence(type, isGrouped) {
    record.record = data.get();
}
void GroupifierDS::reset() {
    source->reset();
    data->clear();
    update();
}
void GroupifierDS::advance() {
    data->clear();
    if (source->hasEnded())
        ended = true;
    else
        update();
}
bool GroupifierDS::hasEnded() const {
    return ended;
}
void GroupifierDS::update() {
    while (!source->hasEnded()) {
        bool isEqual = true;
        auto srcRec = source->get().record;
        if (!data->empty()) {
            for (int i = 0; i < srcRec->size(); i++) {
                if (!record.isGrouped[i]) continue;
                if (record.type.entries[i].compare((*srcRec)[i], data->back()[i]) != 0) {
                    isEqual = false;
                    break;
                }
            }
        }
        if (!isEqual) return;
        data->push_back(*srcRec);
        source->advance();
    }
}

DegroupifierDS::DegroupifierDS(const IntermediateType& type, GroupDataSequence* source) 
    : source(source)
    , data(make_unique<ValueArray>(type.entries.size()))
    , DataSequence(type) {
    record.record = data.get();
}
void DegroupifierDS::reset() {
    source->reset();
    if (!source->hasEnded());
        update();
}
void DegroupifierDS::advance() {
    source->advance();
    if (!source->hasEnded())
        update();
}
bool DegroupifierDS::hasEnded() const {
    return source->hasEnded();
}
void DegroupifierDS::update() {
    int i = 0;
    const auto& t = source->getType();
    for (int j = 0; j < t.entries.size(); j++) {
        if (!source->get().isGrouped[j]) continue;
        (*data)[i] = (*source->get().record)[0][j];
        i++;
    }
}