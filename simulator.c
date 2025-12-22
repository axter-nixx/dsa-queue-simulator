#include <SDL2/SDL.h>
#include <stdio.h>
#include "traffic_generator.h"

#define VEHICLE_TIME 1

int traffic[ROADS][LANES];
int priorityRoad = -1;

/* ================= LOGIC ================= */

int calculateVehiclesServed() {
    int total = 0;
    for (int r = 1; r < ROADS; r++)
        total += traffic[r][1];

    return total / LANES;
}

void updatePriority() {
    if (traffic[0][1] > 10)
        priorityRoad = 0;
    else if (traffic[0][1] < 5)
        priorityRoad = -1;
}

void serveRoad(int road) {
    int V = calculateVehiclesServed();
    int greenTime = V * VEHICLE_TIME;

    printf("Serving Road %c | Vehicles Served: %d\n",
           'A' + road, greenTime);

    if (traffic[road][1] >= greenTime)
        traffic[road][1] -= greenTime;
    else
        traffic[road][1] = 0;
}

/* ================= SDL ================= */

void drawLane(SDL_Renderer *r, int x, int y, int v) {
    SDL_Rect base = {x, y, 120, 40};
    SDL_Rect cars = {x + 5, y + 5, v * 4, 30};

    SDL_SetRenderDrawColor(r, 80, 80, 80, 255);
    SDL_RenderFillRect(r, &base);

    SDL_SetRenderDrawColor(r, 0, 200, 0, 255);
    SDL_RenderFillRect(r, &cars);
}

/* ================= MAIN ================= */

int main() {
    SDL_Init(SDL_INIT_VIDEO);

    SDL_Window *win = SDL_CreateWindow(
        "Traffic Simulation",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        800, 600, 0);

    SDL_Renderer *ren = SDL_CreateRenderer(win, -1, 0);

    int running = 1;
    SDL_Event e;

    while (running) {
        while (SDL_PollEvent(&e))
            if (e.type == SDL_QUIT)
                running = 0;

        /* 🔄 Generate traffic */
        generateTraffic(traffic);

        /* ⚡ Priority logic */
        updatePriority();

        if (priorityRoad != -1)
            serveRoad(priorityRoad);
        else
            serveRoad(rand() % ROADS);

        SDL_SetRenderDrawColor(ren, 20, 20, 20, 255);
        SDL_RenderClear(ren);

        for (int r = 0; r < ROADS; r++)
            drawLane(ren, 50, 50 + r * 80, traffic[r][1]);

        SDL_RenderPresent(ren);
        SDL_Delay(1000);
    }

    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}

