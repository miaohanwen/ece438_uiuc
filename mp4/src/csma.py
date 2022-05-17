#!/usr/bin/env python3
import math
import numpy as np
import random
import sys
import time


class Node:
    """Basic node which tracks its place in line and how many
       successful/failed transmitions it has had."""
    def __init__(self, id):
        self.id = id            # node ID
        self.backoff = -1       # backoff counter, randomly generated per packet
        # self.range_idx = 0      # using first range value
        self.collisions = 0     # number of collisions

        self.total_success = 0  # number of successful transmissions
        self.total_fail = 0     # number of failed transmission attempts
        self.total_drop = 0     # number of packets dropped

    def drop_packet(self):
        """Drop the current packet by reseting collision count and backoff value."""
        self.total_drop += 1    # count against total drop count
        self.collisions = 0     # reset packet collision counter
        self.backoff = -1       # reset the backoff counter

    def set_backoff(self, range):
        """Set the backoff to a random integer in [0,range)."""
        self.backoff = random.randint(0, range)


class Network:
    def __init__(self, n, l, r, m):
        self.n_nodes = n        # number of nodes in the network
        self.pkt_len = l        # length of each packet, in clock ticks
        self.ranges = r         # list of random backoff max values
        self.max_retry = m      # number of attempts before dropping the packet

        self.clock = 0           # global network clock
        self.busy = False       # channel is not busy
        self.collision = False  # did a collision occur?
        self.active_node = None # nobody is transmitting
        self.tx_end = -1        # current transmission end time

        # Create nodes in network and initialize backoff values
        self.nodes = [Node(i) for i in range(n)]
        for node in self.nodes:
            node.set_backoff(self.ranges[0])

    def reset(self):
        """Reset the network to start a new simulation."""
        self.clock = 0
        self.busy = False
        self.collision = False
        self.active_node = None
        self.tx_end = -1

        for node in self.nodes:
            node.set_backoff(self.ranges[0])

    def tick(self):
        """Advance the global network clock."""
        self.clock += 1

        # if a collision happened, advance the clock
        # without doing anything else
        if self.collision:
            self.collision = False
            return

        # network is idle, decrement backoff for each node
        if not self.busy:
            for node in self.nodes:
                node.backoff -= 1
        # network is busy, check if transmission is done
        else:
            if self.clock == self.tx_end:
                self.busy = False

                self.active_node.total_success += 1
                self.active_node.collisions = 0
                self.active_node.set_backoff(self.ranges[0])

                self.active_node = None

    def get_tx_nodes(self):
        """Get a list of nodes in the network which are ready to transmit."""
        return [node for node in self.nodes if node.backoff == 0]

    def transmit(self, node):
        """Start a transmission from the specified node."""
        self.busy = True
        self.active_node = node
        self.tx_end = self.clock + self.pkt_len

    def handle_collision(self, nodes):
        """Handle a collision between specified nodes by either retrying transmission or dropping packets."""
        self.collision = True
        for node in nodes:
            node.collisions += 1     # count collision against current packet
            node.total_fail += 1     # count collision against total

            # Should this packet be dropped?
            if node.collisions >= self.max_retry:
                node.drop_packet()                   # drop the current packet
                node.set_backoff(self.ranges[0])     # initialize backoff for next packet
            # If not, assign a new backoff value and retry transmission
            else:
                range_idx = node.collisions
                if range_idx >= len(self.ranges):
                    range_idx = len(self.ranges) - 1
                node.set_backoff(self.ranges[range_idx])

    def print_status(self):
        """Print a status line to the console describing the current state of the network."""
        active = 'None'
        ready = 'None'
        tx_nodes = self.get_tx_nodes()

        if self.active_node is not None:
            active = str(self.active_node.id)
        if len(tx_nodes) > 0:
            ready = ''
            for node in tx_nodes:
                ready += str(node.id) + ' '

        print(('Tick: ' + str(self.clock)).ljust(15), end=' ')
        print(('Active node: ' + active).ljust(20), end=' ')
        print('Ready to TX:', ready)

    def resize(self, size):
        N = self.n_nodes
        if size < N:
            self.nodes = self.nodes[:size]
        elif size > N:
            self.nodes += [Node(N + i) for i in range(size-N)]

        self.n_nodes = size
        self.reset()


# Taken from StackOverflow user 'scls'
# https://stackoverflow.com/a/15734251/3150481
def to_si(d, sep=' '):
    """Convert number to string with SI prefix"""

    incPrefixes = ['k', 'M', 'G', 'T', 'P', 'E', 'Z', 'Y']
    decPrefixes = ['m', 'µ', 'n', 'p', 'f', 'a', 'z', 'y']

    if d == 0:
        return str(0)

    degree = int(math.floor(math.log10(math.fabs(d)) / 3))
    prefix = ''
    if degree != 0:
        ds = degree/math.fabs(degree)
        if ds == 1:
            if degree - 1 < len(incPrefixes):
                prefix = incPrefixes[degree - 1]
            else:
                prefix = incPrefixes[-1]
                degree = len(incPrefixes)
        elif ds == -1:
            if -degree - 1 < len(decPrefixes):
                prefix = decPrefixes[-degree - 1]
            else:
                prefix = decPrefixes[-1]
                degree = -len(decPrefixes)

        scaled = float(d * math.pow(1000, -degree))
        s = "{scaled}{sep}{prefix}".format(scaled=scaled, sep=sep, prefix=prefix)
    else:
        s = "{d}".format(d=d)

    return(s)


def main():
    argc = len(sys.argv)
    if argc != 2:
        print('Usage: ./csma input.txt')
        sys.exit(1)

    # parse the parameters
    parameters = {}
    try:
        with open(sys.argv[1], 'r') as input:
            line_args = [line.split() for line in input]
            for args in line_args:
                name = args[0]
                values = [int(x) for x in args[1:]]
                parameters[name] = values if len(values) != 1 else values[0]
    except IOError as err:
        print(err)
        sys.exit(1)

    num_nodes = parameters['N']
    pkt_length = parameters['L']
    ranges = parameters['R']
    max_retry = parameters['M']
    duration = parameters['T']

    # create the network
    network = Network(num_nodes, pkt_length, ranges, max_retry)

    # run simulation
    idle_ticks = 0              # number of clock ticks channel is idle for
    utilization_ticks = 0       # number of clock ticks used for correct communication
    collision_count = 0         # number of collisions

    start_time = time.time()

    for t in range(duration):
        # network.print_status()

        # if channel is not busy, check for collisions
        # and send any packets as neccessary
        if not network.busy:
            # check for collisions
            nodes_to_tx = network.get_tx_nodes()

            # collision detected!
            if len(nodes_to_tx) > 1:
                collision_count += 1
                network.handle_collision(nodes_to_tx)

            # no collision detected, only one transmitter
            # this tick counts as active since it is the
            # first of the transmission
            elif len(nodes_to_tx) == 1:
                utilization_ticks += 1
                network.transmit(nodes_to_tx[0])

            # no one is transmitting, network is truely idle
            else:
                idle_ticks += 1

        # channel is busy
        else:
            utilization_ticks += 1

        # advance the clock and loop
        network.tick()

    end_time = time.time()
    print('Simulation finished in ', to_si(end_time - start_time), 's', sep='')

    # calculate statistics
    percent_utilization = utilization_ticks / duration * 100
    percent_idle = idle_ticks / duration * 100
    variance_success = np.var([n.total_success for n in network.nodes])
    variance_collision = np.var([n.total_fail for n in network.nodes])

    print('Dropped %d packets' % sum([n.total_drop for n in network.nodes]))

    # write to output file
    with open('output.txt', 'w') as output:
        output.write('Channel utilization (in percentage) ' + str(round(percent_utilization, 3)) + '\n')
        output.write('Channel idle fraction (in percentage) ' + str(round(percent_idle, 3)) + '\n')
        output.write('Total number of collisions ' + str(collision_count) + '\n')
        output.write('Variance in number of successful transmissions (across all nodes) ' + str(round(variance_success, 3)) + '\n')
        output.write('Variance in number of collisions (across all nodes) ' + str(round(variance_collision, 3)) + '\n')


if __name__ == '__main__':
    main()
