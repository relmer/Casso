#include "Pch.h"

#include "Config/BackgroundFileWriter.h"
#include "Core/ThreadName.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BackgroundFileWriter::BackgroundFileWriter
//
////////////////////////////////////////////////////////////////////////////////

BackgroundFileWriter::BackgroundFileWriter (IFileSystem & fs)
    : m_fs (fs)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  BackgroundFileWriter::~BackgroundFileWriter
//
//  The thread writes what is queued before it ends.
//
////////////////////////////////////////////////////////////////////////////////

BackgroundFileWriter::~BackgroundFileWriter()
{
    {
        std::lock_guard<std::mutex>  guard (m_lock);

        m_stopping = true;
    }

    m_changed.notify_all();

    if (m_thread.joinable())
    {
        m_thread.join();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BackgroundFileWriter::Write
//
//  The thread starts with the first write.
//
////////////////////////////////////////////////////////////////////////////////

void BackgroundFileWriter::Write (const std::wstring & path, std::string content)
{
    {
        std::lock_guard<std::mutex>  guard (m_lock);

        m_pending[path] = std::move (content);

        if (!m_thread.joinable())
        {
            m_thread = std::thread ([this] { HRESULT hrName = ThreadName::SetForCurrentThread (L"Casso file writer"); IGNORE_RETURN_VALUE (hrName, S_OK); Run(); });
        }
    }

    m_changed.notify_all();
}





////////////////////////////////////////////////////////////////////////////////
//
//  BackgroundFileWriter::Flush
//
////////////////////////////////////////////////////////////////////////////////

void BackgroundFileWriter::Flush()
{
    std::unique_lock<std::mutex>  guard (m_lock);



    m_changed.wait (guard, [this] { return !m_thread.joinable() || (m_pending.empty() && !m_writing); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  BackgroundFileWriter::Run
//
////////////////////////////////////////////////////////////////////////////////

void BackgroundFileWriter::Run()
{
    for (;;)
    {
        std::map<std::wstring, std::string>  writes;
        HRESULT                              hr = S_OK;

        {
            std::unique_lock<std::mutex>  guard (m_lock);

            m_writing = false;
            m_changed.notify_all();
            m_changed.wait (guard, [this] { return m_stopping || !m_pending.empty(); });

            if (m_pending.empty())
            {
                break;
            }

            writes.swap (m_pending);
            m_writing = true;
        }

        for (const auto & [path, content] : writes)
        {
            hr = m_fs.WriteAllText (path, content);
            IGNORE_RETURN_VALUE (hr, S_OK);
        }
    }
}
