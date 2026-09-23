#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IFolderWatcher
//
//  Being told that something in a folder changed, without knowing how the
//  platform says so.
//
//  IT REPORTS THE FOLDER, NOT THE FILE, and that is the whole difference from
//  IImageWatcher. The disk layer needs the file's name, because it maps the
//  change onto a mounted image; a browser re-reads the folder either way, so
//  every per-file record it is handed is discarded. Saying only what is
//  actually used lets the platform half be built on the cheaper call.
//
//  A FOLDER THAT CANNOT BE WATCHED IS NOT AN ERROR. A share that refuses is a
//  state to degrade into: what is shown was true when it was read, and the
//  next navigation reads it again.
//
////////////////////////////////////////////////////////////////////////////////

class IFolderWatcher
{
public:

    //  Something under `directory` changed. Called from whatever thread the
    //  implementation uses, so a handler does no work beyond recording.
    using Callback = std::function<void (const std::wstring & directory)>;

    virtual ~IFolderWatcher () = default;

    //  Begin reporting changes in this folder. False where it cannot be
    //  watched, including where the implementation is already watching as
    //  many as it can.
    virtual bool  Watch (const std::wstring & directory, Callback callback) = 0;

    //  Stop reporting. Safe for a folder that was never watched.
    virtual void  Unwatch (const std::wstring & directory) = 0;
};
