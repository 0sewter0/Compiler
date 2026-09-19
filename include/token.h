#pragma once

#include <string>
#include <string_view>

enum class TokenType {
    KwInt,
    kwFloat,
    kwIf,
    kwElse,
    kwReturn,
    kwWhile,
    kwStruct,

    Identifier,
    Number,

    Plus,
    Minus,
    Star,
    Slash,
    Caret,
    Assign,
    Semicolon,
    LParen,
    RParen,
    LBrace,
    RBrace,
    LBracket,
    RBracket,
    Comma,
    Point,

    GT, // Greater than
    LT, // Lower than
    GE, // Greater or equal
    LE, // Lower or equal
    Equal,
    NotEqual,

    include,

    Equation,

    Eof,
    Unknown
};

enum Precedence {
    PREC_NONE = 0,
    PREC_ASSIGNMENT = 10,
    PREC_LOGIC = 20,
    PREC_EQUALITY = 30,
    PREC_COMPARISON = 40,
    PREC_TERM = 50,
    PREC_FACTOR = 60,
    PREC_CALL = 70
};

struct Token {
    TokenType type;
    std::string lexeme;
    size_t line;
    size_t col;
};
