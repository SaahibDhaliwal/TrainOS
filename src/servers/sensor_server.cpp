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
        console_server::Printf(consoleTid, "\033[%d;%dH%s", TABLE_START_ROW + i, TABLE_START_COL, lines[i]);
    }
}

void print_sensor_table(uint32_t consoleTid) {
    console_server::Printf(consoleTid, "\033[%d;%dH", TABLE_START_ROW, TABLE_START_COL);
    draw_grid_frame(consoleTid);
}

void update_sensor(Sensor* sensor_buffer, int sensorBufferIdx, int tid, bool evenParity) {
    console_server::Printf(tid, "\033[%d;%dH", TABLE_START_ROW + 2 + sensorBufferIdx, TABLE_START_COL + 7);
    console_server::Puts(tid, 0, "\033[1m");  // bold or increased intensity

    if (evenParity) {
        cursor_sharp_blue(tid);
    } else {
        cursor_sharp_pink(tid);
    }

    console_server::Printf(tid, "%c%d ", sensor_buffer[sensorBufferIdx].box, sensor_buffer[sensorBufferIdx].num);
    console_server::Puts(tid, 0, "\033[22m");  // normal intensity
}

void process_sensor_byte(unsigned char byte, int sensorByteIdx, uint32_t printerProprietorTid) {
    int sensorByteCount = sensorByteIdx + 1;
    char box = 'A' + (sensorByteCount - 1) / 2;

    for (int bit = 7; bit >= 0; bit -= 1) {
        if (byte & (1 << bit)) {
            unsigned int sensorNum = (8 - bit);
            if (sensorByteCount % 2 == 0) {  // second side?
                sensorNum += 8;
            }

            printer_proprietor::updateSensor(box, sensorNum, printerProprietorTid);
        }
    }
}

void SensorServer() {
    int printerProprietorTid = name_server::WhoIs(PRINTER_PROPRIETOR_NAME);
    ASSERT(printerProprietorTid >= 0, "UNABLE TO GET PRINTER_PROPRIETOR_NAME\r\n");

    int clockServerTid = name_server::WhoIs(CLOCK_SERVER_NAME);
    ASSERT(clockServerTid >= 0, "UNABLE TO GET CLOCK_SERVER_NAME\r\n");

    int marklinTid = name_server::WhoIs(MARKLIN_SERVER_NAME);
    ASSERT(marklinTid >= 0, "UNABLE TO GET MARKLIN_SERVER_NAME\r\n");

    int nonSensorWindow = 5;
    for (;;) {
        clock_server::Delay(clockServerTid, nonSensorWindow);
        marklin_server::Putc(marklinTid, 0, SENSOR_READ_ALL);
        for (int i = 0; i < 10; i++) {
            unsigned char result = marklin_server::Getc(marklinTid, 0);
            process_sensor_byte(result, i, printerProprietorTid);
        }

    }  // for
}  // for
