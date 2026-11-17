#include "Pch.h"

#include "Devices/Disk/IDiskFileIo.h"
#include "Devices/Disk/ImageClassCache.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ImageClassCache::Get
//
//  The file is read outside the lock, so a slow file holds up no other
//  thread's lookup. Two threads that miss the same file both read it, and
//  the second result replaces the first, which is the same.
//
////////////////////////////////////////////////////////////////////////////////

ImageClass ImageClassCache::Get (IDiskFileIo & fileIo, const std::string & path, uint64_t sizeBytes, int64_t modifiedUnix)
{
    ImageClass  result;
    bool        isHit = false;



    {
        std::lock_guard<std::mutex>  guard (m_lock);
        auto                         found = m_entries.find (path);



        isHit = found != m_entries.end() && found->second.sizeBytes == sizeBytes && found->second.modifiedUnix == modifiedUnix;

        if (isHit)
        {
            result = found->second.imageClass;
        }
    }

    if (isHit)
    {
        return result;
    }

    result = ImageClassifier::Classify (fileIo, path);

    {
        std::lock_guard<std::mutex>  guard (m_lock);



        m_entries[path] = Entry { sizeBytes, modifiedUnix, result };
    }

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ImageClassCache::GetCount
//
////////////////////////////////////////////////////////////////////////////////

size_t ImageClassCache::GetCount() const
{
    std::lock_guard<std::mutex>  guard (m_lock);



    return m_entries.size();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ImageClassCache::Clear
//
////////////////////////////////////////////////////////////////////////////////

void ImageClassCache::Clear()
{
    std::lock_guard<std::mutex>  guard (m_lock);



    m_entries.clear();
}
