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

#include "freerds.h"

#include <winpr/crt.h>
#include <winpr/file.h>
#include <winpr/path.h>
#include <winpr/synch.h>
#include <winpr/thread.h>

#include <freerdp/freerdp.h>
#include <freerdp/listener.h>

#include <errno.h>
#include <sys/select.h>
#include <sys/signal.h>

#include <freerds/auth.h>

#include <winpr/tools/makecert.h>

#include "rpc.h"
#include "channels.h"

extern rdsServer* g_Server;

BOOL freerds_peer_capabilities(freerdp_peer* client)
{
	return TRUE;
}

BOOL freerds_peer_post_connect(freerdp_peer* client)
{
	UINT32 ColorDepth;
	UINT32 DesktopWidth;
	UINT32 DesktopHeight;
	rdpSettings* settings;
	rdsConnection* connection;

	settings = client->settings;
	connection = (rdsConnection*) client->context;

	fprintf(stderr, "Client %s is connected", client->hostname);

	if (client->settings->AutoLogonEnabled)
	{
		fprintf(stderr, " and wants to login automatically as %s\\%s",
			client->settings->Domain ? client->settings->Domain : "",
			client->settings->Username);
	}
	fprintf(stderr, "\n");

	DesktopWidth = settings->DesktopWidth;
	DesktopHeight = settings->DesktopHeight;
	ColorDepth = settings->ColorDepth;

	fprintf(stderr, "Client requested desktop: %dx%dx%d\n",
		settings->DesktopWidth, settings->DesktopHeight, settings->ColorDepth);

	//if ((DesktopWidth % 4) != 0)
	//	DesktopWidth += (DesktopWidth % 4);

	//if ((DesktopHeight % 4) != 0)
	//	DesktopHeight += (DesktopHeight % 4);

	if ((DesktopWidth != settings->DesktopWidth) || (DesktopHeight != settings->DesktopHeight)
			|| (ColorDepth != settings->ColorDepth))
	{
		fprintf(stderr, "Resizing desktop to %dx%dx%d\n", DesktopWidth, DesktopHeight, ColorDepth);

		settings->DesktopWidth = DesktopWidth;
		settings->DesktopHeight = DesktopHeight;
		settings->ColorDepth = ColorDepth;

		client->update->DesktopResize(client->update->context);

		return TRUE;
	}

	freerds_channels_post_connect(connection);

	connection->encoder = freerds_bitmap_encoder_new(settings->DesktopWidth,
			settings->DesktopHeight, settings->ColorDepth);

	return TRUE;
}

int freerds_init_client(HANDLE hClientPipe, rdpSettings* settings, wStream* s)
{
	RDS_MSG_CAPABILITIES capabilities;

	ZeroMemory(&capabilities, sizeof(RDS_MSG_CAPABILITIES));
	capabilities.type = RDS_CLIENT_CAPABILITIES;
	capabilities.Version = 1;
	capabilities.DesktopWidth = settings->DesktopWidth;
	capabilities.DesktopHeight = settings->DesktopHeight;
	capabilities.KeyboardLayout = settings->KeyboardLayout;
	capabilities.KeyboardType = settings->KeyboardType;
	capabilities.KeyboardSubType = settings->KeyboardSubType;

	freerds_write_capabilities(s, &capabilities);

	return freerds_named_pipe_write(hClientPipe, Stream_Buffer(s), Stream_GetPosition(s));
}

BOOL freerds_peer_activate(freerdp_peer* client)
{
	int error_code;
	char* endpoint;
	rdpSettings* settings;
	FDSAPI_LOGON_USER_REQUEST request;
	FDSAPI_LOGON_USER_RESPONSE response;
	rdsConnection* connection = (rdsConnection*) client->context;
	rdsBackendConnector* connector = connection->connector;

	if (connector)
	{
		fprintf(stderr, "reactivation\n");
		return TRUE;
	}

	settings = client->settings;

	if (settings->Password)
		settings->AutoLogonEnabled = TRUE;

	connection->codecMode = (settings->RemoteFxCodec && settings->FrameAcknowledge &&
							settings->SurfaceFrameMarkerEnabled);
	
	fprintf(stderr, "codec mode %d\n", connection->codecMode);

	ZeroMemory(&request, sizeof(request));

	request.ConnectionId = connection->id;
	request.User = settings->Username;
	request.Password = settings->Password;
	request.Domain = settings->Domain;
	request.DesktopWidth = settings->DesktopWidth;
	request.DesktopHeight = settings->DesktopHeight;
	request.ColorDepth = settings->ColorDepth;
	request.ClientName = settings->ClientHostname;
	request.ClientAddress = settings->ClientAddress;
	request.ClientBuild = settings->ClientBuild;
	request.ClientProductId = 1;
	request.ClientHardwareId = 0;
	request.ClientProtocolType = 2;

	error_code = freerds_icp_LogonUser(&request, &response);

	endpoint = response.ServiceEndpoint;

	if (error_code != 0)
	{
		fprintf(stderr, "freerds_icp_LogonUser failed %d\n", error_code);
		return FALSE;
	}

	if (!connector)
		connection->connector = connector = freerds_connector_new(connection);

	connector->Endpoint = endpoint;

	if (!freerds_connector_connect(connector))
		return FALSE;

	fprintf(stderr, "Client Activated\n");

	return TRUE;
}

const char* makecert_argv[4] =
{
	"makecert",
	"-rdp",
	"-live",
	"-silent"
};

int makecert_argc = (sizeof(makecert_argv) / sizeof(char*));

int freerds_generate_certificate(rdpSettings* settings)
{
	char* config_home;
	char* server_file_path;
	MAKECERT_CONTEXT* context;

	config_home = GetKnownPath(KNOWN_PATH_XDG_CONFIG_HOME);

	if (!PathFileExistsA(config_home))
		CreateDirectoryA(config_home, 0);

	free(config_home);

	if (!PathFileExistsA(settings->ConfigPath))
		CreateDirectoryA(settings->ConfigPath, 0);

	server_file_path = GetCombinedPath(settings->ConfigPath, "server");

	if (!PathFileExistsA(server_file_path))
		CreateDirectoryA(server_file_path, 0);

	settings->CertificateFile = GetCombinedPath(server_file_path, "server.crt");
	settings->PrivateKeyFile = GetCombinedPath(server_file_path, "server.key");

	if ((!PathFileExistsA(settings->CertificateFile)) ||
			(!PathFileExistsA(settings->PrivateKeyFile)))
	{
		context = makecert_context_new();

		makecert_context_process(context, makecert_argc, (char**) makecert_argv);

		makecert_context_set_output_file_name(context, "server");

		if (!PathFileExistsA(settings->CertificateFile))
			makecert_context_output_certificate_file(context, server_file_path);

		if (!PathFileExistsA(settings->PrivateKeyFile))
			makecert_context_output_private_key_file(context, server_file_path);

		makecert_context_free(context);
	}

	free(server_file_path);

	return 0;
}

void freerds_input_synchronize_event(rdpInput* input, UINT32 flags)
{
	rdsConnection* connection = (rdsConnection*) input->context;
	rdsBackend* backend = (rdsBackend *)connection->connector;

	if (backend)
	{
		if (backend->client->SynchronizeKeyboardEvent)
		{
			backend->client->SynchronizeKeyboardEvent(backend, flags);
		}
	}
}

void freerds_input_keyboard_event(rdpInput* input, UINT16 flags, UINT16 code)
{
	rdsConnection* connection = (rdsConnection*) input->context;
	rdsBackend* backend = (rdsBackend *)connection->connector;

	if (backend)
	{
		if (backend->client->ScancodeKeyboardEvent)
		{
			backend->client->ScancodeKeyboardEvent(backend, flags, code, connection->settings->KeyboardType);
		}
	}
}

void freerds_input_unicode_keyboard_event(rdpInput* input, UINT16 flags, UINT16 code)
{
	rdsConnection* connection = (rdsConnection*) input->context;
	rdsBackend* backend = (rdsBackend *)connection->connector;

	if (backend)
	{
		if (backend->client->UnicodeKeyboardEvent)
		{
			backend->client->UnicodeKeyboardEvent(backend, flags, code);
		}
	}
}

void freerds_input_mouse_event(rdpInput* input, UINT16 flags, UINT16 x, UINT16 y)
{
	rdsConnection* connection = (rdsConnection*) input->context;
	rdsBackend* backend = (rdsBackend *)connection->connector;

	if (backend)
	{
		if (backend->client->MouseEvent)
		{
			backend->client->MouseEvent(backend, flags, x, y);
		}
	}
}

void freerds_input_extended_mouse_event(rdpInput* input, UINT16 flags, UINT16 x, UINT16 y)
{
	rdsConnection* connection = (rdsConnection*) input->context;
	rdsBackend* backend = (rdsBackend *)connection->connector;

	if (backend)
	{
		if (backend->client->ExtendedMouseEvent)
		{
			backend->client->ExtendedMouseEvent(backend, flags, x, y);
		}
	}
}

void freerds_input_register_callbacks(rdpInput* input)
{
	input->SynchronizeEvent = freerds_input_synchronize_event;
	input->KeyboardEvent = freerds_input_keyboard_event;
	input->UnicodeKeyboardEvent = freerds_input_unicode_keyboard_event;
	input->MouseEvent = freerds_input_mouse_event;
	input->ExtendedMouseEvent = freerds_input_extended_mouse_event;
}

void freerds_update_frame_acknowledge(rdpContext* context, UINT32 frameId)
{
	SURFACE_FRAME* frame;
	rdsConnection* connection = (rdsConnection*) context;

	frame = (SURFACE_FRAME*) ListDictionary_GetItemValue(connection->FrameList, (void*) (size_t) frameId);

	if (frame)
	{
		ListDictionary_Remove(connection->FrameList, (void*) (size_t) frameId);
		free(frame);
	}
}

void freerds_suppress_output(rdpContext* context, BYTE allow, RECTANGLE_16* area)
{
	rdsConnection* connection = (rdsConnection*) context;
	rdsBackend* backend = (rdsBackend *)connection->connector;

	if (backend && backend->client && backend->client->SuppressOutput)
		backend->client->SuppressOutput(backend, allow);
}

BOOL freerds_client_process_switch_session(rdsConnection* connection, wMessage* message)
{
	int error = 0;
	BOOL status = FALSE;
	rdsBackendConnector* connector = NULL;
	FDSAPI_SWITCH_SERVICE_ENDPOINT_REQUEST* request;
	FDSAPI_SWITCH_SERVICE_ENDPOINT_RESPONSE response;

	request = (FDSAPI_SWITCH_SERVICE_ENDPOINT_REQUEST*) message->wParam;

	freerds_connector_free(connection->connector);
	connection->connector = connector = freerds_connector_new(connection);
	connector->Endpoint = request->ServiceEndpoint;

	status = freerds_connector_connect(connector);

	response.status = status ? 0 : 1;
	response.callId = request->callId;
	response.msgType = FDSAPI_RESPONSE_ID(request->msgType);

	error = freerds_icp_SwitchServiceEndpointResponse(&response);

	free(request->ServiceEndpoint);
	free(request);

	if (error != 0)
	{
		fprintf(stderr, "problem occured while switching session\n");
		return FALSE;
	}

	return TRUE;
}

BOOL freerds_client_process_logoff(rdsConnection* connection, wMessage* message)
{
	int status = 0;
	FDSAPI_LOGOFF_USER_REQUEST* request;
	FDSAPI_LOGOFF_USER_RESPONSE response;

	request = (FDSAPI_LOGOFF_USER_REQUEST*) message->wParam;

	if (connection->connector)
	{
		freerds_connector_free(connection->connector);
		connection->connector = NULL;
	}

	connection->client->Close(connection->client);

	response.status = 0;
	response.callId = request->callId;
	response.msgType = FDSAPI_RESPONSE_ID(request->msgType);

	status = freerds_icp_LogoffUserResponse(&response);
	free(request);

	return FALSE;
}

BOOL freerds_client_process_channel_endpoint_open(rdsConnection* connection, wMessage* message)
{
	int status = 0;
	rdsChannel* channel;
	FDSAPI_CHANNEL_ENDPOINT_OPEN_REQUEST* request;
	FDSAPI_CHANNEL_ENDPOINT_OPEN_RESPONSE response;

	request = (FDSAPI_CHANNEL_ENDPOINT_OPEN_REQUEST*) message->wParam;

	channel = freerds_channel_new(connection, request->ChannelName);

	response.status = 0;
	response.callId = request->callId;
	response.msgType = FDSAPI_RESPONSE_ID(request->msgType);

	response.ChannelPort = channel->port;
	response.ChannelGuid = _strdup(channel->guidString);

	status = freerds_icp_ChannelEndpointOpenResponse(&response);

	freerds_rpc_msg_free(request->msgType, request);
	free(request);

	return TRUE;
}

BOOL freerds_client_process_notification(rdsConnection* connection, wMessage* message)
{
	BOOL status = FALSE;

	switch (message->id)
	{
		case FDSAPI_SWITCH_SERVICE_ENDPOINT_REQUEST_ID:
			status = freerds_client_process_switch_session(connection, message);
			break;

		case FDSAPI_LOGOFF_USER_REQUEST_ID:
			status = freerds_client_process_logoff(connection, message);
			break;

		case FDSAPI_CHANNEL_ENDPOINT_OPEN_REQUEST_ID:
			status = freerds_client_process_channel_endpoint_open(connection, message);
			break;

		default:
			fprintf(stderr, "%s: unhandled message 0x%x\n", __FUNCTION__, message->id);
			break;
	}

	return status;
}

void* freerds_connection_main_thread(void* arg)
{
	DWORD status;
	DWORD nCount;
	HANDLE events[32];
	HANDLE ClientEvent;
	HANDLE ChannelEvent;
	HANDLE LocalTermEvent;
	HANDLE GlobalTermEvent;
	HANDLE NotificationEvent;
	rdsConnection* connection;
	rdpSettings* settings;
	rdsBackendConnector* connector = NULL;
	freerdp_peer* client = (freerdp_peer*) arg;
#ifndef _WIN32
	sigset_t set;
	int ret;
#endif

	fprintf(stderr, "We've got a client %s\n", client->hostname);

	connection = (rdsConnection*) client->context;
	settings = client->settings;

	freerds_server_add_connection(g_Server, connection);
	freerds_generate_certificate(settings);

	settings->RdpSecurity = TRUE;
	settings->TlsSecurity = TRUE;

	/**
	 * Disable NLA Security for now.
	 * TODO: make this a configurable option.
	 */
	settings->NlaSecurity = FALSE;

	client->Capabilities = freerds_peer_capabilities;
	client->PostConnect = freerds_peer_post_connect;
	client->Activate = freerds_peer_activate;

	client->Initialize(client);

	freerds_input_register_callbacks(client->input);

	client->update->SurfaceFrameAcknowledge = freerds_update_frame_acknowledge;
	client->update->SuppressOutput = freerds_suppress_output;

	ClientEvent = client->GetEventHandle(client);
	ChannelEvent = WTSVirtualChannelManagerGetEventHandle(connection->vcm);

	GlobalTermEvent = g_get_term_event();
	LocalTermEvent = connection->TermEvent;
	NotificationEvent = MessageQueue_Event(connection->notifications);

#ifndef _WIN32
	sigemptyset(&set);
	sigaddset(&set, SIGPIPE);
	ret = pthread_sigmask(SIG_BLOCK, &set, NULL);
	if (0 != ret)
		fprintf(stderr, "couldn't block SIGPIPE\n");
#endif

	while (1)
	{
		nCount = 0;
		events[nCount++] = ClientEvent;
		events[nCount++] = ChannelEvent;
		events[nCount++] = GlobalTermEvent;
		events[nCount++] = LocalTermEvent;
		events[nCount++] = NotificationEvent;

		if (client->activated)
		{
			connector = (rdsBackendConnector*) connection->connector;

			if (connector && connector->GetEventHandles)
				connector->GetEventHandles((rdsBackend *)connector, events, &nCount);
		}

		status = WaitForMultipleObjects(nCount, events, FALSE, INFINITE);

		if (WaitForSingleObject(GlobalTermEvent, 0) == WAIT_OBJECT_0)
		{
			fprintf(stderr, "GlobalTermEvent\n");
			break;
		}

		if (WaitForSingleObject(LocalTermEvent, 0) == WAIT_OBJECT_0)
		{
			fprintf(stderr, "LocalTermEvent\n");
			break;
		}

		if (WaitForSingleObject(ClientEvent, 0) == WAIT_OBJECT_0)
		{
			if (client->CheckFileDescriptor(client) != TRUE)
			{
				fprintf(stderr, "Failed to check freerdp file descriptor\n");
				break;
			}
		}

		if (WaitForSingleObject(ChannelEvent, 0) == WAIT_OBJECT_0)
		{
			if (WTSVirtualChannelManagerCheckFileDescriptor(connection->vcm) != TRUE)
			{
				fprintf(stderr, "WTSVirtualChannelManagerCheckFileDescriptor failure\n");
				break;
			}
		}

		if (client->activated)
		{
			if (connector && connector->CheckEventHandles)
			{
				if (connector->CheckEventHandles((rdsBackend *)connector) < 0)
				{
					fprintf(stderr, "ModuleClient->CheckEventHandles failure\n");
					break;
				}
			}
		}

		if (WaitForSingleObject(NotificationEvent, 0) == WAIT_OBJECT_0)
		{
			wMessage message;

			MessageQueue_Peek(connection->notifications, &message, TRUE);

			if (!freerds_client_process_notification(connection, &message))
				break;
		}
	}

	fprintf(stderr, "Client %s disconnected.\n", client->hostname);

	if (connection->connector)
	{
		FDSAPI_DISCONNECT_USER_REQUEST request;
		FDSAPI_DISCONNECT_USER_RESPONSE response;

		freerds_connector_free(connection->connector);
		connection->connector = NULL;

		request.ConnectionId = connection->id;
		freerds_icp_DisconnectUserSession(&request, &response);
	}

	client->Disconnect(client);
	freerds_server_remove_connection(g_Server, connection->id);

	freerdp_peer_context_free(client);
	freerdp_peer_free(client);

	return NULL;
}

