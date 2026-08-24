#include "parser.h"
#include <iostream>

Parser::Parser(const std::vector<Token>& tokens) : tokens(tokens) {}

bool Parser::isAtEnd() const {
    if(pos >= tokens.size() || tokens[pos].type == TokenType::Eof) {
        return true;
    } else {
        return false;
    }
}

const Token& Parser::peek() const {
    if(isAtEnd()) {
        return tokens.back();
    }
    return Parser::tokens[pos];
}

Token Parser::advance() {
   if(!isAtEnd()) pos++;
    return tokens[pos - 1];
}

std::unique_ptr<ExprNode> Parser::parsePrimary() {
    const Token& current = peek();

    if(current.type == TokenType::Number) {
        Token num = advance();
        int val = std::stoi(num.lexeme);
        return std::make_unique<NumberExprAST>(val);
    }

    std::cout << "Unknown object at pos: " << pos << "\n";
    return nullptr;
}

std::unique_ptr<ExprNode> Parser::parseTerm() {
    std::unique_ptr<ExprNode> left = parsePrimary();

    while(peek().type == TokenType::Star || peek().type == TokenType::Slash) {
        Token op = advance();
        std::unique_ptr<ExprNode> right = parsePrimary();
        
        left = std::make_unique<BinaryExprAST>(op, std::move(left), std::move(right)); // new node
    }
    return left;
}

std::unique_ptr<ExprNode> Parser::parseExpr() {
    std::unique_ptr<ExprNode> left = parseTerm();

    while(peek().type == TokenType::Plus || peek().type == TokenType::Minus) {
        Token op = advance();
        std::unique_ptr<ExprNode> right = parseTerm();

        char opChar = op.lexeme[0];
        left = std::make_unique<BinaryExprAST>(opChar, std::move(left), std::move(right));
    }
    return left;
}