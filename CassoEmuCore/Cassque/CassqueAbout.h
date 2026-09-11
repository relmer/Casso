#pragma once

#include "Pch.h"

#include "Ui/Dialogs/DialogDefinition.h"

class IDxuiTheme;





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueAbout
//
//  The About dialog: what the name means, the version, and the cask and casque
//  the icon shows, the casque credited as Casso credits its cassowary.
//
////////////////////////////////////////////////////////////////////////////////

class CassqueAbout
{
public:
    //  The dialog's text, the credit line's link included.
    static std::vector<DialogTextRun>  GetBody();

    //  Decodes the embedded cask and casque picture for the dialog.
    static HRESULT  LoadPicture (HINSTANCE instance, DialogImage & outImage);

    static void  Show (HWND owner, const IDxuiTheme * theme, HINSTANCE instance);

    static constexpr float  kPictureDp = 160.0f;
    static constexpr const wchar_t *  kPhotoCreditUrl = L"https://bunyipco.blogspot.com/2015/04/cassowary-update.html";
};
