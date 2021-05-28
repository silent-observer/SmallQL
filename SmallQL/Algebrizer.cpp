#include "SqlAst.h"

QTablePtr InsertStmtNode::algebrize(const SystemInfoManager& sysMan) {
    auto result = make_unique<InserterNode>();
    auto table = tableName->algebrize(sysMan);
    result->tableId = convert<ReadTableQNode>(table)->tableId;
    result->source = 
        insertData->algebrizeWithContext(sysMan, table->type);
    result->type = IntermediateType();
    return result;
}

static inline QTablePtr algebrizeSelectWhere(const SelectNode& n, QTablePtr source, const SystemInfoManager& sysMan) {
    if (n.whereCond != NULL) {
        auto filter = make_unique<FilterQNode>();
        filter->type = source->type;
        filter->cond = n.whereCond->algebrizeWithContext(sysMan, source->type, source->type);
        filter->source = move(source);
        source = move(filter);
    }
    return source;
}

static inline pair<IntermediateType, vector<bool>> algebrizeSelectUpdateSourceType(
    const SelectNode& n, const QTableNode* source, const SystemInfoManager& sysMan) {
    auto sourceType = source->type;
    vector<bool> groupPlan(source->type.entries.size(), false);
    if (n.groupBy.size() > 0) {
        sourceType.entries.clear();
        sourceType.isAmbiguous.clear();
        for (const auto& c : n.groupBy) {
            auto colExpr = c->algebrizeWithContext(sysMan, source->type, source->type);
            auto col = convert<ColumnQNode>(colExpr);
            if (!col) 
                throw SemanticException("Cannot group by anything but raw columns!");
            groupPlan[col->columnId] = true;
        }
        for (int id = 0; id < groupPlan.size(); id++) {
            if (groupPlan[id])
                sourceType.addEntry(source->type.entries[id]);
        }
    }
    return make_pair(sourceType, groupPlan);
}

using ColumnsVector = vector<pair<QScalarPtr, string>>;

static inline ColumnsVector algebrizeSelectColumns(
        const SelectNode& n, const IntermediateType& sourceType, 
        const IntermediateType& preaggrType, const SystemInfoManager& sysMan) {
    ColumnsVector algebrized;
    for (const auto& p : n.columns) {
        string alias = p.second;
        auto colExpr = p.first->algebrizeWithContext(sysMan, sourceType, preaggrType);
        algebrized.push_back(make_pair(move(colExpr), alias));
    }
    return algebrized;
}

static inline tuple<int, int, int> algebrizeSelectCalcCounts(
        const QTableNode* source, 
        const ColumnsVector& algebrized, int groupBySize) {
    int columnCount = source->type.entries.size();
    int funcCount = 0;
    int groupFuncCount = 0;
    for (auto& p : algebrized) {
        if (is<AsteriskQNode>(p.first)) continue;
        if (p.first->type.isAggregate)
            groupFuncCount++;
        else if (is<FuncQNode>(p.first))
            funcCount++;
    }
    if (groupFuncCount != 0 || groupBySize > 0) {
        columnCount = groupBySize;
    }
    return make_tuple(columnCount, funcCount, groupFuncCount);
}

static inline unique_ptr<ProjectionQNode> algebrizeSelectProjection(
        const ColumnsVector& algebrized,
        const QTableNode* source, int& funcIndex, int& groupFuncIndex) {
    auto projection = make_unique<ProjectionQNode>();
    projection->type = IntermediateType();
    for (auto& p : algebrized) {
        if (auto col = convert<AsteriskQNode>(p.first)) {
            for (int i = 0; i < source->type.entries.size(); i++) {
                if (col->tableName != "") {
                    string tableName = source->type.entries[i].tableName;
                    if (source->type.tableAliases.count(col->tableName)) {
                        if (tableName != source->type.tableAliases.at(col->tableName) && col->tableName != tableName)
                            continue;
                    }
                    else if (col->tableName != tableName)
                        continue;
                }
                projection->columns.push_back(i);
                projection->type.addEntry(source->type.entries[i]);
            }
        }
        else if (auto col = convert<ColumnQNode>(p.first)) {
            projection->columns.push_back(col->columnId);
            IntermediateTypeEntry scalar = col->type;
            scalar.columnName = p.second;
            projection->type.addEntry(scalar);
        }
        else {
            IntermediateTypeEntry scalar = p.first->type;
            scalar.columnName = p.second;
            projection->type.addEntry(scalar);
            if (p.first->type.isAggregate) {
                projection->columns.push_back(groupFuncIndex);
                groupFuncIndex++;
            }
            else {
                projection->columns.push_back(funcIndex);
                funcIndex++;
            }
        }
    }
    return projection;
}

static inline ColumnsVector algebrizeSelectAggrFuncs(ColumnsVector& algebrized) {
    ColumnsVector aggrFuncs;
    for (auto& p : algebrized) {
        if (is<AsteriskQNode>(p.first)) continue;
        if (p.first->type.isAggregate)
            aggrFuncs.push_back(move(p));
    }
    return aggrFuncs;
}

static inline QTablePtr algebrizeSelectGroupBy(const SelectNode& n, QTablePtr source,
        ColumnsVector& aggrFuncs, const IntermediateType& sourceType,
        const vector<bool>& groupPlan, int columnCount) {

    if (n.groupBy.size() > 0 || aggrFuncs.size() > 0) {
        auto sorter = make_unique<SorterQNode>();
        auto groupifier = make_unique<GroupifierQNode>();
        auto degroupifier = make_unique<DegroupifierQNode>();
        sorter->type = source->type;
        groupifier->type = source->type;
        degroupifier->type = sourceType;
        for (int id = 0; id < groupPlan.size(); id++) {
            if (groupPlan[id])
                sorter->cmpPlan.push_back(make_pair(id, false));
        }
        groupifier->groupPlan = groupPlan;
        if (sorter->cmpPlan.size() != 0) {
            sorter->source = move(source);
            source = move(sorter);
        }
        groupifier->source = move(source);
        QTablePtr groupedSource = move(groupifier);

        if (aggrFuncs.size() > 0) {
            auto funcProjection = make_unique<AggrFuncProjectionQNode>();
            funcProjection->type = groupedSource->type;
            int groupFuncId = columnCount;
            for (auto& p : aggrFuncs) {
                IntermediateTypeEntry scalar = p.first->type;
                scalar.columnName = p.second;
                funcProjection->type.addEntry(scalar);
                funcProjection->funcs.push_back(move(p.first));
                degroupifier->type.addEntry(scalar);
                groupFuncId++;
            }
            funcProjection->source = move(groupedSource);
            groupedSource = move(funcProjection);
        }

        degroupifier->source = move(groupedSource);
        source = move(degroupifier);
    }
    return source;
}

static inline unique_ptr<FuncProjectionQNode> algebrizeSelectFuncProj(
        const QTableNode* source, ColumnsVector& algebrized) {
    auto funcProjection = make_unique<FuncProjectionQNode>();
    funcProjection->type = source->type;
    for (auto& p : algebrized) {
        if (is<FuncQNode>(p.first)) {
            IntermediateTypeEntry scalar = p.first->type;
            scalar.columnName = p.second;
            funcProjection->type.addEntry(scalar);
            funcProjection->funcs.push_back(move(p.first));
        }
    }
    return funcProjection;
}

static inline unique_ptr<SorterQNode> algebrizeSelectOrderBy(
        const SelectNode& n, FuncProjectionQNode* funcProjection,
        const SystemInfoManager& sysMan, const IntermediateType& preaggrType, int& endIndex) {
    if (n.orderBy.size() > 0) {
        auto sorter = make_unique<SorterQNode>();
        sorter->type = funcProjection->type;
        for (const auto& p : n.orderBy) {
            auto colExpr = p.first->algebrizeWithContext(sysMan, funcProjection->type, preaggrType);
            if (auto col = convert<ColumnQNode>(colExpr)) {
                sorter->cmpPlan.push_back(make_pair(col->columnId, p.second));
            }
            else {
                IntermediateTypeEntry scalar = colExpr->type;
                funcProjection->type.addEntry(scalar);
                funcProjection->funcs.push_back(move(colExpr));
                sorter->cmpPlan.push_back(make_pair(endIndex, p.second));
                sorter->type.addEntry(scalar);
                endIndex++;
            }
        }
        return sorter;
    }
    else
        return nullptr;
}

QTablePtr SelectNode::algebrize(const SystemInfoManager& sysMan) {
    auto source = from->algebrize(sysMan);
    source = algebrizeSelectWhere(*this, move(source), sysMan);

    auto preaggrType = source->type;
    auto r1 = algebrizeSelectUpdateSourceType(*this, source.get(), sysMan);
    const auto& sourceType = r1.first;
    const auto& groupPlan = r1.second;

    auto algebrized = algebrizeSelectColumns(*this, sourceType, preaggrType, sysMan);

    auto r2 = algebrizeSelectCalcCounts(source.get(), algebrized, groupBy.size());
    int columnCount = get<0>(r2);
    int funcCount = get<1>(r2);
    int groupFuncCount = get<2>(r2);
    int groupFuncIndex = columnCount;
    int funcIndex = groupFuncIndex + groupFuncCount;
    int endIndex = funcIndex + funcCount;

    auto projection = algebrizeSelectProjection(algebrized, source.get(), funcIndex, groupFuncIndex);
    auto aggrFuncs = algebrizeSelectAggrFuncs(algebrized);
    
    source = algebrizeSelectGroupBy(*this, move(source), aggrFuncs, sourceType, groupPlan, columnCount);
    
    auto funcProjection = algebrizeSelectFuncProj(source.get(), algebrized);

    unique_ptr<SorterQNode> sorter = algebrizeSelectOrderBy(*this, 
        funcProjection.get(), sysMan, preaggrType, endIndex);

    if (funcProjection->funcs.size() > 0) {
        funcProjection->source = move(source);
        source = move(funcProjection);
    }
    if (sorter) {
        sorter->source = move(source);
        source = move(sorter);
    }

    projection->source = move(source);
    return projection;
}

QTablePtr DeleteStmtNode::algebrize(const SystemInfoManager& sysMan) {
    auto result = make_unique<DeleterNode>();
    auto table = tableName->algebrize(sysMan);
    result->tableId = convert<ReadTableQNode>(table)->tableId;
    result->source = move(table);
    if (whereCond != NULL) {
        auto filter = make_unique<FilterQNode>();
        filter->type = result->source->type;
        filter->cond = whereCond->algebrizeWithContext(sysMan, result->source->type, result->source->type);
        filter->source = move(result->source);
        result->source = move(filter);
    }
    result->type = IntermediateType();
    return result;
}

QTablePtr UpdateStmtNode::algebrize(const SystemInfoManager& sysMan) {
    auto result = make_unique<UpdaterNode>();
    auto table = tableName->algebrize(sysMan);
    result->tableId = convert<ReadTableQNode>(table)->tableId;
    result->source = move(table);
    if (whereCond != NULL) {
        auto filter = make_unique<FilterQNode>();
        filter->type = result->source->type;
        filter->cond = whereCond->algebrizeWithContext(sysMan, result->source->type, result->source->type);
        filter->source = move(result->source);
        result->source = move(filter);
    }
    result->type = IntermediateType();
    result->affectsVarData = false;

    for (const auto& p : setData) {
        auto columnExpr = p.first->algebrizeWithContext(sysMan, result->source->type, result->source->type);
        auto valueExpr = p.second->algebrizeWithContext(sysMan, result->source->type, result->source->type);
        auto col = convert<ColumnQNode>(columnExpr);
        if (!col) 
            throw SemanticException("Cannot SET anything but raw columns!");
        result->setData.push_back(make_pair(col->columnId, move(valueExpr)));
        for (auto indexId : sysMan.getTableInfo(result->tableId).indexes) {
            for (const auto& indexCol : sysMan.getIndexInfo(result->tableId, indexId).schema.columns) {
                if (indexCol.id == col->columnId) {
                    result->affectedIndexes.insert(indexId);
                    break;
                }
            }
        }
        if (is<VariableLengthType>(col->type.type))
            result->affectsVarData = true;
    }

    return result;
}

QTablePtr SelectStmtNode::algebrize(const SystemInfoManager& sysMan) {
    auto result = make_unique<SelectorNode>();
    result->source = select.algebrize(sysMan);
    result->type = result->source->type;
    return result;
}

QTablePtr TableSubquery::algebrize(const SystemInfoManager& sysMan) {
    auto result = query->algebrize(sysMan);
    if (alias != "") {
        if (!result->type.isAmbiguous.empty())
            throw SemanticException("Cannot have ambiguous columns in a subquery (" + alias + ")");
        for (auto& entry : result->type.entries) {
            entry.tableName = alias;
        }
    }
    for (auto& entry : result->type.entries) {
        entry.isAggregate = false;
        entry.isConst = false;
     }
    return result;
}

QTablePtr TableName::algebrize(const SystemInfoManager& sysMan) {
    auto result = make_unique<ReadTableQNode>();
    if (!sysMan.tableExists(name))
        throw SemanticException("Table " + name + " doesn't exist!");
    result->tableId = sysMan._getTableId(name);
    result->tableSchema = sysMan._getTableSchema(name);
    result->type = IntermediateType(result->tableSchema, name);
    if (alias != "") {
        result->type.tableAliases.emplace(alias, name);
    }
    return result;
}

QTablePtr JoinNode::algebrize(const SystemInfoManager& sysMan) {
    auto result = make_unique<JoinQNode>();
    result->joinType = joinType;
    result->left = left->algebrize(sysMan);
    result->right = right->algebrize(sysMan);
    result->type = result->left->type;
    for (auto& entry : result->right->type.entries)
        result->type.addEntry(entry);
    for (auto& p : result->right->type.tableAliases)
        result->type.tableAliases.insert(p);
    if (on)
        result->on = on->algebrizeWithContext(sysMan, result->type, result->type);
    else
        result->on = nullptr;
    return result;
}

QScalarPtr ColumnNameExpr::algebrizeWithContext(
        const SystemInfoManager& sysMan, const IntermediateType& type,
        const IntermediateType& preaggrType) const {
    if (name == "*") {
        auto result = make_unique<AsteriskQNode>();
        result->tableName = tableName;
        return result;
    }

    auto result = make_unique<ColumnQNode>();
    result->name = name;
    if (tableName == "") {
        if (type.isAmbiguous.count(name) != 0)
            throw SemanticException("Column " + name + " is ambiguous, specify original table!");

        for (int i = 0; i < type.entries.size(); i++) {
            if (type.entries[i].columnName == name) {
                result->columnId = i;
                result->type = type.entries[i];
                return result;
            }
        }
        throw SemanticException("Column " + name + " doesn't exist!");
    }
    else {
        string actualName = tableName;
        for (int i = 0; i < type.entries.size(); i++) {
            if (type.entries[i].columnName == name && type.entries[i].tableName == actualName) {
                result->columnId = i;
                result->type = type.entries[i];
                return result;
            }
        }
        if (type.tableAliases.count(actualName) != 0)
            actualName = type.tableAliases.at(actualName);
        for (int i = 0; i < type.entries.size(); i++) {
            if (type.entries[i].columnName == name && type.entries[i].tableName == actualName) {
                result->columnId = i;
                result->type = type.entries[i];
                return result;
            }
        }
        throw SemanticException("Column " + actualName + "." + name + " doesn't exist!");
    }
    
}

QScalarPtr ConstExpr::algebrizeWithContext(
        const SystemInfoManager& sysMan, const IntermediateType& type,
        const IntermediateType& preaggrType) const {
    auto result = make_unique<ConstScalarQNode>();
    result->data = v;
    result->type = IntermediateTypeEntry(v.toString(), "", v.defaultType(), v.type == ValueType::Null, false, true);
    return result;
}

QScalarPtr FuncExpr::algebrizeWithContext(
        const SystemInfoManager& sysMan, const IntermediateType& type,
        const IntermediateType& preaggrType) const {
    auto result = make_unique<FuncQNode>();
    result->name = name;
    vector<shared_ptr<DataType>> inputs;
    bool hasAggregate = false;
    bool hasConst = false;
    bool hasNormal = false;
    for (auto& child : children) {
        auto newChild = child->algebrizeWithContext(sysMan, type, preaggrType);
        if (is<AsteriskQNode>(newChild)) {
            throw TypeException("Cannot use * outside of SELECT query");
        }
        if (newChild->type.isAggregate)
            hasAggregate = true;
        else if (newChild->type.isConst)
            hasConst = true;
        else
            hasNormal = true;
        inputs.push_back(newChild->type.type);
        result->children.push_back(move(newChild));
        
    }
    if (hasNormal && hasAggregate)
        throw TypeException("Cannot combine aggregate and non-aggregate values");
    bool isConst = hasConst && !hasAggregate && !hasNormal;

    auto dataType = typeCheckFunc(name, inputs);
    string text = AstNode::prettyPrint();
    result->type = IntermediateTypeEntry(text, "", dataType, true, hasAggregate, isConst);
    return result;
}

QScalarPtr AggrFuncExpr::algebrizeWithContext(
        const SystemInfoManager& sysMan, const IntermediateType& type,
        const IntermediateType& preaggrType) const {
    auto result = make_unique<AggrFuncQNode>();
    result->name = name;
    result->child = child->algebrizeWithContext(sysMan, preaggrType, preaggrType);
    if (name != "COUNT") {
        if (is<AsteriskQNode>(result->child)) {
            throw TypeException("Cannot use * outside of SELECT query");
        }
        if (result->child->type.isAggregate)
            throw TypeException("Cannot have nested aggregate functions");
    }
    auto dataType = typeCheckAggrFunc(name, result->child->type.type);
    string text = AstNode::prettyPrint();
    result->type = IntermediateTypeEntry(text, "", dataType, true, true, false);
    return result;
}

QTablePtr InsertValuesNode::algebrizeWithContext(
        const SystemInfoManager& sysMan, const IntermediateType& type) const {
    auto result = make_unique<ExprDataNode>();
    result->type = type;
    for (auto& r : data) {
        result->data.emplace_back();
        for (auto& e : r) {
            auto v = e->algebrizeWithContext(sysMan, type, type);
            if (!v->type.isConst)
                throw TypeException("Cannot have non-constant values in VALUES clause");
            result->data.back().push_back(move(v));
        }
    }
    return result;
}

QCondPtr AndConditionNode::algebrizeWithContext(
        const SystemInfoManager& sysMan, 
        const IntermediateType& type,
        const IntermediateType& preaggrType) const {
    auto result = make_unique<AndConditionQNode>();
    for (const auto& c : children) {
        result->children.push_back(
            c->algebrizeWithContext(sysMan, type, preaggrType));
    }
    return result;
}

QCondPtr OrConditionNode::algebrizeWithContext(
        const SystemInfoManager& sysMan, 
        const IntermediateType& type,
        const IntermediateType& preaggrType) const {
    auto result = make_unique<OrConditionQNode>();
    for (const auto& c : children) {
        result->children.push_back(
            c->algebrizeWithContext(sysMan, type, preaggrType));
    }
    return result;
}

QCondPtr CompareConditionNode::algebrizeWithContext(
        const SystemInfoManager& sysMan, 
        const IntermediateType& type,
        const IntermediateType& preaggrType) const {
    auto result = make_unique<CompareConditionQNode>();
    result->left = left->algebrizeWithContext(sysMan, type, preaggrType);
    result->right = right->algebrizeWithContext(sysMan, type, preaggrType);
    if (is<AsteriskQNode>(result->left) || is<AsteriskQNode>(result->right)) {
        throw TypeException("Cannot use * outside of SELECT query");
    }
    if (!typeCheckComparable(result->left->type.type, result->right->type.type))
        throw TypeException("Cannot compare " + 
            result->left->type.type->toString() + " and " + 
            result->right->type.type->toString());

    switch (condType)
    {
    case CompareType::Equal:
        result->less = false;
        result->equal = true;
        result->greater = false;
        break;
    case CompareType::NotEqual:
        result->less = true;
        result->equal = false;
        result->greater = true;
        break;
    case CompareType::Greater:
        result->less = false;
        result->equal = false;
        result->greater = true;
        break;
    case CompareType::GreaterOrEqual:
        result->less = false;
        result->equal = true;
        result->greater = true;
        break;
    case CompareType::Less:
        result->less = true;
        result->equal = false;
        result->greater = false;
        break;
    case CompareType::LessOrEqual:
        result->less = true;
        result->equal = true;
        result->greater = false;
        break;
    }
    return result;
}