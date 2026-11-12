#include "Pch.h"

#include "Ui/DiskInspector/GoToDialog.h"

#include "Devices/Disk/Inspector/InspectorFormat.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"





struct GoToKindTip
{
    GoToKind  kind;
    LPCWSTR   pszText;
};

static constexpr int          s_kMaxPathChars = 64;
static constexpr GoToKindTip  s_kKindTips[GoToDialog::kTipCount] =
{
    { GoToKind::PhysicalSector, L"The number in a sector's address field, counting from 0.\nA standard disk has them in order around the track." },
    { GoToKind::DosOrderSector, L"The sector's index in DOS's order, the number DOS uses to read and write it." },
    { GoToKind::ProDosBlock,    L"A 512-byte block, whether or not ProDOS is on the disk." },
};





////////////////////////////////////////////////////////////////////////////////
//
//  GoToBody
//
//  The kinds of target in two columns, then the entry row, a line on what
//  the entry may be for the kind chosen, and the error line. A sector, an
//  offset or a cell is entered with its track: "Track [17] Sector [F]". A track
//  is entered in the same Track field, so it stays through a change of kind.
//
////////////////////////////////////////////////////////////////////////////////

class GoToBody : public DxuiPanel
{
public:
    struct Parts
    {
        const vector<GoToKind> * kindList   = nullptr;
        DxuiRadioGroup         * kinds      = nullptr;
        DxuiInfoTip            * tips       = nullptr;
        IDxuiTextRenderer      * text       = nullptr;
        DxuiLabel              * trackLabel = nullptr;
        DxuiTextInput          * track      = nullptr;
        DxuiLabel              * entryLabel = nullptr;
        DxuiTextInput          * input      = nullptr;
        DxuiComboBox           * files      = nullptr;
        DxuiLabel              * hint       = nullptr;
        DxuiLabel              * error      = nullptr;
    };

    void  Init (const Parts & parts, int initial, std::function<void (IDxuiControl *)> onPress)
    {
        m_parts   = parts;
        m_initial = initial;
        m_onPress = std::move (onPress);

        Adopt (*parts.kinds);

        for (int i = 0; i < GoToDialog::kTipCount; i++)
        {
            Adopt (parts.tips[i]);
        }

        Adopt (*parts.trackLabel);
        Adopt (*parts.track);
        Adopt (*parts.entryLabel);
        Adopt (*parts.input);
        Adopt (*parts.files);
        Adopt (*parts.hint);
        Adopt (*parts.error);
    }

    void  Layout (const RECT & boundsPx, const DxuiDpiScaler & scaler) override
    {
        int                           pad     = scaler.ToPx (kPadDip);
        int                           rowH    = scaler.ToPx (kRowDip);
        int                           labelW  = scaler.ToPx (kLabelDip);
        int                           half    = (boundsPx.right - boundsPx.left - 2 * pad) / 2;
        int                           x       = boundsPx.left + pad;
        int                           y       = boundsPx.top  + pad;
        int                           count   = static_cast<int> (m_parts.kindList->size());
        int                           rows    = (count + 1) / 2;
        int                           sel     = (m_parts.kinds->GetSelected() >= 0) ? m_parts.kinds->GetSelected() : m_initial;
        GoToKind                      kind    = (*m_parts.kindList)[static_cast<size_t> (sel)];
        bool                          isOn    = InspectorGoTo::IsOnTrack (kind);
        bool                          isFile  = kind == GoToKind::File;
        bool                          isTrack = kind == GoToKind::Track;
        int                           fieldW  = (boundsPx.right - boundsPx.left - 3 * pad - 2 * labelW) / 2;
        int                           second  = x + half;



        SetBounds (boundsPx);
        m_boundsPx = boundsPx;
        m_scaler   = scaler;

        //  The gap between the columns is centered: the second column starts
        //  as far right of the center as the first column's widest option
        //  ends left of it.
        SetKindOptions (x, second, y, rows, sel, boundsPx, scaler);

        if (m_parts.text != nullptr && count > rows)
        {
            int  firstEnd = x + MeasureColumn (0, rows, scaler);

            second = std::max (firstEnd + pad, static_cast<int> (boundsPx.left + boundsPx.right) - firstEnd);
            SetKindOptions (x, second, y, rows, sel, boundsPx, scaler);
        }
        LayoutTips (scaler);
        y += rows * rowH + pad;

        m_parts.trackLabel->SetVisible (isOn || isTrack);
        m_parts.track->SetVisible      (isOn || isTrack);
        m_parts.entryLabel->SetVisible (!isTrack);
        m_parts.input->SetVisible      (!isFile && !isTrack);
        m_parts.files->SetVisible      (isFile);

        if (isOn || isTrack)
        {
            m_parts.trackLabel->Layout (RECT { x, y, x + labelW, y + rowH - 4 }, scaler);
            m_parts.track->Layout      (RECT { x + labelW, y, x + labelW + fieldW, y + rowH - 4 }, scaler);
            x += labelW + fieldW + pad;
        }

        m_parts.entryLabel->Layout (RECT { x, y, x + labelW, y + rowH - 4 }, scaler);
        m_parts.input->Layout      (RECT { x + labelW, y, boundsPx.right - pad, y + rowH - 4 }, scaler);
        m_parts.files->Layout      (RECT { x + labelW, y, boundsPx.right - pad, y + rowH - 4 }, scaler);
        x  = boundsPx.left + pad;
        y += rowH;

        //  A file's lines start under the list.
        x += isFile ? labelW : 0;

        m_parts.hint->Layout (RECT { x, y, boundsPx.right - pad, y + scaler.ToPx (kHintDip) }, scaler);

        m_parts.error->Layout (RECT { x, y, boundsPx.right - pad, y + 2 * rowH }, scaler);
    }

    void  Relayout()
    {
        Layout (m_boundsPx, m_scaler);
    }

    bool  OnMouse (const DxuiMouseEvent & ev) override
    {
        bool  isHandled = false;
        bool  isOverTip = false;



        for (int i = 0; i < GoToDialog::kTipCount; i++)
        {
            RECT  tip = m_parts.tips[i].GetBounds();

            if (m_parts.tips[i].IsVisible())
            {
                isHandled = m_parts.tips[i].OnMouse (ev) || isHandled;
                isOverTip = isOverTip || (PtInRect (&tip, ev.positionDip) && ev.kind != DxuiMouseEventKind::Leave);
            }
        }

        //  A tip sits within the row of the radio beside it.
        if (isOverTip)
        {
            return true;
        }

        isHandled = isHandled || m_parts.kinds->OnMouse (ev);



        for (IDxuiControl * control : std::initializer_list<IDxuiControl *> { m_parts.track, m_parts.input, m_parts.files })
        {
            RECT  bounds = control->GetBounds();

            if (ev.kind == DxuiMouseEventKind::Down && control->IsVisible() && control->IsEnabled() && PtInRect (&bounds, ev.positionDip) && m_onPress)
            {
                m_onPress (control);
            }

            isHandled = isHandled || (control->IsVisible() && control->OnMouse (ev));
        }

        return isHandled;
    }

private:
    void  SetKindOptions (int first, int second, int top, int rows, int sel, const RECT & boundsPx, const DxuiDpiScaler & scaler)
    {
        int                           rowH  = scaler.ToPx (kRowDip);
        int                           right = boundsPx.right - scaler.ToPx (kPadDip);
        int                           count = static_cast<int> (m_parts.kindList->size());
        int                           i     = 0;
        std::vector<DxuiRadioOption>  options;



        for (i = 0; i < count; i++)
        {
            options.push_back ({ RECT { (i < rows) ? first : second, top + (i % rows) * rowH, (i < rows) ? second : right, top + (i % rows + 1) * rowH - 4 },
                                 InspectorGoTo::GetLabel ((*m_parts.kindList)[static_cast<size_t> (i)]) });
        }

        m_parts.kinds->SetOptions  (std::move (options));
        m_parts.kinds->SetSelected (sel);
        m_parts.kinds->Layout (RECT { first, top, right, top + rows * rowH }, scaler);
    }

    //  The width of the widest of the options from `from` up to `to`, from
    //  its radio to the end of its label, or of its tip where it has one.
    int  MeasureColumn (int from, int to, const DxuiDpiScaler & scaler) const
    {
        int  widest = 0;
        int  i      = 0;



        for (i = from; i < to; i++)
        {
            GoToKind  kind   = (*m_parts.kindList)[static_cast<size_t> (i)];
            bool      hasTip = std::any_of (std::begin (s_kKindTips), std::end (s_kKindTips), [kind] (const GoToKindTip & tip) { return tip.kind == kind; });
            int       width  = m_parts.kinds->GetLabelRight (i, *m_parts.text) - m_parts.kinds->GetOptions()[static_cast<size_t> (i)].rect.left;

            widest = std::max (widest, width + (hasTip ? scaler.ToPx (kTipGapDip + kTipDip) : 0));
        }

        return widest;
    }

    void  LayoutTips (const DxuiDpiScaler & scaler)
    {
        for (int i = 0; i < GoToDialog::kTipCount; i++)
        {
            auto  found = std::find (m_parts.kindList->begin(), m_parts.kindList->end(), s_kKindTips[i].kind);

            m_parts.tips[i].SetVisible (found != m_parts.kindList->end());

            if (found != m_parts.kindList->end())
            {
                int           option = static_cast<int> (found - m_parts.kindList->begin());
                const RECT &  row    = m_parts.kinds->GetOptions()[static_cast<size_t> (option)].rect;
                int           right  = (m_parts.text != nullptr) ? m_parts.kinds->GetLabelRight (option, *m_parts.text) : row.right - scaler.ToPx (kTipDip);
                int           drop   = static_cast<int> (std::lround (scaler.ToPxf (kTipDropDip)));

                m_parts.tips[i].Layout (RECT { right + scaler.ToPx (kTipGapDip), row.top + drop, right + scaler.ToPx (kTipGapDip + kTipDip), row.bottom + drop }, scaler);
            }
        }
    }

    static constexpr int  kPadDip    = 12;
    static constexpr int  kRowDip    = 32;
    static constexpr int  kHintDip   = 20;
    static constexpr int  kLabelDip  = 56;
    static constexpr int  kTipGapDip = 2;
    static constexpr int  kTipDip    = 18;

    //  The icon font's ink sits higher in its box than the label's text does.
    static constexpr float  kTipDropDip = 1.5f;

    Parts          m_parts;
    int            m_initial  = 0;
    std::function<void (IDxuiControl *)>  m_onPress;
    RECT           m_boundsPx = {};
    DxuiDpiScaler  m_scaler;
};





////////////////////////////////////////////////////////////////////////////////
//
//  GoToDialog::Configure
//
////////////////////////////////////////////////////////////////////////////////

void GoToDialog::Configure (const IDxuiTheme * theme, const DiskAnalysis & analysis, int quarterTrack, GoToKind kind)
{
    m_theme        = theme;
    m_analysis     = &analysis;
    m_quarterTrack = quarterTrack;
    m_kind         = kind;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GoToDialog::OnCreate
//
////////////////////////////////////////////////////////////////////////////////

void GoToDialog::OnCreate()
{
    GoToBody::Parts  parts;



    m_shownKinds = InspectorGoTo::GetKinds (*m_analysis);
    m_kind       = (std::find (m_shownKinds.begin(), m_shownKinds.end(), m_kind) != m_shownKinds.end()) ? m_kind : GoToKind::Track;

    m_kinds.SetDpi (GetDpi());

    for (DxuiTextInput * input : { &m_track, &m_input })
    {
        input->SetTheme        (m_theme);
        input->SetHwnd         (GetHwnd());
        input->SetMaxLength    (s_kMaxPathChars);
        input->SetTextRenderer (GetTextRenderer());
    }

    m_track.SetText     (InspectorFormat::FormatQuarterTrack (m_quarterTrack));
    m_track.SetOnChange ([this] (const std::wstring &) { ShowHint (GetChosenKind()); });
    m_input.SetOnChange ([this] (const std::wstring &) { Validate(); });

    for (int i = 0; i < kTipCount; i++)
    {
        m_tips[i].SetText      (s_kKindTips[i].pszText);
        m_tips[i].SetPopupHost (GetPopupHost());

        if (s_kKindTips[i].kind == GoToKind::DosOrderSector)
        {
            m_tips[i].SetTable (BuildDosOrderTable());
        }
    }

    m_trackLabel.SetText      (L"Track");
    m_trackLabel.SetTextAlign (DxuiTextHAlign::Left, DxuiTextVAlign::Center);
    m_entryLabel.SetTextAlign (DxuiTextHAlign::Left, DxuiTextVAlign::Center);

    m_hint.SetTextRole  (DxuiTextRole::Muted);
    m_hint.SetTextAlign (DxuiTextHAlign::Left, DxuiTextVAlign::Center);

    m_error.SetTextRole  (DxuiTextRole::Error);
    m_error.SetTextAlign (DxuiTextHAlign::Left, DxuiTextVAlign::Top);

    FillFiles();

    parts      = { &m_shownKinds, &m_kinds, m_tips, GetTextRenderer(), &m_trackLabel, &m_track, &m_entryLabel, &m_input, &m_files, &m_hint, &m_error };
    m_body     = CreateDialogContent<GoToBody>();
    m_body->Init (parts, static_cast<int> (std::find (m_shownKinds.begin(), m_shownKinds.end(), m_kind) - m_shownKinds.begin()), [this] (IDxuiControl * control) { FocusControl (control); });

    m_kinds.SetOnChange ([this] (int index)
    {
        GoToKind  kind = m_shownKinds[static_cast<size_t> (index)];



        //  An entry for one kind means nothing for another, but a track does.
        m_input.SetText (L"");
        m_body->Relayout();
        ShowHint (kind);
        (kind == GoToKind::Track) ? m_track.SelectAll() : m_track.ClearSelection();
        FocusControl (GetEntryControl (kind));
    });

    m_ok = AddDialogButton (L"OK", IDOK);
    AddDialogButton (L"Cancel", IDCANCEL);

    m_ok->SetOnClick ([this] () { OnOkClicked(); });

    ShowHint (m_kind);

    if (m_kind == GoToKind::Track)
    {
        m_track.SelectAll();
    }

    SetInitialFocus (GetEntryControl (m_kind));
}





////////////////////////////////////////////////////////////////////////////////
//
//  GoToDialog::OnDialogTick
//
//  An info tip's tooltip opens once its dwell has passed, which only a tick
//  can advance.
//
////////////////////////////////////////////////////////////////////////////////

void GoToDialog::OnDialogTick()
{
    DxuiPanel::Tick (static_cast<int64_t> (GetTickCount64()));
}





////////////////////////////////////////////////////////////////////////////////
//
//  GoToDialog::GetEntryControl
//
//  The control a kind's entry starts in. With no files to choose from, that is
//  the kinds, as a disabled list can't take the focus.
//
////////////////////////////////////////////////////////////////////////////////

IDxuiControl * GoToDialog::GetEntryControl (GoToKind kind)
{
    IDxuiControl *  control = &m_input;



    if (kind == GoToKind::Track)
    {
        control = &m_track;
    }
    else if (kind == GoToKind::File)
    {
        control = m_paths.empty() ? static_cast<IDxuiControl *> (&m_kinds) : &m_files;
    }

    return control;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GoToDialog::Validate
//
//  Checks the entries on each change, so an invalid one has its error as it is
//  typed and OK is enabled only for a target that exists. An empty entry, or one
//  on its way to being valid, has no error yet.
//
////////////////////////////////////////////////////////////////////////////////

bool GoToDialog::Validate()
{
    GoToKind      kind         = GetChosenKind();
    bool          usesTrack    = kind == GoToKind::Track || InspectorGoTo::IsOnTrack (kind);
    int           file         = m_files.GetSelectedIndex();
    int           quarterTrack = GetEnteredTrack();
    bool          isReady      = false;
    std::wstring  text         = (kind == GoToKind::Track) ? m_track.GetText()
                               : (kind != GoToKind::File)  ? m_input.GetText()
                               : (file >= 0 && file < static_cast<int> (m_paths.size())) ? m_paths[static_cast<size_t> (file)] : std::wstring();
    std::wstring  error;



    if (usesTrack && quarterTrack < 0 && !m_track.GetText().empty() && !InspectorGoTo::IsPartial (GoToKind::Track, m_track.GetText()))
    {
        error = L"Enter a track from 0 to 39.75, such as 17.25.";
    }
    else if ((!usesTrack || quarterTrack >= 0) && !text.empty() && m_analysis != nullptr)
    {
        isReady = InspectorGoTo::Resolve (*m_analysis, usesTrack ? quarterTrack : m_quarterTrack, kind, text, m_target, error);
        error   = InspectorGoTo::IsPartial (kind, text) ? std::wstring() : error;
    }

    //  The error replaces the hint, as both have the same range.
    m_error.SetText  (error);
    m_hint.SetVisible (error.empty());
    m_ok->SetEnabled (isReady);
    Invalidate();

    return isReady;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GoToDialog::FillFiles
//
//  The files of every mapped volume that have a sector on the disk, for the
//  File kind's list. With none, the list is empty and can't be opened.
//
////////////////////////////////////////////////////////////////////////////////

void GoToDialog::FillFiles()
{
    vector<std::wstring>  paths;



    for (const FileMap & map : (m_analysis != nullptr) ? m_analysis->fileMaps : vector<FileMap>())
    {
        for (const MappedFile & file : map.files)
        {
            if (!file.isDeleted && std::any_of (file.sectors.begin(), file.sectors.end(), [] (const FilePlace & place) { return place.cell >= 0; }))
            {
                paths.push_back (file.path);
            }
        }
    }

    m_files.SetPopupHost (GetPopupHost());
    m_files.SetElide     (DxuiElide::Middle);
    m_files.SetItems     (paths);
    m_files.SetSelected  (paths.empty() ? -1 : 0);
    m_files.SetEnabled   (!paths.empty());
    m_files.SetSelect    ([this] (int) { Validate(); });
    m_paths = std::move (paths);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GoToDialog::GetChosenKind
//
////////////////////////////////////////////////////////////////////////////////

GoToKind GoToDialog::GetChosenKind() const
{
    return (m_kinds.GetSelected() >= 0) ? m_shownKinds[static_cast<size_t> (m_kinds.GetSelected())] : m_kind;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GoToDialog::GetEnteredTrack
//
//  The quarter track in the Track field, or -1 when it is not a track.
//
////////////////////////////////////////////////////////////////////////////////

int GoToDialog::GetEnteredTrack() const
{
    int  quarterTrack = -1;



    return InspectorFormat::TryParseQuarterTrack (m_track.GetText(), quarterTrack) ? quarterTrack : -1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GoToDialog::ShowHint
//
//  The entry's label and the line under the entry for a kind, on the entered
//  track, and the entries checked again.
//
////////////////////////////////////////////////////////////////////////////////

void GoToDialog::ShowHint (GoToKind kind)
{
    int  quarterTrack = GetEnteredTrack();



    m_entryLabel.SetText (GetEntryLabel (kind));

    m_hint.SetText ((kind == GoToKind::File)                                       ? std::wstring (m_paths.empty() ? L"No files on this disk" : L"Selects the file in the Files tab and marks its sectors")
                    : (m_analysis == nullptr)                                       ? std::wstring()
                    : (InspectorGoTo::IsOnTrack (kind) && quarterTrack < 0)        ? std::wstring (L"Enter a track from 0 to 39.75, such as 17.25")
                    : InspectorGoTo::GetRangeHint (*m_analysis, (quarterTrack >= 0) ? quarterTrack : m_quarterTrack, kind));
    Validate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  GoToDialog::BuildDosOrderTable
//
//  Each DOS-order sector index over the physical sector it is, in hex.
//
////////////////////////////////////////////////////////////////////////////////

vector<vector<std::wstring>> GoToDialog::BuildDosOrderTable()
{
    static constexpr int  kSectors = 16;



    vector<vector<std::wstring>>  rows = { { L"DOS-order index" }, { L"Physical sector" } };
    int                           p    = 0;



    for (int index = 0; index < kSectors; index++)
    {
        p = 0;

        while (p < kSectors && NibblizationLayer::GetDosFileIndexForPhysicalSector (p) != index)
        {
            p++;
        }

        rows[0].push_back (std::format (L"{:X}", index));
        rows[1].push_back (std::format (L"{:X}", p));
    }

    return rows;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GoToDialog::GetEntryLabel
//
////////////////////////////////////////////////////////////////////////////////

LPCWSTR GoToDialog::GetEntryLabel (GoToKind kind)
{
    LPCWSTR  label = L"Track";



    switch (kind)
    {
        case GoToKind::PhysicalSector: label = L"Sector"; break;
        case GoToKind::DosOrderSector: label = L"Index";  break;
        case GoToKind::ProDosBlock:    label = L"Block";  break;
        case GoToKind::NibbleOffset:   label = L"Offset"; break;
        case GoToKind::Cell:           label = L"Cell";   break;
        case GoToKind::File:           label = L"File";   break;
        default:                                          break;
    }

    return label;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GoToDialog::OnOkClicked
//
////////////////////////////////////////////////////////////////////////////////

void GoToDialog::OnOkClicked()
{
    m_kind = GetChosenKind();

    if (Validate())
    {
        m_isChosen = true;
        EndDialog (IDOK);
    }
}
