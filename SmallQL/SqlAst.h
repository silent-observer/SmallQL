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
        const Schema& type) const = 0;
};
struct ConditionAlgebrizableNode : public AstNode {
    virtual QCondPtr algebrizeWithContext(
        const SystemInfoManager& sysMan, 
        const Schema& type) const = 0;
};

struct StatementNode : public TableAlgebrizableNode {};
struct TableExpr : public TableAlgebrizableNode {};
struct ExprNode : public ScalarAlgebrizableNode {};
struct QueryNode : public TableAlgebrizableNode {};
struct InsertDataNode : public TableAlgebrizableNode {
    virtual QTablePtr algebrizeWithContext(
        const SystemInfoManager& sysMan, 
        const Schema& type) const = 0;
};
struct ConditionNode : public ConditionAlgebrizableNode {};

struct TableName;

struct InsertStmtNode : public StatementNode {
    unique_ptr<TableName> tableName;
    unique_ptr<InsertDataNode> insertData;
    virtual void prettyPrint(ostream& s, int level) const;
    virtual QTablePtr algebrize(const SystemInfoManager& sysMan);
};

struct SelectNode : public QueryNode {
    unique_ptr<TableExpr> from;
    bool isStar;
    vector<pair<unique_ptr<ExprNode>, string>> columns;
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
    virtual void prettyPrint(ostream& s, int level) const;
    virtual QTablePtr algebrize(const SystemInfoManager& sysMan);
};

struct ColumnNameExpr : public ExprNode {
    string name;
    virtual void prettyPrint(ostream& s, int level) const;
    virtual QScalarPtr algebrizeWithContext(
        const SystemInfoManager& sysMan,
        const Schema& type) const;
};

struct ConstExpr : public ExprNode {
    Value v;
    virtual void prettyPrint(ostream& s, int level) const;
    virtual QScalarPtr algebrizeWithContext(
        const SystemInfoManager& sysMan,
        const Schema& type) const;
};

struct InsertValuesNode : public InsertDataNode {
    vector<ValueArray> data;
    virtual void prettyPrint(ostream& s, int level) const;
    virtual QTablePtr algebrize(const SystemInfoManager& sysMan) {
        return NULL;
    }
    virtual QTablePtr algebrizeWithContext(
        const SystemInfoManager& sysMan,
        const Schema& type) const;
};

struct AndConditionNode : public ConditionNode {
    vector<unique_ptr<ConditionNode>> children;
    virtual void prettyPrint(ostream& s, int level) const;
    virtual QCondPtr algebrizeWithContext(
        const SystemInfoManager& sysMan, 
        const Schema& type) const;
};

struct OrConditionNode : public ConditionNode {
    vector<unique_ptr<ConditionNode>> children;
    virtual void prettyPrint(ostream& s, int level) const;
    virtual QCondPtr algebrizeWithContext(
        const SystemInfoManager& sysMan, 
        const Schema& type) const;
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
        const Schema& type) const;
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