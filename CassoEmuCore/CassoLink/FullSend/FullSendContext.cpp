#include "Pch.h"

#include "CassoLink/FullSend/FullSendContext.h"

#include "CassoLink/FullSend/FullSendLink.h"
#include "CassoLink/FullSend/FullSendStatus.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendContext::FullSendContext
//
////////////////////////////////////////////////////////////////////////////////

FullSendContext::FullSendContext (IFullSendLink & link, FullSendStatus & status, FullSendFolder & folder) :
    link   (link),
    status (status),
    folder (folder),
    boots  (status, folder.GetManifest())
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendContext::ShowScreen
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FullSendContext::ShowScreen()
{
    HRESULT  hr = S_OK;



    hr = link.ShowScreen (screen);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendContext::Report
//
//  A line for the PC's activity log and the Full Send window's list.
//
////////////////////////////////////////////////////////////////////////////////

void FullSendContext::Report (const std::string & text)
{
    link.Report (text);
    status.AddLine (text);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendContext::WriteManifest
//
//  The boot results that arrived are set in the manifest first, so a write
//  never puts back a row the booting thread updated in the file.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FullSendContext::WriteManifest()
{
    HRESULT  hr = S_OK;



    boots.Apply();

    hr = folder.WriteManifest();
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendContext::FindMovedImages
//
//  Each renamed image found is reported, and the manifest is written with
//  its new name.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FullSendContext::FindMovedImages (const FullSendFolder::RowMatch & matches)
{
    HRESULT                  hr         = S_OK;
    FullSendFolder::Renames  renames;
    bool                     hasRenames = false;



    hr = folder.FindMovedImages (matches, renames);
    CHR (hr);

    for (const auto & [old, renamed] : renames)
    {
        Report (std::format ("{} was renamed {}, and its row in the manifest is updated", old, renamed));
    }

    hasRenames = !renames.empty();
    BAIL_OUT_IF (!hasRenames, S_OK);

    hr = WriteManifest();
    CHR (hr);

Error:
    return hr;
}
