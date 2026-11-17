#pragma once

#include "Pch.h"

class IDiskFileIo;





////////////////////////////////////////////////////////////////////////////////
//
//  VolumeFacts
//
//  An image's file system and volume name, as the File system and Volume
//  name columns list them; both empty for an image whose volume can't be
//  read.
//
////////////////////////////////////////////////////////////////////////////////

struct VolumeFacts
{
    std::wstring  fileSystem;
    std::wstring  volumeName;

    bool  operator== (const VolumeFacts &) const = default;
};





////////////////////////////////////////////////////////////////////////////////
//
//  ImageVolumeFacts
//
//  Reads images' file systems and volume names on a thread of its own, since
//  each image is read whole and decoded. A request replaces the paths not
//  yet read, so the work stops when the list moves to another folder.
//  Results are kept by path, length and modified time, and the notify
//  function is called on the worker's thread after each one.
//
////////////////////////////////////////////////////////////////////////////////

class ImageVolumeFacts
{
public:
    using Notify = std::function<void ()>;
    using Reader = std::function<VolumeFacts (const std::string & path)>;

    struct Request
    {
        std::wstring  path;
        uint64_t      sizeBytes    = 0;
        int64_t       modifiedUnix = 0;
    };

    ImageVolumeFacts (IDiskFileIo & fileIo, Notify notify, Reader read = nullptr);
    ~ImageVolumeFacts ();

    ImageVolumeFacts (const ImageVolumeFacts &)             = delete;
    ImageVolumeFacts & operator= (const ImageVolumeFacts &) = delete;

    //  The facts of a file with that length and time, when they have been read.
    bool  TryGet   (const std::wstring & path, uint64_t sizeBytes, int64_t modifiedUnix, VolumeFacts & outFacts);

    //  Queues the files not read yet, in place of any still queued.
    void  Queue    (const std::vector<Request> & requests);
    void  Cancel   ();

    //  An image's facts, read on the calling thread.
    static VolumeFacts  Read (IDiskFileIo & fileIo, const std::string & path);

private:
    struct Entry
    {
        uint64_t     sizeBytes    = 0;
        int64_t      modifiedUnix = 0;
        VolumeFacts  facts;
    };

    void  Run ();

    IDiskFileIo                             & m_fileIo;
    Notify                                    m_notify;
    Reader                                    m_read;
    std::thread                               m_thread;
    std::mutex                                m_lock;
    std::condition_variable                   m_wake;
    std::deque<Request>                       m_queue;
    std::unordered_map<std::wstring, Entry>   m_entries;
    bool                                      m_isStopping = false;
};
