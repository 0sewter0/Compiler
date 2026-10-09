#pragma once
#include <vector>
#include "token.h"

struct Error {
    size_t line;
    size_t col;
    int exitCode;
    std::string message;
};

class diagnostic {
private:
    static std::vector<Error> errors_;
public:
    static std::vector<Error>* getErrors();
    static void recordError(const std::string& message, const Token &t);
    static void printAllErrors();
    static const std::vector<Error>& errors() { return errors_; }

    static bool is_panicking;
};