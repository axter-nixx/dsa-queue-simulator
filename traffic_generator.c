#include "traffic_generator.h"
#include <stdlib.h>
#include <time.h>

/*
 Generates traffic for each road and lane.
 Incoming lanes get more vehicles.
 Priority lane AL2 may get heavy traffic.
*/

void generateTraffic(int traffic[ROADS][LANES]) {
    static int initialized = 0;

    if (!initialized) {
        srand(time(NULL));
        initialized = 1;
    }

    for (int r = 0; r < ROADS; r++) {
        for (int l = 0; l < LANES; l++) {

            if (r == 0 && l == 1) {
                // AL2 priority lane
                traffic[r][l] += rand() % 4;  // heavy inflow
            } else if (l == 0) {
                // incoming lane
                traffic[r][l] += rand() % 3;
            } else {
                // outgoing / free lane
                traffic[r][l] += rand() % 2;
            }

            if (traffic[r][l] > 50)
                traffic[r][l] = 50;
        }
    }
}

