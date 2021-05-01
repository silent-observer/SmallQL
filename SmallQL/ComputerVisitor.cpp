#include "ComputerVisitor.h"

void ComputerVisitor::visitConstScalarQNode(ConstScalarQNode& n) {
    result = n.data;
}
void ComputerVisitor::visitColumnQNode(ColumnQNode& n) {
    result = record[n.columnId];
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
    }
}