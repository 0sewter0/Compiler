#include "parser.h"
#include <iostream>

Parser::Parser(const std::vector<Token>& tokens) : tokens(tokens) {}

bool Parser::match(TokenType type) {
    if(peek().type == type) {
        advance();
        return true;
    }
    return false;
}

Token Parser::consume(TokenType type, const std::string& message) {
    if(peek().type == type) {
        return advance();
    }
    error(message);
    throw SyntaxError(message);
}

bool Parser::isAtEnd() const {
    if(pos >= tokens.size() || tokens[pos].type == TokenType::Eof) {
        return true;
    } else {
        return false;
    }
}

void Parser::error(const std::string& message) {
    const Token& token = peek();
    std::cerr << "[Line " << token.line << ", Col " << token.col << "] Parse error: " << message << "\n";
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
    } else if(current.type == TokenType::Identifier) {
        Token var = advance();
        return std::make_unique<VariableExprAST>(var.lexeme);
    }

    error("Unknown object");
    return nullptr;
}

std::unique_ptr<ExprNode> Parser::parseTerm() {
    std::unique_ptr<ExprNode> left = parsePrimary();

    while(peek().type == TokenType::Star || peek().type == TokenType::Slash) {
        Token op = advance();
        std::unique_ptr<ExprNode> right = parsePrimary();
        
        auto newLeft = std::make_unique<BinaryExprAST>(op.lexeme[0], std::move(left), std::move(right)); // creating new node
        left = std::move(newLeft);
    }
    return left;
}

std::unique_ptr<ExprNode> Parser::parseExpr() {
    std::unique_ptr<ExprNode> left = parseTerm();

    while(peek().type == TokenType::Plus || peek().type == TokenType::Minus) {
        Token op = advance();
        std::unique_ptr<ExprNode> right = parseTerm();

        auto newLeft = std::make_unique<BinaryExprAST>(op.lexeme[0], std::move(left), std::move(right)); // creating new node
        left = std::move(newLeft);
    }
    return left;
}

std::unique_ptr<ASTNode> Parser::parse() {
    try {
        pos = 0;
        if(peek().type == TokenType::KwInt) {
            return parseVarDecl();
        }
        return parseExpr();
    }
    catch(const SyntaxError& e) {
        return nullptr;
    }
}

std::unique_ptr<VarDecAST> Parser::parseVarDecl() {
    advance();

    Token idToken = consume(TokenType::Identifier, "Expected variable name after 'int'");
    consume(TokenType::Assign, "Expected '=' after variable name");

    std::unique_ptr<ExprNode> initializer = parseExpr();

    consume(TokenType::Semicolon, "Expected ';' at the end of instruction");

    return std::make_unique<VarDecAST>(idToken.lexeme, std::move(initializer));
}