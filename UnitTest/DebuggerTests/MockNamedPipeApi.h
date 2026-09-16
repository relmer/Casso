#pragma once

#include "Debugger/Channel/INamedPipeApi.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MockNamedPipeApi
//
//  A named pipe in memory, driven from the client side by the test.
//
//  Handles are counters, and every one handed out is tracked until it is
//  closed, so a test can assert that a failure left nothing open. Any call can
//  be made to fail on its nth use with a chosen error. Reads go pending when a
//  client has sent nothing and complete when it does, which is what lets the
//  transport's polling be exercised without a thread.
//
////////////////////////////////////////////////////////////////////////////////

class MockNamedPipeApi : public INamedPipeApi
{
public:
    struct CreatedInstance
    {
        std::wstring  name;
        DWORD         openMode      = 0;
        DWORD         pipeMode      = 0;
        DWORD         maxInstances  = 0;
        DWORD         aceCount      = 0;
        bool          isAllowForSid = false;
    };

    std::vector<CreatedInstance>  created;
    std::vector<HANDLE>           pipes;
    std::set<HANDLE>              open;
    int                           doubleCloses = 0;
    std::vector<BYTE>             expectedSid;

    //  When set, writes go pending and never complete.
    bool                          stallWrites = false;



    //  Makes the nth call (1-based) to `call` fail with `error`.
    void FailOn (const std::string & call, int nth, DWORD error)
    {
        m_failures[call] = { nth, error };
    }



    //  The client end of the instance the transport created `index`th.
    void ClientConnect (size_t index)
    {
        Pipe & pipe = m_pipes[pipes.at (index)];

        pipe.isClientConnected = true;

        if (pipe.connectOverlapped != nullptr)
        {
            Signal (pipe.connectOverlapped->hEvent);
        }
    }



    void ClientSend (size_t index, const std::string & bytes)
    {
        Pipe &  pipe  = m_pipes[pipes.at (index)];
        size_t  taken = 0;



        if (pipe.pendingRead != nullptr)
        {
            taken = std::min<size_t> (bytes.size(), pipe.pendingSize);
            memcpy (pipe.pendingBuffer, bytes.data(), taken);

            pipe.lastTransfer = (DWORD) taken;
            Signal (pipe.pendingRead->hEvent);
            pipe.pendingRead = nullptr;
        }

        if (taken < bytes.size())
        {
            pipe.incoming.append (bytes, taken, std::string::npos);
        }
    }



    void ClientDisconnect (size_t index)
    {
        Pipe & pipe = m_pipes[pipes.at (index)];

        pipe.isBroken = true;

        if (pipe.pendingRead != nullptr)
        {
            Signal (pipe.pendingRead->hEvent);
        }
    }



    std::string Received (size_t index)
    {
        return m_pipes[pipes.at (index)].written;
    }



    // INamedPipeApi

    HANDLE CreatePipeInstance (const std::wstring & name, DWORD openMode, DWORD pipeMode, DWORD maxInstances,
                               DWORD, DWORD, SECURITY_ATTRIBUTES * security) override
    {
        CreatedInstance  instance;
        HANDLE           handle = nullptr;



        if (ShouldFail ("CreatePipeInstance"))
        {
            return INVALID_HANDLE_VALUE;
        }

        instance.name         = name;
        instance.openMode     = openMode;
        instance.pipeMode     = pipeMode;
        instance.maxInstances = maxInstances;
        InspectSecurity (security, instance);
        created.push_back (instance);

        handle = Allocate();
        pipes.push_back (handle);
        m_pipes[handle] = Pipe();

        return handle;
    }



    BOOL ConnectPipe (HANDLE pipe, OVERLAPPED * overlapped) override
    {
        if (ShouldFail ("ConnectPipe"))
        {
            return FALSE;
        }

        if (m_pipes[pipe].isClientConnected)
        {
            m_lastError = ERROR_PIPE_CONNECTED;
            return FALSE;
        }

        m_pipes[pipe].connectOverlapped = overlapped;
        m_lastError = ERROR_IO_PENDING;
        return FALSE;
    }



    BOOL ReadPipe (HANDLE handle, void * buffer, DWORD size, DWORD * read, OVERLAPPED * overlapped) override
    {
        Pipe &  pipe  = m_pipes[handle];
        size_t  taken = 0;



        if (ShouldFail ("ReadPipe"))
        {
            return FALSE;
        }

        if (!pipe.incoming.empty())
        {
            taken = std::min<size_t> (pipe.incoming.size(), size);
            memcpy (buffer, pipe.incoming.data(), taken);
            pipe.incoming.erase (0, taken);

            *read = (DWORD) taken;
            Signal (overlapped->hEvent);
            return TRUE;
        }

        if (pipe.isBroken)
        {
            m_lastError = ERROR_BROKEN_PIPE;
            return FALSE;
        }

        pipe.pendingRead   = overlapped;
        pipe.pendingBuffer = (char *) buffer;
        pipe.pendingSize   = size;
        Unsignal (overlapped->hEvent);

        m_lastError = ERROR_IO_PENDING;
        return FALSE;
    }



    BOOL WritePipe (HANDLE handle, const void * data, DWORD size, DWORD * written, OVERLAPPED * overlapped) override
    {
        Pipe & pipe = m_pipes[handle];



        if (ShouldFail ("WritePipe"))
        {
            return FALSE;
        }

        if (pipe.isBroken)
        {
            m_lastError = ERROR_NO_DATA;
            return FALSE;
        }

        if (stallWrites)
        {
            Unsignal (overlapped->hEvent);
            m_lastError = ERROR_IO_PENDING;
            return FALSE;
        }

        pipe.written.append ((const char *) data, size);
        *written = size;
        return TRUE;
    }



    BOOL GetResult (HANDLE handle, OVERLAPPED * overlapped, DWORD * transferred, BOOL) override
    {
        Pipe & pipe = m_pipes[handle];



        if (ShouldFail ("GetResult"))
        {
            return FALSE;
        }

        if (pipe.cancelled == overlapped)
        {
            pipe.cancelled = nullptr;
            m_lastError    = ERROR_OPERATION_ABORTED;
            return FALSE;
        }

        //  A read that completed before the client left still delivers its
        //  bytes; only a read with nothing left to report sees the break.
        if (pipe.isBroken && pipe.lastTransfer == 0)
        {
            m_lastError = ERROR_BROKEN_PIPE;
            return FALSE;
        }

        *transferred      = pipe.lastTransfer;
        pipe.lastTransfer = 0;
        return TRUE;
    }



    BOOL CancelIo (HANDLE handle, OVERLAPPED * overlapped) override
    {
        Pipe & pipe = m_pipes[handle];

        pipe.cancelled   = overlapped;
        pipe.pendingRead = nullptr;
        return TRUE;
    }



    BOOL DisconnectPipe (HANDLE) override
    {
        return TRUE;
    }



    BOOL CloseHandleOf (HANDLE handle) override
    {
        if (open.erase (handle) == 0)
        {
            doubleCloses++;
        }

        return TRUE;
    }



    HANDLE CreateEventHandle (BOOL, BOOL initialState) override
    {
        HANDLE handle = nullptr;



        if (ShouldFail ("CreateEventHandle"))
        {
            return nullptr;
        }

        handle           = Allocate();
        m_events[handle] = initialState != FALSE;
        return handle;
    }



    DWORD WaitForHandle (HANDLE handle, DWORD) override
    {
        return m_events[handle] ? WAIT_OBJECT_0 : WAIT_TIMEOUT;
    }



    DWORD GetLastErrorCode() override
    {
        return m_lastError;
    }

private:
    struct Pipe
    {
        bool          isClientConnected = false;
        bool          isBroken          = false;
        OVERLAPPED  * connectOverlapped = nullptr;
        OVERLAPPED  * pendingRead       = nullptr;
        OVERLAPPED  * cancelled         = nullptr;
        char        * pendingBuffer     = nullptr;
        DWORD         pendingSize       = 0;
        DWORD         lastTransfer      = 0;
        std::string   incoming;
        std::string   written;
    };

    struct Failure
    {
        int    nth   = 0;
        DWORD  error = 0;
    };

    HANDLE Allocate()
    {
        HANDLE handle = (HANDLE) (INT_PTR) m_nextHandle;

        m_nextHandle += 4;
        open.insert (handle);
        return handle;
    }

    bool ShouldFail (const std::string & call)
    {
        int    count = ++m_calls[call];
        auto   found = m_failures.find (call);



        if (found != m_failures.end() && found->second.nth == count)
        {
            m_lastError = found->second.error;
            return true;
        }

        return false;
    }

    void Signal   (HANDLE event) { m_events[event] = true; }
    void Unsignal (HANDLE event) { m_events[event] = false; }

    //  Reads the access list out of what the transport passed, at the moment
    //  it passed it, so a test can check who the pipe admits.
    void InspectSecurity (SECURITY_ATTRIBUTES * security, CreatedInstance & instance)
    {
        BOOL                  present   = FALSE;
        BOOL                  defaulted = FALSE;
        PACL                  dacl      = nullptr;
        ACL_SIZE_INFORMATION  info      = {};
        void                * ace       = nullptr;



        if (security == nullptr || security->lpSecurityDescriptor == nullptr)
        {
            return;
        }

        if (!GetSecurityDescriptorDacl (security->lpSecurityDescriptor, &present, &dacl, &defaulted) || !present || dacl == nullptr)
        {
            return;
        }

        if (!GetAclInformation (dacl, &info, sizeof (info), AclSizeInformation))
        {
            return;
        }

        instance.aceCount = info.AceCount;

        if (info.AceCount == 1 && GetAce (dacl, 0, &ace))
        {
            ACCESS_ALLOWED_ACE * allow = (ACCESS_ALLOWED_ACE *) ace;

            instance.isAllowForSid = allow->Header.AceType == ACCESS_ALLOWED_ACE_TYPE &&
                                     !expectedSid.empty() &&
                                     EqualSid ((PSID) &allow->SidStart, (PSID) expectedSid.data());
        }
    }

    std::map<HANDLE, Pipe>          m_pipes;
    std::map<HANDLE, bool>          m_events;
    std::map<std::string, int>      m_calls;
    std::map<std::string, Failure>  m_failures;
    DWORD                           m_lastError  = ERROR_SUCCESS;
    INT_PTR                         m_nextHandle = 0x1000;
};
