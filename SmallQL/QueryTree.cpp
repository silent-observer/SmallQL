#include "QueryTree.h"

#include <iostream>

class ScalarPrinter : public QScalarNode::Visitor {
public:
    virtual void visitColumnQNode(ColumnQNode& n) {
        cout << "Column<" << n.name << ">";
    }
    virtual void visitAsteriskQNode(AsteriskQNode& n) {
        if (n.tableName != "")
            cout << n.tableName << ".";
        cout << "*";
    }
    virtual void visitConstScalarQNode(ConstScalarQNode& n) {
        cout << n.data;
    }
    virtual void visitFuncQNode(FuncQNode& n) {
        cout << n.name << "(";
        for (unsigned int i = 0; i < n.children.size(); i++) {
            n.children[i]->accept(this);
            if (i != n.children.size() - 1)
                cout << ", ";
        }
        cout << ")";
    }
};

class ConditionPrinter : public QConditionNode::Visitor {
private:
    ScalarPrinter* scalarPrinter;
public:
    ConditionPrinter(ScalarPrinter* scalarPrinter)
        : scalarPrinter(scalarPrinter) {}
    virtual void visitAndConditionQNode(AndConditionQNode& n) {
        cout << "AND(";
        for (int i = 0; i < n.children.size(); i++) {
            if (i > 0) cout << ", ";
            n.children[i]->accept(this);
        }
        cout << ")";
    }
    virtual void visitOrConditionQNode(OrConditionQNode& n) {
        cout << "OR(";
        for (int i = 0; i < n.children.size(); i++) {
            if (i > 0) cout << ", ";
            n.children[i]->accept(this);
        }
        cout << ")";
    }
    virtual void visitCompareConditionQNode(CompareConditionQNode& n) {
        n.left->accept(scalarPrinter);
        cout << " ";
        if (n.less && n.greater) cout << "!=";
        else if (n.less) cout << "<";
        else if (n.greater) cout << ">";
        if (n.equal) cout << "=";
        cout << " ";
        n.right->accept(scalarPrinter);
    }
};

#define indent() string(level*2, ' ')
#define indent1() string((level + 1)*2, ' ')
#define indentM1() string((level - 1)*2, ' ')

class TablePrinter : public QTableNode::Visitor {
private:
    ConditionPrinter* condPrinter;
    ScalarPrinter* scalarPrinter;
    int level;
public:
    TablePrinter(ConditionPrinter* condPrinter, ScalarPrinter* scalarPrinter)
        : condPrinter(condPrinter), scalarPrinter(scalarPrinter) {}
    virtual void visitReadTableQNode(ReadTableQNode& n) {
        cout << indent() << "FullScan[" << endl;
        cout << indent1() << "tableId = " << n.tableId << endl;
        cout << indent1() << "Type: " << n.type << endl;
        cout << indent() << "]" << endl;
    }
    virtual void visitReadTableIndexScanQNode(ReadTableIndexScanQNode& n) {
        cout << indent() << "IndexScan[" << endl;
        cout << indent1() << "tableId = " << n.tableId << ", ";
        cout << "indexId = " << n.indexId << endl;
        cout << indent1() << "Index: " << n.keySchema << endl;
        cout << indent1() << (n.incFrom ? "From(=): " : "From: ");
        cout << n.from << endl;
        cout << indent1() << (n.incTo ? "To(=): " : "To: ");
        cout << n.to << endl;
        cout << indent1() << "Type: " << n.type << endl;
        cout << indent() << "]" << endl;
    }
    virtual void visitProjectionQNode(ProjectionQNode& n) {
        cout << indent1() << "Projection[" << endl;
        cout << indent1() << "Type: " << n.type << endl;
        cout << indent() << "] <-" << endl;
        level++;
        n.source->accept(this);
        level--;
    }
    virtual void visitFuncProjectionQNode(FuncProjectionQNode& n) {
        cout << indent1() << "FuncProjection[" << endl;
        cout << indent1() << "Type: " << n.type << endl;
        cout << indent1() << "Funcs: " << endl;
        level++;
        for (auto& f : n.funcs) {
            cout << indent1();
            f->accept(scalarPrinter);
            cout << endl;
        }
        level--;
        cout << indent() << "] <-" << endl;
        level++;
        n.source->accept(this);
        level--;
    }
    virtual void visitFilterQNode(FilterQNode& n) {
        cout << indent() << "Filter[" << endl;
        cout << indent1() << "Condition: ";
        n.cond->accept(condPrinter); 
        cout << endl;
        cout << indent1() << "Type: " << n.type << endl;
        cout << indent() << "] <-" << endl;
        level++;
        n.source->accept(this);
        level--;
    }
    virtual void visitUnionQNode(UnionQNode& n) {
        cout << indent() << "Union[" << endl;
        level++;
        for (int i = 0; i < n.sources.size(); i++) {
            if (i != 0)
                cout << indentM1() << "," << endl;
            n.sources[i]->accept(this);
        }
        level--;
        cout << indent() << "]";
    }
    virtual void visitJoinQNode(JoinQNode& n) {
        cout << indent();
        switch (n.joinType)
        {
        case JoinType::Cross:
            cout << "Cross";
            break;
        case JoinType::Inner:
            cout << "Inner";
            break;
        case JoinType::Left:
            cout << "Left";
            break;
        case JoinType::Right:
            cout << "Right";
            break;
        case JoinType::Full:
            cout << "Full";
            break;
        }
        cout << "Join[" << endl;
        cout << indent1() << "Type: " << n.type << endl;
        if (n.on) {
            cout << indent1() << "On: ";
            n.on->accept(condPrinter);
            cout << endl;
        }
        cout << indent() << "] <- [" << endl;
        level++;
        n.left->accept(this);
        cout << indentM1() << "," << endl;
        n.right->accept(this);
        level--;
        cout << indent() << "]";
    }
    virtual void visitSorterQNode(SorterQNode& n) {
        cout << indent() << "Sorter[" << endl;
        cout << indent1() << "Sort plan: ";
        for (const auto& p : n.cmpPlan) {
            cout << p.first << (p.second ? "v" : "^") << " ";
        }
        cout << endl;
        cout << indent1() << "Type: " << n.type << endl;
        cout << indent() << "] <-" << endl;
        level++;
        n.source->accept(this);
        level--;
    }
    virtual void visitGroupifierQNode(GroupifierQNode& n) {
        cout << indent() << "Groupifier[" << endl;
        cout << indent1() << "Group plan: ";
        for (const auto& b : n.groupPlan) {
            cout << (b ? "G" : "_");
        }
        cout << endl;
        cout << indent1() << "Type: " << n.type << endl;
        cout << indent() << "] <-" << endl;
        level++;
        n.source->accept(this);
        level--;
    }
    virtual void visitDegroupifierQNode(DegroupifierQNode& n) {
        cout << indent() << "Degroupifier[" << endl;
        cout << indent1() << "Type: " << n.type << endl;
        cout << indent() << "] <-" << endl;
        level++;
        n.source->accept(this);
        level--;
    }
    virtual void visitSelectorNode(SelectorNode& n) {
        cout << indent() << "SELECT" << endl;
        n.source->accept(this);
    }
    virtual void visitInserterNode(InserterNode& n) {
        cout << indent() << "INSERT INTO " << n.tableId << endl;
        n.source->accept(this);
    }
    virtual void visitConstDataNode(ConstDataNode& n) {
        cout << indent() << "ConstData" << endl;
        level++;
        for (auto& v : n.data)
            cout << indent() << v << endl;
        level--;
    }
};

void print(QTablePtr& n) {
    auto scalar = make_unique<ScalarPrinter>();
    auto cond = make_unique<ConditionPrinter>(scalar.get());
    auto table = make_unique<TablePrinter>(cond.get(), scalar.get());
    n->accept(table.get());
}