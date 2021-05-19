#include "ComputerVisitor.h"
#include <sstream>
#include <iomanip>

void ComputerVisitor::visitConstScalarQNode(ConstScalarQNode& n) {
    result = n.data;
}
void ComputerVisitor::visitColumnQNode(ColumnQNode& n) {
    result = (*record)[n.columnId];
}
void ComputerVisitor::visitFuncQNode(FuncQNode& n) {
    if (n.children.size() == 1 && n.name == "-") {
        bool isDouble = is<DoubleType>(n.type.type);
        n.children[0]->accept(this);
        if (isDouble)
            result.doubleVal = -result.doubleVal;
        else
            result.intVal = -result.intVal;
    }
    else if (n.name == "+" || n.name == "-" || n.name == "*" || n.name == "/") {
        char c = n.name[0];
        bool isDouble = is<DoubleType>(n.type.type);

        n.children[0]->accept(this);
        if (result.type == ValueType::Null) return;
        if (isDouble) result.convertToDouble();
        Value v = result;
        for (int i = 1; i < n.children.size(); i++) {
            n.children[i]->accept(this);
            if (result.type == ValueType::Null) return;
            if (isDouble) result.convertToDouble();

            if (isDouble) {
                switch (c)
                {
                case '+':
                    v.doubleVal += result.doubleVal;
                    break;
                case '-':
                    v.doubleVal -= result.doubleVal;
                    break;
                case '*':
                    v.doubleVal *= result.doubleVal;
                    break;
                case '/':
                    v.doubleVal /= result.doubleVal;
                    break;
                default:
                    break;
                }
            }
            else {
                switch (c)
                {
                case '+':
                    v.intVal += result.intVal;
                    break;
                case '-':
                    v.intVal -= result.intVal;
                    break;
                case '*':
                    v.intVal *= result.intVal;
                    break;
                case '/':
                    v.intVal /= result.intVal;
                    break;
                default:
                    break;
                }
            }
        }
        result = v;
    } else if (n.name == "CONCAT") {
        n.children[0]->accept(this);
        Value v = result;
        if (result.type == ValueType::Null) result = Value("");
        for (int i = 1; i < n.children.size(); i++) {
            n.children[i]->accept(this);
            if (result.type == ValueType::Null) continue;
            v.stringVal += result.stringVal;
        }
        result = v;
    } else if (n.name == "TO_DATETIME") {
        string format = "%Y-%m-%d %H:%M:%S";
        if (n.children.size() == 2) {
            n.children[0]->accept(this);
            if (result.type == ValueType::Null) return;
            format = result.stringVal;
        }
        
        n.children.back()->accept(this);
        if (result.type == ValueType::Null) return;
        stringstream ss(result.stringVal);
        tm tmData;
        ss >> get_time(&tmData, format.c_str());
        if (ss.fail()) {
            result = Value(ValueType::Null);
        }
        else {
            result = Value(Datetime(tmData, true, true));
        }
    }
}

void GroupComputerVisitor::visitAggrFuncQNode(AggrFuncQNode& n) {
    if (n.name == "SUM") {
        bool isDouble = is<DoubleType>(n.type.type);
        Value v = isDouble ? Value(0.0) : Value(0);
        for (const auto& groupRow : *group) {
            record = &groupRow;
            n.child->accept(this);
            if (result.type == ValueType::Null) continue;
            if (isDouble)
                v.doubleVal += result.doubleVal;
            else
                v.intVal += result.intVal;
        }
        result = v;
    }
    else if (n.name == "AVG") {
        bool isDouble = is<DoubleType>(n.type.type);
        Value v = isDouble ? Value(0.0) : Value(0);
        int count = 0;
        for (const auto& groupRow : *group) {
            record = &groupRow;
            n.child->accept(this);
            if (result.type == ValueType::Null) continue;
            if (isDouble)
                v.doubleVal += result.doubleVal;
            else
                v.intVal += result.intVal;
            count++;
        }

        if (isDouble)
            v.doubleVal /= count;
        else
            v.intVal /= count;
        result = v;
    }
    else if (n.name == "MIN") {
        Value v = Value(ValueType::MaxVal);
        for (const auto& groupRow : *group) {
            record = &groupRow;
            n.child->accept(this);
            if (result.type == ValueType::Null) continue;
            if (n.child->type.compare(result, v) < 0)
                v = result;
        }
        result = v;
        if (result.type == ValueType::MaxVal)
            result.type = ValueType::Null;
    }
    else if (n.name == "MAX") {
        Value v = Value(ValueType::MinVal);
        for (const auto& groupRow : *group) {
            record = &groupRow;
            n.child->accept(this);
            if (result.type == ValueType::Null) continue;
            if (n.child->type.compare(result, v) > 0)
                v = result;
        }
        result = v;
        if (result.type == ValueType::MinVal)
            result.type = ValueType::Null;
    }
    else if (n.name == "COUNT" && is<AsteriskQNode>(n.child)) {
        result = Value(group->size());
    }
    else if (n.name == "COUNT") {
        int count = 0;
        for (const auto& groupRow : *group) {
            record = &groupRow;
            n.child->accept(this);
            if (result.type != ValueType::Null) count++;
        }
        result = Value(count);
    }
}


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