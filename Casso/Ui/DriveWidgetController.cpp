#include "Pch.h"

#include "DriveWidgetController.h"

#include "DriveWidgetElement.h"







namespace
{
    // File-scope static so it outlives every theme reload + the Rml
    // Factory holds onto it for the lifetime of the process. Allocated
    // in `RegisterInstancer` on first call and never freed (RmlUi's
    // Shutdown handles factory tear-down).
    Rml::ElementInstancerGeneric<DriveWidgetElement>  * s_pDriveInstancer = nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DriveWidgetController
//
////////////////////////////////////////////////////////////////////////////////

DriveWidgetController::DriveWidgetController()
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ~DriveWidgetController
//
////////////////////////////////////////////////////////////////////////////////

DriveWidgetController::~DriveWidgetController()
{
    UnloadDocument();
}





////////////////////////////////////////////////////////////////////////////////
//
//  RegisterInstancer
//
//  Idempotent. Registers `<drive-widget>` with the Rml::Factory before any
//  document containing the tag is loaded.
//
////////////////////////////////////////////////////////////////////////////////

void DriveWidgetController::RegisterInstancer()
{
    if (s_pDriveInstancer == nullptr)
    {
        s_pDriveInstancer = new Rml::ElementInstancerGeneric<DriveWidgetElement>();
    }

    m_pInstancer = s_pDriveInstancer;

    Rml::Factory::RegisterElementInstancer ("drive-widget", s_pDriveInstancer);
}





////////////////////////////////////////////////////////////////////////////////
//
//  LoadDocument
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DriveWidgetController::LoadDocument (
    Rml::Context        * pContext,
    const std::string   & rmlPath,
    IDriveCommandSink   * pSink,
    HWND                  ownerHwnd)
{
    HRESULT  hr = S_OK;



    CBRAEx (pContext, E_INVALIDARG);

    UnloadDocument();

    m_pContext = pContext;
    m_pDoc     = pContext->LoadDocument (rmlPath);

    if (m_pDoc == nullptr)
    {
        // Theme is missing or its drive_widgets.rml failed to parse.
        // Non-asserting: the rest of the chrome still renders.
        hr = E_FAIL;
        goto Error;
    }

    CollectWidgets();

    for (DriveWidgetElement * pW : m_widgets)
    {
        pW->SetCommandSink (pSink);
        pW->SetOwnerHwnd   (ownerHwnd);
    }

    m_pDoc->Show();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UnloadDocument
//
////////////////////////////////////////////////////////////////////////////////

void DriveWidgetController::UnloadDocument()
{
    m_widgets.clear();

    if (m_pDoc != nullptr && m_pContext != nullptr)
    {
        m_pContext->UnloadDocument (m_pDoc);
    }

    m_pDoc     = nullptr;
    m_pContext = nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CollectWidgets
//
//  Recursive descent. RmlUi's QuerySelectorAll would also work but
//  iterating by hand keeps us decoupled from the selector parser and
//  gives us a clear typing seam (dynamic_cast to DriveWidgetElement *).
//
////////////////////////////////////////////////////////////////////////////////

namespace
{
    void CollectWidgetsRecursive (Rml::Element * pNode,
                                  std::vector<DriveWidgetElement *> & outList)
    {
        DriveWidgetElement *  pWidget = nullptr;
        int                   childCount = 0;
        int                   i          = 0;

        if (pNode == nullptr)
        {
            return;
        }

        pWidget = dynamic_cast<DriveWidgetElement *> (pNode);

        if (pWidget != nullptr)
        {
            if (outList.size() < DriveWidgetController::kMaxWidgets)
            {
                outList.push_back (pWidget);
            }
        }

        childCount = pNode->GetNumChildren();

        for (i = 0; i < childCount; i++)
        {
            CollectWidgetsRecursive (pNode->GetChild (i), outList);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CollectWidgets
//
////////////////////////////////////////////////////////////////////////////////

void DriveWidgetController::CollectWidgets()
{
    m_widgets.clear();

    if (m_pDoc == nullptr)
    {
        return;
    }

    CollectWidgetsRecursive (m_pDoc, m_widgets);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SyncFromStates
//
//  Push state[d] into the widget that declares (slot = 6, drive = d).
//  Widgets without a matching state entry are left alone.
//
////////////////////////////////////////////////////////////////////////////////

void DriveWidgetController::SyncFromStates (const std::array<DriveWidgetState, 2> & states)
{
    for (DriveWidgetElement * pW : m_widgets)
    {
        int  drive = pW->GetDriveAttr();

        if (drive < 0 || drive >= static_cast<int> (states.size()))
        {
            continue;
        }

        pW->SyncFromState (states[drive]);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HitTest
//
//  Geometric hit-test in document coordinates. Returns the first cached
//  widget whose `IsPointWithinElement` accepts the point.
//
////////////////////////////////////////////////////////////////////////////////

DriveWidgetElement *  DriveWidgetController::HitTest (int clientX, int clientY) const
{
    Rml::Vector2f  pt (static_cast<float> (clientX), static_cast<float> (clientY));

    for (DriveWidgetElement * pW : m_widgets)
    {
        if (pW->IsPointWithinElement (pt))
        {
            return pW;
        }
    }

    return nullptr;
}
