#include "tools/calculator.h"

namespace {

using Kind = CalcToken::Kind;

int precedence(Kind op) {
    // Multiplication and division happen before addition and subtraction.
    if (op == Kind::Times || op == Kind::Divide) {
        return 2;
    }

    if (op == Kind::Plus || op == Kind::Minus) {
        return 1;
    }

    return 0;
}

bool apply(std::vector<double>& values, std::vector<Kind>& ops,
           std::string* error) {
    // Check before reading or removing anything from the stacks.
    if (ops.empty() || values.size() < 2) {
        *error = "missing operator or operand";
        return false;
    }

    Kind op = ops.back();
    ops.pop_back();

    // The right operand comes off first.
    double right = values.back();
    values.pop_back();

    double left = values.back();
    values.pop_back();

    double result = 0;

    switch (op) {
        case Kind::Plus:
            result = left + right;
            break;

        case Kind::Minus:
            result = left - right;
            break;

        case Kind::Times:
            result = left * right;
            break;

        case Kind::Divide:
            if (right == 0) {
                *error = "division by zero";
                return false;
            }
            result = left / right;
            break;

        default:
            *error = "invalid operator";
            return false;
    }

    // Put the answer back so another operator can use it.
    values.push_back(result);
    return true;
}

}  // namespace

bool calc_evaluate(const std::vector<CalcToken>& tokens, double* out,
                   std::string* error) {
    std::vector<double> values;
    std::vector<Kind> ops;

    bool expect_operand = true;
    error->clear();

    for (const CalcToken& token : tokens) {
        Kind kind = token.kind;

        if (kind == Kind::Number) {
            if (!expect_operand) {
                *error = "unexpected number";
                return false;
            }

            values.push_back(token.value);
            expect_operand = false;
        } else if (kind == Kind::LParen) {
            if (!expect_operand) {
                *error = "unexpected '('";
                return false;
            }

            ops.push_back(kind);
        } else if (kind == Kind::RParen) {
            if (expect_operand) {
                *error = "missing operand";
                return false;
            }

            // Finish the operations inside these parentheses.
            while (!ops.empty() && ops.back() != Kind::LParen) {
                if (!apply(values, ops, error)) {
                    return false;
                }
            }

            if (ops.empty()) {
                *error = "unmatched ')'";
                return false;
            }

            // Remove the matching opening parenthesis.
            ops.pop_back();
        } else if (kind == Kind::Plus || kind == Kind::Minus ||
                   kind == Kind::Times || kind == Kind::Divide) {
            if (expect_operand) {
                *error = "missing operand";
                return false;
            }

            // Finish operators with higher or equal precedence first.
            while (!ops.empty() && ops.back() != Kind::LParen &&
                   precedence(ops.back()) >= precedence(kind)) {
                if (!apply(values, ops, error)) {
                    return false;
                }
            }

            ops.push_back(kind);
            expect_operand = true;
        } else {
            *error = "invalid token";
            return false;
        }
    }

    if (expect_operand) {
        *error = "missing operand";
        return false;
    }

    // Finish any operators left after the last token.
    while (!ops.empty()) {
        if (ops.back() == Kind::LParen) {
            *error = "unmatched '('";
            return false;
        }

        if (!apply(values, ops, error)) {
            return false;
        }
    }

    // A finished expression should leave exactly one answer.
    if (values.size() != 1) {
        *error = "invalid expression";
        return false;
    }

    *out = values.back();
    return true;
}