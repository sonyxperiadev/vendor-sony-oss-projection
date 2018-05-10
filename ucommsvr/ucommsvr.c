/*
 * Micro Communicator for Projection uC
 * a High-Speed Serial communications server
 *
 * Copyright (C) 2018 AngeloGioacchino Del Regno <kholk11@gmail.com>
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
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <termios.h>

#include <private/android_filesystem_config.h>
#include <utils/Log.h>

#include <libpolyreg/polyreg.h>
#include "ucomm_private.h"
#include "ucomm_input.h"
#include "ucomm_ext.h"

#define LOG_TAG			"MicroComm"

#define MICRO_UART		"/dev/ttyHS1"

#define SERPARM_BAUDRATE	B115200		/* 115200bps */
#define SERPARM_IFLAGS		(IGNPAR)	/* Parity enable */

				/* 8n1, RX Enabled */
#define SERPARM_CFLAGS		(CS8 | CREAD)
#define SERPARM_OFLAGS		(0)
#define SERPARM_LFLAGS		(0)

#define UNUSED __attribute__((unused))

/* Serial port fd */
static int serport = -1;
static struct micro_communicator_cached_data ucomm_cached;
static struct micro_communicator_focus_params focus_conf;
static struct micro_communicator_focus_state  focus_state;

/* MicroComm Server */
static int sock;
static int clientsock;
static struct sockaddr_un server_addr;
static pthread_t ucommsvr_thread;
static bool ucthread_run = true;

/* Debugging defines */
// #define DEBUG_FOCUS_STEPTEST
// #define DEBUG_CMDS
// #define DEBUG_FOCUS

/*
 * sendcmd_query - Sends a command to the uC via serial.
 *
 * \return Returns success or negative number for error.
 */
static int sendcmd(int fd, uint8_t cmd[], int cmd_sz,
		const uint8_t reply[], int reply_len)
{
	int i, rc = -2, retry = 0, sz_ans = 32;
	char buf[40];

#ifdef DEBUG_CMDS
	for (i = 0; i < cmd_sz; i++) {
		ALOGE("SEND: %2x", buf[i]);
	}
#endif

	do {
		tcdrain(fd);
		usleep(50);
		write(fd, cmd, cmd_sz);
		usleep(75);

		ioctl(fd, FIONREAD, &sz_ans);
		if (sz_ans <= 0)
			continue;

		if (sz_ans > 40)
			sz_ans = 40;

		rc = read(fd, buf, sz_ans);
		if (buf[0] == 0x0b &&
		    buf[1] == 0x0e) {
			rc = 0;
			/* If VT SO received, check reply */
			for (i = 0; i < reply_len; i++) {
				if (buf[i+3] != reply[i])
					rc = -3;
			}

			/* If no reply to check, force OK */
			if (!reply_len)
				rc = 0;
		}
		else {
			rc = -2;
		}
		if (rc == 0)
			return 0;
		retry++;
	} while (retry < 4);


	if (rc == -2)
		ALOGE("INVALID RX DATA: 0x%x 0x%x",
				buf[0], buf[1]);
	else if (rc == -3)
		ALOGE("INVALID REPLY: 0x%x 0x%x",
				buf[2], buf[3]);

	return rc;
}

/*
 * sendcmd_query - Sends a command and expects back a reply containing
 *		   data to interpret.
 *		   The data is formatted and sent back to the caller
 *		   via the reply pointer as an array of uint8_t.
 *
 * \return Returns reply length or negative number for error.
 */
static int sendcmd_query(int fd, uint8_t cmd[], int cmd_sz,
			uint8_t *reply, int nretries)
{
	int i, rc = -2, retry = 0, sz_ans = 32;
	char buf[40];

	do {
		//tcdrain(fd);
		tcflush(fd, TCIOFLUSH);
		usleep(25);
		write(fd, cmd, cmd_sz);
		tcdrain(fd);
		usleep(8000);

		ioctl(fd, FIONREAD, &sz_ans);
		if (sz_ans <= 0) {
			usleep(12000);

			/* Retry ONE more time */
			ioctl(fd, FIONREAD, &sz_ans);
			if (sz_ans <= 0)
				continue;
		}

		if (sz_ans > 40)
			sz_ans = 40;

		rc = read(fd, buf, sz_ans);

#ifdef DEBUG_CMDS
		for (i = 0; i < sz_ans; i++) {
			ALOGE("Recv: %02x", buf[i]);
		}
#endif

		/* If VT SO received, check reply */
		if (buf[0] == 0x0b &&
		    buf[1] == 0x0e) {
			rc = 0;
			/* Focus set position reply */
			if (buf[2] == 0x04 &&
			    buf[3] == 0x28) {
				reply[0] = buf[4]; /* BYTE1 */
				reply[1] = buf[5]; /* BYTE2 */
				reply[2] = buf[6]; /* CTRL */
				return REPLY_SHORT_FOCUS_LEN;
			}

			/* Focus position query reply */
			if (buf[2] == 0x0c &&
			    buf[3] == 0x28) {
				reply[0] = buf[4]; /* BYTE1 */
				reply[1] = buf[5]; /* BYTE2 */
				reply[2] = buf[12]; /* Sign (+/-) */
				reply[3] = buf[6]; /* NEAR BYTE1 */
				reply[4] = buf[7]; /* NEAR BYTE2 */
				reply[5] = buf[8]; /* FAR BYTE1 */
				reply[6] = buf[9]; /* FAR BYTE1 */
				return REPLY_FOCUS_CUSTOM_LEN;
			}

			/* Focus overflow and underflow errors */
			if (buf[2] == 0x02 &&
			    buf[3] == 0x44 &&
			    buf[4] == 0x46)
				return ERR_UCOMM_FOCUS_OVERFLOW;
			else if (buf[2] == 0x02 &&
				 buf[3] == 0x5a &&
				 buf[4] == 0x5c)
				return ERR_UCOMM_FOCUS_UNDERFLOW;

			/* Unknown and unexpected reply. Can retry. */
			if (buf[2] != 0x04 &&
			    buf[2] != 0x0c)
				rc = -3;
		}
		else {
			rc = -2;
		}
		if (rc == 0)
			return 0;
		retry++;
	} while (retry < nretries);

	if (rc == -2)
		ALOGE("INVALID RX DATA: 0x%x 0x%x",
				buf[0], buf[1]);
	else if (rc == -3)
		ALOGE("UNEXPECTED REPLY: 0x%x 0x%x",
				buf[2], buf[3]);
	return rc;
}

#if 0
static int sendcmd(int fd, uint8_t cmd[], int cmd_sz)
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
#endif

uint8_t *__concat_cmd(const uint8_t head[],
			const uint8_t cmd[],
			int head_len, int cmd_len, int *full_sz)
{
	int footer_len = sizeof(std_footer) / sizeof(std_footer[0]);
	uint8_t *full_cmd = NULL;

	*full_sz = (head_len + cmd_len + footer_len) * sizeof(uint8_t);

	full_cmd = malloc(*full_sz);

	if (full_cmd == NULL) {
		ALOGE("Memory exhausted. Cannot allocate.\n");
		return NULL;
	}

	memcpy(full_cmd, head, head_len);
	memcpy(full_cmd + head_len, cmd, cmd_len);
	memcpy(full_cmd + head_len + cmd_len, std_footer, footer_len);

	return full_cmd;
}

int send_concat_cmd(int fd, const uint8_t head[],
			const uint8_t cmd[],
			int head_len, int cmd_len)
{
	int footer_len = sizeof(std_footer) / sizeof(std_footer[0]);
	int full_sz = 0;
	uint8_t *full_cmd = __concat_cmd(head, cmd,
					head_len, cmd_len, &full_sz);
	if (full_cmd == NULL)
		return -2;

	return sendcmd(fd, full_cmd, full_sz, cmd_reply_nul, 0);
}

int send_init_sequence(int fd)
{
	int rc;
	int head_len = sizeof(std_header) / sizeof(std_header[0]);
	int cmd_len;

	cmd_len = sizeof(cmd_init_hello) / sizeof(cmd_init_hello[0]);
	rc = send_concat_cmd(fd, std_header, cmd_init_hello,
				head_len, cmd_len);
	if (rc)
		goto end;

	cmd_len = sizeof(cmd_init_unk1) / sizeof(cmd_init_unk1[0]);
	rc = send_concat_cmd(fd, std_header, cmd_init_unk1,
				head_len, cmd_len);
	if (rc)
		goto end;

	cmd_len = sizeof(cmd_fan_on) / sizeof(cmd_fan_on[0]);
	rc = send_concat_cmd(fd, std_header, cmd_fan_on,
				head_len, cmd_len);
	if (rc)
		goto end;

	cmd_len = sizeof(cmd_init_unk3) / sizeof(cmd_init_unk3[0]);
	rc = send_concat_cmd(fd, std_header, cmd_init_unk3,
				head_len, cmd_len);
	if (rc)
		goto end;

	cmd_len = sizeof(cmd_init_led) / sizeof(cmd_init_led[0]);
	rc = send_concat_cmd(fd, std_header, cmd_init_led,
				head_len, cmd_len);
end:
	if (rc)
		ALOGE("ERROR: Cannot send init sequence\n");

	return rc;
}

int send_set_brightness(int fd, int brightness)
{
	int rc;
	int head_len = sizeof(std_header) / sizeof(std_header[0]);
	int full_sz;
	int cmd_len;
	uint8_t *full_cmd = NULL;
	uint8_t conv_br = (uint8_t)((((brightness + 1) / 17) * 4) + 40);
	uint8_t control = 45;

	cmd_len = sizeof(cmd_light_lvl) / sizeof(cmd_light_lvl[0]);
	full_cmd = __concat_cmd(std_header, cmd_light_lvl,
				head_len, cmd_len, &full_sz);
	if (full_cmd == NULL) {
		ALOGE("CMDNULL!!!");
		return -2;
	}

	/* Paranoid sanity check:
	 * min 40, max 100 for a total of 60 steps */
	if (conv_br > 100)
		conv_br = 100;
	else if (conv_br < 40)
		conv_br = 40;

	ALOGE("Sending brightness ori %d conv %u", brightness, conv_br);

	full_cmd[head_len + 5] = conv_br;
	full_cmd[head_len + cmd_len - 1] = conv_br + control;

	/* Light setting shall reply XZ */
	rc = sendcmd(fd, full_cmd, full_sz,
			cmd_reply_light_ok, cmd_reply_len);
	if (rc == 0)
		ucomm_cached.light = brightness;

	return rc;
}

int send_power_sequence(int fd, bool poweron)
{
	int rc;
	int head_len = sizeof(std_header) / sizeof(std_header[0]);
	int cmd_len;

	if (poweron) {
		cmd_len = sizeof(cmd_ir_sensor_on) /
				sizeof(cmd_ir_sensor_on[0]);
		rc = send_concat_cmd(fd, std_header, cmd_ir_sensor_on,
					head_len, cmd_len);
		if (rc)
			goto end;

		/* ToDo: When AF implemented, call/restore here */

		rc = send_set_brightness(fd, ucomm_cached.light);
		if (rc)
			goto end;

		ucomm_cached.light_suspended = false;
	} else {
		cmd_len = sizeof(cmd_light_off) /
				sizeof(cmd_light_off[0]);
		rc = send_concat_cmd(fd, std_header, cmd_light_off,
					head_len, cmd_len);
		if (rc)
			goto end;

		ucomm_cached.light_suspended = true;

		cmd_len = sizeof(cmd_ir_sensor_off) /
				sizeof(cmd_ir_sensor_off[0]);
		rc = send_concat_cmd(fd, std_header, cmd_ir_sensor_off,
					head_len, cmd_len);
		if (rc)
			goto end;
	}
end:
	if (rc)
		ALOGE("ERROR: Send power%s sequence FAIL\n",
			poweron ? "on" : "off");

	return rc;
}

int parse_focus_params(int fd, bool stabilized)
{
	int head_len = sizeof(std_header) / sizeof(std_header[0]);
	int full_sz;
	int cmd_len;
	uint8_t *full_cmd = NULL;
	uint8_t *reply = malloc(REPLY_FOCUS_CUSTOM_LEN * sizeof(uint8_t));
	int16_t prev_focus;
	int rc, retry = 0;

	cmd_len = sizeof(cmd_focus_query) / sizeof(cmd_focus_query[0]);
	full_cmd = __concat_cmd(std_header, cmd_focus_query,
					head_len, cmd_len, &full_sz);
	if (full_cmd == NULL)
		return -2;
parse:
	if (retry > 30)
		return -1;

	prev_focus = focus_state.cur_focus;

	rc = sendcmd_query(fd, full_cmd, full_sz, reply, 10);
	if (rc != REPLY_FOCUS_CUSTOM_LEN) {
		if (stabilized) {
			/* The device may be resetting focus... */
			usleep(150000);
			retry++;
			goto parse;
		}
		return rc;
	}

	focus_state.near_max = (reply[3] << 8) | reply[4];
	focus_state.far_max  = -(UINT_MAX - ((reply[5] << 8) | reply[6]) + 1);
	focus_state.cur_focus = (reply[0] << 8) | reply[1];

#ifdef DEBUG_FOCUS
	ALOGE("Current focus: %d", focus_state.cur_focus);
	ALOGE("RANGE: %d to %d", focus_state.far_max, focus_state.near_max);
#endif

	if (!stabilized)
		return 0;

	if (prev_focus != focus_state.cur_focus) {
#ifdef DEBUG_FOCUS
		ALOGE("prev_focus = %d  cur = %d",
			prev_focus, focus_state.cur_focus);
#endif
		/* The lens is moving... let it finish */
		usleep(1000000);

		/* We will never hit max_retry, but let's avoid inf loops.. */
		retry++;

		/* ...And check if the lens has finally got in position */
		goto parse;
	}

	return 0;
}

#define SHIFT24(x)	0xFFFF00 + x

int send_set_focus(int fd, int target_focal)
{
	int head_len = sizeof(std_header) / sizeof(std_header[0]);
	int full_sz;
	int cmd_len;
	int num_steps, tgt, i, rc, reply_type;
	bool is_target_reached, go_near = false;
	uint8_t *full_cmd = NULL;
	uint8_t *reply = malloc(REPLY_FOCUS_CUSTOM_LEN * sizeof(uint8_t));
	uint8_t control;
	static int cur_proc_pass;
	int focus_param[2];

	/* Make sure we initialize the current processing pass at start */
	cur_proc_pass = 0;

	ALOGI("Stepping to focal %d", target_focal);

reprocess:
	/* Retrieve the current lens position */
	rc = parse_focus_params(fd, false);
	if (rc < 0)
		return rc;

	/* Nothing to do? */
	if (target_focal == focus_state.cur_focus) {
		ALOGI("Target step reached.");
		return 0;
	}

	tgt = target_focal;

	cmd_len = sizeof(cmd_focus_setpos) / sizeof(cmd_focus_setpos[0]);
	full_cmd = __concat_cmd(std_header, cmd_focus_setpos,
						head_len, cmd_len, &full_sz);
	if (full_cmd == NULL)
		return -2;

	ALOGE("Target: %d  Cur: %d", tgt, focus_state.cur_focus);

	/* Calculate the number of focuser steps to do */
	if (tgt < focus_state.cur_focus) {
		go_near = false; /* GO FAR */

		if (tgt < focus_state.far_max)
			tgt = focus_state.far_max;

		/* tgt is negative... */
		num_steps = focus_state.cur_focus - tgt;
		if (num_steps > 0)
			num_steps *= -1;
		if (num_steps < -200)
			num_steps = -200;
	} else {
		go_near = true; /* GO NEAR */

		if (tgt > focus_state.near_max)
			tgt = focus_state.near_max;

		/* tgt is positive... */
		num_steps = tgt - focus_state.cur_focus;
		if (num_steps < 0)
			num_steps *= -1;
		if (num_steps > 200)
			num_steps = 200;
	}

	if (go_near) {
		/* NEAR: Number of steps */
		full_cmd[5] = (num_steps & 0xFF00) >> 8;
		full_cmd[6] = num_steps & 0x00FF;

		/* Checksum */
		full_cmd[9] = num_steps + FOCUS_CHECKSUM_BASE;
	} else {
		/* FAR: Number of steps */
		full_cmd[5] = (num_steps & 0xFF00) >> 8;
		full_cmd[6] = num_steps & 0x00FF;

		/* Checksum */
		full_cmd[9] = SHIFT24(full_cmd[5]) + FOCUS_CHECKSUM_BASE;
		full_cmd[9] += SHIFT24(full_cmd[6]) & 0xff;
	}

#ifdef DEBUG_FOCUS
	ALOGE("num_steps = %d", num_steps);
	ALOGE("FOC Prev: %d", focus_state.cur_focus);
#endif

	reply_type = sendcmd_query(fd, full_cmd, full_sz, reply, 0);
	if (reply_type == REPLY_SHORT_FOCUS_LEN ||
	    reply_type == REPLY_FOCUS_CUSTOM_LEN)
		focus_state.cur_focus = (reply[0] << 8) | reply[1];
	else
		ALOGD("Unexpected reply on set focus command.");

	/* Sleep for... */
	if (num_steps < 0)
		num_steps *= -1;

	usleep(1000 * num_steps);

#ifdef DEBUG_FOCUS
	ALOGE("FOC After: %d", focus_state.cur_focus);
#endif

	/* Retrieve the current lens position */
	rc = parse_focus_params(fd, true);
	if (rc < 0)
		return rc;

	/* If anything went wrong, do another pass */
	if (focus_state.cur_focus != tgt &&
	    cur_proc_pass < FOCUS_PROCESSING_MAX_PASS) {
		cur_proc_pass++;
		ALOGD("Uh-Oh! Current step: %d. Target: %d",
			focus_state.cur_focus, target_focal);
		goto reprocess;
	}

	if (focus_state.cur_focus != tgt &&
	    cur_proc_pass == FOCUS_PROCESSING_MAX_PASS) {
		ALOGE("Focus ERROR! Current step: %d. Target: %d",
			focus_state.cur_focus, target_focal);
		reply_type = ERR_UCOMM_FOCUS_GENERAL;
	}

	is_target_reached = (focus_state.cur_focus == tgt);
err:
	if (reply_type == ERR_UCOMM_FOCUS_UNDERFLOW)
		ALOGE("ERROR: FOCUSER UNDERFLOW!");
	else if (reply_type == ERR_UCOMM_FOCUS_OVERFLOW)
		ALOGE("ERROR: FOCUSER OVERFLOW!");
	else if (reply_type == REPLY_FOCUS_CUSTOM_LEN ||
		 reply_type == REPLY_SHORT_FOCUS_LEN ||
		 is_target_reached)
		ALOGI("Focus: Requested step: %d. Current step %d.",
			target_focal, focus_state.cur_focus);
	else
		ALOGE("Error while trying to focus.");
	free(reply);

	return rc;
}

int send_get_focus(int fd)
{
	int rc = parse_focus_params(fd, true);
	if (rc < 0)
		ALOGW("Parse focus params failed");

	return focus_state.cur_focus;
}

int set_reset_focus(int fd)
{
	int rc;
	int head_len = sizeof(std_header) / sizeof(std_header[0]);
	int full_sz;
	int cmd_len;
	uint8_t *full_cmd = NULL;

	cmd_len = sizeof(cmd_focus_reset) / sizeof(cmd_focus_reset[0]);
	full_cmd = __concat_cmd(std_header, cmd_focus_reset,
				head_len, cmd_len, &full_sz);
	if (full_cmd == NULL)
		return -2;

	rc = sendcmd(fd, full_cmd, full_sz,
			cmd_reply_nul, 0);

	return rc;
}


int do_auto_focus(int fd)
{
	int rc, tof_score, focus_step;
	struct micro_communicator_vl53l0 tof_data;

	rc = set_reset_focus(fd);
	if (rc < 0)
		ALOGW("Failed to reset focus!");

	tof_score = ucomm_tof_thr_read_stabilized(&tof_data,
			TOF_STABILIZATION_DEF_RUNS,
			TOF_STABILIZATION_MATCH_NO,
			TOF_STABILIZATION_WAIT_MS,
			TOF_STABILIZATION_HYST_MM);

	ALOGE("Got tof score %d", tof_score);

	/* If the device is not stable, do not proceed */
	if (tof_score < 0)
		return -4;

	/* Focus reset succeeded */
	if (rc == 0)
		usleep(800000); // Allow the reset to finish


	rc = parse_focus_params(fd, true);
	if (rc)
		ALOGW("Focus is not stable!");

	focus_step = (int)polyreg_f(tof_data.range_mm, focus_conf.terms,
					FOCTBL_POLYREG_DEGREE);

	ALOGD("Setting focus %d for %dmm", focus_step, tof_data.range_mm);

	if (focus_step < focus_state.far_max)
		focus_step = focus_state.far_max;
	else if (focus_step > focus_state.near_max)
		focus_step = focus_state.near_max;

	return send_set_focus(fd, focus_step);
}

int send_get_keystone(int fd)
{
	return 0;
}

/*
 * Keystone control: arguments are...
 * 	   01 00 XX YY => \_/
 *	                   _
 *	   01 01 XX YY => / \
 *            /  ||  \
 *        sign  kval  chk
 */
int send_set_keystone(int fd, int ksval)
{
	int rc;
	int head_len = sizeof(std_header) / sizeof(std_header[0]);
	int full_sz;
	int cmd_len;
	uint8_t *full_cmd = NULL;
	uint8_t control;
	uint8_t final_val;

	cmd_len = sizeof(cmd_keystone_val) / sizeof(cmd_keystone_val[0]);
	full_cmd = __concat_cmd(std_header, cmd_keystone_val,
				head_len, cmd_len, &full_sz);
	if (full_cmd == NULL)
		return -2;

	/*
	 * Ranges: -X to -1, 1 to X
	 * 	0 treated as -1.
	 */
	if (ksval > 0) {
		full_cmd[head_len + cmd_len - 3] = 0;
		control = 81;
		final_val = ksval;
	} else {
		full_cmd[head_len + cmd_len - 3] = 1;
		control = 82;

		/* ...we need an unsigned value... */
		final_val = (uint8_t)(ksval * (-1));
	}

	full_cmd[head_len + cmd_len - 2] = final_val;
	full_cmd[head_len + cmd_len - 1] = final_val + control;

	/* Keystone shall reply XZ */
	rc = sendcmd(fd, full_cmd, full_sz,
			cmd_reply_light_ok, cmd_reply_len);
	if (rc == 0)
		ucomm_cached.keystone = ksval;

	return rc;
}

/*
 * ucomm_dispatch - Recognizes the requested operation and calls
 *		    the appropriate function to dispatch the
 *		    command(s) to the uC.
 *
 * \return Returns success(0) or negative errno.
 */
static int32_t ucomm_dispatch(struct micro_communicator_params *params)
{
	int32_t rc;
	int val = params->value;

	switch (params->operation) {
	case OP_INITIALIZE:
		rc = send_init_sequence(serport);
		break;
	case OP_POWER:
		rc = send_power_sequence(serport, val ? true : false);
		break;
	case OP_BRIGHTNESS:
		/*
		 * If requested to set light to 0, run power off sequence.
		 * If the last time we've run the power off sequence, then
		 * we have to run the power on sequence.
		 */
		if (ucomm_cached.light_suspended || val == 0)
			rc = send_power_sequence(serport, val ? true : false);
		else
			rc = send_set_brightness(serport, val);
		break;
	case OP_FOCUS_SET:
		rc = send_set_focus(serport, val);
		break;
	case OP_KEYSTONE_SET:
		rc = send_set_keystone(serport, val);
		break;
	case OP_FOCUS_GET:
		rc = send_get_focus(serport);
		break;
	case OP_KEYSTONE_GET:
		rc = send_get_keystone(serport);
		break;
	case OP_AUTOFOCUS:
		rc = do_auto_focus(serport);
		break;
	case OP_CONT_AF_SET:
	default:
		ALOGE("Invalid operation requested.");
		rc = -2;
	}

	return rc;
}

static void *ucommsvr_looper(void *unusedvar UNUSED)
{
	int ret;
	int32_t microcomm_reply = -EINVAL;
	uint8_t retry;
	socklen_t clientlen = sizeof(struct sockaddr_un);
	struct sockaddr_un client_addr;
	struct micro_communicator_params extparams;

reloop:
	ALOGI("MicroComm Server is waiting for connection...");
	if (clientsock)
		close(clientsock);
	retry = 0;
	while (((clientsock = accept(sock, (struct sockaddr*)&client_addr,
		&clientlen)) > 0) && (ucthread_run == true))
	{
		ret = recv(clientsock, &extparams,
			sizeof(struct micro_communicator_params), 0);
		if (!ret) {
			ALOGE("Cannot receive data from client");
			goto reloop;
		}

		if (ret != sizeof(struct micro_communicator_params)) {
			ALOGE("Received data size mismatch!!");
			goto reloop;
		} else ret = 0;

		microcomm_reply = ucomm_dispatch(&extparams);

retry_send:
		retry++;
		ret = send(clientsock, &microcomm_reply,
			sizeof(microcomm_reply), 0);
		if (ret == -1) {
			microcomm_reply = -EINVAL;
			if (retry < 50)
				goto retry_send;
			ALOGE("ERROR: Cannot send reply!!!");
			goto reloop;
		} else retry = 0;

		if (clientsock)
			close(clientsock);
	}

	ALOGI("MicroComm Server terminated.");
	pthread_exit((void*)((int)0));
}

static int manage_ucommsvr(bool start)
{
	int ret;
	struct stat st = {0};

	if (start == false) {
		ucthread_run = false;
		if (clientsock) {
			shutdown(clientsock, SHUT_RDWR);
			close(clientsock);
		}
		if (sock) {
			shutdown(sock, SHUT_RDWR);
			close(sock);
		}

		return 0;
	}

	ucthread_run = true;

	/* Create folder, if doesn't exist */
	if (stat(UCOMMSERVER_DIR, &st) == -1) {
		mkdir(UCOMMSERVER_DIR, 0773);
	}

	/* Get socket in the UNIX domain */
	sock = socket(PF_UNIX, SOCK_SEQPACKET, 0);
	if (sock < 0) {
		ALOGE("Could not create the socket");
		return -EPROTO;
	}

	/* Create address */
	memset(&server_addr, 0, sizeof(struct sockaddr_un));
	server_addr.sun_family = AF_UNIX;
	strcpy(server_addr.sun_path, UCOMMSERVER_SOCKET);

	/* Free the existing socket file, if any */
	unlink(UCOMMSERVER_SOCKET);

	/* Bind the address to the socket */
	ret = bind(sock, (struct sockaddr*)&server_addr,
			sizeof(struct sockaddr_un));
	if (ret != 0) {
		ALOGE("Cannot bind socket");
		return -EINVAL;
	}

	/* Set socket permissions */
	chown(server_addr.sun_path, AID_ROOT, AID_SYSTEM);
	chmod(server_addr.sun_path, 0666);

	/* Listen on this socket */
	ret = listen(sock, UCOMMSERVER_MAXCONN);
	if (ret != 0) {
		ALOGE("Cannot listen on socket");
		return ret;
	}

	ret = pthread_create(&ucommsvr_thread, NULL, ucommsvr_looper, NULL);
	if (ret != 0) {
		ALOGE("Cannot create MicroComm Server thread");
		return -ENXIO;
	}

	return 0;
}

/*
 * ucomm_autofocus_get_coeff - Prepares the focus algorithm in advance
 *			       by getting the polynomial regression's
 *			       correlation coefficient for the provided
 *			       input-focus table.
 *
 * \return Returns zero or negative errno.
 */
static int ucomm_autofocus_get_coeff(void)
{
	int i;
	struct pair_data *pairs;
	double coeff;
	int rs = 3 * FOCTBL_POLYREG_DEGREE;

	if (focus_conf.table == NULL)
		return -3;

	pairs = (struct pair_data*)calloc(focus_conf.num_steps+1,
				 sizeof(struct pair_data));
	if (pairs == NULL) {
		ALOGE("Memory exhausted. Cannot write focus table");
		return -4;
	}

	for (i = 0; i <= focus_conf.num_steps; i++) {
		pairs[i].x = focus_conf.table[i].input_val;
		pairs[i].y = focus_conf.table[i].focus_step;
		ALOGD("Table x:%.2f  y:%.2f",
			pairs[i].x, pairs[i].y);
	}

	ALOGE("Got %d pairs", focus_conf.num_steps);

	focus_conf.terms = (double*)calloc(rs, sizeof(double));

	compute_coefficients(pairs, focus_conf.num_steps,
				FOCTBL_POLYREG_DEGREE, focus_conf.terms);
	if (focus_conf.terms == NULL) {
		ALOGE("FATAL: Cannot compute coefficients.");
		return -5;
	}

	for (i = 0; i < focus_conf.num_steps; i++)
		ALOGE("Term%d: %.10f",i, focus_conf.terms[i]);

	coeff = corr_coeff(pairs, focus_conf.num_steps, focus_conf.terms);
	if (coeff > 1.0f)
		ALOGW("WARNING! The correlation coefficient is >1!!");
	else if (coeff == 0.0f)
		ALOGW("WARNING! The correlation coefficient is ZERO!!");

	ALOGD("Correlation coefficient: %.10f", coeff);

	ALOGI("Auto-Focus Polynomial Regression coordinates loaded.");

	return 0;
}

int main(void)
{
	struct termios tty;
	int rc;

	ALOGI("Initializing MicroComm Server...");

	serport = open(MICRO_UART, (O_RDWR | O_NOCTTY | O_NONBLOCK));

	if (serport < 0) {
		ALOGE("Error: cannot open the serial port.");
		return -1;
	}

	memset(&tty, 0, sizeof tty);

	/* Get current port attributes */
	if (tcgetattr(serport, &tty) != 0) {
		ALOGE("Error: cannot get port attributes\n");
		rc = -1;
		goto err;
	}

	/* Set flags */
	tty.c_cflag |= SERPARM_CFLAGS;
	tty.c_iflag |= SERPARM_IFLAGS;
	//tty.c_oflag = 0;
	//tty.c_lflag = 0;

	/* Set communication speed */
        cfsetospeed(&tty, SERPARM_BAUDRATE);
        cfsetispeed(&tty, SERPARM_BAUDRATE);

	/* Flush the comms */
	tcflush(serport, TCIFLUSH);

	/* Send configuration to kernel */
	if (tcsetattr(serport, TCSADRAIN, &tty) != 0) {
		ALOGE("Error: cannot set port attributes\n");
		rc = -1;
		goto err;
	}

	/* Fill in cached data with safe values */
	ucomm_cached.light_suspended = false;
	ucomm_cached.light = 100;
	ucomm_cached.keystone = 86;
	ucomm_cached.focus = 129;

	rc = parse_ucomm_xml_data(UCOMMSERVER_CONF_FILE, "tof_focus",
				&focus_conf);
	if (rc < 0) {
		ALOGE("Cannot parse configuration for ToF assisted AF");
	} else {
		rc = ucomm_input_tof_init();
		if (rc < 0)
			ALOGW("Cannot open ToF. Ranging will be unavailable");
		ucomm_autofocus_get_coeff();
	}
	
start:
	/* All devices opened and configured. Start! */
	rc = manage_ucommsvr(true);
	if (rc == 0) {
		ALOGI("MicroComm Server started");
	} else {
		ALOGE("Could not start MicroComm Server");
		goto err;
	}

	pthread_join(ucommsvr_thread, (int**)&rc);
	if (rc == 0)
		goto start;

	return rc;
err:
	close(serport);
	ALOGE("MicroComm Server initialization FAILED.");

	return rc;
}
