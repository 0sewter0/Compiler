#include <iostream>
#include "include/parser.h"

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

Token Parser::lookAhead(int n) {
    if(pos + n < static_cast<int>(tokens.size()) && (pos + n) >= 0) {
        return tokens[pos + n];
    }
    if(!tokens.empty()) {
        return tokens.back();
    }
    return Token{TokenType::Eof, ""};
}

Token Parser::consume(TokenType type, const std::string& message) { // Checks the current token AND if its true returns it, else calls error(advance() makes pos++).
    if(peek().type == type) {
        return advance();
    }
    error(message);
    throw SyntaxError(message);
}

Token Parser::GetNextTok() {
    if(pos == tokens.size()) {
        return tokens.back();
    }
    return tokens[pos+1];
}

bool Parser::isAtEnd() const {
    if(pos >= tokens.size()) {
        return true;
    } else {
        return tokens[pos].type == TokenType::Eof;
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
    return tokens[pos];
}

Token Parser::advance() {
    if(!isAtEnd()) return tokens[pos++];
    return tokens.empty() ? Token{TokenType::Eof, ""} : tokens.back();
}

std::unique_ptr<IfStmtAST> Parser::parseIfStmt() {
    advance();
    consume(TokenType::LParen, "Syntax error: Expected '(' after if");

    auto cond = parseExpr();

    consume(TokenType::RParen, "Syntax error: Expected ')' after condition");

    auto ThenBranch = parseStatement();

    std::unique_ptr<ASTNode> elseBrach = nullptr;
    if(match(TokenType::kwElse)) {
        elseBrach = parseStatement();
    }
    return std::make_unique<IfStmtAST>(std::move(cond), std::move(ThenBranch), std::move(elseBrach));
}

std::unique_ptr<ASTNode> Parser::parseStatement() {
    if(peek().type == TokenType::RBrace || isAtEnd()) {
        return nullptr;
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

    if(peek().type == TokenType::kwStruct) {
        if(lookAhead(1).type == TokenType::Identifier && lookAhead(2).type == TokenType::LBrace) {
            return parseStructDecl();
        }
        return parseVarDecl();
    }

    if(peek().type == TokenType::KwInt) {
        if(lookAhead(2).type == TokenType::LParen) {
            return parseDefinition();
        }
        return parseVarDecl();
    } else if(peek().type == TokenType::kwReturn) {
        return parseReturnStmt();
    }

    auto expr = parseExpr();
    if(peek().type != TokenType::RBrace) {
        consume(TokenType::Semicolon, "Expected ';' at the end of instruction");
    }
    return expr;
}

std::unique_ptr<ExprNode> Parser::parseCallExpr(std::string name) {
    std::vector<std::unique_ptr<ExprNode>> args;

    if(peek().type != TokenType::RParen) {
        while(true) {
            args.push_back(parseExpr());
            if(peek().type == TokenType::RParen) break;
            consume(TokenType::Comma, "Syntax error: Expected ',' between arguments");
        }
    }
    consume(TokenType::RParen, "Expected ')' after arguments");

    return std::make_unique<CallExprAST>(name, std::move(args)); 
}

std::unique_ptr<ASTNode> Parser::parseBlock() {
    std::vector<std::unique_ptr<ASTNode>> stmts;

    while(peek().type != TokenType::RBrace && !isAtEnd()) {
        stmts.push_back(parseStatement());
    }

    consume(TokenType::RBrace, "Expected '}' after block");
    
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

        if(peek().type == TokenType::KwInt || peek().type == TokenType::kwFloat) {
            if(lookAhead(1).type == TokenType::Identifier && lookAhead(2).type == TokenType::LParen) {
                return parseDefinition();
            } else if(lookAhead(1).type == TokenType::Identifier) {
                return parseVarDecl();
            } else {
                error("Syntax error: Expected Variable name, function name after 'int'");
                throw std::runtime_error(".");
            }
        }
    }
    return parseStatement();
}

std::unique_ptr<ASTNode> Parser::parse() {
    pos = 0;
    std::vector<std::unique_ptr<ASTNode>> statements;

    try {
        while(!isAtEnd() && peek().type != TokenType::Eof) {
            if(auto node = parseTopLevel()) {
                statements.push_back(std::move(node));
            }
        }   
    }
    catch(const SyntaxError& e) {
        std::cerr << e.what() << std::endl;
        return nullptr;
    }

    return std::make_unique<ProgramAST>(std::move(statements));
}