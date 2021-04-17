#include "Optimizer.h"
#include "Common.h"
#include <set>

enum class OptimizationOption {
    Filterify,
    FilterifyAnd,
    Unionify,
    Indexify,
    IndexifyAnd,
};

struct OptimizationDecision {
    OptimizationOption option;
    uint16_t indexId;
};

using OptimizationPlan = map<QConditionNode*, OptimizationDecision>;

bool fitsWith(const CompareConditionQNode& cond, uint16_t colId, bool* isReversed = nullptr) {
    if (cond.less && cond.greater) return false;
    bool isLeftConst = is<ConstScalarQNode>(cond.left);
    bool isRightConst = is<ConstScalarQNode>(cond.right);
    auto leftColumn = convert<ColumnQNode>(cond.left);
    auto rightColumn = convert<ColumnQNode>(cond.right);
    if (isLeftConst && rightColumn || leftColumn && isRightConst) {
        auto col = leftColumn? leftColumn : rightColumn;
        if (col->columnId == colId) {
            if (isReversed)
                *isReversed = isLeftConst;
            return true;
        }
    }
    return false;
}

bool isCompareCondOptimizableWithIndex(
    const CompareConditionQNode& cond,
    const Schema& keySchema) {
    return fitsWith(cond, keySchema.columns[0].id);
}

bool isAndCondOptimizableWithIndex(
    const vector<QCondPtr>& children,
    const Schema& keySchema) {
    for (auto& child : children) {
        auto casted = convert<CompareConditionQNode>(child);
        if (casted && isCompareCondOptimizableWithIndex(*casted, keySchema))
            return true;
    }
    return false;
}

class WhereConditionOptimizableVisitor : public QConditionNode::Visitor {
private:
    uint16_t tableId;
    const SystemInfoManager& sysMan;
    bool result;
    OptimizationPlan plan;
    
public:
    WhereConditionOptimizableVisitor(uint16_t tableId, const SystemInfoManager& sysMan)
        : tableId(tableId), sysMan(sysMan), result(false) {}
    virtual void visitAndConditionQNode(AndConditionQNode& n) {
        for (uint16_t indexId : sysMan.getTableInfo(tableId).indexes) {
            const Schema& keySchema = sysMan.getIndexSchema(tableId, indexId);
            if (isAndCondOptimizableWithIndex(n.children, keySchema)) {
                result = true;
                auto decision = OptimizationDecision{ OptimizationOption::IndexifyAnd, indexId };
                plan.emplace(&n, decision);
                return;
            }
        }
        for (uint16_t i = 0; i < n.children.size(); i++) {
            auto casted = convert<OrConditionQNode>(n.children[i]);
            if (casted) {
                casted->accept(this);
                if (result) {
                    auto decision = OptimizationDecision{ OptimizationOption::FilterifyAnd, i };
                    plan.emplace(&n, decision);
                    return;
                }
            }
        }
        result = false;
    }
    virtual void visitOrConditionQNode(OrConditionQNode& n) {
        for (auto& child : n.children) {
            child->accept(this);
            if (!result) return;
        }
        result = true;

        auto decision = OptimizationDecision{ OptimizationOption::Unionify };
        plan.emplace(&n, decision);
    }
    virtual void visitCompareConditionQNode(CompareConditionQNode& n) {
        for (uint16_t indexId : sysMan.getTableInfo(tableId).indexes) {
            const Schema& keySchema = sysMan.getIndexSchema(tableId, indexId);
            if (isCompareCondOptimizableWithIndex(n, keySchema)) {
                result = true;

                auto decision = OptimizationDecision{ OptimizationOption::Indexify, indexId };
                plan.emplace(&n, decision);
                return;
            }
        }
        auto decision = OptimizationDecision{ OptimizationOption::Filterify };
        plan.emplace(&n, decision);
        result = false;
    };
    inline bool getResult() const {
        return result;
    }
    inline const OptimizationPlan& getPlan() const {
        return plan;
    }
};

class TableReadBuildVisitor : public QConditionNode::Visitor {
private:
    uint16_t tableId;
    const SystemInfoManager& sysMan;
    const OptimizationPlan& plan;
    QTablePtr result;

    void indexifyAnd(AndConditionQNode& n) {
        map<int, pair<int, bool>> indexifyable;
        uint16_t indexId = plan.at(&n).indexId;
        const Schema& indexSchema = sysMan.getIndexSchema(tableId, indexId);
        int currentFieldIndex = 0;
        while (currentFieldIndex < indexSchema.columns.size()) {
            bool shouldMoveIndex = false;
            for (int i = 0; i < n.children.size(); i++) {
                auto cmpNode = convert<CompareConditionQNode>(n.children[i]);
                if (!cmpNode) continue;
                bool isReversed;
                if (fitsWith(*cmpNode, indexSchema.columns[currentFieldIndex].id, &isReversed)) {
                    indexifyable.emplace(i, make_pair(currentFieldIndex, isReversed));
                    if (cmpNode->equal && !cmpNode->greater && !cmpNode->less)
                        shouldMoveIndex = true;
                }
            }
            if (!shouldMoveIndex) break;
            else currentFieldIndex++;
        }

        ValueArray from(indexSchema.columns.size(), Value(ValueType::MinVal));
        ValueArray to(indexSchema.columns.size(), Value(ValueType::MaxVal));
        bool incFrom = false, incTo = false;
        for (const auto& p : indexifyable) {
            auto cmpNode = convert<CompareConditionQNode>(n.children[p.first]);
            int fieldId = p.second.first;
            bool isReversed = p.second.second;
            auto& constNode = isReversed ? cmpNode->left : cmpNode->right;
            Value constValue = convert<ConstScalarQNode>(constNode)->data;
            bool isLess = isReversed ? cmpNode->greater : cmpNode->less;
            bool isEqual = cmpNode->equal;
            bool isGreater = isReversed ? cmpNode->less : cmpNode->greater;
            bool isOnlyEqual = isEqual && !isLess && !isGreater;
            if (isGreater || isOnlyEqual) {
                if (compareValue(from[fieldId], constValue) < 0) {
                    from[fieldId] = constValue;
                    incFrom = isEqual;
                }
            }
            if (isLess || isOnlyEqual) {
                if (compareValue(to[fieldId], constValue) > 0) {
                    to[fieldId] = constValue;
                    incTo = isEqual;
                }
            }
        }

        auto indexRead = make_unique<ReadTableIndexScanQNode>();
        indexRead->tableId = tableId;
        indexRead->indexId = indexId;
        indexRead->keySchema = indexSchema;
        indexRead->from = from;
        indexRead->to = to;
        indexRead->incFrom = incFrom;
        indexRead->incTo = incTo;
        indexRead->type = sysMan.getTableSchema(tableId);

        if (indexifyable.size() != n.children.size()) {
            auto filter = make_unique<FilterQNode>();
            if (indexifyable.size() != n.children.size() - 1) {
                auto newAnd = make_unique<AndConditionQNode>();
                for (int i = 0; i < n.children.size(); i++)
                    if (indexifyable.count(i) == 0)
                        newAnd->children.push_back(move(n.children[i]));
                filter->cond = move(newAnd);
            }
            else {
                for (int i = 0; i < n.children.size(); i++)
                    if (indexifyable.count(i) == 0)
                        filter->cond = move(n.children[i]);
            }
            filter->type = indexRead->type;
            filter->source = move(indexRead);
            result = move(filter);
        }
        else {
            result = move(indexRead);
        }
    }

    void filterifyAnd(AndConditionQNode& n) {
        int sourceIndex = plan.at(&n).indexId;
        n.children[sourceIndex]->accept(this);

        auto newAnd = make_unique<AndConditionQNode>();
        for (int i = 0; i < n.children.size(); i++)
            if (i != sourceIndex)
                newAnd->children.push_back(move(n.children[i]));

        auto filter = make_unique<FilterQNode>();
        filter->type = result->type;
        filter->source = move(result);
        filter->cond = move(newAnd);
        result = move(filter);
    }

public:
    TableReadBuildVisitor(uint16_t tableId, const SystemInfoManager& sysMan, const OptimizationPlan& plan)
        : tableId(tableId), sysMan(sysMan), plan(plan), result(nullptr) {}
    virtual void visitOrConditionQNode(OrConditionQNode& n) {
        if (plan.count(&n) == 0) return;
        if (plan.at(&n).option != OptimizationOption::Unionify) return;
        auto newNode = make_unique<UnionQNode>();
        for (auto& child : n.children) {
            child->accept(this);
            newNode->sources.push_back(move(result));
        }
        newNode->type = newNode->sources[0]->type;
        result = move(newNode);
    }
    virtual void visitAndConditionQNode(AndConditionQNode& n) {
        if (plan.count(&n) == 0) return;
        if (plan.at(&n).option == OptimizationOption::IndexifyAnd) {
            indexifyAnd(n);
        }
        if (plan.at(&n).option == OptimizationOption::FilterifyAnd) {
            filterifyAnd(n);
        }
    }
    virtual void visitCompareConditionQNode(CompareConditionQNode& n) {
        if (plan.count(&n) == 0) return;
        if (plan.at(&n).option != OptimizationOption::Indexify) return;
        uint16_t indexId = plan.at(&n).indexId;
        const Schema& indexSchema = sysMan.getIndexSchema(tableId, indexId);
        bool isReversed;
        fitsWith(n, indexSchema.columns[0].id, &isReversed);
        ValueArray from(indexSchema.columns.size(), Value(ValueType::MinVal));
        ValueArray to(indexSchema.columns.size(), Value(ValueType::MaxVal));
        bool incFrom = false, incTo = false;
        auto& constNode = isReversed ? n.left : n.right;
        Value constValue = convert<ConstScalarQNode>(constNode)->data;
        bool isLess = isReversed ? n.greater : n.less;
        bool isEqual = n.equal;
        bool isGreater = isReversed ? n.less : n.greater;
        bool isOnlyEqual = isEqual && !isLess && !isGreater;
        if (isGreater || isOnlyEqual) {
            from[0] = constValue;
            incFrom = isEqual;
        }
        if (isLess || isOnlyEqual) {
            to[0] = constValue;
            incTo = isEqual;
        }
        auto indexRead = make_unique<ReadTableIndexScanQNode>();
        indexRead->tableId = tableId;
        indexRead->indexId = indexId;
        indexRead->keySchema = indexSchema;
        indexRead->from = from;
        indexRead->to = to;
        indexRead->incFrom = incFrom;
        indexRead->incTo = incTo;
        indexRead->type = sysMan.getTableSchema(tableId);
        result = move(indexRead);
    }
    inline QTablePtr getResult() {
        return move(result);
    }
};

class TableReadVisitor : public QTableNode::RecursiveVisitor {
private:
    const SystemInfoManager& sysMan;
public:
    TableReadVisitor(QTablePtr* qPtr, const SystemInfoManager& sysMan)
        : sysMan(sysMan)
        , RecursiveVisitor(qPtr) {}
    virtual void visitFilterQNode(FilterQNode& n) {
        RecursiveVisitor::visitFilterQNode(n);
        auto tableNode = convert<ReadTableQNode>(n.source);
        if (!tableNode) return;

        auto testVisitor = make_unique<WhereConditionOptimizableVisitor>(
            tableNode->tableId, sysMan);
        n.cond->accept(testVisitor.get());
        if (!testVisitor->getResult()) return;
        auto transformVisitor = make_unique<TableReadBuildVisitor>(
            tableNode->tableId, sysMan, testVisitor->getPlan());
        n.cond->accept(transformVisitor.get());
        QTablePtr transformed = transformVisitor->getResult();
        *qPtr = move(transformed);
    }
};

void optimizeTableReads(QTablePtr& tree, const SystemInfoManager& sysMan) {
    auto vis = make_unique<TableReadVisitor>(&tree, sysMan);
    tree->accept(vis.get());
}