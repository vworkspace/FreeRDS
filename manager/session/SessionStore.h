/**
 * Session store class
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

#ifndef __SESSIONSTORE_H_
#define __SESSIONSTORE_H_

#include "Session.h"

#include <map>
#include <list>
#include <string>

#include <winpr/synch.h>

namespace freerds
{
	typedef std::map<UINT32, SessionPtr> TSessionMap;
	typedef std::pair<UINT32, SessionPtr> TSessionPair;

	class SessionStore
	{
	public:
		SessionStore();
		~SessionStore();

		SessionPtr getSession(UINT32 sessionId);
		SessionPtr getFirstSessionUserName(std::string username, std::string domain);
		SessionPtr getFirstDisconnectedSessionUserName(std::string username, std::string domain);
		SessionPtr createSession();
		std::list<SessionPtr> getAllSessions();
		int removeSession(UINT32 sessionId);

	private:
		TSessionMap m_SessionMap;
		UINT32 m_NextSessionId;
		CRITICAL_SECTION m_CSection;
	};
}

#endif //__SESSIONSTORE_H_
