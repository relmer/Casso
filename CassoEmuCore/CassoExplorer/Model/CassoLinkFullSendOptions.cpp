#include "Pch.h"

#include "CassoExplorer/Model/CassoLinkFullSendOptions.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkFullSendOptions::SetTarget
//
//  Where a Full Send started from an Apple writes: the folder, made by
//  makeFolder when the session starts, and the extension with its period,
//  empty for .po on ProDOS and .dsk otherwise.
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkFullSendOptions::SetTarget (const std::string & folder, const std::string & extension, MakeFolderFn makeFolder)
{
    std::lock_guard<std::mutex>  lock (m_lock);



    m_folder     = folder;
    m_extension  = extension;
    m_makeFolder = std::move (makeFolder);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkFullSendOptions::TryGetTarget
//
//  False with no folder set, or when the folder can't be made.
//
////////////////////////////////////////////////////////////////////////////////

bool CassoLinkFullSendOptions::TryGetTarget (std::string & outFolder, std::string & outExtension)
{
    std::lock_guard<std::mutex>  lock (m_lock);
    bool                         ready = !m_folder.empty();
    HRESULT                      made  = S_OK;



    if (ready && m_makeFolder)
    {
        made  = m_makeFolder (m_folder);
        ready = SUCCEEDED (made);
    }

    outFolder    = m_folder;
    outExtension = m_extension;

    return ready;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkFullSendOptions::SetProtection
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkFullSendOptions::SetProtection (const FullSendProtectionChoices & choices)
{
    std::lock_guard<std::mutex>  lock (m_lock);



    m_protection = choices;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkFullSendOptions::GetProtection
//
////////////////////////////////////////////////////////////////////////////////

FullSendProtectionChoices CassoLinkFullSendOptions::GetProtection() const
{
    std::lock_guard<std::mutex>  lock (m_lock);



    return m_protection;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkFullSendOptions::SetSavingReads
//
//  Whether a Full Send saves a raw capture's reads beside its image.
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkFullSendOptions::SetSavingReads (bool on)
{
    std::lock_guard<std::mutex>  lock (m_lock);



    m_savingReads = on;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkFullSendOptions::IsSavingReads
//
////////////////////////////////////////////////////////////////////////////////

bool CassoLinkFullSendOptions::IsSavingReads() const
{
    std::lock_guard<std::mutex>  lock (m_lock);



    return m_savingReads;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkFullSendOptions::SetImageHandler
//
//  Called for each disk image a connection writes, a Full Send's or a raw
//  capture's, on the connection's thread: the connection, the image's path
//  and the model of the Apple.
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkFullSendOptions::SetImageHandler (ImageFn handler)
{
    std::lock_guard<std::mutex>  lock (m_lock);



    m_imageHandler = std::move (handler);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkFullSendOptions::NoteImage
//
//  False with no handler set. The handler is called outside the lock.
//
////////////////////////////////////////////////////////////////////////////////

bool CassoLinkFullSendOptions::NoteImage (const std::string & link, const std::string & path, const std::string & model) const
{
    ImageFn  handler;



    {
        std::lock_guard<std::mutex>  lock (m_lock);

        handler = m_imageHandler;
    }

    if (handler)
    {
        handler (link, path, model);
    }

    return (bool) handler;
}
