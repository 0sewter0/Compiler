#pragma once

#include <string>
#include <string_view>

enum class TokenType {
    KwInt,
    kwIf,
    kwElse,
    kwReturn,

    Identifer,
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