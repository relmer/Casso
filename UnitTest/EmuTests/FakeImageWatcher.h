#pragma once

#include "Pch.h"
#include "Devices/Disk/IImageWatcher.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FakeImageWatcher
//
//  A watcher a test drives by hand.
//
//  IT EXISTS SO A CHANGE COSTS NO FILE AND NO WAIT. Everything above the seam
//  -- which bay a path belongs to, whether a burst is one change, what to do
//  about it -- is exercised by calling Fire, in memory, instantly.
//
//  `failWatch` MODELS A LOCATION THAT CANNOT BE WATCHED, which is a state to
//  degrade into rather than an error, and the only way to reach the degraded
//  path in a test.
//
////////////////////////////////////////////////////////////////////////////////

class FakeImageWatcher : public IImageWatcher
{
public:

    //  Directories currently watched, in the order they were taken up.
    std::vector<std::string>                            watched;

    //  Directories Unwatch was called for, so a test can assert the lifecycle
    //  rather than only the end state.
    std::vector<std::string>                            unwatched;

    //  Refuse every watch, as an unwatchable share does.
    bool                                                failWatch = false;

    std::unordered_map<std::string, Callback>           callbacks;



    //  A DIRECTORY ALREADY WATCHED SUCCEEDS WITHOUT A SECOND ENTRY, exactly as
    //  the platform watcher does. Two disks out of one folder is ordinary, and
    //  it is one watch; a fake that counted it twice would let a leak past.
    bool  Watch (const std::string & directory, Callback callback) override
    {
        bool  taken = !failWatch;



        if (taken && callbacks.find (directory) == callbacks.end())
        {
            watched.push_back (directory);
        }

        if (taken)
        {
            callbacks[directory] = std::move (callback);
        }

        return taken;
    }



    void  Unwatch (const std::string & directory) override
    {
        unwatched.push_back (directory);
        callbacks.erase (directory);

        watched.erase (std::remove (watched.begin(), watched.end(), directory),
                       watched.end());

        return;
    }



    //  Report that `path` changed, to whichever watch covers its directory.
    //
    //  A PATH NOBODY WATCHES FIRES NOTHING and is not an error: that is what a
    //  real watcher does, and a test asserting the store ignores an unrelated
    //  file needs to be able to say so.
    void  Fire (const std::string & directory, const std::string & path)
    {
        auto  found = callbacks.find (directory);



        if (found != callbacks.end() && found->second)
        {
            found->second (path);
        }

        return;
    }
};
