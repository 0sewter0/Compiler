#include "lexer.h"
#include <cctype>

Lexer::Lexer(const std::string& source) : src(source) {}

char Lexer::peek() const {
    if(isAtEnd()) return '\0';
    return src[pos];
}

char Lexer::advance() {
    char c = peek();
    pos++;
    col++;
    if(c == '\n') {
        line++;
        col = 1;
    }
    return c;
}

bool Lexer::isAtEnd() const {
    return pos >= src.size();
}

Token Lexer::makeNumber() {
    size_t Startcol = col;
    std::string numstr;

    while(!isAtEnd() && std::isdigit(peek())) {
        numstr += advance();
    }
    return Token{TokenType::Number, numstr, line, Startcol};
}

Token Lexer::makeIdentiferOrKeyword() {
    size_t Startcol = col;
    std::string text;

    while(!isAtEnd() && (isalnum(peek()) || peek() == '_')) {
        text += advance();
    }

    TokenType type = TokenType::Identifier;
    if(text == "int") type = TokenType::KwInt;
    else if(text == "return") type = TokenType::kwReturn;
    else if(text == "if") type = TokenType::kwIf;
    else if(text == "else") type = TokenType::kwElse;

    return Token{type, text, line, Startcol};
}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;

    while(!isAtEnd()) {
        char c = peek();

        if(std::isspace(c)) {
            advance();
            continue;
        }

        if(std::isdigit(c)) {
            tokens.push_back(makeNumber());
            continue;
        }

        if(std::isalpha(c) || c == '_') {
            tokens.push_back(makeIdentiferOrKeyword());
            continue;
        }

        size_t startCol = col;
        switch(c) {
            case '+': advance(); tokens.push_back({TokenType::Plus, "+", line, startCol}); break;
            case '-': advance(); tokens.push_back({TokenType::Minus, "-", line, startCol}); break;
            case '*': advance(); tokens.push_back({TokenType::Star, "*", line, startCol}); break;
            case '/': advance(); tokens.push_back({TokenType::Star, "/", line, startCol}); break;
            case ';': advance(); tokens.push_back({TokenType::Semicolon, ";", line, startCol}); break;
            case '(': advance(); tokens.push_back({TokenType::LParen, "(", line, startCol}); break;
            case ')': advance(); tokens.push_back({TokenType::RParen, ")", line, startCol}); break;
            case '{': advance(); tokens.push_back({TokenType::LBrace, "{", line, startCol}); break;
            case '}': advance(); tokens.push_back({TokenType::RBrace, "}", line, startCol}); break;
            case '=': advance(); tokens.push_back({TokenType::Assign, "=", line, startCol}); break;
            default:
                advance();
                tokens.push_back({TokenType::Unknown, std::string(1, c), line, startCol});
                break;
        }
    }
    tokens.push_back({TokenType:: Eof, "", line, col});
    return tokens;
}