#ifndef __SENSOR_SERVER__
#define __SENSOR_SERVER__

constexpr const char* SENSOR_SERVER_NAME = "sensor_server";

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    char box;
    uint8_t num;
} Sensor;

#define SENSOR_BUFFER_SIZE 11

void initialize_sensors(Sensor* sensorBuffer);
void print_sensor_table(uint32_t consoleTid);
// void read_sensors(Queue* marklinQueue);
// void process_sensor_byte(char* sensorBytes, int sensorByteIdx, Sensor* sensors, int* sensorBufferIdx,
//                          bool* sensorBufferParityEven, Queue* consoleTid);
void print_sensor_time(uint32_t consoleTid);
void update_sensor_total_time(uint64_t millis, uint32_t consoleTid);

void SensorServer();

#endif