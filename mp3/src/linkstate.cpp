#include <climits>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <queue>
#include <string>
#include <unordered_map>
#include <vector>

#include "routing.h"

using namespace std;

void read_topology();
void read_messages();
void send_messages();
void update_tables();
void Dijkstra(int source, vector<entry_t>& table);
void print_table(vector<entry_t>& table);
int apply_changes();
int main(int argc, char** argv);

// output file steam
ofstream outfile;
// input file streams
ifstream topofile, messagefile, changesfile;
// map of node IDs to node structures storing topology info
map<int, node_t*> topology;
// list of messages to send between nodes
vector<message_t*> message_list;
// map of nodes to routing lists -- network wide routing info
map<int, vector<entry_t>> routing_table;


/**
 * Create a topology map from input file.
 * Sets the link cost between two nodes in the network,
 * overwriting previous values if the nodes have already
 * been defined.
 */
void read_topology() {
    // read through the topology file and
    // create the initial topology
    int a, b, cost;
    while (topofile >> a >> b >> cost) {
        // If either nodeA or nodeB are not in the topology
        // table, create new nodes and add them to the table

        if (topology.find(a) == topology.end()) {
            node_t* nodeA = new node_t; // create new node
            nodeA->id = a;              // set the node ID
            topology[a] = nodeA;        // add the node to the topology table
        }
        if (topology.find(b) == topology.end()) {
            node_t* nodeB = new node_t; // create new node
            nodeB->id = b;              // set the node ID
            topology[b] = nodeB;        // add the node to the topology table
        }

        // update the cost of the link between these nodes
        topology[a]->neighbors[b] = cost;
        topology[b]->neighbors[a] = cost;
    }
}

/**
 * Read messages from message file into
 * a list for later use.
 */
void read_messages() {
    string line;

    int src, dest;
    string message;
    while (getline(messagefile,line)) {
        // save the souce and dest from the line
        sscanf(line.c_str(), "%d %d %*s", &src, &dest);
        // find the start of the message (after the second space)
        line = line.substr(line.find(' ') + 1);     // find first space
        message = line.substr(line.find(' ') + 1);  // find second space

        message_t* msg = new message_t;
        msg->src = src;
        msg->dest = dest;
        msg->message = message;
        message_list.push_back(msg);
    }
}

/**
 * Send messages between nodes, recording the path taken,
 * and write the cost and path to the output file.
 */
void send_messages() {
    for (auto msg : message_list) {
        int src = msg->src;
        int dest = msg->dest;

        outfile << "from " << src << " to " << dest;
        cout << "Sending message from " << src
             << " to " << dest
             << ": " << msg->message << endl;

        // get source node's forwarding table
        vector<entry_t> forward_table = routing_table[src];

        if (forward_table.size() == 0) {
            cout << ">> No routing table for node " << src << ". Skipping message.\n";
            continue;
        }

        // find the entry for the destination
        int index = 0;
        while (forward_table[index].dest != dest && index < forward_table.size()) {
            index++;
        }

        // If destination is reachable, trace the hops and print the
        // cost along with the path taken.  Otherwise print infinite cost
        // and no path.
        int cost = forward_table[index].path_cost;
        if (cost >= 0) {
            outfile << " cost " << cost << " hops ";
            cout << ">> Message delivered with cost " << cost << " via nodes ";

            queue<int> hops;
            hops.push(src);
            int next_hop = forward_table[index].next_hop;
            if (next_hop != dest) {
                hops.push(next_hop);
            }
            // follow the hops until we reach the destination
            while (next_hop != dest) {
                forward_table = routing_table[next_hop];
                index = 0;
                while (forward_table[index].dest != dest && index < forward_table.size()) {
                    index++;
                }
                next_hop = forward_table[index].next_hop;
                if (next_hop != dest) {
                    hops.push(next_hop);
                }
            }

            while (hops.size() > 0) {
                outfile << hops.front() << ' ';
                cout << hops.front() << ' ';
                hops.pop();
            }
            outfile << "message " << msg->message << endl;
            cout << endl;
        }
        // unreachable node
        else {
            outfile << " cost infinite hops unreachable message " << msg->message << endl;
            cout << ">> Message could not be delivered\n";
        }
    }
    outfile << endl;
    cout << endl;
}

/**
 * Update the routing table for each node and write
 * to output file.
 */
void update_tables() {
    // loop through the topology data to get each
    // node's forwarding table for updating
    for (auto node : topology) {
        // create a temp table for this node's forwarding info
        vector<entry_t> forward_table;
        // run Dijkstra's algorithm on this node
        Dijkstra(node.first, forward_table);
        // output the updated table to the outfile
        print_table(forward_table);
        outfile << endl;
        // save this table in the global routing table
        routing_table[node.first] = forward_table;
    }
}

/**
 * Apply Dijkstra's minimum path algorithm to find the minimum
 * weight path from a source node to all connected nodes. For
 * each connected node, add a routing entry indicating the destination,
 * next hop along the path to that destination, and the total weight
 * of the path.
 *
 * @param source Node ID of starting node
 * @param table  Routing table to be filled for source node
 */
void Dijkstra(int source, vector<entry_t>& table) {
    unordered_map<int, bool> N;     // finished nodes
    unordered_map<int, int> D;      // link cost to node
    unordered_map<int, int> P;      // previous node in path

    N[source] = true;   // source is automatically done

    // set initial state of neighbors
    for (auto neighbor : topology[source]->neighbors) {
        D[neighbor.first] = neighbor.second;
        P[neighbor.first] = source;
    }

    // if source is isolated, fill table with unreachable paths
    if (D.size() == 0) {
        for (auto p : topology) {
            entry_t* entry = new entry_t;
            entry->dest = p.first;
            entry->path_cost = -1;
            // if this node is the source, the distance
            // is 0 and the next hop is itself
            if (p.first == source) {
                entry->path_cost = 0;
                entry->next_hop = source;
            }
            // otherwise, there is no next hop
            // since the source is isolated
            else {
                entry->next_hop = -1;
            }

            table.push_back(*entry);
        }
        return;
    }
    // otherwise, continue with neighbors
    else {
        // continue until we know all of the nodes
        while (N.size() < topology.size()) {
            int min_id, min_cost;
            min_cost = INT_MAX;     // set maximum cost to start

            for (auto p : D) {
                // if link cost is smaller than min_cost and node
                // has not yet been processed, save the id
                if (p.second <= min_cost && N.find(p.first) == N.end()) {
                    // tiebreaking -- choose node with lowest ID
                    if (D[p.first] == min_cost) {
                        min_id = min(min_id, p.first);
                        // D[p.first] = min_cost;
                    }
                    // new link is cheaper, so save it
                    else {
                        min_id = p.first;
                        min_cost = p.second;
                    }
                }
            }

            if (min_cost != INT_MAX) {
                // mark current node as finished
                N[min_id] = true;
                // update D(v) for all nodes not already in N
                for (auto p : topology[min_id]->neighbors) {
                    if (N.find(p.first) == N.end()) {
                        // if a path to the current node exists, compare
                        // it to the path through this node.
                        if (D.find(p.first) != D.end()) {
                            int w = D[min_id] + p.second;

                            // tiebreaking
                            if (D[p.first] == w) {
                                // choose path whose last node has a smaller ID
                                if (P[p.first] > min_id) {
                                    P[p.first] = min_id;
                                    // D[p.first] = w;
                                }
                            }

                            else if (D[p.first] > w) {
                                P[p.first] = min_id;
                                D[p.first] = w;
                            }
                        }
                        // otherwise, no previous path exists, so create one
                        // through the current node as a sum of the current
                        // cost plus the cost to this node
                        else {
                            D[p.first] = D[min_id] + p.second;
                            P[p.first] = min_id;
                        }

                    }
                }
            }
            else {
                // if minimum path hasn't changed, there are no more connected nodes
                // set remaining unprocessed nodes to unreachable to exit the while loop
                for (auto p : topology) {
                    if (N.find(p.first) == N.end()) {
                        N[p.first] = true;
                        D[p.first] = -1;
                        P[p.first] = -1;
                    }
                }
            }
        }
    }

    // Dijkstra's Algorithm finished, fill the table
    for (auto p : topology) {
        entry_t* entry = new entry_t;
        entry->dest = p.first;
        if (p.first == source) {
            entry->path_cost = 0;
            entry->next_hop = source;
        }
        else {
            entry->path_cost = D[p.first];

            int prev = P[p.first];
            // if this node's previous node is the source,
            // record the hop
            if (prev == source) {
                entry->next_hop = p.first;
            }
            // if this node has no previous node,
            // mark the hop as unavailable
            else if (prev == -1) {
                entry->next_hop = -1;
            }
            // otherwise, loop until we reach the node
            // at the next hop
            else {
                while (prev != source) {
                    if (P[prev] == source) {
                        entry->next_hop = prev;
                    }
                    prev = P[prev];
                }
            }
        }
        table.push_back(*entry);
    }
}

/**
 * Write a routing table to the output file.
 * Output is formatted as <Destination> <Next Hop> <Path Cost>.
 *
 * @param table Reference to vector of routing
 *              entries comprising the routing table
 */
void print_table(vector<entry_t>& table) {
    for (entry_t entry : table) {
        // if the destination is unreachable, don't print it
        if (entry.path_cost == -1 || entry.next_hop == -1) {
            continue;
        }
        outfile << entry.dest << ' ';
        outfile << entry.next_hop << ' ';
        outfile << entry.path_cost << endl;
    }
}

/**
 * Modify the network topology according to a change from
 * the changefile.  Creates, updates, or destroys a link
 * between two nodes.  Will return zero only when all changes
 * have been made (i.e. EOF reached) or the changefile has
 * been closed.  If a change is read from the file, a non-zero
 * value is returned indicating the network is not steady-state.
 *
 * Note: this does not modify the routing tables, only the topology data.
 *
 * @return 0 if no changes remain, 1 otherwise.
 */
int apply_changes() {
    int src, dest, cost;
    if (changesfile.is_open() && !changesfile.eof()) {
        changesfile >> src >> dest >> cost;
        // update an existing link or add a new one
        // if it doesn't already exist
        if (cost > 0) {
            printf("Setting link %d <-> %d to %d\n", src, dest, cost);
            topology[src]->neighbors[dest] = cost;
            topology[dest]->neighbors[src] = cost;
            // return successful change
            return 1;
        }
        // remove a link or do nothing if no link exists
        else if (cost == -999) {
            printf("Removing link %d <-> %d\n", src, dest);
            topology[src]->neighbors.erase(dest);
            topology[dest]->neighbors.erase(src);
            // return successful change
            return 1;
        }
    }

    // return failure
    return 0;
}

int main(int argc, char** argv) {
    //printf("Number of arguments: %d", argc);
    if (argc != 4) {
        printf("Usage: ./linkstate topofile messagefile changesfile\n");
        return -1;
    }

    // open the files
    outfile.open("output.txt");
    topofile.open(argv[1]);
    messagefile.open(argv[2]);
    changesfile.open(argv[3]);

    // read initial state data
    read_topology();
    read_messages();

    // Update the routing tables and send messages
    // as long as there are changes to be made
    do {
        update_tables();
        send_messages();
    } while (0 != apply_changes());

    // cleanup allocated memory
    for (auto msg : message_list) {
        delete msg;
    }

    // close the files
    outfile.close();
    topofile.close();
    messagefile.close();
    changesfile.close();

    return 0;
}
