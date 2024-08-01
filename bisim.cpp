#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <list>
#include <stack>
#include <queue>
#include <utility>
#include <tuple>
#include <unordered_set>
#include <unordered_map>
#include <numeric>

using State = int;
using Block = int;

class Node
{
    public:
        std::string name;
        int depth;
        std::vector<Node *> children;

        Node(std::string name, int depth) {
            this->name = name;
            this->depth = depth;
        }

        static void print_tree(Node *node, int depth = 0) {
            if (node == nullptr) return;
            std::cout << std::string(depth, '\t') << node->name << std::endl;
            if ((node->children).size()) {
                for (Node *child : node->children) {
                    print_tree(child, depth+1);
                }
            }
        }
};

class Automaton
{
    public:
        Automaton(void) { n_states = 0; }

        State add(Node *tree) {
            // f () -> q
            if ((tree->children).size() == 0) {
                transition_table.push_back(make_tuple(tree->name, std::vector<State>{}, n_states));
            } else {
                std::vector<State> q1r;
                for (Node *child : tree->children) {
                    q1r.push_back(add(child));
                }
                transition_table.push_back(make_tuple(tree->name, q1r, n_states));
                for (State qk : q1r) {
                    transitions_containing_state[qk].insert(transition_table.size()-1);
                }
            }
            return n_states++;
        }

        void summarize(void) {
            // print current transition rules
            std::cout << "Transitions:" << std::endl;
            for (std::tuple<std::string, std::vector<State>, State> t : transition_table) {
                std::cout << std::get<0>(t) << " ( ";
                for (State s : std::get<1>(t)) {
                    std::cout << s << " ";
                }
                std::cout << ") -> " << std::get<2>(t) << std::endl;
            }
        }

        // f (q_1, ..., q_r) -> q
        std::vector<std::tuple<std::string, std::vector<State>, State>> transition_table;
        std::unordered_map<State, std::unordered_set<int>> transitions_containing_state;
        int n_states;
};

class Relation
{
    public:
        Relation(int n_states) {
            for (int i = 0; i < n_states; ++i) {
                block_of_state[i] = 0;
                states_of_block[0].insert(i);
            }
        }

        const Block block_of(State s) {
            return block_of_state[s];
        }
        const std::unordered_set<State> &states_of(Block b) {
            return states_of_block[b];
        }

        // returns newly created blocks
        std::unique_ptr<std::unordered_map<Block, Block>> separate(const std::unordered_set<State> &states) {
            std::unique_ptr<std::unordered_map<Block, Block>> parent = std::unique_ptr<std::unordered_map<Block, Block>>{new std::unordered_map<Block, Block>()};
            std::unordered_map<Block, Block> seen; // old block of state -> new block for state
            for (State s : states) {
                Block old = block_of_state[s];
                states_of_block[old].erase(s);
                if (seen.count(old)) {
                    move(s, old, seen[old]);
                } else {
                    Block nu = states_of_block.size();
                    parent->insert(std::make_pair(nu, old));
                    seen[old] = nu;
                    move(s, old, states_of_block.size());
                }
                // if removed all states from states_of_block[old], move states w/ highest block id back
                if (!states_of_block[old].size()) {
                    seen.erase(old);
                    Block highest = states_of_block.size() - 1;
                    while (states_of_block[highest].size()) {
                        State s = *(states_of_block[highest].begin());
                        move(s, highest, old);
                    }
                    parent->insert(std::make_pair(old, parent->at(highest)));
                    parent->erase(highest);
                    states_of_block.erase(highest);
                }
            }
            return parent;
        }

        void summarize(void) {
            for (int i = 0; i < states_of_block.size(); ++i) {
                std::cout << "[ ";
                for (const int &s : states_of_block[i]) {
                    std::cout << s << " ";
                }
                std::cout << "]" << std::endl;
            }
        }

    private:
        void move(State s, Block old, Block nu) {
            states_of_block[old].erase(s);
            states_of_block[nu].insert(s);
            block_of_state[s] = nu;
        }

        std::unordered_map<State, Block> block_of_state;
        std::unordered_map<Block, std::unordered_set<State>> states_of_block;
};

class ObsQ
{
    public:
        // both b and not_b are in p
        ObsQ(Automaton *a, Relation *p, Block b = -1, Block not_b = -1) {
            // all transition_table indices containing a state in not_b; O(r|delta|)
            std::unordered_set<int> transitions_in_not_b;
            if (not_b != -1) {
                for (const State &s : p->states_of(not_b)) {
                    for (const int &i : a->transitions_containing_state[s]) {
                        transitions_in_not_b.insert(i);
                    }
                }
            }
            // if b == -1, then language is L0 and we include all indices in transition table
            // otherwise, only consider transitions where some argument is a state in b (and is not in not_b)
            std::unordered_set<int> relevant_transitions;
            if (b == -1) {
                for (int i = 0; i < a->transition_table.size(); ++i) {
                    relevant_transitions.insert(i);
                }
            } else {
                for (const State &s : p->states_of(b)) {
                    for (const int &i : a->transitions_containing_state[s]) {
                        if (!transitions_in_not_b.count(i)) {
                            relevant_transitions.insert(i);
                        }
                    }
                }
            }

            for (int i : relevant_transitions) {
                std::string f = std::get<0>((a->transition_table)[i]);
                std::vector<State> q1r = std::get<1>((a->transition_table)[i]);
                State q = std::get<2>((a->transition_table)[i]);

                if (!f_to_qnodes.count(f)) {
                    f_to_qnodes[f] = new ObsQNode();
                }
                ObsQNode *ptr = f_to_qnodes[f];

                for (State qk : q1r) {
                    Block following = p->block_of(qk);
                    if (!ptr->next_block.count(following)) {
                        ptr->next_block[following] = new ObsQNode();
                    }
                    ptr = ptr->next_block[following];
                }

                ptr->transitions_into.insert(q);
            }
        }

        typedef struct ObsQNode {
            std::unordered_map<Block, ObsQNode *> next_block;
            std::unordered_set<State> transitions_into;
            // std::unordered_map<State, int> transitions_into; // (state_id of q, count)
        } ObsQNode;

        std::unordered_map<std::string, ObsQNode *> f_to_qnodes;
};

class BlockSelector
{
    public:
        BlockSelector() {
            p_parent_of_r_block[0] = 0;
        }

        void p_cut(std::unique_ptr<std::unordered_map<Block, Block>> p_parent_child, Block r_b) {
            Block p_child = p_parent_child->begin()->first;
            Block p_parent = p_parent_child->begin()->second;

            r_children_of_p_block[p_parent].erase(r_b);
            if (r_children_of_p_block[p_parent].size() < 2) {
                p_parents_with_two_children.erase(p_parent);
            }
            p_parent_of_r_block[r_b] = p_child;
        }

        void r_split(std::unique_ptr<std::unordered_map<Block, Block>> r_parent_child) {
            for (std::pair<const Block, Block> &e : *r_parent_child) {
                Block r_child = e.first;
                Block r_parent = e.second;

                // if r_child newly created, obviously not in p_parent_of_r_block
                // otherwise, implies r_child was emptied and then swapped; remove old history
                Block p_parent;
                if (p_parent_of_r_block.count(r_child)) {
                    p_parent = p_parent_of_r_block[r_child];
                    r_children_of_p_block[p_parent].erase(r_child);
                    if (r_children_of_p_block[p_parent].size() < 2) {
                        p_parents_with_two_children.erase(p_parent);
                    }
                }
                p_parent_of_r_block[r_child] = p_parent = p_parent_of_r_block[r_parent];
                r_children_of_p_block[p_parent].insert(r_child);
                if (r_children_of_p_block[p_parent].size() >= 2) {
                    p_parents_with_two_children.insert(p_parent);
                }
            }
        }

        std::pair<Block, Block> select(Relation *p, Relation *r) {
            if (!p_parents_with_two_children.size()) {
                return std::make_pair(-1, -1);
            }
            Block pb = *(p_parents_with_two_children.begin());
            std::unordered_set<Block> rc = r_children_of_p_block[pb];
            Block rb1 = *(rc.begin());
            Block rb2 = *(std::next(rc.begin(), 1));
            Block sz1 = r->states_of(rb1).size();
            Block sz2 = r->states_of(rb2).size();
            if (sz1 <= sz2) {
                return std::make_pair(pb, rb1);
            } else {
                return std::make_pair(pb, rb2);
            }
        }
    
    private:
        std::unordered_map<Block, std::unordered_set<Block>> r_children_of_p_block;
        std::unordered_map<Block, Block> p_parent_of_r_block;
        std::unordered_set<Block> p_parents_with_two_children;
};

class PRelation : public Relation
{
    public:
        PRelation(BlockSelector *bs, int n_states) : bs(bs), Relation(n_states) {}

        // P_i\cut(B_i) for B_i in (Q/R_i)
        void cut(Relation *r, Block b) {
            bs->p_cut(separate(r->states_of(b)), b);
        }

    private:
        BlockSelector *bs;
};

class RRelation : public Relation
{
    public:
        RRelation(BlockSelector *bs, int n_states) : bs(bs), Relation(n_states) {}

        // use b = S_i\B_i and not_b = B_i for splitn
        void split(Automaton *a, Relation *p, Block b = -1, Block not_b = -1) {
            ObsQ obsq(a, p, b, not_b);
            for (std::pair<const std::string, ObsQ::ObsQNode *> &e : obsq.f_to_qnodes) {
                split_helper(e.second);
            }
        }
    
    private:
        void split_helper(ObsQ::ObsQNode *qnode) {
            for (std::pair<const Block, ObsQ::ObsQNode *> &e : qnode->next_block) {
                split_helper(e.second);
            }
            bs->r_split(separate(qnode->transitions_into));
        }

        BlockSelector *bs;
};

void summarize(int iteration, Relation *p, Relation *r) {
    std::cout << std::endl << "Iteration " << iteration << ":" << std::endl;
    std::cout << "P:" << std::endl;
    p->summarize();
    std::cout << "R:" << std::endl;
    r->summarize();
}

// output: (Automaton, block_id -> [element ids in block])
// std::pair<Automaton *, std::unordered_map<int, std::vector<int>>> back_minim(Automaton *a) {
Automaton *back_minim(Automaton *a) {
    BlockSelector bs;
    PRelation p(&bs, a->n_states);
    RRelation r(&bs, a->n_states);
    r.split(a, &p);

    summarize(0, &p, &r);

    std::pair<Block, Block> si_bi;
    for (int i = 0; (si_bi = bs.select(&p, &r)).first != -1; ++i) {
        p.cut(&r, si_bi.second);
        r.split(a, &p, i+1);
        r.split(a, &p, si_bi.first, i+1);

        summarize(i+1, &p, &r);
    }

    return a;
}

std::pair<Automaton *, std::unordered_map<int, std::vector<int>>> forward_minim(Automaton *a) {

}


int main()
{
    // freopen("test.out", "w", stdout);

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
    }

    automaton.summarize();

    back_minim(&automaton);
}
