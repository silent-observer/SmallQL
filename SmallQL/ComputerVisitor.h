#pragma once

#include "QueryTree.h"

class ComputerVisitor : public QScalarNode::Visitor {
private:
    const IntermediateType& type;
    const ValueArray& record;
    Value result;
public:
    ComputerVisitor(const IntermediateType& type, const ValueArray& record)
        : type(type), record(record) {}
    inline const Value& getResult() const {
        return result;
    }
    virtual void visitColumnQNode(ColumnQNode& n);
    virtual void visitAsteriskQNode(AsteriskQNode& n) {}
    virtual void visitConstScalarQNode(ConstScalarQNode& n);
    virtual void visitFuncQNode(FuncQNode& n);
};