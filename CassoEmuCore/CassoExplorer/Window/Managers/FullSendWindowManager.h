#pragma once

#include "Pch.h"

#include "CassoExplorer/FullSendDialog.h"

struct ExplorerContext;





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendWindowManager
//
//  The Full Send windows: one for each connection with a session, and the
//  folder's, which lists the disks in its manifest. Also the Full Send
//  folder and the options a session starts with.
//
////////////////////////////////////////////////////////////////////////////////

class FullSendWindowManager
{
public:
    explicit FullSendWindowManager (ExplorerContext & context);
    ~FullSendWindowManager();

    static std::wstring  GetDefaultFullSendFolder ();

    void                              ApplyFullSendPrefs  ();
    void                              OpenFullSendOnStart ();
    std::vector<FullSendPanel::View>  GetRunningViews     ();

    //  The Full Send window of each connection with a session in progress,
    //  opened or brought to the front; with none, the window of the
    //  connection whose session ended, or the folder's. Without activation
    //  the keyboard focus is where it was.
    void          ShowFullSendWindow (bool activate);
    bool          IsFullSendRunning  ();
    std::wstring  GetFullSendFolder  () const;

private:
    bool  FillFullSendView     (FullSendPanel::View & ioView);
    bool  FillConnectionView   (const std::wstring & connection, FullSendPanel::View & ioView);
    void  ShowConnectionWindow (const std::wstring & connection, bool activate);
    void  EndFullSend          (const std::wstring & connection);
    bool  FillFromManifest     (FullSendPanel::View & ioView);
    void  RunFullSendDisk      (const std::string & file, FullSendDialog::DiskAction action);
    bool  CanRepair            (const std::string & file) const;

    static constexpr int  kCascadeDip = 40;     // between two Full Send windows open at once

    ExplorerContext  & m_context;

    //  The Full Send window of each connection, by its name, and the folder's
    //  under an empty name.
    std::map<std::wstring, std::unique_ptr<FullSendDialog>>  m_fullSendWindows;
    bool                                                     m_fullSendFilled = false;   // the folder's window has its manifest
};
