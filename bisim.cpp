#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <list>
#include <stack>
#include <queue>
#include <utility>
#include <tuple>
#include <unordered_set>
#include <unordered_map>

struct Block;

typedef struct State
{
    int id;
    Block *p_block;
    Block *r_block;
    // indices of transitions in m_transition_table that include state
    std::unordered_set<int> in_transition;

    State(int id) : id(id) {}
} State;

typedef struct Block
{
    int id;
    std::unordered_map<int, State *> states;

    Block(int id) : id(id) {}
} Block;

typedef struct Node
{
    std::string name;
    int depth;
    std::vector<Node *> children;

    Node(std::string name, int depth) {
        this->name = name;
        this->depth = depth;
    }
} Node;

void print_tree(Node *node, int depth=0) {
    if (node == nullptr) return;
    std::cout << std::string(depth, '\t') << node->name << std::endl;
    if ((node->children).size()) {
        for (Node *child : node->children) {
            print_tree(child, depth+1);
        }
    }
}

class Automaton
{
    public:
        Automaton() {}

        State *add(Node *tree) {
            // f () -> q
            if ((tree->children).size() == 0) {
                int state_id = m_states.size();
                m_states[state_id] = new State(state_id);
                m_transition_table.push_back(make_tuple(
                    tree->name, std::vector<State *>{}, m_states[state_id]
                ));
                m_states[state_id]->in_transition.insert(m_transition_table.size()-1);
                return m_states[state_id];
            }
            std::vector<State *> q_i;
            for (Node *child : tree->children) {
                q_i.push_back(add(child));
            }
            int state_id = m_states.size();
            m_states[state_id] = new State(state_id);
            m_transition_table.push_back(make_tuple(
                tree->name, q_i, m_states[state_id]
            ));
            m_states[state_id]->in_transition.insert(m_transition_table.size()-1);
            return m_states[state_id];
        }

        void summarize(void) {
            // print current transition rules
            std::cout << "Transitions:" << std::endl;
            for (std::tuple<std::string, std::vector<State *>, State *> t : m_transition_table) {
                std::cout << std::get<0>(t) << " ( ";
                for (State *s : std::get<1>(t)) {
                    std::cout << s->id << " ";
                }
                std::cout << ") -> " << std::get<2>(t)->id << std::endl;
            }
        }

        // f (q_1, ..., q_r) -> q
        std::vector<std::tuple<std::string, std::vector<State *>, State *>> m_transition_table;
        std::unordered_map<int, State *> m_states;
};

class ObsQ
{
    public:
        // L_i(B) = relation(b)
        ObsQ(Automaton *a, std::vector<Block *> *relation, Block *b) {
            // only search for transitions with states included in b
            std::unordered_set<int> visited;
            for (std::pair<const int, State *> &e : b->states) {
                State *s = e.second;
                for (int i : s->in_transition) {
                    if (visited.count(i)) {
                        continue;
                    }
                    std::string f = std::get<0>((a->m_transition_table)[i]);
                    std::vector<State *> q1r = std::get<1>((a->m_transition_table)[i]);
                    State *q = std::get<2>((a->m_transition_table)[i]);

                    if (!m_f_to_countll.count(f)) {
                        m_f_to_countll[f] = new CountLL();
                    }
                    CountLL *ptr = m_f_to_countll[f];

                    for (State *qk : q1r) {
                        if (!(ptr->next_block.count(qk->r_block->id))) {
                            ptr->next_block[qk->r_block->id] = new CountLL();
                        }
                        ptr = ptr->next_block[qk->r_block->id];
                    }

                    if (!(ptr->transitions_into_q.count(q->id))) {
                        ptr->transitions_into_q[q->id] = 0;
                    }
                    ptr->transitions_into_q[q->id]++;

                    visited.insert(i);
                }
            }
        }

        std::unordered_map<int, int> *obs(std::pair<std::string, std::vector<State *>> w) {
            CountLL *ptr = m_f_to_countll[w.first];
            for (State *s : w.second) {
                ptr = ptr->next_block[s->r_block->id];
            }
            return &(ptr->transitions_into_q);
        }

        typedef struct CountLL {
            std::unordered_map<int, CountLL *> next_block;
            std::unordered_map<int, int> transitions_into_q; // (state_id of q, count)
        } CountLL;

        std::unordered_map<std::string, CountLL *> m_f_to_countll;
};

void split(Automaton *a, std::vector<Block *> *r, ObsQ::CountLL *cll_node) {
    for (std::pair<const int, ObsQ::CountLL *> &e : cll_node->next_block) {
        split(a, r, e.second);
    }
    // move all elements from transitions_into_q into new blocks
    std::unordered_map<int, std::vector<Block *>::iterator> visited_blocks;
    for (std::pair<const int, int> &state_id_and_count : cll_node->transitions_into_q) {
        int state_id = state_id_and_count.first;
        int block_id = a->m_states[state_id]->r_block->id;

        if (!visited_blocks.count(block_id)) {
            r->push_back(new Block(r->size()));
            visited_blocks[block_id] = r->end() - 1;
        }

        // detach state from old Block, attach to new
        State *s = a->m_states[state_id];
        s->r_block->states.erase(state_id);
        if (s->r_block->states.empty()) { // all elements now moved out
            r->at(s->r_block->id) = r->back();
            r->erase(r->end() - 1);
        }
        s->r_block = *(visited_blocks[block_id]);
        (*(visited_blocks[block_id]))->states[state_id] = s;
    }
}

void summarize(std::vector<Block *> &relation) {
    for (Block *b : relation) {
        std::cout << "[ ";
        for (std::pair<const int, State *> &e : b->states) {
            std::cout << e.second->id << " ";
        }
        std::cout << "]" << std::endl;
    }
}

// output: (Automaton, block_id -> [element ids in block])
// std::pair<Automaton *, std::unordered_map<int, std::vector<int>>> back_minim(Automaton *a) {
Automaton *back_minim(Automaton *a) {
    std::vector<Block *> p, r;

    // P_0 := QxQ, R_0 := QxQ
    p.push_back(new Block(0));
    r.push_back(new Block(0));
    for (std::pair<const int, State *> &e : a->m_states) {
        State *s = e.second;
        s->p_block = p[0];
        s->r_block = r[0];
        p[0]->states[s->id] = s;
        r[0]->states[s->id] = s;
    }

    // R_0 := R_0 \ split(L_0)
    ObsQ r0_obsq(a, &r, p[0]);
    for (std::pair<const std::string, ObsQ::CountLL *> &e : r0_obsq.m_f_to_countll) {
        split(a, &r, e.second);
    }

    std::cout << "P:" << std::endl;
    summarize(p);
    std::cout << "R:" << std::endl;
    summarize(r);

    return a;
}

std::pair<Automaton *, std::unordered_map<int, std::vector<int>>> forward_minim(Automaton *a) {

}


int main()
{
    Automaton automaton;

    // parse file and add trees to Automaton
    for (const auto &entry : std::filesystem::directory_iterator("other_tests")) {
        std::ifstream pos_file;
        pos_file.open(entry.path());

        std::stack<Node *> stk;

        while (pos_file.good()) {
            std::string line;

            if (std::getline(pos_file, line)) {
                int depth = 0;
                while (line[depth] == '\t')
                    depth++;

                if (stk.empty()) {
                    stk.push(new Node(line.substr(0), 0));
                } else {
                    while (stk.top()->depth >= depth) {
                        stk.pop();
                    }
                    Node *p = new Node(line.substr(depth), depth);
                    stk.top()->children.push_back(p);
                    stk.push(p);
                }
            }
        }
        while (stk.top()->depth > 0)
            stk.pop();

        automaton.add(stk.top());

        // break;
    }

    automaton.summarize();

    back_minim(&automaton);
}
