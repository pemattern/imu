#ifndef IMU_H_
#define IMU_H_

#include <stdio.h>

#define CTRL_KEY(k) ((k) & 0x1f)

void kill(const char* message);
void set_terminal_flags();
void reset_terminal_flags();
void open_file(FILE* file, const char* path);

#endif