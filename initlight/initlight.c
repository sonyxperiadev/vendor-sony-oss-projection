/*
 * Copyright (C) 2017 AngeloGioacchino Del Regno <kholk11@gmail.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

//#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include <termios.h>

#include <private/android_filesystem_config.h>
#include <utils/Log.h>


#define LOG_TAG			"InitLight"

#define MICRO_UART		"/dev/ttyHS1"

#define SERPARM_BAUDRATE	B115200		/* 115200bps */
#define SERPARM_IFLAGS		(IGNPAR)	/* Parity enable */

				/* 8n1, RX Enabled */
#define SERPARM_CFLAGS		(CS8 | CREAD)
#define SERPARM_OFLAGS		(0)
#define SERPARM_LFLAGS		(0)


				/****  VT    SO   */
static const uint8_t std_header[] = { 0x0b, 0x0e };

static const uint8_t cmd_init[]	  = { 0x02, 0x00, 0x02 };
static const uint8_t cmd_unk1[]	  = { 0x02, 0x01, 0x03 };
static const uint8_t cmd_fan_on[] = { 0x05, 0x80, 0x01,
				      0x03, 0x7f, 0x08 };
static const uint8_t cmd_unk3[]	= { 0x04, 0x55, 0x01,
				    0x01, 0x5b };
static const uint8_t cmd_led_on[] = { 0x08, 0x20, 0x04,
				      0x00, 0x00, 0x00,
				      0x00, 0x00, 0x2c };

				/***   CR    LF    SI   */
static const uint8_t std_footer[] = { 0x0d, 0x0a, 0x0f };

int sendcmd(int fd, uint8_t cmd[], int cmd_sz)
{
	int rc, sz_ans = 32;
	char buf[40];

	do {
		write(fd, cmd, cmd_sz);
		sleep(1);

		ioctl(fd, FIONREAD, &sz_ans);
		if (sz_ans <= 0)
			continue;

		if (sz_ans > 40)
			sz_ans = 40;

		rc = read(fd, buf, sz_ans);
		if (buf[0] == 0x0b &&
		    buf[1] == 0x0e)
			return 0;
		else
			ALOGE("INVALID RX DATA: 0x%x 0x%x",
				buf[0], buf[1]);
	} while (1);

	return -1;
}

int send_concat_cmd(int fd, const uint8_t head[],
			const uint8_t cmd[],
			int head_len, int cmd_len)
{
	int footer_len = sizeof(std_footer) / sizeof(std_footer[0]);
	int full_sz = (head_len + cmd_len + footer_len) * sizeof(uint8_t);
	uint8_t *full_cmd = malloc(full_sz);

	if (full_cmd == NULL) {
		ALOGE("Memory exhausted. Cannot allocate.\n");
		return -1;
	}

	memcpy(full_cmd, head, head_len);
	memcpy(full_cmd + head_len, cmd, cmd_len);
	memcpy(full_cmd + head_len + cmd_len, std_footer, footer_len);

	return sendcmd(fd, full_cmd, full_sz);
}

int send_init_sequence(int fd)
{
	int rc;
	int head_len = sizeof(std_header) / sizeof(std_header[0]);
	int cmd_len;

	cmd_len = sizeof(cmd_init) / sizeof(cmd_init[0]);
	rc = send_concat_cmd(fd, std_header, cmd_init, head_len, cmd_len);
	if (rc)
		goto end;

	cmd_len = sizeof(cmd_unk1) / sizeof(cmd_unk1[0]);
	rc = send_concat_cmd(fd, std_header, cmd_unk1, head_len, cmd_len);
	if (rc)
		goto end;

	cmd_len = sizeof(cmd_fan_on) / sizeof(cmd_fan_on[0]);
	rc = send_concat_cmd(fd, std_header, cmd_fan_on, head_len, cmd_len);
	if (rc)
		goto end;

	cmd_len = sizeof(cmd_unk3) / sizeof(cmd_unk3[0]);
	rc = send_concat_cmd(fd, std_header, cmd_unk3, head_len, cmd_len);
	if (rc)
		goto end;

	cmd_len = sizeof(cmd_led_on) / sizeof(cmd_led_on[0]);
	rc = send_concat_cmd(fd, std_header,
				cmd_led_on, head_len, cmd_len);
end:
	if (rc)
		ALOGE("ERROR: Cannot send init sequence\n");

	return rc;
}

int main(void)
{
	struct termios tty;
	int rc;
	int serport = open(MICRO_UART, (O_RDWR | O_NOCTTY | O_SYNC));

	if (serport < 0) {
		ALOGE("Error: cannot open the serial port.");
		return -1;
	}

	memset(&tty, 0, sizeof tty);

	/* Get current port attributes */
	if (tcgetattr(serport, &tty) != 0) {
		ALOGE("Error: cannot get port attributes\n");
		rc = -1;
		goto end;
	}

	/* Set flags */
	tty.c_cflag = SERPARM_CFLAGS;
	tty.c_iflag = SERPARM_IFLAGS;
	tty.c_oflag = 0;
	tty.c_lflag = 0;

	/* Set communication speed */
        cfsetospeed(&tty, SERPARM_BAUDRATE);
        cfsetispeed(&tty, SERPARM_BAUDRATE);

	/* Flush the comms */
	tcflush(serport, TCIFLUSH);

	/* Send configuration to kernel */
	if (tcsetattr(serport, TCSANOW, &tty) != 0) {
		ALOGE("Error: cannot set port attributes\n");
		rc = -1;
		goto end;
	}

	rc = send_init_sequence(serport);
	if (rc != 0)
		goto end;

end:
	close(serport);
	return rc;
}

