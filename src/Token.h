#pragma once

#include <assert.h>

#include <concepts>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <utility>
#include <vector>

struct Token {
    enum class NormalType {
        N_CHARACTER,
        N_LPAREN,
        N_RPAREN,
        N_LSET,
        N_RSET,
        N_EOL,
        N_BOL,
        N_OR,
        N_POST_MODIFIER,
        N_BOUNDARY,
        N_RESERV_SET,
        N_TERMINATOR,
    };

    enum class SetType {
        S_CHARACTER,
        S_NEG,
        S_RANGE,
        S_LSET,
        S_RSET,
        S_TERMINATOR,
        S_RESERV_SET,
    };

    NormalType normal_type;
    SetType set_type;

    char base_character;
};

inline std::ostream &operator<<(std::ostream &os, Token const &token) {
    using enum Token::NormalType;
    os << "base char: " << token.base_character << ";";
    os << "normal_type: ";
    switch (token.normal_type) {
    case N_CHARACTER:
        os << "CHARACTER; ";
        break;
    case N_LPAREN:
        os << "LPAREN; ";
        break;
    case N_RPAREN:
        os << "RPAREN; ";
        break;
    case N_LSET:
        os << "LSET; ";
        break;
    case N_RSET:
        os << "RSET; ";
        break;
    case N_EOL:
        os << "EOL; ";
        break;
    case N_BOL:
        os << "BOL; ";
        break;
    case N_OR:
        os << "OR; ";
        break;
    case N_POST_MODIFIER:
        os << "POST_MODIFIER; ";
        break;
    case N_BOUNDARY:
        os << "BOUNDARY; ";
        break;
    case N_RESERV_SET:
        os << "SET; ";
        break;
    case N_TERMINATOR:
        os << "NORMAL_TERMINATOR; ";
        break;
    }
    using enum Token::SetType;
    os << "set_type: ";
    switch (token.set_type) {
    case S_CHARACTER:
        os << "S_CHARACTER";
        break;
    case S_NEG:
        os << "S_NEG";
        break;
    case S_RANGE:
        os << "S_RANGE";
        break;
    case S_TERMINATOR:
        os << "S_TERMINATOR";
        break;
    case S_RESERV_SET:
        os << "S_SET";
        break;
    }
    os << std::endl;
    return os;
}

// it's basically a stack but pop does not actually erase
class TokenStack {
    std::vector<Token> token_list;
    size_t curr_idx;

  public:
    TokenStack() : curr_idx(0) {
    }
    TokenStack(std::vector<Token> token_list)
        : token_list(std::move(token_list)), curr_idx(0) {
    }

    Token peek() const {
        return token_list[curr_idx];
    }

    // if successful, consumes one more on the stack
    template <typename... A>
    requires(std::same_as<A, Token::NormalType> &&...) bool expect(
        A... expected) {
        Token curr_token = peek();
        bool matched_something = std::invoke(
            [curr_token](A const &...expected) {
                return ((curr_token.normal_type == expected) || ...);
            },
            std::forward<A>(expected)...);

        if (matched_something) {
            pop();
        }
        return matched_something;
    }

    template <typename... A>
    requires(std::same_as<A, Token::SetType> &&...) bool expect(A... expected) {
        Token curr_token = peek();
        bool matched_something = std::invoke(
            [curr_token](A const &...expected) {
                return ((curr_token.set_type == expected) || ...);
            },
            std::forward<A>(expected)...);

        if (matched_something) {
            pop();
        }
        return matched_something;
    }

    template <typename... A>
    requires(std::same_as<A, Token::NormalType> &&...) bool except(
        A... expected) {
        Token curr_token = peek();
        bool matched_something = std::invoke(
            [curr_token](A const &...expected) {
                return ((curr_token.normal_type == expected) || ...);
            },
            std::forward<A>(expected)...);

        if (!matched_something) {
            pop();
        }
        return !matched_something;
    }

    Token lookahead() const {
        return token_list[curr_idx + 1];
    }

    Token pop() {
        if (empty()) {
            throw std::runtime_error("popping from empty list.");
        }
        return token_list[curr_idx++];
    }

    void reset_state() {
        curr_idx = 0;
    }

    void push(Token token) {
        token_list.push_back(std::move(token));
    }

    bool empty() const {
        // maybe we should make it so that our constructor always pushes this
        return token_list[curr_idx].normal_type ==
               Token::NormalType::N_TERMINATOR;
    }

    size_t size() const {
        if (empty()) {
            return 0;
        }
        // we should always have a PATTERN_TERMINATOR
        assert(token_list.size() >= 1);
        return token_list.size() - 1 - curr_idx;
    }

    friend std::ostream &operator<<(std::ostream &os, TokenStack const &ts);
};

inline std::ostream &operator<<(std::ostream &os, TokenStack const &ts) {
    for (auto const &tk : ts.token_list) {
        os << "{" << tk << "}";
    }
    return os;
}