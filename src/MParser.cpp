#include "include/ast.h"
#include "include/parser.h"
#include "include/token.h"
#include "include/diagnostic.h"
#include <cerrno>
#include <cstdlib>
#include <limits>

bool isRightAssoc(TokenType type) {
    return type == TokenType::Caret;
} 

int precedenceOf(TokenType type) {
    switch(type) {
        case TokenType::Assign: return 1;

        case TokenType::Equal:
        case TokenType::NotEqual:
        case TokenType::LT:
        case TokenType::GT:
        case TokenType::LE:
        case TokenType::GE: return 2;

        case TokenType::Plus:
        case TokenType::Minus: return 10;
        case TokenType::Star:
        case TokenType::Slash: return 20;
        case TokenType::Caret: return 30;
        
        default: return -1;  
    }
}

bool startsPrimary(TokenType type) {
    return type == TokenType::Number || type == TokenType::Identifier || type == TokenType::LParen;
}

std::unique_ptr<ExprNode> Parser::parsePrimary() {
    TRACE_PARSER;

    if(diagnostic::is_panicking) return std::make_unique<ErrorExprNode>();

    switch(peek().type) {
        case TokenType::Number: {
            Token number = advance();
            std::string lexeme = number.lexeme;

            char* end = nullptr;
            errno = 0;

            if (lexeme.find('.') != std::string::npos) {
                float value = std::strtof(lexeme.c_str(), &end);
                if(errno == ERANGE || end == lexeme.c_str() || *end != '\0') {
                    diagnostic::recordError("Floating-point literal is out of range", number);
                    return std::make_unique<ErrorExprNode>();
                }
                return std::make_unique<FloatExprAST>(value);
            }
            long value = std::strtol(lexeme.c_str(), &end, 10);
            if(errno == ERANGE || end == lexeme.c_str() || *end != '\0' || value < std::numeric_limits<int>::min() || value > std::numeric_limits<int>::max()) {
                diagnostic::recordError("Integer literal is out of range", number);
                return std::make_unique<ErrorExprNode>();
            }
            return std::make_unique<NumberExprAST>(static_cast<int>(value));
        }
        case TokenType::Identifier: {
            std::string name = advance().lexeme;
            std::unique_ptr<ExprNode> expr = std::make_unique<VariableExprAST>(name);

            while(true) {
                if(match(TokenType::Point)) {
                    std::string fieldName = consume(TokenType::Identifier, "Expected field name after '.'").lexeme;
                    expr = std::make_unique<StructAccessAST>(std::move(expr), fieldName);
                } else if(match(TokenType::LBracket)) {
                    auto indexExpr = parseExpr();

                    consume(TokenType::RBracket, "Syntax error: Expected ']'");

                    expr = std::make_unique<ArrayAccessAST>(std::move(expr), std::move(indexExpr));
                } else if(match(TokenType::LParen)) {
                    return parseCallExpr(name);
                } else {
                    break;
                }
            }
            return expr;
        }
        case TokenType::Minus: {
            advance();

            auto operand = parseExpr(25);

            return std::make_unique<UnaryMinusExprAST>('-', std::move(operand));
        }
        case TokenType::LParen: {
            advance();
            auto inner = parseExpr();

            consume(TokenType::RParen, "Syntax error: Expected ')'");
            return inner;
        }
        default: {
            diagnostic::recordError("Expected number, variable or '('", peek());
            diagnostic::is_panicking = true;
            sync();
            return std::make_unique<ErrorExprNode>();
        }
    }
}

std::unique_ptr<ExprNode> Parser::parseExpr(int precedence) {
    TRACE_PARSER;

    if(diagnostic::is_panicking) return std::make_unique<ErrorExprNode>();

    auto lhs = parsePrimary();
    if(!lhs) lhs = std::make_unique<ErrorExprNode>();

    while(true) {
        int prec = precedenceOf(peek().type);
        bool implicitMultiplication = prec < 0 && startsPrimary(peek().type);

        if(implicitMultiplication) {
            prec = precedenceOf(TokenType::Star);
        }

        if(prec < precedence) break;

        Token op = implicitMultiplication ? Token{TokenType::Star, "*", peek().line, peek().col} : advance();

        int nextMinPrec = isRightAssoc(op.type) ? prec : prec + 1;

        std::unique_ptr<ExprNode> rhs = parseExpr(nextMinPrec);
        if(!rhs) rhs = std::make_unique<ErrorExprNode>();

        lhs = std::make_unique<BinaryExprAST>(op.lexeme, std::move(lhs), std::move(rhs));
    }
    return lhs;
}

std::unique_ptr<EquationAST> Parser::parseEquation() {
    diagnostic::is_panicking = false;
    consume(TokenType::Equation, "Expected 'equation'");

    Token targetTok = consume(TokenType::Identifier, "Expected unknown variable after 'equation'");

    std::string target = targetTok.lexeme;

    consume(TokenType::LBrace, "Syntax error: Expected '{' after 'equation'");

    auto left = parseExpr(2);

    consume(TokenType::Assign, "Syntax error: Expected '=' after left side of equation");
    
    auto right = parseExpr(2);

    consume(TokenType::Semicolon, "Syntax error: Expected ';' at the end");

    consume(TokenType::RBrace, "Syntax error: Expected '}'");

    if(diagnostic::is_panicking) return nullptr;

    return std::make_unique<EquationAST>(std::move(left), std::move(right), std::move(target));
}