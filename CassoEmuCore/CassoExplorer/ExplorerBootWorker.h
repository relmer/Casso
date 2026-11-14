#pragma once

#include "Pch.h"

#include "DiskCheck/BootCheck.h"
#include "DiskCheck/ThumbnailStore.h"

class IDiskFileIo;





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerBootWorker
//
//  A thread that boots disk images one at a time, with no window, and
//  stores each one's thumbnail and boot result: a Full Send and Capture raw
//  disk queue each image they write, and a list queues each image in view
//  that has no thumbnail. An image waits while the busy function is true,
//  as a boot takes a core that a link reading a disk is using.
//
//  A result is written to the thumbnail store, to the row of the image in
//  its folder's Full Send manifest when it has one, and then passed to the
//  done function, on the worker's thread.
//
////////////////////////////////////////////////////////////////////////////////

class ExplorerBootWorker
{
public:
    struct Result
    {
        std::wstring              path;         // the image, under its new name when the boot renamed it
        std::string               renamedFrom;  // the file's name before, when the boot renamed it
        std::string               source;       // as given to Queue
        bool                      hasReport  = false;
        bool                      isComplete = true;    // false for a Full Send image with unreadable sectors
        BootVerdict               verdict    = BootVerdict::Unclear;
        std::string               reason;
        std::string               machine;
        std::vector<std::string>  missing;      // each machine that couldn't be built, with why
    };

    using DoneFn = std::function<void (const Result &)>;
    using BusyFn = std::function<bool()>;

    ExplorerBootWorker (ThumbnailStore & store, BootCheck::MachineFn build);
    ~ExplorerBootWorker ();

    ExplorerBootWorker (const ExplorerBootWorker &)             = delete;
    ExplorerBootWorker & operator= (const ExplorerBootWorker &) = delete;

    void    SetDone       (DoneFn done);
    void    SetBusy       (BusyFn isBusy);
    void    SetManifestIo (IDiskFileIo * fileIo);
    void    SetLimits     (const BootProbe::Limits & limits);

    //  An image by its path, and the model of Apple it was captured from,
    //  empty when that isn't known. The source is returned with the result,
    //  as the connection the image came through. A path already waiting
    //  isn't added.
    void    Queue         (const std::wstring & path, const std::string & model, const std::string & source = std::string());

    size_t  GetPending    () const;

    //  Returns once nothing is waiting or being booted.
    void    WaitUntilIdle ();

    //  One image: read, booted, stored. The thread's body, and a test's.
    Result  Check         (const std::wstring & path, const std::string & model);

    //  A numbered image's name with its title after the number, as
    //  BootTitle::ToName has it: 0019 Archon.woz from 0019.woz; empty for a name that isn't
    //  digits alone, or a title with nothing a file name can have.
    static std::string  MakeTitledName (const std::string & name, const std::string & title);

    //  A result as a line for an activity list.
    static std::string  Describe (const Result & result);

private:
    struct Item
    {
        std::wstring  path;
        std::string   model;
        std::string   source;
    };

    void  Run ();

    static constexpr DWORD   s_kBusyPollMs    = 250;
    static constexpr size_t  s_kMaxTitleChars = 60;

    ThumbnailStore           & m_store;
    BootCheck::MachineFn       m_build;
    DoneFn                     m_done;
    BusyFn                     m_isBusy;
    IDiskFileIo              * m_fileIo  = nullptr;
    BootProbe::Limits          m_limits;
    mutable std::mutex         m_lock;
    std::condition_variable    m_wake;
    std::condition_variable    m_idle;
    std::deque<Item>           m_queue;
    bool                       m_working = false;
    bool                       m_stop    = false;
    std::thread                m_thread;
};
