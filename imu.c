#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <ctype.h>
#include <string.h>

#include "imu.h"
#include "append_buffer.h"

#define CTRL_KEY(k) ((k) & 0x1f)

struct state
{
    int cursor_x, cursor_y;
    int screen_columns, screen_rows;
    struct termios original_termios;
};

struct state state;

void clear_screen_direct()
{
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3); 
}

void initialize()
{
    state.cursor_x = 0;
    state.cursor_y = 0;

    clear_screen_direct();

    if (get_window_size(&state.screen_columns, &state.screen_rows) == -1)
        kill("failed to get window size");
}

void clear_rest_of_line(struct append_buffer *ab)
{
    append(ab, "\x1b[H", 3);
}

void hide_cursor(struct append_buffer *ab)
{
    append(ab, "\x1b[?25l", 6);
}

void show_cursor(struct append_buffer *ab)
{
    append(ab, "\x1b[?25h", 6);
}

void clear_screen(struct append_buffer *ab)
{
    append(ab, "\x1b[2J", 4);
    append(ab, "\x1b[H", 3); 
}

void draw_rows(struct append_buffer *ab)
{
    int y;
    for (y = 0; y < state.screen_rows; y++)
    {
        append(ab, "~", 1);
        clear_rest_of_line(ab);

        if (y < state.screen_rows - 1)
        {
            append(ab, "\r\n", 2);
        }
    }
}

int get_cursor_position(int *columns, int *rows) {
    char buffer[32];
    unsigned int i = 0;

    if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) return -1;
    
    while (i < sizeof(buffer) - 1)
    {
        if (read(STDIN_FILENO, &buffer[i], 1) != 1) break;
        if (buffer[i] == 'R') break;
        i++;
    }
    buffer[i] = '\0';

    if (buffer[0] != '\x1b' || buffer[1] != '[') return -1;
    if (sscanf(&buffer[2], "%d;%d", rows, columns) != 2) return -1;

    return 0;
}

int get_window_size(int *columns, int *rows)
{
    struct winsize window_size;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &window_size) == -1 || window_size.ws_col == 0) 
    {
        if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) return -1;
        return get_cursor_position(columns, rows);
    }
    else 
    {
        *columns = window_size.ws_col;
        *rows = window_size.ws_row;
        return 0;
    }
}

void update_cursor_position(struct append_buffer *ab)
{
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "\x1b[%d;%dH", state.cursor_y + 1, state.cursor_x + 1);
    append(ab, buffer, strlen(buffer));
}

void refresh_screen()
{
    struct append_buffer ab = APPEND_BUFFER_INIT;

    hide_cursor(&ab);
    clear_rest_of_line(&ab);

    draw_rows(&ab);

    update_cursor_position(&ab);
    show_cursor(&ab);

    write(STDOUT_FILENO, ab.buffer, ab.length);
    free_append_buffer(&ab);
}

char read_key()
{
    int key;
    char c;
    while ((key = read(STDIN_FILENO, &c, 1)) != 1)
    {
        if (key == -1 && errno != EAGAIN)
            kill("failed to read key");
    }
    return c;
}

void move_cursor(char key)
{
    switch (key)
    {
        case 'a':
            state.cursor_x--;
            break;
        case 'd':
            state.cursor_x++;
            break;
        case 'w':
            state.cursor_y--;
            break;
        case 's':
            state.cursor_y++;
            break;
    }
}

void process_user_input()
{
    char c = read_key();

    switch (c)
    {
        case CTRL_KEY('q'):
            clear_screen_direct();
            exit(0);
            break;
        
        case 'w':
        case 's':
        case 'a':
        case 'd':
            move_cursor(c);
            break;
    }
}

void kill(const char* message)
{
    clear_screen_direct();
    
    perror(message);
    exit(1);
}

void set_terminal_flags()
{
    if (tcgetattr(STDIN_FILENO, &state.original_termios) == -1)
        kill("error retrieving termios attributes");
    atexit(reset_terminal_flags);

    struct termios imu_termios = state.original_termios;
    imu_termios.c_iflag &= ~(ICRNL | IXON | BRKINT | INPCK | ISTRIP);
    imu_termios.c_oflag &= ~(OPOST);
    imu_termios.c_cflag |= (CS8);
    imu_termios.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
    imu_termios.c_cc[VMIN] = 0;
    imu_termios.c_cc[VTIME] = 1;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &imu_termios) == -1)
        kill("error setting termios attributes");
}

void reset_terminal_flags()
{
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &state.original_termios) == -1)
        kill("error resetting termios attributes");
}

void open_file(FILE* file, const char* path)
{
    file = fopen(path, "r+");

    if (file == NULL)
        perror("File could not be opened.");
}