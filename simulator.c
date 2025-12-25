// Build (MSYS2 MinGW64 example):
// gcc diagonal_simulator.c -o diagonal_simulator.exe -lraylib -lopengl32 -lgdi32 -lwinmm

#include "raylib.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

typedef struct {
    float x, y;
    float vx, vy;
    int road;       // 0=Top(A),1=Bottom(B),2=Right(C),3=Left(D)
    int lane;       // 0=left,1=center,2=right
    bool active;
    char plate[10];
    float speed;    // variable speed
    int turn;       // 0=straight, 1=left, 2=right
} Vehicle;

#define MAX_VEH 64
static Vehicle vehicles[MAX_VEH];

static int currentGreen = 0;    // multiple roads can be green
static float phaseTimer = 0.0f;
static float TIME_PER_VEHICLE = 0.7f;

static int screenW = 1200;
static int screenH = 900;
static const int roadWidth = 180;
static const int laneWidth = 60;
static int centerX = 600;
static int centerY = 450;

static Color roadColor = {70, 70, 70, 255};
static Color laneColor = {150, 150, 150, 255};

// Initialize all vehicles inactive
static void InitVehicles() {
    for (int i = 0; i < MAX_VEH; i++) vehicles[i].active = false;
}

// Count vehicles in a specific lane
static int LaneCount(int road, int lane) {
    int c = 0;
    for (int i = 0; i < MAX_VEH; i++)
        if (vehicles[i].active && vehicles[i].road == road && vehicles[i].lane == lane) c++;
    return c;
}

// Generate vehicle plate
static void GenerateVehicleNumber(char *buffer) {
    for (int i = 0; i < 7; i++) {
        if (i < 2 || (i>=3 && i<5)) buffer[i] = 'A' + GetRandomValue(0,25);
        else buffer[i] = '0' + GetRandomValue(0,9);
    }
    buffer[7] = '\0';
}

// Spawn vehicle with random speed and turn
static void SpawnVehicle(int road, int lane) {
    for (int i = 0; i < MAX_VEH; i++) {
        if (!vehicles[i].active) {
            vehicles[i].active = true;
            vehicles[i].road = road;
            vehicles[i].lane = lane;
            GenerateVehicleNumber(vehicles[i].plate);
            vehicles[i].speed = 60 + GetRandomValue(0, 60); // 60-120 px/s
            vehicles[i].turn = GetRandomValue(0,2); // straight, left, right

            switch(road) {
                case 0: vehicles[i].x = centerX - roadWidth/2 + lane*laneWidth + laneWidth/2; vehicles[i].y = -40; vehicles[i].vx=0; vehicles[i].vy=vehicles[i].speed; break;
                case 1: vehicles[i].x = centerX + roadWidth/2 - lane*laneWidth - laneWidth/2; vehicles[i].y = screenH + 40; vehicles[i].vx=0; vehicles[i].vy=-vehicles[i].speed; break;
                case 2: vehicles[i].x = screenW + 40; vehicles[i].y = centerY + roadWidth/2 - lane*laneWidth - laneWidth/2; vehicles[i].vx=-vehicles[i].speed; vehicles[i].vy=0; break;
                case 3: vehicles[i].x = -40; vehicles[i].y = centerY - roadWidth/2 + lane*laneWidth + laneWidth/2; vehicles[i].vx=vehicles[i].speed; vehicles[i].vy=0; break;
            }
            break;
        }
    }
}

// Determine if vehicle should stop at traffic light (only lane 1 obeys)
static bool ShouldStop(Vehicle *v) {
    return (v->lane==1 && v->road!=currentGreen);
}

// Update vehicle positions and despawn when off-screen
static void UpdateVehicles(float dt) {
    float stopOffset = roadWidth/2 + 15;
    for(int i=0;i<MAX_VEH;i++){
        Vehicle *v = &vehicles[i];
        if(!v->active) continue;

        bool stop = ShouldStop(v);
        switch(v->road){
            case 0: v->vy = stop ? 0 : v->speed; v->vx = 0; break;
            case 1: v->vy = stop ? 0 : -v->speed; v->vx = 0; break;
            case 2: v->vx = stop ? 0 : -v->speed; v->vy = 0; break;
            case 3: v->vx = stop ? 0 : v->speed; v->vy = 0; break;
        }

        // apply turn adjustments when inside intersection
        if (v->x > centerX-roadWidth/2 && v->x < centerX+roadWidth/2 &&
            v->y > centerY-roadWidth/2 && v->y < centerY+roadWidth/2) {
            if(v->turn==1){v->vx -= v->vy; v->vy = -v->vx;}  // simple left turn
            else if(v->turn==2){v->vx += v->vy; v->vy = -v->vx;} // simple right turn
        }

        v->x += v->vx*dt;
        v->y += v->vy*dt;

        if(v->x<-200 || v->x>screenW+200 || v->y<-200 || v->y>screenH+200) v->active=false;
    }
}

static void DrawRoads() {
    ClearBackground(RAYWHITE);
    DrawRectangle(centerX-roadWidth/2,0,roadWidth,screenH,roadColor);
    DrawRectangle(0,centerY-roadWidth/2,screenW,roadWidth,roadColor);

    for(int i=1;i<3;i++){
        DrawLine(centerX-roadWidth/2+laneWidth*i,0,centerX-roadWidth/2+laneWidth*i,screenH,laneColor);
        DrawLine(0,centerY-roadWidth/2+laneWidth*i,screenW,centerY-roadWidth/2+laneWidth*i,laneColor);
    }
}

static void DrawVehicles() {
    for(int i=0;i<MAX_VEH;i++){
        if(!vehicles[i].active) continue;
        Color c = (vehicles[i].lane==1)? ORANGE : SKYBLUE;
        float size = 20;
        Vector2 p1 = {vehicles[i].x,vehicles[i].y-size/2};
        Vector2 p2 = {vehicles[i].x-size/2, vehicles[i].y+size/2};
        Vector2 p3 = {vehicles[i].x+size/2, vehicles[i].y+size/2};
        DrawTriangle(p1,p2,p3,c);
        DrawText(vehicles[i].plate,(int)(vehicles[i].x-10),(int)(vehicles[i].y-30),10,BLACK);
    }
}

static void DrawLights(){
    const char *labels[4]={"A","B","C","D"};
    Vector2 pos[4] = {{centerX-25,centerY-roadWidth/2-100},{centerX-25,centerY+roadWidth/2+20},
                      {centerX+roadWidth/2+20,centerY-25},{centerX-roadWidth/2-70,centerY-25}};
    for(int i=0;i<4;i++){
        DrawRectangle(pos[i].x,pos[i].y,50,90,DARKGRAY);
        Color green = (i==currentGreen)? GREEN:GRAY;
        DrawCircle(pos[i].x+25,pos[i].y+68,12,green);
        DrawText(labels[i],pos[i].x+18,pos[i].y+44,12,WHITE);
    }
}

int main(void){
    InitWindow(screenW,screenH,"Diagonal Traffic Simulator");
    SetTargetFPS(60);
    InitVehicles();

    while(!WindowShouldClose()){
        float dt = GetFrameTime();

        // simple spawning every frame based on lane occupancy
        for(int r=0;r<4;r++)
            for(int l=0;l<3;l++)
                if(LaneCount(r,l)<5 && GetRandomValue(0,100)<5) SpawnVehicle(r,l);

        phaseTimer+=dt;
        if(phaseTimer>=3+calculateGreenDuration()){ // dynamic green duration
            phaseTimer=0;
            currentGreen=(currentGreen+1)%4;
        }

        UpdateVehicles(dt);

        BeginDrawing();
        DrawRoads();
        DrawVehicles();
        DrawLights();
        DrawText(TextFormat("Green: %c", 'A'+currentGreen), 20,20,22,BLACK);
        EndDrawing();
    }

    CloseWindow();
    return 0;
}

