#include "DataSequence.h"
#include "QueryTree.h"
#include "ComputerVisitor.h"

TableFullScanDS::TableFullScanDS(const Schema& schema, DataFile& data)
    : data(data)
    , iter(makeConst(data).begin())
    , recordData(make_unique<ValueArray>())
    , schema(schema)
    , DataSequence(IntermediateType(schema)) {
    record.record = recordData.get();
    reset();
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
    reset();
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
    : source(move(source))
    , recordData(make_unique<ValueArray>(columns.size()))
    , columns(columns)
    , DataSequence(type) {
    record.record = recordData.get();
    reset();
}

void ProjectorDS::reset() {
    source->reset();
    if (!source->hasEnded())
        update();
}
void ProjectorDS::advance() {
    source->advance();
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
    auto vis = make_unique<ComputerVisitor>(schema, *record);
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
    : source(move(source))
    , funcs()
    , recordData(make_unique<ValueArray>(type.entries.size()))
    , DataSequence(type) {
    record.record = recordData.get();
    for (auto& f : funcs)
        this->funcs.push_back(move(f));
    visitor = make_unique<ComputerVisitor>
        (type, *record.record);
    update();
}

void FuncProjectorDS::reset() {
    source->reset();
    if (!source->hasEnded())
        update();
}
void FuncProjectorDS::advance() {
    source->advance();
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
    : source(move(source))
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
    : sources(move(sources))
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