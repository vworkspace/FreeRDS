/**
 * Executor which runs Task objects
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

#ifndef __EXECUTOR_H_
#define __EXECUTOR_H_

#include <list>

#include "Task.h"

#include <utils/SignalingQueue.h>

#define PIPE_BUFFER_SIZE	0xFFFF

namespace freerds
{
	class Executor
	{
	public:
		Executor();
		~Executor();

		int start();
		int stop();

		void runExecutor();

		bool addTask(TaskPtr task);

	private:
		static void* execThread(void* arg);
		static void* execTask(void* arg);

		bool checkThreadHandles(const HANDLE value) const;
		bool waitThreadHandles(const HANDLE value) const;

	private:
		HANDLE m_hStopEvent;
		HANDLE m_hServerThread;

		HANDLE m_hStopThreads;
		HANDLE m_hTaskThreadStarted;

		bool m_Running;

		std::list<HANDLE> m_TaskThreadList;
		CRITICAL_SECTION m_CSection;
		SignalingQueue<TaskPtr> m_Tasks;
	};
}

#endif /* __EXECUTOR_H_ */
