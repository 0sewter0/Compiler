#pragma once
#include "token.h"
#include <vector>
#include <string>

class Lexer {
public:
    explicit Lexer(const std::string& source);

    std::vector<Token> tokenize();

private:
    const std::string& src;
    size_t pos = 0;
    size_t line = 1;
    size_t col = 1;

    char peek() const;
    char advance();
    bool isAtEnd() const;

    Token makeNumber();
    Token makeIdentiferOrKeyword();
};