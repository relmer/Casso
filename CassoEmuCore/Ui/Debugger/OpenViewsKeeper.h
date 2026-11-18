#pragma once





////////////////////////////////////////////////////////////////////////////////
//
//  OpenViewsKeeper
//
//  When the debugger window saves the views it has open. The first snapshot
//  reopens the set saved at the last close; until the reopened views have
//  settled, the set on screen is the one the window started with, not a
//  choice the user made, so nothing is written; after that each change is.
//
////////////////////////////////////////////////////////////////////////////////

class OpenViewsKeeper
{
public:
    //  How many snapshots the reopened views get to arrive in.
    static constexpr int  kSettlingSnapshots = 10;

    //  Whether the saved set has yet to be reopened, and the set to reopen,
    //  once taken.
    bool                  IsRestoreDue () const { return !m_isRestored; }
    void                  BeginRestore (const std::string & saved);
    const std::string   & GetSaved     () const { return m_saved; }

    //  True from the first snapshot until the reopened views have settled.
    bool  IsRestoring () const { return !m_isRestored || m_settling > 0; }

    //  Whether the set on screen, as text, is to be written now.
    bool  ShouldSave  (const std::string & text);

private:
    std::string  m_saved;
    bool         m_isRestored = false;
    int          m_settling   = 0;
};
