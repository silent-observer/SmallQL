#include "SqlAst.h"

#define indent() string(level*2, ' ')
#define indent1() string((level + 1)*2, ' ')

void InsertStmtNode::prettyPrint(ostream& s, int level) const
{
    s << indent() << "INSERT {" << endl;
    tableName->prettyPrint(s, level + 1);
    insertData->prettyPrint(s, level + 1);
    s << indent() << "}" << endl;
}

void SelectNode::prettyPrint(ostream& s, int level) const
{
    s << indent() << "SELECT {" << endl;
    if (isStar)
        s << indent1() << "*" << endl;
    else {
        for (auto& p : columns) {
            s << indent1() << p.second << " = ";
            p.first->prettyPrint(s, 0);
            s << endl;
        }
    }
    s << indent() << "} FROM {" << endl;
    from->prettyPrint(s, level + 1);
    if (whereCond) {
        s << indent() << "} WHERE " << endl << indent();
        whereCond->prettyPrint(s, level);
        s << endl;
    }
    else
        s << indent() << "}" << endl;
}

void SelectStmtNode::prettyPrint(ostream& s, int level) const
{
    select.prettyPrint(s, level);
}

void TableName::prettyPrint(ostream& s, int level) const
{
    s << indent() << "TableName(" << name << ")" << endl;
}

void ColumnNameExpr::prettyPrint(ostream& s, int level) const
{
    s << "Column(" << name << ")";
}

void ConstExpr::prettyPrint(ostream& s, int level) const
{
    s << v;
}

void FuncExpr::prettyPrint(ostream& s, int level) const
{
    s << name << "(";
    for (unsigned int i = 0; i < children.size(); i++) {
        children[i]->prettyPrint(s, level);
        if (i != children.size() - 1)
            s << ", ";
    }
    s << ")";
}

void AndConditionNode::prettyPrint(ostream& s, int level) const
{
    s << "(";
    for (unsigned int i = 0; i < children.size(); i++) {
        children[i]->prettyPrint(s, level);
        if (i != children.size() - 1)
            s << " AND ";
    }
    s << ")";
}

void OrConditionNode::prettyPrint(ostream& s, int level) const
{
    s << "(";
    for (unsigned int i = 0; i < children.size(); i++) {
        children[i]->prettyPrint(s, level);
        if (i != children.size() - 1)
            s << " OR ";
    }
    s << ")";
}

void CompareConditionNode::prettyPrint(ostream& s, int level) const
{
    left->prettyPrint(s, level);
    switch (condType)
    {
    case CompareType::Equal:
        s << " = ";
        break;
    case CompareType::NotEqual:
        s << " != ";
        break;
    case CompareType::Greater:
        s << " > ";
        break;
    case CompareType::GreaterOrEqual:
        s << " >= ";
        break;
    case CompareType::Less:
        s << " < ";
        break;
    case CompareType::LessOrEqual:
        s << " <= ";
        break;
    default:
        break;
    }
    right->prettyPrint(s, level);
}

void InsertValuesNode::prettyPrint(ostream& s, int level) const
{
    s << indent() << "VALUES {" << endl;
    for (auto const& v : data) {
        s << indent1() << v << endl;
    }
    s << indent() << "}" << endl;
}

void ColumnSpecNode::prettyPrint(ostream& s, int level) const {
    s << indent() << name << " " << *type;
    if (isPrimary)
        s << " PRIMARY KEY";
    s << endl;
}

void CreateTableNode::prettyPrint(ostream& s, int level) const {
    s << indent() << "CREATE TABLE " << name << "{" << endl;
    for (auto const& c : columns) {
        c->prettyPrint(s, level + 1);
    }
    s << "}" << endl;
}