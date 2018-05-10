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

#define LOG_TAG "MicroCommCTL"

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <private/android_filesystem_config.h>
#include <hardware/hardware.h>
#include <hardware/power.h>
#include <utils/Log.h>

#include "ucomm_private.h"

static int send_ucommsvr_data(struct micro_communicator_params params)
{
	register int sock, recvsock;
	int ret, len = sizeof(struct sockaddr_un);
	int32_t ucommsvr_reply;
	fd_set receivefd;
	struct sockaddr_un server_address;
	struct timeval timeout;

	/* Get socket in the UNIX domain */
	sock = socket(PF_UNIX, SOCK_SEQPACKET, 0);
	if (sock < 0) {
		ALOGE("Could not get the MicroComm Server from client");
		return -EPROTO;
	}

	memset(&server_address, 0, sizeof(struct sockaddr_un));
	server_address.sun_family = AF_UNIX;
	strcpy(server_address.sun_path, UCOMMSERVER_SOCKET);

	/* Set nonblocking I/O for socket to avoid stall */
	fcntl(sock, F_SETFL, O_NONBLOCK);

	ret = connect(sock, (struct sockaddr*)&server_address, len);
	if (ret < 0) {
		ALOGE("Cannot connect to MicroComm Server socket");
		goto end;
	}

	/* Send the filled struct */
	ret = send(sock, &params, sizeof(struct micro_communicator_params), 0);
	if (ret < 0) {
		ALOGE("Cannot send data to MicroComm Server");
		goto end;
	}

	/* Setup for receiving server reply (handle) */
	/* Initialize and set a new FD for receive operation */
	FD_ZERO(&receivefd);
	FD_SET(sock, &receivefd);

	/*
	 * Set a six seconds timeout for select operation 
	 * because serial communication may be slow sometimes
	 */
	timeout.tv_sec = 6;
	timeout.tv_usec = 0;

	/* Wait until the socket is ready for receive operation */
	ret = select(sock+1, &receivefd, NULL, NULL, &timeout);
	if (ret < 0) {
		ALOGE("Socket error. Cannot continue.");
		if (ret == -1) {
			ALOGE("Socket not ready: timed out");
			ret = -ETIMEDOUT;
		}
		goto end;
	}

	/* New FD is set and the socket is ready to receive data */
	ret = recv(sock, &ucommsvr_reply, sizeof(int32_t), 0);
	if (ret == -1) {
		ALOGE("Cannot receive reply from MicroComm Server");
		ret = ucommsvr_reply = -EINVAL;
	} else ret = ucommsvr_reply;
end:
	if (sock)
		close(sock);
	return ret;
}

static int ucommsvr_send_set(int operation, int value)
{
	struct micro_communicator_params params;

	params.operation = operation;
	params.value = (int32_t)value;

	return send_ucommsvr_data(params);
}

int ucommsvr_set_backlight(int brightness)
{
	return ucommsvr_send_set(OP_BRIGHTNESS, brightness);
}

int ucommsvr_set_keystone(int ksval)
{
	return ucommsvr_send_set(OP_KEYSTONE_SET, ksval);
}

int ucommsvr_set_focus(int focus)
{
	return ucommsvr_send_set(OP_FOCUS_SET, focus);
}

int ucommsvr_do_autofocus(void)
{
	return ucommsvr_send_set(OP_AUTOFOCUS, 0);
}

int ucommsvr_get_keystone(void)
{
	return ucommsvr_send_set(OP_KEYSTONE_GET, 0);
}

int ucommsvr_get_focus(void)
{
	return ucommsvr_send_set(OP_FOCUS_GET, 0);
}


