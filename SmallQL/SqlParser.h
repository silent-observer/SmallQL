#pragma once
#include "Common.h"
#include "SqlAst.h"
#include <string>

using namespace std;

enum class TokenType {
    EndOfString,
    Keyword,
    Type,
    FunctionName,
    Id,
    LParen,
    RParen,
    Comma,
    Equals,
    NotEquals,
    Less,
    LessOrEquals,
    Greater,
    GreaterOrEquals,
    Semicolon,
    Plus,
    Minus,
    Asterisk,
    Slash,
    Dot,
    StringLiteral,
    NumberIntLiteral,
    NumberDoubleLiteral
};

struct Token {
    TokenType type;
    string text;
    int intVal;
    double doubleVal;
    int pos;
    Token(): type(TokenType::EndOfString), text(""), pos(0), intVal(0) {}
    Token(TokenType type, int pos)
        : type(type), text(""), pos(pos), intVal(0), doubleVal(0) {}
    Token(TokenType type, string text, int pos, int intVal = 0, double doubleVal = 0)
        : type(type), text(text), pos(pos), intVal(intVal), doubleVal(doubleVal) {}
    inline bool isKeyword(const string& keyword) const {
        return type == TokenType::Keyword && text == keyword;
    }
    inline bool isEOS() const {
        return type == TokenType::EndOfString;
    }
};

class ParserException : public SQLException
{
private:
    int pos;
    string message;
public:
    ParserException(string cause, int pos)
        : pos(pos)
        , message("Parser Exception at position " + to_string(pos) + ": " + cause) {}
    const char* what() const throw ()
    {
        return message.c_str();
    }
};

class Lexer {
private:
    string source;
    int pos;
    int posBefore;
    Token t;
    void lexNumber();
    void lexWord();
    void lexString();
public:
    Lexer(string source);
    inline const Token& get() const {
        return t;
    };
    void advance();
    inline Token pop() {
        Token copyT = t;
        advance();
        return copyT;
    };
    inline int getPos() const {
        return pos;
    }
    inline int getPosBefore() const {
        return posBefore;
    }

    template <typename T>
    inline T create() {
        T result();
        result.pos = posBefore;
        return result;
    }

    template <typename T>
    inline unique_ptr<T> createPtr() {
        unique_ptr<T> result = make_unique<T>();
        result->pos = posBefore;
        return result;
    }
    inline string sourceSubstr(int startPos) const {
        return source.substr(startPos, posBefore - startPos);
    }
};

class Parser {
private:
    Lexer l;
    unique_ptr<SelectNode> parseSelect();
    unique_ptr<InsertStmtNode> parseInsert();
    unique_ptr<ColumnNameExpr> parseColumnNameExpr();
    unique_ptr<ConstExpr> parseConstExpr();
    unique_ptr<FuncExpr> parseFuncExpr();
    unique_ptr<ExprNode> parseAtomicExpr();
    unique_ptr<ExprNode> parseMultExpr();
    unique_ptr<ExprNode> parseAddExpr();
    unique_ptr<ExprNode> parseExpr();
    unique_ptr<ConditionNode> parseCondition();
    unique_ptr<ConditionNode> parseAndCondition();
    unique_ptr<ConditionNode> parseElementaryCondition();
    unique_ptr<ConditionNode> parseCompareCondition();
    unique_ptr<TableName> parseTableName();
    unique_ptr<InsertDataNode> parseInsertData();
    unique_ptr<InsertValuesNode> parseInsertValues();
    Value parseValue();

    unique_ptr<CreateTableNode> parseCreateTable();
    unique_ptr<ColumnSpecNode> parseColumnSpec();

    inline void check(TokenType type, string errorCause);
    inline void checkKeyword(string keyword, string errorCause);
    inline void checkEOS();
public:
    Parser(string source): l(source) {}
    unique_ptr<StatementNode> parse();
};