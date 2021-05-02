#include "DataSequence.h"
#include "QueryTree.h"
#include "ComputerVisitor.h"
#include <algorithm>

TableFullScanDS::TableFullScanDS(const Schema& schema, DataFile& data)
    : data(data)
    , iter(makeConst(data).begin())
    , recordData(make_unique<ValueArray>())
    , schema(schema)
    , DataSequence(IntermediateType(schema)) {
    record.record = recordData.get();
}
void TableFullScanDS::reset() {
    iter = makeConst(data).begin();
    if (iter != makeConst(data).end()) {
        *recordData = schema.decode(*iter);
    }
}
void TableFullScanDS::advance() {
    if (iter == makeConst(data).end()) return;
    iter++;
    *recordData = schema.decode(*iter);
}
bool TableFullScanDS::hasEnded() const {
    return iter == makeConst(data).end();
}


TableIndexScanDS::TableIndexScanDS(const Schema& schema, DataFile& data, IndexFile& index,
        ValueArray from, ValueArray to, bool incFrom, bool incTo)
    : data(data)
    , index(index)
    , iter(index.end())
    , from(from)
    , to(to)
    , incFrom(incFrom)
    , incTo(incTo)
    , schema(schema)
    , recordData(make_unique<ValueArray>())
    , DataSequence(IntermediateType(schema)) {
    record.record = recordData.get();
}
void TableIndexScanDS::reset() {
    iter = index.getIterator(from);
    if (iter == index.end()) return;

    *recordData = schema.decode(data.readRecord(*iter));
    if (!incFrom) {
        while (index.getKeySchema().compare(from, *recordData) == 0) {
            iter++;
            *recordData = schema.decode(data.readRecord(*iter));
            if (iter == index.end()) return;
        }
    }
}
void TableIndexScanDS::advance() {
    if (iter == index.end()) return;
    iter++;
    *recordData = schema.decode(data.readRecord(*iter));
    int cmpVal = index.getKeySchema().compare(to, *recordData);
    if (cmpVal < 0 || !incTo && cmpVal == 0)
        iter = index.end();
}
bool TableIndexScanDS::hasEnded() const {
    return iter == index.end();
}


ProjectorDS::ProjectorDS(const IntermediateType& type,
    DataSequence* source,
    vector<uint16_t> columns)
    : source(source)
    , recordData(make_unique<ValueArray>(columns.size()))
    , columns(columns)
    , DataSequence(type) {
    record.record = recordData.get();
}

void ProjectorDS::reset() {
    source->reset();
    if (!source->hasEnded())
        update();
}
void ProjectorDS::advance() {
    source->advance();
    if (!source->hasEnded())
        update();
}
bool ProjectorDS::hasEnded() const {
    return source->hasEnded();
}
void ProjectorDS::update() {
    const ValueArray& input = *source->get().record;
    for (int i = 0; i < columns.size(); i++)
        (*recordData)[i] = input[columns[i]];
}

class CondCheckerVisitor : public QConditionNode::Visitor {
private:
    const IntermediateType& schema;
    const ValueArray* record;
    bool result;
public:
    CondCheckerVisitor(const IntermediateType& schema, const ValueArray* record)
        : schema(schema), record(record) {}
    inline bool getResult() const {
        return result;
    }
    inline void updateRecord(const ValueArray* newRecord) {
        record = newRecord;
    }
    virtual void visitAndConditionQNode(AndConditionQNode& n);
    virtual void visitOrConditionQNode(OrConditionQNode& n);
    virtual void visitCompareConditionQNode(CompareConditionQNode& n);
};

void CondCheckerVisitor::visitOrConditionQNode(OrConditionQNode& n) {
    for (const auto& childCond : n.children) {
        childCond->accept(this);
        if (result) return;
    }
    result = false;
}

void CondCheckerVisitor::visitAndConditionQNode(AndConditionQNode& n) {
    for (const auto& childCond : n.children) {
        childCond->accept(this);
        if (!result) return;
    }
    result = true;
}

void CondCheckerVisitor::visitCompareConditionQNode(CompareConditionQNode& n) {
    auto vis = make_unique<ComputerVisitor>(schema, record);
    n.left->accept(vis.get());
    Value v1 = vis->getResult();
    n.right->accept(vis.get());
    Value v2 = vis->getResult();

    int cmpResult = compareValue(v1, v2);
    if (cmpResult < 0)
        result = n.less;
    else if (cmpResult > 0)
        result = n.greater;
    else
        result = n.equal;
}

FuncProjectorDS::FuncProjectorDS(const IntermediateType& type,
    DataSequence* source,
    vector<unique_ptr<QScalarNode>> funcs)
    : source(source)
    , funcs()
    , recordData(make_unique<ValueArray>(type.entries.size()))
    , DataSequence(type) {
    record.record = recordData.get();
    for (auto& f : funcs)
        this->funcs.push_back(move(f));
    visitor = make_unique<ComputerVisitor>
        (type, record.record);
}

void FuncProjectorDS::reset() {
    source->reset();
    if (!source->hasEnded())
        update();
}
void FuncProjectorDS::advance() {
    source->advance();
    if (!source->hasEnded())
        update();
}
bool FuncProjectorDS::hasEnded() const {
    return source->hasEnded();
}
void FuncProjectorDS::update() {
    for (int i = 0; i < source->get().record->size(); i++)
        (*recordData)[i] = (*source->get().record)[i];
    for (int i = 0; i < funcs.size(); i++) {
        int index = source->get().record->size() + i;
        funcs[i]->accept(visitor.get());
        (*recordData)[index] = visitor->getResult();
    }
}


FilterDS::FilterDS(const IntermediateType& type,
    DataSequence* source,
    QCondPtr cond) 
    : source(source)
    , cond(move(cond))
    , DataSequence(type) {
    record.type = this->source->getType();
    visitor = make_unique<CondCheckerVisitor>
        (this->source->getType(), record.record);
}
void FilterDS::reset() {
    source->reset();
    if (!source->hasEnded())
        update();
}
void FilterDS::advance() {
    source->advance();
    if (!source->hasEnded())
        update();
}
bool FilterDS::hasEnded() const {
    return source->hasEnded();
}
void FilterDS::update() {
    record.record = source->get().record;
    visitor->updateRecord(record.record);
    while (!source->hasEnded()) {
        cond->accept(visitor.get());
        if (visitor->getResult()) return;
        source->advance();
        record.record = source->get().record;
        visitor->updateRecord(record.record);
    }
}

UnionDS::UnionDS(const IntermediateType& type,
    vector<DataSequence*> sources) 
    : sources(sources)
    , DataSequence(type) {
    record.type = this->sources[0]->getType();
}
void UnionDS::reset() {
    for (auto source : sources)
        source->reset();
    currentIndex = 0;
    update();
}
void UnionDS::advance() {
    sources[currentIndex]->advance();
    update();
}
bool UnionDS::hasEnded() const {
    return currentIndex == sources.size();
}
void UnionDS::update() {
    while (currentIndex < sources.size()) {
        if (!sources[currentIndex]->hasEnded()) {
            record.record = sources[currentIndex]->get().record;
            return;
        }
        currentIndex++;
    }
}

CrossJoinDS::CrossJoinDS(const IntermediateType& type, DataSequence* left, DataSequence* right)
    : left(left)
    , right(right)
    , recordData(make_unique<ValueArray>(type.entries.size()))
    , offset(left->getType().entries.size())
    , DataSequence(type) {
    record.record = recordData.get();
}

void CrossJoinDS::reset() {
    left->reset();
    right->reset();
    update(true);
}
void CrossJoinDS::advance() {
    if (left->hasEnded()) return;
    if (right->hasEnded()) return;

    right->advance();
    if (right->hasEnded()) {
        right->reset();
        left->advance();
        if (left->hasEnded()) return;
        update(true);
    }
    else
        update(false);
}
bool CrossJoinDS::hasEnded() const {
    return left->hasEnded();
}
void CrossJoinDS::update(bool newLeft) {
    if (newLeft) {
        const ValueArray& leftData = *left->get().record;
        for (int i = 0; i < offset; i++)
            (*recordData)[i] = leftData[i];
    }
    const ValueArray& rightData = *right->get().record;
    for (int i = 0; i < rightData.size(); i++)
        (*recordData)[offset + i] = rightData[i];
}

CondJoinDS::CondJoinDS(const IntermediateType& type, DataSequence* left,
    DataSequence* right, JoinType joinType, unique_ptr<QConditionNode> cond)
    : left(left)
    , right(right)
    , joinType(joinType)
    , recordData(make_unique<ValueArray>(type.entries.size()))
    , offset(left->getType().entries.size())
    , cond(move(cond))
    , noPair(true)
    , hasJustAddedNull(false)
    , rightIndex(0)
    , rightNulls()
    , isLeftJoin(joinType == JoinType::Left || joinType == JoinType::Full)
    , isRightJoin(joinType == JoinType::Right || joinType == JoinType::Full)
    , isFillingRight(isRightJoin)
    , rightNullWalk(false)
    , DataSequence(type) {
    record.record = recordData.get();
    visitor = make_unique<CondCheckerVisitor>(type, record.record);
}

void CondJoinDS::reset() {
    left->reset();
    right->reset();
    update(true);
    if (!hasEnded()) {
        if (isFillingRight) {
            rightNulls.push_back(true);
        }
        skipToNext();
    }
}
void CondJoinDS::crossStep() {
    if (hasJustAddedNull) {
        hasJustAddedNull = false;
        left->advance();
        if (left->hasEnded()) {
            if (isRightJoin) {
                rightNullWalk = true;
                updateRightNull();
                rightIndex = 0;
                rightNullStep();
            }
            return;
        }
        update(true);
        return;
    }

    right->advance();
    if (right->hasEnded()) {
        right->reset();
        if (noPair && isLeftJoin) {
            rightIndex = 0;
            updateLeftNull();
            return;
        }
        left->advance();
        if (left->hasEnded()) {
            if (isRightJoin) {
                rightNullWalk = true;
                updateRightNull();
                rightIndex = 0;
                rightNullStep();
            }
            return;
        }
        update(true);
        isFillingRight = false;
        rightIndex = 0;
    }
    else {
        update(false);
        rightIndex++;
        if (isFillingRight)
            rightNulls.push_back(true);
    }
}
void CondJoinDS::skipToNext() {
    while (true) {
        if (left->hasEnded() && (!isLeftJoin || !noPair)) return;
        if (right->hasEnded()) return;
        cond->accept(visitor.get());
        if (visitor->getResult()) break;

        crossStep();
        if (hasJustAddedNull) return;
    }
    noPair = false;
    if (isRightJoin) {
        rightNulls[rightIndex] = false;
    }
}
void CondJoinDS::advance() {
    if (left->hasEnded() && (!isLeftJoin || !noPair)) {
        if (isRightJoin && left->hasEnded()) {
            right->advance();

            rightNullStep();
        }
        return;
    }
    if (right->hasEnded()) return;
    crossStep();
    skipToNext();
}
bool CondJoinDS::hasEnded() const {
    return left->hasEnded() && !rightNullWalk;
}
void CondJoinDS::update(bool newLeft) {
    if (newLeft) {
        const ValueArray& leftData = *left->get().record;
        for (int i = 0; i < offset; i++)
            (*recordData)[i] = leftData[i];
        noPair = true;
    }
    const ValueArray& rightData = *right->get().record;
    for (int i = 0; i < rightData.size(); i++)
        (*recordData)[offset + i] = rightData[i];
}
void CondJoinDS::updateLeftNull() {
    for (int i = offset; i < recordData->size(); i++)
        (*recordData)[i] = Value(ValueType::Null);
    noPair = false;
    hasJustAddedNull = true;
}
void CondJoinDS::updateRightNull() {
    for (int i = 0; i < offset; i++)
        (*recordData)[i] = Value(ValueType::Null);
}
void CondJoinDS::rightNullStep() {
    while (!right->hasEnded() && !rightNulls[rightIndex]) {
        right->advance();
        rightIndex++;
    }
    if (right->hasEnded())
        rightNullWalk = false;
    else
        update(false);
}


SorterDS::SorterDS(const IntermediateType& type,
    DataSequence* source,
    vector<pair<int, bool>> cmpPlan) 
    : source(source)
    , cmpPlan(cmpPlan)
    , index(0)
    , values()
    , DataSequence(type) {
    record.type = this->source->getType();
}
void SorterDS::reset() {
    source->reset();
    while (!source->hasEnded()) {
        values.push_back(*source->get().record);
        source->advance();
    }
    sort(values.begin(), values.end(), [&](const ValueArray& a, const ValueArray& b) {
        for (const auto& p : cmpPlan) {
            const auto& entry = record.type.entries[p.first];
            int r = entry.compare(a[p.first], b[p.first]);
            if (r == 0) continue;
            return (r < 0) != p.second;
        }
        return false;
    });
    index = 0;
    if (values.size() != 0)
        record.record = &values[0];
}
void SorterDS::advance() {
    if (index == values.size()) return;
    index++;
    if (index == values.size()) return;
    record.record = &values[index];
}
bool SorterDS::hasEnded() const {
    return index == values.size();
}


ConstTableDS::ConstTableDS(const IntermediateType& type,
    vector<ValueArray> values)
    : DataSequence(type)
    , values(values)
    , index(0) {
    update();
}
void ConstTableDS::update() {
    record.record = &values[index];
}
void ConstTableDS::reset() {
    index = 0;
    update();
}
void ConstTableDS::advance() {
    index++;
    if (!hasEnded())
        update();
}
bool ConstTableDS::hasEnded() const {
    return index == values.size();
}

vector<ValueArray> selectData(DataSequence* source) {
    const IntermediateType& t = source->getType();
    source->reset();
    vector<ValueArray> result;
    while (!source->hasEnded()) {
        result.push_back(*source->get().record);
        source->advance();
    }
    return result;
}

Inserter::Inserter(const SystemInfoManager& sysMan, uint16_t tableId) 
    : dataFile(sysMan, tableId)
    , schema(sysMan.getTableSchema(tableId))
{
    const vector<uint16_t>& indexes = sysMan.getTableInfo(tableId).indexes;
    for (uint16_t indexId : indexes) {
        const Schema& keySchema = sysMan.getIndexSchema(tableId, indexId);
        indexFiles.emplace_back(tableId, indexId, keySchema);
    }
}

bool Inserter::insert(RecordPtr record) {
    vector<char> encoded = schema.encode(*record.record);
    RecordId recordId = dataFile.addRecord(encoded.data());
    ValueArray decoded = *record.record;
    for (IndexFile& index : indexFiles) {
        ValueArray keyValues = index.getKeySchema().narrow(decoded);
        auto keys = index.getKeySchema().encode(keyValues);
        bool result = index.addKey(keys, recordId);
        if (!result) {
            dataFile.deleteRecord(recordId);
            // TODO: delete from indexes too
            return false;
        }
    }
    return true;
}

bool Inserter::insert(DataSequence* source) {
    source->reset();
    while (!source->hasEnded()) {
        bool result = insert(source->get());
        if (!result) return false;
        source->advance();
    }
    return true;
}