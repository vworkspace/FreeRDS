/**
 * pbRPC - a simple, protocol buffer based RCP mechanism
 *
 * Copyright 2013 Thincast Technologies GmbH
 * Copyright 2013 Bernhard Miklautz <bernhard.miklautz@thincast.com>
 * Copyright 2013 Hardening <contact@hardening-consulting.com>
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

#include <winpr/crt.h>
#include <winpr/pipe.h>
#include <winpr/thread.h>
#include <winpr/interlocked.h>
#include <winpr/wlog.h>

#include "core.h"

#include "rpc.h"

#define TAG "freerds.server.rpc"

extern rdsServer* g_Server;

int tp_npipe_open(pbRPCContext* context, int timeout)
{
	HANDLE hNamedPipe = 0;
	char pipeName[] = "\\\\.\\pipe\\FreeRDS_Manager";

	if (!WaitNamedPipeA(pipeName, timeout))
	{
		return -1;
	}

	hNamedPipe = CreateFileA(pipeName,
			GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);

	if ((!hNamedPipe) || (hNamedPipe == INVALID_HANDLE_VALUE))
	{
		return -1;
	}

	context->hPipe = hNamedPipe;

	return 0;
}

int tp_npipe_close(pbRPCContext* context)
{
	if (context->hPipe)
	{
		CloseHandle(context->hPipe);
		context->hPipe = 0;
	}

	return 0;
}

int tp_npipe_write(pbRPCContext* context, BYTE* data, UINT32 size)
{
	DWORD bytesWritten;
	BOOL fSuccess = FALSE;

	fSuccess = WriteFile(context->hPipe, data, size, (LPDWORD) &bytesWritten, NULL);

	if (!fSuccess || (bytesWritten < size))
	{
		return -1;
	}

	return bytesWritten;
}

int tp_npipe_read(pbRPCContext* context, BYTE* data, UINT32 size)
{
	DWORD bytesRead;
	BOOL fSuccess = FALSE;

	fSuccess = ReadFile(context->hPipe, data, size, &bytesRead, NULL);

	if (!fSuccess || (bytesRead < size))
	{
		return -1;
	}

	return bytesRead;
}

struct pbrpc_transaction
{
	void* callbackArg;
	BOOL freeAfterResponse;
	pbRpcResponseCallback responseCallback;
};
typedef struct pbrpc_transaction pbRPCTransaction;

DWORD pbrpc_getTag(pbRPCContext *context)
{
	return InterlockedIncrement(&(context->tag));
}

FDSAPI_MSG_PACKET* pbrpc_message_new()
{
	FDSAPI_MSG_PACKET* msg = calloc(1, sizeof(FDSAPI_MSG_PACKET));

	if (!msg)
		return msg;

	return msg;
}

void pbrpc_message_free(FDSAPI_MSG_PACKET* msg, BOOL freePayload)
{
	if (freePayload)
		free(msg->buffer);

	free(msg);
}

pbRPCPayload* pbrpc_payload_new()
{
	pbRPCPayload* pl = calloc(1, sizeof(pbRPCPayload));
	return pl;
}

void pbrpc_free_payload(pbRPCPayload* response)
{
	if (!response)
		return;

	free(response->buffer);
	free(response);
}

static void queue_item_free(void* obj)
{
	pbrpc_message_free((FDSAPI_MSG_PACKET*) obj, FALSE);
}

static void list_dictionary_item_free(void* item)
{
	wListDictionaryItem* di = (wListDictionaryItem*) item;
	free((pbRPCTransaction*)(di->value));
}

pbRPCContext* pbrpc_server_new()
{
	pbRPCContext* context = calloc(1, sizeof(pbRPCContext));

	context->stopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	context->transactions = ListDictionary_New(TRUE);
	ListDictionary_ValueObject(context->transactions)->fnObjectFree = list_dictionary_item_free;
	context->writeQueue = Queue_New(TRUE, -1, -1);
	context->writeQueue->object.fnObjectFree = queue_item_free;

	return context;
}

void pbrpc_server_free(pbRPCContext* context)
{
	if (!context)
		return;

	CloseHandle(context->stopEvent);
	CloseHandle(context->thread);
	ListDictionary_Free(context->transactions);
	Queue_Free(context->writeQueue);

	free(context);
}

int pbrpc_receive_message(rdsServer* server, FDSAPI_MSG_HEADER* header, BYTE** buffer)
{
	int status = 0;
	pbRPCContext* context = server->rpc;

	status = tp_npipe_read(context, (BYTE*) header, FDSAPI_MSG_HEADER_SIZE);

	if (status < 0)
		return status;

	*buffer = malloc(header->msgSize);

	if (!(*buffer))
		return -1;

	status = tp_npipe_read(context, *buffer, header->msgSize);

	if (status < 0)
	{
		free(*buffer);
		return status;
	}

	return status;
}

int pbrpc_send_message(rdsServer* server, FDSAPI_MSG_HEADER* header, BYTE* buffer)
{
	int status;
	pbRPCContext* context = server->rpc;

	status = tp_npipe_write(context, (BYTE*) header, FDSAPI_MSG_HEADER_SIZE);

	if (status < 0)
		return status;

	status = tp_npipe_write(context, buffer, header->msgSize);

	if (status < 0)
		return status;

	return 0;
}

static int pbrpc_process_response(rdsServer* server, FDSAPI_MSG_PACKET* msg)
{
	pbRPCContext* context = server->rpc;
	pbRPCTransaction* ta = ListDictionary_GetItemValue(context->transactions, (void*)((UINT_PTR)msg->callId));

	if (!ta)
	{
		WLog_ERR(TAG, "unsoliciated response - ignoring (tag %d)", msg->callId);
		return 1;
	}

	ListDictionary_Remove(context->transactions, (void*)((UINT_PTR)msg->callId));

	if (ta->responseCallback)
		ta->responseCallback(msg->status, msg, ta->callbackArg);

	if (ta->freeAfterResponse)
		free(ta);

	return 0;
}

static int pbrpc_process_message_out(rdsServer* server, FDSAPI_MSG_PACKET* msg)
{
	int status;
	FDSAPI_MSG_HEADER header;

	header.msgType = msg->msgType;
	header.msgSize = msg->length;
	header.callId = msg->callId;
	header.status = msg->status;

	status = pbrpc_send_message(server, &header, msg->buffer);

	return status;
}

static pbRPCPayload* pbrpc_fill_payload(FDSAPI_MSG_PACKET* msg)
{
	pbRPCPayload* pl = (pbRPCPayload*) calloc(1, sizeof(pbRPCPayload));

	pl->buffer = msg->buffer;
	pl->length = msg->length;

	return pl;
}

int pbrpc_send_response(rdsServer* server, pbRPCPayload* response, UINT32 status, UINT32 type, UINT32 tag)
{
	int ret;
	FDSAPI_MSG_PACKET* msg;

	msg = pbrpc_message_new();

	msg->callId = tag;
	msg->msgType = FDSAPI_RESPONSE_ID(type);
	msg->status = status;

	if (response)
	{
		if (status == 0)
		{
			msg->buffer = response->buffer;
			msg->length = response->length;
		}
	}

	ret = pbrpc_process_message_out(server, msg);

	if (response)
	{
		free(response->buffer);
		free(response);
	}

	pbrpc_message_free(msg, FALSE);

	return ret;
}

static int pbrpc_process_request(rdsServer* server, FDSAPI_MSG_PACKET* msg)
{
	int status = 0;
	UINT32 msgType;
	pbRPCCallback cb;
	pbRPCPayload* response = NULL;

	msgType = msg->msgType;

	switch (msgType)
	{
		case FDSAPI_HEARTBEAT_REQUEST_ID:
			cb = freerds_icp_Heartbeat;
			break;

		case FDSAPI_SWITCH_SERVICE_ENDPOINT_REQUEST_ID:
			cb = freerds_icp_SwitchServiceEndpoint;
			break;

		case FDSAPI_LOGOFF_USER_REQUEST_ID:
			cb = freerds_icp_LogoffUser;
			break;

		case FDSAPI_CHANNEL_ENDPOINT_OPEN_REQUEST_ID:
			cb = freerds_icp_ChannelEndpointOpen;
			break;
	}

	if (!cb)
	{
		WLog_ERR(TAG, "server callback not found %d", msg->msgType);
		msg->status = FDSAPI_STATUS_NOTFOUND;
		status = pbrpc_send_response(server, NULL, msg->status, msg->msgType, msg->callId);
		return status;
	}

	status = cb(msg, &response);

	if (!response)
		return 0;

	status = pbrpc_send_response(server, response, status, msg->msgType, msg->callId);

	return status;
}

int pbrpc_process_message_in(rdsServer* server)
{
	BYTE* buffer;
	int status = 0;
	FDSAPI_MSG_HEADER header;
	FDSAPI_MSG_PACKET* msg;

	if (pbrpc_receive_message(server, &header, &buffer) < 0)
		return -1;

	msg = (FDSAPI_MSG_PACKET*) calloc(1, sizeof(FDSAPI_MSG_PACKET));

	CopyMemory(msg, &header, sizeof(FDSAPI_MSG_HEADER));

	msg->buffer = buffer;
	msg->length = header.msgSize;

	if (FDSAPI_IS_RESPONSE_ID(msg->msgType))
		status = pbrpc_process_response(server, msg);
	else
		status = pbrpc_process_request(server, msg);

	return status;
}

static int pbrpc_transport_open(pbRPCContext* context)
{
	UINT32 sleepInterval = 200;

	while (1)
	{
		if (tp_npipe_open(context, sleepInterval) == 0)
		{
			return 0;
		}

		if (WaitForSingleObject(context->stopEvent, 0) == WAIT_OBJECT_0)
		{
			return -1;
		}

		Sleep(sleepInterval);
	}

	return 0;
}

static void pbrpc_connect(pbRPCContext* context)
{
	if (0 != pbrpc_transport_open(context))
		return;

	context->isConnected = TRUE;
}

static void pbrpc_reconnect(pbRPCContext* context)
{
	pbRPCTransaction* ta = NULL;
	context->isConnected = FALSE;

	tp_npipe_close(context);
	Queue_Clear(context->writeQueue);

	while ((ta = ListDictionary_Remove_Head(context->transactions)))
	{
		ta->responseCallback(PBRCP_TRANSPORT_ERROR, 0, ta->callbackArg);

		if (ta->freeAfterResponse)
			free(ta);
	}

	pbrpc_connect(context);
}

static void pbrpc_main_loop(pbRPCContext* context)
{
	int status;
	DWORD nCount;
	HANDLE events[32];
	rdsServer* server = g_Server;

	pbrpc_connect(context);

	while (context->isConnected)
	{
		nCount = 0;
		events[nCount++] = context->stopEvent;
		events[nCount++] = Queue_Event(context->writeQueue);
		events[nCount++] = context->hPipe;

		status = WaitForMultipleObjects(nCount, events, FALSE, INFINITE);

		if (status == WAIT_FAILED)
		{
			break;
		}

		if (WaitForSingleObject(context->stopEvent, 0) == WAIT_OBJECT_0)
		{
			break;
		}

		if (WaitForSingleObject(context->hPipe, 0) == WAIT_OBJECT_0)
		{
			status = pbrpc_process_message_in(server);

			if (status < 0)
			{
				WLog_ERR(TAG, "Transport problem reconnecting..");
				pbrpc_reconnect(context);
				continue;
			}
		}

		if (WaitForSingleObject(Queue_Event(context->writeQueue), 0) == WAIT_OBJECT_0)
		{
			FDSAPI_MSG_PACKET* msg = NULL;

			while ((msg = Queue_Dequeue(context->writeQueue)))
			{
				status = pbrpc_process_message_out(server, msg);
				pbrpc_message_free(msg, FALSE);
			}

			if (status < 0)
			{
				WLog_ERR(TAG, "Transport problem reconnecting..");
				pbrpc_reconnect(context);
				continue;
			}
		}
	}
}

int pbrpc_server_start(pbRPCContext* context)
{
	context->isConnected = FALSE;
	context->thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) pbrpc_main_loop, context, 0, NULL);
	return 0;
}

int pbrpc_server_stop(pbRPCContext* context)
{
	context->isConnected = FALSE;
	SetEvent(context->stopEvent);
	WaitForSingleObject(context->thread, INFINITE);
	tp_npipe_close(context);
	return 0;
}

/** @brief contextual data to handle a local call */
struct pbrpc_local_call_context {
	HANDLE event;
	FDSAPI_MSG_PACKET *response;
	PBRPCSTATUS status;
};

static void pbrpc_response_local_cb(PBRPCSTATUS reason, FDSAPI_MSG_PACKET* response, void *args)
{
	struct pbrpc_local_call_context *context = (struct pbrpc_local_call_context*) args;
	context->response = response;
	context->status = reason;
	SetEvent(context->event);
}

int pbrpc_call_method(rdsServer* server, UINT32 type, pbRPCPayload* request, pbRPCPayload** response)
{
	UINT32 tag;
	DWORD wait_ret;
	pbRPCTransaction ta;
	UINT32 ret = PBRPC_FAILED;
	FDSAPI_MSG_PACKET* msg;
	pbRPCContext* context = server->rpc;
	struct pbrpc_local_call_context local_context;

	if (!context->isConnected)
		return PBRCP_TRANSPORT_ERROR;

	tag = pbrpc_getTag(context);

	msg = pbrpc_message_new();

	msg->callId = tag;
	msg->status = FDSAPI_STATUS_SUCCESS;
	msg->buffer = request->buffer;
	msg->length = request->length;
	msg->msgType = FDSAPI_REQUEST_ID(type);

	ZeroMemory(&local_context, sizeof(local_context));
	local_context.event = CreateEvent(NULL, TRUE, FALSE, NULL);
	local_context.status = PBRCP_CALL_TIMEOUT;

	ta.responseCallback = pbrpc_response_local_cb;
	ta.callbackArg = &local_context;
	ta.freeAfterResponse = FALSE;

	ListDictionary_Add(context->transactions, (void*)((UINT_PTR)(msg->callId)), &ta);
	Queue_Enqueue(context->writeQueue, msg);

	wait_ret = WaitForSingleObject(local_context.event, 10000);

	if (wait_ret != WAIT_OBJECT_0)
	{
		if (!ListDictionary_Remove(context->transactions, (void*)((UINT_PTR)(tag))))
		{
			// special case - timeout occurred but request is already processing, see comment above
			WaitForSingleObject(local_context.event, INFINITE);
		}
		else
		{
			ret = PBRCP_CALL_TIMEOUT;
		}
	}

	CloseHandle(local_context.event);
	msg = local_context.response;

	if (!msg)
	{
		if (local_context.status)
			ret = local_context.status;
		else
			ret = PBRPC_FAILED;
	}
	else
	{
		*response = pbrpc_fill_payload(msg);
		ret = msg->status;
		pbrpc_message_free(msg, FALSE);
	}

	return ret;
}

void pbrcp_call_method_async(pbRPCContext* context, UINT32 type, pbRPCPayload* request,
		pbRpcResponseCallback callback, void *callback_args)
{
	UINT32 tag;
	pbRPCTransaction* ta;
	FDSAPI_MSG_PACKET* msg;

	if (!context->isConnected)
	{
		callback(PBRCP_TRANSPORT_ERROR, 0, callback_args);
		return;
	}

	ta = (pbRPCTransaction*) calloc(1, sizeof(pbRPCTransaction));
	ta->freeAfterResponse = TRUE;
	ta->responseCallback = callback;
	ta->callbackArg = callback_args;

	tag = pbrpc_getTag(context);

	msg = pbrpc_message_new();

	msg->callId = tag;
	msg->status = FDSAPI_STATUS_SUCCESS;
	msg->buffer = request->buffer;
	msg->length = request->length;
	msg->msgType = FDSAPI_REQUEST_ID(type);

	ListDictionary_Add(context->transactions, (void*)((UINT_PTR)(msg->callId)), ta);
	Queue_Enqueue(context->writeQueue, msg);
}

/* RPC Client Stubs */

int freerds_icp_IsChannelAllowed(FDSAPI_CHANNEL_ALLOWED_REQUEST* pRequest, FDSAPI_CHANNEL_ALLOWED_RESPONSE* pResponse)
{
	int status;
	pbRPCPayload pbrequest;
	pbRPCPayload* pbresponse = NULL;
	UINT32 type = FDSAPI_CHANNEL_ALLOWED_REQUEST_ID;

	pbrequest.s = freerds_rpc_msg_pack(type, pRequest, NULL);
	pbrequest.buffer = Stream_Buffer(pbrequest.s);
	pbrequest.length = Stream_Length(pbrequest.s);

	status = pbrpc_call_method(g_Server, FDSAPI_REQUEST_ID(type), &pbrequest, &pbresponse);

	Stream_Free(pbrequest.s, TRUE);

	if (status != 0)
		return status;

	freerds_rpc_msg_unpack(FDSAPI_RESPONSE_ID(type), pResponse, pbresponse->buffer, pbresponse->length);

	pbrpc_free_payload(pbresponse);

	return PBRPC_SUCCESS;
}

int freerds_icp_DisconnectUserSession(FDSAPI_DISCONNECT_USER_REQUEST* pRequest, FDSAPI_DISCONNECT_USER_RESPONSE* pResponse)
{
	int status;
	pbRPCPayload pbrequest;
	pbRPCPayload* pbresponse = NULL;
	UINT32 type = FDSAPI_DISCONNECT_USER_REQUEST_ID;

	pbrequest.s = freerds_rpc_msg_pack(type, pRequest, NULL);
	pbrequest.buffer = Stream_Buffer(pbrequest.s);
	pbrequest.length = Stream_Length(pbrequest.s);

	status = pbrpc_call_method(g_Server, FDSAPI_REQUEST_ID(type), &pbrequest, &pbresponse);

	Stream_Free(pbrequest.s, TRUE);

	if (status != 0)
		return status;

	freerds_rpc_msg_unpack(FDSAPI_RESPONSE_ID(type), pResponse, pbresponse->buffer, pbresponse->length);

	pbrpc_free_payload(pbresponse);

	return PBRPC_SUCCESS;
}

int freerds_icp_LogOffUserSession(FDSAPI_LOGOFF_USER_REQUEST* pRequest, FDSAPI_LOGOFF_USER_RESPONSE* pResponse)
{
	int status;
	pbRPCPayload pbrequest;
	pbRPCPayload* pbresponse = NULL;
	UINT32 type = FDSAPI_LOGOFF_USER_REQUEST_ID;

	pbrequest.s = freerds_rpc_msg_pack(type, pRequest, NULL);
	pbrequest.buffer = Stream_Buffer(pbrequest.s);
	pbrequest.length = Stream_Length(pbrequest.s);

	status = pbrpc_call_method(g_Server, FDSAPI_REQUEST_ID(type), &pbrequest, &pbresponse);

	Stream_Free(pbrequest.s, TRUE);

	if (status != 0)
		return status;

	freerds_rpc_msg_unpack(FDSAPI_RESPONSE_ID(type), pResponse, pbresponse->buffer, pbresponse->length);

	pbrpc_free_payload(pbresponse);

	return PBRPC_SUCCESS;
}

int freerds_icp_LogonUser(FDSAPI_LOGON_USER_REQUEST* pRequest, FDSAPI_LOGON_USER_RESPONSE* pResponse)
{
	int status;
	pbRPCPayload pbrequest;
	pbRPCPayload* pbresponse = NULL;
	UINT32 type = FDSAPI_LOGON_USER_REQUEST_ID;

	pbrequest.s = freerds_rpc_msg_pack(type, pRequest, NULL);
	pbrequest.buffer = Stream_Buffer(pbrequest.s);
	pbrequest.length = Stream_Length(pbrequest.s);

	status = pbrpc_call_method(g_Server, FDSAPI_REQUEST_ID(type), &pbrequest, &pbresponse);

	Stream_Free(pbrequest.s, TRUE);

	if (status != 0)
		return status;

	freerds_rpc_msg_unpack(FDSAPI_RESPONSE_ID(type), pResponse, pbresponse->buffer, pbresponse->length);

	pbrpc_free_payload(pbresponse);

	return PBRPC_SUCCESS;
}

/* RPC server stubs */

int freerds_icp_Heartbeat(FDSAPI_MSG_PACKET* msg, pbRPCPayload** pbresponse)
{
	pbRPCPayload* payload;
	FDSAPI_HEARTBEAT_REQUEST request;
	FDSAPI_HEARTBEAT_RESPONSE response;
	UINT32 type = FDSAPI_HEARTBEAT_REQUEST_ID;

	freerds_rpc_msg_unpack(FDSAPI_REQUEST_ID(type), &request, msg->buffer, msg->length);
	CopyMemory(&request, msg, sizeof(FDSAPI_MSG_HEADER));

	response.msgType = FDSAPI_RESPONSE_ID(type);
	response.callId = request.callId;
	response.HeartbeatId = request.HeartbeatId;

	payload = pbrpc_payload_new();
	payload->s = freerds_rpc_msg_pack(FDSAPI_RESPONSE_ID(type), &response, NULL);
	payload->buffer = Stream_Buffer(payload->s);
	payload->length = Stream_Length(payload->s);
	*pbresponse = payload;

	return PBRPC_SUCCESS;
}

int freerds_icp_SwitchServiceEndpointResponse(FDSAPI_SWITCH_SERVICE_ENDPOINT_RESPONSE* pResponse)
{
	int status;
	wStream* s;
	FDSAPI_MSG_PACKET msg;
	UINT32 type = FDSAPI_SWITCH_SERVICE_ENDPOINT_REQUEST_ID;

	pResponse->status = 0;
	pResponse->msgType = FDSAPI_RESPONSE_ID(type);

	CopyMemory(&msg, pResponse, sizeof(FDSAPI_MSG_HEADER));

	s = freerds_rpc_msg_pack(FDSAPI_RESPONSE_ID(type), pResponse, NULL);
	msg.buffer = Stream_Buffer(s);
	msg.length = Stream_Length(s);

	status = pbrpc_process_message_out(g_Server, &msg);

	Stream_Free(s, TRUE);

	return status;
}

int freerds_icp_SwitchServiceEndpoint(FDSAPI_MSG_PACKET* msg, pbRPCPayload** pbresponse)
{
	rdsConnection* connection = NULL;
	FDSAPI_SWITCH_SERVICE_ENDPOINT_REQUEST* pRequest;
	FDSAPI_SWITCH_SERVICE_ENDPOINT_RESPONSE response;
	UINT32 type = FDSAPI_SWITCH_SERVICE_ENDPOINT_REQUEST_ID;

	pRequest = (FDSAPI_SWITCH_SERVICE_ENDPOINT_REQUEST*) calloc(1, sizeof(FDSAPI_SWITCH_SERVICE_ENDPOINT_REQUEST));

	if (!pRequest)
		return -1;

	freerds_rpc_msg_unpack(FDSAPI_REQUEST_ID(type), pRequest, msg->buffer, msg->length);
	CopyMemory(pRequest, msg, sizeof(FDSAPI_MSG_HEADER));

	response.msgType = FDSAPI_RESPONSE_ID(type);
	response.callId = pRequest->callId;

	connection = freerds_server_get_connection(g_Server, pRequest->ConnectionId);

	if (!connection)
	{
		response.status = 1;
		return -1;
	}

	MessageQueue_Post(connection->notifications, (void*) connection, pRequest->msgType, (void*) pRequest, NULL);
	*pbresponse = NULL;

	return PBRPC_SUCCESS;
}

int freerds_icp_LogoffUserResponse(FDSAPI_LOGOFF_USER_RESPONSE* pResponse)
{
	int status;
	wStream* s;
	FDSAPI_MSG_PACKET msg;
	UINT32 type = FDSAPI_LOGOFF_USER_REQUEST_ID;

	pResponse->status = 0;
	pResponse->msgType = FDSAPI_RESPONSE_ID(type);

	CopyMemory(&msg, pResponse, sizeof(FDSAPI_MSG_HEADER));

	s = freerds_rpc_msg_pack(FDSAPI_RESPONSE_ID(type), pResponse, NULL);
	msg.buffer = Stream_Buffer(s);
	msg.length = Stream_Length(s);

	status = pbrpc_process_message_out(g_Server, &msg);

	Stream_Free(s, TRUE);

	return status;
}

int freerds_icp_LogoffUser(FDSAPI_MSG_PACKET* msg, pbRPCPayload** pbresponse)
{
	rdsConnection* connection = NULL;
	FDSAPI_LOGOFF_USER_REQUEST* pRequest;
	FDSAPI_LOGOFF_USER_RESPONSE response;
	UINT32 type = FDSAPI_LOGOFF_USER_REQUEST_ID;

	pRequest = (FDSAPI_LOGOFF_USER_REQUEST*) calloc(1, sizeof(FDSAPI_LOGOFF_USER_REQUEST));

	if (!pRequest)
		return -1;

	freerds_rpc_msg_unpack(FDSAPI_REQUEST_ID(type), pRequest, msg->buffer, msg->length);
	CopyMemory(pRequest, msg, sizeof(FDSAPI_MSG_HEADER));

	response.msgType = FDSAPI_RESPONSE_ID(type);
	response.callId = pRequest->callId;

	connection = freerds_server_get_connection(g_Server, pRequest->ConnectionId);

	if (!connection)
	{
		response.status = 1;
		return -1;
	}

	MessageQueue_Post(connection->notifications, (void*) connection, pRequest->msgType, (void*) pRequest, NULL);
	*pbresponse = NULL;

	return PBRPC_SUCCESS;
}

int freerds_icp_ChannelEndpointOpenResponse(FDSAPI_CHANNEL_ENDPOINT_OPEN_RESPONSE* pResponse)
{
	int status;
	wStream* s;
	FDSAPI_MSG_PACKET msg;
	UINT32 type = FDSAPI_CHANNEL_ENDPOINT_OPEN_REQUEST_ID;

	pResponse->status = 0;
	pResponse->msgType = FDSAPI_RESPONSE_ID(type);

	CopyMemory(&msg, pResponse, sizeof(FDSAPI_MSG_HEADER));

	s = freerds_rpc_msg_pack(FDSAPI_RESPONSE_ID(type), pResponse, NULL);
	msg.buffer = Stream_Buffer(s);
	msg.length = Stream_Length(s);

	status = pbrpc_process_message_out(g_Server, &msg);

	Stream_Free(s, TRUE);

	return status;
}

int freerds_icp_ChannelEndpointOpen(FDSAPI_MSG_PACKET* msg, pbRPCPayload** pbresponse)
{
	rdsConnection* connection = NULL;
	FDSAPI_CHANNEL_ENDPOINT_OPEN_REQUEST* pRequest;
	UINT32 type = FDSAPI_CHANNEL_ENDPOINT_OPEN_REQUEST_ID;

	pRequest = (FDSAPI_CHANNEL_ENDPOINT_OPEN_REQUEST*) calloc(1, sizeof(FDSAPI_CHANNEL_ENDPOINT_OPEN_REQUEST));

	if (!pRequest)
		return -1;

	freerds_rpc_msg_unpack(FDSAPI_REQUEST_ID(type), pRequest, msg->buffer, msg->length);
	CopyMemory(pRequest, msg, sizeof(FDSAPI_MSG_HEADER));

	connection = freerds_server_get_connection(g_Server, pRequest->ConnectionId);

	if (!connection)
		return -1;

	MessageQueue_Post(connection->notifications, (void*) connection, pRequest->msgType, (void*) pRequest, NULL);
	*pbresponse = NULL;

	return 0;
}

