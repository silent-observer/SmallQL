#include "Optimizer.h"
#include "ComputerVisitor.h"

class ConstOptimizerScalarVisitor : public QScalarNode::RecursiveVisitor {
private:
    IntermediateType emptyType;
    unique_ptr<ComputerVisitor> visitor;
public:
    ConstOptimizerScalarVisitor()
        : emptyType()
        , visitor(make_unique<ComputerVisitor>(emptyType, nullptr)) {}
    virtual void visitFuncQNode(FuncQNode& n) {
        if (!n.type.isConst) {
            RecursiveVisitor::visitFuncQNode(n);
            return;
        }
        n.accept(visitor.get());
        auto newNode = make_unique<ConstScalarQNode>();
        newNode->data = visitor->getResult();
        newNode->type = n.type;
        *qPtr = move(newNode);
    }
    void setQPtr(QScalarPtr* qPtr) {
        this->qPtr = qPtr;
    }
};

class ConstOptimizerCondVisitor : public QConditionNode::RecursiveVisitor {
private:
    ConstOptimizerScalarVisitor* visitor;
public:
    ConstOptimizerCondVisitor(ConstOptimizerScalarVisitor* visitor)
        : visitor(visitor) {}
    virtual void visitCompareConditionQNode(CompareConditionQNode& n) {
        visitor->setQPtr(&n.left);
        n.left->accept(visitor);
        visitor->setQPtr(&n.right);
        n.right->accept(visitor);
    }
};

class ConstOptimizerTableVisitor : public QTableNode::RecursiveVisitor {
private:
    ConstOptimizerScalarVisitor* scalarVisitor;
    ConstOptimizerCondVisitor* condVisitor;
public:
    ConstOptimizerTableVisitor(
        ConstOptimizerScalarVisitor* scalarVisitor, 
        ConstOptimizerCondVisitor* condVisitor)
        : RecursiveVisitor(nullptr)
        , scalarVisitor(scalarVisitor)
        , condVisitor(condVisitor) {}
    virtual void visitFilterQNode(FilterQNode& n) {
        RecursiveVisitor::visitFilterQNode(n);
        n.cond->accept(condVisitor);
    }
    virtual void visitJoinQNode(JoinQNode& n) {
        RecursiveVisitor::visitJoinQNode(n);
        n.on->accept(condVisitor);
    }
    virtual void visitExprDataNode(ExprDataNode& n) {
        RecursiveVisitor::visitExprDataNode(n);
        bool allConsts = true;
        for (auto& r : n.data)
            for (auto& e : r) {
                if (!e->type.isConst) allConsts = false;
                scalarVisitor->setQPtr(&e);
                e->accept(scalarVisitor);
            }
        if (allConsts) {
            auto newNode = make_unique<ConstDataNode>();
            newNode->type = n.type;
            for (auto& r : n.data) {
                newNode->data.emplace_back();
                for (auto& e : r) {
                    newNode->data.back().push_back(convert<ConstScalarQNode>(e)->data);
                }
            }
            *qPtr = move(newNode);
        }
    }
    virtual void visitFuncProjectionQNode(FuncProjectionQNode& n) {
        RecursiveVisitor::visitFuncProjectionQNode(n);
        for (auto& e : n.funcs) {
            scalarVisitor->setQPtr(&e);
            e->accept(scalarVisitor);
        }
    }
    virtual void visitAggrFuncProjectionQNode(AggrFuncProjectionQNode& n) {
        RecursiveVisitor::visitAggrFuncProjectionQNode(n);
        for (auto& e : n.funcs) {
            scalarVisitor->setQPtr(&e);
            e->accept(scalarVisitor);
        }
    }
};

void optimizeConstants(QTablePtr& tree) {
    auto visS = make_unique<ConstOptimizerScalarVisitor>();
    auto visC = make_unique<ConstOptimizerCondVisitor>(visS.get());
    auto visT = make_unique<ConstOptimizerTableVisitor>(visS.get(), visC.get());
    tree->accept(visT.get());
}