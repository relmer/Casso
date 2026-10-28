#pragma once

#include "Pch.h"

#include "Ui/Scene/DeskScene.h"

class SettingsSheet;





////////////////////////////////////////////////////////////////////////////////
//
//  SettingsPreviewScene
//
//  The theme preview's desk scene. The shell's scene is on the emulator
//  window's device, and the settings sheet is a separate window with a
//  separate device. So the models load a second time here, which is cheap
//  (a few thousand triangles) next to showing the user a presentation the
//  app no longer uses.
//
//  Drawn from inside the sheet's compose hook rather than an after-paint
//  hook, because a window that composes never runs the after-paint hook at
//  all: the compose hook owns the back buffer, and right after Compose is
//  where the finished 2D frame is.
//
////////////////////////////////////////////////////////////////////////////////

class SettingsPreviewScene
{
public:
    explicit SettingsPreviewScene (SettingsSheet & sheet) : m_sheet (sheet) {}

    void  Render (ID3D11RenderTargetView * rtv, int widthPx, int heightPx);

private:
    HRESULT  LoadModels ();

    SettingsSheet  & m_sheet;
    DeskScene        m_previewScene;
    bool             m_previewSceneReady = false;
    bool             m_previewSceneTried = false;
    bool             m_previewSceneIsC   = false;
};
