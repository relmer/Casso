#include "Pch.h"

#include "CassoExplorer/WozPropertiesDialog.h"
#include "CassoExplorer/WozPropertiesPage.h"
#include "CassoExplorer/Window/ExplorerTextStyles.h"
#include "Core/TextEncoding.h"
#include "Core/UnicodeSymbols.h"
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
//  The reference's keys in its order, then the custom keys the image has.
//  Language and the RAM size are lists, with the image's value added when
//  the reference doesn't list it, and the machines are boxes.
//
////////////////////////////////////////////////////////////////////////////////

void WozPropertiesDialog::BuildMetaPage()
{
    std::vector<AppleFilePropertiesPanel::Row>  & rows     = m_standardRows;
    std::wstring                                  language = GetMetaValue (WozProperties::kLanguage);
    std::wstring                                  metaRam  = GetMetaValue (WozProperties::kRequiresRam);



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

    //  The custom keys, the ones the reference doesn't define, under a heading,
    //  each as the file has it.
    ExplorerTextStyles::Apply (m_customHeading, ExplorerTextStyles::kFormNote);
    m_customHeading.SetTextAlign (DxuiTextHAlign::Left, DxuiTextVAlign::Center);
    m_customHeading.SetText      (L"Custom fields");

    for (const WozLoader::MetaField & field : m_woz.meta)
    {
        if (!WozProperties::IsStandardKey (field.key))
        {
            AddCustomText (field.key);
        }
    }

    m_addField.SetOnClick ([this]() { AddCustomField(); });

    m_metaPage = CreatePage<WozPropertiesPage> (L"Metadata");
    m_metaPage->Init (std::vector<AppleFilePropertiesPanel::Row>(), [this] (IDxuiControl * child) { FocusControl (child); });

    SetMetaRows();
    Validate();
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
//  A standard key's fields, a list's a value each, and the row under them
//  for why they can't be saved.
//
////////////////////////////////////////////////////////////////////////////////

void WozPropertiesDialog::AddMetaText (std::vector<AppleFilePropertiesPanel::Row> & rows, const std::string & key, const std::wstring & label, bool isList)
{
    std::unique_ptr<MetaText>  text   = std::make_unique<MetaText>();
    MetaText                 * shown  = text.get();
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

    rows.push_back ({ nullptr, { { &text->error } }, false, kErrorDip });

    m_validator.AddField ([shown]() { return CheckValues (*shown); }, &text->error);
    m_metaTexts.push_back (std::move (text));
}





////////////////////////////////////////////////////////////////////////////////
//
//  WozPropertiesDialog::AddCustomText
//
//  A custom key's fields: the key as the file has it, a value each, and a
//  button that deletes the key. A key that isn't a list has one value.
//
////////////////////////////////////////////////////////////////////////////////

WozPropertiesDialog::MetaText & WozPropertiesDialog::AddCustomText (const std::string & key)
{
    std::unique_ptr<MetaText>  text  = std::make_unique<MetaText>();
    MetaText                 & added = *text;
    std::vector<std::wstring>  items = WozProperties::SplitList (GetMetaValue (key));



    text->key    = key;
    text->custom = true;

    SetUpInput (text->keyInput, TextEncoding::Utf8ToWide (key));
    text->keyInput.SetPlaceholder (L"Key");

    for (size_t at = 0; at < (std::max) (items.size(), (size_t) 1); at++)
    {
        text->values.push_back (std::make_unique<DxuiTextInput>());
        SetUpInput (*text->values.back(), (at < items.size()) ? items[at] : std::wstring());
    }

    text->remove.SetGlyph          (s_kpszMdl2Delete);
    text->remove.SetAccessibleName (L"Delete this field");
    text->remove.SetOnClick        ([this, removed = text.get()]() { RemoveCustom (removed); });

    m_metaTexts.push_back (std::move (text));

    return added;
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
//  A pasted line break or tab stays in the field, with a message under the
//  field.
//
////////////////////////////////////////////////////////////////////////////////

void WozPropertiesDialog::SetUpInput (DxuiTextInput & input, const std::wstring & text)
{
    input.SetTheme            (m_theme);
    input.SetHwnd             (GetHwnd());
    input.SetTextRenderer     (GetTextRenderer());
    input.SetText             (text);
    input.SetPasteKeepsBreaks (true);
    input.SetOnChange         ([this] (const std::wstring &) { MarkEdited(); });
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
//  Both pages, since one Apply saves both, and every field checked again.
//
////////////////////////////////////////////////////////////////////////////////

void WozPropertiesDialog::MarkEdited()
{
    if (m_infoPage != nullptr && m_metaPage != nullptr)
    {
        m_infoPage->MarkDirty();
        m_metaPage->MarkDirty();
    }

    Validate();
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

    //  A value that can't be saved has its message, and OK and Apply are off.
    if (!m_validator.Revalidate())
    {
        return false;
    }

    //  A list's fields a line each; a custom key as typed.
    for (const std::unique_ptr<MetaText> & text : m_metaTexts)
    {
        std::string  key = text->custom ? TextEncoding::WideToUtf8 (text->keyInput.GetText()) : text->key;

        typed.emplace_back (key, JoinLines (*text));
    }

    //  The first item of each list is no value.
    typed.emplace_back (WozProperties::kLanguage,        (language > 0) ? m_languages[(size_t) language] : std::wstring());
    typed.emplace_back (WozProperties::kRequiresRam,     (metaRam  > 0) ? m_metaRams[(size_t) metaRam]   : std::wstring());
    typed.emplace_back (WozProperties::kRequiresMachine, ToLines (m_machinesEdited ? TextEncoding::Utf8ToWide (machines) : GetMetaValue (WozProperties::kRequiresMachine)));

    if (TryGetImageDate (imageDate))
    {
        typed.emplace_back (WozProperties::kImageDate, TextEncoding::Utf8ToWide (imageDate));
    }

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
//  The image's day in local time, typed or chosen from a calendar, and its
//  time of day chosen from lists: hours, minutes and seconds, with AM and
//  PM when the user's clock has them.
//
////////////////////////////////////////////////////////////////////////////////

void WozPropertiesDialog::AddImageDate (std::vector<AppleFilePropertiesPanel::Row> & rows)
{
    std::vector<std::wstring>  hours;
    std::vector<std::wstring>  sixty;
    int                        hour  = 0;



    m_clock24 = WozProperties::IsClock24Hour();
    m_hadDate = WozProperties::TryGetLocalTime (TextEncoding::WideToUtf8 (GetMetaValue (WozProperties::kImageDate)), m_shownLocal);

    SetUpLabel (m_imageDateLabel, L"Image date:");
    SetUpInput (m_imageDate, m_hadDate ? WozProperties::FormatShortDate (m_shownLocal) : std::wstring());

    m_imageDatePick.SetGlyph          (s_kpszMdl2Calendar);
    m_imageDatePick.SetAccessibleName (L"Choose a date");
    m_imageDatePick.SetOnClick        ([this]() { PickImageDate(); });

    //  A 12-hour clock lists 12 first, as its midnight and noon.
    for (int h = 0; h < (m_clock24 ? kHoursPerDay : kHoursPerHalf); h++)
    {
        hours.push_back (m_clock24 ? std::format (L"{:02}", h) : std::format (L"{}", (h == 0) ? kHoursPerHalf : h));
    }

    for (int m = 0; m < kMinutesPerHour; m++)
    {
        sixty.push_back (std::format (L"{:02}", m));
    }

    hour = m_hadDate ? m_shownLocal.wHour : 0;

    SetUpClock (m_hour,    hours, m_clock24 ? hour : hour % kHoursPerHalf);
    SetUpClock (m_minute,  sixty, m_hadDate ? m_shownLocal.wMinute : 0);
    SetUpClock (m_second,  sixty, m_hadDate ? m_shownLocal.wSecond : 0);
    SetUpClock (m_dayHalf, { WozProperties::GetDayHalfLabel (false), WozProperties::GetDayHalfLabel (true) }, (hour >= kHoursPerHalf) ? 1 : 0);
    m_dayHalf.SetVisible (!m_clock24);

    rows.push_back ({ &m_imageDateLabel, { { &m_imageDate, kDateWidthDip }, { &m_imageDatePick, kPickWidthDip }, { &m_hour, kClockWidthDip },
                                           { &m_minute, kClockWidthDip }, { &m_second, kClockWidthDip }, { &m_dayHalf, kClockWidthDip } } });
    rows.push_back ({ nullptr, { { &m_imageDateError } }, false, kErrorDip });

    m_validator.AddField ([this]()
    {
        std::string  value;
        bool         dated = TryGetImageDate (value);

        return dated ? std::wstring() : std::wstring (L"This isn't a date. Type one as Windows writes dates, or choose one from the calendar.");
    }, &m_imageDateError);
}





////////////////////////////////////////////////////////////////////////////////
//
//  WozPropertiesDialog::SetUpClock
//
////////////////////////////////////////////////////////////////////////////////

void WozPropertiesDialog::SetUpClock (DxuiComboBox & combo, std::vector<std::wstring> items, int selected)
{
    combo.SetPopupHost (GetPopupHost());
    combo.SetItems     (items);
    combo.SetSelected  (selected);
    combo.SetSelect    ([this] (int) { MarkEdited(); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  WozPropertiesDialog::PickImageDate
//
//  The calendar opens on the field's day, or on today when the field is
//  empty or isn't a date.
//
////////////////////////////////////////////////////////////////////////////////

void WozPropertiesDialog::PickImageDate()
{
    SYSTEMTIME         local = {};
    DxuiCalendar::Day  shown;
    bool               dated = WozProperties::TryParseShortDate (m_imageDate.GetText(), local);



    if (dated)
    {
        shown = DxuiCalendar::Day { local.wYear, local.wMonth, local.wDay };
    }

    if (m_theme == nullptr || GetPopupHost() == nullptr)
    {
        return;
    }

    m_calendar.Show (*GetPopupHost(), *m_theme, m_imageDate.GetRect(), shown, [this] (const DxuiCalendar::Day & day)
    {
        SYSTEMTIME  picked = {};

        picked.wYear  = (WORD) day.year;
        picked.wMonth = (WORD) day.month;
        picked.wDay   = (WORD) day.day;

        m_imageDate.SetText (WozProperties::FormatShortDate (picked));
        MarkEdited();
        Invalidate();
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  WozPropertiesDialog::TryGetImageDate
//
//  The day and time of day as image_date has it: empty with no day, and
//  the image's value as stored, fraction of a second and all, when neither
//  changed. False for a day that isn't a date.
//
////////////////////////////////////////////////////////////////////////////////

bool WozPropertiesDialog::TryGetImageDate (std::string & outValue) const
{
    SYSTEMTIME  local = {};
    bool        same  = false;



    outValue.clear();

    if (m_imageDate.GetText().find_first_not_of (L' ') == std::wstring::npos)
    {
        return true;
    }

    if (!TryGetTypedTime (local))
    {
        return false;
    }

    same = m_hadDate && local.wYear == m_shownLocal.wYear && local.wMonth == m_shownLocal.wMonth && local.wDay == m_shownLocal.wDay
        && local.wHour == m_shownLocal.wHour && local.wMinute == m_shownLocal.wMinute && local.wSecond == m_shownLocal.wSecond;

    outValue = same ? TextEncoding::WideToUtf8 (GetMetaValue (WozProperties::kImageDate)) : WozProperties::MakeLocalImageDate (local);

    return !outValue.empty();
}





////////////////////////////////////////////////////////////////////////////////
//
//  WozPropertiesDialog::TryGetTypedTime
//
//  The typed day at the time of day the lists have.
//
////////////////////////////////////////////////////////////////////////////////

bool WozPropertiesDialog::TryGetTypedTime (SYSTEMTIME & outLocal) const
{
    int  hour = (std::max) (m_hour.GetSelectedIndex(), 0);



    if (!WozProperties::TryParseShortDate (m_imageDate.GetText(), outLocal))
    {
        return false;
    }

    outLocal.wHour         = (WORD) ((m_clock24 || m_dayHalf.GetSelectedIndex() <= 0) ? hour : hour + kHoursPerHalf);
    outLocal.wMinute       = (WORD) (std::max) (m_minute.GetSelectedIndex(), 0);
    outLocal.wSecond       = (WORD) (std::max) (m_second.GetSelectedIndex(), 0);
    outLocal.wMilliseconds = 0;

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WozPropertiesDialog::SetMetaRows
//
//  The standard keys' rows, then the custom fields' under their heading,
//  each a row of key, value and delete button, then Add field. Called again
//  when a custom field is added or deleted.
//
////////////////////////////////////////////////////////////////////////////////

void WozPropertiesDialog::SetMetaRows()
{
    std::vector<AppleFilePropertiesPanel::Row>  rows = m_standardRows;



    rows.push_back ({ &m_customHeading, {}, true, AppleFilePropertiesPanel::kRowHeightDip, true });

    for (const std::unique_ptr<MetaText> & text : m_metaTexts)
    {
        if (!text->custom)
        {
            continue;
        }

        for (size_t at = 0; at < text->values.size(); at++)
        {
            AppleFilePropertiesPanel::Row  row;

            row.fromLeft = at == 0;
            row.fields   = (at == 0) ? std::vector<AppleFilePropertiesPanel::Field> { { &text->keyInput, kKeyWidthDip }, { text->values[at].get(), kValueWidthDip }, { &text->remove, kPickWidthDip } }
                                     : std::vector<AppleFilePropertiesPanel::Field> { { text->values[at].get(), kValueWidthDip } };

            rows.push_back (row);
        }

        rows.push_back ({ nullptr, { { &text->error } }, false, kErrorDip });
    }

    rows.push_back ({ nullptr, { { &m_addField, kComboWidthDip / 2 } }, true });

    m_metaPage->SetRows (std::move (rows));
}





////////////////////////////////////////////////////////////////////////////////
//
//  WozPropertiesDialog::AddCustomField
//
//  A new custom field, with its key's field focused for typing.
//
////////////////////////////////////////////////////////////////////////////////

void WozPropertiesDialog::AddCustomField()
{
    MetaText  & added = AddCustomText (std::string());



    SetMetaRows();
    MarkEdited();
    FocusControl (&added.keyInput);
    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  WozPropertiesDialog::RemoveCustom
//
//  The field is taken out of the page and moved to m_removedTexts, which
//  lasts until the window closes, since its delete button is running this.
//
////////////////////////////////////////////////////////////////////////////////

void WozPropertiesDialog::RemoveCustom (MetaText * text)
{
    auto  found = std::find_if (m_metaTexts.begin(), m_metaTexts.end(), [text] (const std::unique_ptr<MetaText> & t) { return t.get() == text; });



    if (found == m_metaTexts.end())
    {
        return;
    }

    FocusControl (&m_addField);

    m_removedTexts.push_back (std::move (*found));
    m_metaTexts.erase (found);

    SetMetaRows();
    MarkEdited();
    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  WozPropertiesDialog::Validate
//
//  Every field checked: each message under its field, the rows laid out
//  again around the messages, and OK and Apply only while none has one.
//
////////////////////////////////////////////////////////////////////////////////

void WozPropertiesDialog::Validate()
{
    bool  valid = m_validator.Revalidate();



    for (const std::unique_ptr<MetaText> & text : m_metaTexts)
    {
        if (text->custom)
        {
            text->error.SetMessage (CheckCustom (*text));
            valid = valid && !text->error.HasError();
        }

        text->error.SetVisible (text->error.HasError());
    }

    m_imageDateError.SetVisible (m_imageDateError.HasError());

    SetConfirmable (valid);

    if (m_metaPage != nullptr)
    {
        m_metaPage->Relayout();
    }

    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  WozPropertiesDialog::CheckCustom
//
//  The key first, then its values.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring WozPropertiesDialog::CheckCustom (const MetaText & text) const
{
    std::vector<std::wstring>  others;
    std::wstring               values = JoinLines (text);
    std::wstring               error;



    for (const std::unique_ptr<MetaText> & other : m_metaTexts)
    {
        if (other->custom && other.get() != &text)
        {
            others.push_back (other->keyInput.GetText());
        }
    }

    error = WozProperties::GetKeyError (text.keyInput.GetText(), values.find_first_not_of (L"\n ") != std::wstring::npos, others);

    return error.empty() ? CheckValues (text) : error;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WozPropertiesDialog::CheckValues
//
//  Each field checked separately, by WozProperties::GetListError.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring WozPropertiesDialog::CheckValues (const MetaText & text)
{
    std::vector<std::wstring>  values;



    for (const std::unique_ptr<DxuiTextInput> & value : text.values)
    {
        values.push_back (value->GetText());
    }

    return WozProperties::GetListError (values);
}





////////////////////////////////////////////////////////////////////////////////
//
//  WozPropertiesDialog::JoinLines
//
//  The fields' values a line each, as a list is typed.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring WozPropertiesDialog::JoinLines (const MetaText & text)
{
    std::wstring  lines;



    for (const std::unique_ptr<DxuiTextInput> & value : text.values)
    {
        lines += (lines.empty() ? L"" : L"\n") + value->GetText();
    }

    return lines;
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
