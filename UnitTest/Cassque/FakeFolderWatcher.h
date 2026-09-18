#pragma once

#include "Pch.h"
#include "Cassque/Model/IFolderWatcher.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FakeFolderWatcher
//
//  A folder watcher a test drives by hand.
//
//  IT EXISTS SO A CHANGE COSTS NO FILE AND NO WAIT. Everything above the seam
//  -- which folders are worth watching, whether a burst is one change, what to
//  do about it -- is exercised by calling Fire, in memory, instantly.
//
//  `failWatch` MODELS A FOLDER THAT CANNOT BE WATCHED, which is a state to
//  degrade into rather than an error, and the only way to reach that path in
//  a test.
//
////////////////////////////////////////////////////////////////////////////////

class FakeFolderWatcher : public IFolderWatcher
{
public:

    //  Folders currently watched, in the order they were added.
    std::vector<std::wstring>                    watched;

    //  Folders Unwatch was called for, so a test can assert the lifecycle
    //  rather than only the end state.
    std::vector<std::wstring>                    unwatched;

    //  Refuse every watch, as an unwatchable share does.
    bool                                         failWatch = false;

    std::map<std::wstring, Callback>             callbacks;



    bool  Watch (const std::wstring & directory, Callback callback) override
    {
        if (failWatch)
        {
            return false;
        }

        if (callbacks.find (directory) == callbacks.end())
        {
            watched.push_back (directory);
        }

        callbacks[directory] = std::move (callback);

        return true;
    }



    void  Unwatch (const std::wstring & directory) override
    {
        unwatched.push_back (directory);
        callbacks.erase (directory);

        watched.erase (std::remove (watched.begin(), watched.end(), directory), watched.end());
    }



    //  Report that something in `directory` changed. A folder nobody watches
    //  fires nothing, which is what a real watcher does.
    void  Fire (const std::wstring & directory)
    {
        auto  found = callbacks.find (directory);

        if (found != callbacks.end() && found->second)
        {
            found->second (directory);
        }
    }
};
