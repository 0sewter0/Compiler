#pragma once

#include <string>
#include <string_view>

enum class TokenType {
    KwInt,
    kwIf,
    kwElse,
    kwReturn,

    Identifier,
    Number,

    Plus,
    Minus,
    Star,
    Slash,
    Assign,
    Semicolon,
    LParen,
    RParen,
    LBrace,
    RBrace,
    Comma,

    GT, // Greater than
    LT, // Lower than
    GE, // Greater or equal
    LE, // Lower or equal

    Eof,
    Unknown
};

struct Token {
    TokenType type;
    std::string lexeme;
    size_t line;
    size_t col;
};

std::string_view TokenTypeToString(TokenType type);