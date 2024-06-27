#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <list>
#include <stack>
#include <queue>
#include <utility>
#include <tuple>

struct Block;

typedef struct State
{
    int id;
    Block *block;

    State(int id, Block *block) : id(id), block(block) {}
} State;

typedef struct Block
{
    int id;
    std::list<State *> states;
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
        Automaton() {
            m_blocks.push_back(new Block()); // QxQ
        }

        State *add(Node *tree) {
            // f () -> q
            if ((tree->children).size() == 0) {
                m_states.push_back(new State(m_states.size(), m_blocks[0]));
                m_blocks[0]->states.push_back(m_states.back());
                m_transition_table.push_back(make_tuple(
                    tree->name, std::vector<State *>{}, m_states.back()
                ));
                return m_states.back();
            }
            std::vector<State *> q_i;
            for (Node *child : tree->children) {
                q_i.push_back(add(child));
            }
            m_states.push_back(new State(m_states.size(), m_blocks[0]));
            m_blocks[0]->states.push_back(m_states.back());
            m_transition_table.push_back(make_tuple(
                tree->name, q_i, m_states.back()
            ));
            return m_states.back();
        }

        void back_minim(void) {
        }

        void forward_minim(void) {
        }

        void summarize(void) {
            // print current equivalence class split
            std::cout << "Blocks:" << std::endl;
            for (Block *b : m_blocks) {
                for (State *s : b->states) {
                    std::cout << s->id << " ";
                }
                std::cout << std::endl;
            }
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
    
    private:
        // f (q_1, ..., q_n) -> q
        std::vector<std::tuple<std::string, std::vector<State *>, State *>> m_transition_table;

        std::vector<Block *> m_blocks;
        std::vector<State *> m_states;
};


int main()
{
    Automaton automaton;

    // parse file and add trees to Automaton
    for (const auto &entry : std::filesystem::directory_iterator("penn_treebank_subset")) {
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
}
