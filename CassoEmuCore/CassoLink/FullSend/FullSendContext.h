#pragma once

#include "Pch.h"

#include "CassoLink/FullSend/FullSendBootResults.h"
#include "CassoLink/FullSend/FullSendFolder.h"
#include "CassoLink/FullSend/FullSendQuips.h"
#include "CassoLink/FullSend/FullSendScreen.h"

class FullSendStatus;
class IFullSendLink;





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendContext
//
//  What the steps of one Full Send share: the Apple, the folder, the Full
//  Send window's status, the Apple's screen and the boot checks' results.
//
////////////////////////////////////////////////////////////////////////////////

class FullSendContext
{
public:
    FullSendContext (IFullSendLink & link, FullSendStatus & status, FullSendFolder & folder);

    HRESULT  ShowScreen      ();
    void     Report          (const std::string & text);
    HRESULT  WriteManifest   ();
    HRESULT  FindMovedImages (const FullSendFolder::RowMatch & matches);

    IFullSendLink        & link;
    FullSendStatus       & status;
    FullSendFolder       & folder;
    FullSendScreen         screen;
    FullSendQuips          quips;
    FullSendBootResults    boots;
};
