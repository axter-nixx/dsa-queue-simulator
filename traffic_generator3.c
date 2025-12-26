#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <string.h>

#define FILENAME "vehicles.data"
#define MAX_VEHICLES_PER_BATCH 5
#define MIN_DELAY 1
#define MAX_DELAY 3

// Function to generate a random vehicle number
// Format: <2 letters><1 digit><2 letters><3 digits>
// Example: AA0BB123
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

// Function to generate a random lane/road
// Returns: 'A', 'B', 'C', or 'D'
char generateRoad() {
    char roads[] = {'A', 'B', 'C', 'D'};
    return roads[rand() % 4];
}

// Function to generate random road with weighted probability
// This can be used to test priority lane (give more weight to Road A)
char generateRoadWeighted(int priorityRoadWeight) {
    int total = 100;
    int rand_val = rand() % total;
    
    if (rand_val < priorityRoadWeight) {
        return 'A';  // Priority road gets more vehicles
    } else {
        char roads[] = {'B', 'C', 'D'};
        return roads[rand() % 3];
    }
}

// Function to clear the vehicle data file
void clearVehicleFile() {
    FILE* file = fopen(FILENAME, "w");
    if (file) {
        fclose(file);
        printf("Cleared existing vehicle data file.\n");
    }
}



int main(int argc, char* argv[]) {
    FILE* file = fopen(FILENAME, "a");
    if (!file) {
        perror("Error opening file");
        return 1;
    }

    srand(time(NULL)); // Initialize random seed
    
    // Clear existing data
    fclose(file);
    clearVehicleFile();
    file = fopen(FILENAME, "a");
    
    // Determine mode
    char mode[20] = "normal";
    if (argc > 1) {
        strncpy(mode, argv[1], sizeof(mode) - 1);
    }
    
    printf("\n=== Traffic Generator Started ===\n");
    printf("Mode: %s\n", mode);
    printf("Output file: %s\n", FILENAME);
    printf("Press Ctrl+C to stop\n");
    printf("================================\n\n");
    
    int vehicleCount = 0;
    
    // Normal mode - equal distribution
    if (strcmp(mode, "normal") == 0) {
        while (1) {
            char vehicle[9];
            generateVehicleNumber(vehicle);
            char road = generateRoad();

            fprintf(file, "%s:%c\n", vehicle, road);
            fflush(file);

            vehicleCount++;
            printf("[%4d] Generated: %s -> Road %c\n", vehicleCount, vehicle, road);

            sleep(rand() % (MAX_DELAY - MIN_DELAY + 1) + MIN_DELAY);
        }
    }
    
    // Priority mode - more vehicles on Road A
    else if (strcmp(mode, "priority") == 0) {
        printf("Generating 60%% vehicles on Road A (priority lane)...\n\n");
        
        while (1) {
            char vehicle[9];
            generateVehicleNumber(vehicle);
            char road = generateRoadWeighted(60); // 60% chance for Road A

            fprintf(file, "%s:%c\n", vehicle, road);
            fflush(file);

            vehicleCount++;
            printf("[%4d] Generated: %s -> Road %c", vehicleCount, vehicle, road);
            if (road == 'A') printf(" (Priority)");
            printf("\n");

            sleep(rand() % (MAX_DELAY - MIN_DELAY + 1) + MIN_DELAY);
        }
    }
    
    // Burst mode - generate vehicles in batches
    else if (strcmp(mode, "burst") == 0) {
        printf("Generating vehicles in bursts...\n\n");
        
        while (1) {
            int burstSize = rand() % MAX_VEHICLES_PER_BATCH + 2;
            printf("--- Burst of %d vehicles ---\n", burstSize);
            
            for (int i = 0; i < burstSize; i++) {
                char vehicle[9];
                generateVehicleNumber(vehicle);
                char road = generateRoad();

                fprintf(file, "%s:%c\n", vehicle, road);
                fflush(file);

                vehicleCount++;
                printf("[%4d] Generated: %s -> Road %c\n", vehicleCount, vehicle, road);
            }
            
            printf("Burst complete. Waiting...\n\n");
            sleep(5 + rand() % 5); // Wait 5-10 seconds between bursts
        }
    }
    
    // Custom mode - interactive
    else if (strcmp(mode, "custom") == 0) {
        int interval, priorityWeight;
        
        printf("Enter generation interval (seconds): ");
        scanf("%d", &interval);
        
        printf("Enter Road A priority weight (0-100, 25=equal, 60=high priority): ");
        scanf("%d", &priorityWeight);
        
        if (priorityWeight < 0) priorityWeight = 0;
        if (priorityWeight > 100) priorityWeight = 100;
        
        printf("\nGenerating with %d second interval, Road A weight=%d%%...\n\n", 
               interval, priorityWeight);
        
        while (1) {
            char vehicle[9];
            generateVehicleNumber(vehicle);
            char road = generateRoadWeighted(priorityWeight);

            fprintf(file, "%s:%c\n", vehicle, road);
            fflush(file);

            vehicleCount++;
            printf("[%4d] Generated: %s -> Road %c\n", vehicleCount, vehicle, road);

            sleep(interval);
        }
    }
    
    else {
        printf("Unknown mode: %s\n\n", mode);
        fclose(file);
        return 1;
    }

    fclose(file);
    return 0;
}
