# DSA-QUEUE-SIMULATOR

Developed a real time traffic analyzing system where a simulation of traffic at a junction is visualized with graphics so that the traffic can be managed from the system we developed and this simulation is also an example to show that it can be applicable in the real life world. In the following program i developed 2 main programs called **simulator.c** and **traffic_generator.c** where simulator.c is the logic to manage the data and has the graphics libraries in it and traffic_generator.c is the program that generates the traffic data like vehicle number plate,road and lane and stores the data into vehicle.data and that file is used by simulator.c to simulate the traffic into the traffic junction. Here, I used the concept of queue and priority queue for the management of traffic system and **sdl2 libraries** for the graphical feature to my program. The vehicle logics that I used in this program is, theres this lane called **AL2** where, if the vehicles number exceed than 10 in that lane then, all other lane which is being served will be halted and the AL2 lane will be prioritized to serve its vehicle first, implementing the concept of priority queue in it. When the vehicle number is below 10 in the lane AL2 then all the **4 lanes** are served as normal lanes and the cycle of traffic light goes in a normal way. Vehicles are served based on the average number of vehicles waiting in normal lanes. To calculate the estimated time of green light in a lane will be the product of number of vehicles in the lane and the estimated time to serve one vehicle.


# List of the Functions used in this program

  

The functions used in the program are categorized into 3 main parts.

  

## Queue functions

  

**CreateQueue()** : It is for the creation of a empty queue

**Enqueue()** - This will add up the vehicle to rear

**Dequeue()** - This will remove vehicle from front

**Peek()** - View the vehicles without removing or modifying the vehicle

**isEmpty()** - Checks if there’s no vehicles in the lane or not

**getSize()** - This will get the current count of vehicle which is waiting in the lane

**freeQueue()** - Deallocate memory

  

## Priority Queue Function

  

**createPriorityQueue()** - This is the initialization of priority system

**updatePriority()** - When the lane AL2 exceeds the vehicle threshold number then this function will update the lane priority to AL2

**getNextLane()** - Loads up the Next lane to serve

# Traffic Junction Simulation - Algorithm Overview

## Core Algorithms

### 1. Vehicle Generation & File Communication

Random vehicle IDs (format: `AA0AA000`) generate alongside lane assignments (A/B/C/D) at 1-second intervals. Data persists to `vehicles.data` as `VEHICLEID:LANE`. A reader thread monitors this file every 2 seconds, extracting entries into respective lane queues.

> **Performance:** `O(1)` generation, `O(n)` parsing

---

### 2. Queue Operations

Each lane maintains a **FIFO queue** supporting:
- `Enqueue` - Insert at tail
- `Dequeue` - Remove from head  
- `GetSize` - Query count

Four independent queues exist. Operations execute in `O(1)` time with `O(m)` total space.

---

### 3. Priority Detection

`AL2` receives preferential treatment via threshold logic:
```
AL2 > 10  → activate priority
AL2 ≥ 5   → maintain priority
AL2 < 5   → revert to normal
```

Standard mode distributes service using: `⌈(BL2 + CL3 + DL4) / 3⌉`

The **10/5 gap** prevents oscillation. 

**Complexity:** `O(1)`

---

### 4. Traffic Light State Machine

System operates in five states: `STATE_0` (all red) and `STATE_A/B/C/D` (single green).

#### Priority Path:
```
A → green | serve AL2 until < 5 | duration = count × unit_time
```

#### Standard Path:
```
B → C → D rotation | serve average vehicles | fair distribution
```

Transitions include **1-second all-red safety buffer**. 

**Complexity:** `O(v)`

---

### 5. Multi-threaded Architecture

| Thread | Responsibility |
|--------|---------------|
| **Generator** | File writing |
| **Reader** | File parsing → queue population |
| **Processor** | Vehicle management → signal control |
| **Graphics** | Visual rendering (30 FPS) |

**Mutex locks** ensure thread-safe access to shared resources. 

**Overhead:** `O(1)`

---

## Complexity Analysis

| Metric | Value |
|--------|-------|
| **Cycle time** | `O(v + n)` |
| **Space usage** | `O(m)` |
| **Queue operations** | `O(1)` |
| **Priority evaluation** | `O(1)` |

*Where: v = vehicles served, n = file entries, m = total vehicles*

---

## Design Highlights

- ✓ **Dual-threshold** hysteresis (10/5) stabilizes mode transitions
- ✓ **Averaging formula** ensures equitable lane service
- ✓ **Mutual exclusion** prevents race conditions
- ✓ **Single-green** policy eliminates deadlock potential
- ✓ **Constant-time** queue primitives

> **Key Insight:** The threshold separation proves essential—without the gap between activation and deactivation points, the system would experience unstable behavior when AL2 fluctuates near a single boundary value.

## Traffic Operation management Functions

  

**checkQueue()** - This is where the main traffic logic stays on

**readAndParseFile()** - This function is responsible to constantly monitor the vehicles.data file and will update the system when there is a new vehicle entry and add them to there appropriate queues.

**refreshLight()** - This will update the traffic light display

# Time Complexity analysis

## Queue Operation

| Operation | Complexity | Why |
|-----------|------------|-----|
| Dequeue | O(1) | When you add a vehicle, it just goes to the next spot in the array. The rear pointer moves by one position using modulo arithmetic, so it wraps around. No need to move other vehicles around or search for a spot. |
| Enqueue | O(1) | Taking out a vehicle is basically the same thing - grab whatever's at the front position and move the front pointer forward. The circular design means we don't have to shift everything down like you would in a regular array. |
| Peek | O(1) | This one's straightforward - you're just looking at what vehicle is at the front without actually removing it. One array access, that's it. |
| GetSize | O(1) | We keep a counter that tracks how many vehicles are in the queue, so getting the size is just returning that number. No counting needed. |

## Priority Queue

| Operation | Complexity | Why |
|-----------|------------|-----|
| Update priority | O(1) | We keep a counter that tracks how many vehicles are in the queue, so getting the size is just returning that number. No counting needed. |
| Get next lane | O(4)=O(1) | This one loops through all 4 roads checking their priorities and vehicle counts. Yeah it's technically a loop, but with only 4 iterations every time, it's basically constant. Doesn't matter if you have 10 vehicles or 100, still checking the same 4 roads. |




## References
-   Assignment: COMP202 DSA Queue Simulator
-   SDL Documentation:  [https://wiki.libsdl.org/SDL2/FrontPage](https://wiki.libsdl.org/SDL2/FrontPage)
-   C Programming: Kernighan & Ritchie
