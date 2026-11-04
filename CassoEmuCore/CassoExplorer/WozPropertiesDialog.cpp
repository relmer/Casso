#include "Pch.h"

#include "CassoExplorer/WozPropertiesDialog.h"
#include "CassoExplorer/WozPropertiesPage.h"
#include "CassoExplorer/Window/ExplorerTextStyles.h"
#include "Core/TextEncoding.h"
#include "Machines/Apple2/Common/WozCompatibility.h"





//  The machines' boxes a family a row, by their compatible-hardware bits.
static constexpr uint16_t  s_kMachineFamilies[][2] =
{
    { WozCompatibility::kApple2,   WozCompatibility::kApple2Plus       },
    { WozCompatibility::kApple2e,  WozCompatibility::kApple2eEnhanced  },
    { WozCompatibility::kApple2c,  WozCompatibility::kApple2cPlus      },
    { WozCompatibility::kApple3,   WozCompatibility::kApple3Plus       },
    { WozCompatibility::kApple2gs, 0                                   },
};





////////////////////////////////////////////////////////////////////////////////
//
//  WozPropertiesDialog::Show
//
//  Opens on the tab given, kInfoTab or kMetaTab.
//
////////////////////////////////////////////////////////////////////////////////

void WozPropertiesDialog::Show (HWND owner, const IDxuiTheme * theme, const std::wstring & leafName, const WozLoader::Description & woz, int tab, ApplyFn apply)
{
    HRESULT                   hr     = S_OK;
    WozPropertiesDialog       dialog;
    DxuiWindow::CreateParams  params;



    dialog.m_theme = theme;
    dialog.m_woz   = woz;
    dialog.m_apply = std::move (apply);

    dialog.SetDesignWidthDip (kWidthDip);

    params.title                    = leafName + L" disk properties";
    params.hInstance                = GetModuleHandleW (nullptr);
    params.ownerHwnd                = owner;
    params.initialSizeDip           = { kWidthDip, kHeightDip };
    params.minSizeDip               = { kWidthDip, kMinHeightDip };
    params.fitToWorkArea            = true;
    params.resizable                = true;
    params.insetContentBelowCaption = true;
    params.captionStyle             = DxuiCaptionStyle::CloseOnly;
    params.placement                = DxuiWindowPlacement::CenteredOnOwner;

    hr = dialog.Create (params);
    CHR (hr);

    dialog.SetTheme        (theme);
    dialog.SetActivePage   (tab);
    dialog.ShowModalDialog (IDOK);

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WozPropertiesDialog::OnBuildPages
//
////////////////////////////////////////////////////////////////////////////////

void WozPropertiesDialog::OnBuildPages()
{
    BuildInfoPage();
    BuildMetaPage();
}





////////////////////////////////////////////////////////////////////////////////
//
//  WozPropertiesDialog::BuildInfoPage
//
//  INFO's fields. A version 1 INFO ends before the boot sector format, so
//  those fields are disabled, with a note.
//
////////////////////////////////////////////////////////////////////////////////

void WozPropertiesDialog::BuildInfoPage()
{
    static constexpr int                          kNoteHeightDip = 40;
    std::vector<AppleFilePropertiesPanel::Row>    rows;
    std::vector<std::wstring>                     ramItems;
    const std::vector<std::wstring>             & bootLabels     = WozProperties::GetBootSectorLabels();
    bool                                          version2       = m_woz.hasBootSectorFormat;
    int                                           ramIndex       = -1;



    SetUpLabel (m_formatLabel,  L"Format:");
    SetUpLabel (m_format,       std::format (L"WOZ {}, {} disk", m_woz.wozVersion, (m_woz.diskType == WozLoader::kDiskType35) ? L"3.5-inch" : L"5.25-inch"));
    SetUpLabel (m_creatorLabel, L"Creator:");
    SetUpLabel (m_creator,      TextEncoding::Utf8ToWide (m_woz.creator));
    SetUpLabel (m_bootLabel,    L"Boot sector:");
    SetUpLabel (m_hardwareLabel, L"Runs on:");
    SetUpLabel (m_ramLabel,     L"Required RAM:");

    m_writeProtected.SetSingleLineLabel (true);
    m_writeProtected.SetChecked         (m_woz.writeProtected);
    m_writeProtected.SetOnChange        ([this] (bool) { MarkEdited(); });

    m_boot.SetPopupHost (GetPopupHost());
    m_boot.SetItems     (bootLabels);
    m_boot.SetSelected  ((m_woz.bootSectorFormat < bootLabels.size()) ? m_woz.bootSectorFormat : 0);
    m_boot.SetEnabled   (version2);
    m_boot.SetSelect    ([this] (int) { MarkEdited(); });

    //  A size the reference doesn't list is offered as the image has it.
    for (const WozProperties::RamSize & size : WozProperties::GetRamSizes())
    {
        ramIndex = (size.kilobytes == m_woz.requiredRamK) ? (int) m_ramSizes.size() : ramIndex;

        m_ramSizes.push_back (size.kilobytes);
        ramItems.push_back   (size.label);
    }

    if (ramIndex < 0)
    {
        ramIndex = (int) m_ramSizes.size();

        m_ramSizes.push_back (m_woz.requiredRamK);
        ramItems.push_back   (std::format (L"{} KB", m_woz.requiredRamK));
    }

    m_ram.SetPopupHost (GetPopupHost());
    m_ram.SetItems     (ramItems);
    m_ram.SetSelected  (ramIndex);
    m_ram.SetEnabled   (version2);
    m_ram.SetSelect    ([this] (int) { MarkEdited(); });

    ExplorerTextStyles::Apply  (m_version1Note, ExplorerTextStyles::kFormNote);
    m_version1Note.SetTextAlign (DxuiTextHAlign::Left, DxuiTextVAlign::Top);
    m_version1Note.SetText      (L"This image's INFO chunk is version 1, which doesn't record the boot sector format, compatible hardware or required RAM.");
    m_version1Note.SetVisible   (!version2);

    rows.push_back ({ &m_formatLabel,  { { &m_format } } });
    rows.push_back ({ &m_creatorLabel, { { &m_creator } } });
    rows.push_back ({ nullptr,         { { &m_writeProtected } }, true });
    rows.push_back ({ &m_bootLabel,    { { &m_boot, kComboWidthDip } }, true });

    AddMachineRows (rows, m_hardwareLabel, m_hardware, m_woz.compatibleHardware);

    for (DxuiCheckbox & box : m_hardware)
    {
        box.SetEnabled (version2);
    }

    rows.push_back ({ &m_ramLabel,     { { &m_ram, kComboWidthDip } }, true });
    rows.push_back ({ &m_version1Note, {}, true, kNoteHeightDip });

    m_infoPage = CreatePage<WozPropertiesPage> (L"Disk");
    m_infoPage->Init       (std::move (rows), [this] (IDxuiControl * child) { FocusControl (child); });
    m_infoPage->SetOnApply ([this]() { return TrySave(); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  WozPropertiesDialog::BuildMetaPage
//
//  The reference's keys in its order, then any other key the image has.
//  Language and the RAM size are lists, with the image's value added when
//  the reference doesn't list it, and the machines are boxes.
//
////////////////////////////////////////////////////////////////////////////////

void WozPropertiesDialog::BuildMetaPage()
{
    std::vector<AppleFilePropertiesPanel::Row>  rows;
    std::wstring                                language = GetMetaValue (WozProperties::kLanguage);
    std::wstring                                metaRam  = GetMetaValue (WozProperties::kRequiresRam);



    m_languages = { std::wstring (kNotGiven) };
    m_languages.insert (m_languages.end(), WozProperties::GetLanguages().begin(), WozProperties::GetLanguages().end());
    m_metaRams  = { std::wstring (kNotGiven) };
    m_metaRams.insert (m_metaRams.end(), WozProperties::GetMetaRamValues().begin(), WozProperties::GetMetaRamValues().end());

    for (const WozProperties::MetaKey & key : WozProperties::GetMetaKeys())
    {
        std::string  name = key.key;

        if (name == WozProperties::kLanguage)
        {
            SetUpLabel   (m_languageLabel, std::wstring (key.label) + L":");
            SetUpChoices (m_language, m_languages, language);
            rows.push_back ({ &m_languageLabel, { { &m_language, kComboWidthDip } } });
        }
        else if (name == WozProperties::kRequiresRam)
        {
            SetUpLabel   (m_metaRamLabel, std::wstring (key.label) + L":");
            SetUpChoices (m_metaRam, m_metaRams, metaRam);
            rows.push_back ({ &m_metaRamLabel, { { &m_metaRam, kComboWidthDip } } });
        }
        else if (name == WozProperties::kRequiresMachine)
        {
            SetUpLabel     (m_machinesLabel, std::wstring (key.label) + L":");
            AddMachineRows (rows, m_machinesLabel, m_machines, WozProperties::ParseMachineCodes (TextEncoding::WideToUtf8 (GetMetaValue (name))));

            //  A value with codes the reference doesn't define is saved as
            //  it was unless a box is clicked.
            for (DxuiCheckbox & box : m_machines)
            {
                box.SetOnChange ([this] (bool) { m_machinesEdited = true; MarkEdited(); });
            }
        }
        else if (name == WozProperties::kImageDate)
        {
            AddImageDate (rows);
        }
        else
        {
            AddMetaText (rows, name, std::wstring (key.label) + L":", WozProperties::IsListKey (name));
        }
    }

    //  A key the reference doesn't define is one value, or a list when the
    //  image has one there.
    for (const WozLoader::MetaField & field : m_woz.meta)
    {
        if (!WozProperties::IsStandardKey (field.key))
        {
            AddMetaText (rows, field.key, WozProperties::GetKeyLabel (field.key) + L":", field.value.find ('|') != std::string::npos);
        }
    }

    m_metaPage = CreatePage<WozPropertiesPage> (L"Metadata");
    m_metaPage->Init (std::move (rows), [this] (IDxuiControl * child) { FocusControl (child); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  WozPropertiesDialog::AddMachineRows
//
//  The machines' boxes, in two rows, the first beside the label.
//
////////////////////////////////////////////////////////////////////////////////

void WozPropertiesDialog::AddMachineRows (std::vector<AppleFilePropertiesPanel::Row> & rows, DxuiLabel & label, Machines & boxes, uint16_t checked)
{
    const std::vector<WozProperties::Machine>  & machines = WozProperties::GetMachines();



    for (size_t at = 0; at < boxes.size(); at++)
    {
        boxes[at].SetLabel           (machines[at].label);
        boxes[at].SetSingleLineLabel (true);
        boxes[at].SetChecked         ((checked & machines[at].bit) != 0);
        boxes[at].SetOnChange        ([this] (bool) { MarkEdited(); });
    }

    //  A family a row, the first beside the label.
    for (const auto & family : s_kMachineFamilies)
    {
        AppleFilePropertiesPanel::Row  row;
        bool                           first = &family == &s_kMachineFamilies[0];

        row.label    = first ? &label : nullptr;
        row.gapAbove = first;

        for (uint16_t bit : family)
        {
            for (size_t at = 0; bit != 0 && at < boxes.size(); at++)
            {
                if (machines[at].bit == bit)
                {
                    row.fields.push_back ({ &boxes[at], (bit == WozCompatibility::kApple2eEnhanced) ? kWideMachineDip : kMachineDip });
                }
            }
        }

        rows.push_back (row);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  WozPropertiesDialog::AddMetaText
//
////////////////////////////////////////////////////////////////////////////////

void WozPropertiesDialog::AddMetaText (std::vector<AppleFilePropertiesPanel::Row> & rows, const std::string & key, const std::wstring & label, bool isList)
{
    std::unique_ptr<MetaText>  text   = std::make_unique<MetaText>();
    std::vector<std::wstring>  items  = isList ? WozProperties::SplitList (GetMetaValue (key)) : std::vector<std::wstring> { GetMetaValue (key) };
    size_t                     fields = isList ? (std::max) (items.size() + 1, kMinListFields) : 1;



    text->key = key;

    SetUpLabel (text->label, label);

    //  The first field beside the label, the rest under it.
    for (size_t at = 0; at < fields; at++)
    {
        text->values.push_back (std::make_unique<DxuiTextInput>());
        SetUpInput (*text->values.back(), (at < items.size()) ? items[at] : std::wstring());

        rows.push_back ({ (at == 0) ? &text->label : nullptr, { { text->values.back().get() } } });
    }

    m_metaTexts.push_back (std::move (text));
}





////////////////////////////////////////////////////////////////////////////////
//
//  WozPropertiesDialog::SetUpLabel
//
////////////////////////////////////////////////////////////////////////////////

void WozPropertiesDialog::SetUpLabel (DxuiLabel & label, const std::wstring & text)
{
    ExplorerTextStyles::Apply (label, ExplorerTextStyles::kFormLabel);

    label.SetTextAlign (DxuiTextHAlign::Left, DxuiTextVAlign::Center);
    label.SetText      (text);
}





////////////////////////////////////////////////////////////////////////////////
//
//  WozPropertiesDialog::SetUpInput
//
////////////////////////////////////////////////////////////////////////////////

void WozPropertiesDialog::SetUpInput (DxuiTextInput & input, const std::wstring & text)
{
    input.SetTheme         (m_theme);
    input.SetHwnd          (GetHwnd());
    input.SetTextRenderer  (GetTextRenderer());
    input.SetText          (text);
    input.SetOnChange      ([this] (const std::wstring &) { MarkEdited(); });
    input.SetRejectedChars (WozProperties::kIllegalChars);
    input.SetOnRejected    ([]() { MessageBeep (MB_OK); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  WozPropertiesDialog::SetUpChoices
//
//  The list, with the image's value added at the end when it isn't in it.
//
////////////////////////////////////////////////////////////////////////////////

void WozPropertiesDialog::SetUpChoices (DxuiComboBox & combo, std::vector<std::wstring> & items, const std::wstring & value)
{
    auto  found = std::find (items.begin(), items.end(), value);



    if (value.empty())
    {
        found = items.begin();
    }
    else if (found == items.end())
    {
        items.push_back (value);
        found = items.end() - 1;
    }

    combo.SetPopupHost (GetPopupHost());
    combo.SetItems     (items);
    combo.SetSelected  ((int) (found - items.begin()));
    combo.SetSelect    ([this] (int) { MarkEdited(); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  WozPropertiesDialog::MarkEdited
//
//  Both pages, since one Apply saves both.
//
////////////////////////////////////////////////////////////////////////////////

void WozPropertiesDialog::MarkEdited()
{
    if (m_infoPage != nullptr && m_metaPage != nullptr)
    {
        m_infoPage->MarkDirty();
        m_metaPage->MarkDirty();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  WozPropertiesDialog::TrySave
//
//  Both pages' fields, saved by the apply function. False, after a message,
//  when they weren't saved.
//
////////////////////////////////////////////////////////////////////////////////

bool WozPropertiesDialog::TrySave()
{
    WozLoader::Edits                                     edits;
    std::vector<std::pair<std::string, std::wstring>>    typed;
    std::wstring                                         error;
    int                                                  boot     = m_boot.GetSelectedIndex();
    int                                                  ram      = m_ram.GetSelectedIndex();
    int                                                  language = m_language.GetSelectedIndex();
    int                                                  metaRam  = m_metaRam.GetSelectedIndex();
    std::string                                          machines = WozProperties::FormatMachineCodes (GetChecked (m_machines));
    std::string                                          imageDate;



    edits.writeProtected = m_writeProtected.IsChecked();

    if (m_woz.hasBootSectorFormat)
    {
        edits.bootSectorFormat   = (Byte) (std::max) (boot, 0);
        edits.compatibleHardware = GetChecked (m_hardware);
        edits.requiredRamK       = (ram >= 0) ? m_ramSizes[(size_t) ram] : 0;
    }

    //  A list's fields a line each.
    for (const std::unique_ptr<MetaText> & text : m_metaTexts)
    {
        std::wstring  lines;

        for (const std::unique_ptr<DxuiTextInput> & value : text->values)
        {
            lines += (lines.empty() ? L"" : L"\n") + value->GetText();
        }

        typed.emplace_back (text->key, lines);
    }

    //  The first item of each list is no value.
    typed.emplace_back (WozProperties::kLanguage,        (language > 0) ? m_languages[(size_t) language] : std::wstring());
    typed.emplace_back (WozProperties::kRequiresRam,     (metaRam  > 0) ? m_metaRams[(size_t) metaRam]   : std::wstring());
    typed.emplace_back (WozProperties::kRequiresMachine, ToLines (m_machinesEdited ? TextEncoding::Utf8ToWide (machines) : GetMetaValue (WozProperties::kRequiresMachine)));

    if (!TryGetImageDate (imageDate))
    {
        SetActivePage (kMetaTab);
        DxuiMessageBox (GetHwnd(), m_theme, L"The image date isn't a date and time. Type it as Windows writes dates, or choose one from the calendar.", L"Disk properties", MB_OK | MB_ICONWARNING);
        return false;
    }

    typed.emplace_back (WozProperties::kImageDate, TextEncoding::Utf8ToWide (imageDate));

    edits.meta = WozProperties::MakeMeta (typed);
    error      = m_apply ? m_apply (edits) : std::wstring();

    if (!error.empty())
    {
        DxuiMessageBox (GetHwnd(), m_theme, error.c_str(), L"Disk properties", MB_OK | MB_ICONWARNING);
        return false;
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WozPropertiesDialog::AddImageDate
//
//  The image's date in local time, typed or chosen from a calendar, as the
//  Apple file properties dialog has its dates.
//
////////////////////////////////////////////////////////////////////////////////

void WozPropertiesDialog::AddImageDate (std::vector<AppleFilePropertiesPanel::Row> & rows)
{
    m_imageDateShown = WozProperties::FormatImageDate (TextEncoding::WideToUtf8 (GetMetaValue (WozProperties::kImageDate)));

    SetUpLabel (m_imageDateLabel, WozProperties::GetKeyLabel (WozProperties::kImageDate) + L":");
    SetUpInput (m_imageDate, m_imageDateShown);

    m_imageDatePick.SetGlyph          (s_kpszMdl2Calendar);
    m_imageDatePick.SetAccessibleName (L"Choose a date");
    m_imageDatePick.SetOnClick        ([this]() { PickImageDate(); });

    rows.push_back ({ &m_imageDateLabel, { { &m_imageDate, kDateWidthDip }, { &m_imageDatePick, kPickWidthDip } } });
}





////////////////////////////////////////////////////////////////////////////////
//
//  WozPropertiesDialog::PickImageDate
//
//  The time of day stays as the field has it, or midnight when the field
//  has no date.
//
////////////////////////////////////////////////////////////////////////////////

void WozPropertiesDialog::PickImageDate()
{
    std::string        value;
    SYSTEMTIME         utc   = {};
    SYSTEMTIME         local = {};
    DxuiCalendar::Day  shown;
    bool               dated = TryGetImageDate (value) && WozProperties::TryParseImageDate (value, utc) && SystemTimeToTzSpecificLocalTime (nullptr, &utc, &local);



    if (dated)
    {
        shown = DxuiCalendar::Day { local.wYear, local.wMonth, local.wDay };
    }
    else
    {
        local = SYSTEMTIME {};
    }

    if (m_theme == nullptr || GetPopupHost() == nullptr)
    {
        return;
    }

    m_calendar.Show (*GetPopupHost(), *m_theme, m_imageDate.GetRect(), shown, [this, local] (const DxuiCalendar::Day & day)
    {
        SYSTEMTIME  picked = local;
        SYSTEMTIME  inUtc  = {};

        picked.wYear  = (WORD) day.year;
        picked.wMonth = (WORD) day.month;
        picked.wDay   = (WORD) day.day;

        if (TzSpecificLocalTimeToSystemTime (nullptr, &picked, &inUtc))
        {
            m_imageDate.SetText (WozProperties::FormatImageDate (WozProperties::MakeImageDate (inUtc)));
            MarkEdited();
            Invalidate();
        }
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  WozPropertiesDialog::TryGetImageDate
//
//  The field's date as image_date has it. A field as it was first filled
//  is the image's value as stored, fraction of a second and all. False for
//  text that isn't a date and time.
//
////////////////////////////////////////////////////////////////////////////////

bool WozPropertiesDialog::TryGetImageDate (std::string & outValue) const
{
    if (m_imageDate.GetText() == m_imageDateShown)
    {
        outValue = TextEncoding::WideToUtf8 (GetMetaValue (WozProperties::kImageDate));
        return true;
    }

    return WozProperties::TryParseTypedImageDate (m_imageDate.GetText(), outValue);
}





////////////////////////////////////////////////////////////////////////////////
//
//  WozPropertiesDialog::ToLines
//
//  A list's values as stored, a line each, as typed values are.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring WozPropertiesDialog::ToLines (std::wstring value)
{
    std::replace (value.begin(), value.end(), WozProperties::kValueSeparator, L'\n');

    return value;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WozPropertiesDialog::GetMetaValue
//
//  The image's value for a key, or an empty string when the key isn't in META.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring WozPropertiesDialog::GetMetaValue (const std::string & key) const
{
    auto  found = std::find_if (m_woz.meta.begin(), m_woz.meta.end(), [&key] (const WozLoader::MetaField & field) { return field.key == key; });



    return (found != m_woz.meta.end()) ? TextEncoding::Utf8ToWide (found->value) : std::wstring();
}





////////////////////////////////////////////////////////////////////////////////
//
//  WozPropertiesDialog::GetChecked
//
////////////////////////////////////////////////////////////////////////////////

uint16_t WozPropertiesDialog::GetChecked (const Machines & boxes)
{
    const std::vector<WozProperties::Machine>  & machines = WozProperties::GetMachines();
    uint16_t                                     checked  = 0;



    for (size_t at = 0; at < boxes.size(); at++)
    {
        checked |= boxes[at].IsChecked() ? machines[at].bit : 0;
    }

    return checked;
}
