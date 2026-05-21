#pragma once

#include "Pch.h"

#include "DriveWidgetElement.h"
#include "DriveWidgetState.h"
#include "IDriveCommandSink.h"

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/ElementInstancer.h>

#include <array>
#include <vector>





////////////////////////////////////////////////////////////////////////////////
//
//  DriveWidgetController
//
//  Owns the lifetime of the `<drive-widget>` element instancer + the
//  active drive_widgets.rml document. Acts as the host-side adapter
//  between `EmulatorShell` (which owns the per-drive `DriveWidgetState`
//  and implements `IDriveCommandSink`) and the RmlUi document tree
//  (where the actual element instances live).
//
//  Lifecycle
//  ---------
//      RegisterInstancer()
//          -- exactly once before any drive_widgets.rml document is
//             loaded. Idempotent; safe to call across theme reloads.
//      LoadDocument(context, rmlPath, sink, ownerHwnd)
//          -- unloads any prior document, loads `rmlPath`, walks the
//             tree for every <drive-widget> element, installs the
//             command sink + owner HWND, and caches the pointers.
//      UnloadDocument()
//          -- detaches the cached pointers + unloads the document.
//      SyncFromStates(states)
//          -- pushes per-drive state into each cached widget. Called
//             once per UI frame.
//      HitTest(clientX, clientY)
//          -- returns the topmost cached drive widget whose absolute
//             box contains the point, or nullptr.
//
//  Slot/drive identification
//  -------------------------
//  Each <drive-widget> RML tag carries `slot` + `drive` integer
//  attributes. The controller only cares about slot 6 + drive 0/1; any
//  widget that doesn't identify itself with valid slot/drive ints is
//  still tracked for hit-test purposes but is excluded from state sync.
//
////////////////////////////////////////////////////////////////////////////////

class DriveWidgetController
{
public:
    static constexpr int  kMaxWidgets = 4;  // headroom for >2-drive future configs


    DriveWidgetController();
    ~DriveWidgetController();

    // Idempotent. Registers `<drive-widget>` with the Rml::Factory.
    // Must be called after `Rml::Initialise()` and before any document
    // containing the tag is loaded.
    void  RegisterInstancer();

    HRESULT  LoadDocument (
        Rml::Context        * pContext,
        const std::string   & rmlPath,
        IDriveCommandSink   * pSink,
        HWND                  ownerHwnd);

    void     UnloadDocument();

    // Push state[d] into the widget that declares (slot=6, drive=d).
    // Widgets without a state entry (e.g. slot/drive out of range) are
    // left alone.
    void     SyncFromStates (const std::array<DriveWidgetState, 2> & states);

    // Geometric hit-test in document (client) coordinates. Walks the
    // cached widgets; returns the first one whose `IsPointWithinElement`
    // accepts the point, or nullptr.
    DriveWidgetElement *  HitTest (int clientX, int clientY) const;

    // Diagnostic accessors -- exposed for tests + sanity logging.
    size_t  GetWidgetCount() const { return m_widgets.size(); }

private:
    void     CollectWidgets();

    Rml::ElementInstancer        * m_pInstancer = nullptr;   // owned static singleton
    Rml::Context                 * m_pContext   = nullptr;
    Rml::ElementDocument         * m_pDoc       = nullptr;

    std::vector<DriveWidgetElement *>  m_widgets;
};
