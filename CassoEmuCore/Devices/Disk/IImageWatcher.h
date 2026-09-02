#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IImageWatcher
//
//  Being told that a file under a directory changed, without knowing how the
//  platform says so.
//
//  THE SEAM EXISTS FOR THE SAME REASON IDiskFileIo DOES. A watcher built
//  directly on the platform call is a watcher no test can drive: every rule
//  above it -- which bay a path belongs to, whether a burst is one change,
//  what to do about it -- would need a real file, a real directory and a real
//  wait to exercise. Behind this, all of that runs in memory and finishes
//  instantly.
//
//  IT WATCHES A DIRECTORY, NOT A FILE, and that is not an implementation
//  detail. Both writers in this system commit by renaming a temporary over the
//  target, so a handle held on the image itself sees its own replacement as a
//  deletion and stops watching exactly when it matters most.
//
////////////////////////////////////////////////////////////////////////////////

class IImageWatcher
{
public:
    //  A path under a watched directory changed. Called from whatever thread
    //  the implementation uses, so a handler does no work beyond recording.
    using Callback = std::function<void (const std::string & path)>;

    virtual ~IImageWatcher () = default;

    //  Begin reporting changes under this directory. False where the location
    //  cannot be watched at all, which is a state to degrade into rather than
    //  an error: the check made before every write is what carries the
    //  guarantee, and notification only makes it prompt.
    virtual bool  Watch     (const std::string & directory, Callback callback) = 0;

    //  Stop reporting. Safe to call for a directory that was never watched.
    virtual void  Unwatch   (const std::string & directory) = 0;
};
