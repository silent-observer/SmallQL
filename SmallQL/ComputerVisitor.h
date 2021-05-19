#pragma once

#include "QueryTree.h"

class ComputerVisitor : public QScalarNode::Visitor {
protected:
    const IntermediateType& type;
    const ValueArray* record;
    Value result;
public:
    ComputerVisitor(const IntermediateType& type, const ValueArray* record)
        : type(type), record(record) {}
    inline const Value& getResult() const {
        return result;
    }
    virtual void visitColumnQNode(ColumnQNode& n);
    virtual void visitAsteriskQNode(AsteriskQNode& n) {}
    virtual void visitConstScalarQNode(ConstScalarQNode& n);
    virtual void visitFuncQNode(FuncQNode& n);
    virtual void visitAggrFuncQNode(AggrFuncQNode& n) {}
};

class GroupComputerVisitor : public ComputerVisitor {
private:
    const vector<ValueArray>* group;
public:
    GroupComputerVisitor(const IntermediateType& type, const vector<ValueArray>* group)
        : group(group), ComputerVisitor(type, nullptr) {}
    inline const Value& getResult() const {
        return result;
    }
    virtual void visitAggrFuncQNode(AggrFuncQNode& n);
};

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