#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define MAX_QUEUE_SIZE 10
#define MAX_LINE_LENGTH 20
#define MAIN_FONT "/usr/share/fonts/TTF/DejaVuSans.ttf"
#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 800
#define SCALE 1
#define ROAD_WIDTH 150
#define LANE_WIDTH 50
#define ARROW_SIZE 15

const char *VEHICLE_FILE = "vehicles.data";
// queue starts
typedef struct {
  char vehicleNumber[10];
  char road;
  time_t arrivalTime;
} Vehicle;

typedef struct {
  Vehicle *items[MAX_QUEUE_SIZE];
  int front;
  int rear;
  int size;
} Queue;

typedef struct {
  int laneId;
  int priority;
  int vehicleCount;
} LaneInfo;

typedef struct {
  LaneInfo lanes[4];
  int size;
} PriorityQueue;

typedef struct {
  int currentLight;
  int nextLight;
  bool stopSimulation;
} SharedData;

void displayText(SDL_Renderer *renderer, TTF_Font *font, char *text, int x,
                 int y);

Queue *createQueue() {
  Queue *q = (Queue *)malloc(sizeof(Queue));
  if (!q) {
    printf("Error: Failed to allocate memory for queue\n");
    return NULL;
  }
  q->front = 0;
  q->rear = -1;
  q->size = 0;
  return q;
}

int enqueue(Queue *q, Vehicle *v) {
  if (!q || !v)
    return -1;

  if (q->size >= MAX_QUEUE_SIZE) {
    printf("Warning: Queue is full, cannot add vehicle %s\n", v->vehicleNumber);
    return -1;
  }

  q->rear = (q->rear + 1) % MAX_QUEUE_SIZE;
  q->items[q->rear] = v;
  q->size++;
  return 0;
}

Vehicle *dequeue(Queue *q) {
  if (!q || q->size == 0) {
    return NULL;
  }

  Vehicle *v = q->items[q->front];
  q->front = (q->front + 1) % MAX_QUEUE_SIZE;
  q->size--;
  return v;
}
int isEmpty(Queue *q) { return (q == NULL || q->size == 0); }

// Get queue size
int getSize(Queue *q) { return (q == NULL) ? 0 : q->size; }
Vehicle *peek(Queue *q) {
  if (isEmpty(q))
    return NULL;
  return q->items[q->front];
}
void freeQueue(Queue *q) {
  if (!q)
    return;

  while (!isEmpty(q)) {
    Vehicle *v = dequeue(q);
    free(v);
  }
  free(q);
}

PriorityQueue *createPriorityQueue() {
  PriorityQueue *pq = (PriorityQueue *)malloc(sizeof(PriorityQueue));
  if (!pq) {
    printf("Error: Failed to allocate memory for priority queue\n");
    return NULL;
  }

  pq->size = 4;

  // Initialize all lanes with normal priority
  for (int i = 0; i < 4; i++) {
    pq->lanes[i].laneId = i;
    pq->lanes[i].priority = 0; // Normal priority
    pq->lanes[i].vehicleCount = 0;
  }

  return pq;
}

// Update priority based on vehicle count
void updatePriority(PriorityQueue *pq, int laneId, int count) {
  if (!pq || laneId < 0 || laneId >= 4)
    return;

  pq->lanes[laneId].vehicleCount = count;

  // Special logic for AL2 (lane 0) - priority lane
  if (laneId == 0) {
    if (count > 7) {               // Trigger at 8 (near capacity of 10)
      pq->lanes[0].priority = 100; // High priority
      printf(">>> PRIORITY MODE ACTIVATED: AL2 has %d vehicles\n", count);
    } else if (count < 4) {
      pq->lanes[0].priority = 0; // Back to normal
      if (pq->lanes[0].priority == 100) {
        printf(">>> PRIORITY MODE DEACTIVATED: AL2 has %d vehicles\n", count);
      }
    }
    // Between 5 and 10, maintain current priority
  } else {
    // Other lanes always have normal priority
    pq->lanes[laneId].priority = 0;
  }
}

// Get the next lane to serve based on priority
int getNextLane(PriorityQueue *pq) {
  if (!pq)
    return 0;

  int maxPriority = -1;
  int selectedLane = -1;

  // Find lane with highest priority that has vehicles waiting
  for (int i = 0; i < 4; i++) {
    if (pq->lanes[i].vehicleCount > 0 && pq->lanes[i].priority > maxPriority) {
      maxPriority = pq->lanes[i].priority;
      selectedLane = i;
    }
  }

  // If no priority lane, use round-robin on lanes with vehicles
  if (selectedLane == -1) {
    for (int i = 0; i < 4; i++) {
      if (pq->lanes[i].vehicleCount > 0) {
        selectedLane = i;
        break;
      }
    }
  }

  return selectedLane;
}

Queue *queueA = NULL;
Queue *queueB = NULL;
Queue *queueC = NULL;
Queue *queueD = NULL;

PriorityQueue *lanePriorityQueue = NULL;

pthread_mutex_t queueMutex;

void freePriorityQueue(PriorityQueue *pq) {
  if (pq)
    free(pq);
}

void printMessageHelper(const char *message, int count) {
  for (int i = 0; i < count; i++)
    printf("%s\n", message);
}

//---------------------------------------------------------------------------GRAPHICS--------------------------------------------------------------------//

bool initializeSDL(SDL_Window **window, SDL_Renderer **renderer) {
  if (SDL_Init(SDL_INIT_VIDEO) < 0) {
    SDL_Log("Failed to initialize SDL: %s", SDL_GetError());
    return false;
  }
  // font init
  if (TTF_Init() < 0) {
    SDL_Log("SDL_ttf could not initialize! TTF_Error: %s\n", TTF_GetError());
    return false;
  }

  *window = SDL_CreateWindow("Junction Diagram", SDL_WINDOWPOS_CENTERED,
                             SDL_WINDOWPOS_CENTERED, WINDOW_WIDTH * SCALE,
                             WINDOW_HEIGHT * SCALE, SDL_WINDOW_SHOWN);
  if (!*window) {
    SDL_Log("Failed to create window: %s", SDL_GetError());
    SDL_Quit();
    return false;
  }

  *renderer = SDL_CreateRenderer(*window, -1, SDL_RENDERER_ACCELERATED);
  // if you have high resolution monitor 2K or 4K then scale
  SDL_RenderSetScale(*renderer, SCALE, SCALE);

  if (!*renderer) {
    SDL_Log("Failed to create renderer: %s", SDL_GetError());
    SDL_DestroyWindow(*window);
    TTF_Quit();
    SDL_Quit();
    return false;
  }

  return true;
}

// graphics of the roads and lanes
void swap(int *a, int *b) {
  int temp = *a;
  *a = *b;
  *b = temp;
}

void drawArrow(SDL_Renderer *renderer, int x1, int y1, int x2, int y2, int x3,
               int y3) {
  // Sort vertices by ascending Y (bubble sort approach)
  if (y1 > y2) {
    swap(&y1, &y2);
    swap(&x1, &x2);
  }
  if (y1 > y3) {
    swap(&y1, &y3);
    swap(&x1, &x3);
  }
  if (y2 > y3) {
    swap(&y2, &y3);
    swap(&x2, &x3);
  }

  // Compute slopes
  float dx1 = (y2 - y1) ? (float)(x2 - x1) / (y2 - y1) : 0;
  float dx2 = (y3 - y1) ? (float)(x3 - x1) / (y3 - y1) : 0;
  float dx3 = (y3 - y2) ? (float)(x3 - x2) / (y3 - y2) : 0;

  float sx1 = x1, sx2 = x1;

  // Fill first part (top to middle)
  for (int y = y1; y < y2; y++) {
    SDL_RenderDrawLine(renderer, (int)sx1, y, (int)sx2, y);
    sx1 += dx1;
    sx2 += dx2;
  }

  sx1 = x2;

  // Fill second part (middle to bottom)
  for (int y = y2; y <= y3; y++) {
    SDL_RenderDrawLine(renderer, (int)sx1, y, (int)sx2, y);
    sx1 += dx3;
    sx2 += dx2;
  }
}

void drawLightForRoad(SDL_Renderer *renderer, int road, bool isGreen) {
  int boxX, boxY;

  // Position lights based on road
  switch (road) {
  case 0: // Road A (top)
    boxX = 400;
    boxY = 280;
    break;
  case 1: // Road B (bottom)
    boxX = 350;
    boxY = 490;
    break;
  case 2: // Road C (right)
    boxX = 490;
    boxY = 375;
    break;
  case 3: // Road D (left)
    boxX = 260;
    boxY = 425;
    break;
  default:
    return;
  }

  // Draw light box
  SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255);
  SDL_Rect lightBox = {boxX, boxY, 50, 30};
  SDL_RenderFillRect(renderer, &lightBox);

  // Draw light color
  if (isGreen) {
    SDL_SetRenderDrawColor(renderer, 11, 156, 50, 255); // Green
  } else {
    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255); // Red
  }

  SDL_Rect light = {boxX + 5, boxY + 5, 20, 20};
  SDL_RenderFillRect(renderer, &light);

  // Draw arrow indicator
  if (isGreen) {
    drawArrow(renderer, boxX + 35, boxY + 5, boxX + 35, boxY + 25, boxX + 45,
              boxY + 15);
  }
}

void drawRoadsAndLane(SDL_Renderer *renderer, TTF_Font *font) {
  // Draw gray roads
  SDL_SetRenderDrawColor(renderer, 80, 80, 80, 255);

  // Vertical road
  SDL_Rect verticalRoad = {WINDOW_WIDTH / 2 - ROAD_WIDTH / 2, 0, ROAD_WIDTH,
                           WINDOW_HEIGHT};
  SDL_RenderFillRect(renderer, &verticalRoad);

  // Horizontal road
  SDL_Rect horizontalRoad = {0, WINDOW_HEIGHT / 2 - ROAD_WIDTH / 2,
                             WINDOW_WIDTH, ROAD_WIDTH};
  SDL_RenderFillRect(renderer, &horizontalRoad);

  // Draw lane dividers
  SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
  for (int i = 0; i <= 3; i++) {
    // Horizontal lanes
    SDL_RenderDrawLine(renderer, 0,
                       WINDOW_HEIGHT / 2 - ROAD_WIDTH / 2 + LANE_WIDTH * i,
                       WINDOW_WIDTH / 2 - ROAD_WIDTH / 2,
                       WINDOW_HEIGHT / 2 - ROAD_WIDTH / 2 + LANE_WIDTH * i);
    SDL_RenderDrawLine(renderer, WINDOW_WIDTH,
                       WINDOW_HEIGHT / 2 - ROAD_WIDTH / 2 + LANE_WIDTH * i,
                       WINDOW_WIDTH / 2 + ROAD_WIDTH / 2,
                       WINDOW_HEIGHT / 2 - ROAD_WIDTH / 2 + LANE_WIDTH * i);

    // Vertical lanes
    SDL_RenderDrawLine(renderer,
                       WINDOW_WIDTH / 2 - ROAD_WIDTH / 2 + LANE_WIDTH * i, 0,
                       WINDOW_WIDTH / 2 - ROAD_WIDTH / 2 + LANE_WIDTH * i,
                       WINDOW_HEIGHT / 2 - ROAD_WIDTH / 2);
    SDL_RenderDrawLine(
        renderer, WINDOW_WIDTH / 2 - ROAD_WIDTH / 2 + LANE_WIDTH * i,
        WINDOW_HEIGHT, WINDOW_WIDTH / 2 - ROAD_WIDTH / 2 + LANE_WIDTH * i,
        WINDOW_HEIGHT / 2 + ROAD_WIDTH / 2);
  }

  // Draw road labels
  if (font) {
    displayText(renderer, font, "A (Priority)", 350, 30);
    displayText(renderer, font, "B", 380, 740);
    displayText(renderer, font, "C", 720, 380);
    displayText(renderer, font, "D", 30, 380);
  }
}

void displayText(SDL_Renderer *renderer, TTF_Font *font, char *text, int x,
                 int y) {
  if (!font)
    return;

  SDL_Color textColor = {0, 0, 0, 255};
  SDL_Surface *textSurface = TTF_RenderText_Solid(font, text, textColor);
  if (!textSurface)
    return;

  SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, textSurface);
  SDL_FreeSurface(textSurface);

  if (!texture)
    return;

  SDL_Rect textRect = {x, y, 0, 0};
  SDL_QueryTexture(texture, NULL, NULL, &textRect.w, &textRect.h);
  SDL_RenderCopy(renderer, texture, NULL, &textRect);
  SDL_DestroyTexture(texture);
}

// edited part
void drawQueueInfo(SDL_Renderer *renderer, TTF_Font *font) {
  if (!font)
    return;

  char buffer[100];

  pthread_mutex_lock(&queueMutex);

  int countA = getSize(queueA);
  int countB = getSize(queueB);
  int countC = getSize(queueC);
  int countD = getSize(queueD);

  pthread_mutex_unlock(&queueMutex);

  // Draw semi-transparent background for info panel
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
  SDL_SetRenderDrawColor(renderer, 240, 240, 240, 200);
  SDL_Rect infoPanel = {10, 10, 180, 140};
  SDL_RenderFillRect(renderer, &infoPanel);

  // Draw border
  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
  SDL_RenderDrawRect(renderer, &infoPanel);

  // Display queue counts
  snprintf(buffer, sizeof(buffer), "Road A: %d", countA);
  displayText(renderer, font, buffer, 20, 20);

  snprintf(buffer, sizeof(buffer), "Road B: %d", countB);
  displayText(renderer, font, buffer, 20, 50);

  snprintf(buffer, sizeof(buffer), "Road C: %d", countC);
  displayText(renderer, font, buffer, 20, 80);

  snprintf(buffer, sizeof(buffer), "Road D: %d", countD);
  displayText(renderer, font, buffer, 20, 110);

  // Show priority status
  if (countA > 7) {
    SDL_SetRenderDrawColor(renderer, 255, 200, 200, 200);
    SDL_Rect priorityIndicator = {10, 160, 180, 30};
    SDL_RenderFillRect(renderer, &priorityIndicator);
    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
    SDL_RenderDrawRect(renderer, &priorityIndicator);
    displayText(renderer, font, "PRIORITY MODE", 20, 165);
  }
}

void drawVehicles(SDL_Renderer *renderer) {
  int carWidth = 20;
  int carHeight = 20;
  int gap = 5;

  // Stop lines positions (approximate based on road width 150)
  int centerX = WINDOW_WIDTH / 2;
  int centerY = WINDOW_HEIGHT / 2;
  int offset = ROAD_WIDTH / 2 +
               10; // Start drawing slightly away from intersection center

  pthread_mutex_lock(&queueMutex);

  // Draw Road A (Top) - Queue builds upwards
  for (int i = 0; i < getSize(queueA); i++) {
    SDL_SetRenderDrawColor(renderer, 0, 0, 255, 255); // Blue
    int yPos = centerY - offset - (i * (carHeight + gap));
    // Clamp to screen
    if (yPos > -carHeight) {
      SDL_Rect car = {centerX - 15, yPos, carWidth, carHeight};
      SDL_RenderFillRect(renderer, &car);
    }
  }

  // Draw Road B (Bottom) - Queue builds downwards
  for (int i = 0; i < getSize(queueB); i++) {
    SDL_SetRenderDrawColor(renderer, 0, 0, 255, 255); // Blue
    int yPos = centerY + offset + (i * (carHeight + gap));
    if (yPos < WINDOW_HEIGHT) {
      SDL_Rect car = {centerX - 15, yPos, carWidth, carHeight};
      SDL_RenderFillRect(renderer, &car);
    }
  }

  // Draw Road C (Right) - Queue builds rightwards
  for (int i = 0; i < getSize(queueC); i++) {
    SDL_SetRenderDrawColor(renderer, 0, 0, 255, 255); // Blue
    int xPos = centerX + offset + (i * (carWidth + gap));
    if (xPos < WINDOW_WIDTH) {
      SDL_Rect car = {xPos, centerY - 15, carHeight, carWidth}; // Rotated
      SDL_RenderFillRect(renderer, &car);
    }
  }

  // Draw Road D (Left) - Queue builds leftwards
  for (int i = 0; i < getSize(queueD); i++) {
    SDL_SetRenderDrawColor(renderer, 0, 0, 255, 255); // Blue
    int xPos = centerX - offset - (i * (carWidth + gap));
    if (xPos > -carWidth) {
      SDL_Rect car = {xPos, centerY - 15, carHeight, carWidth}; // Rotated
      SDL_RenderFillRect(renderer, &car);
    }
  }

  pthread_mutex_unlock(&queueMutex);
}

void refreshLight(SDL_Renderer *renderer, SharedData *sharedData) {
  // Always redraw lights to ensure they don't disappear on screen clear
  // if (sharedData->nextLight == sharedData->currentLight) return;

  // Draw lights for all roads
  for (int i = 0; i < 4; i++) {
    bool isGreen = (sharedData->nextLight == i + 1);
    drawLightForRoad(renderer, i, isGreen);
  }

  sharedData->currentLight = sharedData->nextLight;
}
// edited part
// edited part
void *checkQueue(void *arg) {
  SharedData *sharedData = (SharedData *)arg;

  printf("Traffic processing thread started\n");

  while (!sharedData->stopSimulation) {
    pthread_mutex_lock(&queueMutex);
    int countA = getSize(queueA);
    int countB = getSize(queueB);
    int countC = getSize(queueC);
    int countD = getSize(queueD);
    pthread_mutex_unlock(&queueMutex);

    // 1. Check AL2 (Road A) Priority
    if (countA > 7) { // Adjusted threshold for max capacity 10
      printf("\n>>> PRIORITY MODE ACTIVATED: AL2 has %d vehicles (>7)\n",
             countA);

      // Switch Light to A
      sharedData->nextLight = 1; // 1=A
      sleep(1);                  // Transition time

      while (countA >= 4 && !sharedData->stopSimulation) {
        // Serve vehicle from A
        pthread_mutex_lock(&queueMutex);
        if (!isEmpty(queueA)) {
          Vehicle *v = dequeue(queueA);
          printf("  >> Served Priority AL2: %s (Remaining: %d)\n",
                 v->vehicleNumber, getSize(queueA));
          free(v);
          countA = getSize(queueA); // Update local count
        } else {
          countA = 0;
        }
        updatePriority(lanePriorityQueue, 0, countA); // Keep UI updated
        pthread_mutex_unlock(&queueMutex);

        usleep(750000); // Simulate time per vehicle (0.75s)
      }
      printf("<<< PRIORITY MODE ENDED: AL2 count dropped to %d (<4)\n", countA);

      sharedData->nextLight = 0; // Red
      sleep(1);
    }

    // 2. Normal Condition
    else {
      // Calculate average from B, C, D
      int average = (countB + countC + countD) / 3;
      // Ensure at least 1 vehicle is served if queues are not empty but average
      // is low due to integer division
      if (average < 1)
        average = 1;

      // printf("\n--- Normal Mode (Avg Quantum: %d) ---\n", average);

      // Serve each lane (A, B, C, D) in round robin
      Queue *queues[] = {queueA, queueB, queueC, queueD};
      // char roadIds[] = {'A', 'B', 'C', 'D'};

      bool anyServed = false;

      for (int i = 0; i < 4; i++) {
        if (sharedData->stopSimulation)
          break;

        pthread_mutex_lock(&queueMutex);
        int currentSize = getSize(queues[i]);
        pthread_mutex_unlock(&queueMutex);

        if (currentSize > 0) {
          anyServed = true;
          // Switch Light
          sharedData->nextLight = i + 1; // 1=A, 2=B...
          sleep(1);

          int servedCount = 0;
          // Serve 'average' number of vehicles or until empty
          while (servedCount < average) {
            pthread_mutex_lock(&queueMutex);
            if (!isEmpty(queues[i])) {
              Vehicle *v = dequeue(queues[i]);
              // printf("  - Normal Served Road %c: %s\n", roadIds[i],
              // v->vehicleNumber);
              free(v);
              servedCount++;
              // Update UI priority/counts
              updatePriority(lanePriorityQueue, i, getSize(queues[i]));
            } else {
              pthread_mutex_unlock(&queueMutex);
              break;
            }
            pthread_mutex_unlock(&queueMutex);
            usleep(750000); // Service rate (0.75s)
          }

          sharedData->nextLight = 0; // Red
          sleep(1);
        }
      }

      // If no vehicles in any lane, just wait a bit
      if (!anyServed) {
        sleep(1);
      }
    }
  }

  printf("Traffic processing thread stopped\n");
  return NULL;
}

// file reading (edited part)
// file reading (edited part)
void *readAndParseFile(void *arg) {
  (void)arg; // Suppress unused parameter warning
  printf("File reading thread started\n");
  printf("Monitoring file: %s\n", VEHICLE_FILE);

  long lastFileSize = 0;

  // Start reading from the END of the file to ignore old history
  // This ensures queues start empty (0) and only new vehicles appear
  FILE *initialFile = fopen(VEHICLE_FILE, "r");
  if (initialFile) {
    fseek(initialFile, 0, SEEK_END);
    lastFileSize = ftell(initialFile);
    fclose(initialFile);
  }

  // Seed random for interval variation
  srand(time(NULL) + 1);

  while (!((SharedData *)arg)->stopSimulation) {
    // Check if file exists and has new data
    FILE *file = fopen(VEHICLE_FILE, "r");
    if (!file) {
      // Check less frequently if file doesn't exist yet
      sleep(1);
      continue;
    }

    // Get file size
    fseek(file, 0, SEEK_END);
    long fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);

    // If file has new data, read from last position
    if (fileSize > lastFileSize) {
      fseek(file, lastFileSize, SEEK_SET);

      char line[MAX_LINE_LENGTH];
      while (fgets(line, sizeof(line), file)) {
        // Remove newline
        line[strcspn(line, "\n")] = 0;

        if (strlen(line) == 0)
          continue;

        // Parse: "VEHICLEID:LANE" -> "AA1BB234:A"
        char *vehicleNumber = strtok(line, ":");
        char *roadStr = strtok(NULL, ":");

        if (vehicleNumber && roadStr) {
          // Create new vehicle
          Vehicle *v = (Vehicle *)malloc(sizeof(Vehicle));
          if (v) {
            strncpy(v->vehicleNumber, vehicleNumber, 9);
            v->vehicleNumber[9] = '\0';
            v->road = roadStr[0];
            v->arrivalTime = time(NULL);

            // Add to appropriate queue
            pthread_mutex_lock(&queueMutex);
            int result = -1;
            switch (v->road) {
            case 'A':
              result = enqueue(queueA, v);
              break;
            case 'B':
              result = enqueue(queueB, v);
              break;
            case 'C':
              result = enqueue(queueC, v);
              break;
            case 'D':
              result = enqueue(queueD, v);
              break;
            default:
              printf("Warning: Unknown road '%c' for vehicle %s\n", v->road,
                     v->vehicleNumber);
              free(v);
              v = NULL;
            }
            pthread_mutex_unlock(&queueMutex);

            if (result == 0 && v) {
              printf("+ Vehicle %s added to Road %c queue\n", v->vehicleNumber,
                     v->road);
            } else if (v) {
              // Queue full or error
              free(v);
            }
          }
        }
      }

      lastFileSize = fileSize;
    }

    // Check for read errors
    if (ferror(file)) {
      perror("Error reading vehicle file");
      clearerr(file);
    }

    fclose(file);

    // 1-2 seconds interval
    int sleepTime = 1 + rand() % 2;
    sleep(sleepTime);
  }

  return NULL;
}

int main() {
  pthread_t tQueue, tReadFile;
  SDL_Window *window = NULL;
  SDL_Renderer *renderer = NULL;
  SDL_Event event;

  // Initialize SDL
  if (!initializeSDL(&window, &renderer)) {
    return -1;
  }

  // Initialize mutex
  pthread_mutex_init(&queueMutex, NULL);

  // Initialize queues
  queueA = createQueue();
  queueB = createQueue();
  queueC = createQueue();
  queueD = createQueue();
  lanePriorityQueue = createPriorityQueue();

  if (!queueA || !queueB || !queueC || !queueD || !lanePriorityQueue) {
    printf("Error: Failed to create queues\n");
    return -1;
  }

  printf("=== Traffic Junction Simulator Started ===\n");
  printf("Waiting for vehicles from traffic generator...\n\n");

  // Shared data for light control
  SharedData sharedData = {0, 0, false}; // Start with all lights red

  // Load font
  TTF_Font *font = TTF_OpenFont(MAIN_FONT, 24);
  if (!font) {
    printf("Warning: Failed to load font: %s\n", TTF_GetError());
  }

  // Initial render
  SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
  SDL_RenderClear(renderer);
  drawRoadsAndLane(renderer, font);
  SDL_RenderPresent(renderer);

  // Create worker threads
  pthread_create(&tQueue, NULL, checkQueue, &sharedData);
  pthread_create(&tReadFile, NULL, readAndParseFile, &sharedData);

  // Main UI thread - rendering loop
  bool running = true;
  while (running) {
    // Clear and redraw
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderClear(renderer);

    // Draw all elements
    drawRoadsAndLane(renderer, font);
    drawVehicles(renderer);
    refreshLight(renderer, &sharedData);
    drawQueueInfo(renderer, font);

    // Update display
    SDL_RenderPresent(renderer);

    // Handle events
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_QUIT) {
        running = false;
        sharedData.stopSimulation = true;
      }
    }

    SDL_Delay(16); // ~60 FPS
  }

  // Cleanup
  printf("\nShutting down simulator...\n");
  sharedData.stopSimulation = true;

  // Wait for threads to finish
  pthread_join(tReadFile, NULL);
  pthread_join(tQueue, NULL);

  pthread_mutex_destroy(&queueMutex);

  freeQueue(queueA);
  freeQueue(queueB);
  freeQueue(queueC);
  freeQueue(queueD);
  freePriorityQueue(lanePriorityQueue);

  if (font)
    TTF_CloseFont(font);
  if (renderer)
    SDL_DestroyRenderer(renderer);
  if (window)
    SDL_DestroyWindow(window);

  TTF_Quit();
  SDL_Quit();

  printf("Simulator stopped.\n");
  return 0;
}
