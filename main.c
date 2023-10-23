#include "imu.h"

int main(int argc, char** argv)
{
    initialize();

    if (argc >= 2)
    {
        open_file(argv[1]);
    }

    while (1)
    {
        refresh_screen();
        process_user_input();
    }

    return 0;
}