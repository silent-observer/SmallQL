#include "Optimizer.h"

class CondOptimizerVisitor : public QConditionNode::RecursiveVisitor {
public:
    virtual void visitAndConditionQNode(AndConditionQNode& n) {
        RecursiveVisitor::visitAndConditionQNode(n);
        for (int i = 0; i < n.children.size(); i++) {
            auto casted = convert<AndConditionQNode>(n.children[i]);
            if (casted) {
                for (auto& child : casted->children)
                    n.children.push_back(move(child));
                n.children.erase(n.children.begin() + i);
                i--;
            }
        }
    }
    virtual void visitOrConditionQNode(OrConditionQNode& n) {
        RecursiveVisitor::visitOrConditionQNode(n);
        for (int i = 0; i < n.children.size(); i++) {
            auto casted = convert<OrConditionQNode>(n.children[i]);
            if (casted) {
                n.children.erase(n.children.begin() + i);
                for (auto& child : casted->children)
                    n.children.push_back(move(child));
                i--;
            }
        }
    }
};

class CondOptimizerTableVisitor : public QTableNode::RecursiveVisitor {
public:
    CondOptimizerTableVisitor(): RecursiveVisitor(nullptr) {}
    virtual void visitFilterQNode(FilterQNode& n) {
        RecursiveVisitor::visitFilterQNode(n);
        auto vis = make_unique<CondOptimizerVisitor>();
        n.cond->accept(vis.get());
    }
};

void optimizeConditions(QTablePtr& tree) {
    auto vis = make_unique<CondOptimizerTableVisitor>();
    tree->accept(vis.get());
}