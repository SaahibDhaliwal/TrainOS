#ifndef __PRINTER_PROPRIETOR_HELPERS__
#define __PRINTER_PROPRIETOR_HELPERS__

#include "command_server_protocol.h"
#include "pos.h"
#include "rpi.h"
#include "sensor.h"
#include "string.h"
#include "timer.h"
#include "turnout.h"

/*********** CURSOR ********************************/

bool get_cursor_visibility();

void back_space(int console);
void cursor_down_one(int console);
void clear_current_line(int console);
void clear_screen(int console);
void hide_cursor(int console);
void show_cursor(int console);
void cursor_top_left(int console);

void cursor_white(int console);
void cursor_soft_blue(int console);
void cursor_soft_pink(int console);
void cursor_soft_green(int console);
void cursor_soft_red(int console);
void cursor_sharp_green(int console);
void cursor_sharp_yellow(int console);
void cursor_sharp_orange(int console);
void cursor_sharp_blue(int console);
void cursor_sharp_pink(int console);

/*********** IDLE TIME ********************************/

void print_idle_percentage(int printTid);
void update_idle_percentage(int percentage, int printTid);

/*********** UPTIME ********************************/

void print_uptime(int consoleTid);
void update_uptime(int printTid, uint64_t micros);

/*********** ASCII ART ********************************/

void print_ascii_art(int console);

/*********** TURNOUTS ********************************/

void draw_turnout_grid_frame(uint32_t consoleTid);
void update_turnout(Turnout* turnouts, int idx, uint32_t consoleServerTid);
void print_turnout_table(uint32_t consoleTid);

/*********** SENSORS ********************************/

#define SENSOR_BYTE_SIZE 10

void initialize_sensors(Sensor* sensors);
void draw_sensor_grid_frame(uint32_t consoleTid);
void print_sensor_table(uint32_t consoleTid);
void update_sensor(uint32_t consoleTid, const char* msg, int sensorBufferIdx, bool evenParity);
void update_sensor_time(uint32_t consoleTid, const char* msg, int sensorBufferIdx);

/*********** CLOCKS ********************************/

void refresh_clocks(int tid, unsigned int idleTime);

/*********** COMMAND ********************************/

void command_feedback(int tid, command_server::Reply reply);
void print_command_feedback(uint32_t consoleTid, command_server::Reply reply);
void print_initial_command_prompt(uint32_t consoleTid);
void print_clear_command_prompt(uint32_t consoleTid);
void print_command_prompt_blocked(uint32_t consoleTid);

/*********** MEASURMENTS ********************************/
void print_train_status(uint32_t consoleTid, const char* message);
void print_measurement(uint32_t consoleTid, unsigned int measurementNum, const char* message);

/*********** DEBUG ********************************/
void print_debug(uint32_t consoleTid, int row, const char* message);

/*********** STARTUP ********************************/

void startup_print(int consoleTid);

#endif
