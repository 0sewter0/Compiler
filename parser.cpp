#include "parser.h"
#include <iostream>

Parser::Parser(const std::vector<Token>& tokens) : tokens(tokens) {}

bool Parser::match(TokenType type) { //Checks the current Token
    if(peek().type == type) {
        advance();
        return true;
    }
    return false;
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
        if(peek().type == TokenType::LParen) {
            return parseCallExpr(var.lexeme);
        }
        return std::make_unique<VariableExprAST>(var.lexeme);
    } else if(current.type == TokenType::LParen) {
        advance();
        std::unique_ptr<ExprNode> expr = parseExpr();
        consume(TokenType::RParen, "Expected ')' after '('");
        return expr;
    }

    error("Unknown object: '" + peek().lexeme + "' of type " + std::to_string((int)peek().type));
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

std::unique_ptr<VarDecAST> Parser::parseVarDecl() {
    advance();

    Token idToken = consume(TokenType::Identifier, "Expected variable name after 'int'");
    consume(TokenType::Assign, "Expected '=' after variable name");

    std::unique_ptr<ExprNode> initializer = parseExpr();

    consume(TokenType::Semicolon, "Expected ';' at the end of instruction");

    return std::make_unique<VarDecAST>(idToken.lexeme, std::move(initializer));
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
    if(match(TokenType::LBrace)) {
        return parseBlock();
    }

    if(peek().type == TokenType::kwWhile) {
        return parseWhileLoop();
    }

    if(peek().type == TokenType::kwIf) {
        return parseIfStmt();
    }

    if(peek().type == TokenType::KwInt) {
        return parseVarDecl();
    }

    auto expr = parseExpr();
    if(peek().type != TokenType::RBrace) {
        consume(TokenType::Semicolon, "Expected ';' at the end of instruction");
    }
    return expr;
}

std::unique_ptr<ExprNode> Parser::parseCallExpr(std::string name) {
    advance();
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

std::unique_ptr<ASTNode> Parser::parseWhileLoop() {
    match(TokenType::kwWhile);

    consume(TokenType::LParen, "Syntax error: Expected '(' after while");

    auto cond = parseExpr();

    consume(TokenType::RParen, "Syntax error: Expected ')' after condition");

    auto body = parseStatement();

    return std::make_unique<WhileLoopAST>(std::move(cond), std::move(body));
}

std::unique_ptr<ASTNode> Parser::parse() {
    pos = 0;
    std::vector<std::unique_ptr<ASTNode>> statements;

    try {
        while(!isAtEnd()) {
            if(tokens[pos].type == TokenType::KwInt || tokens[pos].type == TokenType::kwIf || tokens[pos].type == TokenType::LBrace || tokens[pos].type == TokenType::kwWhile) {
                statements.push_back(parseStatement());
            }
            else {
                statements.push_back(parseExpr());

                if(tokens[pos].type != TokenType::RBrace) {
                    consume(TokenType::Semicolon, "Expected ';' at the end of instruction");
                }
            }
        }   
    }
    catch(const SyntaxError& e) {
        return nullptr;
    }

    return std::make_unique<ProgramAST>(std::move(statements));
}