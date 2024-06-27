from nltk import Tree
from nltk.corpus import treebank
from os import mkdir
from os.path import exists
from collections import deque

if not exists("./penn_treebank_subset"):
    mkdir("./penn_treebank_subset")

for fid in treebank.fileids():

    parsed_sents = treebank.parsed_sents(fid)
    for i, tree in enumerate(parsed_sents):

        n_subtrees = tree.subtrees(lambda t: t.height() == 5)
        for j, st in enumerate(n_subtrees):
            f = open(f"./penn_treebank_subset/{fid.split('.')[0]}-sentence-{i}-subtree-{j}", 'w')

            stk = deque()
            stk.append((st, 0))
            while len(stk) > 0:
                t, depth = stk.pop()
                f.write("\t"*depth + f"{str(t.label())}\n")
                for st in reversed(t):
                    if type(st) == Tree:
                        stk.append((st, depth+1))

            f.close()
