#ifdef _WIN32
	#define _WINSOCK_DEPRECATED_NO_WARNINGS
	#define _CRT_SECURE_NO_WARNINGS 
	#define _SCL_SECURE_NO_WARNINGS
	
	#include <stdio.h>
	#include <stdlib.h>
	#include <WinSock2.h>
	#include <process.h>

	#ifdef _MSC_VER
		#pragma comment(lib, "Ws2_32.lib") //link to dll
	#endif
#else
	#include <pthread.h>
	#include <arpa/inet.h>
	#include <sys/types.h>
	#include <sys/time.h>
	#include <sys/socket.h>
	#include <unistd.h>
	#include <stdio.h>
	#include <errno.h>
#endif

#ifdef _WIN32
	//Typedefs section
	typedef HANDLE THREAD_HANDLE;
	typedef HANDLE* PTR_EVENT_HANDLE;

	//Threads and Atoms section
	#define DEFINE_ATOM(ATOM_NAME) CRITICAL_SECTION ATOM_NAME;
	#define PTR_DEFINE_ATOM(ATOM_NAME) CRITICAL_SECTION* ATOM_NAME;
	#define DEFINE_EVENT(EVENT_NAME) HANDLE EVENT_NAME = CreateEvent(NULL, true, false, NULL);
	#define DESTROY_EVENT(EVENT_NAME) CloseHandle(EVENT_NAME);
	#define DEFINE_THREAD_PROCEDURE(PROC_NAME) unsigned int WINAPI PROC_NAME( void* arg )
	#define ATOM_LOCK(ATOM_NAME) EnterCriticalSection( &ATOM_NAME );
	#define ATOM_UNLOCK(ATOM_NAME) LeaveCriticalSection( &ATOM_NAME );
	#define DESTROY_ATOM(ATOM_NAME) DeleteCriticalSection(&ATOM_NAME);
	#define EVENT_WAIT(EVENT_NAME,ATOM_NAME) \
		if (WaitForSingleObject(EVENT_NAME, INFINITE) == WAIT_FAILED) \
		{ \
			printf("beda\n"); \
		}
	#define EVENT_SEND(EVENT_NAME) SetEvent(EVENT_NAME)
	#define START_THREAD_DEMON(PROC_NAME,PARAM,THREAD_ID) \
		(HANDLE) _beginthreadex(NULL,0,PROC_NAME,PARAM,0,&THREAD_ID)

	//Sockets section
	#define SOCKET_CLOSE(SOCKET_NAME,ERROR_DESCRIPTION) \
		if (closesocket(SOCKET_NAME) == SOCKET_ERROR) \
		{ \
			printf(ERROR_DESCRIPTION,WSAGetLastError()); \
		}
	#define SOCKET_NON_BLOCK(SOCKET_NAME,ERROR_DESCRIPTION) \
		u_long iMode = 1;\
		int iResult;\
		iResult = ioctlsocket(SOCKET_NAME,FIONBIO,&iMode); \
		if (iResult != NO_ERROR) \
		{ \
			printf(ERROR_DESCRIPTION, iResult); \
		}
#else
	//Typedefs section
	typedef pthread_t THREAD_HANDLE;
	typedef int SOCKET;
	typedef sockaddr SOCKADDR;
	typedef pthread_cond_t* PTR_EVENT_HANDLE;

	//Threads and Atoms section
	#define DEFINE_ATOM(ATOM_NAME)   pthread_mutex_t ATOM_NAME = PTHREAD_MUTEX_INITIALIZER;
	#define PTR_DEFINE_ATOM(ATOM_NAME) pthread_mutex_t* ATOM_NAME; 
	#define DEFINE_EVENT(EVENT_NAME) pthread_cond_t  EVENT_NAME = PTHREAD_COND_INITIALIZER;
	#define DESTROY_EVENT(EVENT_NAME) pthread_cond_destroy(&EVENT_NAME);
	#define DEFINE_THREAD_PROCEDURE(PROC_NAME) void * PROC_NAME(void *arg)
	#define ATOM_LOCK(ATOM_NAME) pthread_mutex_lock( &ATOM_NAME )
	#define ATOM_UNLOCK(ATOM_NAME) pthread_mutex_unlock( &ATOM_NAME )
	#define DESTROY_ATOM(ATOM_NAME) pthread_mutex_destroy(&ATOM_NAME);
	#define EVENT_WAIT(EVENT_NAME,ATOM_NAME) { \
			pthread_mutex_lock(&ATOM_NAME);\
			pthread_cond_wait(&EVENT_NAME,&ATOM_NAME);\
			pthread_mutex_unlock(&ATOM_NAME);\
		}
	#define EVENT_SEND(EVENT_NAME) pthread_cond_signal(&EVENT_NAME);
	#define START_THREAD_DEMON(PROC_NAME,PARAM,THREAD_ID) \
		pthread_create(&THREAD_ID,NULL,&PROC_NAME,PARAM);
	
	//Sockets section
	#define SOCKET_ERROR -1
	#define SOCKET_CLOSE(SOCKET_NAME,ERROR_DESCRIPTION) \
		close(SOCKET_NAME);
	#define SOCKET_NON_BLOCK(SOCKET_NAME,ERROR_DESCRIPTION) \
		if (fcntl(SOCKET_NAME, F_SETFL, O_NONBLOCK) == SOCKET_ERROR) \
		{ \
			printf("error: %d", errno); \
		}
#endif