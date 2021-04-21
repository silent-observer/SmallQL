#include "Executor.h"
#include "QueryTree.h"

DataFile& Executor::addDataFile(uint16_t id, const Schema& schema) {
    if (dataFiles.count(id) == 0)
        dataFiles.emplace(id, make_unique<DataFile>(id, schema.size));
    return *dataFiles[id];
}
IndexFile& Executor::addIndexFile(uint16_t tableId, uint16_t indexId, const Schema& keySchema) {
    pair<uint16_t, uint16_t> p = make_pair(tableId, indexId);
    if (indexFiles.count(p) == 0)
        indexFiles.emplace(p, make_unique<IndexFile>(tableId, indexId, keySchema));
    return *indexFiles[p];
}

class PreparerVisitor : public QTableNode::Visitor {
private:
    Executor* exec;
    uint32_t lastResult;
public:
    PreparerVisitor(Executor* exec) : exec(exec) {}
    virtual void visitReadTableQNode(ReadTableQNode& n);
    virtual void visitReadTableIndexScanQNode(ReadTableIndexScanQNode& n);
    virtual void visitProjectionQNode(ProjectionQNode& n);
    virtual void visitFuncProjectionQNode(FuncProjectionQNode& n);
    virtual void visitFilterQNode(FilterQNode& n);
    virtual void visitUnionQNode(UnionQNode& n);
    virtual void visitJoinQNode(JoinQNode& n);
    virtual void visitSelectorNode(SelectorNode& n);
    virtual void visitInserterNode(InserterNode& n);
    virtual void visitConstDataNode(ConstDataNode& n);
};

void Executor::prepare(QTablePtr tree) {
    auto v = make_unique<PreparerVisitor>(this);
    tree->accept(v.get());
}

void PreparerVisitor::visitReadTableQNode(ReadTableQNode& n) {
    DataFile& data = exec->addDataFile(n.tableId, n.tableSchema);
    auto seq = make_unique<TableFullScanDS>(n.tableSchema, data);
    exec->sequences.push_back(move(seq));
    lastResult = exec->sequences.size() - 1;
}

void PreparerVisitor::visitReadTableIndexScanQNode(ReadTableIndexScanQNode& n) {
    DataFile& data = exec->addDataFile(n.tableId, n.tableSchema);
    IndexFile& index = exec->addIndexFile(n.tableId, n.indexId, n.keySchema);
    auto seq = make_unique<TableIndexScanDS>(
        n.tableSchema, data, index,
        n.from, n.to, n.incFrom, n.incTo);
    exec->sequences.push_back(move(seq));
    lastResult = exec->sequences.size() - 1;
}

void PreparerVisitor::visitProjectionQNode(ProjectionQNode& n) {
    n.source->accept(this);
    uint32_t sourceId = lastResult;
    auto seq = make_unique<ProjectorDS>(
        n.type, exec->sequences[sourceId].get(), n.columns);
    exec->sequences.push_back(move(seq));
    lastResult = exec->sequences.size() - 1;
}

void PreparerVisitor::visitFuncProjectionQNode(FuncProjectionQNode& n) {
    n.source->accept(this);
    uint32_t sourceId = lastResult;
    auto seq = make_unique<FuncProjectorDS>(
        n.type, exec->sequences[sourceId].get(), move(n.funcs));
    exec->sequences.push_back(move(seq));
    lastResult = exec->sequences.size() - 1;
}

void PreparerVisitor::visitFilterQNode(FilterQNode& n) {
    n.source->accept(this);
    uint32_t sourceId = lastResult;
    auto seq = make_unique<FilterDS>(
        n.type, exec->sequences[sourceId].get(), move(n.cond));
    exec->sequences.push_back(move(seq));
    lastResult = exec->sequences.size() - 1;
}

void PreparerVisitor::visitUnionQNode(UnionQNode& n) {
    vector<DataSequence*> newSources;
    for (auto& source : n.sources) {
        source->accept(this);
        newSources.push_back(exec->sequences[lastResult].get());
    }
    auto seq = make_unique<UnionDS>(n.type, newSources);
    exec->sequences.push_back(move(seq));
    lastResult = exec->sequences.size() - 1;
}

void PreparerVisitor::visitJoinQNode(JoinQNode& n) {
    n.left->accept(this);
    DataSequence* newLeft = exec->sequences[lastResult].get();
    n.right->accept(this);
    DataSequence* newRight = exec->sequences[lastResult].get();
    auto seq = make_unique<CrossJoinDS>(n.type, newLeft, newRight);
    exec->sequences.push_back(move(seq));
    lastResult = exec->sequences.size() - 1;
}

void PreparerVisitor::visitSelectorNode(SelectorNode& n) {
    exec->queryType = QueryType::Select;
    n.source->accept(this);
}

void PreparerVisitor::visitInserterNode(InserterNode& n) {
    exec->queryType = QueryType::Insert;
    exec->tableId = n.tableId;
    n.source->accept(this);
}

void PreparerVisitor::visitConstDataNode(ConstDataNode& n) {
    auto seq = make_unique<ConstTableDS>(n.type, n.data);
    exec->sequences.push_back(move(seq));
    lastResult = exec->sequences.size() - 1;
}

vector<ValueArray> Executor::execute() {
    switch (queryType)
    {
    case QueryType::Select:
        return selectData(sequences.back().get());
    case QueryType::Insert: {
        Inserter inserter(sysMan, tableId);
        inserter.insert(sequences.back().get());
        return vector<ValueArray>();
    }
    default:
        return vector<ValueArray>();
    }
}