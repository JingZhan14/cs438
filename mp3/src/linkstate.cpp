#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <iostream>
#include <map>
#include <set>
#include <fstream>
#include <climits>
#include "utility.hpp"
#include "graph.hpp"

// #define INT_MAX ~(1 << (sizeof(int) * 8 - 1))

using std::cout;
using std::endl;
using std::map;
using std::vector;
using std::set;

#define BROKEN -999

typedef struct spanTreeEntry {
    int dst;
    int nextHop;
    int weightSum;
    int prevHop;
} SpanTreeEntry;

Graph<int, int> curGraph;
set<int> allNodes;
map<int, map<int, SpanTreeEntry>> routers;
std::ofstream fpOut;

void writeMessage(string filename) {
    vector<vector<int>> ret;
    std::ifstream ifs(filename);
    if (ifs.fail()) {
        perror("read file failed!");
        exit(1);
    }
    string line;
    while (std::getline(ifs, line)) {
        // parse the line
        int src, dst;
        size_t pos = line.find(" ");
        src = stoi(line.substr(0, pos));
        line = line.substr(pos + 1, string::npos);
        pos = line.find(" ");
        dst = stoi(line.substr(0, pos));
        string message = line.substr(pos + 1, string::npos);
        // cout << src << endl << dst << endl << message << "END" << endl;

        if (routers[src][dst].nextHop == BROKEN) {
            fpOut << "from " << src << " to " << dst
                  << " cost infinite hops unreachable message " << message << endl;
            continue;
        }

        // calculate hops needed on the way
        fpOut << "from " << src << " to " << dst << " cost "
              << routers[src][dst].weightSum << " hops ";
        int curHop = src;
        while (curHop != dst) {
            fpOut << curHop << " ";
            curHop = routers[curHop][dst].nextHop;
        }
        fpOut << "message " << message << endl;
    }
}

map<int, SpanTreeEntry> Dijkstra(int curNode) {
    map<int, SpanTreeEntry> ret = map<int, SpanTreeEntry>{};
    // initialize each map entry to infty
    for (auto& v : allNodes) {
        if (v == curNode)
            ret[v] = SpanTreeEntry{v, v, 0, BROKEN};
        else
            ret[v] = SpanTreeEntry{v, BROKEN, INT_MAX, BROKEN};
    }
    set<int> visited = set<int>{};
    visited.insert(curNode);
    int cur = curNode;
    while (visited.size() != allNodes.size()) {
        // update all adjacent nodes
        vector<int> adj_v = curGraph.GetAdjVertices(cur);
        for (auto& v : adj_v) {
            if (visited.find(v) == visited.end()) {
                int newWeight = ret[cur].weightSum + curGraph.GetEdgeWeight(cur, v);
                if (newWeight < ret[v].weightSum) {
                    int nexthop = ret[cur].nextHop == curNode? v : ret[cur].nextHop;
                    ret[v] = SpanTreeEntry{v, nexthop, newWeight, cur};
                } else if (newWeight == ret[v].weightSum) {
                    int nexthop, prevhop;
                    if (cur < ret[v].prevHop) {
                        // choose the newly found path
                        prevhop = cur;
                        nexthop = ret[cur].nextHop == curNode? v : ret[cur].nextHop;
                    } else {
                        // stick to the old path
                        prevhop = ret[v].prevHop;
                        nexthop = ret[v].nextHop;
                    }
                    ret[v] = SpanTreeEntry{v, nexthop, newWeight, prevhop};
                }
            }
        }

        // iterate through all unvisited nodes
        int nextMin = BROKEN;
        bool allInfty = true;
        for (auto& v : allNodes) {
            // if not visited
            if (visited.find(v) == visited.end()) {
                if (ret[v].nextHop != BROKEN) allInfty = false;
                if (nextMin == BROKEN) {
                    nextMin = v;
                } else {
                    if (ret[v].weightSum < ret[nextMin].weightSum)
                        nextMin = v;
                    else if (ret[v].weightSum == ret[nextMin].weightSum && v < nextMin)
                        nextMin = v;
                }
            }
        }
        if (allInfty) break;
        cur = nextMin;
        visited.insert(cur);
    }
    return ret;
}

void writeTopo(int curNode) {
    set<int> visitedNodes = set<int>{};
    map<int, SpanTreeEntry> spanTree = Dijkstra(curNode);
    routers[curNode] = spanTree;
    // TODO(me): what should be print if two nodes are not reachable
    for (auto& node : allNodes) {
        if (node == curNode) {
            fpOut << node << " " << node << " " << 0 << endl;
        } else if (spanTree[node].nextHop != BROKEN) {
            fpOut << node << " " << spanTree[node].nextHop << " " << spanTree[node].weightSum << endl;
        }
    }
    //fpOut << endl;
}

int main(int argc, char** argv) {
    // printf("Number of arguments: %d", argc);
    if (argc != 4) {
        printf("Usage: ./linkstate topofile messagefile changesfile\n");
        return -1;
    }

    vector<vector<int>> topo = ReadFile(argv[1]);
    curGraph = TopoGraph(argv[1]);
    allNodes = set<int>{};
    for (auto& line : topo) {
        if (allNodes.find(line[0]) == allNodes.end()) {
            allNodes.insert(line[0]);
        }
        if (allNodes.find(line[1]) == allNodes.end()) {
            allNodes.insert(line[1]);
        }
    }

    fpOut.open("output.txt");
    if (!fpOut.is_open()) {
        perror("cannot open output.txt");
        exit(1);
    }

    routers = map<int, map<int, SpanTreeEntry>>{};

    //fpOut << endl;
    for (auto& p : allNodes) {
        // cout << p << endl;
        writeTopo(p);
        //fpOut << endl;
    }
    writeMessage(argv[2]);
    //fpOut << endl << endl;

    vector<vector<int>> changes = ReadFile(argv[3]);
    for (auto& line : changes) {
        int src = line[0], dst = line[1], weight = line[2];
        // cout << "change now: " << src << " " << dst << " " << weight << endl;
        // previously non-exist node
        // avoid insert edge twice

        if (allNodes.find(src) == allNodes.end() && allNodes.find(dst) == allNodes.end()) {
            allNodes.insert(src);
            allNodes.insert(dst);
            curGraph.InsertEdge(src, dst, weight);
            curGraph.InsertEdge(dst, src, weight);
        } else {
            if (allNodes.find(src) == allNodes.end()) {
                allNodes.insert(src);
                curGraph.InsertEdge(src, dst, weight);
                curGraph.InsertEdge(dst, src, weight);
            }

            if (allNodes.find(dst) == allNodes.end()) {
                allNodes.insert(dst);
                curGraph.InsertEdge(src, dst, weight);
                curGraph.InsertEdge(dst, src, weight);
            }
        }

        if (weight == BROKEN) {
            curGraph.DeleteEdge(src, dst);
            curGraph.DeleteEdge(dst, src);
        } else {
            if (curGraph.IfEdgeExist(src, dst)) {
                curGraph.UpdateEdge(src, dst, weight);
            } else {
                curGraph.InsertEdge(src, dst, weight);
            }
            if (curGraph.IfEdgeExist(dst, src)) {
                curGraph.UpdateEdge(dst, src, weight);
            } else {
                curGraph.InsertEdge(dst, src, weight);
            }
        }
        // cout << curGraph << endl;
        // TODO(me): possible that after deletion, a node has no link
        //fpOut << endl;
        for (auto& p : allNodes) {
            // cout << p << endl;
            writeTopo(p);
            //fpOut << endl;
        }
        writeMessage(argv[2]);
        //fpOut << endl << endl;
    }


    fpOut.close();
    return 0;
}

