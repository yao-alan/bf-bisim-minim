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
#include <numeric>

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
    int size(void) { return states.size(); }
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
                // m_states[state_id]->in_transition.insert(m_transition_table.size()-1);
                return m_states[state_id];
            }
            std::vector<State *> q_i;
            std::vector<State *> child_states;
            for (Node *child : tree->children) {
                child_states.push_back(add(child));
                q_i.push_back(child_states.back());
            }
            int state_id = m_states.size();
            m_states[state_id] = new State(state_id);
            m_transition_table.push_back(make_tuple(
                tree->name, q_i, m_states[state_id]
            ));
            for (State *cs : child_states) {
                cs->in_transition.insert(m_transition_table.size()-1);
            }
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
        ObsQ(Automaton *a, std::vector<Block *> *relation, Block *b = nullptr, Block *not_b = nullptr) {
            // all transition_table indices containing a state in not_b
            std::unordered_set<int> transitions_in_not_b;
            if (not_b != nullptr) {
                for (std::pair<const int, State *> &e : not_b->states) {
                    for (const int &i : e.second->in_transition) {
                        transitions_in_not_b.insert(i);
                    }
                }
            }

            // if b == nullptr, then language is L0 and we include all indices in transition table
            // otherwise, only consider transitions where some argument is a state in b (and is not in not_b)
            std::vector<int> relevant_transitions;
            if (b == nullptr) {
                relevant_transitions.resize(a->m_transition_table.size());
                std::iota(relevant_transitions.begin(), relevant_transitions.end(), 0);
            } else {
                std::unordered_set<int> visited;
                for (std::pair<const int, State *> &e : b->states) {
                    State *s = e.second;
                    for (int i : s->in_transition) {
                        if (visited.count(i) || transitions_in_not_b.count(i)) {
                            continue;
                        }
                        relevant_transitions.push_back(i);
                        visited.insert(i);
                    }
                }
            }

            for (int i : relevant_transitions) {
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

class Composite
{
    public:
        Composite(std::vector<Block *> *p, std::vector<Block *> *r) : p(p), r(r) {}

        void split(std::vector<Block *> *r, std::vector<int> *altered) {
            for (int block_id : *altered) {
                // id of p_block from which r_split is derived
                int p_id = (r->at(block_id)->states).begin()->second->p_block->id;
                m_parent_to_split[p_id].insert(block_id);
                m_split_to_parent[block_id] = p_id;
                if (m_parent_to_split[p_id].size() >= 2) {
                    m_two_or_more_splits.insert(p_id);
                }
            }
        }

        void cut(std::vector<Block *> *p, Block *b) {
            // m_parent_to_split[p->back()->id][b->id] = b;
            int bp_id = m_split_to_parent[b->id];
            m_parent_to_split[bp_id].erase(b->id);
            if (m_parent_to_split[bp_id].size() < 2) {
                m_two_or_more_splits.erase(bp_id);
            }
            m_parent_to_split[p->back()->id].insert(b->id);
            m_split_to_parent[b->id] = p->back()->id;
        }

        // returns S_i, B_i s.t. B_i \subset S_i and |B_i| <= |S_i|/2
        std::pair<Block *, Block *> select(void) {
            if (!m_two_or_more_splits.size()) {
                return std::make_pair(nullptr, nullptr);
            }
            int p_id = *(m_two_or_more_splits.begin());
            int sz_1 = r->at(*(m_parent_to_split[p_id].begin()))->size();
            int sz_2 = r->at(*std::next((m_parent_to_split[p_id].begin()), 1))->size();
            if (sz_1 <= sz_2) {
                return std::make_pair(p->at(p_id), r->at(*(m_parent_to_split[p_id].begin())));
            } else {
                return std::make_pair(p->at(p_id), r->at(*std::next((m_parent_to_split[p_id].begin()), 1)));
            }
        }

    private:
        std::vector<Block *> *p;
        std::vector<Block *> *r;
        // split in r, parent in p
        std::unordered_map<int, std::unordered_set<int>> m_parent_to_split;
        std::unordered_map<int, int> m_split_to_parent;
        std::unordered_set<int> m_two_or_more_splits;
};

void cut(Composite *c, std::vector<Block *> *p, Block *b) {
    p->push_back(new Block(p->size()));
    for (std::pair<const int, State *> &s : b->states) {
        (p->back()->states)[s.second->id] = s.second;
        p->at(s.second->p_block->id)->states.erase(s.second->id);
        s.second->p_block = p->back();
    }
    c->cut(p, b);
}

void split(Automaton *a, Composite *c, std::vector<Block *> *r, ObsQ::CountLL *cll_node) {
    for (std::pair<const int, ObsQ::CountLL *> &e : cll_node->next_block) {
        split(a, c, r, e.second);
    }
    // move all elements from transitions_into_q into new blocks
    std::vector<int> altered;
    std::unordered_map<int, std::vector<Block *>::iterator> visited_blocks;
    for (std::pair<const int, int> &state_id_and_count : cll_node->transitions_into_q) {
        int state_id = state_id_and_count.first;
        int block_id = a->m_states[state_id]->r_block->id;

        if (!visited_blocks.count(block_id)) {
            altered.push_back(r->size());
            r->push_back(new Block(r->size()));
            visited_blocks[block_id] = r->end() - 1;
        }

        // detach state from old Block, attach to new
        State *s = a->m_states[state_id];
        s->r_block->states.erase(state_id);
        if (s->r_block->states.empty()) { // all elements now moved out
            // swap newly added block with empty block
            altered.back() = s->r_block->id;
            r->back()->id = s->r_block->id;
            r->at(s->r_block->id) = r->back();
            r->erase(r->end() - 1);
        }
        s->r_block = *(visited_blocks[block_id]);
        (*(visited_blocks[block_id]))->states[state_id] = s;
    }
    c->split(r, &altered);
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

    Composite c(&p, &r);

    // R_0 := R_0 \ split(L_0)
    ObsQ r0_obsq(a, &r, nullptr);
    for (std::pair<const std::string, ObsQ::CountLL *> &e : r0_obsq.m_f_to_countll) {
        split(a, &c, &r, e.second);
    }

    std::cout << "P0:" << std::endl;
    summarize(p);
    std::cout << "R0:" << std::endl;
    summarize(r);

    int i = 1; 
    std::pair<Block *, Block *> si_bi = c.select();
    while (si_bi.first != nullptr && si_bi.second != nullptr) {
        cut(&c, &p, si_bi.second);
        si_bi.second = p.back();
        // R_i \ split(L_{i+1}(B_i))
        ObsQ ri_obsq_bi(a, &r, si_bi.second);
        for (std::pair<const std::string, ObsQ::CountLL *> &e : ri_obsq_bi.m_f_to_countll) {
            split(a, &c, &r, e.second);
        }

        
        std::cout << "P" << i << ":" << std::endl;
        summarize(p);
        /*
        std::cout << "R" << i-1 << " \\ split:" << std::endl;
        summarize(r);
        */
        

        // std::cout << si_bi.first << " " << si_bi.second << std::endl;
        ObsQ ri_obsq_si_bi(a, &r, si_bi.first, si_bi.second);
        for (std::pair<const std::string, ObsQ::CountLL *> &e : ri_obsq_si_bi.m_f_to_countll) {
            split(a, &c, &r, e.second);
        }

        std::cout << "R" << i << ":" << std::endl;
        summarize(r);

        si_bi = c.select();

        i++;
    }

    return a;
}

std::pair<Automaton *, std::unordered_map<int, std::vector<int>>> forward_minim(Automaton *a) {

}


int main()
{
    freopen("test.out", "w", stdout);


    Automaton automaton;

    // parse file and add trees to Automaton
    for (const auto &entry : std::filesystem::directory_iterator("other_tests_2")) {
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
