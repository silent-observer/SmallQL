#pragma once
#include <string>
#include <vector>
#include <memory>
#include "DataType.h"
#include "Executor.h"
#include "Common.h"

using namespace std;

struct QTableNode {
    Schema type;
    int id;
    QTableNode(): id(0) {}
    class Visitor;
    class RecursiveVisitor;
    virtual void accept(Visitor* v) = 0;
};
using QTablePtr = unique_ptr<QTableNode>;
struct QScalarNode {
    SchemaEntry type;
    QScalarNode() {}
    class Visitor;
    class RecursiveVisitor;
    virtual void accept(Visitor* v) = 0;
};
using QScalarPtr = unique_ptr<QScalarNode>;
struct QConditionNode {
    class Visitor;
    class RecursiveVisitor;
    virtual void accept(Visitor* v) = 0;
};
using QCondPtr = unique_ptr<QConditionNode>;


struct ReadTableQNode;
struct ReadTableIndexScanQNode;
struct ProjectionQNode;
struct FuncProjectionQNode;
struct FilterQNode;
struct UnionQNode;
struct SelectorNode;
struct InserterNode;
struct ConstDataNode;
struct ColumnQNode;
struct FuncQNode;
struct ConstScalarQNode;
struct AndConditionQNode;
struct OrConditionQNode;
struct CompareConditionQNode;


class QTableNode::Visitor {
public:
    virtual void visitReadTableQNode(ReadTableQNode& n) = 0;
    virtual void visitReadTableIndexScanQNode(ReadTableIndexScanQNode& n) = 0;
    virtual void visitProjectionQNode(ProjectionQNode& n) = 0;
    virtual void visitFuncProjectionQNode(FuncProjectionQNode& n) = 0;
    virtual void visitFilterQNode(FilterQNode& n) = 0;
    virtual void visitUnionQNode(UnionQNode& n) = 0;
    virtual void visitSelectorNode(SelectorNode& n) = 0;
    virtual void visitInserterNode(InserterNode& n) = 0;
    virtual void visitConstDataNode(ConstDataNode& n) = 0;
};
class QScalarNode::Visitor {
public:
    virtual void visitColumnQNode(ColumnQNode& n) = 0;
    virtual void visitConstScalarQNode(ConstScalarQNode& n) = 0;
    virtual void visitFuncQNode(FuncQNode& n) = 0;
};
class QConditionNode::Visitor {
public:
    virtual void visitAndConditionQNode(AndConditionQNode& n) = 0;
    virtual void visitOrConditionQNode(OrConditionQNode& n) = 0;
    virtual void visitCompareConditionQNode(CompareConditionQNode& n) = 0;
};


struct ReadTableQNode : public QTableNode {
    uint16_t tableId;
    ReadTableQNode() {}
    virtual void accept(QTableNode::Visitor* v) {
        v->visitReadTableQNode(*this);
    }
};
struct ReadTableIndexScanQNode : public QTableNode {
    uint16_t tableId;
    uint16_t indexId;
    Schema keySchema;
    ValueArray from, to;
    bool incFrom, incTo;
    ReadTableIndexScanQNode() {}
    virtual void accept(Visitor* v) {
        v->visitReadTableIndexScanQNode(*this);
    }
};

struct ProjectionQNode : public QTableNode {
    vector<uint16_t> columns;
    QTablePtr source;
    ProjectionQNode() {}
    virtual void accept(Visitor* v) {
        v->visitProjectionQNode(*this);
    }
};
struct FuncProjectionQNode : public QTableNode {
    vector<QScalarPtr> funcs;
    QTablePtr source;
    FuncProjectionQNode() {}
    virtual void accept(Visitor* v) {
        v->visitFuncProjectionQNode(*this);
    }
};

struct AndConditionQNode : public QConditionNode {
    vector<QCondPtr> children;
    AndConditionQNode() {}
    virtual void accept(Visitor* v) {
        v->visitAndConditionQNode(*this);
    }
};
struct OrConditionQNode : public QConditionNode {
    vector<QCondPtr> children;
    OrConditionQNode() {}
    virtual void accept(Visitor* v) {
        v->visitOrConditionQNode(*this);
    }
};
struct CompareConditionQNode : public QConditionNode {
    QScalarPtr left, right;
    bool less, equal, greater;
    CompareConditionQNode() {}
    virtual void accept(Visitor* v) {
        v->visitCompareConditionQNode(*this);
    }
};

struct FilterQNode : public QTableNode {
    QCondPtr cond;
    QTablePtr source;
    FilterQNode() {}
    virtual void accept(Visitor* v) {
        v->visitFilterQNode(*this);
    }
};

struct UnionQNode : public QTableNode {
    vector<QTablePtr> sources;
    UnionQNode() {}
    virtual void accept(Visitor* v) {
        v->visitUnionQNode(*this);
    }
};

struct SelectorNode : public QTableNode {
    QTablePtr source;
    SelectorNode() {}
    virtual void accept(Visitor* v) {
        v->visitSelectorNode(*this);
    }
};

struct InserterNode : public QTableNode {
    uint16_t tableId;
    QTablePtr source;
    InserterNode() {}
    virtual void accept(Visitor* v) {
        v->visitInserterNode(*this);
    }
};

struct ConstDataNode : public QTableNode {
    vector<ValueArray> data;
    ConstDataNode() {}
    virtual void accept(Visitor* v) {
        v->visitConstDataNode(*this);
    }
};

struct ColumnQNode : public QScalarNode {
    string name;
    uint16_t columnId;
    ColumnQNode() {}
    virtual void accept(Visitor* v) {
        v->visitColumnQNode(*this);
    }
};

struct ConstScalarQNode : public QScalarNode {
    Value data;
    ConstScalarQNode() {}
    virtual void accept(Visitor* v) {
        v->visitConstScalarQNode(*this);
    }
};

struct FuncQNode : public QScalarNode {
    string name;
    vector<QScalarPtr> children;
    virtual void accept(Visitor* v) {
        v->visitFuncQNode(*this);
    }
};


class QTableNode::RecursiveVisitor : public QTableNode::Visitor {
protected:
    QTablePtr* qPtr;
public:
    RecursiveVisitor(QTablePtr* qPtr): qPtr(qPtr) {}
    virtual void visitReadTableQNode(ReadTableQNode& n) {}
    virtual void visitReadTableIndexScanQNode(ReadTableIndexScanQNode& n) {}
    virtual void visitProjectionQNode(ProjectionQNode& n) {
        auto oldQPtr = qPtr;
        qPtr = &n.source;
        n.source->accept(this);
        qPtr = oldQPtr;
    }
    virtual void visitFuncProjectionQNode(FuncProjectionQNode& n) {
        auto oldQPtr = qPtr;
        qPtr = &n.source;
        n.source->accept(this);
        qPtr = oldQPtr;
    }
    virtual void visitFilterQNode(FilterQNode& n) {
        auto oldQPtr = qPtr;
        qPtr = &n.source;
        n.source->accept(this);
        qPtr = oldQPtr;
    }
    virtual void visitUnionQNode(UnionQNode& n) {
        auto oldQPtr = qPtr;
        for (auto& source : n.sources) {
            qPtr = &source;
            source->accept(this);
        }
        qPtr = oldQPtr;
    }
    virtual void visitSelectorNode(SelectorNode& n) {
        auto oldQPtr = qPtr;
        qPtr = &n.source;
        n.source->accept(this);
        qPtr = oldQPtr;
    }
    virtual void visitInserterNode(InserterNode& n) {
        auto oldQPtr = qPtr;
        qPtr = &n.source;
        n.source->accept(this);
        qPtr = oldQPtr;
    }
    virtual void visitConstDataNode(ConstDataNode& n) {}
};
class QScalarNode::RecursiveVisitor : public QScalarNode::Visitor {
public:
    virtual void visitColumnQNode(ColumnQNode& n) {};
    virtual void visitConstScalarQNode(ConstScalarQNode& n) {};
    virtual void visitFuncQNode(FuncQNode& n) {
        for (auto& child : n.children)
            child->accept(this);
    };
};
class QConditionNode::RecursiveVisitor : public QConditionNode::Visitor {
public:
    virtual void visitAndConditionQNode(AndConditionQNode& n) {
        for (auto& child : n.children)
            child->accept(this);
    };
    virtual void visitOrConditionQNode(OrConditionQNode& n) {
        for (auto& child : n.children)
            child->accept(this);
    };
    virtual void visitCompareConditionQNode(CompareConditionQNode& n) {};
};

class SemanticException : public SQLException
{
private:
    string message;
public:
    SemanticException(string message)
        : message("Semantic Exception: " + message) {}
    const char* what() const throw ()
    {
        return message.c_str();
    }
};

void print(QTablePtr& n);