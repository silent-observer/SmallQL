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
        tm tmData = {0, 0, 0, 1, 0, 0, 0, 0, 0};
        ss >> get_time(&tmData, format.c_str());
        if (ss.fail()) {
            result = Value(ValueType::Null);
        }
        else {
            result = Value(Datetime(tmData, true, true));
        }
    }
    else if (n.name == "UCASE") {
        n.children[0]->accept(this);
        for (auto& c: result.stringVal) c = toupper(c);
    }
    else if (n.name == "LCASE") {
        n.children[0]->accept(this);
        for (auto& c: result.stringVal) c = tolower(c);
    }
    else if (n.name == "LENGTH") {
        n.children[0]->accept(this);
        result = Value(result.stringVal.size());
    }
    else if (n.name == "SUBSTR") {
        n.children[0]->accept(this);
        string s = result.stringVal;
        n.children[1]->accept(this);
        int pos = result.intVal;
        if (n.children.size() == 2) {
            result = Value(s.substr(pos));
        }
        else {
            n.children[2]->accept(this);
            result = Value(s.substr(pos, result.intVal));
        }
    }
    else if (n.name == "ROUND") {
        n.children[0]->accept(this);
        result = Value((int)round(result.doubleVal));
    }
    else if (n.name == "CEIL") {
        n.children[0]->accept(this);
        result = Value((int)ceil(result.doubleVal));
    }
    else if (n.name == "FLOOR") {
        n.children[0]->accept(this);
        result = Value((int)floor(result.doubleVal));
    }
    else if (n.name == "ABS") {
        n.children[0]->accept(this);
        if (result.type == ValueType::Double)
            result.doubleVal = abs(result.doubleVal);
        else if (result.type == ValueType::Integer)
            result.intVal = abs(result.intVal);
    }
    else if (n.name == "SQRT") {
        n.children[0]->accept(this);
        if (result.type == ValueType::Double)
            result = Value(sqrt(result.doubleVal));
        else if (result.type == ValueType::Integer)
            result = Value(sqrt(result.intVal));
    }
    else if (n.name == "POW") {
        n.children[0]->accept(this);
        double base = result.type == ValueType::Double ? result.doubleVal : result.intVal;
        n.children[1]->accept(this);
        double exponent = result.type == ValueType::Double ? result.doubleVal : result.intVal;
        result = Value(pow(base, exponent));
    }
    else if (n.name == "RAND") {
        result = Value((double)rand() / RAND_MAX);
    }
    else if (n.name == "NOW") {
        time_t now = time(0);
        tm tmData;
        localtime_s(&tmData, &now);
        result = Value(Datetime(tmData, true, true));
    }
    else if (n.name == "YEAR") {
        n.children[0]->accept(this);
        result = Value(result.datetimeVal.year);
    }
    else if (n.name == "MONTH") {
        n.children[0]->accept(this);
        result = Value(result.datetimeVal.month);
    }
    else if (n.name == "DAY") {
        n.children[0]->accept(this);
        result = Value(result.datetimeVal.day);
    }
    else if (n.name == "HOUR") {
        n.children[0]->accept(this);
        result = Value(result.datetimeVal.hour);
    }
    else if (n.name == "MINUTE") {
        n.children[0]->accept(this);
        result = Value(result.datetimeVal.minute);
    }
    else if (n.name == "SECOND") {
        n.children[0]->accept(this);
        result = Value(result.datetimeVal.second);
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