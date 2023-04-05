#pragma once

#include "TransitionTable.h"
#include "parser.h"

#include <algorithm>
#include <array>
#include <list>
#include <stdexcept>
#include <string_view>
#include <unordered_set>
#include <vector>

struct Result {
    size_t offset;
    size_t length;
};

class Matcher {
    struct State {
        std::unordered_set<TransitionTable::State, TransitionTable::State::Hash>
            fa_states;
        size_t starting_offset;
        size_t ending_offset;
    };

    struct Result {
        size_t starting_offset;
        size_t ending_offset;
    };

    TransitionTable table;
    std::vector<State> active_matches;

  public:
    Matcher(std::string_view pattern, bool reverse = false) {
        // should we really be validating the token stack in the matcher?
        auto token_stack = tokenize(pattern);
        if (!token_stack) {
            throw std::runtime_error("invalid tokenization");
        }

        if (!validate(*token_stack)) {
            throw std::runtime_error("invalid regex pattern");
        }
        (*token_stack).reset_state();
        table = compile(*token_stack, reverse);
    }

    std::vector<Result> match(std::string_view str) {
        std::vector<Result> to_return;

        std::vector<State> next_active_states;

        // used to merge sets
        std::unordered_set<TransitionTable::State, TransitionTable::State::Hash>
            temp_union;

        for (size_t str_idx = 0; str_idx < str.length(); ++str_idx) {

            // add a new active_match here
            active_matches.push_back(
                {{table.starting_states.begin(), table.starting_states.end()},
                 str_idx,
                 str_idx});

            // for each active match
            for (auto &ac_st : active_matches) {
                // for each active match
                temp_union.clear();

                // go through all the possible states of that active match
                for (auto const &fa_state : ac_st.fa_states) {
                    // get get all the possible next states
                    auto const &next_states =
                        table.get_transition(fa_state, str[str_idx]);
                    std::for_each(next_states.begin(), next_states.end(),
                                  [&temp_union](auto const &next_state) {
                                      temp_union.insert(next_state);
                                  });
                }

                if (!temp_union.empty()) {
                    // update the ending idx
                    ac_st.ending_offset = str_idx + 1;
                    // overwrite the state list
                    ac_st.fa_states = temp_union;
                    // see if any are matching

                    if (table.is_accepting(ac_st.fa_states)) {
                        to_return.push_back(
                            {ac_st.starting_offset, ac_st.ending_offset});
                    }
                    // move this into next_active_state;
                    next_active_states.push_back(std::move(ac_st));
                }
            }
            active_matches = std::move(next_active_states);
            next_active_states.clear();
        }

        return to_return;
    }

    friend std::ostream &operator<<(std::ostream &os, Matcher const &matcher);
};

inline std::ostream &operator<<(std::ostream &os, Matcher const &matcher) {
    os << matcher.table << std::endl;
    return os;
}