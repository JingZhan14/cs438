#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include <iostream>
#include <unordered_map>
#include <fstream>
#include <set>
#include <vector>

using namespace std;

string topofile, messagefile, changesfile;
unordered_map<int, unordered_map<int, int> > graph; // node1 node2 cost
set<int> allNodes;
unordered_map<int, unordered_map<int, int> > disVec;
unordered_map<int, unordered_map<int, int> > nextHop;
ofstream fpOut;
typedef struct message {
    int src;
    int des;
    string content;

    message(int src, int des, string content) : src(src), des(des), content(content) {}
} message;
vector<message> msgVec;

void update(unordered_map<int, unordered_map<int, int> > &disVec, unordered_map<int, unordered_map<int, int> > &nextHop) {
    for(int i=0; i<allNodes.size(); i++) {
        for(auto x=allNodes.begin(); x!=allNodes.end(); x++) {
            for(auto y=allNodes.begin(); y!=allNodes.end(); y++) {
                int from = *x;
                int to = *y;
                int oriDis = disVec[from][to];
                int oriNext = nextHop[from][to];
                for(auto v=allNodes.begin(); v!=allNodes.end(); v++) {
                    int neighbour = *v;
                    int xv = graph[from][neighbour];
                    int vy = disVec[neighbour][to];
                    // if x->v & v-> y
                    if(xv>=0 && vy>=0) {
                        int curC = xv + vy;
                        if(oriDis<0 || curC<oriDis) {
                            oriNext = neighbour;
                            oriDis = curC;
                        }
                        // tie break
                        // else if(curC==oriDis && neighbour<oriNext) {
                        //     oriNext = neighbour;
                        // }
                    }
                }
                disVec[from][to] = oriDis;
                nextHop[from][to] = oriNext;
            }
        }
    }
    fpOut << endl;
    for(auto i=allNodes.begin(); i!=allNodes.end(); i++) {
        for(auto j=allNodes.begin(); j!=allNodes.end(); j++) {
            fpOut << *j << " " << nextHop[*i][*j] << " " << disVec[*i][*j] << endl;
        }
        fpOut << endl;
    }
}

void sendMessage() {
    for(int i=0; i<msgVec.size(); i++) {
        int from = msgVec[i].src;
        int to = msgVec[i].des;
        string msg = msgVec[i].content;
        fpOut << "from " << from << " to " << to;
        fpOut << " cost ";
        int cost = disVec[from][to];
        if(cost==-999) fpOut << "infinite hops unreachable";
        else {
            fpOut << cost;
            // output the hops
            fpOut << " hops ";
            int hop = from;
            while(hop!=to) {
                fpOut << hop << " ";
                hop = nextHop[hop][to];
            }
        }
        fpOut << "message " << msg << endl;
    }
}

int main(int argc, char** argv) {
    //printf("Number of arguments: %d", argc);
    if (argc != 4) {
        printf("Usage: ./distvec topofile messagefile changesfile\n");
        return -1;
    }

    ifstream topofile(argv[1]);
    ifstream messagefile(argv[2]);
    ifstream changesfile(argv[3]);

    int from, to, cost;
    // build the graph
    while(topofile >> from >> to >> cost) {
        graph[from][to] = cost;
        graph[to][from] = cost;
        allNodes.insert(from);
        allNodes.insert(to);
    }
    topofile.close();

    // read message
    string curLine;
    while (getline(messagefile, curLine)){
        sscanf(curLine.c_str(), "%d %d %*s", &from, &to);
        string content = curLine.substr(curLine.find(" ")+1);
        content = content.substr(content.find(" ")+1);
        message newMessage(from, to, content);
        msgVec.push_back(newMessage);
    }
    messagefile.close();

    // read changefile


    // inital disVec and nextHop
    for(auto i=allNodes.begin(); i!=allNodes.end(); i++) {
        for(auto j=allNodes.begin(); j!=allNodes.end(); j++) {
            from = *i;
            to = *j;
            if(from==to) graph[from][to] = 0;
            else {
                auto src = graph.find(from);
                auto des = src->second.find(to);
                if(des==src->second.end()) {
                    graph[from][to] = -999;
                }
            }
            disVec[from][to] = graph[from][to];
            nextHop[from][to] = to;
        }
    }
    topofile.close();
    fpOut.open("output.txt");

    // update disVec and nexthop
    update(disVec, nextHop);
    
    // send Message once before any change
    sendMessage();

    // do cost change
    while(changesfile >> from >> to >> cost) {
        graph[from][to] = cost;
        graph[to][from] = cost;
        // link cost change
        // disVec[from][to] = cost;
        // disVec[to][from] = cost;
        // nextHop[from][to] = to;
        // nextHop[to][from] = from;
        for(auto i=allNodes.begin(); i!=allNodes.end(); i++) {
            for(auto j=allNodes.begin(); j!=allNodes.end(); j++) {
                disVec[*i][*j] = graph[*i][*j];
                nextHop[*i][*j] = *j;
            }
        }
        update(disVec, nextHop);
        sendMessage();
    }

    fpOut.close();

    return 0;
}
