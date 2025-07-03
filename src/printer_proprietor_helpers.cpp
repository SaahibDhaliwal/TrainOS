#include "printer_proprietor_helpers.h"

#include "command_server_protocol.h"
#include "console_server_protocol.h"
#include "pos.h"
#include "rpi.h"
#include "string.h"
#include "timer.h"
#include "turnout.h"

/*********** CURSOR ********************************/

static bool g_isCursorVisible = false;

bool get_cursor_visibility() {
    return g_isCursorVisible;
}

void back_space(int console) {
    console_server::Puts(console, 0, "\b \b");
}

void cursor_down_one(int console) {
    console_server::Puts(console, 0, "\033[1B");
}

void clear_current_line(int console) {
    console_server::Puts(console, 0, "\033[2K");
}

void clear_screen(int console) {
    console_server::Puts(console, 0, "\033[2J");
}

void hide_cursor(int console) {
    console_server::Puts(console, 0, "\033[?25l");
    g_isCursorVisible = false;
}

void show_cursor(int console) {
    console_server::Puts(console, 0, "\033[?25h");
    g_isCursorVisible = true;
}

void cursor_top_left(int console) {
    console_server::Puts(console, 0, "\033[H");
}

void cursor_white(int console) {
    console_server::Puts(console, 0, "\033[0m");
}

void cursor_soft_blue(int console) {
    console_server::Puts(console, 0, "\033[38;5;153m");
}

void cursor_soft_pink(int console) {
    console_server::Puts(console, 0, "\033[38;5;218m");
}

void cursor_soft_green(int console) {
    console_server::Puts(console, 0, "\033[38;5;34m");
}

void cursor_soft_red(int console) {
    console_server::Puts(console, 0, "\033[38;5;160m");
}

void cursor_sharp_green(int console) {
    console_server::Puts(console, 0, "\033[38;5;46m");
}

void cursor_sharp_yellow(int console) {
    console_server::Puts(console, 0, "\033[38;5;226m");
}

void cursor_sharp_orange(int console) {
    console_server::Puts(console, 0, "\033[38;5;202m");
}

void cursor_sharp_blue(int console) {
    console_server::Puts(console, 0, "\033[38;5;45m");
}

void cursor_sharp_pink(int console) {
    console_server::Puts(console, 0, "\033[38;5;201m");
}

/*********** IDLE TIME ********************************/

#define IDLE_START_ROW 13
#define IDLE_START_COL 0
#define IDLE_LABEL "Idle Time: "

void print_idle_percentage(int printTid) {
    console_server::Printf(printTid, "\033[%d;%dH%s", IDLE_START_ROW, IDLE_START_COL, IDLE_LABEL);
}

void update_idle_percentage(int percentage, int printTid) {
    console_server::Printf(printTid, "\033[%d;%dH%d.%d%%  ", IDLE_START_ROW, IDLE_START_COL + strlen(IDLE_LABEL),
                           percentage / 100,
                           percentage % 100);  // padded to get rid of excess
}

/*********** UPTIME ********************************/

#define UPTIME_START_ROW 13
#define UPTIME_START_COL 30
#define UPTIME_LABEL "Uptime: "

void print_uptime(int consoleTid) {
    console_server::Printf(consoleTid, "\033[%d;%dH%s", UPTIME_START_ROW, UPTIME_START_COL, UPTIME_LABEL);
}

void update_uptime(int printTid, uint64_t micros) {
    uint64_t millis = micros / 1000;
    uint32_t tenths = (millis % 1000) / 100;
    uint32_t seconds = (millis / 1000) % 60;
    uint32_t minutes = (millis / (1000 * 60)) % 60;
    uint32_t hours = millis / (1000 * 60 * 60);

    console_server::Printf(printTid, "\033[%d;%dH%dh %dm %d.%ds   ", UPTIME_START_ROW,
                           UPTIME_START_COL + strlen(UPTIME_LABEL), hours, minutes, seconds,
                           tenths);  // padded to get rid of excess
}

/*********** ASCII ART ********************************/

void print_ascii_art(int console) {
    cursor_soft_pink(console);
    // clang-format off
	console_server::Puts(console, 0,
	"__| |________________________________________________________________________________| |__\n\r"
	"__   ________________________________________________________________________________   __\n\r"
	"  | |                _____                        _                                  | |  \n\r"
	"  | |               |_   _|     _ _    __ _      (_)     _ _       ___               | |  \n\r"
	"  | |                 | |      | '_|  / _` |     | |    | ' \\     (_-<               | |  \n\r"
	"  | |                _|_|_    _|_|_   \\__,_|    _|_|_   |_||_|    /__/_              | |  \n\r"
	"  | |              _|\"\"\"\"\"| _|\"\"\"\"\"| _|\"\"\"\"\"| _|\"\"\"\"\"| _|\"\"\"\"\"| _|\"\"\"\"|              | | \n\r"
	"  | |              \"`-0-0-' \"`-0-0-' \"`-0-0-' \"`-0-0-' \"`-0-0-' \"`-0-0-'             | |  \n\r"
	"__| |________________________________________________________________________________| |__\n\r"
	"__   ________________________________________________________________________________   __\n\r"
	"  | |                                                                                | |  \n\r"
	);
    // clang-format on
}

/*********** TURNOUTS  ********************************/

#define TURNOUT_TABLE_START_ROW 15
#define TURNOUT_TABLE_START_COL 10

static const Pos turnout_offsets[SINGLE_SWITCH_COUNT + DOUBLE_SWITCH_COUNT] = {
    {2, 1},  {4, 1},  {6, 1},  {8, 1},  {10, 1}, {12, 1},  {2, 8},   {4, 8},  {6, 8},  {8, 8},  {10, 8},
    {12, 8}, {2, 15}, {4, 15}, {6, 15}, {8, 15}, {10, 15}, {12, 15}, {2, 22}, {4, 22}, {6, 22}, {8, 22}};

void draw_turnout_grid_frame(uint32_t consoleTid) {
    // clang-format off
    const char* lines[] = {
        "           Turnouts          ",
        "┌─────╦──────╦──────╦──────┐",
        "│ 1│  ║  7│  ║ 13│  ║ 153│ │",
        "├─────╬──────╬──────╬──────┤",
        "│ 2│  ║  8│  ║ 14│  ║ 154│ │",
        "├─────╬──────╬──────╬──────┤",
        "│ 3│  ║  9│  ║ 15│  ║ 155│ │",
        "├─────╬──────╬──────╬──────┤",
        "│ 4│  ║ 10│  ║ 16│  ║ 156│ │",
        "├─────╬──────╬──────╬──────┘",
        "│ 5│  ║ 11│  ║ 17│  │",
        "├─────╬──────╬──────│",
        "│ 6│  ║ 12│  ║ 18│  │",
        "└─────╩──────╩──────┘\n"
    };
    // clang-format on

    int num_lines = sizeof(lines) / sizeof(lines[0]);

    for (int i = 0; i < num_lines; i += 1) {
        console_server::Printf(consoleTid, "\033[%d;%dH%s", TURNOUT_TABLE_START_ROW + i, TURNOUT_TABLE_START_COL,
                               lines[i]);
    }
}

void update_turnout(Turnout* turnouts, int idx, uint32_t consoleServerTid) {
    Pos pos = turnout_offsets[idx];
    char state = turnouts[idx].state == SwitchState::STRAIGHT ? 'S' : 'C';

    if (pos.col == 22) {
        console_server::Printf(consoleServerTid, "\033[%d;%dH", TURNOUT_TABLE_START_ROW + pos.row,
                               TURNOUT_TABLE_START_COL + pos.col + 4);
    } else {
        console_server::Printf(consoleServerTid, "\033[%d;%dH", TURNOUT_TABLE_START_ROW + pos.row,
                               TURNOUT_TABLE_START_COL + pos.col + 3);
    }

    console_server::Puts(consoleServerTid, 0, "\033[1m");

    if (turnouts[idx].state == SwitchState::STRAIGHT) {
        cursor_sharp_blue(consoleServerTid);
    } else {
        cursor_sharp_pink(consoleServerTid);
    }

    console_server::Putc(consoleServerTid, 0, state);
    console_server::Puts(consoleServerTid, 0, "\033[22m");
}

void print_turnout_table(uint32_t consoleTid) {
    console_server::Printf(consoleTid, "\033[%d;%dH", TURNOUT_TABLE_START_ROW, TURNOUT_TABLE_START_COL);
    draw_turnout_grid_frame(consoleTid);
}

/*********** SENSORS  ********************************/

#define SENSOR_TABLE_START_ROW 15
#define SENSOR_TABLE_START_COL 60
#define SENSOR_BYTE_SIZE 10

void initialize_sensors(Sensor* sensors) {
    for (int i = 0; i < SENSOR_BUFFER_SIZE; i += 1) {
        sensors[i].box = '\0';
        sensors[i].num = 0;
    }
}

void draw_sensor_grid_frame(uint32_t consoleTid) {
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
        console_server::Printf(consoleTid, "\033[%d;%dH%s", SENSOR_TABLE_START_ROW + i, SENSOR_TABLE_START_COL,
                               lines[i]);
    }
}

void print_sensor_table(uint32_t consoleTid) {
    console_server::Printf(consoleTid, "\033[%d;%dH", SENSOR_TABLE_START_ROW, SENSOR_TABLE_START_COL);
    draw_sensor_grid_frame(consoleTid);
}

void update_sensor(Sensor* sensor_buffer, int sensorBufferIdx, int tid, bool evenParity) {
    console_server::Printf(tid, "\033[%d;%dH", SENSOR_TABLE_START_ROW + 2 + sensorBufferIdx,
                           SENSOR_TABLE_START_COL + 7);
    console_server::Puts(tid, 0, "\033[1m");  // bold or increased intensity

    if (evenParity) {
        cursor_sharp_blue(tid);
    } else {
        cursor_sharp_pink(tid);
    }

    console_server::Printf(tid, "%c%d ", sensor_buffer[sensorBufferIdx].box, sensor_buffer[sensorBufferIdx].num);
    console_server::Puts(tid, 0, "\033[22m");  // normal intensity
}

/*********** CLOCKS  ********************************/

void refresh_clocks(int tid, unsigned int idleTime) {
    update_idle_percentage(idleTime, tid);
    update_uptime(tid, timerGet());
}

/*********** COMMAND  ********************************/
#define COMMAND_PROMPT_START_ROW 29
#define COMMAND_PROMPT_START_COL 0

void command_feedback(int tid, command_server::Reply reply) {
    cursor_down_one(tid);
    clear_current_line(tid);
    if (reply == command_server::Reply::SUCCESS) {
        cursor_soft_green(tid);
        console_server::Puts(tid, 0, "✔ Command accepted");
    } else {
        cursor_soft_red(tid);
        console_server::Puts(tid, 0, "✖ Invalid command");
    }  // if
}

void print_command_feedback(uint32_t consoleTid, command_server::Reply reply) {
    cursor_down_one(consoleTid);
    clear_current_line(consoleTid);
    if (reply == command_server::Reply::SUCCESS) {
        cursor_soft_green(consoleTid);
        console_server::Puts(consoleTid, 0, "✔ Command accepted");
    } else {
        cursor_soft_red(consoleTid);
        console_server::Puts(consoleTid, 0, "✖ Invalid command");
    }  // if
}

void print_initial_command_prompt(uint32_t consoleTid) {
    console_server::Printf(consoleTid, "\033[%d;%dH", COMMAND_PROMPT_START_ROW, COMMAND_PROMPT_START_COL);
    clear_current_line(consoleTid);
    console_server::Puts(consoleTid, 0, "\r> ");
}

void print_clear_command_prompt(uint32_t consoleTid) {
    console_server::Printf(consoleTid, "\033[%d;%dH", COMMAND_PROMPT_START_ROW, COMMAND_PROMPT_START_COL);
    clear_current_line(consoleTid);
    console_server::Puts(consoleTid, 0, "\r> ");
}

void print_command_prompt_blocked(uint32_t consoleTid) {
    console_server::Printf(consoleTid, "\033[%d;%dH", COMMAND_PROMPT_START_ROW, COMMAND_PROMPT_START_COL);
    console_server::Puts(consoleTid, 0, "Initializing...");
}

/*********** MEASUREMENTS  ********************************/
#define MEASUREMENT_START_ROW 1
#define MEASUREMENT_START_COL 95

void print_measurement(uint32_t consoleTid, unsigned int measurementNum, const char* message) {
    // console_server::Printf(consoleTid, "\033[%d;%dH", MEASUREMENT_START_ROW + measurementNum, MEASUREMENT_START_COL);
    console_server::Measurement(consoleTid, 0, message);
    console_server::Measurement(consoleTid, 0, "\r\n");
}

void print_train_status(uint32_t consoleTid, char* trainNum, char* sensor) {
    console_server::Printf(consoleTid, "\033[%d;%dHTrainNum: %s Next Sensor: %s", MEASUREMENT_START_ROW,
                           MEASUREMENT_START_COL, trainNum, sensor);
}

/*********** STARTUP  ********************************/
void startup_print(int consoleTid) {
    hide_cursor(consoleTid);
    clear_screen(consoleTid);
    cursor_top_left(consoleTid);

    print_ascii_art(consoleTid);

    cursor_white(consoleTid);
    print_uptime(consoleTid);
    print_idle_percentage(consoleTid);

    print_turnout_table(consoleTid);
    print_sensor_table(consoleTid);

    cursor_soft_pink(consoleTid);
    print_command_prompt_blocked(consoleTid);
    cursor_white(consoleTid);
}