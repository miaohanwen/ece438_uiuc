#ifndef _ROUTING_H
#define _ROUTING_H

#include <string>
#include <vector>
#include <unordered_map>

using namespace std;

/**
 * Routing Table Entry (RTE) Struct
 *   Stores one entry in the routing table for a node,
 *   containing a destination node, the next hop on the
 *   path to that destination, and the total path cost.
 */
typedef struct rte {
    int         dest;           // destination node
    int         next_hop;       // next hop to dest
    int         path_cost;      // total path cost to dest
} entry_t;

/**
 * Routing Node Struct
 *   Stores information for routing
 */
typedef struct routing_node {
    int                     id;         // node ID
    unordered_map<int, int> neighbors;  // <neighbor ID, path cost>
    #ifdef DISTVEC
    vector<vector<int>>     cost_table; // matrix of costs from one node to another
    #endif
} node_t;

/**
 * Message Struct
 *   Stores information for sending a
 *   message from one node to another
 */
typedef struct message {
    int src;            // source node
    int dest;           // destination node
    string message;     // message to send
} message_t;

#endif /* _ROUTING_H */
