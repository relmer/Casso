#pragma once

#include "Pch.h"

#include "Devices/Disk/ImageClassifier.h"

class IDiskFileIo;





////////////////////////////////////////////////////////////////////////////////
//
//  ImageClassCache
//
//  Each file's class, by its path, its length and its modified time, so a
//  folder listed again reads only the files that changed. Safe to call from
//  more than one thread.
//
////////////////////////////////////////////////////////////////////////////////

class ImageClassCache
{
public:
    //  The class of the file with that length and time: the one stored, or
    //  one read with ImageClassifier::Classify and stored.
    ImageClass  Get   (IDiskFileIo & fileIo, const std::string & path, uint64_t sizeBytes, int64_t modifiedUnix);

    size_t      GetCount () const;
    void        Clear    ();

private:
    struct Entry
    {
        uint64_t    sizeBytes    = 0;
        int64_t     modifiedUnix = 0;
        ImageClass  imageClass;
    };

    mutable std::mutex                       m_lock;
    std::unordered_map<std::string, Entry>   m_entries;
};
