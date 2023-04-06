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
        CHARACTER,
        LPAREN,
        RPAREN,
        LSET,
        RSET,
        PLUS,
        DOT,
        EOL,
        BOL,
        OR,
        STAR,
        QUESTION,
        NORMAL_TERMINATOR,
    };

    enum class SetType {
        MEMBER,
        NEG,
        RANGE,
        SET_TERMINATOR,
    };

    NormalType normal_type;
    SetType set_type;

    char base_character;
};

inline std::ostream &operator<<(std::ostream &os, Token const &token) {
    using enum Token::NormalType;
    switch (token.normal_type) {
    case CHARACTER:
        os << "token type: "
           << "CHARACTER"
           << " " << token.base_character;
        break;

    case LPAREN:
        os << "token type: "
           << "LPAREN"
           << " " << token.base_character;
        break;
    case RPAREN:
        os << "token type: "
           << "RPAREN"
           << " " << token.base_character;
        break;
    case PLUS:
        os << "token type: "
           << "PLUS"
           << " " << token.base_character;
        break;
    case DOT:
        os << "token type: "
           << "DOT"
           << " " << token.base_character;
        break;
    case EOL:
        os << "token type: "
           << "EOL"
           << " " << token.base_character;
        break;
    case BOL:
        os << "token type: "
           << "BOL"
           << " " << token.base_character;
        break;
    case OR:
        os << "token type: "
           << "OR"
           << " " << token.base_character;
        break;
    case STAR:
        os << "token type: "
           << "STAR"
           << " " << token.base_character;
        break;
    case NORMAL_TERMINATOR:
        os << "token type: "
           << "PATTERN_TERMINATOR";
        break;
    case LSET:
        os << "token type: "
           << "LSET";
        break;
    case RSET:
        os << "token type: "
           << "RSET";
        break;
    case QUESTION:
        os << "token type: "
           << "QUESTION MARK";
        break;
    }
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
               Token::NormalType::NORMAL_TERMINATOR;
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