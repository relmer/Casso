#pragma once

#include "Pch.h"

#include "Config/IFileSystem.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BackgroundFileWriter
//
//  Writes files on a background thread, so a save made on the UI thread
//  doesn't wait for the disk. A write to a path whose earlier write hasn't
//  started replaces it: only the newest content of each file is written.
//
//  Flush, or destroying the writer, waits for every queued write. A
//  synchronous write to the same file through the file system must come
//  after a Flush, or a queued write can land after it.
//
////////////////////////////////////////////////////////////////////////////////

class BackgroundFileWriter
{
public:
    explicit BackgroundFileWriter (IFileSystem & fs);
    ~BackgroundFileWriter();

    BackgroundFileWriter (const BackgroundFileWriter &)             = delete;
    BackgroundFileWriter & operator= (const BackgroundFileWriter &) = delete;

    void  Write (const std::wstring & path, std::string content);
    void  Flush ();

private:
    void  Run ();

    IFileSystem                          & m_fs;
    std::mutex                             m_lock;
    std::condition_variable                m_changed;
    std::map<std::wstring, std::string>    m_pending;
    bool                                   m_writing  = false;
    bool                                   m_stopping = false;
    std::thread                            m_thread;
};
