
#include "display_refresher.h"

#include "clock_server.h"
#include "clock_server_protocol.h"
#include "name_server_protocol.h"
#include "printer_proprietor_protocol.h"
#include "sys_call_stubs.h"
#include "test_utils.h"

void displayRefresher() {
    int clockServerTid = name_server::WhoIs(CLOCK_SERVER_NAME);
    ASSERT(clockServerTid >= 0, "UNABLE TO GET CLOCK_SERVER_NAME\r\n");

    uint32_t printerTid = sys::MyParentTid();

    for (;;) {
        clock_server::Delay(clockServerTid, 10);
        printer_proprietor::refreshClocks(printerTid);
    }
}