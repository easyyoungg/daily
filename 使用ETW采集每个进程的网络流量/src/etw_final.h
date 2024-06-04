#ifndef ETW_TRACE_H
#define ETW_TRACE_H

#define INITGUID
#include <windows.h>
#include <evntrace.h>
#include <tdh.h>

#define LOGSESSION_NAME L"My Event Trace Session"//KERNEL_LOGGER_NAMEW//

#define PROVIDER_GUID { 0x7DD42A49, 0x5329, 0x4832, 0x8D, 0xFD, 0x43, 0xD9, 0x79, 0x15, 0x3A, 0x88 }
#define SESSION_GUID { 0xae44cb98, 0xbd11, 0x4069,  0x80, 0x93, 0x77, 0xe, 0xc9, 0x25, 0x8a, 0x14  }

#define ARR_SIZE(arr) (sizeof(arr)/sizeof(arr[0]))

typedef struct {
    ULONG pid;
    ULONG64 sentBytes;
    ULONG64 receivedBytes;
} NetworkTraffic;

NetworkTraffic* GetNetworkTrafficByPID(ULONG pid);
void ProcessEvent(PEVENT_RECORD pEvent);
void Controller();
void Consumer();

BOOL g_bUserMode = 0;
TRACEHANDLE g_hTrace = 0;

#ifdef __cplusplus
#endif

#endif // ETW_TRACE_H