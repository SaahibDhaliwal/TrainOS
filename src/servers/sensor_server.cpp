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
#include "clock_server.h"
#include "clock_server_protocol.h"
#include "ring_buffer.h"
#include "stack.h"
#include "sys_call_stubs.h"
#include "test_utils.h"
#include "timer.h"

// using namespace marklin_command;

void SensorServer() {
    uint32_t commandTid = sys::MyParentTid();
    uint32_t clockTid = name_server::WhoIs(CLOCK_SERVER_NAME);
    uint32_t marklinTid = name_server::WhoIs(MARKLIN_SERVER_NAME);

    int nonSensorWindow = 10;
    unsigned char requestChar = 0x85;

    for (;;) {
        // delay
        clock_server::Delay(clockTid, nonSensorWindow);

        // marklin request
        marklin_server::Putc(marklinTid, 0, requestChar);

        char sensorBase = 'A';
        bool first8 = true;
        int sensorval = 1;

        // we expect to getC ten times. NO HANDLING OF TIMEOUT
        for (int i = 0; i < 11; i++) {
            char result = marklin_server::Getc(marklinTid, 0);

            if (result) {
                first8 ? sensorval = 1 : sensorval = 9;

                char commandSend[3] = {0};
                // I imagine the first byte is some command ID
                for (int i = 0; i < 8; i++) {
                    if (result & (1 << (7 - i))) {
                        sensorval = i + sensorval;  // sensor 1, or 2, ..
                        // if we got something, send it to the command
                        if (!first8) {
                            sensorval += 8;
                        }
                        commandSend[1] = sensorBase;
                        commandSend[2] = sensorval;
                        int res = sys::Send(commandTid, commandSend, 3, nullptr, 0);
                        handleSendResponse(res, commandTid);
                    }
                }
            }

            if (first8) {
                first8 = false;
            } else {
                // we got the last 8 sensors of the bank,
                // so our next reading will have the first 8 sensors.
                // and advance our sensor base
                first8 = true;
                sensorBase++;
            }
        }  // for
    }  // for
}