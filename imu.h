#ifndef IMU_H_
#define IMU_H_

#include <stdio.h>

void initialize();
void refresh_screen();
int get_window_size(int *columns, int *rows);
void process_user_input();
void set_terminal_flags();
void reset_terminal_flags();
void kill(const char *message);
void open_file(FILE* file, const char* path);
char read_key();

#endif