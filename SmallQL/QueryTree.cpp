#include "QueryTree.h"

class ScalarPrinter : public QScalarNode::Visitor {
public:
    virtual void visitColumnQNode(ColumnQNode& n) {
        cout << "Column<" << n.name << ">";
    }
    virtual void visitConstScalarQNode(ConstScalarQNode& n) {
        cout << n.data;
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
    int level;
public:
    TablePrinter(ConditionPrinter* condPrinter)
        : condPrinter(condPrinter) {}
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
    auto table = make_unique<TablePrinter>(cond.get());
    n->accept(table.get());
}