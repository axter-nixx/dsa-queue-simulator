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
-   GitHub Repository:  [https://github.com/sujan0629/dsa-queue-simulator](https://github.com/sujan0629/dsa-queue-simulator)


