#include "Pch.h"

#include "Ui/DiskInspector/CompareDialog.h"
#include "Seams/Win32HostDialogs.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CompareBody
//
//  A's sources on the left and B's on the right, each under its title, then
//  the error line.
//
////////////////////////////////////////////////////////////////////////////////

class CompareBody : public DxuiPanel
{
public:
    void  Init (DxuiRadioGroup & a, DxuiRadioGroup & b, DxuiLabel & titleA, DxuiLabel & titleB, DxuiLabel & error, const vector<ComparisonSource> & choices)
    {
        m_a       = &a;
        m_b       = &b;
        m_titleA  = &titleA;
        m_titleB  = &titleB;
        m_error   = &error;
        m_choices = &choices;

        Adopt (a);
        Adopt (b);
        Adopt (titleA);
        Adopt (titleB);
        Adopt (error);
    }

    void  Layout (const RECT & boundsPx, const DxuiDpiScaler & scaler) override
    {
        int  pad  = scaler.ToPx (kPadDip);
        int  rowH = scaler.ToPx (kRowDip);
        int  w    = (boundsPx.right - boundsPx.left - 3 * pad) / 2;
        int  x    = boundsPx.left + pad;
        int  y    = boundsPx.top  + pad;
        int  rows = static_cast<int> (m_choices->size());



        SetBounds (boundsPx);

        m_titleA->Layout (RECT { x,           y, x + w,           y + rowH }, scaler);
        m_titleB->Layout (RECT { x + w + pad, y, x + 2 * w + pad, y + rowH }, scaler);
        y += rowH;

        LayoutColumn (*m_a, x,           y, w, rowH, scaler);
        LayoutColumn (*m_b, x + w + pad, y, w, rowH, scaler);
        y += rows * rowH + pad;

        m_error->Layout (RECT { x, y, boundsPx.right - pad, y + 2 * rowH }, scaler);
    }

    bool  OnMouse (const DxuiMouseEvent & ev) override
    {
        return m_a->OnMouse (ev) || m_b->OnMouse (ev);
    }

private:
    static constexpr int  kPadDip = 12;
    static constexpr int  kRowDip = 26;

    void  LayoutColumn (DxuiRadioGroup & group, int x, int y, int w, int rowH, const DxuiDpiScaler & scaler)
    {
        int                           sel     = group.GetSelected();
        int                           i       = 0;
        std::vector<DxuiRadioOption>  options;



        for (const ComparisonSource & source : *m_choices)
        {
            options.push_back ({ RECT { x, y + i * rowH, x + w, y + (i + 1) * rowH - 2 },
                                 source.kind == ComparisonSourceKind::ImageFile ? std::wstring (L"Image file...") : ComparisonText::FormatSource (source) });
            i++;
        }

        group.SetOptions  (std::move (options));
        group.SetSelected (sel);
        group.Layout (RECT { x, y, x + w, y + i * rowH }, scaler);
    }

    DxuiRadioGroup *                  m_a       = nullptr;
    DxuiRadioGroup *                  m_b       = nullptr;
    DxuiLabel *                       m_titleA  = nullptr;
    DxuiLabel *                       m_titleB  = nullptr;
    DxuiLabel *                       m_error   = nullptr;
    const vector<ComparisonSource> *  m_choices = nullptr;
};





////////////////////////////////////////////////////////////////////////////////
//
//  CompareDialog::BuildSources
//
//  Each drive's three sources in drive order, then an image file.
//
////////////////////////////////////////////////////////////////////////////////

vector<ComparisonSource> CompareDialog::BuildSources (int driveCount)
{
    vector<ComparisonSource>  sources;
    int                       drive   = 0;



    for (drive = 0; drive < driveCount; drive++)
    {
        for (ComparisonSourceKind kind : { ComparisonSourceKind::DriveNow, ComparisonSourceKind::AsInserted, ComparisonSourceKind::ItsFile })
        {
            sources.push_back ({ kind, drive });
        }
    }

    sources.push_back ({ ComparisonSourceKind::ImageFile, 0 });

    return sources;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CompareDialog::Configure
//
////////////////////////////////////////////////////////////////////////////////

void CompareDialog::Configure (const IDxuiTheme * theme, int driveCount, const ComparisonSource & a, const ComparisonSource & b)
{
    m_theme   = theme;
    m_choices = BuildSources (driveCount);
    m_sources = { a, b };
}





////////////////////////////////////////////////////////////////////////////////
//
//  CompareDialog::OnCreate
//
////////////////////////////////////////////////////////////////////////////////

void CompareDialog::OnCreate()
{
    CompareBody *  body = nullptr;
    DxuiButton *   ok   = nullptr;
    int            side = 0;



    for (DxuiRadioGroup * group : { &m_sideA, &m_sideB })
    {
        ComparisonSource  wanted = m_sources[side];
        auto              found  = std::find_if (m_choices.begin(), m_choices.end(), [&wanted] (const ComparisonSource & s)
                                                 {
                                                     return s.kind == wanted.kind && (s.kind == ComparisonSourceKind::ImageFile || s.drive == wanted.drive);
                                                 });

        group->SetDpi      (GetDpi());
        group->SetSelected (static_cast<int> (found - m_choices.begin()) % static_cast<int> (m_choices.size()));
        side++;
    }

    m_titleA.SetText      (L"A");
    m_titleB.SetText      (L"B");
    m_titleA.SetTextRole  (DxuiTextRole::Heading);
    m_titleB.SetTextRole  (DxuiTextRole::Heading);
    m_error.SetTextRole   (DxuiTextRole::Error);
    m_error.SetTextAlign  (DxuiTextHAlign::Left, DxuiTextVAlign::Top);

    body = CreateDialogContent<CompareBody>();
    body->Init (m_sideA, m_sideB, m_titleA, m_titleB, m_error, m_choices);

    ok = AddDialogButton (L"Compare", IDOK);
    AddDialogButton (L"Cancel", IDCANCEL);

    ok->SetOnClick ([this] () { OnOkClicked(); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  CompareDialog::OnOkClicked
//
//  An image file on either side is asked for now; backing out of the
//  picker leaves the dialog open.
//
////////////////////////////////////////////////////////////////////////////////

void CompareDialog::OnOkClicked()
{
    std::wstring  error;
    bool          isPicked = true;



    m_sources[0] = m_choices[std::clamp (m_sideA.GetSelected(), 0, static_cast<int> (m_choices.size()) - 1)];
    m_sources[1] = m_choices[std::clamp (m_sideB.GetSelected(), 0, static_cast<int> (m_choices.size()) - 1)];

    if (m_sources[0] == m_sources[1] && m_sources[0].kind != ComparisonSourceKind::ImageFile)
    {
        error = L"Choose a different source for A and for B.";
    }

    for (ComparisonSource & source : m_sources)
    {
        if (error.empty() && isPicked && source.kind == ComparisonSourceKind::ImageFile)
        {
            isPicked = PickImage (source);
        }
    }

    if (!error.empty())
    {
        m_error.SetText (error);
        Invalidate();
    }
    else if (isPicked)
    {
        m_isChosen = true;
        EndDialog (IDOK);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CompareDialog::PickImage
//
////////////////////////////////////////////////////////////////////////////////

bool CompareDialog::PickImage (ComparisonSource & inOut)
{
    HRESULT                hr       = S_OK;
    Win32HostDialogs       dialogs;
    FileDialogSpec         spec;
    std::filesystem::path  chosen;
    bool                   isPicked = false;



    spec.filters = { { L"Disk images", L"*.dsk;*.do;*.po;*.nib;*.nb2;*.woz" },
                     { L"All files",   L"*.*" } };

    hr = dialogs.PickFileToOpen (GetHwnd(), spec, chosen, isPicked);
    CHR (hr);

    inOut.path = isPicked ? chosen.wstring() : inOut.path;

Error:
    return SUCCEEDED (hr) && isPicked;
}
