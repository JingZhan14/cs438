#pragma once
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <iostream>
#include <algorithm>
#include <climits>

// #define INT_MAX ~(1 << (sizeof(int) * 8 - 1))

using std::pair;
using std::make_pair;
using std::unordered_map;
using std::unordered_set;
using std::vector;

template <typename T, typename V> 
class Edge {
 public:
  T src_;
  T dest_;
  V weight_;

  /*
   * default constructor
   */
  Edge() {
    // intentionally left blank
  }

  /*
   * initialize for edge
   */
  Edge(const T &src, const T &dest, const V &weight) {
    src_ = src;
    dest_ = dest;
    weight_ = weight;
  }
};

template <typename T, typename V>
std::ostream& operator<<(std::ostream& os, const Edge<T,V>& edge) {
    os << edge.src_ << " --_" << edge.weight_ << "_--> " << edge.dest_;
    return os;
}

template <class T, class V> class Graph {
 public:
  /**
   * defualt constructor
   */
  Graph() {
    // intentionally left blank here
  }

  /**
   * constructor for initialization of list
   * @param adj contains adjacency lists for the constructed graph
   */

  Graph(unordered_map<T, vector<Edge<T, V>>> adj) { adj_ = adj; }

  /**
   * a helper function that returns the shortest distance of all visited
   * vertices
   * @param src the starting vertex of queried vertex
   * @param dest the ending vertex of queried vertex
   * @param cur_dist the map that contains all visited vertices
   * @return the current shortest distance from src to dest in the map
   */
  int getDist(T src, T dest, unordered_map<T, pair<int, T>> &cur_dist) {
    T cur = dest;
    int res = 0;
    while (cur != src) {
      res += cur_dist[cur].first;
      cur = cur_dist[cur].second;
    }
    // cout<<"src:"<<src<<" dest:"<<dest<<" dist:"<<res<<endl;
    return res;
  }

  /**
   * compute the shortest path between two input vertices using Dijkstra's
   * algorithm
   * @param src the starting vertex of queried vertex
   * @param dest the ending vertex of queried vertex
   * @return a vector of all the vertices on the shortest path
   */
  vector<T> shortestPath(T src, T dest) {
    vector<T> res;

    // key: cur, value: edge weight, prev vertex
    unordered_map<T, pair<int, T>> cur_dist;
    unordered_set<T> visited;
    cur_dist[src] = make_pair(0, T());

    T cur = src;
    while (true) {
      if (cur == dest)
        break;
      pair<T, int> smallest = make_pair(cur, INT_MAX);

      bool allNeighborVisited = true;
      for (auto &e : adj_[cur]) {
        T next = e.dest_;

        if (visited.find(next) == visited.end()) {
          // not visited before
          allNeighborVisited = false;
          if (cur_dist.find(next) == cur_dist.end()) {
            cur_dist[next] = make_pair(e.weight_, cur);
          } else {
            if (getDist(src, next, cur_dist) >
                getDist(src, cur, cur_dist) + e.weight_) {
              cur_dist[next] = make_pair(e.weight_, cur);
            }
          }
          int next_dist = getDist(src, next, cur_dist);
          if (next_dist < smallest.second)
            smallest = make_pair(next, next_dist);
        }
      }

      if (allNeighborVisited)
        break;
      visited.insert(cur);
      cur = smallest.first;
    }

    // return correct path
    cur = dest;
    while (cur != src) {
      res.push_back(cur);
      cur = cur_dist[cur].second;
    }
    res.push_back(src);
    reverse(res.begin(), res.end());
    return res;
  }

  /**
   * getter for all adjacency relations
   * @return a map contains all the adjacency lists
   */
  unordered_map<T, vector<Edge<T, V>>> GetAdj() { return adj_; }

  /**
   * getter for all adjacent edges
   * @param src the queried src vertex
   * @return a vector containing all adjacent edges
   */
  vector<Edge<T, V>> GetAdjEdges(T src) {
    if (adj_.find(src) != adj_.end()) {
      return adj_[src];
    }
    return vector<Edge<T, V>>{};
  }

  /**
   * getter for all adjacent vertices
   * @param src the queried src vertex
   * @return a vector containing all adjacent vertices
   */
  vector<T> GetAdjVertices(T src) {
    if (adj_.find(src) != adj_.end()) {
      vector<T> res;
      for (auto &v : adj_[src]) {
        res.push_back(v.dest_);
      }
      return res;
    }
    return vector<T>{};
  }

  /**
   * check for vertex existence
   * @param src the queried src vertex
   * @return a bool represents the existence of vertex
   */
  bool IfVertexExist(T src) { return adj_.find(src) != adj_.end(); }

  /**
   * check for edge existence
   * @param src the queried src vertex
   * @param dest the queried dest vertex
   * @return a bool represents the existence of edge
   */
  bool IfEdgeExist(T src, T dest) {
    if (!IfVertexExist(src))
      return false;
    vector<Edge<T, V>> &tmp = adj_[src];
    for (auto &e : tmp) {
      if (e.dest_ == dest)
        return true;
    }
    return false;
  }

  /**
   * getter for edge weight
   * @param src the queried src vertex
   * @param dest the queried dest vertex
   * @return a V type value represents the edge weight
   * if such edge does not exist, throw exception
   */
  V GetEdgeWeight(T src, T dest) {
    if (!IfEdgeExist(src, dest))
      throw std::invalid_argument("Queried Edge does not exist!");
    vector<Edge<T, V>> &tmp = adj_[src];
    for (auto &e : tmp) {
      if (e.dest_ == dest)
        return e.weight_;
    }
    return V(); // to silence the warning
  }

  /**
   * insert a new vertex
   * @param src the newly inserted src vertex
   */
  void InsertVertex(T src) {
    if (!IfVertexExist(src)) {
      adj_[src] = vector<Edge<T, V>>();
    }
  }

  /**
   * insert a new edge
   * @param src the inserted src vertex
   * @param dest the inserted dest vertex
   * @param weight the inserted edge weight
   */
  void InsertEdge(T src, T dest, V weight) {
    if (!IfVertexExist(src)) {
      adj_[src] = vector<Edge<T, V>>({Edge<T, V>(src, dest, weight)});
      return;
    }
    if (!IfEdgeExist(src, dest)) {
      adj_[src].push_back(Edge<T, V>(src, dest, weight));
      return;
    }
    throw std::invalid_argument("Insert an edge that already exists!");
  }

  /**
   * update an existing edge with a new weight
   * @param src the src vertex
   * @param dest the dest vertex
   * @param weight the newly updated edge weight
   */
  void UpdateEdge(T src, T dest, V weight) {
    if (!IfEdgeExist(src, dest)) {
      throw std::invalid_argument("Update an edge that does not exist!");
    }
    vector<Edge<T, V>> &tmp = adj_[src];
    for (auto &e : tmp) {
      if (e.dest_ == dest) {
        e.weight_ = weight;
        return;
      }
    }
  }

  /**
   * delete a vertex
   * @param src the src vertex to be deleted
   */
  void DeleteVertex(T src) {
    if (!IfVertexExist(src)) {
      throw std::invalid_argument("Deleting a vertex that does not exist!");
    }
    adj_.erase(src);
    for (auto &p : adj_) {
      auto &v = p.second;
      for (size_t i = 0; i < v.size(); i++) {
        if (v[i].dest_ == src) {
          v.erase(v.begin() + i);
        }
      }
    }
  }

  /**
   * delete an edge
   * @param src the src vertex to be deleted
   * @param dest the dest vertex to be deleted
   */
  void DeleteEdge(T src, T dest) {
    if (!IfEdgeExist(src, dest)) {
      throw std::invalid_argument("Delete an edge that does not exist!");
    }
    auto &v = adj_[src];
    for (size_t i = 0; i < v.size(); i++) {
      if (v[i].dest_ == dest) {
        v.erase(v.begin() + i);
      }
    }
  }

 private:
  /**
   * adjacency list for the graph, key is the src vertex,
   * the value is a vector of dest vertex and the edge weight
   */
  unordered_map<T, vector<Edge<T, V>>> adj_;
};

template <typename T, typename V>
std::ostream& operator<<(std::ostream& os, Graph<T,V>& graph) {
    auto m = graph.GetAdj();
    for (auto& k : m){
        os << "edges for " << k.first << ":" << std::endl;
        for (auto& e: k.second){
            os << e << std::endl;
        }
    }
    return os;
}
