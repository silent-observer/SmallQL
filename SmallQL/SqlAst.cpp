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
    for (auto& p : columns) {
        s << indent1() << p.second << " = ";
        p.first->prettyPrint(s, 0);
        s << endl;
    }
    s << indent() << "} FROM {" << endl;
    from->prettyPrint(s, level + 1);
    if (whereCond) {
        s << indent() << "} WHERE {" << endl << indent();
        whereCond->prettyPrint(s, level);
        s << endl;
    }
    if (orderBy.size() > 0) {
        s << indent() << "} ORDER BY {" << endl;
        for (const auto& p : orderBy) {
            s << indent1();
            p.first->prettyPrint(s, level);
            s << (p.second ? " DESC" : " ASC") << endl;
        }
        whereCond->prettyPrint(s, level);
        s << endl;
    }
    
    s << indent() << "}" << endl;
}

void SelectStmtNode::prettyPrint(ostream& s, int level) const
{
    select.prettyPrint(s, level);
}

void TableSubquery::prettyPrint(ostream& s, int level) const
{
    s << indent() << "(";
    query->prettyPrint(s, level + 1);
    s << indent() << ") AS " << alias << endl;
}

void TableName::prettyPrint(ostream& s, int level) const
{
    s << indent() << "TableName(" << name << ")" << endl;
}

void ColumnNameExpr::prettyPrint(ostream& s, int level) const
{
    s << "Column(";
    if (tableName != "")
        s << tableName + ".";
    s << name << ")";
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

void AggrFuncExpr::prettyPrint(ostream& s, int level) const
{
    s << name << "(";
    child->prettyPrint(s, level);
    s << ")";
}

void JoinNode::prettyPrint(ostream& s, int level) const
{
    s << indent();
    switch (joinType)
    {
    case JoinType::Cross:
        s << "CROSS ";
        break;
    case JoinType::Inner:
        s << "INNER ";
        break;
    case JoinType::Left:
        s << "LEFT ";
        break;
    case JoinType::Right:
        s << "RIGHT ";
        break;
    case JoinType::Full:
        s << "FULL ";
        break;
    }
    s << "JOIN {" << endl;
    if (on) {
        s << indent1() << "On: ";
        on->prettyPrint(s, level + 1);
        s << "," << endl;
    }
    left->prettyPrint(s, level + 1);
    s << indent() << "," << endl;
    right->prettyPrint(s, level + 1);
    s << indent() << "}" << endl;
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

void DropTableNode::prettyPrint(ostream& s, int level) const {
    s << indent() << "DROP TABLE " << name << endl;
}