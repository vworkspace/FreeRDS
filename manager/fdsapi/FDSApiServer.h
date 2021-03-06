/**
 *  FDSApiServer
 *
 *  Starts and stops the thrift server.
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


#ifndef FDSAPISERVER_H_
#define FDSAPISERVER_H_

#include <winpr/synch.h>
#include <winpr/thread.h>

#include <freerds/rpc.h>

#include <boost/shared_ptr.hpp>

#include "FDSApiHandler.h"

using boost::shared_ptr;

namespace freerds
{
	class FDSApiServer
	{
	public:
		FDSApiServer();
		virtual ~FDSApiServer();

		void startFDSApi();
		void stopFDSApi();

		CRITICAL_SECTION* getCritSection();

		void fireSessionEvent(UINT32 sessionId, UINT32 stateChange);

	private:
		static int RpcConnectionAccepted(rdsRpcClient* rpcClient);
		static int RpcConnectionClosed(rdsRpcClient* rpcClient);
		static int RpcMessageReceived(rdsRpcClient* rpcClient, BYTE* buffer, UINT32 length);

		CRITICAL_SECTION m_CSection;
		HANDLE m_ServerThread;

		static shared_ptr<FDSApiHandler> m_FDSApiHandler;

		rdsRpcServer* m_RpcServer;
	};
}

#endif /* FDSAPISERVER_H_ */
