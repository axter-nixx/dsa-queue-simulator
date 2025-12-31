#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define FILENAME "vehicles.data"
#define MIN_SLEEP_SEC 1
#define MAX_SLEEP_SEC 2

// Function to generate a random vehicle number
// Format: 2 letters + 1 digit + 2 letters + 3 digits (e.g., AA1BB234)
void generateVehicleNumber(char *buffer) {
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

// Function to generate a random lane
char generateLane() {
  char lanes[] = {'A', 'B', 'C', 'D'};
  return lanes[rand() % 4];
}

int main() {
  srand(time(NULL)); // Initialize random seed

  // Clear file initially
  FILE *file = fopen(FILENAME, "w");
  if (file) {
    fclose(file);
    printf("Initialized %s\n", FILENAME);
  } else {
    perror("Error initializing file");
    return 1;
  }

  printf("Starting Traffic Generator...\n");
  printf("Traffic Generation Started with Varied Rates:\n");
  printf("  Road A: Every 1-2s (Fast)\n");
  printf("  Road B: Every 2-3s (Medium)\n");
  printf("  Road C: Every 3-5s (Slow)\n");
  printf("  Road D: Every 4-6s (Very Slow)\n");
  printf("Press Ctrl+C to stop.\n\n");

  // Track next generation time for each lane
  time_t nextTime[4];
  time_t now = time(NULL);

  for (int i = 0; i < 4; i++) {
    nextTime[i] = now + (rand() % 3); // Stagger start times
  }

  while (1) {
    now = time(NULL);
    bool generated = false;

    // Check each lane
    for (int i = 0; i < 4; i++) {
      if (now >= nextTime[i]) {
        // Generate for this lane
        char vehicle[9];
        generateVehicleNumber(vehicle);
        char laneIds[] = {'A', 'B', 'C', 'D'};
        char lane = laneIds[i];

        // Append to file
        file = fopen(FILENAME, "a");
        if (file) {
          fprintf(file, "%s:%c\n", vehicle, lane);
          fflush(file);
          fclose(file);
          printf("Generated: %s:%c\n", vehicle, lane);
          generated = true;
        } else {
          perror("Error opening file");
        }

        // Set next time based on specific lane rates
        // Faster generation to ensure vehicles "appear again" quickly
        int interval = 0;
        switch (lane) {
        case 'A':
          interval = 1;
          break; // 1s (Very Fast)
        case 'B':
          interval = 1 + rand() % 2;
          break; // 1-2s
        case 'C':
          interval = 1 + rand() % 3;
          break; // 1-3s
        case 'D':
          interval = 2 + rand() % 2;
          break; // 2-3s
        }
        nextTime[i] = now + interval;
      }
    }

    if (!generated) {
      usleep(100000); // Sleep 100ms to prevent CPU hogging
    }
  }

  return 0;
}
