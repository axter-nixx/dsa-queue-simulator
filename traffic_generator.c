#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>

// ================ Configuration ================
#define NUM_ROADS 4
#define NUM_LANES 3
#define MAX_VEHICLES 1000

// ================ Data Types ================
typedef enum {
    ROAD_A = 0,
    ROAD_B = 1,
    ROAD_C = 2,
    ROAD_D = 3
} RoadID;

typedef enum {
    LANE_INCOMING = 0,
    LANE_NORMAL = 1,
    LANE_FREE_LEFT = 2
} LaneType;

typedef struct {
    int id;
    RoadID road;
    LaneType lane;
    time_t arrival_time;
} Vehicle;

// ================ Communication with Simulator ================
// This file works independently but in a full integration,
// it would share data structures with simulator.c

// ================ Vehicle Generation Logic ================
void generate_random_vehicles(int* vehicle_id) {
    const double GEN_PROBABILITY = 0.4;  // 40% chance per second
    const int MAX_VEHICLES_PER_GEN = 3;
    
    srand(time(NULL));
    
    printf("=== Traffic Generator Started ===\n");
    printf("Generating vehicles with %.0f%% probability every second\n", GEN_PROBABILITY * 100);
    printf("Maximum %d vehicles per generation\n\n", MAX_VEHICLES_PER_GEN);
    
    int total_generated = 0;
    
    while (1) {
        // Generate vehicles for each road and lane
        for (int road = 0; road < NUM_ROADS; road++) {
            for (int lane = 0; lane < NUM_LANES; lane++) {
                // Check if we should generate vehicles
                if ((double)rand() / RAND_MAX < GEN_PROBABILITY) {
                    int num_vehicles = rand() % MAX_VEHICLES_PER_GEN + 1;
                    
                    for (int i = 0; i < num_vehicles; i++) {
                        Vehicle v = {
                            .id = (*vehicle_id)++,
                            .road = road,
                            .lane = lane,
                            .arrival_time = time(NULL)
                        };
                        
                        total_generated++;
                        
                        // In a real integration, this would enqueue to shared queues
                        printf("[GEN] Vehicle %d at Road %c Lane %d (Total: %d)\n", 
                               v.id, 'A' + road, lane + 1, total_generated);
                        
                        // Special message for AL2 priority condition
                        if (road == ROAD_A && lane == LANE_NORMAL) {
                            // Simulate occasional high traffic on AL2
                            if (rand() % 100 < 20) {  // 20% chance
                                printf("[PRIORITY] High traffic building on AL2\n");
                            }
                        }
                    }
                }
            }
        }
        
        // Display generation summary every 10 seconds
        static int seconds = 0;
        seconds++;
        if (seconds % 10 == 0) {
            printf("\n=== Generation Summary (Last 10 seconds) ===\n");
            printf("Total vehicles generated: %d\n", total_generated);
            printf("Estimated rate: %.1f vehicles/second\n\n", total_generated / (float)seconds);
        }
        
        // Sleep for 1 second
        sleep(1);
    }
}

// ===============



