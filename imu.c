#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <ctype.h>
#include <string.h>
#include <math.h>

#include "imu.h"
#include "append_buffer.h"

#define CTRL_KEY(k) ((k) & 0x1f)

enum key_mapping
{
    CURSOR_LEFT = 1000,
    CURSOR_RIGHT,
    CURSOR_UP,
    CURSOR_DOWN
};

typedef struct row_buffer
{
    int size;
    char *chars;
} row_buffer;

struct state
{
    int cursor_x, cursor_y;
    int screen_columns, screen_rows;
    int row_count;
    int row_offset;
    row_buffer *row;
    struct termios original_termios;
};

struct state state;

void clear_screen_direct()
{
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3); 
}

void kill(const char* message)
{
    clear_screen_direct();
    
    perror(message);
    exit(1);
}

void clear_rest_of_line(struct append_buffer *ab)
{
    append(ab, "\x1b[K", 3);
}

void hide_cursor(struct append_buffer *ab)
{
    append(ab, "\x1b[?25l", 6);
}

void reset_cursor(struct append_buffer *ab)
{
    append(ab, "\x1b[H", 3);
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

void scroll()
{
    if (state.cursor_y < state.row_offset)
    {
        state.row_offset = state.cursor_y;
    }
    if (state.cursor_y >= state.row_offset + state.screen_rows)
    {
        state.row_offset = state.cursor_y - state.screen_rows + 1;
    }
}

void draw_rows(struct append_buffer *ab)
{
    int y;
    for (y = 0; y < state.screen_rows; y++)
    {
        int filerow = y + state.row_offset;
        if (filerow >= state.row_count)
        {
            append(ab, "~", 1);
        }
        else
        {
            int length = state.row[filerow].size;
            if (length > state.screen_columns) length = state.screen_columns;
            append(ab, state.row[filerow].chars, length);
        }

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
    scroll();

    struct append_buffer ab = APPEND_BUFFER_INIT;

    hide_cursor(&ab);
    reset_cursor(&ab);

    draw_rows(&ab);

    update_cursor_position(&ab);
    show_cursor(&ab);

    write(STDOUT_FILENO, ab.buffer, ab.length);
    free_append_buffer(&ab);
}

int read_key()
{
    int key;
    char c;
    while ((key = read(STDIN_FILENO, &c, 1)) != 1)
    {
        if (key == -1 && errno != EAGAIN)
            kill("failed to read key");
    }

    if (c == '\x1b') 
    {
        char sequence[3];
        if (read(STDIN_FILENO, &sequence[0], 1) != 1) return '\x1b';
        if (read(STDIN_FILENO, &sequence[1], 1) != 1) return '\x1b';
        if (sequence[0] == '[') 
        {
            switch (sequence[1]) 
            {
                case 'A': return CURSOR_UP;
                case 'B': return CURSOR_DOWN;
                case 'C': return CURSOR_RIGHT;
                case 'D': return CURSOR_LEFT;
            }
        }
        return '\x1b';
    } 
    else 
    {
        return c;
    }
}

void move_cursor(int key)
{
    switch (key)
    {
        case CURSOR_LEFT:
            if (state.cursor_x != 0) state.cursor_x--;
            break;
        case CURSOR_RIGHT:
            if (state.cursor_x != state.screen_columns - 1) state.cursor_x++;
            break;
        case CURSOR_UP:
            if (state.cursor_y != 0) state.cursor_y--;
            break;
        case CURSOR_DOWN:
            if (state.cursor_y != state.row_count) state.cursor_y++;
            break;
    }
}

void process_user_input()
{
    int c = read_key();

    switch (c)
    {
        case CTRL_KEY('q'):
            clear_screen_direct();
            exit(0);
            break;
        
        case CURSOR_UP:
        case CURSOR_DOWN:
        case CURSOR_LEFT:
        case CURSOR_RIGHT:
            move_cursor(c);
            break;
    }
}

void reset_terminal_flags()
{
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &state.original_termios) == -1)
        kill("error resetting termios attributes");
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

void append_row(const char *s, size_t length)
{
    state.row = realloc(state.row, sizeof(row_buffer) * (state.row_count + 1));

    int index = state.row_count;
    state.row[index].size = length;
    state.row[index].chars = malloc(length + 1);
    memcpy(state.row[index].chars, s, length);
    state.row[index].chars[length] = '\0';
    state.row_count++;
}

void open_file(const char *filename)
{
    FILE *file_ptr = fopen(filename, "r");
    if (!file_ptr) kill("failed to open file");

    char *line = NULL;
    size_t cap = 0;
    ssize_t length;

    while ((length = getline(&line, &cap, file_ptr)) != -1)
    {
        while (length > 0 && (line[length - 1] == '\n' || line[length - 1] == '\r'))
            length--;
        append_row(line, length);        
    }
    free(line);
    fclose(file_ptr);
}

void initialize()
{
    set_terminal_flags();

    state.cursor_x = 0;
    state.cursor_y = 0;
    state.row_count = 0;
    state.row_offset = 0;
    state.row = NULL;

    clear_screen_direct();

    if (get_window_size(&state.screen_columns, &state.screen_rows) == -1)
        kill("failed to get window size");
}