#pragma once

#include "Token.h"
#include "TransitionTable.h"

#include <optional>
#include <string_view>
#include <vector>

std::optional<TokenStack> tokenize(std::string_view regex_string);
bool validate(TokenStack &token_list);
TransitionTable compile(TokenStack &token_stack, bool reverse);

class TableBuilder {
    TransitionTable built_table;

  public:
    TableBuilder() : built_table() {
    }

    TransitionTable const &operator*() const {
        return built_table;
    }

    TransitionTable &operator*() {
        return built_table;
    }

    TransitionTable &get_table() {
        return built_table;
    }

    void operator+=(TableBuilder const &other) {
        // special case, if there is only a single state
        if (built_table.table.size() == 1) {
            // we just copy the other table
            // because we're the "empty" pattern
            *this = other;
            return;
        }

        // each accepting state in this is now
        // starting in the other
        for (auto const &acc_state : built_table.accepting_states) {
            for (auto &row_pair : built_table.table) {
                row_pair.second.add_parallel_transition(
                    acc_state, (*other).starting_states);
            }
        }

        // append the tables
        built_table.table.insert((*other).table.begin(), (*other).table.end());

        // clear the accepting set
        built_table.accepting_states.clear();

        // take the new accepting states
        built_table.accepting_states = (*other).accepting_states;
    }

    void operator|=(TableBuilder const &other) {
        // append the starting states
        built_table.starting_states.insert(built_table.starting_states.end(),
                                           (*other).starting_states.begin(),
                                           (*other).starting_states.end());

        // append the accepting states
        built_table.accepting_states.insert(built_table.accepting_states.end(),
                                            (*other).accepting_states.begin(),
                                            (*other).accepting_states.end());

        // append the transition table
        built_table.table.insert((*other).table.begin(), (*other).table.end());
    }

    void star_modify() {
        // you need to transition back to the start
        for (auto const &acc_state : built_table.accepting_states) {
            for (auto &row_pair : built_table.table) {
                row_pair.second.add_parallel_transition(
                    acc_state, built_table.starting_states);
            }
        }

        // set starting states as accepting states
        built_table.accepting_states.insert(built_table.accepting_states.end(),
                                            built_table.starting_states.begin(),
                                            built_table.starting_states.end());
    }

    void plus_modify() {
        // make a copy
        TableBuilder temp = *this;
        temp.star_modify();
        *this += temp;
    }

    void bol_modify() {
        TableBuilder temp;
        // add the bol char = 2
        temp.add_char(2);
        temp += *this;
        // now swap this out for temp
        *this = std::move(temp);
    }

    void eol_modify() {
        TableBuilder temp;
        // add the bol char = 2
        temp.add_char(10);
        *this += temp;
    }

    void question_modify() {
        // set starting states as accepting states
        built_table.accepting_states.insert(built_table.accepting_states.end(),
                                            built_table.starting_states.begin(),
                                            built_table.starting_states.end());
    }

    void add_char(char c) {
        // appends a char
        TransitionTable::State new_acc_state;

        // add the transitions
        for (auto const &acc : built_table.accepting_states) {
            // add a transition to the new state
            built_table.table.at(acc).add_transition(new_acc_state, c);
        }

        // set the new accepting state
        built_table.accepting_states.clear();
        built_table.accepting_states.push_back(new_acc_state);

        // add a new transition row for the new state;
        // TODO: see where else we need this
        built_table.table.insert({new_acc_state, {}});
    }

    void add_star_char(char c) {
        TableBuilder char_table;
        char_table.add_char(c);
        char_table.star_modify();
        *this += char_table;
    }

    void add_plus_char(char c) {
        TableBuilder char_table;
        char_table.add_char(c);
        char_table.plus_modify();
        *this += char_table;
    }

    void add_question_char(char c) {
        TableBuilder char_table;
        char_table.add_char(c);
        char_table.question_modify();
        *this += char_table;
    }

    void add_char_set_mode(std::vector<char> const &char_set) {
        TableBuilder accum;
        // it only has one starting state
        auto &accum_starting_state = (*accum).starting_states.front();
        TransitionTable::State new_acc;
        for (char c : char_set) {
            (*accum).table.at(accum_starting_state).add_transition(new_acc, c);
        }
        (*accum).table.insert({new_acc, {}});
        (*accum).accepting_states.clear();
        (*accum).accepting_states.push_back(new_acc);

        *this += accum;
    }

    void add_dot_char() {
        // use add char set?
        std::vector<char> char_dot_set;
        for (int16_t c = 33; c < 127; ++c) {
            char_dot_set.push_back((char)c);
        }

        add_char_set_mode(char_dot_set);
    }

    void add_dot_star() {
        TableBuilder tb;
        tb.add_dot_char();
        tb.star_modify();
        *this += tb;
    }

    void add_dot_plus() {
        TableBuilder tb;
        tb.add_dot_char();
        tb.plus_modify();
        *this += tb;
    }

    void add_dot_question() {
        TableBuilder tb;
        tb.add_dot_char();
        tb.question_modify();
        *this += tb;
    }

    void add_char_neg_set_mode(std::vector<char> const &char_set) {
        std::vector<char> to_insert;
        for (int16_t c = 32; c < 127; ++c) {
            if (auto it = std::find(char_set.begin(), char_set.end(), c);
                it != char_set.end()) {
                continue;
            }
            to_insert.push_back((char)c);
        }
        add_char_set_mode(to_insert);
    }

    void reverse_table() {
        std::unordered_map<TransitionTable::State,
                           TransitionTable::TransitionRow,
                           TransitionTable::State::Hash>
            new_table;
        for (auto const &state_row : built_table.table) {
            new_table.insert({state_row.first, {}});
        }

        for (auto const &state_row : built_table.table) {
            for (size_t idx = 0; idx < 127; ++idx) {
                for (auto const &s : state_row.second[(char)idx]) {
                    new_table.at(s).add_transition(state_row.first, (char)idx);
                }
            }
        }

        built_table.table = std::move(new_table);
        // swap the starting and ending tables
        built_table.starting_states.swap(built_table.accepting_states);
    }

    void shrink_to_fit() {
        for (auto &transition_row_pair : built_table.table) {
            TransitionTable::TransitionRow &transition_row =
                transition_row_pair.second;
            for (size_t idx = 0; idx < 128; ++idx) {
                transition_row[(char)idx].shrink_to_fit();
            }
        }
    }
};