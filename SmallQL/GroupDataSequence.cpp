#include "GroupDataSequence.h"
#include "QueryTree.h"
#include "ComputerVisitor.h"

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
    ended = false;
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


AggregatorDS::AggregatorDS(const IntermediateType& type, vector<bool> isGrouped,
    GroupDataSequence* source,
    vector<unique_ptr<QScalarNode>> funcs)
    : source(source)
    , funcs()
    , data(make_unique<vector<ValueArray>>())
    , GroupDataSequence(type, isGrouped) {
    record.record = data.get();
    for (auto& f : funcs)
        this->funcs.push_back(move(f));
    visitor = make_unique<GroupComputerVisitor>(type, record.record);
}
void AggregatorDS::reset() {
    source->reset();
    if (!source->hasEnded())
        update();
}
void AggregatorDS::advance() {
    source->advance();
    if (!source->hasEnded())
        update();
}
bool AggregatorDS::hasEnded() const {
    return source->hasEnded();
}
void AggregatorDS::update() {
    data->clear();
    for (int i = 0; i < source->get().record->size(); i++)
        data->push_back((*source->get().record)[i]);
    visitor->updateRecord();

    ValueArray results;
    for (int i = 0; i < funcs.size(); i++) {
        funcs[i]->accept(visitor.get());
        results.push_back(visitor->getResult());
    }

    if (data->empty())
        data->emplace_back(source->getType().entries.size(), Value(ValueType::Null));
    (*data)[0].insert((*data)[0].end(), results.begin(), results.end());
}