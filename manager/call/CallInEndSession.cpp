/**
 * Class for rpc call CallInEndSession (service to session manager)
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

#include "CallInEndSession.h"
#include "TaskEndSession.h"

#include <session/ApplicationContext.h>

namespace freerds
{
	static wLog* logger_CallInEndSession = WLog_Get("freerds.CallInEndSession");

	CallInEndSession::CallInEndSession()
	: m_RequestId(FDSAPI_END_SESSION_REQUEST_ID), m_ResponseId(FDSAPI_END_SESSION_RESPONSE_ID)
	{
		mSessionId = 0;
		mSuccess = false;
	};

	CallInEndSession::~CallInEndSession()
	{

	};

	unsigned long CallInEndSession::getCallType()
	{
		return m_RequestId;
	};

	int CallInEndSession::decodeRequest()
	{
		BYTE* buffer;
		UINT32 length;

		buffer = (BYTE*) mEncodedRequest.data();
		length = (UINT32) mEncodedRequest.size();

		freerds_rpc_msg_unpack(m_RequestId, &m_Request, buffer, length);

		mSessionId = m_Request.SessionId;

		freerds_rpc_msg_free(m_RequestId, &m_Request);

		WLog_Print(logger_CallInEndSession, WLOG_DEBUG,
			"request: sessionId=%lu", mSessionId);

		return 0;
	};

	int CallInEndSession::encodeResponse()
	{
		wStream* s;

		WLog_Print(logger_CallInEndSession, WLOG_DEBUG,
			"response: sessionId=-%lu, status=%d",
			mSessionId, 0);

		m_Response.status = 0;
		m_Response.SessionId = mSessionId;

		s = freerds_rpc_msg_pack(m_ResponseId, &m_Response, NULL);

		mEncodedResponse.assign((const char*) Stream_Buffer(s), Stream_Length(s));

		Stream_Free(s, TRUE);

		return 0;
	};

	int CallInEndSession::doStuff()
	{
		WLog_Print(logger_CallInEndSession, WLOG_DEBUG,
			"terminating session (sessionId=%lu)", mSessionId);

		TaskEndSessionPtr task = TaskEndSessionPtr(new TaskEndSession());
		task->setSessionId(mSessionId);
		APP_CONTEXT.addTask(task);

		mSuccess = true;

		return 0;
	}
}
