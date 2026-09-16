#pragma once

#include "Debugger/Channel/INamedPipeApi.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Win32NamedPipeApi
//
//  INamedPipeApi straight through to Windows. It holds no state and makes no
//  decisions, so there is nothing here a test could find wrong that the
//  transport's tests do not already cover.
//
////////////////////////////////////////////////////////////////////////////////

class Win32NamedPipeApi : public INamedPipeApi
{
public:
    HANDLE  CreatePipeInstance (const std::wstring   & name,
                                DWORD                  openMode,
                                DWORD                  pipeMode,
                                DWORD                  maxInstances,
                                DWORD                  outBufferSize,
                                DWORD                  inBufferSize,
                                SECURITY_ATTRIBUTES  * security) override;

    BOOL    ConnectPipe        (HANDLE pipe, OVERLAPPED * overlapped) override;
    BOOL    ReadPipe           (HANDLE pipe, void * buffer, DWORD size, DWORD * read, OVERLAPPED * overlapped) override;
    BOOL    WritePipe          (HANDLE pipe, const void * data, DWORD size, DWORD * written, OVERLAPPED * overlapped) override;
    BOOL    GetResult          (HANDLE pipe, OVERLAPPED * overlapped, DWORD * transferred, BOOL wait) override;
    BOOL    CancelIo           (HANDLE pipe, OVERLAPPED * overlapped) override;
    BOOL    DisconnectPipe     (HANDLE pipe) override;
    BOOL    CloseHandleOf      (HANDLE handle) override;
    HANDLE  CreateEventHandle  (BOOL manualReset, BOOL initialState) override;
    DWORD   WaitForHandle      (HANDLE handle, DWORD milliseconds) override;
    DWORD   GetLastErrorCode   () override;
};
