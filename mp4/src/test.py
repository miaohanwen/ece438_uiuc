from csma import Node, Network
import csv
import time


def test_N(L, R0, M, T):

    utilization = []
    idle = []
    collisions = []

    r = [R0 * 2**i for i in range(7)]
    network = Network(5, L, r, M)
    for N in range(5, 501):
        print('Simulating Network: N={}, L={}, R0={}, M={}'.format(N, L, R0, M))
        network.resize(N)

        idle_ticks = 0
        utilization_ticks = 0
        collision_count = 0

        for t in range(T):
            if not network.busy:
                nodes_to_tx = network.get_tx_nodes()

                if len(nodes_to_tx) > 1:
                    collision_count += 1
                    network.handle_collision(nodes_to_tx)
                elif len(nodes_to_tx) == 1:
                    utilization_ticks += 1
                    network.transmit(nodes_to_tx[0])
                else:
                    idle_ticks += 1
            else:
                utilization_ticks += 1

            network.tick()

        utilization.append(utilization_ticks / T * 100)
        idle.append(idle_ticks / T * 100)
        collisions.append(collision_count)

    return (utilization, idle, collisions)


def main():

    # Default test parameters
    L = 20
    R0 = 8
    M = 6
    T = 50000

    # Variable test parameters
    L_list = [20,40,60,80,100]      # list of L values to test
    R_list = [1,2,4,8,16]           # list of initial R values to test


    csv_header = ['N', 'L', 'R0', 'M', 'T', 'Util', 'Idle', 'Collisions']

    start = start_default = time.time()

    util, idle, collision = test_N(L, R0, M, T)
    with open('out/default.csv', 'w') as csvfile:
        csvwriter = csv.writer(csvfile, delimiter=',')
        csvwriter.writerow(csv_header)
        for i in range(len(util)):
            csvwriter.writerow([i+5, L, R0, M, T, util[i], idle[i], collision[i]])

    start_r = end_default = time.time()

    for r0 in R_list:
        util, idle, collision = test_N(L, r0, M, T)
        with open('out/R_{}.csv'.format(r0), 'w') as csvfile:
            csvwriter = csv.writer(csvfile, delimiter=',')
            csvwriter.writerow(csv_header)
            for i in range(len(util)):
                csvwriter.writerow([i+5, L, r0, M, T, util[i], idle[i], collision[i]])

    start_l = end_r = time.time()

    for l in L_list:
        util, idle, collision = test_N(l, R0, M, T)
        with open('out/L_{}.csv'.format(l), 'w') as csvfile:
            csvwriter = csv.writer(csvfile, delimiter=',')
            csvwriter.writerow(csv_header)
            for i in range(len(util)):
                csvwriter.writerow([i+5, l, R0, M, T, util[i], idle[i], collision[i]])

    end = end_l = time.time()

    print('Runtime (default):', str(round(end_default - start_default, 5)))
    print('Runtime (R0):', str(round(end_r - start_r, 5)))
    print('Runtime (L):', str(round(end_l - start_l, 5)))
    print('Runtime (total):', str(round(end - start, 5)))


if __name__ == '__main__':
    main()
