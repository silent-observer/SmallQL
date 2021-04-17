#include "SqlAst.h"

QTablePtr InsertStmtNode::algebrize(const SystemInfoManager& sysMan) {
    auto result = make_unique<InserterNode>();
    auto table = tableName->algebrize(sysMan);
    result->tableId = convert<ReadTableQNode>(table)->tableId;
    result->source = 
        insertData->algebrizeWithContext(sysMan, table->type);
    result->type = Schema();
    return result;
}

QTablePtr SelectNode::algebrize(const SystemInfoManager& sysMan) {
    auto result = make_unique<SelectorNode>();
    auto source = from->algebrize(sysMan);
    if (whereCond != NULL) {
        auto filter = make_unique<FilterQNode>();
        filter->type = source->type;
        filter->cond = whereCond->algebrizeWithContext(sysMan, filter->type);
        filter->source = move(source);
        source = move(filter);
    }
    if (!isStar) {
        auto projection = make_unique<ProjectionQNode>();
        projection->type = Schema();
        for (const auto& p : columns) {
            string alias = p.second;
            auto colExpr = p.first->algebrizeWithContext(sysMan, source->type);
            if (auto col = convert<ColumnQNode>(colExpr)) {
                projection->columns.push_back(col->columnId);
                SchemaEntry scalar = col->type;
                scalar.name = alias;
                projection->type.addColumn(scalar);
            }
        }
        projection->source = move(source);
        source = move(projection);
    }
    result->source = move(source);
    result->type = result->source->type;
    return result;
}

QTablePtr SelectStmtNode::algebrize(const SystemInfoManager& sysMan) {
    return select.algebrize(sysMan);
}

QTablePtr TableName::algebrize(const SystemInfoManager& sysMan) {
    auto result = make_unique<ReadTableQNode>();
    result->tableId = sysMan._getTableId(name);
    result->type = sysMan._getTableSchema(name);
    return result;
}

QScalarPtr ColumnNameExpr::algebrizeWithContext(
        const SystemInfoManager& sysMan, const Schema& type) const {
    auto result = make_unique<ColumnQNode>();
    result->name = name;
    for (int i = 0; i < type.columns.size(); i++) {
        if (type.columns[i].name == name) {
            result->columnId = i;
            result->type = type.columns[i];
            return result;
        }
    }
    return NULL;
}

QScalarPtr ConstExpr::algebrizeWithContext(
        const SystemInfoManager& sysMan, const Schema& type) const {
    auto result = make_unique<ConstScalarQNode>();
    result->data = v;
    return result;
}

QTablePtr InsertValuesNode::algebrizeWithContext(
        const SystemInfoManager& sysMan, const Schema& type) const {
    auto result = make_unique<ConstDataNode>();
    result->type = type;
    result->data = data;
    return result;
}

QCondPtr AndConditionNode::algebrizeWithContext(
        const SystemInfoManager& sysMan, 
        const Schema& type) const {
    auto result = make_unique<AndConditionQNode>();
    for (const auto& c : children) {
        result->children.push_back(
            c->algebrizeWithContext(sysMan, type));
    }
    return result;
}

QCondPtr OrConditionNode::algebrizeWithContext(
        const SystemInfoManager& sysMan, 
        const Schema& type) const {
    auto result = make_unique<OrConditionQNode>();
    for (const auto& c : children) {
        result->children.push_back(
            c->algebrizeWithContext(sysMan, type));
    }
    return result;
}

QCondPtr CompareConditionNode::algebrizeWithContext(
        const SystemInfoManager& sysMan, 
        const Schema& type) const {
    auto result = make_unique<CompareConditionQNode>();
    result->left = left->algebrizeWithContext(sysMan, type);
    result->right = right->algebrizeWithContext(sysMan, type);
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