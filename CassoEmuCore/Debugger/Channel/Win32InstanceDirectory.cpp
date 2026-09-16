#include "Pch.h"

#include "Debugger/Channel/Win32InstanceDirectory.h"

#include "Debugger/Channel/Win32PipeTransport.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Win32PipeClient
//
//  A client end of the pipe, reading with overlapped I/O so a read can give up
//  at a deadline instead of blocking until the instance answers.
//
////////////////////////////////////////////////////////////////////////////////

class Win32PipeClient : public IChannelClient
{
public:
    Win32PipeClient (HANDLE pipe, HANDLE event) :
        m_pipe  (pipe),
        m_event (event)
    {
    }

    ~Win32PipeClient() override
    {
        DWORD  transferred = 0;



        if (m_isReadPending)
        {
            CancelIoEx (m_pipe, &m_overlapped);
            GetOverlappedResult (m_pipe, &m_overlapped, &transferred, TRUE);
        }

        CloseHandle (m_pipe);
        CloseHandle (m_event);
    }

    Win32PipeClient             (const Win32PipeClient &) = delete;
    Win32PipeClient & operator= (const Win32PipeClient &) = delete;

    bool WriteLine (const std::string & line) override;
    bool ReadLine  (std::string & line, DWORD timeoutMs) override;
    bool IsClosed  () const override { return m_isClosed; }

private:
    bool TakeLine (std::string & line);

    HANDLE                  m_pipe          = INVALID_HANDLE_VALUE;
    HANDLE                  m_event         = nullptr;
    OVERLAPPED              m_overlapped    = {};
    bool                    m_isReadPending = false;
    bool                    m_isClosed      = false;
    std::array<char, 4096>  m_buffer        = {};
    std::string             m_pending;
};





////////////////////////////////////////////////////////////////////////////////
//
//  Win32PipeClient::WriteLine
//
//  Waited for in full. A request is small and the server reads every pump, so
//  a write that cannot complete means the instance has gone.
//
////////////////////////////////////////////////////////////////////////////////

bool Win32PipeClient::WriteLine (const std::string & line)
{
    std::string  data    = line + "\n";
    OVERLAPPED   overlap = {};
    DWORD        written = 0;
    BOOL         wrote   = FALSE;
    HANDLE       event   = CreateEventW (nullptr, TRUE, FALSE, nullptr);



    if (m_isClosed || event == nullptr)
    {
        if (event != nullptr)
        {
            CloseHandle (event);
        }

        return false;
    }

    overlap.hEvent = event;
    wrote          = WriteFile (m_pipe, data.data(), (DWORD) data.size(), &written, &overlap);

    if (!wrote && GetLastError() == ERROR_IO_PENDING)
    {
        wrote = GetOverlappedResult (m_pipe, &overlap, &written, TRUE);
    }

    CloseHandle (event);

    if (!wrote || written != data.size())
    {
        m_isClosed = true;
        return false;
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32PipeClient::TakeLine
//
////////////////////////////////////////////////////////////////////////////////

bool Win32PipeClient::TakeLine (std::string & line)
{
    size_t  newline = m_pending.find ('\n');



    if (newline == std::string::npos)
    {
        return false;
    }

    line = m_pending.substr (0, newline);
    m_pending.erase (0, newline + 1);

    if (!line.empty() && line.back() == '\r')
    {
        line.pop_back();
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32PipeClient::ReadLine
//
//  A read left pending by a timeout stays pending and is picked up by the next
//  call, so no bytes are lost between calls.
//
////////////////////////////////////////////////////////////////////////////////

bool Win32PipeClient::ReadLine (std::string & line, DWORD timeoutMs)
{
    ULONGLONG  deadline = GetTickCount64() + timeoutMs;
    DWORD      read     = 0;



    for (;;)
    {
        ULONGLONG  now = GetTickCount64();



        if (TakeLine (line))
        {
            return true;
        }

        if (m_isClosed)
        {
            return false;
        }

        if (!m_isReadPending)
        {
            m_overlapped        = OVERLAPPED();
            m_overlapped.hEvent = m_event;

            if (ReadFile (m_pipe, m_buffer.data(), (DWORD) m_buffer.size(), &read, &m_overlapped))
            {
                m_pending.append (m_buffer.data(), read);
                continue;
            }

            if (GetLastError() != ERROR_IO_PENDING)
            {
                m_isClosed = true;
                return false;
            }

            m_isReadPending = true;
        }

        if (WaitForSingleObject (m_event, (now >= deadline) ? 0 : (DWORD) (deadline - now)) != WAIT_OBJECT_0)
        {
            return false;
        }

        m_isReadPending = false;

        if (!GetOverlappedResult (m_pipe, &m_overlapped, &read, FALSE))
        {
            m_isClosed = true;
            return false;
        }

        m_pending.append (m_buffer.data(), read);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32InstanceDirectory::TryParsePipeName
//
////////////////////////////////////////////////////////////////////////////////

bool Win32InstanceDirectory::TryParsePipeName (const std::wstring & name, uint32_t & processId)
{
    std::wstring  prefix (kPipePrefix);
    uint64_t      value  = 0;



    if (name.size() <= prefix.size() || name.compare (0, prefix.size(), prefix) != 0)
    {
        return false;
    }

    for (size_t i = prefix.size(); i < name.size(); i++)
    {
        if (name[i] < L'0' || name[i] > L'9')
        {
            return false;
        }

        value = value * 10 + (uint64_t) (name[i] - L'0');

        if (value > UINT32_MAX)
        {
            return false;
        }
    }

    processId = (uint32_t) value;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32InstanceDirectory::ListProcessIds
//
////////////////////////////////////////////////////////////////////////////////

std::vector<uint32_t> Win32InstanceDirectory::ListProcessIds()
{
    std::vector<uint32_t>  ids;
    WIN32_FIND_DATAW       found     = {};
    HANDLE                 search    = FindFirstFileW (L"\\\\.\\pipe\\*", &found);
    uint32_t               processId = 0;



    if (search == INVALID_HANDLE_VALUE)
    {
        return ids;
    }

    do
    {
        if (TryParsePipeName (found.cFileName, processId))
        {
            ids.push_back (processId);
        }
    }
    while (FindNextFileW (search, &found));

    FindClose (search);
    return ids;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32InstanceDirectory::Connect
//
////////////////////////////////////////////////////////////////////////////////

std::unique_ptr<IChannelClient> Win32InstanceDirectory::Connect (uint32_t processId)
{
    std::wstring  name  = Win32PipeTransport::GetPipeName (processId);
    HANDLE        pipe  = CreateFileW (name.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr);
    HANDLE        event = nullptr;



    if (pipe == INVALID_HANDLE_VALUE)
    {
        return nullptr;
    }

    event = CreateEventW (nullptr, TRUE, FALSE, nullptr);

    if (event == nullptr)
    {
        CloseHandle (pipe);
        return nullptr;
    }

    return std::make_unique<Win32PipeClient> (pipe, event);
}
