#include "SqlParser.h"
#include "Common.h"
#include <cctype>
#include <iostream>
#include <set>
#include <regex>

Lexer::Lexer(string source) : source(source), pos(0), posBefore(0) {
    advance();
}

void Lexer::advance() {
    while (pos < source.size() && isspace(source[pos]))
        pos++;
    posBefore = pos;
    if (pos >= source.size()) {
        t = Token(TokenType::EndOfString, pos);
        return;
    }
    char c = source[pos];
    if (isdigit(c))
        lexNumber();
    else if (isalpha(c))
        lexWord();
    else if (c == '"')
        lexString();
#define CHECK(character, type)  \
    else if (c == character) { \
        t = Token(type, string(1, c), pos); \
        pos++; \
    }

    CHECK(',', TokenType::Comma)
    CHECK('(', TokenType::LParen)
    CHECK(')', TokenType::RParen)
    CHECK(';', TokenType::Semicolon)
    CHECK('+', TokenType::Plus)
    CHECK('-', TokenType::Minus)
    CHECK('*', TokenType::Asterisk)
    CHECK('/', TokenType::Slash)
    CHECK('=', TokenType::Equals)
    else if (c == '!' && source[pos + 1] == '=') {
        t = Token(TokenType::NotEquals, "!=", pos);
        pos += 2;
    }
    else if (c == '<') {
        if (source[pos + 1] == '=') {
            t = Token(TokenType::LessOrEquals, "<=", pos);
            pos += 2;
        }
        else {
            t = Token(TokenType::Less, "<", pos);
            pos++;
        }
    }
    else if (c == '>') {
        if (source[pos + 1] == '=') {
            t = Token(TokenType::GreaterOrEquals, ">=", pos);
            pos += 2;
        }
        else {
            t = Token(TokenType::Greater, ">", pos);
            pos++;
        }
    }
    else {
        throw ParserException("Unknown token", pos);
    }
}

void Lexer::lexNumber() {
    size_t index = 0;
    int number = stoi(source.substr(pos), &index);
    t = Token(TokenType::NumberLiteral, source.substr(pos, index), pos, number);
    pos += index;
}

const static set<string> KEYWORDS_SET = {
    "SELECT", "FROM", "INSERT", "INTO", "VALUES", "AS", "NOT",
    "AND", "OR", "WHERE", "CREATE", "TABLE", "PRIMARY", "KEY",
    "NULL"
};

const static set<string> TYPE_SET = {
    "INT", "INTEGER", "BYTE", "TINYINT", "SMALLINT", "VARCHAR"
};

const static set<string> FUNCTION_NAME_SET = {
    "CONCAT"
};

const regex TYPE_REGEX(R"((INTEGER|INT|BYTE|TINYINT|SMALLINT|VARCHAR\([0-9]+\)).*)",
    regex_constants::ECMAScript | regex_constants::icase);

void Lexer::lexWord() {
    int endIndex = pos;
    while (endIndex < source.size() && 
            (isalnum(source[endIndex]) || source[endIndex] == '_'))
        endIndex++;
    string text = source.substr(pos, endIndex - pos);
    makeUpper(text);
    if (TYPE_SET.count(text) != 0) {
        smatch result;
        if (!regex_match(source.cbegin() + pos, source.cend(), result, TYPE_REGEX)) {
            throw ParserException("Invalid type literal", pos);
        }
        text = result.str(1);
        makeUpper(text);
        endIndex = pos + text.length();
        t = Token(TokenType::Type, text, pos);
    }
    else if (KEYWORDS_SET.count(text) != 0)
        t = Token(TokenType::Keyword, text, pos);
    else if (FUNCTION_NAME_SET.count(text) != 0)
        t = Token(TokenType::FunctionName, text, pos);
    else
        t = Token(TokenType::Id, text, pos);
    pos = endIndex;
}

void Lexer::lexString() {
    int endIndex = source.find('"', pos + 1);
    string text;
    if (endIndex == string::npos) {
        throw ParserException("Unclosed string literal", pos);
    }
    
    text = source.substr(pos, endIndex - pos + 1);
    t = Token(TokenType::StringLiteral, text, pos);
    pos = endIndex + 1;
}