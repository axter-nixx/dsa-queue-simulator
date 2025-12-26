#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdbool.h>
#include <pthread.h>
#include <unistd.h> 
#include <stdio.h> 
#include <string.h>

#define MAX_LINE_LENGTH 20
#define MAIN_FONT "/usr/share/fonts/TTF/DejaVuSans.ttf"
#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 800
#define SCALE 1
#define ROAD_WIDTH 150
#define LANE_WIDTH 50
#define ARROW_SIZE 15


const char* VEHICLE_FILE = "vehicles.data";

typedef struct {
    char vehicleNumber[10];
    char road;  
    time_t arrivalTime;
} Vehicle;


typedef struct {
    Vehicle* items[MAX_QUEUE_SIZE];
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

typedef struct{
    int currentLight;
    int nextLight;
} SharedData;

Queue* createQueue() {
    Queue* q = (Queue*)malloc(sizeof(Queue));
    if (!q) {
        printf("Error: Failed to allocate memory for queue\n");
        return NULL;
    }
    q->front = 0;
    q->rear = -1;
    q->size = 0;
    return q;
}

int enqueue(Queue* q, Vehicle* v) {
    if (!q || !v) return -1;
    
    if (q->size >= MAX_QUEUE_SIZE) {
        printf("Warning: Queue is full, cannot add vehicle %s\n", v->vehicleNumber);
        return -1;
    }
    
    q->rear = (q->rear + 1) % MAX_QUEUE_SIZE;
    q->items[q->rear] = v;
    q->size++;
    return 0;
}

Vehicle* dequeue(Queue* q) {
    if (!q || q->size == 0) {
        return NULL;
    }
    
    Vehicle* v = q->items[q->front];
    q->front = (q->front + 1) % MAX_QUEUE_SIZE;
    q->size--;
    return v;
}
int isEmpty(Queue* q) {
    return (q == NULL || q->size == 0);
}

// Get queue size
int getSize(Queue* q) {
    return (q == NULL) ? 0 : q->size;
}
Vehicle* peek(Queue* q) {
    if (isEmpty(q)) return NULL;
    return q->items[q->front];
}
void freeQueue(Queue* q) {
    if (!q) return;
    
    while (!isEmpty(q)) {
        Vehicle* v = dequeue(q);
        free(v);
    }
    free(q);
}

PriorityQueue* createPriorityQueue() {
    PriorityQueue* pq = (PriorityQueue*)malloc(sizeof(PriorityQueue));
    if (!pq) {
        printf("Error: Failed to allocate memory for priority queue\n");
        return NULL;
    }
    
    pq->size = 4;
    
    // Initialize all lanes with normal priority
    for (int i = 0; i < 4; i++) {
        pq->lanes[i].laneId = i;
        pq->lanes[i].priority = 0;  // Normal priority
        pq->lanes[i].vehicleCount = 0;
    }
    
    return pq;
}

// Update priority based on vehicle count
void updatePriority(PriorityQueue* pq, int laneId, int count) {
    if (!pq || laneId < 0 || laneId >= 4) return;
    
    pq->lanes[laneId].vehicleCount = count;
    
    // Special logic for AL2 (lane 0) - priority lane
    if (laneId == 0) {
        if (count > 10) {
            pq->lanes[0].priority = 100;  // High priority
            printf(">>> PRIORITY MODE ACTIVATED: AL2 has %d vehicles\n", count);
        } else if (count < 5) {
            pq->lanes[0].priority = 0;    // Back to normal
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
int getNextLane(PriorityQueue* pq) {
    if (!pq) return 0;
    
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

Queue* queueA = NULL;
Queue* queueB = NULL;
Queue* queueC = NULL;
Queue* queueD = NULL;

PriorityQueue* lanePriorityQueue = NULL;

pthread_mutex_t queueMutex;


void freePriorityQueue(PriorityQueue* pq) {
    if (pq) free(pq);
}

bool initializeSDL(SDL_Window **window, SDL_Renderer **renderer);
void drawRoadsAndLane(SDL_Renderer *renderer, TTF_Font *font);
void displayText(SDL_Renderer *renderer, TTF_Font *font, char *text, int x, int y);
void drawLightForB(SDL_Renderer* renderer, bool isRed);
void refreshLight(SDL_Renderer *renderer, SharedData* sharedData);
void* chequeQueue(void* arg);
void* readAndParseFile(void* arg);


void printMessageHelper(const char* message, int count) {
    for (int i = 0; i < count; i++) printf("%s\n", message);
}


int main() {
    pthread_t tQueue, tReadFile;
    SDL_Window* window = NULL;
    SDL_Renderer* renderer = NULL;    
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
    SharedData sharedData = { 0, 0, false }; // Start with all lights red
    
    // Load font
    TTF_Font* font = TTF_OpenFont(MAIN_FONT, 24);
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
    pthread_create(&tReadFile, NULL, readAndParseFile, NULL);

    // Main UI thread - rendering loop
    bool running = true;
    while (running) {
        // Clear and redraw
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderClear(renderer);
        
        // Draw all elements
        drawRoadsAndLane(renderer, font);
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
        
        SDL_Delay(100); // ~10 FPS for UI updates
    }
    
    // Cleanup
    printf("\nShutting down simulator...\n");
    sharedData.stopSimulation = true;
    
    pthread_mutex_destroy(&queueMutex);
    
    freeQueue(queueA);
    freeQueue(queueB);
    freeQueue(queueC);
    freeQueue(queueD);
    freePriorityQueue(lanePriorityQueue);
    
    if (font) TTF_CloseFont(font);
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window) SDL_DestroyWindow(window);
    
    TTF_Quit();
    SDL_Quit();
    
    printf("Simulator stopped.\n");
    return 0;
}

//graphics realted 

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


    *window = SDL_CreateWindow("Junction Diagram",
                               SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                               WINDOW_WIDTH*SCALE, WINDOW_HEIGHT*SCALE,
                               SDL_WINDOW_SHOWN);
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

//graphics of the roads and lanes
void swap(int *a, int *b) {
    int temp = *a;
    *a = *b;
    *b = temp;
}


void drawArrwow(SDL_Renderer* renderer, int x1, int y1, int x2, int y2, int x3, int y3) {
    // Sort vertices by ascending Y (bubble sort approach)
    if (y1 > y2) { swap(&y1, &y2); swap(&x1, &x2); }
    if (y1 > y3) { swap(&y1, &y3); swap(&x1, &x3); }
    if (y2 > y3) { swap(&y2, &y3); swap(&x2, &x3); }

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

void drawLightForRoad(SDL_Renderer* renderer, int road, bool isGreen) {
    int boxX, boxY;
    
    // Position lights based on road
    switch(road) {
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
        drawArrow(renderer, boxX + 35, boxY + 5, boxX + 35, boxY + 25, boxX + 45, boxY + 15);
    }
}


/*void drawLightForB(SDL_Renderer* renderer, bool isRed){
    // draw light box
    SDL_SetRenderDrawColor(renderer, 150, 150, 150, 255);
    SDL_Rect lightBox = {400, 300, 50, 30};
    SDL_RenderFillRect(renderer, &lightBox);
    // draw light
    if(isRed) SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255); // red
    else SDL_SetRenderDrawColor(renderer, 11, 156, 50, 255);    // green
    SDL_Rect straight_Light = {405, 305, 20, 20};
    SDL_RenderFillRect(renderer, &straight_Light);
    drawArrwow(renderer, 435,305, 435, 305+20, 435+10, 305+10);
}*/


void drawRoadsAndLane(SDL_Renderer *renderer, TTF_Font *font) {
    // Draw gray roads
    SDL_SetRenderDrawColor(renderer, 80, 80, 80, 255);
    
    // Vertical road
    SDL_Rect verticalRoad = {WINDOW_WIDTH / 2 - ROAD_WIDTH / 2, 0, ROAD_WIDTH, WINDOW_HEIGHT};
    SDL_RenderFillRect(renderer, &verticalRoad);

    // Horizontal road
    SDL_Rect horizontalRoad = {0, WINDOW_HEIGHT / 2 - ROAD_WIDTH / 2, WINDOW_WIDTH, ROAD_WIDTH};
    SDL_RenderFillRect(renderer, &horizontalRoad);
    
    // Draw lane dividers
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    for (int i = 0; i <= 3; i++) {
        // Horizontal lanes
        SDL_RenderDrawLine(renderer, 
            0, WINDOW_HEIGHT/2 - ROAD_WIDTH/2 + LANE_WIDTH*i,
            WINDOW_WIDTH/2 - ROAD_WIDTH/2, WINDOW_HEIGHT/2 - ROAD_WIDTH/2 + LANE_WIDTH*i
        );
        SDL_RenderDrawLine(renderer, 
            WINDOW_WIDTH, WINDOW_HEIGHT/2 - ROAD_WIDTH/2 + LANE_WIDTH*i,
            WINDOW_WIDTH/2 + ROAD_WIDTH/2, WINDOW_HEIGHT/2 - ROAD_WIDTH/2 + LANE_WIDTH*i
        );
        
        // Vertical lanes
        SDL_RenderDrawLine(renderer,
            WINDOW_WIDTH/2 - ROAD_WIDTH/2 + LANE_WIDTH*i, 0,
            WINDOW_WIDTH/2 - ROAD_WIDTH/2 + LANE_WIDTH*i, WINDOW_HEIGHT/2 - ROAD_WIDTH/2
        );
        SDL_RenderDrawLine(renderer,
            WINDOW_WIDTH/2 - ROAD_WIDTH/2 + LANE_WIDTH*i, WINDOW_HEIGHT,
            WINDOW_WIDTH/2 - ROAD_WIDTH/2 + LANE_WIDTH*i, WINDOW_HEIGHT/2 + ROAD_WIDTH/2
        );
    }
    
    // Draw road labels
    if (font) {
        displayText(renderer, font, "A (Priority)", 350, 30);
        displayText(renderer, font, "B", 380, 740);
        displayText(renderer, font, "C", 720, 380);
        displayText(renderer, font, "D", 30, 380);
    }
}


void displayText(SDL_Renderer *renderer, TTF_Font *font, char *text, int x, int y) {
    if (!font) return;
    
    SDL_Color textColor = {0, 0, 0, 255};
    SDL_Surface *textSurface = TTF_RenderText_Solid(font, text, textColor);
    if (!textSurface) return;
    
    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, textSurface);
    SDL_FreeSurface(textSurface);
    
    if (!texture) return;
    
    SDL_Rect textRect = {x, y, 0, 0};
    SDL_QueryTexture(texture, NULL, NULL, &textRect.w, &textRect.h);
    SDL_RenderCopy(renderer, texture, NULL, &textRect);
    SDL_DestroyTexture(texture);
}

//edited part
void drawQueueInfo(SDL_Renderer *renderer, TTF_Font *font) {
    if (!font) return;
    
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
    if (countA > 10) {
        SDL_SetRenderDrawColor(renderer, 255, 200, 200, 200);
        SDL_Rect priorityIndicator = {10, 160, 180, 30};
        SDL_RenderFillRect(renderer, &priorityIndicator);
        SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
        SDL_RenderDrawRect(renderer, &priorityIndicator);
        displayText(renderer, font, "PRIORITY MODE", 20, 165);
    }
}

void refreshLight(SDL_Renderer *renderer, SharedData* sharedData) {
    if (sharedData->nextLight == sharedData->currentLight) return;
    
    // Draw lights for all roads
    for (int i = 0; i < 4; i++) {
        bool isGreen = (sharedData->nextLight == i + 1);
        drawLightForRoad(renderer, i, isGreen);
    }
    
    sharedData->currentLight = sharedData->nextLight;
}
//edited part
void* checkQueue(void* arg) {
    SharedData* sharedData = (SharedData*)arg;
    int currentServing = 0; // Round robin counter
    
    printf("Traffic processing thread started\n");
    
    while (!sharedData->stopSimulation) {
        pthread_mutex_lock(&queueMutex);
        
        // Update priority queue with current counts
        updatePriority(lanePriorityQueue, 0, getSize(queueA));
        updatePriority(lanePriorityQueue, 1, getSize(queueB));
        updatePriority(lanePriorityQueue, 2, getSize(queueC));
        updatePriority(lanePriorityQueue, 3, getSize(queueD));
        
        int countA = getSize(queueA);
        int countB = getSize(queueB);
        int countC = getSize(queueC);
        int countD = getSize(queueD);
        int totalVehicles = countA + countB + countC + countD;
        
        pthread_mutex_unlock(&queueMutex);
        
        // If no vehicles, turn all lights red
        if (totalVehicles == 0) {
            sharedData->nextLight = 0; // All red
            sleep(2);
            continue;
        }
        
        // Check if priority lane (AL2) needs immediate service
        if (countA > 10) {
            printf("\n>>> PRIORITY MODE: Serving Road A (AL2) - %d vehicles waiting\n", countA);
            
            // Calculate vehicles to serve
            int vehiclesToServe = (countA > 5) ? (countA - 4) : 1;
            
            // Set green light for road A
            sharedData->nextLight = 1;
            sleep(2); // Light transition time
            
            // Serve vehicles from Road A
            pthread_mutex_lock(&queueMutex);
            for (int i = 0; i < vehiclesToServe && !isEmpty(queueA); i++) {
                Vehicle* v = dequeue(queueA);
                if (v) {
                    printf("  ✓ Served: %s from Road A (Priority)\n", v->vehicleNumber);
                    free(v);
                }
            }
            pthread_mutex_unlock(&queueMutex);
            
            sleep(3); // Green light duration
            continue;
        }
        
        // Normal mode - fair distribution
        printf("\n--- Normal Mode: Fair Distribution ---\n");
        
        // Calculate average vehicles to serve per lane
        int avgVehicles = (totalVehicles / 4) + 1;
        if (avgVehicles < 1) avgVehicles = 1;
        
        Queue* queues[] = {queueA, queueB, queueC, queueD};
        char roadNames[] = {'A', 'B', 'C', 'D'};
        
        // Serve each lane in round-robin fashion
        for (int i = 0; i < 4; i++) {
            int laneIndex = (currentServing + i) % 4;
            
            pthread_mutex_lock(&queueMutex);
            int queueSize = getSize(queues[laneIndex]);
            pthread_mutex_unlock(&queueMutex);
            
            if (queueSize > 0) {
                printf("Serving Road %c (%d vehicles waiting):\n", roadNames[laneIndex], queueSize);
                
                // Set green light
                sharedData->nextLight = laneIndex + 1;
                sleep(2); // Light transition
                
                // Serve vehicles
                int served = 0;
                pthread_mutex_lock(&queueMutex);
                for (int j = 0; j < avgVehicles && !isEmpty(queues[laneIndex]); j++) {
                    Vehicle* v = dequeue(queues[laneIndex]);
                    if (v) {
                        printf("  ✓ Served: %s from Road %c\n", v->vehicleNumber, roadNames[laneIndex]);
                        free(v);
                        served++;
                    }
                }
                pthread_mutex_unlock(&queueMutex);
                
                printf("  Total served from Road %c: %d\n", roadNames[laneIndex], served);
                
                sleep(3); // Green light duration
                currentServing = (laneIndex + 1) % 4;
                break; // Serve one lane per cycle
            }
        }
        
        // All lights red between cycles
        sharedData->nextLight = 0;
        sleep(1);
    }
    
    printf("Traffic processing thread stopped\n");
    return NULL;
}

// you may need to pass the queue on this function for sharing the data
void* readAndParseFile(void* arg) {
    while(1){ 
        FILE* file = fopen(VEHICLE_FILE, "r");
        if (!file) {
            perror("Error opening file");
            continue;
        }

        char line[MAX_LINE_LENGTH];
        while (fgets(line, sizeof(line), file)) {
            // Remove newline if present
            line[strcspn(line, "\n")] = 0;

            // Split using ':'
            char* vehicleNumber = strtok(line, ":");
            char* road = strtok(NULL, ":"); // read next item resulted from split

            if (vehicleNumber && road)  printf("Vehicle: %s, Raod: %s\n", vehicleNumber, road);
            else printf("Invalid format: %s\n", line);
        }
        fclose(file);
        sleep(2); // manage this time
    }
}
