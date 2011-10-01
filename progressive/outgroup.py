#!/usr/bin/env python

#Copyright (C) 2011 by Glenn Hickey
#
#Released under the MIT license, see LICENSE.txt

""" Compute outgroups for subtrees by greedily finding
the nearest valid candidate.  has option to only assign
leaves as outgroups

"""

import os
import xml.etree.ElementTree as ET
import sys
import math
import copy
import networkx as NX

from optparse import OptionParser

from cactus.progressive.multiCactusProject import MultiCactusProject
from cactus.progressive.multiCactusTree import MultiCactusTree

class GreedyOutgroup:
    def __init__(self):
        self.dag = None
        self.dm = None
        self.dmDirected = None
        self.root = None
        self.ogMap = None
        self.mcTree = None
        
    # add edges from sonlib tree to self.dag
    # compute self.dm: an undirected distance matrix
    def importTree(self, mcTree):
        self.mcTree = mcTree
        self.dag = mcTree.nxDg.copy()
        self.root = mcTree.rootId
        self.stripNonEvents(self.root, mcTree.subtreeRoots)
        self.dmDirected = NX.algorithms.shortest_paths.weighted.\
        all_pairs_dijkstra_path_length(self.dag)
        graph = NX.Graph(self.dag)
        self.dm = NX.algorithms.shortest_paths.weighted.\
        all_pairs_dijkstra_path_length(graph)
 
    # get rid of any node that's not an event
    def stripNonEvents(self, id, subtreeRoots):
        children = []
        for outEdge in self.dag.out_edges(id):
            children.append(outEdge[1])
        parentCount = len(self.dag.in_edges(id))
        assert parentCount <= 1
        parent = None
        if parentCount == 1:
            parent = self.dag.in_edges[0][0]
        if id not in subtreeRoots and len(children) >= 1:
            assert parentCount == 1
            self.dag.remove_node(id)
            for child in children:
                self.dag.add_edge(parent, child)
                self.stripNonEvents(child, subtreeRoots)
    
    # are source and sink son same path to to root? if so,
    # they shouldn't be outgroups of each other.             
    def onSamePath(self, source, sink):
        if source in self.dmDirected:
            if sink in self.dmDirected[source]:
                return True
        if sink in self.dmDirected:
            if source in self.dmDirected[sink]:
                return True 
        return False
    
    # greedily assign closest possible valid outgroup
    # all outgroups are stored in self.ogMap
    # edges between leaves ARE NOT kept in the dag         
    def greedy(self, justLeaves = False):
        orderedPairs = []
        for source, sinks in self.dm.items():
            for sink, dist in sinks.items():
                if source != self.root and sink != self.root:
                    orderedPairs.append((dist, (source, sink)))
        orderedPairs.sort(key = lambda x: x[0])
        finished = set()
        self.ogMap = dict()
        
        for candidate in orderedPairs:
            source = candidate[1][0]
            sink = candidate[1][1]
            dist = candidate[0]
            
            # skip leaves (as sources)
            if len(self.dag.out_edges(source)) == 0:
                finished.add(source)
                
            # skip internal as sink if param specified
            if justLeaves == True and len(self.dag.out_edges(sink)) > 0:
                continue
    
            if source not in finished and \
            not self.onSamePath(source, sink):
                self.dag.add_edge(source, sink, weight=dist, info='outgroup')
                if NX.is_directed_acyclic_graph(self.dag):
                    finished.add(source)
                    sourceName = self.mcTree.getName(source)
                    sinkName = self.mcTree.getName(sink)
                    self.ogMap[sourceName] = (sinkName, dist)                    
                else:
                    self.dag.remove_edge(source, sink)

def main():
    usage = "usage: %prog <project> <output graphviz .dot file>"
    description = "TEST: draw the outgroup DAG"
    parser = OptionParser(usage=usage, description=description)
    parser.add_option("--justLeaves", dest="justLeaves", action="store_true", 
                      default = False, help="Assign only leaves as outgroups")
    options, args = parser.parse_args()
    
    if len(args) != 2:
        parser.print_help()
        raise RuntimeError("Wrong number of arguments")

    proj = MultiCactusProject()
    proj.readXML(args[0])
    outgroup = GreedyOutgroup()
    outgroup.importTree(proj.mcTree)
    outgroup.greedy(options.justLeaves)
    NX.drawing.nx_agraph.write_dot(outgroup.dag, args[1])
    return 0

if __name__ == '__main__':    
    main()