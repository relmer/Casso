#include "Pch.h"

#include "Video/MachineFrameRenderer.h"
#include "Machines/Apple2/Apple2e/Apple2eSoftSwitchBank.h"
#include "Machines/Apple2/Common/Apple80ColTextMode.h"
#include "Machines/Apple2/Common/AppleDoubleHiResMode.h"
#include "Machines/Apple2/Common/AppleHiResMode.h"
#include "Machines/Apple2/Common/AppleTextMode.h"
#include "Shell/MachineBuilder.h"
#include "Shell/MachineHost.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MachineFrameRenderer::Render
//
//  The same steps the emulator takes for its own frame: text in white, the
//  graphics modes decoding for color, the mode chosen from the soft
//  switches, every text row drawn afresh, and the mode rendered through the
//  bus so the MMU's banking decides which memory it reads.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MachineFrameRenderer::Render (
    MachineHost            & machine,
    MachineBuilder         & builder,
    bool                     flashOn,
    std::vector<uint32_t>  & outBgra)
{
    HRESULT                   hr       = S_OK;
    MachineRefs             & refs     = machine.GetRefs();
    const SoftSwitchMirror  & mirror   = machine.GetSoftSwitchMirror();
    bool                      hasModes = refs.text40 != nullptr && refs.text80 != nullptr && refs.hiRes != nullptr && refs.doubleHiRes != nullptr;
    bool                      use80Col = false;



    outBgra.assign ((size_t) kWidth * kHeight, kBlackArgb);

    CBRA (hasModes);

    refs.text40->SetOnColor    (kWhiteArgb);
    refs.text40->SetFlashState (flashOn);
    refs.text80->SetOnColor    (kWhiteArgb);
    refs.text80->SetFlashState (flashOn);

    refs.hiRes->SetMonochrome       (false);
    refs.doubleHiRes->SetMonochrome (false);

    builder.SelectVideoMode();

    refs.text40->InvalidateCache();
    refs.text80->InvalidateCache();

    if (refs.activeVideoMode != nullptr)
    {
        refs.activeVideoMode->Render (nullptr, outBgra.data(), kWidth, kHeight);
    }

    if (mirror.mixedMode && mirror.graphicsMode)
    {
        use80Col = refs.iieSoftSwitches != nullptr && refs.iieSoftSwitches->Is80ColMode();

        if (use80Col)
        {
            refs.text80->SetPage2       (false);
            refs.text80->RenderRowRange (kMixedFirstRow, kMixedLastRow, nullptr, outBgra.data(), kWidth, kHeight);
        }
        else
        {
            refs.text40->SetPage2       (mirror.page2);
            refs.text40->RenderRowRange (kMixedFirstRow, kMixedLastRow, nullptr, outBgra.data(), kWidth, kHeight);
        }
    }

Error:
    return hr;
}
