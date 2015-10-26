/**
 * Class for rpc call LogOffUserSession (freerds to session manager)
 *
 * Copyright 2013 Thincast Technologies GmbH
 * Copyright 2013 DI (FH) Martin Haimberger <martin.haimberger@thincast.com>
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

#include "CallInLogOffUserSession.h"

#include <session/ApplicationContext.h>

namespace freerds
{
	static wLog* logger_CallInLogOffUserSession = WLog_Get("freerds.CallInLogOffUserSession");

	CallInLogOffUserSession::CallInLogOffUserSession()
	: m_RequestId(FDSAPI_LOGOFF_USER_REQUEST_ID), m_ResponseId(FDSAPI_LOGOFF_USER_RESPONSE_ID)
	{
		mConnectionId = 0;
		mLoggedOff = false;
	};

	CallInLogOffUserSession::~CallInLogOffUserSession()
	{

	};

	unsigned long CallInLogOffUserSession::getCallType()
	{
		return m_RequestId;
	};

	int CallInLogOffUserSession::decodeRequest()
	{
		BYTE* buffer;
		UINT32 length;

		buffer = (BYTE*) mEncodedRequest.data();
		length = (UINT32) mEncodedRequest.size();

		freerds_rpc_msg_unpack(m_RequestId, &m_Request, buffer, length);

		mConnectionId = m_Request.ConnectionId;

		freerds_rpc_msg_free(m_RequestId, &m_Request);

		WLog_Print(logger_CallInLogOffUserSession, WLOG_DEBUG,
			"request: connectionId=%lu", mConnectionId);

		return 0;
	};

	int CallInLogOffUserSession::encodeResponse()
	{
		wStream* s;

		WLog_Print(logger_CallInLogOffUserSession, WLOG_DEBUG,
			"response: connectionId=%lu", mConnectionId);

		m_Response.ConnectionId = mConnectionId;

		s = freerds_rpc_msg_pack(m_ResponseId, &m_Response, NULL);

		mEncodedResponse.assign((const char*) Stream_Buffer(s), Stream_Length(s));

		Stream_Free(s, TRUE);

		return 0;
	};

	int CallInLogOffUserSession::doStuff()
	{
		ConnectionPtr currentConnection = APP_CONTEXT.getConnectionStore()->getConnection(mConnectionId);

		if (!currentConnection)
		{
			WLog_Print(logger_CallInLogOffUserSession, WLOG_ERROR,
				"connection does not exist for connectionId=%lu", mConnectionId);
			mLoggedOff = false;
			return -1;
		}

		UINT32 sessionId = currentConnection->getSessionId();
		SessionPtr currentSession = APP_CONTEXT.getSessionStore()->getSession(sessionId);

		if (!currentSession)
		{
			WLog_Print(logger_CallInLogOffUserSession, WLOG_ERROR,
				"session does not exist for sessionId=%lu", sessionId);
			mLoggedOff = false;
			return -1;
		}

		WLog_Print(logger_CallInLogOffUserSession, WLOG_DEBUG,
			"logging off session (connectionId=%lu, sessionId=%lu)",
			mConnectionId, sessionId);

		currentSession->stopModule();

		APP_CONTEXT.getSessionStore()->removeSession(sessionId);
		APP_CONTEXT.getConnectionStore()->removeConnection(mConnectionId);
		mLoggedOff = true;

		return 0;
	}
}
