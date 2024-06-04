#include "etw_final.h"
#include <windows.h>
#include <stdio.h>
#include <conio.h>
#include <strsafe.h>
#include <wmistr.h>
#include <evntrace.h>
#include <evntcons.h>
#include <comutil.h>
#include <wbemidl.h>
#include <fstream>
#include <tdh.h>
#include <in6addr.h>
#include <mutex>
#include <string>


static std::mutex g_print_mutex;
static std::ofstream g_outfile;

const GUID SessionGuid = SESSION_GUID;

const GUID FileIoProviderGuid = PROVIDER_GUID;

#define MAX_TRAFFIC_COUNT 1024

static NetworkTraffic trafficData[MAX_TRAFFIC_COUNT];
static int trafficCount = 0;
static TRACEHANDLE sessionHandle = 0;
static TRACEHANDLE traceHandle = 0;

void Controller()
{
    ULONG status = ERROR_SUCCESS;
    TRACEHANDLE SessionHandle = 0;
    EVENT_TRACE_PROPERTIES* pSessionProperties = NULL;
    ULONG BufferSize = 0;
    BOOL TraceOn = TRUE;
    ENABLE_TRACE_PARAMETERS EnableParameters;

    BufferSize = sizeof(EVENT_TRACE_PROPERTIES) + 2 * MAX_PATH * sizeof(WCHAR);
    pSessionProperties = (EVENT_TRACE_PROPERTIES*)malloc(BufferSize);
    if (NULL == pSessionProperties)
    {
        wprintf(L"Unable to allocate %d bytes for properties structure.\n", BufferSize);
        goto cleanup;
    }

    ZeroMemory(pSessionProperties, BufferSize);
    pSessionProperties->Wnode.BufferSize = BufferSize;
    pSessionProperties->Wnode.Flags = WNODE_FLAG_TRACED_GUID;
    pSessionProperties->Wnode.ClientContext = 1; //1:QPC clock resolution,
    pSessionProperties->Wnode.Guid = SessionGuid;
    pSessionProperties->LogFileMode = EVENT_TRACE_REAL_TIME_MODE;// | EVENT_TRACE_SYSTEM_LOGGER_MODE;  
    pSessionProperties->MaximumFileSize = 1024;  // 1 MB
    pSessionProperties->FlushTimer = 1;
    pSessionProperties->LoggerNameOffset = sizeof(EVENT_TRACE_PROPERTIES);
    wcscpy_s((WCHAR*)((char*)pSessionProperties + pSessionProperties->LoggerNameOffset), ARR_SIZE(LOGSESSION_NAME), LOGSESSION_NAME);


    //StringCbCopy((LPWSTR)((char*)pSessionProperties + pSessionProperties->LogFileNameOffset), sizeof(LOGFILE_PATH), LOGFILE_PATH);
    //pSessionProperties->EnableFlags = EVENT_TRACE_FLAG_PROCESS | EVENT_TRACE_FLAG_PROCESS_COUNTERS | EVENT_TRACE_FLAG_DISK_FILE_IO | EVENT_TRACE_FLAG_DISK_IO | EVENT_TRACE_FLAG_DISK_IO_INIT | EVENT_TRACE_FLAG_FILE_IO | EVENT_TRACE_FLAG_FILE_IO_INIT;
    //pSessionProperties->LogFileNameOffset = sizeof(EVENT_TRACE_PROPERTIES) + sizeof(WCHAR)*MAX_PATH;

    ZeroMemory(&EnableParameters, sizeof(EnableParameters));
    EnableParameters.Version = ENABLE_TRACE_PARAMETERS_VERSION_2;
    EnableParameters.EnableProperty = EVENT_ENABLE_PROPERTY_STACK_TRACE;

    status = StartTrace(&SessionHandle, LOGSESSION_NAME, pSessionProperties);
    if (ERROR_SUCCESS != status)
    {
        wprintf(L"StartTrace() failed with %lu\n", status);
        //log_messageA("StartTrace() failed %lu\n", status);

        if (status == ERROR_ALREADY_EXISTS) {
            status = ::ControlTrace(
                (TRACEHANDLE)NULL,
                LOGSESSION_NAME,
                pSessionProperties,
                EVENT_TRACE_CONTROL_STOP);

            wprintf(L"stop trace return : %d\n", status);
            if (SUCCEEDED(status)) {
                status = ::StartTrace(
                    (PTRACEHANDLE)&SessionHandle,
                    LOGSESSION_NAME,
                    pSessionProperties);
                wprintf(L"again starttrace return : %d\n", status);
            }
        }
    }

    if (ERROR_SUCCESS != status) {
        goto cleanup;
    }
    wprintf(L"starttrace success\n");
    status = EnableTraceEx2(
        SessionHandle,
        (LPCGUID)&FileIoProviderGuid,
        EVENT_CONTROL_CODE_ENABLE_PROVIDER,
        TRACE_LEVEL_INFORMATION,
        0,
        0,
        0,
        &EnableParameters
    );

    if (ERROR_SUCCESS != status)
    {
        wprintf(L"EnableTrace() failed with %lu\n", status);
        TraceOn = FALSE;
        goto cleanup;
    }

    wprintf(L"Run the provider application. Then hit any key to stop the session.\n");
    //log_messageA("Run the provider application.Then hit any key to stop the session.\n");
    _getch();

cleanup:

    if (SessionHandle)
    {
        if (TraceOn)
        {
            status = EnableTraceEx2(
                SessionHandle,
                (LPCGUID)&FileIoProviderGuid,
                EVENT_CONTROL_CODE_DISABLE_PROVIDER,
                TRACE_LEVEL_INFORMATION,
                0,
                0,
                0,
                NULL
            );
        }

        status = ControlTrace(SessionHandle, LOGSESSION_NAME, pSessionProperties, EVENT_TRACE_CONTROL_STOP);

        if (ERROR_SUCCESS != status)
        {
            wprintf(L"ControlTrace(stop) failed with %lu\n", status);
        }
    }

    if (pSessionProperties)
    {
        free(pSessionProperties);
        pSessionProperties = NULL;
    }

}

void Consumer()
{
    TDHSTATUS status = ERROR_SUCCESS;
    EVENT_TRACE_LOGFILE trace;
    HRESULT hr = S_OK;
    TRACE_LOGFILE_HEADER* pHeader = &trace.LogfileHeader;

    ZeroMemory(&trace, sizeof(EVENT_TRACE_LOGFILE));
    trace.LogFileName = NULL;
    trace.LoggerName = (LPWSTR)LOGSESSION_NAME;
    trace.EventRecordCallback = (PEVENT_RECORD_CALLBACK)ProcessEvent;
    trace.ProcessTraceMode = PROCESS_TRACE_MODE_EVENT_RECORD | PROCESS_TRACE_MODE_REAL_TIME;
    g_hTrace = OpenTrace(&trace);
    if (INVALID_PROCESSTRACE_HANDLE == g_hTrace)
    {
        //log_messageA("OpenTrace failed with %lu\n", GetLastError());
        wprintf(L"OpenTrace failed with %lu\n", GetLastError());
        goto cleanup;
    }

    //g_bUserMode = pHeader->LogFileMode & EVENT_TRACE_PRIVATE_LOGGER_MODE;  // 事件跟踪会话的日志记录模式
    Sleep(10000);

    status = ProcessTrace(&g_hTrace, 1, 0, 0);
    if (status != ERROR_SUCCESS && status != ERROR_CANCELLED)
    {
        //log_messageA("ProcessTrace failed with %lu\n", status);
        wprintf(L"ProcessTrace failed with %lu\n", status);
        goto cleanup;
    }

    wprintf(L"ProcessTrace return: %lu, press any key to quit\n\n", status);
    _getch();

cleanup:

    if (INVALID_PROCESSTRACE_HANDLE != g_hTrace)
    {
        status = CloseTrace(g_hTrace);
    }
}   

NetworkTraffic* FindOrCreateTrafficData(ULONG pid) {
    std::lock_guard<std::mutex>lock(g_print_mutex);
    for (int i = 0; i < trafficCount; i++) {
        if (trafficData[i].pid == pid) {
            return &trafficData[i];
        }
    }
    if (trafficCount < MAX_TRAFFIC_COUNT) {
        trafficData[trafficCount].pid = pid;
        trafficData[trafficCount].sentBytes = 0;
        trafficData[trafficCount].receivedBytes = 0;
        return &trafficData[trafficCount++];
    }
    return NULL;
}


// 回调函数
void ProcessEvent(PEVENT_RECORD pEvent)
{
    DWORD status = ERROR_SUCCESS;
    PTRACE_EVENT_INFO pInfo = NULL;
    LPWSTR pwsEventGuid = NULL;
    PBYTE pUserData = NULL;
    PBYTE pEndOfUserData = NULL;
    DWORD PointerSize = 0;
    ULONGLONG TimeStamp = 0;
    ULONGLONG Nanoseconds = 0;
    wprintf(L"ProcessEvent  ");
    /*每个ETL文件中的第一个事件都包含文件头中的数据。这与OpenTrace在EVENT_TRACE_LOGFILEW中返回的数据相同。由于我们已经看到了这些信息，我们将跳过此活动。实时会话没有这部分*/
    if (IsEqualGUID(pEvent->EventHeader.ProviderId, SESSION_GUID) &&
        pEvent->EventHeader.EventDescriptor.Opcode == EVENT_TRACE_TYPE_INFO)
    {
        wprintf(L"IsEqualGUID\n");

    }
    else
    {
        wprintf(L"Opcode is %d  ", pEvent->EventHeader.EventDescriptor.Opcode);
        if (pEvent->EventHeader.EventDescriptor.Opcode == 10) {
            ULONG pid = pEvent->EventHeader.ProcessId;
            ULONG64 bytes = *((ULONG64*)pEvent->UserData);
            NetworkTraffic* traffic = FindOrCreateTrafficData(pid);
            if (traffic) {
                traffic->sentBytes += bytes;
            }
            wprintf(L"pid is %lu, sentBytes is %llu\n", pid, traffic->sentBytes);
        }
        else if (pEvent->EventHeader.EventDescriptor.Opcode == 11) {
            ULONG pid = pEvent->EventHeader.ProcessId;
            ULONG64 bytes = *((ULONG64*)pEvent->UserData);
            NetworkTraffic* traffic = FindOrCreateTrafficData(pid);
            if (traffic) {
                traffic->receivedBytes += bytes;
            }
            wprintf(L"pid is %lu, receivedBytes is %llu\n", pid, traffic->receivedBytes);
        }
        else
        {
            wprintf(L"nothing\n");
            //goto cleanup;
        }

    cleanup:

        if (pInfo)
        {
            free(pInfo);
        }

        if (ERROR_SUCCESS != status)
        {
            CloseTrace(g_hTrace);
        }
    }
}


NetworkTraffic* GetNetworkTrafficByPID(ULONG pid) {
    std::lock_guard<std::mutex>lock(g_print_mutex);
    for (int i = 0; i < trafficCount; i++) {
        if (trafficData[i].pid == pid) {
            return &trafficData[i];
        }
    }
    return NULL;
}

int main() {

    std::thread t(Controller);
    t.detach();
    
    auto pid = GetCurrentProcessId();
    wprintf(L"pid is %lu\n", pid);

    Consumer();

    /*std::thread c(Consumer);

    auto pid = GetCurrentProcessId();

    for (int i = 0; i < 100; i++) {
         auto proc = GetNetworkTrafficByPID(pid);
        if (proc != nullptr) {
            wprintf(L"processid is %lu,receivedBytes is %llu,sentBytes is %llu ", proc->pid, proc->receivedBytes, proc->sentBytes);
        }
        Sleep(10);

    }*/

    return 0;
}