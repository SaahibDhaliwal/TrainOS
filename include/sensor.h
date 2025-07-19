#ifndef __SENSOR__
#define __SENSOR__
#include "cstdint"

#define SENSOR_BUFFER_SIZE 11

struct Sensor {
    char box;
    uint8_t num;

    bool operator==(const Sensor& other) const {
        return box == other.box && num == other.num;
    }
};

#endif