#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <stdlib.h>

#include "imu.h"

struct termios original_termios;

void kill(const char* message)
{
    perror(message);
    exit(1);
}

void set_terminal_flags()
{
    if (tcgetattr(STDIN_FILENO, &original_termios) == -1) kill("error retrieving termios attributes");
    atexit(reset_terminal_flags);

    struct termios imu_termios = original_termios;
    imu_termios.c_iflag &= ~(ICRNL | IXON | BRKINT | INPCK | ISTRIP);
    imu_termios.c_oflag &= ~(OPOST);
    imu_termios.c_cflag |= (CS8);
    imu_termios.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
    imu_termios.c_cc[VMIN] = 0;
    imu_termios.c_cc[VTIME] = 1;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &imu_termios) == -1) kill("error setting termios attributes");
}

void reset_terminal_flags()
{
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &original_termios) == -1) kill("error resetting termios attributes");
}

void open_file(FILE* file, const char* path)
{
    file = fopen(path, "r+");

    if (file == NULL) perror("File could not be opened.");
}