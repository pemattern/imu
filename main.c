#include "imu.h"
//
int main(int argc, char** argv)
{
    set_terminal_flags();
    initialize();

    while (1)
    {
        refresh_screen();
        process_user_input();
    }

    return 0;
}