#include "Pch.h"

#include "DriveWidgetElement.h"

#include "LedElement.h"








////////////////////////////////////////////////////////////////////////////////
//
//  DriveWidgetElement
//
////////////////////////////////////////////////////////////////////////////////

DriveWidgetElement::DriveWidgetElement (const Rml::String & tag)
    : Rml::Element (tag)
{
}



DriveWidgetElement::~DriveWidgetElement()
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetSlotAttr
//
//  Read the slot attribute from the RML tag. Returns -1 if it is missing
//  or invalid.
//
////////////////////////////////////////////////////////////////////////////////

int DriveWidgetElement::GetSlotAttr() const
{
    return GetAttribute<int> ("slot", -1);
}



////////////////////////////////////////////////////////////////////////////////
//
//  GetDriveAttr
//
//  Read the drive attribute from the RML tag. Returns -1 if it is missing
//  or invalid.
//
////////////////////////////////////////////////////////////////////////////////

int DriveWidgetElement::GetDriveAttr() const
{
    return GetAttribute<int> ("drive", -1);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProcessDefaultAction
//
//  Click routing: body clicks open the file dialog; eject-btn child clicks
//  dispatch `IDriveCommandSink::Eject`.
//
////////////////////////////////////////////////////////////////////////////////

void DriveWidgetElement::ProcessDefaultAction (Rml::Event & event)
{
    Rml::Element  * target       = nullptr;
    Rml::Element  * walker       = nullptr;
    bool            fOnEjectBtn  = false;

    Rml::Element::ProcessDefaultAction (event);

    if (event.GetType() != "click")
    {
        return;
    }

    target = event.GetTargetElement();

    for (walker = target;
         walker != nullptr && walker != this;
         walker = walker->GetParentNode())
    {
        if (walker->IsClassSet ("eject-btn"))
        {
            fOnEjectBtn = true;
            break;
        }
    }

    if (fOnEjectBtn)
    {
        if (m_pSink != nullptr)
        {
            int  slot  = GetSlotAttr();
            int  drive = GetDriveAttr();

            if (slot >= 0 && drive >= 0)
            {
                m_pSink->Eject (slot, drive);
            }
        }
        return;
    }

    OpenAndMount();
}





////////////////////////////////////////////////////////////////////////////////
//
//  OpenAndMount
//
////////////////////////////////////////////////////////////////////////////////

void DriveWidgetElement::OpenAndMount()
{
    std::wstring  path;
    int           slot  = 0;
    int           drive = 0;

    if (m_pSink == nullptr)
    {
        return;
    }

    if (!ShowFileOpenDialog (path))
    {
        // User cancelled, or COM/IFileDialog refused. Both are
        // non-asserting outcomes -- swallow silently.
        return;
    }

    slot  = GetSlotAttr();
    drive = GetDriveAttr();

    if (slot < 0 || drive < 0)
    {
        return;
    }

    HRESULT  hr = m_pSink->Mount (slot, drive, path);
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShowFileOpenDialog
//
//  IFileOpenDialog filtered to the four supported disk image extensions
//  (FR-022b). The UI thread is OleInitialized in wWinMain so COM is
//  already up; we still call CoCreateInstance + Release locally so the
//  dialog has no lifetime entanglement with the rest of the shell.
//
//  Non-asserting throughout -- the user can legitimately cancel, and a
//  COM failure here only means "no mount happens", not "the emulator
//  is broken".
//
////////////////////////////////////////////////////////////////////////////////

bool DriveWidgetElement::ShowFileOpenDialog (std::wstring & outPath) const
{
    HRESULT                       hr        = S_OK;
    IFileOpenDialog             * pDialog   = nullptr;
    IShellItem                  * pItem     = nullptr;
    PWSTR                         pwszPath  = nullptr;
    bool                          fOk       = false;

    const COMDLG_FILTERSPEC kFilters[] =
    {
        { L"Disk Images (*.dsk;*.nib;*.woz;*.po)", L"*.dsk;*.nib;*.woz;*.po" },
        { L"All Files (*.*)",                       L"*.*"                    },
    };

    hr = CoCreateInstance (CLSID_FileOpenDialog,
                           nullptr,
                           CLSCTX_INPROC_SERVER,
                           IID_PPV_ARGS (&pDialog));

    if (FAILED (hr) || pDialog == nullptr)
    {
        goto Cleanup;
    }

    hr = pDialog->SetFileTypes (static_cast<UINT> (std::size (kFilters)), kFilters);
    if (FAILED (hr))
    {
        goto Cleanup;
    }

    hr = pDialog->SetTitle (L"Insert Disk");
    if (FAILED (hr))
    {
        goto Cleanup;
    }

    hr = pDialog->Show (m_ownerHwnd);
    if (FAILED (hr))
    {
        // S_FALSE / cancelled -- normal user outcome.
        goto Cleanup;
    }

    hr = pDialog->GetResult (&pItem);
    if (FAILED (hr) || pItem == nullptr)
    {
        goto Cleanup;
    }

    hr = pItem->GetDisplayName (SIGDN_FILESYSPATH, &pwszPath);
    if (FAILED (hr) || pwszPath == nullptr)
    {
        goto Cleanup;
    }

    outPath = pwszPath;
    fOk     = true;

Cleanup:
    if (pwszPath != nullptr)
    {
        CoTaskMemFree (pwszPath);
    }

    if (pItem != nullptr)
    {
        pItem->Release();
    }

    if (pDialog != nullptr)
    {
        pDialog->Release();
    }

    return fOk;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HandleDroppedFile
//
//  Drag-drop entry point after DragDropTarget confirms a supported file.
//  Returns S_OK on successful dispatch to the sink.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DriveWidgetElement::HandleDroppedFile (const std::wstring & path)
{
    HRESULT  hr    = S_OK;
    int      slot  = 0;
    int      drive = 0;

    if (m_pSink == nullptr)
    {
        hr = S_FALSE;
        goto Error;
    }

    slot  = GetSlotAttr();
    drive = GetDriveAttr();

    if (slot < 0 || drive < 0)
    {
        hr = E_INVALIDARG;
        goto Error;
    }

    hr = m_pSink->Mount (slot, drive, path);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplyDoorClasses
//
////////////////////////////////////////////////////////////////////////////////

void DriveWidgetElement::ApplyDoorClasses (DriveWidgetState::Door door)
{
    const bool  fOpen   = (door == DriveWidgetState::Door::Open ||
                           door == DriveWidgetState::Door::Opening);
    const bool  fClosed = !fOpen;
    const bool  fMoving = (door == DriveWidgetState::Door::Opening ||
                           door == DriveWidgetState::Door::Closing);

    // The door class lives on a child element if present; falling back
    // to the widget itself if the theme didn't ship a `.door` child.
    Rml::Element *  pDoor = QuerySelector (".door");

    if (pDoor == nullptr)
    {
        pDoor = this;
    }

    pDoor->SetClass ("door-open",    fOpen);
    pDoor->SetClass ("door-closed",  fClosed);
    pDoor->SetClass ("door-moving",  fMoving);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SyncFromState
//
//  Push the current state into the element. Reads the atomic motor/active
//  flags and the UI-only door / mounted fields, then skips redundant Rml
//  mutations when nothing changed.
//
////////////////////////////////////////////////////////////////////////////////

void DriveWidgetElement::SyncFromState (const DriveWidgetState & state)
{
    const bool  fMounted  = state.IsMounted();
    const bool  fMotorOn  = state.motorOn.load (std::memory_order_relaxed);
    const bool  fActive   = state.diskActive.load (std::memory_order_relaxed);

    // is-mounted / is-empty on the widget itself.
    if (fMounted != m_lastMounted)
    {
        SetClass ("is-mounted", fMounted);
        SetClass ("is-empty",   !fMounted);
        m_lastMounted = fMounted;
    }

    // Spinning -- prefer a `.disk-graphic` child if the theme ships one,
    // otherwise apply to the widget itself.
    {
        Rml::Element *  pDisk = QuerySelector (".disk-graphic");

        if (pDisk == nullptr)
        {
            pDisk = this;
        }

        if (fMotorOn != m_lastSpinning)
        {
            pDisk->SetClass ("is-spinning", fMotorOn);
            m_lastSpinning = fMotorOn;
        }
    }

    // Door FSM
    if (state.doorState != m_lastDoor)
    {
        ApplyDoorClasses (state.doorState);
        m_lastDoor = state.doorState;
    }

    // LED state: Idle (empty) -> Present (mounted, motor off) ->
    // Active (motor on, or actively reading/writing).
    LedElement::State  ledState = LedElement::State::Idle;

    if (fActive || fMotorOn)
    {
        ledState = LedElement::State::Active;
    }
    else if (fMounted)
    {
        ledState = LedElement::State::Present;
    }

    int  ledInt = static_cast<int> (ledState);

    if (ledInt != m_lastLed)
    {
        Rml::Element *  pLed = QuerySelector (".led");
        LedElement::SetState (pLed, ledState);
        m_lastLed = ledInt;
    }
}
