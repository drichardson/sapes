/*
 * Copyright (c) 2003, Douglas Ryan Richardson
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation and/or
 * other materials provided with the distribution.
 * * Neither the name of the organization nor the names of its contributors may be
 * used to endorse or promote products derived from this software without specific
 * prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef MAILSERV_THREAD_H
#define MAILSERV_THREAD_H

#ifdef WIN32
// Include winsock2.h instead of windows.h because you get conflicts if you do.
#include <winsock2.h>
typedef DWORD THREAD_RETTYPE;
typedef HANDLE MUTEX;
typedef HANDLE SEMAPHORE;

#else
#include <pthread.h>
#include <semaphore.h>
#define WINAPI

typedef void THREAD_RETTYPE;
typedef pthread_mutex_t MUTEX;
typedef sem_t SEMAPHORE;

#endif

// Returns true if the thread is created and able to run.
// Returns false otherwise.
bool create_thread(THREAD_RETTYPE (WINAPI *function_addr)(void*), void* data);

bool create_mutex(MUTEX & mutex);
void delete_mutex(MUTEX & mutex);
bool wait_mutex(MUTEX & mutex);
void release_mutex(MUTEX & mutex);

bool create_semaphore(SEMAPHORE & sem);
void delete_semaphore(SEMAPHORE & sem);
bool wait_semaphore(SEMAPHORE & sem);
void signal_semaphore(SEMAPHORE & sem);

#endif
