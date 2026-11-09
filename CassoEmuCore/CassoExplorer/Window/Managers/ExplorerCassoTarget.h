#pragma once

#include "Pch.h"

#include "Seams/Win32ProcessLauncher.h"

class IFileSystem;
struct ExplorerContext;





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerCassoTarget
//
//  The Casso a disk image goes to: inserting the selected image into a drive
//  of the Casso the window targets, opening it in a new Casso, and the number
//  of drives the target has.
//
////////////////////////////////////////////////////////////////////////////////

class ExplorerCassoTarget
{
public:
    explicit ExplorerCassoTarget (ExplorerContext & context);

    void            InsertIntoDrive             (const std::wstring & imagePath, int drive);
    void            OpenInNewCasso              (const std::wstring & imagePath);
    HWND            FindCassoTarget             () const;
    int             GetDefaultMachineDriveCount ();
    void            AskCassoToDescribe          ();
    std::wstring    GetSelectedImagePath        () const;
    static HRESULT  MergeUserMachineDelta       (const std::string & machine, IFileSystem & files, std::string & inoutJson);

    //  How many drives the Casso the window targets has, from its reply.
    void            SetCassoDriveCount          (int count) { m_cassoDriveCount = count; }
    int             GetCassoDriveCount          () const    { return m_cassoDriveCount; }

private:
    ExplorerContext       & m_context;
    Win32ProcessLauncher    m_launcher;
    int                     m_defaultDriveCount = -1;   // the default machine's, once read
    int                     m_cassoDriveCount   = 0;    // the drives the Casso the window targets has
};