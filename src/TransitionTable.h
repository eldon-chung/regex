#pragma once

#include <stddef.h>

#include <algorithm>
#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include <vector>

struct TransitionTable {
    struct State {
        static size_t next_idx;
        size_t state_idx;

        // should we really be doing this?
        State() : state_idx(next_idx++){};
        State(size_t init_idx) : state_idx(init_idx){};
        ~State() = default;

        void swap(State &a, State &b) {
            using std::swap;
            swap(a.state_idx, b.state_idx);
        }

        State(const State &other) : state_idx(other.state_idx) {
        }
        State &operator=(const State &other) {
            if (this != &other) {
                State temp(other);
                swap(*this, temp);
            }
            return *this;
        }
        State(State &&other) : state_idx(other.state_idx) {
        }
        State &operator=(State &&other) {
            if (this != &other) {
                State temp(std::move(other));
                swap(*this, temp);
            }
            return *this;
        }

        bool operator==(State const &other) const {
            return state_idx == other.state_idx;
        }

        struct Hash {
            size_t operator()(const State &k) const {
                return std::hash<size_t>()(k.state_idx);
            }
        };
    };

    // maps each char to a list of states
    struct TransitionRow {
        // 10 is reserved for EOL
        // 2 is reserved for BOL
        std::array<std::vector<State>, 128> row;

        TransitionRow() {
        }

        std::vector<State> &operator[](char index) {
            return row[(unsigned char)index];
        }

        std::vector<State> const &operator[](char index) const {
            return row[(unsigned char)index];
        }

        void add_transition(State const &target, char transition_char) {
            row[(unsigned char)transition_char].push_back(target);
        }

        void add_parallel_transition(State const &target,
                                     std::vector<State> const &new_targets) {
            // if for a char it contains target, we append new_targets to it
            for (size_t idx = 0; idx < 127; ++idx) {
                if (auto it =
                        std::find(row[idx].begin(), row[idx].end(), target);
                    it != row[idx].end()) {
                    row[idx].insert(row[idx].end(), new_targets.begin(),
                                    new_targets.end());
                }
            }
        }
    };

    std::unordered_map<State, TransitionRow, State::Hash> table;
    std::vector<State> starting_states;
    std::vector<State> accepting_states;

    friend void swap(TransitionTable &a, TransitionTable &b) {
        using std::swap;
        swap(a.table, b.table);
        swap(a.starting_states, b.starting_states);
        swap(a.accepting_states, b.accepting_states);
    }

    // empty string is accepting, anything else goes into rejection
    TransitionTable()
        : starting_states({State()}),
          accepting_states({starting_states.front()}) {
        // create the trivial row
        table.insert({starting_states.front(), TransitionRow()});
    }

    TransitionTable(TransitionTable const &other)
        : starting_states(other.starting_states),
          accepting_states(other.accepting_states) {

        // map the old states to the new states
        std::unordered_map<State, State, State::Hash> old_to_new_states;
        for (auto const &state_pair : other.table) {
            old_to_new_states.insert({state_pair.first, State()});
        }

        auto in_place_replace = [&](State &s) { s = old_to_new_states.at(s); };

        // now we replace everything in our data
        std::for_each(starting_states.begin(), starting_states.end(),
                      in_place_replace);

        std::for_each(accepting_states.begin(), accepting_states.end(),
                      in_place_replace);

        for (auto const &row_pair : other.table) {

            auto const &new_state = old_to_new_states.at(row_pair.first);

            table.insert({new_state, row_pair.second});
            for (int16_t idx = 0; idx < 127; ++idx) {
                std::for_each(table.at(new_state)[(char)idx].begin(),
                              table.at(new_state)[(char)idx].end(),
                              in_place_replace);
            }
        }
    }

    TransitionTable(TransitionTable &&other)
        : table(std::move(other.table)),
          starting_states(std::move(other.starting_states)),
          accepting_states(std::move(other.accepting_states)) {
    }

    TransitionTable &operator=(TransitionTable const &other) {
        if (this != &other) {
            TransitionTable temp{other};

            // using std::swap;
            swap(*this, temp);
        }
        return *this;
    }

    TransitionTable &operator=(TransitionTable &&other) {
        if (this != &other) {
            TransitionTable temp{std::move(other)};
            using std::swap;
            swap(*this, temp);
        }
        return *this;
    }

    bool
    is_accepting(std::unordered_set<State, State::Hash> const &set_of_states) {
        return std::any_of(set_of_states.begin(), set_of_states.end(),
                           [this](State const &s) { return is_accepting(s); });
    }

    bool is_accepting(State const &s) const {
        auto it =
            std::find(accepting_states.begin(), accepting_states.end(), s);
        return it != accepting_states.end();
    }

    std::vector<State> const &get_transition(State curr_state, char c) const {
        return table.at(curr_state)[c];
    }
};
inline constinit size_t TransitionTable::State::next_idx = 0;

inline std::ostream &operator<<(std::ostream &os,
                                TransitionTable::State const &s) {
    os << "{" << s.state_idx << "}";
    return os;
}

inline std::ostream &operator<<(std::ostream &os,
                                TransitionTable::TransitionRow const &tr) {
    for (int16_t c = 0; c < 127; ++c) {
        if (tr[(char)c].empty()) {
            continue;
        }

        os << "char " << c << ": ";
        os << "[";
        std::for_each(tr[(char)c].begin(), tr[(char)c].end(),
                      [&os](TransitionTable::State const &s) { os << s; });
        os << "]" << std::endl;
    }
    return os;
}

inline std::ostream &operator<<(std::ostream &os, TransitionTable const &tb) {
    os << "starting states:" << std::endl;
    for (auto const &s : tb.starting_states) {
        os << s << std::endl;
    }
    os << "============================" << std::endl;

    os << "accepting states:" << std::endl;
    for (auto const &s : tb.accepting_states) {
        os << s << std::endl;
    }
    os << "============================" << std::endl;
    os << "table:" << std::endl;
    for (auto const &row : tb.table) {
        os << "============================" << std::endl;
        os << "state: ";
        os << row.first << std::endl;
        os << "row: ";
        os << row.second << std::endl;
        os << "============================" << std::endl;
    }
    return os;
}