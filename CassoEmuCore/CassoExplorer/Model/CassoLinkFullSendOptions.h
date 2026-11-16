#pragma once

#include "Pch.h"

#include "CassoLink/FullSend/FullSendProtection.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkFullSendOptions
//
//  The options every connection's Full Send starts with, set on the window's
//  thread and read on the connections' threads.
//
////////////////////////////////////////////////////////////////////////////////

class CassoLinkFullSendOptions
{
public:
    using MakeFolderFn = std::function<HRESULT (const std::string & folder)>;
    using ImageFn      = std::function<void (const std::string & link, const std::string & path, const std::string & model)>;

    void                       SetTarget          (const std::string & folder, const std::string & extension, MakeFolderFn makeFolder);
    bool                       TryGetTarget       (std::string & outFolder, std::string & outExtension);
    void                       SetProtection      (const FullSendProtectionChoices & choices);
    FullSendProtectionChoices  GetProtection      () const;
    void                       SetSavingReads     (bool on);
    bool                       IsSavingReads      () const;
    void                       SetAppleBootSounds (bool on)   { m_appleBootSounds = on; }
    bool                       HasAppleBootSounds () const    { return m_appleBootSounds; }
    void                       SetImageHandler    (ImageFn handler);
    bool                       NoteImage          (const std::string & link, const std::string & path, const std::string & model) const;

private:
    mutable std::mutex         m_lock;
    std::string                m_folder;
    std::string                m_extension;
    MakeFolderFn               m_makeFolder;
    FullSendProtectionChoices  m_protection;
    bool                       m_savingReads     = false;
    std::atomic<bool>          m_appleBootSounds { false };
    ImageFn                    m_imageHandler;
};
