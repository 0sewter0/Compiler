#include <iostream>
#include "include/parser.h"
#include "include/diagnostic.h"

Parser::Parser(const std::vector<Token>& tokens) : tokens(tokens) {}

bool Parser::match(TokenType type) { // Checks the current Token
    if(peek().type == type) {
        if(!isAtEnd()) {
            advance();
            return true;
        }
    }
    return false;
}

void Parser::sync() { // Skips tokens until ';', eof or '}'
    while(peek().type != TokenType::Eof && peek().type != TokenType::Semicolon && peek().type != TokenType::RBrace) {
        advance();
    }
    if(peek().type == TokenType::Semicolon) {
        advance();
    }
    diagnostic::is_panicking = false;
}

Token Parser::lookAhead(int n) {
    if(pos + n < static_cast<int>(tokens.size()) && (pos + n) >= 0) {
        return tokens[pos + n];
    }
    if(!tokens.empty()) {
        return tokens.back();
    }
    return Token{TokenType::Eof, ""};
}

Token Parser::consume(TokenType type, const char* message) {
    if(peek().type == type) {
        return advance();
    }

    const Token& found = peek();
    std::string diagnostic = message;
    
    if(found.type == TokenType::Eof) {
        diagnostic += ", reached end of file";
    } else {
        diagnostic += ", got '" + found.lexeme + "'";
    }
    diagnostic::recordError(diagnostic, peek());
    return Token{TokenType::Error, "", found.line, found.col};
}

bool Parser::isAtEnd() const {
    if(pos >= tokens.size()) {
        return true;
    } else {
        return tokens[pos].type == TokenType::Eof;
    }
}

const Token& Parser::peek() const { // returns current token
    if(isAtEnd()) {
        return tokens.back();
    }
    return tokens[pos];
}

Token Parser::advance() {
    if(!isAtEnd()) return tokens[pos++];
    return tokens.empty() ? Token{TokenType::Eof, ""} : tokens.back();
}

std::unique_ptr<IfStmtAST> Parser::parseIfStmt() {
    if(diagnostic::is_panicking) return nullptr;
    advance();
    consume(TokenType::LParen, "Syntax error: Expected '(' after if");

    auto cond = parseExpr();

    consume(TokenType::RParen, "Syntax error: Expected ')' after condition");

    auto ThenBranch = parseStatement();

    std::unique_ptr<ASTNode> elseBrach = nullptr;
    if(match(TokenType::kwElse)) {
        elseBrach = parseStatement();
    }
    if(diagnostic::is_panicking || !ThenBranch) return nullptr;
    return std::make_unique<IfStmtAST>(std::move(cond), std::move(ThenBranch), std::move(elseBrach));
}

std::unique_ptr<ASTNode> Parser::parseStatement() {
    if(peek().type == TokenType::RBrace || isAtEnd()) {
        return nullptr;
    }
    if(peek().type == TokenType::Equation) {
        return parseEquation();
    }
    if(match(TokenType::LBrace)) {
        return parseBlock();
    }

    if(peek().type == TokenType::kwWhile) {
        return parseWhileLoop();
    }

    if(peek().type == TokenType::kwIf) {
        return parseIfStmt();
    }

    if(match(TokenType::kwBreak)) {
        consume(TokenType::Semicolon, "Expected ';' after break");
        return std::make_unique<BreakAST>();
    }

    if(match(TokenType::kwContinue)) {
        consume(TokenType::Semicolon, "Expected ';' after continue");
        return std::make_unique<ContinueAST>();
    }

    if(peek().type == TokenType::kwStruct) {
        if(lookAhead(1).type == TokenType::Identifier && lookAhead(2).type == TokenType::LBrace) {
            return parseStructDecl();
        }
        return parseVarDecl();
    }

    if(peek().type == TokenType::KwInt) {
        return parseVarDecl();
    } else if(peek().type == TokenType::kwReturn) {
        return parseReturnStmt();
    }
    if(peek().type == TokenType::Semicolon) {
        advance();
        return nullptr;
    }

    auto expr = parseExpr();
    if(peek().type != TokenType::RBrace) {
        consume(TokenType::Semicolon, "Expected ';' at the end of instruction");
    }
    return expr;
}

std::unique_ptr<ExprNode> Parser::parseCallExpr(std::string name) {
    if(diagnostic::is_panicking) return std::make_unique<ErrorExprNode>();
    std::vector<std::unique_ptr<ExprNode>> args;

    if(peek().type != TokenType::RParen) {
        while(true) {
            auto argument = parseExpr();

            args.push_back(std::move(argument));
            if(peek().type == TokenType::RParen) break;

            consume(TokenType::Comma, "Syntax error: Expected ',' between arguments");
        }
    }
    consume(TokenType::RParen, "Expected ')' after arguments");

    return std::make_unique<CallExprAST>(name, std::move(args)); 
}

std::unique_ptr<ASTNode> Parser::parseBlock() {
    if(diagnostic::is_panicking) return nullptr;
    std::vector<std::unique_ptr<ASTNode>> stmts;

    while(peek().type != TokenType::RBrace && !isAtEnd()) {
        auto statement = parseStatement();
        if(statement) {
            stmts.push_back(std::move(statement));
        } else {
            sync();
        }
    }

    consume(TokenType::RBrace, "Expected '}' after block");
    
    if(diagnostic::is_panicking) return nullptr;
    return std::make_unique<BlockAST>(std::move(stmts));
}

std::unique_ptr<ASTNode> Parser::parseReturnStmt() {
    TRACE_PARSER;
    advance();

    std::unique_ptr<ASTNode> Expr = nullptr;

    if(peek().type != TokenType::Semicolon) {
        Expr = parseExpr();
    }

    consume(TokenType::Semicolon, "Syntax error: Expected ';' after return value");
    if(diagnostic::is_panicking) return nullptr;
    return std::make_unique<ReturnStmtAST>(std::move(Expr));
}

std::unique_ptr<ASTNode> Parser::parseTopLevel() {
    TRACE_PARSER;
    while(peek().type == TokenType::Semicolon) {
        advance();
    }
    if(isAtEnd()) return nullptr;


    if(peek().type == TokenType::kwStruct) {
        if(lookAhead(1).type == TokenType::Identifier && lookAhead(2).type == TokenType::LBrace) {
            return parseStructDecl();
        }
    }
    if(peek().type == TokenType::KwInt || peek().type == TokenType::kwFloat) {
        if(lookAhead(2).type == TokenType::LParen) {
            return parseDefinition();
        } else if(lookAhead(1).type == TokenType::Identifier) {
            return parseVarDecl();
        }
    }
    if(peek().type == TokenType::Equation) {
        return parseEquation();
    }
    return parseStatement();
}

std::unique_ptr<ASTNode> Parser::parse() {
    pos = 0;
    diagnostic::errors().clear();
    std::vector<std::unique_ptr<ASTNode>> statements;

    while(!isAtEnd() && peek().type != TokenType::Eof) {
        size_t start = pos;
        size_t errorsBefore = diagnostic::errors().size();
        auto node = parseTopLevel();
        if(diagnostic::errors().size() != errorsBefore) {
            if(diagnostic::is_panicking) sync();
        } else if(node) {
            statements.push_back(std::move(node));
        }

        if(pos == start) {
            const Token& unexpected = peek();
            if(diagnostic::errors().size() == errorsBefore) {
                diagnostic::errors().push_back({unexpected.line, unexpected.col, 1, "Unexpected token '" + unexpected.lexeme + "'"});
            }
            advance();
        }
    }

    return std::make_unique<ProgramAST>(std::move(statements));
}