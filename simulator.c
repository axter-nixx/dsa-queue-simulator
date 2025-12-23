#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <math.h>
#include <pthread.h>

// ================ Configuration Constants ================
#define NUM_ROADS 4
#define NUM_LANES 3
#define MAX_VEHICLES 1000
#define PRIORITY_THRESHOLD 10
#define PRIORITY_RESET 5
#define TIME_PER_VEHICLE 2  // seconds per vehicle

#define SCREEN_WIDTH 1200
#define SCREEN_HEIGHT 800
#define JUNCTION_X 500
#define JUNCTION_Y 300
#define JUNCTION_SIZE 200
#define LANE_LENGTH 300
#define LANE_WIDTH 40
#define VEHICLE_SIZE 20

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

typedef enum {
    LIGHT_RED = 1,
    LIGHT_GREEN = 2
} LightState;

typedef struct {
    int id;
    RoadID road;
    LaneType lane;
    time_t arrival_time;
    int x, y;  // Position for drawing
} Vehicle;

// ================ Queue Implementation ================
typedef struct QueueNode {
    Vehicle vehicle;
    struct QueueNode* next;
} QueueNode;

typedef struct {
    QueueNode* front;
    QueueNode* rear;
    int count;
    RoadID road;
    LaneType lane;
    pthread_mutex_t mutex;
} Queue;

Queue* create_queue(RoadID road, LaneType lane) {
    Queue* q = malloc(sizeof(Queue));
    q->front = q->rear = NULL;
    q->count = 0;
    q->road = road;
    q->lane = lane;
    pthread_mutex_init(&q->mutex, NULL);
    return q;
}

void enqueue(Queue* q, Vehicle v) {
    pthread_mutex_lock(&q->mutex);
    
    QueueNode* newNode = malloc(sizeof(QueueNode));
    newNode->vehicle = v;
    newNode->next = NULL;
    
    if (q->rear == NULL) {
        q->front = q->rear = newNode;
    } else {
        q->rear->next = newNode;
        q->rear = newNode;
    }
    q->count++;
    
    pthread_mutex_unlock(&q->mutex);
}

Vehicle dequeue(Queue* q) {
    pthread_mutex_lock(&q->mutex);
    
    if (q->front == NULL) {
        pthread_mutex_unlock(&q->mutex);
        Vehicle empty = {0};
        return empty;
    }
    
    QueueNode* temp = q->front;
    Vehicle v = temp->vehicle;
    
    q->front = q->front->next;
    
    if (q->front == NULL) {
        q->rear = NULL;
    }
    
    free(temp);
    q->count--;
    
    pthread_mutex_unlock(&q->mutex);
    return v;
}

int queue_size(Queue* q) {
    pthread_mutex_lock(&q->mutex);
    int size = q->count;
    pthread_mutex_unlock(&q->mutex);
    return size;
}

// ================ Priority Queue Implementation ================
typedef struct {
    RoadID road;
    int priority;  // Higher number = higher priority
    int vehicle_count;
    LightState light_state;
    time_t light_start_time;
    int green_duration;
} TrafficLight;

typedef struct PriorityQueueNode {
    TrafficLight data;
    struct PriorityQueueNode* next;
} PriorityQueueNode;

typedef struct {
    PriorityQueueNode* front;
    int size;
    pthread_mutex_t mutex;
} PriorityQueue;

PriorityQueue* create_priority_queue() {
    PriorityQueue* pq = malloc(sizeof(PriorityQueue));
    pq->front = NULL;
    pq->size = 0;
    pthread_mutex_init(&pq->mutex, NULL);
    return pq;
}

void pq_enqueue(PriorityQueue* pq, TrafficLight data) {
    pthread_mutex_lock(&pq->mutex);
    
    PriorityQueueNode* newNode = malloc(sizeof(PriorityQueueNode));
    newNode->data = data;
    newNode->next = NULL;
    
    // Insert based on priority (higher priority first)
    if (pq->front == NULL || data.priority > pq->front->data.priority) {
        newNode->next = pq->front;
        pq->front = newNode;
    } else {
        PriorityQueueNode* current = pq->front;
        while (current->next != NULL && 
               current->next->data.priority >= data.priority) {
            current = current->next;
        }
        newNode->next = current->next;
        current->next = newNode;
    }
    pq->size++;
    
    pthread_mutex_unlock(&pq->mutex);
}

TrafficLight pq_dequeue(PriorityQueue* pq) {
    pthread_mutex_lock(&pq->mutex);
    
    if (pq->front == NULL) {
        pthread_mutex_unlock(&pq->mutex);
        TrafficLight empty = {0};
        return empty;
    }
    
    PriorityQueueNode* temp = pq->front;
    TrafficLight data = temp->data;
    pq->front = pq->front->next;
    free(temp);
    pq->size--;
    
    pthread_mutex_unlock(&pq->mutex);
    return data;
}

void pq_update_priority(PriorityQueue* pq, RoadID road, int priority) {
    pthread_mutex_lock(&pq->mutex);
    
    // Remove the road and reinsert with new priority
    PriorityQueueNode* current = pq->front;
    PriorityQueueNode* prev = NULL;
    
    while (current != NULL) {
        if (current->data.road == road) {
            TrafficLight data = current->data;
            data.priority = priority;
            
            // Remove current node
            if (prev == NULL) {
                pq->front = current->next;
            } else {
                prev->next = current->next;
            }
            free(current);
            pq->size--;
            
            // Reinsert with new priority
            pthread_mutex_unlock(&pq->mutex);
            pq_enqueue(pq, data);
            return;
        }
        prev = current;
        current = current->next;
    }
    
    pthread_mutex_unlock(&pq->mutex);
}

TrafficLight* pq_get_front(PriorityQueue* pq) {
    pthread_mutex_lock(&pq->mutex);
    if (pq->front == NULL) {
        pthread_mutex_unlock(&pq->mutex);
        return NULL;
    }
    TrafficLight* data = &pq->front->data;
    pthread_mutex_unlock(&pq->mutex);
    return data;
}

// ================ Simulation Structures ================
typedef struct {
    SDL_Window* window;
    SDL_Renderer* renderer;
    TTF_Font* font;
    Queue* vehicle_queues[NUM_ROADS][NUM_LANES];
    PriorityQueue* light_queue;
    bool running;
    int simulation_speed;
    time_t simulation_time;
    int total_vehicles_served;
    int vehicle_id_counter;
    pthread_t generator_thread;
    bool generator_running;
} Simulator;

// ================ Global Variables ================
const SDL_Color road_colors[NUM_ROADS] = {
    {255, 0, 0, 255},      // Road A - Red
    {0, 255, 0, 255},      // Road B - Green
    {0, 0, 255, 255},      // Road C - Blue
    {255, 255, 0, 255}     // Road D - Yellow
};

const char* road_names[NUM_ROADS] = {"A", "B", "C", "D"};
Simulator* global_sim = NULL;

// ================ Vehicle Position Calculation ================
void calculate_vehicle_position(int* x, int* y, RoadID road, LaneType lane, int position) {
    int spacing = 25;
    int base_x = 0, base_y = 0;
    int offset_x = 0, offset_y = 0;
    
    switch (road) {
        case ROAD_A:  // Top road
            base_x = JUNCTION_X - LANE_LENGTH + 50;
            base_y = JUNCTION_Y - 60;
            offset_y = lane * 30;
            *x = base_x + (position * spacing);
            *y = base_y + offset_y;
            break;
        case ROAD_B:  // Right road
            base_x = JUNCTION_X + JUNCTION_SIZE + 50;
            base_y = JUNCTION_Y + 20;
            offset_x = lane * 30;
            *x = base_x + offset_x;
            *y = base_y + (position * spacing);
            break;
        case ROAD_C:  // Bottom road
            base_x = JUNCTION_X + LANE_LENGTH - 50;
            base_y = JUNCTION_Y + JUNCTION_SIZE + 20;
            offset_y = lane * 30;
            *x = base_x - (position * spacing);
            *y = base_y + offset_y;
            break;
        case ROAD_D:  // Left road
            base_x = JUNCTION_X - LANE_LENGTH + 50;
            base_y = JUNCTION_Y + JUNCTION_SIZE - 20;
            offset_x = lane * 30;
            *x = base_x + offset_x;
            *y = base_y - (position * spacing);
            break;
    }
}

// ================ Vehicle Generation Thread ================
void* generate_vehicles_thread(void* arg) {
    Simulator* sim = (Simulator*)arg;
    const double GEN_PROBABILITY = 0.3;
    const int MAX_VEHICLES_PER_GEN = 2;
    
    srand(time(NULL));
    
    while (sim->generator_running) {
        // Randomly generate vehicles
        for (int road = 0; road < NUM_ROADS; road++) {
            for (int lane = 0; lane < NUM_LANES; lane++) {
                if ((double)rand() / RAND_MAX < GEN_PROBABILITY) {
                    int num_vehicles = rand() % MAX_VEHICLES_PER_GEN + 1;
                    
                    for (int i = 0; i < num_vehicles; i++) {
                        Vehicle v = {
                            .id = sim->vehicle_id_counter++,
                            .road = road,
                            .lane = lane,
                            .arrival_time = time(NULL)
                        };
                        
                        calculate_vehicle_position(&v.x, &v.y, road, lane, 
                                                  queue_size(sim->vehicle_queues[road][lane]));
                        enqueue(sim->vehicle_queues[road][lane], v);
                    }
                }
            }
        }
        
        // Sleep for 1 second
        struct timespec ts = {1, 0};
        nanosleep(&ts, NULL);
    }
    
    return NULL;
}

// ================ Traffic Logic ================
int calculate_vehicles_to_serve(Simulator* sim) {
    int total_vehicles = 0;
    
    // Count vehicles in normal lanes (BL2, CL2, DL2)
    for (int road = 1; road < NUM_ROADS; road++) {
        total_vehicles += queue_size(sim->vehicle_queues[road][LANE_NORMAL]);
    }
    
    // Calculate average of 3 normal lanes
    return total_vehicles > 0 ? total_vehicles / 3 : 0;
}

void update_traffic_priorities(Simulator* sim) {
    // Check AL2 for priority
    int al2_count = queue_size(sim->vehicle_queues[ROAD_A][LANE_NORMAL]);
    
    if (al2_count > PRIORITY_THRESHOLD) {
        pq_update_priority(sim->light_queue, ROAD_A, 10);  // Highest priority
        printf("AL2 Priority Active: %d vehicles\n", al2_count);
    } else if (al2_count < PRIORITY_RESET) {
        pq_update_priority(sim->light_queue, ROAD_A, 1);   // Normal priority
    }
}

void process_vehicles(Simulator* sim, RoadID current_road) {
    // Serve vehicles from the current road's normal lane
    Queue* queue = sim->vehicle_queues[current_road][LANE_NORMAL];
    int vehicles_to_serve = calculate_vehicles_to_serve(sim);
    
    if (vehicles_to_serve > 0) {
        for (int i = 0; i < vehicles_to_serve && queue_size(queue) > 0; i++) {
            Vehicle v = dequeue(queue);
            sim->total_vehicles_served++;
            printf("Vehicle %d served from Road %c Lane 2\n", v.id, 'A' + current_road);
        }
    }
}

// ================ Graphics Functions ================
void draw_road(SDL_Renderer* renderer, RoadID road, LightState state) {
    SDL_Color color = road_colors[road];
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, 150);
    
    SDL_Rect road_rect;
    
    switch (road) {
        case ROAD_A:  // Top road (incoming from top)
            road_rect = (SDL_Rect){JUNCTION_X - LANE_LENGTH, JUNCTION_Y - LANE_WIDTH/2, 
                                  LANE_LENGTH * 2, LANE_WIDTH};
            break;
        case ROAD_B:  // Right road
            road_rect = (SDL_Rect){JUNCTION_X + JUNCTION_SIZE/2, JUNCTION_Y - LANE_LENGTH, 
                                  LANE_WIDTH, LANE_LENGTH * 2};
            break;
        case ROAD_C:  // Bottom road
            road_rect = (SDL_Rect){JUNCTION_X - LANE_LENGTH, JUNCTION_Y + JUNCTION_SIZE - LANE_WIDTH/2, 
                                  LANE_LENGTH * 2, LANE_WIDTH};
            break;
        case ROAD_D:  // Left road
            road_rect = (SDL_Rect){JUNCTION_X - LANE_WIDTH/2, JUNCTION_Y - LANE_LENGTH, 
                                  LANE_WIDTH, LANE_LENGTH * 2};
            break;
    }
    
    SDL_RenderFillRect(renderer, &road_rect);
    
    // Draw traffic light
    SDL_Color light_color = (state == LIGHT_GREEN) ? 
                           (SDL_Color){0, 255, 0, 255} : 
                           (SDL_Color){255, 0, 0, 255};
    
    SDL_SetRenderDrawColor(renderer, light_color.r, light_color.g, light_color.b, 255);
    
    int light_x, light_y;
    switch (road) {
        case ROAD_A:
            light_x = JUNCTION_X + JUNCTION_SIZE/2 - 10;
            light_y = JUNCTION_Y - 40;
            break;
        case ROAD_B:
            light_x = JUNCTION_X + JUNCTION_SIZE + 20;
            light_y = JUNCTION_Y + JUNCTION_SIZE/2 - 10;
            break;
        case ROAD_C:
            light_x = JUNCTION_X + JUNCTION_SIZE/2 - 10;
            light_y = JUNCTION_Y + JUNCTION_SIZE + 20;
            break;
        case ROAD_D:
            light_x = JUNCTION_X - 40;
            light_y = JUNCTION_Y + JUNCTION_SIZE/2 - 10;
            break;
    }
    
    SDL_Rect light_rect = {light_x, light_y, 20, 20};
    SDL_RenderFillRect(renderer, &light_rect);
}

void draw_junction(SDL_Renderer* renderer) {
    // Draw junction area
    SDL_SetRenderDrawColor(renderer, 80, 80, 80, 255);
    SDL_Rect junction_rect = {JUNCTION_X, JUNCTION_Y, JUNCTION_SIZE, JUNCTION_SIZE};
    SDL_RenderFillRect(renderer, &junction_rect);
    
    // Draw crossing lines
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    
    // Horizontal line
    SDL_RenderDrawLine(renderer, JUNCTION_X, JUNCTION_Y + JUNCTION_SIZE/2,
                      JUNCTION_X + JUNCTION_SIZE, JUNCTION_Y + JUNCTION_SIZE/2);
    
    // Vertical line
    SDL_RenderDrawLine(renderer, JUNCTION_X + JUNCTION_SIZE/2, JUNCTION_Y,
                      JUNCTION_X + JUNCTION_SIZE/2, JUNCTION_Y + JUNCTION_SIZE);
}

void draw_vehicles(SDL_Renderer* renderer, Queue* queue) {
    QueueNode* current = queue->front;
    int position = 0;
    
    while (current) {
        Vehicle v = current->vehicle;
        SDL_Color color = road_colors[v.road];
        SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, 255);
        
        // Update vehicle position
        calculate_vehicle_position(&v.x, &v.y, v.road, v.lane, position);
        current->vehicle.x = v.x;
        current->vehicle.y = v.y;
        
        // Draw vehicle as a rectangle
        SDL_Rect vehicle_rect = {v.x, v.y, VEHICLE_SIZE, VEHICLE_SIZE};
        SDL_RenderFillRect(renderer, &vehicle_rect);
        
        // Draw vehicle ID
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderDrawRect(renderer, &vehicle_rect);
        
        current = current->next;
        position++;
    }
}

void render_text(SDL_Renderer* renderer, TTF_Font* font, const char* text, 
                 int x, int y, SDL_Color color) {
    if (!font) return;
    
    SDL_Surface* surface = TTF_RenderText_Solid(font, text, color);
    if (surface) {
        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
        if (texture) {
            SDL_Rect dest = {x, y, surface->w, surface->h};
            SDL_RenderCopy(renderer, texture, NULL, &dest);
            SDL_DestroyTexture(texture);
        }
        SDL_FreeSurface(surface);
    }
}

// ================ Simulation Core ================
Simulator* init_simulator() {
    Simulator* sim = malloc(sizeof(Simulator));
    
    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL initialization failed: %s\n", SDL_GetError());
        free(sim);
        return NULL;
    }
    
    // Initialize SDL_ttf
    if (TTF_Init() == -1) {
        printf("TTF initialization failed: %s\n", TTF_GetError());
        SDL_Quit();
        free(sim);
        return NULL;
    }
    
    // Create window
    sim->window = SDL_CreateWindow("Traffic Junction Simulator",
                                   SDL_WINDOWPOS_CENTERED,
                                   SDL_WINDOWPOS_CENTERED,
                                   SCREEN_WIDTH, SCREEN_HEIGHT,
                                   SDL_WINDOW_SHOWN);
    
    if (!sim->window) {
        printf("Window creation failed: %s\n", SDL_GetError());
        TTF_Quit();
        SDL_Quit();
        free(sim);
        return NULL;
    }
    
    // Create renderer
    sim->renderer = SDL_CreateRenderer(sim->window, -1, 
                                       SDL_RENDERER_ACCELERATED);
    
    if (!sim->renderer) {
        printf("Renderer creation failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(sim->window);
        TTF_Quit();
        SDL_Quit();
        free(sim);
        return NULL;
    }
    
    // Load font
    sim->font = TTF_OpenFont("/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf", 18);
    if (!sim->font) {
        // Try fallback
        sim->font = TTF_OpenFont("arial.ttf", 18);
        if (!sim->font) {
            printf("Warning: Could not load font\n");
        }
    }
    
    // Initialize vehicle queues
    for (int road = 0; road < NUM_ROADS; road++) {
        for (int lane = 0; lane < NUM_LANES; lane++) {
            sim->vehicle_queues[road][lane] = create_queue(road, lane);
        }
    }
    
    // Initialize priority queue for traffic lights
    sim->light_queue = create_priority_queue();
    
    // Initialize traffic lights with different initial states
    for (int road = 0; road < NUM_ROADS; road++) {
        TrafficLight light = {
            .road = road,
            .priority = 1,
            .vehicle_count = 0,
            .light_state = (road == ROAD_A) ? LIGHT_GREEN : LIGHT_RED,
            .light_start_time = time(NULL),
            .green_duration = 10
        };
        pq_enqueue(sim->light_queue, light);
    }
    
    sim->running = true;
    sim->simulation_speed = 1;
    sim->simulation_time = time(NULL);
    sim->total_vehicles_served = 0;
    sim->vehicle_id_counter = 1;
    sim->generator_running = true;
    
    // Start vehicle generator thread
    if (pthread_create(&sim->generator_thread, NULL, generate_vehicles_thread, sim) != 0) {
        printf("Failed to create generator thread\n");
        sim->generator_running = false;
    }
    
    global_sim = sim;
    return sim;
}

void render(Simulator* sim) {
    // Clear screen
    SDL_SetRenderDrawColor(sim->renderer, 40, 40, 40, 255);
    SDL_RenderClear(sim->renderer);
    
    // Draw junction
    draw_junction(sim->renderer);
    
    // Draw all roads with their current light states
    for (int road = 0; road < NUM_ROADS; road++) {
        // Find the light state for this road
        LightState state = LIGHT_RED;
        PriorityQueueNode* current = sim->light_queue->front;
        while (current) {
            if (current->data.road == road) {
                state = current->data.light_state;
                break;
            }
            current = current->next;
        }
        draw_road(sim->renderer, road, state);
    }
    
    // Draw vehicles for each queue
    for (int road = 0; road < NUM_ROADS; road++) {
        for (int lane = 0; lane < NUM_LANES; lane++) {
            draw_vehicles(sim->renderer, sim->vehicle_queues[road][lane]);
        }
    }
    
    // Draw UI information
    SDL_Color white = {255, 255, 255, 255};
    SDL_Color yellow = {255, 255, 0, 255};
    SDL_Color red = {255, 100, 100, 255};
    
    // Draw queue counts
    for (int road = 0; road < NUM_ROADS; road++) {
        for (int lane = 0; lane < NUM_LANES; lane++) {
            int count = queue_size(sim->vehicle_queues[road][lane]);
            char buffer[50];
            sprintf(buffer, "%sL%d: %d", road_names[road], lane+1, count);
            
            int x = 20 + road * 280;
            int y = 20 + lane * 30;
            
            // Highlight AL2
            if (road == ROAD_A && lane == LANE_NORMAL) {
                render_text(sim->renderer, sim->font, buffer, x, y, 
                           count > PRIORITY_THRESHOLD ? red : yellow);
            } else {
                render_text(sim->renderer, sim->font, buffer, x, y, white);
            }
        }
    }
    
    // Draw traffic light states
    for (int road = 0; road < NUM_ROADS; road++) {
        char light_state[20];
        PriorityQueueNode* current = sim->light_queue->front;
        while (current) {
            if (current->data.road == road) {
                sprintf(light_state, "Light: %s", 
                       current->data.light_state == LIGHT_GREEN ? "GREEN" : "RED");
                render_text(sim->renderer, sim->font, light_state, 
                           20 + road * 280, 120, white);
                break;
            }
            current = current->next;
        }
    }
    
    // Draw statistics
    char stats[100];
    sprintf(stats, "Vehicles Served: %d", sim->total_vehicles_served);
    render_text(sim->renderer, sim->font, stats, SCREEN_WIDTH - 300, 20, white);
    
    sprintf(stats, "Simulation Speed: %dx", sim->simulation_speed);
    render_text(sim->renderer, sim->font, stats, SCREEN_WIDTH - 300, 50, white);
    
    // Draw AL2 priority status
    int al2_count = queue_size(sim->vehicle_queues[ROAD_A][LANE_NORMAL]);
    if (al2_count > PRIORITY_THRESHOLD) {
        char priority_status[100];
        sprintf(priority_status, "AL2 PRIORITY ACTIVE: %d vehicles", al2_count);
        render_text(sim->renderer, sim->font, priority_status, 
                   SCREEN_WIDTH/2 - 150, SCREEN_HEIGHT - 50, red);
    }
    
    // Draw controls
    render_text(sim->renderer, sim->font, "Controls: +/- Speed, SPACE Pause, ESC Quit", 
                20, SCREEN_HEIGHT - 30, white);
    
    // Present renderer
    SDL_RenderPresent(sim->renderer);
}

void process_events(Simulator* sim) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT:
                sim->running = false;
                break;
                
            case SDL_KEYDOWN:
                switch (event.key.keysym.sym) {
                    case SDLK_ESCAPE:
                        sim->running = false;
                        break;
                    case SDLK_SPACE:
                        // Pause simulation
                        sim->generator_running = !sim->generator_running;
                        break;
                    case SDLK_PLUS:
                    case SDLK_EQUALS:
                        sim->simulation_speed++;
                        if (sim->simulation_speed > 5) sim->simulation_speed = 5;
                        break;
                    case SDLK_MINUS:
                        if (sim->simulation_speed > 1) {
                            sim->simulation_speed--;
                        }
                        break;
                }
                break;
        }
    }
}

void update_simulation(Simulator* sim) {
    static time_t last_update = 0;
    time_t current_time = time(NULL);
    
    if (current_time - last_update < 1) {
        return;
    }
    last_update = current_time;
    
    // Update priorities
    update_traffic_priorities(sim);
    
    // Get current green light road
    TrafficLight* current_green = NULL;
    PriorityQueueNode* current = sim->light_queue->front;
    while (current) {
        if (current->data.light_state == LIGHT_GREEN) {
            current_green = &current->data;
            break;
        }
        current = current->next;
    }
    
    if (current_green) {
        // Process vehicles for current green light
        process_vehicles(sim, current_green->road);
        
        // Check if it's time to change light
        double elapsed = difftime(current_time, current_green->light_start_time);
        
        if (elapsed > current_green->green_duration) {
            // Change this light to red
            current_green->light_state = LIGHT_RED;
            current_green->light_start_time = current_time;
            
            // Find next road with highest priority and set to green
            current = sim->light_queue->front;
            while (current) {
                if (current->data.light_state == LIGHT_RED) {
                    current->data.light_state = LIGHT_GREEN;
                    current->data.light_start_time = current_time;
                    
                    // Calculate green duration based on vehicles
                    int vehicles_to_serve = calculate_vehicles_to_serve(sim);
                    current->data.green_duration = vehicles_to_serve * TIME_PER_VEHICLE;
                    if (current->data.green_duration < 5) current->data.green_duration = 5;
                    if (current->data.green_duration > 30) current->data.green_duration = 30;
                    
                    printf("Changed to GREEN: Road %c for %d seconds\n", 
                           'A' + current->data.road, current->data.green_duration);
                    break;
                }
                current = current->next;
            }
        }
    }
}

void cleanup_simulator(Simulator* sim) {
    if (!sim) return;
    
    // Stop generator thread
    sim->generator_running = false;
    pthread_join(sim->generator_thread, NULL);
    
    // Free queues
    for (int road = 0; road < NUM_ROADS; road++) {
        for (int lane = 0; lane < NUM_LANES; lane++) {
            // Free all nodes in queue
            while (sim->vehicle_queues[road][lane]->front) {
                dequeue(sim->vehicle_queues[road][lane]);
            }
            pthread_mutex_destroy(&sim->vehicle_queues[road][lane]->mutex);
            free(sim->vehicle_queues[road][lane]);
        }
    }
    
    // Free priority queue
    pthread_mutex_destroy(&sim->light_queue->mutex);
    free(sim->light_queue);
    
    // Free SDL resources
    if (sim->font) TTF_CloseFont(sim->font);
    if (sim->renderer) SDL_DestroyRenderer(sim->renderer);
    if (sim->window) SDL_DestroyWindow(sim->window);
    
    TTF_Quit();
    SDL_Quit();
    free(sim);
}

int main() {
    printf("Starting Traffic Junction Simulator...\n");
    printf("Road Layout:\n");
    printf("  A: Top road (Red)\n");
    printf("  B: Right road (Green)\n");
    printf("  C: Bottom road (Blue)\n");
    printf("  D: Left road (Yellow)\n");
    printf("\nPriority Logic:\n");
    printf("  - AL2 gets priority when > %d vehicles\n", PRIORITY_THRESHOLD);
    printf("  - Priority resets when < %d vehicles\n", PRIORITY_RESET);
    printf("\nControls:\n");
    printf("  +/- : Change simulation speed\n");
    printf("  SPACE: Pause/Resume vehicle generation\n");
    printf("  ESC  : Quit\n");
    
    Simulator* sim = init_simulator();
    
    if (!sim) {
        printf("Failed to initialize simulator\n");
        return 1;
    }
    
    // Main simulation loop
    const int TARGET_FPS = 60;
    const int FRAME_DELAY = 1000 / TARGET_FPS;
    
    Uint32 frame_start;
    int frame_time;
    
    while (sim->running) {
        frame_start = SDL_GetTicks();
        
        process_events(sim);
        update_simulation(sim);
        render(sim);
        
        frame_time = SDL_GetTicks() - frame_start;
        
        if (FRAME_DELAY > frame_time) {
            SDL_Delay(FRAME_DELAY - frame_time);
        }
    }
    
    cleanup_simulator(sim);
    printf("Simulation ended. Total vehicles served: %d\n", global_sim ? global_sim->total_vehicles_served : 0);
    
    return 0;
}
