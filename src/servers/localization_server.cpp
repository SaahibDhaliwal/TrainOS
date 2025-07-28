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
#include "train_manager.h"
#include "train_protocol.h"
#include "train_task.h"
#include "turnout.h"

using namespace localization_server;

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
            printer_proprietor::updateTurnout(printerProprietorTid, static_cast<Command_Byte>(replyBuff[0]),
                                              turnoutIdx(replyBuff[1]));
        } else {
            marklin_server::solenoidOff(marklinServerTid);
        }

        // could this delay stop us and have a higher priority task run and have the solenoid burn?
        // answer: yes, let's give it higher prio - saahib
        clock_server::Delay(clockServerTid, 20);
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

    uint32_t turnoutNotifierTid = sys::Create(30, &TurnoutNotifier);

    uint32_t clientTid = 0;
    sys::Receive(&clientTid, nullptr, 0);
    ASSERT(clientTid == turnoutNotifierTid, "LOCALIZATION SERVER DID NOT RECEIVE FROM NOTIFIER\r\n");

    TrainManager trainManager(marklinServerTid, printerProprietorTid, clockServerTid, turnoutNotifierTid);

    uint32_t sensorTid = sys::Create(24, &SensorTask);
    printer_proprietor::debugPrintF(printerProprietorTid, "localization server TID: %u", sys::MyTid());

    for (;;) {
        uint32_t clientTid;
        char receiveBuffer[Config::MAX_MESSAGE_LENGTH] = {0};
        int msgLen = sys::Receive(&clientTid, receiveBuffer, Config::MAX_MESSAGE_LENGTH - 1);
        receiveBuffer[msgLen] = '\0';  // is this breaking things?

        if (clientTid == commandServerTid) {
            Command command = commandFromByte(receiveBuffer[0]);
            switch (command) {
                case Command::SET_SPEED: {
                    trainManager.setTrainSpeed(receiveBuffer);
                    break;
                }
                // case Command::REVERSE_TRAIN: {
                //     int trainNumber = 10 * a2d(receiveBuffer[1]) + a2d(receiveBuffer[2]);
                //     int trainIdx = trainNumToIndex(trainNumber);
                //     trainManager.reverseTrain(trainIdx);
                //     break;
                // }
                case Command::SET_TURNOUT: {
                    char turnoutDirection = receiveBuffer[1];
                    char turnoutNumber = receiveBuffer[2];

                    Turnout *turnouts = trainManager.getTurnouts();
                    TrackNode *track = trainManager.getTrack();

                    int index = turnoutIdx(turnoutNumber);
                    turnouts[index].state = static_cast<SwitchState>(turnoutDirection);

                    marklin_server::setTurnout(marklinServerTid, turnoutDirection, turnoutNumber);

                    TrackNode *newNextSensor = getNextSensor(&track[(2 * index) + 80], turnouts);
                    ASSERT(newNextSensor != nullptr, "newNextSensor == nullptr");
                    // start at the reverse of the new upcoming sensor, so we can work backwards to update sensors
                    setAllImpactedSensors(newNextSensor->reverse, turnouts, newNextSensor, 0);  // work backwards
                    // if you were to follow the track backwards, you should get a sensor. That sensor should have this
                    // new next sensor updated
                    ASSERT(newNextSensor == newNextSensor->reverse->nextSensor->reverse->nextSensor,
                           "An impacted sensor was not updated");
                    break;
                }
                case Command::SOLENOID_OFF: {
                    marklin_server::solenoidOff(marklinServerTid);
                    break;
                }
                case Command::RESET_TRACK: {
                    // this can be more efficient in the future, like checking how the current switch
                    // state compares to the default
#if defined(TRACKA)
                    initialTurnoutConfigTrackA(trainManager.getTurnouts());
                    initializeTurnouts(trainManager.getTurnouts(), marklinServerTid, printerProprietorTid,
                                       clockServerTid);
#else
                    initialTurnoutConfigTrackB(trainManager.getTurnouts());
                    initializeTurnouts(trainManager.getTurnouts(), marklinServerTid, printerProprietorTid,
                                       clockServerTid);
#endif
                    initTrackSensorInfo(trainManager.getTrack(), trainManager.getTurnouts());
                    break;
                }
                case Command::SET_STOP: {
                    trainManager.setTrainStop(receiveBuffer);
                    break;
                }
                case Command::INIT_TRAIN: {
                    int trainIndex = receiveBuffer[1] - 1;
                    Sensor initSensor = Sensor{.box = receiveBuffer[2], .num = static_cast<uint8_t>(receiveBuffer[3])};
                    trainManager.initializeTrain(trainIndex, initSensor);
                    break;
                }
                default: {
                    ASSERT(1 == 2, "bad localization server command");
                }
            }
            ASSERT(clientTid < 100, "client broke here");
            emptyReply(clientTid);
        } else if (clientTid == sensorTid) {
            trainManager.processSensorReading(receiveBuffer);
            emptyReply(clientTid);
        } else if (clientTid == turnoutNotifierTid) {
            trainManager.processTurnoutNotifier();  // will reply depending on the state of the turnout queue
        } else if (clientTid >= trainManager.getSmallestTrainTid() && clientTid <= trainManager.getLargestTrainTid()) {
            trainManager.processTrainRequest(receiveBuffer, clientTid);

        } else {
            ASSERT(0, "Localization server received from someone unexpected. TID: %u", clientTid);
        }
    }

    sys::Exit();
}