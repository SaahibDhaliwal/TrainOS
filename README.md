Implemented a preemptive microkernel for the Raspberry Pi (BCM2711, ARMv8, single-core) from scratch. It supports real-time train control through efficient task management, interprocess communication, and interrupt-driven I/O.

---

## Kernel Architecture

### Overview
We implemented a preemptive microkernel for the Raspberry Pi (BCM2711, ARMv8, single-core) from scratch. It supports real-time train control through efficient task management, interprocess communication, and interrupt-driven I/O.

### Task Manager
- **Preallocated slabs** for `TaskDescriptor` objects (max 64 concurrent tasks)
- **64 priority levels**, round-robin scheduling within each
- Stores kernel and user **Context** (registers, SP, ELR, SPSR) for fast context switches
- **Context switching**:
  - `kernelToUser` restores user registers and executes `eret`
  - `userToKernel` saves user state and restores kernel registers on SVC or interrupt

### Data Structures
- **Queue / Stack / RingBuffer** – Intrusively linked, fixed-size, no dynamic allocation
- **Context** – Holds all GPRs plus special registers for task resumption

### System Calls
- **Create, MyTid, MyParentTid, Yield, Exit**
- **Send / Receive / Reply** – Message passing for IPC, no shared memory
- **AwaitEvent** – Blocks until a hardware event occurs
- **GetIdleTime** – Returns kernel idle percentage
- **Quit** – Shuts down the system

### Servers
- **Name Server** – Registers and looks up task IDs by name
- **Clock Server + Notifier** – Implements `Time`, `Delay`, `DelayUntil`
- **Console Server + Notifiers** – UART I/O for the console
- **Marklin Server + Notifiers** – UART I/O for the Marklin controller
- **Printer Proprietor** – Serializes console output to prevent corruption
- **Command Server** – Parses and executes user commands

---

## Train Control

### Overview
Built on top of the kernel, the train control system models train motion, maintains track state, and coordinates multiple trains to prevent collisions.  

All train-related commands (`tr`, `rv`, `sw`, `stop`, `reset`, `q`) are funneled through the **Localization Server**, which integrates with a **zone reservation system**.

---

### Train Model & Calibration

Calibration data is stored in `raw data/` and includes:
- **Stopping distance** – 20 trials per train at specific speeds
- **Velocity** – Measured for speed 8 high and speed 14
- **Acceleration / Deceleration** – Extracted from automated start/stop tests

The **train structure** holds:
- Velocity estimate
- Sensor ahead/behind
- Stopping distance
- Target node and offset
- Track references (with added next-sensor pointers and distances)

---

### Localization Server

Central authority for:
- Updating train velocity estimates via exponential weighted averages
- Routing trains with Dijkstra’s algorithm
  - Considers reversing at start
  - Avoids reserved zones
  - Resolves head-on conflicts by minimizing total travel distance
- Auto-configuring switches to route trains to their targets
- Managing stop commands with tick-accurate timing

---

### Zone Reservation System

- Track is split into **zones** bounded by sensors
- Trains must reserve zones before entry
- Zones are released once the rear of a train passes the exit sensor
- Head-on conflicts trigger rerouting based on shortest total travel distance

---

### Routing & Stopping

- **Routing**: Dijkstra’s algorithm on the track graph with branch/merge handling and reverse-path consideration
- **Stopping**:
  - Computes future tick for stop command based on stopping distance
  - Adjusts for acceleration profiles when distance is too short for full stop
  - Handles multiple stop points along a route for reverses

---

### Execution Flow

At startup:
1. Kernel initializes BSS, constructors, exception vectors, GPIO, UART, and servers
2. Train control initializes default switch configuration
3. User commands are processed by the Command Server → Localization Server → Marklin Server
4. Sensor data updates train positions in real time
5. Zone reservations prevent collisions while routing trains

---

## Known Bugs / Limitations
- Reverse commands to a reversing train are ignored
- Setting speed during reverse only takes effect after stop completes
- Pasting multiple commands quickly may drop some
- No MMU guard for stack overflow (possible stack corruption in extreme cases)

---
