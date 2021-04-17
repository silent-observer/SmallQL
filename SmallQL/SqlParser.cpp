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
        l.pop();
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
    result->from = parseTableName();
    if (l.get().isKeyword("WHERE")) {
        l.advance();
        result->whereCond = parseCondition();
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
    if (l.get().type == TokenType::NumberLiteral) {
        return Value(l.pop().intVal);
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
    return result;
}

unique_ptr<ColumnNameExpr> Parser::parseColumnNameExpr() {
    check(TokenType::Id, "Expected column name");
    auto result = l.createPtr<ColumnNameExpr>();
    result->name = l.pop().text;
    return result;
}

unique_ptr<ConstExpr> Parser::parseConstExpr() {
    auto result = l.createPtr<ConstExpr>();
    result->v = parseValue();
    return result;
}

unique_ptr<ExprNode> Parser::parseExpr() {
    if (l.get().type == TokenType::Id) {
        return parseColumnNameExpr();
    }
    else
        return parseConstExpr();
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
    l.pop();
    while (l.get().type == TokenType::Id) {
        result->columns.push_back(parseColumnSpec());
        if (l.get().type != TokenType::RParen) {
            check(TokenType::Comma, "Expected comma");
            l.pop();
        }
    }
    check(TokenType::RParen, "Expected right paren");
    l.pop();
    check(TokenType::Semicolon, "Expected semicolon");
    l.pop();
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
            l.pop();
            checkKeyword("KEY", "Expected KEY");
            l.pop();
            result->isPrimary = true;
            result->canBeNull = false;
        }
        else if (l.get().isKeyword("NOT")) {
            l.pop();
            checkKeyword("NULL", "Expected NULL");
            l.pop();
            result->canBeNull = false;
        }
    }
    
    return result;
}