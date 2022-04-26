#pragma once
#include <string>
#include <vector>
#include <fstream>
#include <iostream>
#include <unordered_map>

#include "graph.hpp"

using std::cout;
using std::endl;
using std::string;
using std::vector;
using std::unordered_map;

vector<int> splitStr(string line) {
    vector<int> res;
    while (true) {
        size_t pos = line.find(" ");
        if (pos == string::npos) {
            res.push_back(stoi(line));
            break;
        }
        res.push_back(stoi(line.substr(0, pos)));
        line = line.substr(pos+1, string::npos);
    }
    return res;
}

vector<vector<int>> ReadFile(string filename) {
    vector<vector<int>> ret;
    std::ifstream ifs(filename);
    if (ifs.fail()) {
        perror("read file failed!");
        exit(1);
    }
    std::string line;
    unordered_map<int, vector<Edge<int, int>>> adj;
    while (std::getline(ifs, line)) {
        ret.push_back(vector<int>(splitStr(line)));
    }
    return ret;
}

Graph<int,int> TopoGraph(string topofile) {
    unordered_map<int, vector<Edge<int, int>>> adj;
    vector<vector<int>> ret_vec = ReadFile(topofile);
    for (auto& res : ret_vec) {
        int src = res[0], dst = res[1], weight = res[2];
        if(adj.find(src) == adj.end() ){
            adj[src] = vector<Edge<int, int>>{};
        }
        adj[src].push_back({src,dst,weight});
        if(adj.find(dst) == adj.end() ){
            adj[dst] = vector<Edge<int, int>>{};
        }
        adj[dst].push_back({dst,src,weight});
    }
    Graph<int,int> ret(adj);
    return ret;
}