#include "Pch.h"

#include "CassoExplorer/Window/Managers/CassoLinkManager.h"
#include "CassoExplorer/Window/Managers/CassoLinkJobManager.h"
#include "CassoExplorer/Window/Managers/CassoLinkDriveManager.h"
#include "CassoExplorer/Window/Managers/FullSendWindowManager.h"
#include "CassoExplorer/Model/CassoLinkConnectionText.h"
#include "CassoExplorer/Model/CassoLinkShimBuilder.h"
#include "CassoLink/LinkAudioBootstrap.h"
#include "CassoExplorer/CassoExplorerLinkDialog.h"
#include "CassoExplorer/CassoLinkActivityDialog.h"
#include "CassoExplorer/CassoExplorerShell.h"
#include "CassoExplorer/CassoExplorerPromptDialog.h"
#include "CassoLink/LinkFault.h"
#include "Core/TextEncoding.h"
#include "Seams/IHostDialogs.h"
#include "Seams/Win32HostDialogs.h"
#include "CassoExplorer/Window/Managers/ExplorerLayout.h"
#include "CassoExplorer/Window/Managers/ExplorerDragDrop.h"
#include "CassoExplorer/Window/Managers/ExplorerMenus.h"
#include "CassoExplorer/Window/Panes/ExplorerAddressBar.h"
#include "CassoExplorer/Window/Panes/ExplorerTreePane.h"
#include "CassoExplorer/Window/Panes/ExplorerStatusBar.h"
#include "CassoExplorer/Window/Panes/ExplorerListPane.h"
#include "CassoExplorer/Window/Managers/ExplorerPreviewManager.h"
#include "CassoExplorer/Actions/ActionsTypes.h"
#include "Core/AppTimerIds.h"
#include "CassoExplorer/CassoExplorerBrowser.h"





//  The window whose m_cassetteWav is the sound in progress. PlaySound reads
//  from that memory, so that window stops the sound before the memory is
//  freed, and no other window stops it.
static const ExplorerContext *  s_cassettePlayer = nullptr;





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkManager::CassoLinkManager
//
////////////////////////////////////////////////////////////////////////////////

CassoLinkManager::CassoLinkManager (ExplorerContext & context)
    : m_context (context)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkManager::~CassoLinkManager
//
////////////////////////////////////////////////////////////////////////////////

CassoLinkManager::~CassoLinkManager()
{
    if (s_cassettePlayer == &m_context)
    {
        PlaySoundW (nullptr, nullptr, 0);
        s_cassettePlayer = nullptr;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkManager::ApplyVerifyPrefs
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkManager::ApplyVerifyPrefs()
{
    if (m_context.app.links != nullptr)
    {
        m_context.app.links->SetVerifyDefaults (m_context.prefs.verifyReads, m_context.prefs.verifyWrites, m_context.prefs.autoVerify);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkManager::TryGetConnectionEntry
//
////////////////////////////////////////////////////////////////////////////////

bool CassoLinkManager::TryGetConnectionEntry (const std::wstring & name, LinkTreeEntry & outEntry) const
{
    const std::vector<LinkTreeEntry>  & links = m_context.browser.GetTreeModel().GetLinks();
    auto                                found = std::find_if (links.begin(), links.end(), [&name] (const LinkTreeEntry & link) { return link.name == name; });



    if (found != links.end())
    {
        outEntry = *found;
    }

    return found != links.end();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkManager::EditConnection
//
//  An empty name adds a link. An edited link is stopped, as its settings
//  are no longer the ones its port was opened with.
//
//  The CassoLink connections: added or edited through the link dialog, removed,
//  or disconnected, which closes the link's port. Each change is saved
//  at once.
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkManager::EditConnection (const std::string & name)
{
    HRESULT                           hr      = S_OK;
    CassoLinkSettings                 initial;
    std::vector<std::wstring>         others;
    std::vector<std::wstring>         ports;
    CassoExplorerLinkDialog::Outcome  outcome;
    auto                              isNamed = [&name] (const CassoLinkSettings & link) { return link.name == name; };
    auto                              found   = std::find_if (m_context.prefs.links.begin(), m_context.prefs.links.end(), isNamed);



    if (found != m_context.prefs.links.end())
    {
        initial = *found;
    }

    for (const CassoLinkSettings & link : m_context.prefs.links)
    {
        if (link.name != name)
        {
            others.push_back (TextEncoding::NarrowToWide (link.name));
        }
    }

    //  With no ports listed, the drop-down has Any alone.
    if (m_context.app.links != nullptr)
    {
        hr = m_context.app.links->ListComPorts (ports);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    outcome = CassoExplorerLinkDialog::Ask (m_context.window.GetHwnd(), m_context.theme, initial, others, ports, found == m_context.prefs.links.end(),
                                            [this] (HWND owner, std::wstring & inOutFolder) -> bool
    {
        std::filesystem::path  picked;
        bool                   chosen = false;
        HRESULT                result = m_context.dialogs.PickFolder (owner, picked, chosen);

        if (SUCCEEDED (result) && chosen)
        {
            inOutFolder = picked.wstring();
        }

        return SUCCEEDED (result) && chosen;
    },
    [this] (HWND owner, int drive, std::wstring & inOutImage) -> bool
    {
        std::filesystem::path  picked;
        bool                   chosen = false;
        FileDialogSpec         spec;
        HRESULT                result = S_OK;

        spec.filters = { FileDialogFilter { L"ProDOS images (*.po, *.dsk, *.hdv, *.2mg)", L"*.po;*.dsk;*.hdv;*.2mg" },
                         FileDialogFilter { L"All files (*.*)", L"*.*" } };
        spec.title   = std::format (L"Choose the image for virtual drive {}", drive);
        spec.content = FileDialogContent::Images;

        result = m_context.dialogs.PickFileToOpen (owner, spec, picked, chosen);

        if (SUCCEEDED (result) && chosen)
        {
            inOutImage = picked.wstring();
        }

        return SUCCEEDED (result) && chosen;
    });

    if (!outcome.confirmed)
    {
        return;
    }

    //  Another window can add or remove a connection while the dialog is open,
    //  so the link is found again.
    found = std::find_if (m_context.prefs.links.begin(), m_context.prefs.links.end(), isNamed);

    if (found != m_context.prefs.links.end())
    {
        outcome.settings.userDisconnected = found->userDisconnected;
        outcome.settings.lastPort         = outcome.settings.port == found->port ? found->lastPort : std::string();
        outcome.settings.lastUsed         = found->lastUsed;

        *found = outcome.settings;
        ApplyConnections();
        return;
    }

    m_context.prefs.links.push_back (outcome.settings);
    ApplyConnections();

    //  A new connection takes its port from any other.
    Connect (outcome.settings.name);

    //  A new connection is selected in the tree, as a new folder is.
    m_context.addressBar.GoToLocation (Location::MakeRoot (TreeModel::MakeLinkId (TextEncoding::NarrowToWide (outcome.settings.name))));
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkManager::RemoveConnection
//
//  A tab on the link goes to This PC.
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkManager::RemoveConnection (const std::string & name)
{
    std::wstring  link;
    int           slot  = 0;
    int           drive = 0;



    std::erase_if (m_context.prefs.links, [&name] (const CassoLinkSettings & settings) { return settings.name == name; });

    if (TreeModel::TryParseLinkId (m_context.browser.GetLocation().path, link, slot, drive) && TextEncoding::WideToNarrow (link) == name)
    {
        m_context.addressBar.GoToLocation (Location::MakeRoot (TreeModel::kThisPcRootId));
    }

    ApplyConnections();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkManager::Disconnect
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkManager::Disconnect (const std::string & name)
{
    m_context.app.links->Disconnect (name);
    HandleConnectionChanged();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkManager::SearchComPorts
//
//  The link is started again, searching the COM ports for the Apple; the
//  link's root reports the search and then what it found.
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkManager::SearchComPorts (const std::string & name)
{
    HRESULT  hr = S_OK;



    m_context.app.links->Stop (name);

    hr = m_context.app.links->Start (name, true);

    if (FAILED (hr))
    {
        m_context.window.ShowMessage (L"CassoLink could not be started.", MB_ICONERROR);
    }

    HandleConnectionChanged();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkManager::BootstrapConnection
//
//  The user readies the Apple first; the loader is then typed into it
//  over the link.
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkManager::BootstrapConnection (const std::string & name)
{
    constexpr double     kUsPerSecond = 1000000.0;
    HRESULT              hr           = S_OK;
    std::wstring         steps;
    int                  answer       = IDCANCEL;
    std::vector<double>  halves;
    std::string          command;
    double               seconds      = 0.0;



    if (IsCassetteConnection (name))
    {
        hr = LinkAudioBootstrap::BuildCassetteLinkRecord (halves, command);
        CHR (hr);

        command.pop_back();

        for (double half : halves)
        {
            seconds += half / kUsPerSecond;
        }

        steps  = std::format (L"Connect this PC's sound output to the cassette input of the ][+ or //e, and the Apple's cassette output "
                              L"to this PC's sound input. Turn the volume up most of the way.\n\n"
                              L"On the Apple, at the prompt, type:\n\n"
                              L"1. **CALL -151**, then press Return\n"
                              L"2. **{}**, then press Return\n\n"
                              L"Then choose Send. The audio takes about {} seconds.", TextEncoding::NarrowToWide (command), (int) ceil (seconds));
        answer = DxuiMessageBox (m_context.window.GetHwnd(), m_context.theme, steps.c_str(), L"Send the program",
                                 MB_OKCANCEL | MB_ICONINFORMATION, { L"Send", L"Cancel" });
    }
    else
    {
        steps  = L"On the Apple, at the prompt, type:\n\n"
                 L"1. **IN#2** on a //c's port 2, or IN# and the Super Serial Card's slot, then press Return\n"
                 L"2. **Ctrl+A 14B**, then press Return\n\n"
                 L"On a //c's port 1, type **IN#1** and press Return, then type **POKE 1913,65:POKE 49306,11:POKE 49307,30:POKE 43605,5:POKE 43606,193** with DOS 3.3 loaded, "
                 L"**POKE 1913,65:POKE 49306,11:POKE 49307,30:POKE 48690,5:POKE 48691,193** with ProDOS, or "
                 L"**POKE 1913,65:POKE 49306,11:POKE 49307,30:POKE 56,5:POKE 57,193** with neither, and press Return.\n\nThe loader is then typed into the Apple, which takes about 40 seconds.";
        answer = DxuiMessageBox (m_context.window.GetHwnd(), m_context.theme, steps.c_str(), L"Bootstrap the Apple",
                                 MB_OKCANCEL | MB_ICONINFORMATION, { L"Bootstrap", L"Cancel" });
    }

    BAIL_OUT_IF (answer != IDOK, S_OK);

    hr = m_context.app.links->Bootstrap (name, [] (const LinkOperation & operation) -> std::wstring
    {
        return FAILED (operation.result) ? CassoLinkConnectionText::DescribeFailure (operation) : std::wstring();
    });
    CHR (hr);

    HandleConnectionChanged();

Error:
    if (FAILED (hr))
    {
        m_context.window.ShowMessage (L"CassoLink could not be started.", MB_ICONERROR);
        HandleConnectionChanged();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkManager::ArmLinkFaults
//
//  A Debug build's faults for the next operations, typed in the form
//  --link-faults takes. The dialog opens again for text that doesn't parse.
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkManager::ArmLinkFaults()
{
    std::wstring            text;
    std::vector<LinkFault>  faults;
    HRESULT                 hrParse = S_OK;
    bool                    parsed  = false;



    while (!parsed && CassoExplorerPromptDialog::Ask (m_context.window.GetHwnd(), m_context.theme, L"Arm link faults",
                                                       L"Faults for the next operations, such as pc:flip@40 or apple:unreadable@T17S3:", text, kFaultTextMax, text))
    {
        hrParse = LinkFault::Parse (TextEncoding::WideToNarrow (text), faults);
        parsed  = SUCCEEDED (hrParse) && !faults.empty();

        if (!parsed)
        {
            DxuiMessageBox (m_context.window.GetHwnd(), m_context.theme, (L"\"" + text + L"\" isn't a list of faults. Each is source:kind@where, separated by semicolons.").c_str(),
                            L"Arm link faults", MB_OK | MB_ICONWARNING);
        }
    }

    if (!parsed)
    {
        return;
    }

    LinkFaults::Arm (faults);

    for (const LinkFault & fault : faults)
    {
        m_context.app.links->AppendLogRow (CassoExplorerShell::kFaultLogName, "Armed a fault: " + LinkFault::Describe (fault));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkManager::PlayCassetteBootstrap
//
//  The shim as audio from the PC's sound output into a ][+ or //e's
//  cassette input. The user readies the monitor's read
//  first. The link is started while the audio is sent, so its session is
//  open when the shim sends Ready.
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkManager::PlayCassetteBootstrap (const std::string & name)
{
    HRESULT            hr      = S_OK;
    CassoLinkSettings  link;
    std::string        command;
    std::wstring       steps;
    int                answer  = IDCANCEL;
    bool               played  = false;



    for (const CassoLinkSettings & settings : m_context.app.links->GetLinks())
    {
        link = (settings.name == name) ? settings : link;
    }

    //  PlaySound has one sound at a time, and the buffer is about to change.
    PlaySoundW (nullptr, nullptr, 0);
    s_cassettePlayer = nullptr;

    hr = LinkAudioBootstrap::BuildWav (CassoLinkShimBuilder::MakeShimOptions (CassoLinkShimBuilder::GetPcName(), link), m_cassetteWav, command);
    CHR (hr);

    command.pop_back();

    steps  = std::format (L"Connect this PC's sound output to the cassette input of the ][+ or //e, and turn the volume up most of the way.\n\n"
                          L"On the Apple, at the prompt, type:\n\n"
                          L"1. **CALL -151**, then press Return\n"
                          L"2. **{}**, then press Return\n\n"
                          L"Then choose Send. The audio takes about 12 seconds.", TextEncoding::NarrowToWide (command));
    answer = DxuiMessageBox (m_context.window.GetHwnd(), m_context.theme, steps.c_str(), L"Bootstrap from the cassette input",
                             MB_OKCANCEL | MB_ICONINFORMATION, { L"Send", L"Cancel" });
    BAIL_OUT_IF (answer != IDOK, S_OK);

    m_cassetteLink  = name;
    m_cassettePlays = 0;

    played = PlayCassette();
    CBR (played);

    s_cassettePlayer = &m_context;

    if (!m_context.app.links->IsStarted (name))
    {
        Connect (name);
    }

Error:
    if (FAILED (hr))
    {
        m_context.window.ShowMessage (L"The cassette bootstrap could not be sent to this PC's sound output.", MB_ICONERROR);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkManager::PlayCassette
//
//  The audio sent once more, with the timer for the look at whether the
//  Apple started set past its end.
//
////////////////////////////////////////////////////////////////////////////////

bool CassoLinkManager::PlayCassette()
{
    BOOL  played = PlaySoundW (reinterpret_cast<LPCWSTR> (m_cassetteWav.data()), nullptr, SND_MEMORY | SND_ASYNC | SND_NODEFAULT);



    if (!played)
    {
        return false;
    }

    m_cassettePlays++;
    SetTimer (m_context.window.GetHwnd(), (UINT_PTR) AppTimerId::ExplorerCassette, LinkAudioBootstrap::GetLengthMs (m_cassetteWav) + kCassetteGraceMs, nullptr);
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkManager::HasCassetteStarted
//
//  The shim replied: the program is loading, or loaded.
//
////////////////////////////////////////////////////////////////////////////////

bool CassoLinkManager::HasCassetteStarted() const
{
    CassoLinkSession  * session = m_context.app.links->GetSession (m_cassetteLink);
    LinkState           state   = (session != nullptr) ? session->GetState() : LinkState::Closed;



    return state == LinkState::LoadingProgram || state == LinkState::Ready || state == LinkState::Busy;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkManager::HandleCassetteTimer
//
//  A few seconds past the audio's end. While the Apple hasn't started, the
//  shim's slot finder has the monitor read the tape again, so the audio is
//  sent again; after kCassettePlays, ShowMessage reports the failure.
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkManager::HandleCassetteTimer()
{
    KillTimer (m_context.window.GetHwnd(), (UINT_PTR) AppTimerId::ExplorerCassette);

    //  Started, or disconnected by the user since.
    if (m_cassetteLink.empty() || HasCassetteStarted() || !m_context.app.links->IsStarted (m_cassetteLink))
    {
        m_cassetteLink.clear();
        return;
    }

    if (m_cassettePlays < kCassettePlays && PlayCassette())
    {
        m_context.app.links->AppendLogRow (m_cassetteLink, "The Apple didn't start from the cassette audio, so it was sent again");
        return;
    }

    m_cassetteLink.clear();
    m_context.window.ShowMessage (L"The Apple didn't start from the cassette audio after multiple retries. Turn the volume up, check the cable, and choose Bootstrap from the cassette input again.", MB_ICONWARNING);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkManager::SaveCassetteBootstrap
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkManager::SaveCassetteBootstrap (const std::string & name)
{
    HRESULT                        hr     = S_OK;
    std::filesystem::path          picked;
    bool                           chosen = false;
    FileDialogSpec                 spec;
    CassoLinkSettings              link;
    std::string                    command;
    CassoExplorerActions::Outcome  outcome;



    for (const CassoLinkSettings & settings : m_context.app.links->GetLinks())
    {
        link = (settings.name == name) ? settings : link;
    }

    spec.filters          = { FileDialogFilter { L"Sound files (*.wav)", L"*.wav" } };
    spec.clientGuid       = CassoLinkDriveManager::s_kLinkDiskPickerGuid;
    spec.defaultFileName  = L"CassoLink.wav";
    spec.defaultExtension = L"wav";
    spec.title            = L"Save cassette bootstrap";

    hr = m_context.dialogs.PickFileToSave (m_context.window.GetHwnd(), spec, picked, chosen);
    CHR (hr);
    BAIL_OUT_IF (!chosen, S_OK);

    outcome = m_context.actions.GetLinkDisks().SaveCassetteBootstrap (picked.wstring(), link, command);

    if (!outcome.Succeeded())
    {
        m_context.window.ShowMessage (outcome.message, MB_ICONERROR);
    }
    else
    {
        command.pop_back();
        m_context.window.ShowMessage (std::format (L"To load it, type CALL -151 and then {} on the Apple, and play the file into its cassette input.",
                                           TextEncoding::NarrowToWide (command)), MB_ICONINFORMATION);
    }

Error:
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkManager::ReleaseIdleConnections
//
//  A link connects only while its machine, or one of its drives, is the
//  location of some window. One that has not connected by the time every
//  window has gone elsewhere is stopped, closing its port; one that has
//  connected, or is bootstrapping or loading the program, stays. So does one
//  started at launch, which connects whenever a Ready from its Apple arrives,
//  whether or not a window is on it: stopping it before its first Ready left the Apple's requests,
//  such as F for a Full Send, with nothing to read them.
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkManager::ReleaseIdleConnections()
{
    std::wstring              current;
    int                       slot    = 0;
    int                       drive   = 0;
    std::vector<std::string>  release;



    if (m_context.app.links == nullptr)
    {
        return;
    }

    TreeModel::TryParseLinkId (m_context.browser.GetLocation().path, current, slot, drive);
    m_context.app.links->SetViewing (&m_context, TextEncoding::WideToNarrow (current));

    for (const CassoLinkSettings & settings : m_context.app.links->GetLinks())
    {
        CassoLinkSession  * session = m_context.app.links->GetSession (settings.name);
        LinkState           state   = session != nullptr ? session->GetState() : LinkState::Closed;
        bool                connected = state == LinkState::Ready || state == LinkState::Busy
                                     || state == LinkState::Bootstrapping || state == LinkState::LoadingProgram;

        //  ADTPro's client speaks first, so an ADTPro server stays open for it.
        if (session != nullptr && !connected && settings.transport != LinkTransport::AdtProServer && !IsLaunchConnection (settings) && !m_context.app.links->IsViewed (settings.name))
        {
            release.push_back (settings.name);
        }
    }

    for (const std::string & name : release)
    {
        m_context.app.links->Stop (name);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkManager::Connect
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkManager::Connect (const std::string & name)
{
    HRESULT  hr = m_context.app.links->Start (name);



    if (hr == HRESULT_FROM_WIN32 (ERROR_BUSY))
    {
        m_context.window.ShowMessage (L"A transfer is using this connection's port. Try again when it finishes.", MB_ICONINFORMATION);
    }

    HandleConnectionChanged();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkManager::HasVirtualDrives
//
//  Whether the link is to a 6551, which the CassoLink ProDOS disk's driver
//  uses: a serial port, or Casso's emulated one on TCP.
//
////////////////////////////////////////////////////////////////////////////////

bool CassoLinkManager::HasVirtualDrives (const std::string & name) const
{
    const std::vector<CassoLinkSettings> &  links = m_context.app.links->GetLinks();



    return std::any_of (links.begin(), links.end(), [&name] (const CassoLinkSettings & settings)
    {
        return settings.name == name
            && (settings.transport == LinkTransport::Serial || settings.transport == LinkTransport::Tcp || settings.transport == LinkTransport::Emulated);
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkManager::IsSerialConnection
//
////////////////////////////////////////////////////////////////////////////////

bool CassoLinkManager::IsSerialConnection (const std::string & name) const
{
    const std::vector<CassoLinkSettings> &  links = m_context.app.links->GetLinks();



    return std::any_of (links.begin(), links.end(), [&name] (const CassoLinkSettings & settings)
    {
        return settings.name == name && settings.transport == LinkTransport::Serial;
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkManager::IsCassetteConnection
//
////////////////////////////////////////////////////////////////////////////////

bool CassoLinkManager::IsCassetteConnection (const std::string & name) const
{
    const std::vector<CassoLinkSettings> &  links = m_context.app.links->GetLinks();



    return std::any_of (links.begin(), links.end(), [&name] (const CassoLinkSettings & settings)
    {
        return settings.name == name && settings.transport == LinkTransport::Cassette;
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkManager::CanMakeSosDisk
//
//  The SOS disk's program uses the Apple ///'s serial port through .RS232:
//  a serial cable, or a Wi-Fi modem connection, which accepts MAME's serial
//  socket too. The program doesn't dial, so the user dials a real modem.
//
////////////////////////////////////////////////////////////////////////////////

bool CassoLinkManager::CanMakeSosDisk (const std::string & name) const
{
    const std::vector<CassoLinkSettings> &  links = m_context.app.links->GetLinks();



    return std::any_of (links.begin(), links.end(), [&name] (const CassoLinkSettings & settings)
    {
        return settings.name == name && (settings.transport == LinkTransport::Serial || settings.transport == LinkTransport::WifiModem);
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkManager::CanBootstrapConnection
//
////////////////////////////////////////////////////////////////////////////////

bool CassoLinkManager::CanBootstrapConnection (const std::string & name) const
{
    const std::vector<CassoLinkSettings> &  links = m_context.app.links->GetLinks();



    return std::any_of (links.begin(), links.end(), [&name] (const CassoLinkSettings & settings)
    {
        return settings.name == name && settings.CanBootstrap();
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkManager::AddConnectionRows
//
//  A linked machine's settings, its port and the machine itself; a drive's
//  whole disk; and CassoLink's Add a CassoLink machine. The tree's node and
//  the list's row have the same rows.
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkManager::AddConnectionRows (std::vector<DxuiPopupMenuItem> & items, bool hasLocation, const Location & location)
{
    std::wstring  link;
    int           slot  = 0;
    int           drive = 0;



    if (m_context.app.links != nullptr && hasLocation && location.kind == Location::Kind::Root && TreeModel::TryParseLinkId (location.path, link, slot, drive))
    {
        std::string  name = TextEncoding::WideToNarrow (link);

        bool  saved = !m_context.app.links->IsDiscovered (name);

        if (saved)
        {
            m_context.menus.AddMenuCommand (items, L"&Edit connection...", [this, name]() { EditConnection (name); });
        }

        if (m_context.app.links->IsStarted (name))
        {
            m_context.menus.AddMenuCommand (items, L"&Disconnect", [this, name]() { Disconnect (name); });
        }
        else
        {
            m_context.menus.AddMenuCommand (items, L"&Connect", [this, name]() { Connect (name); });
        }

        if (IsSerialConnection (name))
        {
            m_context.menus.AddMenuCommand (items, L"&Search COM ports", [this, name]() { SearchComPorts (name); });
        }

        //  An Apple on its cassette ports is started by the program sent as audio.
        if (IsCassetteConnection (name))
        {
            m_context.menus.AddMenuCommand (items, L"Send the &program...", [this, name]() { BootstrapConnection (name); });
            m_context.menus.AddMenuCommand (items, L"Save the program as a&udio...", [this, name]() { SaveCassetteBootstrap (name); });
        }

        //  Only an Apple on a serial card can be started from here.
        if (CanBootstrapConnection (name) && !IsCassetteConnection (name))
        {
            m_context.menus.AddMenuCommand (items, L"&Bootstrap the Apple...", [this, name]() { BootstrapConnection (name); });
            m_context.menus.AddMenuCommand (items, L"Bootstrap from the c&assette input...", [this, name]() { PlayCassetteBootstrap (name); });
            m_context.menus.AddMenuCommand (items, L"Save cassette boo&tstrap...", [this, name]() { SaveCassetteBootstrap (name); });
        }

        //  A boot disk is built for its link: at its speed, a Wi-Fi modem's
        //  dialing this PC, and an Uthernet II's with the link's address for
        //  when no DHCP reply comes.
        if (!IsCassetteConnection (name))
        {
            m_context.menus.AddMenuCommand (items, L"Save CassoLink disk &image...", [this, name]() { m_context.linkDrives.SaveLinkDiskImage (name); });
        }

        //  The ProDOS disk's driver talks to a 6551, so only a link to a
        //  serial port has its virtual drives.
        if (HasVirtualDrives (name))
        {
            m_context.menus.AddMenuCommand (items, L"Make CassoLink &virtual drives disk...", [this, name]() { m_context.linkDrives.SaveLinkProDosDisk (name); });
        }

        if (CanMakeSosDisk (name))
        {
            m_context.menus.AddMenuCommand (items, L"Make CassoLink S&OS disk...", [this, name]() { m_context.linkDrives.SaveLinkSosDisk (name); });
        }

        if (saved)
        {
            m_context.menus.AddMenuCommand (items, L"Re&move connection", [this, name]() { RemoveConnection (name); });
        }

#if defined (_DEBUG)
        m_context.menus.AddMenuCommand (items, L"Arm link fa&ults...", [this]() { ArmLinkFaults(); });
#endif
    }
    else if (m_context.app.links != nullptr && hasLocation && location.kind == Location::Kind::AppleDrive && location.innerPath.empty())
    {
        //  A drive's whole disk, read or written from its node. A block device
        //  has no CassoLink disk or format here.
        bool          isBlock = TreeModel::TryParseLinkId (location.path, link, slot, drive) && drive >= CassoLinkBlockDevices::kFirstDrive;

        m_context.menus.AddMenuCommand (items, L"Create &image...", [this, location]() { m_context.linkDrives.ReadDiskToImage (location); });
        m_context.menus.AddMenuCommand (items, L"&Write image...",  [this, location]() { m_context.linkDrives.WriteImageToDisk (location); });

        if (!isBlock)
        {
            m_context.menus.AddMenuCommand (items, L"Write &CassoLink to disk...", [this, location]() { m_context.linkDrives.SaveLinkToDisk (location); });

            if (HasVirtualDrives (TextEncoding::WideToNarrow (link)))
            {
                m_context.menus.AddMenuCommand (items, L"Write CassoLink &virtual drives disk...", [this, location]() { m_context.linkDrives.SaveLinkProDosToDisk (location); });
            }

            m_context.menus.AddMenuCommand (items, L"&Format disk...", [this, location]() { m_context.linkDrives.FormatAppleDisk (location); });
        }

        //  As a Pascal image's menu has it, with its icon.
        if (TreeModel::TryParseLinkId (location.path, link, slot, drive) && m_context.actions.IsDrivePascal (TextEncoding::WideToNarrow (link), drive))
        {
            m_context.menus.AddMenuCommand (items, ExplorerMenus::GetVerbLabel (CassoExplorerActions::Verb::Krunch), [this, location]() { m_context.linkDrives.KrunchAppleDisk (location); },
                                            L"", ExplorerMenus::GetVerbMenuGlyph (CassoExplorerActions::Verb::Krunch));
        }

#if defined (_DEBUG)
        m_context.menus.AddMenuCommand (items, L"Arm link fa&ults...",        [this]() { ArmLinkFaults(); });
#endif
    }

    if (m_context.app.links != nullptr && hasLocation && location.kind == Location::Kind::Root && location.path == TreeModel::kCassoLinkRootId)
    {
        m_context.menus.AddMenuCommand (items, L"&Add a CassoLink connection...", [this]() { EditConnection (std::string()); });
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkManager::StartConnections
//
//  At launch, the most recently chosen first, so it has its port, each that
//  IsLaunchConnection allows. A running Casso waits for its list.
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkManager::StartConnections()
{
    HRESULT                         hr    = S_OK;
    std::vector<CassoLinkSettings>  links = m_context.prefs.links;



    BAIL_OUT_IF (m_context.app.links == nullptr, S_OK);

    std::stable_sort (links.begin(), links.end(), [] (const CassoLinkSettings & a, const CassoLinkSettings & b) { return a.lastUsed > b.lastUsed; });

    for (const CassoLinkSettings & settings : links)
    {
        if (settings.userDisconnected)
        {
            m_context.app.links->Hold (settings.name);
        }
        else if (IsLaunchConnection (settings) && !m_context.app.links->IsStarted (settings.name) && !m_context.app.links->IsHeld (settings.name))
        {
            hr = m_context.app.links->StartIfFree (settings.name);
            IGNORE_RETURN_VALUE (hr, S_OK);
        }
    }

Error:
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkManager::IsLaunchConnection
//
//  An ADTPro server is started by its button. An any-port connection opens
//  only a port that has connected to an Apple, as opening another can disturb
//  the device on it, and a connection the firewall blocks would raise a prompt.
//
////////////////////////////////////////////////////////////////////////////////

bool CassoLinkManager::IsLaunchConnection (const CassoLinkSettings & settings) const
{
    LinkTreeEntry  entry;
    bool           blocked = TryGetConnectionEntry (TextEncoding::NarrowToWide (settings.name), entry) && !entry.firewall.empty();



    return !settings.userDisconnected && settings.transport != LinkTransport::AdtProServer && !blocked
        && !(settings.IsComPort() && settings.port.empty() && settings.lastPort.empty());
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkManager::SaveConnectionState
//
//  What outlasts a restart: whether the user disconnected each connection, and
//  for one replying, when it was last chosen and the port it is on.
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkManager::SaveConnectionState()
{
    bool  changed = false;
    int   newest  = 0;



    for (const CassoLinkSettings & settings : m_context.prefs.links)
    {
        newest = std::max (newest, settings.lastUsed);
    }

    for (CassoLinkSettings & settings : m_context.prefs.links)
    {
        bool         held    = m_context.app.links->IsHeldByUser (settings.name);
        bool         replies = IsConnectionReplying (settings.name);
        std::string  port    = TextEncoding::WideToNarrow (m_context.app.links->GetOpenPort (settings.name));

        changed = changed || held != settings.userDisconnected;
        settings.userDisconnected = held;

        if (replies && settings.lastUsed != newest)
        {
            settings.lastUsed = ++newest;
            changed           = true;
        }

        if (replies && settings.port.empty() && !port.empty() && port != settings.lastPort)
        {
            settings.lastPort = port;
            changed           = true;
        }
    }

    if (changed)
    {
        m_context.window.SaveSession();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkManager::IsConnectionReplying
//
////////////////////////////////////////////////////////////////////////////////

bool CassoLinkManager::IsConnectionReplying (const std::string & name) const
{
    CassoLinkSession  * session = m_context.app.links->GetSession (name);
    LinkState           state   = (session != nullptr) ? session->GetState() : LinkState::Closed;



    return state == LinkState::Ready || state == LinkState::Busy;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkManager::IsWifiModemConnection
//
////////////////////////////////////////////////////////////////////////////////

bool CassoLinkManager::IsWifiModemConnection (const std::string & name) const
{
    const std::vector<CassoLinkSettings> &  links = m_context.app.links->GetLinks();



    return std::any_of (links.begin(), links.end(), [&name] (const CassoLinkSettings & settings)
    {
        return settings.name == name && settings.transport == LinkTransport::WifiModem;
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkManager::ApplyConnections
//
//  The preferences' links to the connections and the tree, and saved.
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkManager::ApplyConnections()
{
    m_context.app.links->SetLinks (m_context.prefs.links);
    m_context.browser.GetTreeModel().SetLinks (m_context.app.links->BuildTreeEntries());

    m_context.window.SaveSession();
    m_context.treePane.RefreshTree();

    //  A link that was never started changes nothing that would refresh the
    //  other windows, and one of them can be on a link just removed.
    if (m_context.app.host != nullptr)
    {
        m_context.app.host->ForEachWindow ([] (CassoExplorerWindow & window) { window.GetCassoLinkManager().RefreshCassoLinkViews(); });
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkManager::UpdateUnseenFiles
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkManager::UpdateUnseenFiles()
{
    TreeModel  & tree = m_context.browser.GetTreeModel();



    for (const std::wstring & folder : tree.GetLinkFolders())
    {
        std::error_code  error;
        int              count = 0;
        auto             seen  = m_context.prefs.linkFolderSeen.find (folder);

        //  A folder not seen before counts what arrives from now on.
        if (seen == m_context.prefs.linkFolderSeen.end())
        {
            m_context.prefs.linkFolderSeen[folder] = std::filesystem::file_time_type::clock::now().time_since_epoch().count();
            continue;
        }

        for (const std::filesystem::directory_entry & entry : std::filesystem::directory_iterator (folder, std::filesystem::directory_options::skip_permission_denied, error))
        {
            if (entry.is_regular_file (error) && TreeModel::IsReceivedFile (entry.path().filename().wstring()) && entry.last_write_time (error).time_since_epoch().count() > seen->second)
            {
                count++;
            }
        }

        tree.SetUnseenFiles (folder, count);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkManager::UpdateTransferTimer
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkManager::UpdateTransferTimer()
{
    bool  transferring = m_context.app.links->IsAnyTransferring();



    if (transferring && !m_transferTimer)
    {
        SetTimer (m_context.window.GetHwnd(), (UINT_PTR) AppTimerId::ExplorerLinkTransfer, kTransferFrameMs, nullptr);
    }
    else if (!transferring && m_transferTimer)
    {
        KillTimer (m_context.window.GetHwnd(), (UINT_PTR) AppTimerId::ExplorerLinkTransfer);
        m_context.statusBar.SetShimmer (false);
    }

    m_transferTimer = transferring;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkManager::HandleTransferTimer
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkManager::HandleTransferTimer()
{
    int        fromPeak = 0;
    ULONGLONG  now      = GetTickCount64();
    bool       animated = DxuiSystemSettings::Instance().AreAnimationsEnabled();



    m_context.linkJobs.Update();

    //  The last progress aged out, as in a Full Send waiting for disks.
    if (m_context.app.links == nullptr || !m_context.app.links->IsAnyTransferring())
    {
        m_context.statusBar.SetShimmer (false);
        RefreshCassoLinkViews();
        return;
    }

    m_pulseFrame = (int) ((now / kPulseFrameMs) % (kPulseFrames * 2));
    fromPeak     = std::abs (m_pulseFrame - kPulseFrames);

    m_context.treePane.SetActivityPulse (animated ? (float) fromPeak / kPulseFrames : 1.0f);
    m_context.statusBar.SetShimmer (animated);
    m_context.window.Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkManager::NoteListFilled
//
//  A receiving folder now listed has no files waiting to be seen.
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkManager::NoteListFilled()
{
    TreeModel       & tree     = m_context.browser.GetTreeModel();
    const Location &  location = m_context.browser.GetLocation();



    if (location.kind != Location::Kind::HostFolder)
    {
        return;
    }

    for (const std::wstring & folder : tree.GetLinkFolders())
    {
        if (_wcsicmp (folder.c_str(), location.path.c_str()) == 0)
        {
            int  unseen = tree.GetUnseenFiles (folder);

            m_context.prefs.linkFolderSeen[folder] = std::filesystem::file_time_type::clock::now().time_since_epoch().count();

            if (unseen != 0)
            {
                tree.SetUnseenFiles (folder, 0);
                m_context.window.SaveSession();
                RefreshCassoLinkViews();
            }
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkManager::HandleConnectionChanged
//
//  An CassoLink finished an operation or changed state. The links post to
//  one window, which runs the finished operations' completions and displays
//  their messages; every window then refreshes its own views.
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkManager::HandleConnectionChanged()
{
    std::vector<std::wstring>  messages;
    std::wstring               text;



    if (m_context.app.links == nullptr || !m_context.app.links->TakeChanged())
    {
        return;
    }

    messages = m_context.app.links->RunCompletions();
    m_context.app.links->ProbeDrives();
    SaveConnectionState();
    m_context.fullSendWindows.OpenFullSendOnStart();

    if (m_context.app.host != nullptr)
    {
        m_context.app.host->ForEachWindow ([] (CassoExplorerWindow & window) { window.GetCassoLinkManager().RefreshCassoLinkViews(); });
    }
    else
    {
        RefreshCassoLinkViews();
    }

    for (const std::wstring & message : messages)
    {
        text += (text.empty() ? L"" : L"\n") + message;
    }

    if (!text.empty())
    {
        m_context.window.ShowMessage (text, MB_ICONERROR);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkManager::DescribeJobs
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CassoLinkManager::DescribeJobs()
{
    std::wstring  text;



    if (m_context.app.links == nullptr)
    {
        return text;
    }

    for (const FullSendPanel::View & view : m_context.fullSendWindows.GetRunningViews())
    {
        text += (text.empty() ? L"" : L"     ") + FullSendPanel::DescribeSummary (view.snapshot, view.connection);
    }

    text = text.empty() ? m_context.linkJobs.DescribeJobs() : text;

    return text.empty() ? CassoLinkConnectionText::DescribeJobs (m_context.app.links->GetPendingJobs()) : text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkManager::RefreshCassoLinkViews
//
//  The links' roots and drives are listed again where they are in the tree,
//  and the list and preview are refreshed when the location is on a link.
//
//  A CassoLink operation finished or a link changed state: the tree, the
//  list and the status bar are brought up to date.
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkManager::RefreshCassoLinkViews()
{
    HRESULT                    hr       = S_OK;
    Location                   location = m_context.browser.GetLocation();
    std::wstring               link;
    int                        slot     = 0;
    int                        drive    = 0;
    bool                       showing  = TreeModel::TryParseLinkId (location.path, link, slot, drive);
    std::wstring               jobs;



    if (m_context.app.links == nullptr)
    {
        return;
    }

    //  The running operation's progress and the queue after it; the status
    //  bar's own text returns once nothing is pending.
    jobs = DescribeJobs();

    if (!jobs.empty())
    {
        m_context.statusBar.SetDetail (jobs);
        m_linkJobsShown = true;
    }
    else if (m_linkJobsShown)
    {
        m_linkJobsShown = false;
        m_context.statusBar.Fill();
    }

    m_context.browser.GetTreeModel().SetLinks (m_context.app.links->BuildTreeEntries());
    UpdateUnseenFiles();
    UpdateTransferTimer();
    m_context.statusBar.SetActivity (GetLatestActivity());

    for (const CassoLinkSettings & settings : m_context.app.links->GetLinks())
    {
        std::wstring  id = TreeModel::MakeLinkId (TextEncoding::NarrowToWide (settings.name));

        for (const std::wstring & node : { id, TreeModel::kLinkCopyTag + id })
        {
            m_context.treePane.ReloadChildren (node);
        }
    }

    for (const std::wstring & node : { std::wstring (TreeModel::kCassoLinkRootId), TreeModel::kLinkCopyTag + std::wstring (TreeModel::kCassoLinkRootId) })
    {
        m_context.treePane.ReloadChildren (node);
    }

    //  A connection removed in another window has nothing to list. A drive of a
    //  machine that stopped answering has nothing either, so the list goes up
    //  to the machine, which has the reason.
    if (showing && IsConnectionRemoved (link))
    {
        m_context.addressBar.GoToLocation (Location::MakeRoot (TreeModel::kThisPcRootId));
        showing = false;
    }
    else if (showing && drive != 0 && !IsConnectionReplying (TextEncoding::WideToNarrow (link)))
    {
        m_context.addressBar.GoToLocation (Location::MakeRoot (TreeModel::MakeLinkId (link)));
    }

    showing = showing || m_context.browser.GetLocation().path == TreeModel::kCassoLinkRootId;

    //  The preview of a drive whose catalog was being read is drawn again
    //  with the catalog. A CHANGE TO ANY LINK CALLS THIS, NOT ONLY A CHANGE TO
    //  THE LINK THE LIST IS ON: a link with nothing to connect to changes state
    //  about once a second as it retries, so the list and the preview are
    //  redrawn only when the reload changed them, or each retry would scroll the
    //  preview to its top.
    if (showing)
    {
        std::vector<CatalogRow>  rows     = m_context.browser.GetRows();
        std::vector<int>         selected = m_context.browser.GetSelectedRows();
        std::wstring             message  = m_context.browser.GetListError();
        PreviewContent           preview  = m_context.browser.GetPreviewer().GetPreview();

        hr = m_context.browser.Reload (true);
        IGNORE_RETURN_VALUE (hr, S_OK);

        m_context.browser.GetPreviewer().UpdatePreview();

        //  Filling the list fills the preview as well.
        if (m_context.browser.GetRows() != rows || m_context.browser.GetSelectedRows() != selected || m_context.browser.GetListError() != message)
        {
            m_context.listPane.FillList();
        }
        else if (m_context.browser.GetPreviewer().GetPreview() != preview)
        {
            m_context.preview.FillPreview();
        }
    }

    m_context.window.Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkManager::IsConnectionRemoved
//
//  True when no connection, saved or found running, has the link's name.
//
////////////////////////////////////////////////////////////////////////////////

bool CassoLinkManager::IsConnectionRemoved (const std::wstring & link) const
{
    const std::vector<CassoLinkSettings> &  links = m_context.app.links->GetLinks();



    return std::none_of (links.begin(), links.end(), [&link] (const CassoLinkSettings & settings) { return TextEncoding::NarrowToWide (settings.name) == link; });
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkManager::HandleRunningChanged
//
//  A root whose Casso has exited is removed; when the list is on it, the
//  list goes to This PC.
//
//  The running Cassos read again, at start and when their folder changes.
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkManager::HandleRunningChanged()
{
    HRESULT                    hr      = S_OK;
    std::vector<RunningCasso>  running;
    std::wstring               link;
    int                        slot    = 0;
    int                        drive   = 0;
    bool                       showing = false;
    bool                       gone    = false;



    hr = m_context.app.running->Enumerate (running);
    CHR (hr);

    m_context.app.links->SetRunning (running);

    if (m_context.app.announced != nullptr)
    {
        m_announcedChanges = m_context.app.announced->GetChangeCount();
        m_context.app.links->SetAnnounced (m_context.app.announced->GetAnnounced());
    }

    showing = TreeModel::TryParseLinkId (m_context.browser.GetLocation().path, link, slot, drive);
    gone    = showing && IsConnectionRemoved (link);

    if (gone)
    {
        m_context.addressBar.GoToLocation (Location::MakeRoot (TreeModel::kThisPcRootId));
    }

    m_context.browser.GetTreeModel().SetLinks (m_context.app.links->BuildTreeEntries());
    m_context.treePane.RefreshTree();

Error:
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkManager::GetLatestActivity
//
//  The status bar's CassoLink line: the log's latest entry, for a quarter
//  of an hour after it. While that entry's connection waits for the Apple,
//  the line is "Waiting for the Apple" instead of an older entry such as
//  Disconnected.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CassoLinkManager::GetLatestActivity() const
{
    std::vector<CassoLinkConnections::Activity>    log;
    FILETIME                                       local   = {};
    FILETIME                                       then    = {};
    FILETIME                                       now     = {};
    ULARGE_INTEGER                                 a       = {};
    ULARGE_INTEGER                                 b       = {};
    CassoLinkSession                             * session = nullptr;
    LinkState                                      state   = LinkState::Closed;



    if (m_context.app.links == nullptr)
    {
        return std::wstring();
    }

    log = m_context.app.links->GetActivity();

    if (log.empty())
    {
        return std::wstring();
    }

    session = m_context.app.links->GetSession (TextEncoding::WideToNarrow (log.back().connection));
    state   = (session != nullptr) ? session->GetState() : LinkState::Closed;

    if (session != nullptr && !session->IsServingVirtualDrives()
        && (state == LinkState::Searching || state == LinkState::Opening || state == LinkState::WaitingForReady))
    {
        return std::format (L"CassoLink: {}: Waiting for the Apple", log.back().connection);
    }

    GetSystemTimeAsFileTime (&now);
    SystemTimeToFileTime (&log.back().time, &local);
    LocalFileTimeToFileTime (&local, &then);

    a.LowPart  = then.dwLowDateTime;
    a.HighPart = then.dwHighDateTime;
    b.LowPart  = now.dwLowDateTime;
    b.HighPart = now.dwHighDateTime;

    if (b.QuadPart > a.QuadPart + kActivityShownMinutes * 60ULL * 10'000'000ULL)
    {
        return std::wstring();
    }

    return std::format (L"CassoLink: {}: {}", log.back().connection, log.back().text);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkManager::FormatActivity
//
//  The log's last `lines` entries, or every one for 0, oldest first, each
//  with its time.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CassoLinkManager::FormatActivity (size_t lines) const
{
    std::vector<CassoLinkConnections::Activity>  log   = (m_context.app.links != nullptr) ? m_context.app.links->GetActivity() : std::vector<CassoLinkConnections::Activity>();
    size_t                                       first = (lines == 0 || log.size() <= lines) ? 0 : log.size() - lines;
    std::wstring                                 text;



    for (size_t i = first; i < log.size(); i++)
    {
        const CassoLinkConnections::Activity &  entry = log[i];

        text += std::format (L"{}{:04}-{:02}-{:02} {:02}:{:02}:{:02}  {}: {}", text.empty() ? L"" : L"\n",
                             entry.time.wYear, entry.time.wMonth, entry.time.wDay, entry.time.wHour, entry.time.wMinute, entry.time.wSecond,
                             entry.connection, entry.text);
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkManager::ShowActivityLog
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkManager::ShowActivityLog()
{
    CassoLinkConnections  * links = m_context.app.links;
    HRESULT                 hr    = S_OK;



    hr = CassoLinkActivityDialog::Show (m_context.window.GetHwnd(), m_context.theme, [links] () { return (links != nullptr) ? links->GetActivity() : std::vector<CassoLinkConnections::Activity>(); });
    IGNORE_RETURN_VALUE (hr, S_OK);
}
