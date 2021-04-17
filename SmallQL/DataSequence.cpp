#include "DataSequence.h"
#include "QueryTree.h"

TableFullScanDS::TableFullScanDS(const Schema& type, DataFile& data)
    : data(data)
    , iter(makeConst(data).begin())
    , DataSequence(type) {
    reset();
}
void TableFullScanDS::reset() {
    iter = makeConst(data).begin();
    if (iter != makeConst(data).end()) {
        record.record = *iter;
    }
}
void TableFullScanDS::advance() {
    if (iter == makeConst(data).end()) return;
    iter++;
    record.record = *iter;
}
bool TableFullScanDS::hasEnded() const {
    return iter == makeConst(data).end();
}


TableIndexScanDS::TableIndexScanDS(const Schema& type, DataFile& data, IndexFile& index,
        ValueArray from, ValueArray to, bool incFrom, bool incTo)
    : data(data)
    , index(index)
    , iter(index.end())
    , from(from)
    , to(to)
    , incFrom(incFrom)
    , incTo(incTo)
    , DataSequence(type) {
    reset();
}
void TableIndexScanDS::reset() {
    iter = index.getIterator(from);
    if (iter == index.end()) return;

    record.record = data.readRecord(*iter);
    if (!incFrom) {
        while (index.getKeySchema().compare(from, record.record) == 0) {
            iter++;
            record.record = data.readRecord(*iter);
            if (iter == index.end()) return;
        }
    }
}
void TableIndexScanDS::advance() {
    if (iter == index.end()) return;
    iter++;
    record.record = data.readRecord(*iter);
    int cmpVal = index.getKeySchema().compare(to, record.record);
    if (cmpVal < 0 || !incTo && cmpVal == 0)
        iter = index.end();
}
bool TableIndexScanDS::hasEnded() const {
    return iter == index.end();
}


ProjectorDS::ProjectorDS(const Schema& type,
    DataSequence* source,
    vector<uint16_t> columns) 
    : source(move(source))
    , buffer(type.getSize())
    , DataSequence(type) {
    const Schema& inputSchema = this->source->getType();
    vector<int> inputNullOffsets = inputSchema.getNullBitOffsets();
    vector<int> outputNullOffsets = type.getNullBitOffsets();
    for (int i = 0; i < columns.size(); i++) {
        int to = type.columns[i].offset;
        int from = inputSchema.columns[columns[i]].offset;
        int size = type.columns[i].type->getSize();
        copyInfo.push_back({from, -1, to, -1, size});
    }
    for (int i = 0; i < outputNullOffsets.size(); i++) {
        copyInfo[outputNullOffsets[i]].nullTo = i;
    }
    for (int i = 0; i < inputNullOffsets.size(); i++) {
        copyInfo[columns[inputNullOffsets[i]]].nullFrom = i;
    }
    record.record = buffer.data();
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
    const char* input = source->get().record;
    char* output = buffer.data();
    memset(output, 0, record.type.totalNullBytes);
    for (const auto& c : copyInfo) {
        memcpy(output + c.to, input + c.from, c.size);
        if (c.nullTo != -1 && c.nullFrom != -1) {
            int bit = (input[c.nullFrom / 8] >> (c.nullFrom % 8)) & 1;
            output[c.nullTo / 8] |= bit << (c.nullTo % 8);
        }
    }
}

class ComputerVisitor : public QScalarNode::Visitor {
private:
    const Schema& schema;
    const char* record;
    Value result;
public:
    ComputerVisitor(const Schema& schema, const char* record)
        : schema(schema), record(record) {}
    inline const Value& getResult() const {
        return result;
    }
    virtual void visitColumnQNode(ColumnQNode& n);
    virtual void visitConstScalarQNode(ConstScalarQNode& n);
};
void ComputerVisitor::visitConstScalarQNode(ConstScalarQNode& n) {
    result = n.data;
}
void ComputerVisitor::visitColumnQNode(ColumnQNode& n) {
    const SchemaEntry& col = schema.columns[n.columnId];
    if (col.canBeNull) {
        bool isNull = (record[col.nullBit / 8] >> (col.nullBit % 8)) & 1;
        if (isNull) {
            result = Value(ValueType::Null);
            return;
        }
    }
    result = col.type->decode(record + col.offset);
}

class CondCheckerVisitor : public QConditionNode::Visitor {
private:
    const Schema& schema;
    const char* record;
    bool result;
public:
    CondCheckerVisitor(const Schema& schema, const char* record)
        : schema(schema), record(record) {}
    inline bool getResult() const {
        return result;
    }
    inline void updateRecord(const char* newRecord) {
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

FilterDS::FilterDS(const Schema& type,
    DataSequence* source,
    QCondPtr cond) 
    : source(move(source))
    , cond(move(cond))
    , DataSequence(type) {
    record.type = source->getType();
    visitor = make_unique<CondCheckerVisitor>
        (source->getType(), record.record);
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

UnionDS::UnionDS(const Schema& type,
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

ConstTableDS::ConstTableDS(const Schema& type,
    vector<ValueArray> values)
    : DataSequence(type)
    , index(0) {
    for (const ValueArray& valArr : values) {
        data.push_back(type.encode(valArr));
    }
    update();
}
void ConstTableDS::update() {
    record.record = data[index].data();
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
    return index == data.size();
}

vector<ValueArray> selectData(DataSequence* source) {
    const Schema& t = source->getType();
    source->reset();
    vector<ValueArray> result;
    while (!source->hasEnded()) {
        const char* data = source->get().record;
        result.push_back(t.decode(data));
        source->advance();
    }
    return result;
}

Inserter::Inserter(const SystemInfoManager& sysMan, uint16_t tableId) 
    : dataFile(tableId)
{
    const vector<uint16_t>& indexes = sysMan.getTableInfo(tableId).indexes;
    for (uint16_t indexId : indexes) {
        const Schema& keySchema = sysMan.getIndexSchema(tableId, indexId);
        indexFiles.emplace_back(tableId, indexId, keySchema);
    }
}

bool Inserter::insert(RecordPtr record) {
    RecordId recordId = dataFile.addRecord(record.record);
    ValueArray decoded = record.type.decode(record.record);
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