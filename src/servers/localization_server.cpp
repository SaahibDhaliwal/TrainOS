#include "localization_server.h"

#include <utility>

#include "clock_server_protocol.h"
#include "command.h"
#include "command_client.h"
#include "command_server.h"
#include "command_server_protocol.h"
#include "config.h"
#include "console_server_protocol.h"
#include "generic_protocol.h"
#include "localization_helper.h"
#include "marklin_server.h"
#include "marklin_server_protocol.h"
#include "name_server_protocol.h"
#include "pathfinding.h"
#include "printer_proprietor.h"
#include "printer_proprietor_protocol.h"
#include "ring_buffer.h"
#include "rpi.h"
#include "sensor_task.h"
#include "sys_call_stubs.h"
#include "test_utils.h"
#include "timer.h"
#include "track_data.h"
#include "train.h"
#include "train_manager.h"
#include "turnout.h"

using namespace localization_server;

void StopTrain() {
    int clockServerTid = name_server::WhoIs(CLOCK_SERVER_NAME);
    ASSERT(clockServerTid >= 0, "UNABLE TO GET CLOCK_SERVER_NAME\r\n");
    int parentTid = sys::MyParentTid();
    char replyBuff[8];  // assuming our ticks wont be more than 99999 ticks
    unsigned int delayAmount = 0;
    for (;;) {
        int res = sys::Send(parentTid, nullptr, 0, replyBuff, 7);
        handleSendResponse(res, parentTid);
        a2ui(replyBuff, 10, &delayAmount);
        clock_server::Delay(clockServerTid, delayAmount);
    }
    sys::Exit();
}

#define DONE_TURNOUTS 1
void TurnoutNotifier() {
    int clockServerTid = name_server::WhoIs(CLOCK_SERVER_NAME);
    ASSERT(clockServerTid >= 0, "UNABLE TO GET CLOCK_SERVER_NAME\r\n");
    int marklinServerTid = name_server::WhoIs(MARKLIN_SERVER_NAME);
    ASSERT(clockServerTid >= 0, "UNABLE TO GET MARKLIN_SERVER_NAME\r\n");

    int printerProprietorTid = name_server::WhoIs(PRINTER_PROPRIETOR_NAME);
    ASSERT(printerProprietorTid >= 0, "UNABLE TO GET PRINTER PROPRIETOR\r\n");

    int parentTid = sys::MyParentTid();
    for (;;) {
        char replyBuff[3];
        int res = sys::Send(parentTid, nullptr, 0, replyBuff, 3);
        handleSendResponse(res, parentTid);

        if (replyBuff[0] != DONE_TURNOUTS) {
            marklin_server::setTurnout(marklinServerTid, replyBuff[0], replyBuff[1]);
        } else {
            marklin_server::solenoidOff(marklinServerTid);
            clock_server::Delay(clockServerTid, 20);
            continue;
        }
        // could this delay stop us and have a higher priority task run and have the solenoid burn?
        clock_server::Delay(clockServerTid, 20);  // give the turnout time to turn on
        printer_proprietor::updateTurnout(printerProprietorTid, static_cast<Command_Byte>(replyBuff[0]),
                                          turnoutIdx(replyBuff[1]));

        char sendBuff[100] = {0};
        if (replyBuff[0] == 33) {
            printer_proprietor::formatToString(sendBuff, 100, "set turnout: %d to straight", replyBuff[1]);
        } else {
            printer_proprietor::formatToString(sendBuff, 100, "set turnout: %d to curved", replyBuff[1]);
        }
    }
    sys::Exit();
}

void LocalizationServer() {
    int registerReturn = name_server::RegisterAs(LOCALIZATION_SERVER_NAME);
    ASSERT(registerReturn != -1, "UNABLE TO REGISTER CONSOLE SERVER\r\n");

    int marklinServerTid = name_server::WhoIs(MARKLIN_SERVER_NAME);
    ASSERT(marklinServerTid >= 0, "UNABLE TO GET MARKLIN_SERVER_NAME\r\n");

    int printerProprietorTid = name_server::WhoIs(PRINTER_PROPRIETOR_NAME);
    ASSERT(printerProprietorTid >= 0, "UNABLE TO GET PRINTER_PROPRIETOR_NAME\r\n");

    int clockServerTid = name_server::WhoIs(CLOCK_SERVER_NAME);
    ASSERT(clockServerTid >= 0, "UNABLE TO GET CLOCK_SERVER_NAME\r\n");

    // could just be MyParent()
    int commandServerTid = name_server::WhoIs(COMMAND_SERVER_NAME);
    ASSERT(commandServerTid >= 0, "UNABLE TO GET CLOCK_SERVER_NAME\r\n");

    uint32_t stoppingTid = sys::Create(42, &StopTrain);
    uint32_t client = 0;
    int res = sys::Receive(&client, nullptr, 0);
    ASSERT(client == stoppingTid, "localization startup received from someone besides the stoptrain task");

    uint32_t turnoutNotifierTid = sys::Create(30, &TurnoutNotifier);
    res = sys::Receive(&client, nullptr, 0);
    ASSERT(client == turnoutNotifierTid, "localization startup received from someone besides the turnoutNotifier task");

    TrainManager trainManager(marklinServerTid, printerProprietorTid, clockServerTid, stoppingTid, turnoutNotifierTid);

    uint32_t sensorTid = sys::Create(20, &SensorTask);

    for (;;) {
        uint32_t clientTid;
        char receiveBuffer[21] = {0};
        int msgLen = sys::Receive(&clientTid, receiveBuffer, 20);
        receiveBuffer[msgLen] = '\0';

        if (clientTid == commandServerTid) {
            trainManager.processInputCommand(receiveBuffer);
            emptyReply(clientTid);

        } else if (clientTid == sensorTid) {
            trainManager.processSensorReading(receiveBuffer);
            emptyReply(clientTid);

        } else if (clientTid == trainManager.getReverseTid()) {
            trainManager.processReverse();
            // no need to reply, reverse task is dead
        } else if (clientTid == stoppingTid) {
            // we have delayed enough, issue the stop command:
            // ideally, we know which train this is (we are the localization server)
            // current implementation only works for one train
            trainManager.processStopping();
        } else if (clientTid == turnoutNotifierTid) {
            trainManager.processTurnoutNotifier();

        } else {
            ASSERT(0, "Localization server received from someone unexpected. TID: %u", clientTid);
        }
    }

    sys::Exit();
}