/**
 * Task for service call EndSession
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

#include <winpr/wlog.h>

#include "TaskEndSession.h"

#include <session/ApplicationContext.h>
#include <call/CallOutLogOffUserSession.h>

namespace freerds
{
	static wLog* logger_EndSessionTask = WLog_Get("freerds.TaskEndSession");

	void TaskEndSession::run()
	{
		UINT32 connectionId = APP_CONTEXT.getConnectionStore()->getConnectionIdForSessionId(m_SessionId);

		if (connectionId == 0)
		{
			stopSession();
		}
		else
		{
			CallOutLogOffUserSession logoffSession;

			logoffSession.setConnectionId(connectionId);

			APP_CONTEXT.getRpcOutgoingQueue()->addElement(&logoffSession);
			WaitForSingleObject(logoffSession.getAnswerHandle(),INFINITE);

			if (logoffSession.getResult() == 0)
			{
				if (logoffSession.isLoggedOff())
				{
					stopSession();
					APP_CONTEXT.getConnectionStore()->removeConnection(connectionId);
				}
				else
				{
					WLog_Print(logger_EndSessionTask, WLOG_ERROR, "CallOutLogOffUserSession reported that logoff in freerds was not successful!");
				}
			}
			else
			{
				// report error
				WLog_Print(logger_EndSessionTask, WLOG_ERROR, "CallOutLogOffUserSession reported error %d!", logoffSession.getResult());
			}
		}
	}

	void TaskEndSession::setSessionId(UINT32 sessionId) {
		m_SessionId = sessionId;
	}

	void TaskEndSession::stopSession()
	{
		SessionPtr session = APP_CONTEXT.getSessionStore()->getSession(m_SessionId);

		if (session)
		{
			session->stopModule();
			APP_CONTEXT.getSessionStore()->removeSession(m_SessionId);
		}
		else
		{
			WLog_Print(logger_EndSessionTask, WLOG_ERROR, "session for id %d was not found!", m_SessionId);
		}
	}
}

