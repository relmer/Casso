#pragma once





////////////////////////////////////////////////////////////////////////////////
//
//  INamedPipeApi
//
//  Every Win32 call the pipe transport makes, and nothing else.
//
//  THIS IS WHAT MAKES THE TRANSPORT TESTABLE WITHOUT A PIPE. Overlapped I/O has
//  more failure paths than success paths -- a read that stays pending, a client
//  that leaves in the middle of one, a handle that fails to open halfway through
//  setting up an instance -- and each has to release what was already acquired.
//  None of those can be produced on demand against a real pipe, so the
//  transport reaches Windows only through here and a test double stands in.
//
//  The methods are named for what they do rather than after the Win32
//  functions, because several of those names are macros in the Windows headers
//  and a method sharing one would be renamed by the preprocessor.
//
////////////////////////////////////////////////////////////////////////////////

class INamedPipeApi
{
public:
    virtual ~INamedPipeApi() = default;

    virtual HANDLE  CreatePipeInstance (const std::wstring   & name,
                                        DWORD                  openMode,
                                        DWORD                  pipeMode,
                                        DWORD                  maxInstances,
                                        DWORD                  outBufferSize,
                                        DWORD                  inBufferSize,
                                        SECURITY_ATTRIBUTES  * security) = 0;

    virtual BOOL    ConnectPipe        (HANDLE pipe, OVERLAPPED * overlapped) = 0;
    virtual BOOL    ReadPipe           (HANDLE pipe, void * buffer, DWORD size, DWORD * read, OVERLAPPED * overlapped) = 0;
    virtual BOOL    WritePipe          (HANDLE pipe, const void * data, DWORD size, DWORD * written, OVERLAPPED * overlapped) = 0;
    virtual BOOL    GetResult          (HANDLE pipe, OVERLAPPED * overlapped, DWORD * transferred, BOOL wait) = 0;
    virtual BOOL    CancelIo           (HANDLE pipe, OVERLAPPED * overlapped) = 0;
    virtual BOOL    DisconnectPipe     (HANDLE pipe) = 0;
    virtual BOOL    CloseHandleOf      (HANDLE handle) = 0;
    virtual HANDLE  CreateEventHandle  (BOOL manualReset, BOOL initialState) = 0;
    virtual DWORD   WaitForHandle      (HANDLE handle, DWORD milliseconds) = 0;
    virtual DWORD   GetLastErrorCode   () = 0;
};
