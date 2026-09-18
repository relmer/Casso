#pragma once

#include "Pch.h"

#include "Ui/Dialogs/DialogDefinition.h"

class IDxuiTheme;





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueAbout
//
//  The About dialog: the version and copyright beside the cassowary, the name
//  explained as cask + Casso = Cassque in icons, and the same links Casso's
//  About box carries.
//
////////////////////////////////////////////////////////////////////////////////

class CassqueAbout
{
public:
    //  The three icons the body explains the name with. The body sets the
    //  size each is shown at; any left empty gives way to text.
    struct Pictures
    {
        DialogImage  cask;
        DialogImage  casso;
        DialogImage  cassque;
    };

    //  The dialog's text and pictures, the links included.
    static std::vector<DialogTextRun>  GetBody (const Pictures & pictures);

    //  Decodes one embedded PNG for the dialog, to be shown at displayDp.
    static HRESULT  LoadPicture (HINSTANCE instance, int resourceId, float displayDp, DialogImage & outImage);

    static void  Show (HWND owner, const IDxuiTheme * theme, HINSTANCE instance);

    static constexpr float            kHeaderPictureDp   = 128.0f;
    static constexpr float            kEquationPictureDp = 48.0f;
    static constexpr float            kRowPictureDp      = 32.0f;

    static constexpr const wchar_t *  kPhotoCreditUrl = L"https://bunyipco.blogspot.com/2015/04/cassowary-update.html";
    static constexpr const wchar_t *  kRepositoryUrl  = L"https://github.com/relmer/Casso";
    static constexpr const wchar_t *  kBugReportUrl   = L"https://github.com/relmer/Casso/issues/new?template=bug_report.yml";
    static constexpr const wchar_t *  kLicenseUrl     = L"https://github.com/relmer/Casso/blob/master/LICENSE";

private:
    static Pictures       LoadPictures   (HINSTANCE instance);
    static DialogImage    MakeSized      (const DialogImage & image, float displayDp);
    static DialogTextRun  MakeLeadingRun (const DialogImage & picture, const wchar_t * text);
    static DialogTextRun  MakeLink       (const std::wstring & text, const wchar_t * url);
};
