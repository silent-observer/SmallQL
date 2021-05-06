#pragma once
#include <string>
#include <memory>
#include <vector>
#include <sstream>
#include "DataType.h"
#include "QueryTree.h"
#include "SystemInfoManager.h"

using namespace std;

struct AstNode {
    int pos;
    inline string prettyPrint() const {
        stringstream s;
        prettyPrint(s, 0);
        return s.str();
    };
    virtual void prettyPrint(ostream& s, int level) const = 0;
};

struct NonAlgebrizableNode : public AstNode {};
struct TableAlgebrizableNode : public AstNode {
    virtual QTablePtr algebrize(const SystemInfoManager& sysMan) = 0;
};
struct ScalarAlgebrizableNode : public AstNode {
    virtual QScalarPtr algebrizeWithContext(
        const SystemInfoManager& sysMan, 
        const IntermediateType& type,
        const IntermediateType& preaggrType) const = 0;
};
struct ConditionAlgebrizableNode : public AstNode {
    virtual QCondPtr algebrizeWithContext(
        const SystemInfoManager& sysMan, 
        const IntermediateType& type,
        const IntermediateType& preaggrType) const = 0;
};

struct StatementNode : public TableAlgebrizableNode {};
struct TableExpr : public TableAlgebrizableNode {};
struct ExprNode : public ScalarAlgebrizableNode {};
struct InsertDataNode : public TableAlgebrizableNode {
    virtual QTablePtr algebrizeWithContext(
        const SystemInfoManager& sysMan, 
        const IntermediateType& type) const = 0;
};
struct ConditionNode : public ConditionAlgebrizableNode {};

struct TableName;

struct InsertStmtNode : public StatementNode {
    unique_ptr<TableName> tableName;
    unique_ptr<InsertDataNode> insertData;
    virtual void prettyPrint(ostream& s, int level) const;
    virtual QTablePtr algebrize(const SystemInfoManager& sysMan);
};

struct SelectNode : public TableExpr {
    unique_ptr<TableExpr> from;
    vector<pair<unique_ptr<ExprNode>, string>> columns;
    unique_ptr<ConditionNode> whereCond;
    vector<pair<unique_ptr<ExprNode>, bool>> orderBy;
    vector<unique_ptr<ExprNode>> groupBy;
    virtual void prettyPrint(ostream& s, int level) const;
    virtual QTablePtr algebrize(const SystemInfoManager& sysMan);
};

struct DeleteStmtNode : public StatementNode {
    unique_ptr<TableName> tableName;
    unique_ptr<ConditionNode> whereCond;
    virtual void prettyPrint(ostream& s, int level) const;
    virtual QTablePtr algebrize(const SystemInfoManager& sysMan);
};

struct SelectStmtNode : public StatementNode {
    SelectNode select;
    virtual void prettyPrint(ostream& s, int level) const;
    virtual QTablePtr algebrize(const SystemInfoManager& sysMan);
};

struct TableName : public TableExpr {
    string name;
    string alias;
    virtual void prettyPrint(ostream& s, int level) const;
    virtual QTablePtr algebrize(const SystemInfoManager& sysMan);
};

struct TableSubquery : public TableExpr {
    unique_ptr<SelectNode> query;
    string alias;
    virtual void prettyPrint(ostream& s, int level) const;
    virtual QTablePtr algebrize(const SystemInfoManager& sysMan);
};

struct ColumnNameExpr : public ExprNode {
    string name;
    string tableName;
    virtual void prettyPrint(ostream& s, int level) const;
    virtual QScalarPtr algebrizeWithContext(
        const SystemInfoManager& sysMan,
        const IntermediateType& type,
        const IntermediateType& preaggrType) const;
};

struct ConstExpr : public ExprNode {
    Value v;
    virtual void prettyPrint(ostream& s, int level) const;
    virtual QScalarPtr algebrizeWithContext(
        const SystemInfoManager& sysMan,
        const IntermediateType& type,
        const IntermediateType& preaggrType) const;
};

struct FuncExpr : public ExprNode {
    string name;
    vector<unique_ptr<ExprNode>> children;
    virtual void prettyPrint(ostream& s, int level) const;
    virtual QScalarPtr algebrizeWithContext(
        const SystemInfoManager& sysMan,
        const IntermediateType& type,
        const IntermediateType& preaggrType) const;
};

struct AggrFuncExpr : public ExprNode {
    string name;
    unique_ptr<ExprNode> child;
    virtual void prettyPrint(ostream& s, int level) const;
    virtual QScalarPtr algebrizeWithContext(
        const SystemInfoManager& sysMan,
        const IntermediateType& type,
        const IntermediateType& preaggrType) const;
};

struct InsertValuesNode : public InsertDataNode {
    vector<ValueArray> data;
    virtual void prettyPrint(ostream& s, int level) const;
    virtual QTablePtr algebrize(const SystemInfoManager& sysMan) {
        return NULL;
    }
    virtual QTablePtr algebrizeWithContext(
        const SystemInfoManager& sysMan,
        const IntermediateType& type) const;
};

struct AndConditionNode : public ConditionNode {
    vector<unique_ptr<ConditionNode>> children;
    virtual void prettyPrint(ostream& s, int level) const;
    virtual QCondPtr algebrizeWithContext(
        const SystemInfoManager& sysMan, 
        const IntermediateType& type,
        const IntermediateType& preaggrType) const;
};

struct OrConditionNode : public ConditionNode {
    vector<unique_ptr<ConditionNode>> children;
    virtual void prettyPrint(ostream& s, int level) const;
    virtual QCondPtr algebrizeWithContext(
        const SystemInfoManager& sysMan, 
        const IntermediateType& type,
        const IntermediateType& preaggrType) const;
};

struct JoinNode : public TableExpr {
    JoinType joinType;
    unique_ptr<TableExpr> left, right;
    unique_ptr<ConditionNode> on;
    virtual void prettyPrint(ostream& s, int level) const;
    virtual QTablePtr algebrize(const SystemInfoManager& sysMan);
};

enum class CompareType {
    Equal,
    NotEqual,
    Greater,
    GreaterOrEqual,
    Less,
    LessOrEqual
};

struct CompareConditionNode : public ConditionNode {
    unique_ptr<ExprNode> left, right;
    CompareType condType;
    virtual void prettyPrint(ostream& s, int level) const;
    virtual QCondPtr algebrizeWithContext(
        const SystemInfoManager& sysMan, 
        const IntermediateType& type,
        const IntermediateType& preaggrType) const;
};

struct ColumnSpecNode : public NonAlgebrizableNode {
    string name;
    string typeStr;
    shared_ptr<DataType> type;
    bool isPrimary;
    bool canBeNull;
    virtual void prettyPrint(ostream& s, int level) const;
};

struct CreateTableNode : public StatementNode {
    string name;
    vector<unique_ptr<ColumnSpecNode>> columns;
    virtual void prettyPrint(ostream& s, int level) const;
    virtual QTablePtr algebrize(const SystemInfoManager& sysMan) { return nullptr; };
};

struct DropTableNode : public StatementNode {
    string name;
    virtual void prettyPrint(ostream& s, int level) const;
    virtual QTablePtr algebrize(const SystemInfoManager& sysMan) { return nullptr; };
};