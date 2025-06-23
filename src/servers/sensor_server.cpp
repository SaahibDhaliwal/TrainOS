#include "sensor_server.h"

#include "config.h"
#include "generic_protocol.h"
#include "idle_time.h"
#include "interrupt.h"
#include "intrusive_node.h"
#include "marklin_command_protocol.h"
#include "marklin_server.h"
#include "marklin_server_protocol.h"
#include "name_server.h"
#include "name_server_protocol.h"
#include "queue.h"
// #include "rpi.h"
#include <stdbool.h>
#include <string.h>

#include "clock_server.h"
#include "clock_server_protocol.h"
#include "command.h"
#include "cursor.h"
#include "pos.h"
#include "printer_proprietor.h"
#include "printer_proprietor_protocol.h"
#include "ring_buffer.h"
#include "rpi.h"
#include "sensor.h"
#include "stack.h"
#include "sys_call_stubs.h"
#include "test_utils.h"
#include "timer.h"

#define TABLE_START_ROW 15
#define TABLE_START_COL 60
#define SENSOR_TIME_START_ROW 13
#define SENSOR_TIME_START_COL 73
#define SENSOR_TIME_LABEL "Sensor Query: "
#define SENSOR_BYTE_SIZE 10

void initialize_sensors(Sensor* sensors) {
    for (int i = 0; i < SENSOR_BYTE_SIZE; i += 1) {
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

void update_sensor(Sensor* sensor_buffer, int sensorBufferIdx, int tid, bool evenParity) {
    printer_proprietor::printF(tid, 0, "\033[%d;%dH", TABLE_START_ROW + 2 + sensorBufferIdx, TABLE_START_COL + 7);
    printer_proprietor::printS(tid, 0, "\033[1m");  // bold or increased intensity

    if (evenParity) {
        cursor_sharp_blue(tid);
    } else {
        cursor_sharp_pink(tid);
    }

    printer_proprietor::printF(tid, 0, "%c%d ", sensor_buffer[sensorBufferIdx].box, sensor_buffer[sensorBufferIdx].num);
    printer_proprietor::printS(tid, 0, "\033[22m");  // normal intensity
}

// one is for their position in the array, the other is their position on the UI

void process_sensor_byte(unsigned char* sensorBytes, int sensorByteIdx, Sensor* sensors, int* sensorBufferIdx,
                         bool* isSensorBufferParityEven, uint32_t printerTid) {
    char byte = sensorBytes[sensorByteIdx];
    int sensorByteCount = sensorByteIdx + 1;
    char box = 'A' + (sensorByteCount - 1) / 2;

    for (int bit = 7; bit >= 0; bit -= 1) {
        if (byte & (1 << bit)) {
            int sensorNum = (8 - bit);
            if (sensorByteCount % 2 == 0) {  // second side?
                sensorNum += 8;
            }

            int prevSensorIdx = (*sensorBufferIdx - 1 + SENSOR_BUFFER_SIZE) % SENSOR_BUFFER_SIZE;
            if (sensors[prevSensorIdx].box != box ||
                sensors[prevSensorIdx].num != sensorNum) {  // has the sensor changed?
                if (*sensorBufferIdx == 0) {                // starting from the top, we can switch colour
                    *isSensorBufferParityEven = !*isSensorBufferParityEven;
                }
                sensors[*sensorBufferIdx].box = box;
                sensors[*sensorBufferIdx].num = sensorNum;
                update_sensor(sensors, *sensorBufferIdx, printerTid, *isSensorBufferParityEven);
                *sensorBufferIdx = (*sensorBufferIdx + 1) % SENSOR_BUFFER_SIZE;
            }
        }
    }
}

void SensorServer() {
    // uint32_t commandTid = sys::MyParentTid();
    int printerTid = name_server::WhoIs(PRINTER_PROPRIETOR_NAME);
    ASSERT(printerTid >= 0, "UNABLE TO GET PRINTER_PROPRIETOR_NAME\r\n");

    int clockServerTid = name_server::WhoIs(CLOCK_SERVER_NAME);
    ASSERT(clockServerTid >= 0, "UNABLE TO GET CLOCK_SERVER_NAME\r\n");

    int marklinTid = name_server::WhoIs(MARKLIN_SERVER_NAME);
    ASSERT(marklinTid >= 0, "UNABLE TO GET MARKLIN_SERVER_NAME\r\n");

    // clock_server::Delay(clockServerTid, 200);  // some delay at the start

    Sensor sensorBytes[SENSOR_BYTE_SIZE];
    initialize_sensors(sensorBytes);
    int nonSensorWindow = 5;
    unsigned char requestChar = 0x85;
    // int sensorByteIdx = 0;
    int sensorBufferIdx = 0;
    bool isSensorBufferParityEven = true;

    for (;;) {
        // uart_printf(CONSOLE, "Start delay ");
        // delay
        clock_server::Delay(clockServerTid, nonSensorWindow);
        // uart_printf(CONSOLE, "Done delay ");

        // uart_printf(CONSOLE, "Start sens req ");
        // marklin request
        marklin_server::Putc(marklinTid, 0, requestChar);

        // uart_printf(CONSOLE, "Done sensor req ");

        // we expect to getC ten times. NO HANDLING OF TIMEOUT
        for (int i = 0; i < 10; i++) {
            unsigned char result = marklin_server::Getc(marklinTid, 0);
            // uart_printf(CONSOLE, "%d ", result);
            // printer_proprietor::printF(printerTid, "%d ", result);
            // ASSERT(1 == 2, "got here");
            if (result) {
                // printer_proprietor::printF(printerTid, "nonzero: %d ", result);
                // uart_printf(CONSOLE, "\n\r nonzero: %d", result);
                process_sensor_byte(&result, i, sensorBytes, &sensorBufferIdx, &isSensorBufferParityEven, printerTid);
                // clock_server::Delay(clockServerTid, nonSensorWindow);
            }
        }
        // uart_printf(CONSOLE, " got all sensors \n\r");
        // ASSERT(1 == 2, "sensor server got all sensors");

    }  // for
}  // for
