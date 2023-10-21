imu: main.c
	$(CC) main.c imu.c append_buffer.c -o imu -Wall -Wextra -pedantic -std=c99