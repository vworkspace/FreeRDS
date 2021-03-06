/**
 * FreeRDS: FreeRDP Remote Desktop Services (RDS)
 *
 * Copyright 2013-2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <winpr/crt.h>
#include <winpr/rpc.h>
#include <winpr/file.h>
#include <winpr/path.h>
#include <winpr/winsock.h>
#include <winpr/platform.h>
#include <winpr/wlog.h>

#include "channels.h"

//#define WITH_FREERDS_CHANNELS	1

#include "rpc.h"

#define TAG "freerds.server.channels"

BOOL freerds_channels_is_channel_allowed(UINT32 SessionId, char* ChannelName)
{
	FDSAPI_CHANNEL_ALLOWED_REQUEST request;
	FDSAPI_CHANNEL_ALLOWED_RESPONSE response;

	ZeroMemory(&request, sizeof(request));
	ZeroMemory(&response, sizeof(response));

	request.ChannelName = ChannelName;

	freerds_icp_IsChannelAllowed(&request, &response);

	return response.ChannelAllowed;
}

int freerds_channels_post_connect(rdsConnection* session)
{
	BOOL allowed;

	if (WTSVirtualChannelManagerIsChannelJoined(session->vcm, "cliprdr"))
	{
		allowed = freerds_channels_is_channel_allowed(session->id, "cliprdr");
		WLog_INFO(TAG, "channel %s is %s", "cliprdr", allowed ? "allowed" : "not allowed");

#ifdef WITH_FREERDS_CHANNELS
		WLog_INFO(TAG, "Channel %s registered", "cliprdr");
		session->cliprdr = cliprdr_server_context_new(session->vcm);
		session->cliprdr->Start(session->cliprdr);
#endif
	}

	if (WTSVirtualChannelManagerIsChannelJoined(session->vcm, "rdpdr"))
	{
		allowed = freerds_channels_is_channel_allowed(session->id, "rdpdr");
		WLog_INFO(TAG, "channel %s is %s", "rdpdr", allowed ? "allowed" : "not allowed");

#ifdef WITH_FREERDS_CHANNELS
		WLog_INFO(TAG, "Channel %s registered", "rdpdr");
		session->rdpdr = rdpdr_server_context_new(session->vcm);
		session->rdpdr->Start(session->rdpdr);
#endif
	}

	if (WTSVirtualChannelManagerIsChannelJoined(session->vcm, "rdpsnd"))
	{
		allowed = freerds_channels_is_channel_allowed(session->id, "rdpsnd");
		WLog_INFO(TAG, "channel %s is %s", "rdpsnd", allowed ? "allowed" : "not allowed");

#ifdef WITH_FREERDS_CHANNELS
		WLog_INFO(TAG, "Channel %s registered", "rdpsnd");
		session->rdpsnd = rdpsnd_server_context_new(session->vcm);
		session->rdpsnd->Start(session->rdpsnd);
#endif
	}

	return 0;
}

/* channel server */

int freerds_detect_ephemeral_port_range(int* begPort, int* endPort)
{
	/* IANA port range */
	*begPort = 49152;
	*endPort = 65535;

#ifdef __linux__
	/* default linux port range */
	*begPort = 32768;
	*endPort = 61000;
#endif

	return 1;
}

int freerds_bind_local_ephemeral_port(SOCKET* pSocket)
{
	SOCKET s;
	int port;
	int status;
	int begPort;
	int endPort;
	int optlen = 0;
	UINT32 optval = 0;
	unsigned long addr;
	struct sockaddr_in sockAddr;

	*pSocket = 0;

	freerds_detect_ephemeral_port_range(&begPort, &endPort);

	s = _socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	if (s == INVALID_SOCKET)
		return -1;

	*pSocket = s;

	_setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char*) &optval, sizeof(optlen));

	status = -1;
	addr = _inet_addr("127.0.0.1");

	for (port = begPort; port <= endPort; port++)
	{
		sockAddr.sin_family = AF_INET;
		sockAddr.sin_addr.s_addr = addr;
		sockAddr.sin_port = htons(port);

		status = _bind(s, (struct sockaddr*) &sockAddr, sizeof(sockAddr));

		if (status == 0)
			break;
	}

	if (status != 0)
	{
		closesocket(s);
		*pSocket = 0;
		return -1;
	}

	return port;
}

int freerds_channel_server_open(rdsChannelServer* channels)
{
	int status = 0;

	channels->listenPort = freerds_bind_local_ephemeral_port(&(channels->listenerSocket));

	status = _listen(channels->listenerSocket, SOMAXCONN);

	if (status == SOCKET_ERROR)
	{
		WLog_ERR(TAG, "listen failure: %d", WSAGetLastError());
		closesocket(channels->listenerSocket);
		channels->listenerSocket = 0;
		return -1;
	}

	channels->listenEvent = CreateFileDescriptorEvent(NULL, FALSE, FALSE, (int) channels->listenerSocket);

	WLog_INFO(TAG, "Listening on %s:%d for channels...", channels->listenAddress, channels->listenPort);

	return 1;
}

int freerds_channel_server_close(rdsChannelServer* channels)
{
	if (channels->listenerSocket)
	{
		closesocket(channels->listenerSocket);
		channels->listenerSocket = 0;
	}

	if (channels->listenEvent)
	{
		CloseHandle(channels->listenEvent);
		channels->listenEvent = NULL;
	}

	return 1;
}

HANDLE freerds_channel_server_listen_event(rdsChannelServer* channels)
{
	return channels->listenEvent;
}

int freerds_channel_server_accept(rdsChannelServer* channels)
{
	int status;
	int optlen = 0;
	UINT32 optval = 0;
	SOCKET socket;
	char guidString[36 + 1];
	rdsChannel* channel = NULL;

	socket = accept(channels->listenerSocket, NULL, NULL);

	if (socket == INVALID_SOCKET)
	{
		WLog_ERR(TAG, "accept failed with error: %d", WSAGetLastError());
		return -1;
	}

	/* receive channel connection GUID */

	guidString[36] = '\0';
	status = _recv(socket, guidString, 36, 0);

	if (status != 36)
	{
		closesocket(socket);
		return -1;
	}

	channel = (rdsChannel*) HashTable_GetItemValue(channels->table, guidString);

	if (channel && channel->connected)
		channel = NULL;

	if (!channel)
	{
		closesocket(socket);
		return -1;
	}

	optval = TRUE;
	optlen = sizeof(optval);

	_setsockopt(socket, IPPROTO_TCP, TCP_NODELAY, (char*) &optval, optlen);

	channel->socket = socket;
	channel->event = CreateFileDescriptorEvent(NULL, FALSE, FALSE, (int) channel->socket);

	channel->connected = TRUE;
	SetEvent(channel->readyEvent);

	return 1;
}

int freerds_channel_server_add(rdsChannelServer* channels, rdsChannel* channel)
{
	HashTable_Add(channels->table, channel->guidString, channel);
	return 1;
}

int freerds_channel_server_remove(rdsChannelServer* channels, rdsChannel* channel)
{
	HashTable_Remove(channels->table, channel->guidString);
	return 1;
}

rdsChannelServer* freerds_channel_server_new()
{
	rdsChannelServer* channels;

	channels = (rdsChannelServer*) calloc(1, sizeof(rdsChannelServer));

	if (channels)
	{
		channels->listenPort = 0;
		channels->listenAddress = _strdup("127.0.0.1");

		channels->table = HashTable_New(TRUE);

		if (!channels->table)
		{
			free(channels);
			return NULL;
		}

		channels->table->hash = HashTable_StringHash;
		channels->table->keyCompare = HashTable_StringCompare;
		channels->table->keyClone = HashTable_StringClone;
		channels->table->keyFree = HashTable_StringFree;
	}

	return channels;
}

void freerds_channel_server_free(rdsChannelServer* channels)
{
	if (!channels)
		return;

	free(channels->listenAddress);

	if (channels->table)
	{
		HashTable_Free(channels->table);
		channels->table = NULL;
	}

	free(channels);
}

/* channel */

int freerds_client_add_channel(rdsConnection* connection, rdsChannel* channel)
{
	ArrayList_Add(connection->channels, channel);
	return 1;
}

int freerds_client_remove_channel(rdsConnection* connection, rdsChannel* channel)
{
	ArrayList_Remove(connection->channels, channel);
	return 1;
}

int freerdp_client_virtual_channel_read(freerdp_peer* client, HANDLE hChannel, BYTE* buffer, UINT32 length)
{
	int status = 1;
	int offset = 0;
	rdsChannel* channel;

	if (!hChannel)
		return -1;

	channel = (rdsChannel*) client->VirtualChannelGetData(client, hChannel);

	if (!channel)
		return -1;

	while (offset < length)
	{
		status = _send(channel->socket, (char*) &buffer[offset], length - offset, 0);

		if (status < 1)
			break;

		offset += status;
	}

	if (offset == length)
		status = offset;

	return status;
}

int freerds_channel_check_socket(rdsConnection* connection, rdsChannel* channel)
{
	int status;
	UINT32 length;
	BYTE* buffer;
	freerdp_peer* client;

	length = sizeof(buffer);
	client = connection->client;

	status = _recv(channel->socket, (char*) &length, sizeof(length), 0);

	if (status != sizeof(length))
	{
		WLog_ERR(TAG, "socket recv failed with status %d (lasterror=%d)", status, WSAGetLastError());
		return -1;
	}

	buffer = (BYTE*) malloc(length);

	if (!buffer)
	{
		WLog_ERR(TAG, "memory allocation error");
		return -1;
	}

	status = _recv(channel->socket, (char*) buffer, length, 0);

	if (status != length)
	{
		WLog_ERR(TAG, "recv failed with status %d (lasterror=%d)", status, WSAGetLastError());
		free(buffer);
		return -1;
	}

	client->VirtualChannelWrite(client, channel->rdpChannel, buffer, status);

	free(buffer);

	return status;
}

int freerds_client_get_channel_event_handles(rdsConnection* connection, HANDLE* events, DWORD* nCount)
{
	int index;
	int count;
	ULONG_PTR* items;
	rdsChannel* channel;

	count = ArrayList_Items(connection->channels, &items);

	for (index = 0; index < count; index++)
	{
		channel = (rdsChannel*) items[index];
		events[*nCount] = channel->event;
		(*nCount)++;
	}

	return 1;
}

int freerds_client_check_channel_event_handles(rdsConnection* connection)
{
	int index = 0;
	int count = 0;
	int status = 1;
	ULONG_PTR* items = NULL;
	rdsChannel* channel = NULL;

	count = ArrayList_Items(connection->channels, &items);

	for (index = 0; index < count; index++)
	{
		channel = (rdsChannel*) items[index];

		if (WaitForSingleObject(channel->event, 0) == WAIT_OBJECT_0)
		{
			status = freerds_channel_check_socket(connection, channel);

			if (status < 1)
				break;
		}
	}

	if (status < 1)
	{
		freerds_client_remove_channel(connection, channel);
		freerds_channel_free(channel);
	}

	return 1;
}

rdsChannel* freerds_channel_new(rdsConnection* connection, const char* name)
{
	rdsChannel* channel;

	channel = (rdsChannel*) calloc(1, sizeof(rdsChannel));

	if (channel)
	{
		RPC_CSTR rpcString = NULL;

		channel->connection = connection;
		channel->server = connection->server;
		channel->channels = connection->channelServer;

		channel->name = _strdup(name);

		channel->connected = FALSE;
		channel->port = channel->channels->listenPort;

		UuidCreateSequential(&(channel->guid));

		UuidToStringA(&(channel->guid), &rpcString);
		channel->guidString = _strdup(rpcString);
		RpcStringFreeA(&rpcString);

		channel->readyEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	}

	return channel;
}

void freerds_channel_free(rdsChannel* channel)
{
	if (!channel)
		return;

	free(channel->name);
	free(channel->guidString);

	CloseHandle(channel->event);
	CloseHandle(channel->readyEvent);

	free(channel);
}
