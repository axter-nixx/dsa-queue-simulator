#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#define FILENAME "vehicles.data"
#define MAX_LINES 5000         
#define MAX_LINE_LENGTH 64
#define TRIM_INTERVAL 1000      

//this will generate the vehicle number plate
void generateVehicleNumber(char* buffer) {
    buffer[0] = 'A' + rand() % 26;
    buffer[1] = 'A' + rand() % 26;
    buffer[2] = '0' + rand() % 10;
    buffer[3] = 'A' + rand() % 26;
    buffer[4] = 'A' + rand() % 26;
    buffer[5] = '0' + rand() % 10;
    buffer[6] = '0' + rand() % 10;
    buffer[7] = '0' + rand() % 10;
    buffer[8] = '\0';
}


void pickRoadLane(char* road, int* lane) {
    const char roads[] = {'A', 'B', 'C', 'D'};
    *road = roads[rand() % 4];
    *lane = rand() % 3;  
}

// ---------------- Trim File Function ----------------
void trimFile(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) return;

    char lines[MAX_LINES][MAX_LINE_LENGTH];
    int count = 0;

    while (fgets(lines[count % MAX_LINES], MAX_LINE_LENGTH, file))
        count++;

    fclose(file);

    if (count <= MAX_LINES) return;

    file = fopen(filename, "w");
    if (!file) return;

    int start = count % MAX_LINES;
    for (int i = 0; i < MAX_LINES; i++) {
        fputs(lines[(start + i) % MAX_LINES], file);
    }

    fclose(file);
}

// ---------------- Main Generator ----------------
int main() {
    FILE* file = fopen(FILENAME, "a");
    if (!file) {
        perror("Error opening file");
        return 1;
    }

    srand(time(NULL));

    int vehicleCount = 0;

    printf("=== Traffic Generator Started ===\n");

    while (1) {
        // Decide number of vehicles to generate this tick
        int burstSize = 1 + rand() % 3;   // normal 1-3 vehicles
        if (rand() % 100 < 20)            // 20% chance of burst
            burstSize = 5 + rand() % 8;   // burst 5-12 vehicles

        for (int i = 0; i < burstSize; i++) {
            char plate[9];
            char road;
            int lane;

            generateVehicleNumber(plate);
            pickRoadLane(&road, &lane);

            // Bias AL2 traffic (road A, lane 1)
            if (road == 'A' && lane == 1 && (rand() % 100 < 30))
                lane = 1; // ensure some congestion on AL2

            // Write vehicle to file
            fprintf(file, "%s:%c:%d\n", plate, road, lane);
            fflush(file);

            printf("Generated: %s:%c:%d\n", plate, road, lane);

            vehicleCount++;
            if (vehicleCount % TRIM_INTERVAL == 0) {
                fclose(file);
                trimFile(FILENAME);
                file = fopen(FILENAME, "a");
                if (!file) return 1;
            }
        }

        // Random delay between vehicle batches
        int delayMs = 150 + rand() % 550;  // 150-700ms
        if (rand() % 100 < 10) delayMs = 30; // occasional near-immediate
        usleep(delayMs * 1000); // convert ms -> us
    }

    fclose(file);
    return 0;
}

