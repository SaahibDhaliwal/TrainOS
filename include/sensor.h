#ifndef __SENSOR__
#define __SENSOR_
#include "cstdint"

#define SENSOR_BUFFER_SIZE 11

typedef struct {
    char box;
    uint8_t num;
} Sensor;

#endif