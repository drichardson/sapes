/*
 * Copyright (c) 2003, Douglas Ryan Richardson
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 * * Neither the name of the organization nor the names of its contributors may
 *   be used to endorse or promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "thread.h"

#ifdef WIN32
#include <windows.h>
#endif

bool create_thread(THREAD_RETTYPE (WINAPI *function_addr)(void*), void* data)
{
#ifdef WIN32
	DWORD id;
	return CreateThread(NULL, 0, function_addr, data, 0, &id) != NULL;
#else
	pthread_t thread;
	return pthread_create(&thread, NULL, function_addr, data) == 0;
#endif
}

bool create_mutex(MUTEX & mutex)
{
#ifdef WIN32
	mutex = CreateMutex(NULL, FALSE, NULL);
	return mutex != NULL;
#else
	return pthread_mutex_init(&mutex, NULL) == 0;
#endif
}

void delete_mutex(MUTEX & mutex)
{
#ifdef WIN32
	CloseHandle(mutex);
#else
	pthread_mutex_destroy(&mutex);
#endif
}

bool wait_mutex(MUTEX & mutex)
{
#ifdef WIN32
	return WaitForSingleObject(mutex, INFINITE) != WAIT_FAILED;
#else
	pthread_mutex_lock(&mutex);
	return true;
#endif
}

void release_mutex(MUTEX & mutex)
{
#ifdef WIN32
	ReleaseMutex(mutex);
#else
	pthread_mutex_unlock(&mutex);
#endif
}


bool create_semaphore(SEMAPHORE & sem)
{
#ifdef WIN32
	const LONG MAX_SEM_COUNT = 100000;
	sem = CreateSemaphore(NULL, 0, MAX_SEM_COUNT, NULL);
	return sem != NULL;
#else
	return sem_init(&sem, 0, 0) == 0;
#endif
}

void delete_semaphore(SEMAPHORE & sem)
{
#ifdef WIN32
	CloseHandle(sem);
#else
	sem_destroy(&sem);
#endif
}

bool wait_semaphore(SEMAPHORE & sem)
{
#ifdef WIN32
	return WaitForSingleObject(sem, INFINITE) != WAIT_FAILED;
#else
	return sem_wait(&sem) == 0;
#endif
}

void signal_semaphore(SEMAPHORE & sem)
{
#ifdef WIN32
	ReleaseSemaphore(sem, 1, NULL);
#else
	sem_post(&sem);
#endif
}
