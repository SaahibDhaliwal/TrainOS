#include "sensor.h"

#include <stdbool.h>
#include <string.h>

#include "command.h"
#include "cursor.h"
#include "pos.h"
#include "printer_proprietor_protocol.h"
#include "rpi.h"

#define TABLE_START_ROW 15
#define TABLE_START_COL 60
#define SENSOR_TIME_START_ROW 13
#define SENSOR_TIME_START_COL 73
#define SENSOR_TIME_LABEL "Sensor Query: "

void initialize_sensors(Sensor* sensors) {
    for (int i = 0; i < SENSOR_BUFFER_SIZE; i += 1) {
        sensors[i].box = '\0';
        sensors[i].num = 0;
    }
}

static void draw_grid_frame(uint32_t consoleTid) {
    // clang-format off
    const char* lines[] = {
        " Recent Sensors ↓ ",   
        "     ┌─────┐     ",
        "     │     │     ",
        "     │     │     ",
        "     │     │     ",
        "     │     │     ",
        "     │     │     ",
        "     │     │     ",
        "     │     │     ",
        "     │     │     ",
        "     │     │     ",  
        "     │     │     ",
        "     │     │     ",
        "     └─────┘     \n"
    };
    // clang-format on

    int num_lines = sizeof(lines) / sizeof(lines[0]);

    for (int i = 0; i < num_lines; i += 1) {
        printer_proprietor::printF(consoleTid, "\033[%d;%dH%s", TABLE_START_ROW + i, TABLE_START_COL, lines[i]);
    }
}

void print_sensor_table(uint32_t consoleTid) {
    printer_proprietor::printF(consoleTid, "\033[%d;%dH", TABLE_START_ROW, TABLE_START_COL);
    draw_grid_frame(consoleTid);
}

// static void update_sensor(Sensor* sensor_buffer, int sensorBufferIdx, Queue* console, bool evenParity) {
//     buffer_printf(console, "\033[%d;%dH", TABLE_START_ROW + 2 + sensorBufferIdx, TABLE_START_COL + 7);
//     buffer_puts(console, "\033[1m");

//     if (evenParity) {
//         cursor_sharp_blue(console);
//     } else {
//         cursor_sharp_pink(console);
//     }

//     buffer_printf(console, "%c%d ", sensor_buffer[sensorBufferIdx].box, sensor_buffer[sensorBufferIdx].num);
//     buffer_puts(console, "\033[22m");
// }

// void read_sensors(Queue* marklin_queue) {
//     Command readSensorsCmd = Base_Command;
//     readSensorsCmd.operation = SENSOR_READ_ALL;
//     readSensorsCmd.msDelay = 200;
//     queue_push(marklin_queue, readSensorsCmd);
// }

// void process_sensor_byte(char* sensorBytes, int sensorByteIdx, Sensor* sensors, int* sensorBufferIdx,
//                          bool* isSensorBufferParityEven, Queue* console) {
//     char byte = sensorBytes[sensorByteIdx];
//     int sensorByteCount = sensorByteIdx + 1;
//     char box = 'A' + (sensorByteCount - 1) / 2;

//     for (int bit = 7; bit >= 0; bit -= 1) {
//         if (byte & (1 << bit)) {
//             int sensorNum = (8 - bit);
//             if (sensorByteCount % 2 == 0) {  // second side?
//                 sensorNum += 8;
//             }

//             int prevSensorIdx = (*sensorBufferIdx - 1 + SENSOR_BUFFER_SIZE) % SENSOR_BUFFER_SIZE;
//             if (sensors[prevSensorIdx].box != box ||
//                 sensors[prevSensorIdx].num != sensorNum) {  // has the sensor changed?
//                 if (*sensorBufferIdx == 0) {                // starting from the top, we can switch colour
//                     *isSensorBufferParityEven = !*isSensorBufferParityEven;
//                 }
//                 sensors[*sensorBufferIdx].box = box;
//                 sensors[*sensorBufferIdx].num = sensorNum;
//                 WITH_HIDDEN_CURSOR(console,
//                                    update_sensor(sensors, *sensorBufferIdx, console, *isSensorBufferParityEven));
//                 *sensorBufferIdx = (*sensorBufferIdx + 1) % SENSOR_BUFFER_SIZE;
//             }
//         }
//     }
// }

void print_sensor_time(uint32_t consoleTid) {
    printer_proprietor::printF(consoleTid, "\033[%d;%dH", SENSOR_TIME_START_ROW, SENSOR_TIME_START_COL);
    printer_proprietor::printF(consoleTid, SENSOR_TIME_LABEL);
}

// updates the total time to read all the sensor bytes
void update_sensor_total_time(uint64_t millis, uint32_t consoleTid) {
    printer_proprietor::printF(consoleTid, "\033[%d;%dH", SENSOR_TIME_START_ROW,
                               SENSOR_TIME_START_COL + strlen(SENSOR_TIME_LABEL));
    printer_proprietor::printF(consoleTid, "%2dms", millis);
}
