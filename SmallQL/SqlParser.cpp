#include "SqlParser.h"
#include "Common.h"

unique_ptr<StatementNode> Parser::parse() {
    if (l.get().type != TokenType::Keyword)
        throw ParserException("Expected statement keyword", l.getPos());
    if (l.get().text == "SELECT") {
        auto result = l.createPtr<SelectStmtNode>();
        result->select = move(*parseSelect());
        return result;
    }
    else if (l.get().text == "INSERT") {
        return parseInsert();
    }
    else if (l.get().text == "CREATE") {
        l.advance();
        if (l.get().isKeyword("TABLE"))
            return parseCreateTable();
        throw ParserException("Unknown CREATE command", l.getPos());
    }
    else
        throw ParserException("Unknown command", l.getPos());
}

inline void Parser::check(TokenType type, string errorCause) {
    if (l.get().type != type)
        throw ParserException(errorCause, l.getPos());
}

inline void Parser::checkKeyword(string keyword, string errorCause) {
    if (!l.get().isKeyword(keyword))
        throw ParserException(errorCause, l.getPos());
}

inline void Parser::checkEOS() {
    if (l.get().isEOS())
        throw ParserException("Unexpected end of string", l.getPos());
}

unique_ptr<SelectNode> Parser::parseSelect() {
    auto result = l.createPtr<SelectNode>();
    l.advance();
    if (l.get().type == TokenType::Asterisk) {
        result->isStar = true;
        l.advance();
    }
    else result->isStar = false;
    checkEOS();
    if (!l.get().isKeyword("FROM")) {
        bool isFirst = true;
        do {
            if (!isFirst)
                l.advance();
            int posBefore = l.getPosBefore();
            auto expr = parseExpr();
            string alias = l.sourceSubstr(posBefore);
            makeUpper(alias);
            if (l.get().isKeyword("AS")) {
                l.advance();
                check(TokenType::Id, "Expected alias");
                alias = l.pop().text;
            }
            result->columns.push_back(make_pair(move(expr), alias));
            isFirst = false;
        } while (l.get().type == TokenType::Comma);
    }
    checkKeyword("FROM", "Expected FROM");
    l.advance();
    result->from = parseJoin();
    if (l.get().isKeyword("WHERE")) {
        l.advance();
        result->whereCond = parseCondition();
    }
    if (l.get().isKeyword("ORDER")) {
        l.advance();
        checkKeyword("BY", "Expected BY");
        l.advance();
        bool isFirst = true;
        while (isFirst || l.get().type == TokenType::Comma) {
            if (!isFirst) {
                l.advance();
            }
            isFirst = false;
            auto expr = parseExpr();
            bool isDesc = false;
            if (l.get().isKeyword("ASC")) {
                l.advance();
            }
            else if (l.get().isKeyword("DESC")) {
                l.advance();
                isDesc = true;
            }
            result->orderBy.push_back(make_pair(move(expr), isDesc));
        }
    }

    check(TokenType::Semicolon, "Expected semicolon");
    return result;
}

unique_ptr<InsertStmtNode> Parser::parseInsert() {
    auto result = l.createPtr<InsertStmtNode>();
    l.advance();
    checkKeyword("INTO", "Expected INTO");
    l.advance();
    result->tableName = parseTableName();
    result->insertData = parseInsertData();
    check(TokenType::Semicolon, "Expected semicolon");
    return result;
}

unique_ptr<InsertDataNode> Parser::parseInsertData() {
    return parseInsertValues();
}

unique_ptr<InsertValuesNode> Parser::parseInsertValues() {
    auto result = l.createPtr<InsertValuesNode>();
    checkKeyword("VALUES", "Expected VALUES");
    l.advance();
    bool isFirstArr = true;
    do {
        if (!isFirstArr)
            l.advance();
        check(TokenType::LParen, "Expected left parenthesis");
        l.advance();
        result->data.emplace_back();
        bool isFirst = true;
        do {
            if (!isFirst)
                l.advance();
            result->data.back().push_back(parseValue());
            isFirst = false;
        } while (l.get().type == TokenType::Comma);
        check(TokenType::RParen, "Expected right parenthesis");
        l.advance();
        isFirstArr = false;
    } while (l.get().type == TokenType::Comma);
    
    
    
    return result;
}

Value Parser::parseValue() {
    if (l.get().type == TokenType::NumberIntLiteral) {
        return Value((int64_t)l.pop().intVal);
    }
    else if (l.get().type == TokenType::NumberDoubleLiteral) {
        return Value(l.pop().doubleVal);
    }
    else if (l.get().type == TokenType::StringLiteral) {
        string s = l.pop().text;
        return Value(s.substr(1, s.size() - 2));
    }
    else
        throw ParserException("Expected value", l.getPos());
}

unique_ptr<TableName> Parser::parseTableName() {
    check(TokenType::Id, "Expected table name");
    auto result = l.createPtr<TableName>();
    result->name = l.pop().text;
    if (l.get().isKeyword("AS")) {
        l.advance();
        check(TokenType::Id, "Expected table alias");
        result->alias = l.pop().text;
    }
    else
        result->alias = "";
    return result;
}

unique_ptr<TableExpr> Parser::parseJoin() {
    unique_ptr<TableExpr> result = parseTableName();
    while (l.get().type == TokenType::Comma || 
            l.get().isKeyword("CROSS") ||
            l.get().isKeyword("INNER") ||
            l.get().isKeyword("LEFT") ||
            l.get().isKeyword("RIGHT") ||
            l.get().isKeyword("FULL") ||
            l.get().isKeyword("JOIN")) {
        auto join = l.createPtr<JoinNode>();
        if (l.get().type == TokenType::Comma) {
            join->joinType = JoinType::Cross;
            l.advance();
        } 
        else if (l.get().text == "JOIN") {
            join->joinType = JoinType::Inner;
            l.advance();
        }
        else if (l.get().text == "CROSS" ||
                 l.get().text == "INNER" ||
                 l.get().text == "LEFT" ||
                 l.get().text == "RIGHT" ||
                 l.get().text == "FULL"){
            if (l.get().text == "CROSS")
                join->joinType = JoinType::Cross;
            else if (l.get().text == "INNER")
                join->joinType = JoinType::Inner;
            else if (l.get().text == "LEFT")
                join->joinType = JoinType::Left;
            else if (l.get().text == "RIGHT")
                join->joinType = JoinType::Right;
            else if (l.get().text == "FULL")
                join->joinType = JoinType::Full;
            l.advance();
            checkKeyword("JOIN", "Expected JOIN");
            l.advance();
        }

        join->left = move(result);
        join->right = parseTableName();

        if (join->joinType == JoinType::Cross)
            join->on = nullptr;
        else {
            checkKeyword("ON", "Expected ON");
            l.advance();
            join->on = parseCondition();
        }

        result = move(join);
    }
    return result;
}

unique_ptr<ColumnNameExpr> Parser::parseColumnNameExpr() {
    check(TokenType::Id, "Expected column name");
    auto result = l.createPtr<ColumnNameExpr>();
    result->name = l.pop().text;
    result->tableName = "";
    if (l.get().type == TokenType::Dot) {
        l.advance();
        result->tableName = result->name;
        check(TokenType::Id, "Expected column name");
        result->name = l.pop().text;
    }
    return result;
}

unique_ptr<ConstExpr> Parser::parseConstExpr() {
    auto result = l.createPtr<ConstExpr>();
    result->v = parseValue();
    return result;
}

unique_ptr<FuncExpr> Parser::parseFuncExpr() {
    check(TokenType::FunctionName, "Expected function name");
    auto result = l.createPtr<FuncExpr>();
    result->name = l.pop().text;

    check(TokenType::LParen, "Expected left parenthesis");
    l.advance();
    bool isFirst = true;
    do {
        if (!isFirst)
            l.advance();
        result->children.push_back(parseExpr());
        isFirst = false;
    } while (l.get().type == TokenType::Comma);
    check(TokenType::RParen, "Expected right parenthesis");
    l.advance();

    return result;
}

unique_ptr<ExprNode> Parser::parseAtomicExpr() {
    if (l.get().type == TokenType::LParen) {
        l.advance();
        auto result = parseExpr();
        check(TokenType::RParen, "Expected right parenthesis");
        l.advance();
        return result;
    }
    else if (l.get().type == TokenType::Id) {
        return parseColumnNameExpr();
    }
    else if (l.get().type == TokenType::FunctionName) {
        return parseFuncExpr();
    }
    else
        return parseConstExpr();
}

unique_ptr<ExprNode> Parser::parseMultExpr() {
    auto result = parseAtomicExpr();
    while (l.get().type == TokenType::Asterisk || l.get().type == TokenType::Slash) {
        auto n = l.createPtr<FuncExpr>();
        n->name = l.get().type == TokenType::Asterisk ? "*" : "/";
        n->children.push_back(move(result));
        auto currentType = l.get().type;
        while (l.get().type == currentType) {
            l.advance();
            n->children.push_back(parseAtomicExpr());
        }
        result = move(n);
    }
    return result;
}

unique_ptr<ExprNode> Parser::parseAddExpr() {
    auto result = parseMultExpr();
    while (l.get().type == TokenType::Plus || l.get().type == TokenType::Minus) {
        auto n = l.createPtr<FuncExpr>();
        n->name = l.get().type == TokenType::Plus ? "+" : "-";
        n->children.push_back(move(result));
        auto currentType = l.get().type;
        while (l.get().type == currentType) {
            l.advance();
            n->children.push_back(parseMultExpr());
        }
        result = move(n);
    }
    return result;
}

unique_ptr<ExprNode> Parser::parseExpr() {
    return parseAddExpr();
}

unique_ptr<ConditionNode> Parser::parseCondition() {
    auto result = parseAndCondition();
    if (l.get().isKeyword("OR")) {
        auto newResult = l.createPtr<OrConditionNode>();
        newResult->children.push_back(move(result));
        while (l.get().isKeyword("OR")) {
            l.advance();
            newResult->children.push_back(parseAndCondition());
        }
        result = move(newResult);
    }
    return result;
}

unique_ptr<ConditionNode> Parser::parseAndCondition() {
    auto result = parseElementaryCondition();
    if (l.get().isKeyword("AND")) {
        auto newResult = l.createPtr<AndConditionNode>();
        newResult->children.push_back(move(result));
        while (l.get().isKeyword("AND")) {
            l.advance();
            newResult->children.push_back(parseElementaryCondition());
        }
        result = move(newResult);
    }
    return result;
}

unique_ptr<ConditionNode> Parser::parseElementaryCondition() {
    if (l.get().type == TokenType::LParen) {
        l.advance();
        auto result = parseCondition();
        check(TokenType::RParen, "Expected right parenthesis");
        l.advance();
        return result;
    }
    else
        return parseCompareCondition();
}

unique_ptr<ConditionNode> Parser::parseCompareCondition() {
    auto result = l.createPtr<CompareConditionNode>();
    result->left = parseExpr();
    switch (l.get().type)
    {
    case TokenType::Equals:
        result->condType = CompareType::Equal;
        break;
    case TokenType::NotEquals:
        result->condType = CompareType::NotEqual;
        break;
    case TokenType::Less:
        result->condType = CompareType::Less;
        break;
    case TokenType::Greater:
        result->condType = CompareType::Greater;
        break;
    case TokenType::LessOrEquals:
        result->condType = CompareType::LessOrEqual;
        break;
    case TokenType::GreaterOrEquals:
        result->condType = CompareType::GreaterOrEqual;
        break;
    default:
        throw ParserException("Expected comparison operator", l.getPos());
    }
    l.advance();
    result->right = parseExpr();
    return result;
}

unique_ptr<CreateTableNode> Parser::parseCreateTable() {
    auto result = l.createPtr<CreateTableNode>();
    l.advance();
    check(TokenType::Id, "Expected table name");
    result->name = l.pop().text;
    check(TokenType::LParen, "Expected left paren");
    l.advance();
    while (l.get().type == TokenType::Id) {
        result->columns.push_back(parseColumnSpec());
        if (l.get().type != TokenType::RParen) {
            check(TokenType::Comma, "Expected comma");
            l.advance();
        }
    }
    check(TokenType::RParen, "Expected right paren");
    l.advance();
    check(TokenType::Semicolon, "Expected semicolon");
    l.advance();
    return result;
}

unique_ptr<ColumnSpecNode> Parser::parseColumnSpec() {
    auto result = l.createPtr<ColumnSpecNode>();
    check(TokenType::Id, "Expected column name");
    result->name = l.pop().text;
    check(TokenType::Type, "Expected column type");
    result->typeStr = l.pop().text;
    result->type = parseDataType(result->typeStr);

    result->isPrimary = false;
    result->canBeNull = true;

    while (l.get().type == TokenType::Keyword && (l.get().text == "PRIMARY" || l.get().text == "NOT")) {
        if (l.get().isKeyword("PRIMARY")) {
            l.advance();
            checkKeyword("KEY", "Expected KEY");
            l.advance();
            result->isPrimary = true;
            result->canBeNull = false;
        }
        else if (l.get().isKeyword("NOT")) {
            l.advance();
            checkKeyword("NULL", "Expected NULL");
            l.advance();
            result->canBeNull = false;
        }
    }
    
    return result;
}