#include "sensor_task.h"

#include <stdbool.h>
#include <string.h>

#include "clock_server_protocol.h"
#include "command.h"
#include "config.h"
#include "generic_protocol.h"
#include "interrupt.h"
#include "intrusive_node.h"
#include "localization_server_protocol.h"
#include "marklin_server.h"
#include "marklin_server_protocol.h"
#include "name_server.h"
#include "name_server_protocol.h"
#include "pos.h"
#include "printer_proprietor.h"
#include "printer_proprietor_protocol.h"
#include "queue.h"
#include "ring_buffer.h"
#include "rpi.h"
#include "stack.h"
#include "sys_call_stubs.h"
#include "test_utils.h"
#include "timer.h"

void process_sensor_byte(unsigned char byte, int sensorByteIdx, uint32_t localizationServerTid) {
    int sensorByteCount = sensorByteIdx + 1;
    char box = 'A' + (sensorByteCount - 1) / 2;

    for (int bit = 7; bit >= 0; bit -= 1) {
        if (byte & (1 << bit)) {
            unsigned int sensorNum = (8 - bit);
            if (sensorByteCount % 2 == 0) {  // second side?
                sensorNum += 8;
            }

            localization_server::updateSensor(localizationServerTid, box, sensorNum);
        }
    }
}

void SensorTask() {
    int printerProprietorTid = name_server::WhoIs(PRINTER_PROPRIETOR_NAME);
    ASSERT(printerProprietorTid >= 0, "UNABLE TO GET PRINTER_PROPRIETOR_NAME\r\n");

    int clockServerTid = name_server::WhoIs(CLOCK_SERVER_NAME);
    ASSERT(clockServerTid >= 0, "UNABLE TO GET CLOCK_SERVER_NAME\r\n");

    int marklinTid = name_server::WhoIs(MARKLIN_SERVER_NAME);
    ASSERT(marklinTid >= 0, "UNABLE TO GET MARKLIN_SERVER_NAME\r\n");

    int localizationServerTid = sys::MyParentTid();
    ASSERT(marklinTid >= 0, "UNABLE TO GET SENSOR TASK'S PARENT\r\n");

    for (;;) {
        clock_server::Delay(clockServerTid, 5);
        marklin_server::Putc(marklinTid, 0, SENSOR_READ_ALL);
        for (int i = 0; i < 10; i++) {
            unsigned char result = marklin_server::Getc(marklinTid, 0);
            if (result) process_sensor_byte(result, i, localizationServerTid);
        }

    }  // for
}  // for
