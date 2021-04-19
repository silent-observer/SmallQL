#pragma once

#include "QueryTree.h"

class ComputerVisitor : public QScalarNode::Visitor {
private:
    const Schema& schema;
    const ValueArray& record;
    Value result;
public:
    ComputerVisitor(const Schema& schema, const ValueArray& record)
        : schema(schema), record(record) {}
    inline const Value& getResult() const {
        return result;
    }
    virtual void visitColumnQNode(ColumnQNode& n);
    virtual void visitConstScalarQNode(ConstScalarQNode& n);
    virtual void visitFuncQNode(FuncQNode& n);
};