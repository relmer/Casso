#include "Pch.h"

#include "Devices/Disk/Inspector/Blocks/BlockInspectorModel.h"
#include "Ui/DiskInspector/Blocks/BlockFieldsView.h"
#include "Ui/DiskInspector/Blocks/BlockHexView.h"
#include "Ui/DiskInspector/Blocks/BlockInspectorPanel.h"
#include "Ui/DiskInspector/Blocks/BlockMapGrid.h"
#include "Ui/DiskInspector/Blocks/BlockMapView.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorPanel::BlockInspectorPanel
//
////////////////////////////////////////////////////////////////////////////////

BlockInspectorPanel::BlockInspectorPanel()  = default;
BlockInspectorPanel::~BlockInspectorPanel() = default;





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorPanel::Init
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorPanel::Init (BlockInspectorModel * model, const IDxuiTheme * theme, HWND hwnd, IDxuiTextRenderer * text)
{
    constexpr uint32_t  kChangedColor = 0xFFE0A030;
    constexpr uint32_t  kFieldColor   = 0xFF60B0F0;
    constexpr uint32_t  kLinkColor    = 0xFF60D090;
    constexpr size_t    kInputChars   = 8;



    m_model = model;

    m_back      = CreateChild<DxuiButton> (L"Back");
    m_forward   = CreateChild<DxuiButton> (L"Forward");
    m_previous  = CreateChild<DxuiButton> (L"Previous block");
    m_next      = CreateChild<DxuiButton> (L"Next block");
    m_highlight = CreateChild<DxuiButton> (L"Highlight file's blocks");
    m_header    = CreateChild<DxuiButton> (L"Header");
    m_input     = CreateChild<DxuiTextInput>();
    m_map       = CreateChild<BlockMapView>();
    m_hex       = CreateChild<BlockHexView>();
    m_fields    = CreateChild<BlockFieldsView>();

    m_back->SetOnClick     ([this]() { GoBack(); });
    m_forward->SetOnClick  ([this]() { GoForward(); });
    m_previous->SetOnClick ([this]() { Step (-1); });
    m_next->SetOnClick     ([this]() { Step (1); });

    m_highlight->SetOnClick ([this]()
    {
        if (m_model->HasHighlight())
        {
            m_model->ClearHighlight();
        }
        else
        {
            m_model->HighlightFileAt (m_model->GetBlock());
        }

        Refresh();
    });

    m_header->SetOnClick ([this]()
    {
        m_isHeader   = !m_isHeader;
        m_shownBlock = -1;
        Refresh();
    });

    m_input->SetTheme        (theme);
    m_input->SetHwnd         (hwnd);
    m_input->SetTextRenderer (text);
    m_input->SetMaxLength    (kInputChars);
    m_input->SetOnChange     ([this] (const std::wstring & value) { OnInputChange (value); });

    m_map->SetModel    (model);
    m_map->SetOnSelect ([this] (uint32_t block) { SelectBlock (block); });
    m_map->SetOnHover  ([this] (const std::wstring & value)
    {
        m_hoverText = value;

        if (m_onChanged)
        {
            m_onChanged();
        }
    });

    m_hex->SetBytesPerRow  (kBytesPerRow);
    m_hex->SetOwnerWindow  (hwnd);
    m_hex->SetTextEncoding (DxuiHexView::TextEncoding::AppleHighBit);
    m_hex->SetOnType       ([this] (uint64_t offset, bool isHex, bool isHigh, wchar_t ch) { return OnType (offset, isHex, isHigh, ch); });
    m_hex->SetOnNavigate   ([this] (const DxuiKeyEvent & ev) { return OnNavigateKey (ev); });
    m_hex->SetOnClick      ([this] (uint64_t offset)
    {
        if (!m_isHeader && m_model->TryFollowLink ((int) offset))
        {
            m_isTyping = false;
            Refresh();
        }
    });

    m_hex->SetMarkColor ([] (uint8_t mark, uint32_t & outArgb)
    {
        outArgb = mark == BlockHexSource::kMarkChanged ? kChangedColor
                : mark == BlockHexSource::kMarkField   ? kFieldColor
                :                                        kLinkColor;

        return mark != BlockHexSource::kMarkNone;
    });

    m_fields->SetOnHover  ([this] (const BlockField * field)
    {
        MarkField (field);

        if (m_onChanged)
        {
            m_onChanged();
        }
    });
    m_fields->SetOnFollow ([this] (const BlockField * field)
    {
        if (field != nullptr && field->link != BlockField::kNoLink && !m_isHeader)
        {
            SelectBlock ((uint32_t) field->link);
        }
    });

    Refresh();
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorPanel::TryParseBlock
//
////////////////////////////////////////////////////////////////////////////////

bool BlockInspectorPanel::TryParseBlock (const std::wstring & text, uint32_t & outBlock)
{
    constexpr int  kHex     = 16;
    constexpr int  kDecimal = 10;



    std::wstring  trimmed = text;
    wchar_t     * end     = nullptr;
    int           base    = kDecimal;
    uint64_t      value   = 0;



    trimmed.erase (0, trimmed.find_first_not_of (L' '));
    trimmed.erase (trimmed.find_last_not_of (L' ') + 1);

    if (!trimmed.empty() && trimmed[0] == L'$')
    {
        trimmed.erase (0, 1);
        base = kHex;
    }

    if (trimmed.empty() || !iswxdigit (trimmed[0]))
    {
        return false;
    }

    value = wcstoull (trimmed.c_str(), &end, base);

    if (end == nullptr || *end != L'\0' || value > UINT32_MAX)
    {
        return false;
    }

    outBlock = (uint32_t) value;

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorPanel::OnInputChange
//
//  The first change after a step adds the block to Back and Forward, and
//  each change after it replaces it, so typing "123" is one step.
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorPanel::OnInputChange (const std::wstring & text)
{
    uint32_t  block = 0;



    if (!TryParseBlock (text, block) || block >= m_model->GetBlockCount())
    {
        return;
    }

    if (m_isTyping)
    {
        m_model->ReplaceBlock (block);
    }
    else
    {
        m_model->SelectBlock (block);
        m_isTyping = true;
    }

    ShowBytes();
    BuildStatus();
    m_map->Refresh();
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorPanel::SelectBlock
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorPanel::SelectBlock (uint32_t block)
{
    m_isHeader = false;
    m_isTyping = false;
    m_model->SelectBlock (block);

    Refresh();
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorPanel::Step
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorPanel::Step (int64_t delta)
{
    m_isHeader = false;
    m_isTyping = false;
    m_model->StepBlock (delta);

    Refresh();
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorPanel::GoBack
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorPanel::GoBack()
{
    m_isHeader = false;
    m_isTyping = false;
    m_model->GoBack();

    Refresh();
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorPanel::GoForward
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorPanel::GoForward()
{
    m_isHeader = false;
    m_isTyping = false;
    m_model->GoForward();

    Refresh();
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorPanel::OnNavigateKey
//
////////////////////////////////////////////////////////////////////////////////

bool BlockInspectorPanel::OnNavigateKey (const DxuiKeyEvent & ev)
{
    if (ev.vk == VK_PRIOR)
    {
        Step (-1);
    }
    else if (ev.vk == VK_NEXT)
    {
        Step (1);
    }
    else if (ev.alt && ev.vk == VK_LEFT)
    {
        GoBack();
    }
    else if (ev.alt && ev.vk == VK_RIGHT)
    {
        GoForward();
    }
    else
    {
        return false;
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorPanel::OnType
//
//  The header isn't edited here.
//
////////////////////////////////////////////////////////////////////////////////

bool BlockInspectorPanel::OnType (uint64_t offset, bool isHexColumn, bool isHighNibble, wchar_t ch)
{
    bool  isChanged = false;



    if (m_isHeader)
    {
        return false;
    }

    isChanged = isHexColumn ? m_model->TypeHexDigit ((int) offset, isHighNibble, ch)
                            : m_model->TypeCharacter ((int) offset, ch);

    if (isChanged)
    {
        Refresh();
    }

    return isChanged;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorPanel::ShowBytes
//
//  The block's bytes, or the header's, with their marks and fields. The hex
//  view starts over at the top only when the block changes.
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorPanel::ShowBytes()
{
    BlockInspectorModel::Block  bytes  = {};
    vector<BlockField>          fields;
    uint32_t                    block  = m_model->GetBlock();
    int64_t                     shown  = m_isHeader ? -2 : (int64_t) block;
    HRESULT                     hr     = S_OK;
    size_t                      i      = 0;



    if (m_isHeader)
    {
        m_source.SetBytes (m_model->GetFile().GetHeader());
        m_model->GetFile().AnnotateHeader (fields);
        m_fields->SetHeading (L"Container header");
    }
    else
    {
        hr = m_model->ReadBlock (block, bytes);
        IGNORE_RETURN_VALUE (hr, S_OK);

        m_source.SetBytes (bytes);
        m_model->GetFields (block, bytes, fields);
        m_fields->SetHeading (L"Block " + std::to_wstring (block));

        for (i = 0; i < bytes.size(); i++)
        {
            if (m_model->GetEdits().IsChanged (block, (int) i))
            {
                m_source.SetMark (i, BlockHexSource::kMarkChanged);
            }
        }
    }

    for (const BlockField & field : fields)
    {
        if (field.link == BlockField::kNoLink)
        {
            continue;
        }

        for (i = field.offset; i < (size_t) field.offset + field.length; i++)
        {
            m_source.SetMark (i, BlockHexSource::kMarkLink);
        }

        if (field.pairOffset >= 0)
        {
            m_source.SetMark ((uint64_t) field.pairOffset, BlockHexSource::kMarkLink);
        }
    }

    m_fields->SetFields (std::move (fields));

    if (shown != m_shownBlock)
    {
        m_shownBlock = shown;
        m_hex->SetSource (&m_source);
        m_hex->ResetNibble();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorPanel::MarkField
//
//  The field under the pointer, marked over the link marks.
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorPanel::MarkField (const BlockField * field)
{
    uint32_t  i = 0;



    ShowBytes();

    if (field == nullptr)
    {
        return;
    }

    for (i = field->offset; i < field->offset + field->length; i++)
    {
        m_source.SetMark (i, BlockHexSource::kMarkField);
    }

    if (field->pairOffset >= 0)
    {
        m_source.SetMark ((uint64_t) field->pairOffset, BlockHexSource::kMarkField);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorPanel::BuildStatus
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorPanel::BuildStatus()
{
    uint32_t                                                 block  = m_model->GetBlock();
    std::wstring                                             owners = m_model->GetOwners (block);
    std::array<Byte, BlockImageFile::kTagBytesPerBlock>      tags   = {};
    int                                                      edits  = m_model->GetEdits().CountChangedBytes();
    wchar_t                                                  hex[4] = {};



    std::replace (owners.begin(), owners.end(), L'\n', L',');

    m_status = L"Block " + std::to_wstring (block) + L": " + BlockMapGrid::GetRoleName (m_model->GetRole (block));

    if (!owners.empty())
    {
        m_status += L". " + owners;
    }

    if (m_model->GetFile().TryReadTags (block, tags))
    {
        m_status += L". Tag bytes:";

        for (Byte tag : tags)
        {
            swprintf_s (hex, L" %02X", tag);
            m_status += hex;
        }
    }

    if (m_model->HasHighlight())
    {
        m_status += L". Highlighted: " + m_model->GetHighlightedPath();
    }

    if (edits > 0)
    {
        m_status += L". " + std::to_wstring (edits) + (edits == 1 ? L" byte changed" : L" bytes changed") + L", not written";
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorPanel::Refresh
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorPanel::Refresh()
{
    if (m_model == nullptr)
    {
        return;
    }

    ShowBytes();
    BuildStatus();

    if (!m_input->IsFocused() || !m_isTyping)
    {
        m_input->SetText (std::to_wstring (m_model->GetBlock()));
    }

    m_back->SetEnabled      (m_model->CanGoBack());
    m_forward->SetEnabled   (m_model->CanGoForward());
    m_previous->SetEnabled  (m_model->GetBlock() > 0);
    m_next->SetEnabled      (m_model->GetBlock() + 1 < m_model->GetBlockCount());
    m_highlight->SetLabel   (m_model->HasHighlight() ? L"Clear highlight" : L"Highlight file's blocks");
    m_header->SetVisible    (!m_model->GetFile().GetHeader().empty());
    m_header->SetLabel      (m_isHeader ? L"Blocks" : L"Header");

    m_map->Refresh();

    if (m_onChanged)
    {
        m_onChanged();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorPanel::Layout
//
//  The toolbar across the top and the status line along the bottom; between
//  them the map and the hover line on the left, and the hex view over the
//  fields on the right.
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorPanel::Layout (const RECT & boundsPx, const DxuiDpiScaler & scaler)
{
    constexpr float  kBackDip      = 64.0f;
    constexpr float  kForwardDip   = 76.0f;
    constexpr float  kStepDip      = 112.0f;
    constexpr float  kHighlightDip = 160.0f;
    constexpr float  kHeaderDip    = 72.0f;



    LONG  pad     = scaler.ToPx (kPadDip);
    LONG  toolbar = scaler.ToPx (kToolbarDip);
    LONG  gap     = scaler.ToPx (kButtonGap);
    LONG  line    = scaler.ToPx (kLineDip);
    LONG  x       = boundsPx.left;
    LONG  top     = boundsPx.top;
    LONG  body    = top + toolbar + pad;
    LONG  bottom  = boundsPx.bottom - line - pad;
    LONG  split   = boundsPx.left + (LONG) ((boundsPx.right - boundsPx.left) * kMapShare);
    LONG  hexEnd  = body + (LONG) ((bottom - body) * kHexShare);

    auto  place = [&] (IDxuiControl * control, float widthDip)
    {
        LONG  width = scaler.ToPx (widthDip);

        control->Layout (RECT { x, top, x + width, top + toolbar }, scaler);
        x += width + gap;
    };



    SetBounds (boundsPx);
    m_scaler = scaler;

    place (m_back,     kBackDip);
    place (m_forward,  kForwardDip);
    place (m_previous, kStepDip);

    m_labelRect = RECT { x, top, x + scaler.ToPx (kLabelDip), top + toolbar };
    x += scaler.ToPx (kLabelDip);

    place (m_input, kInputDip);

    m_countRect = RECT { x, top, x + scaler.ToPx (kCountDip), top + toolbar };
    x += scaler.ToPx (kCountDip);

    place (m_next,      kStepDip);
    place (m_highlight, kHighlightDip);
    place (m_header,    kHeaderDip);

    m_map->Layout    (RECT { boundsPx.left, body, split - pad, bottom - line }, scaler);
    m_hoverRect      = RECT { boundsPx.left, bottom - line, split - pad, bottom };
    m_hex->Layout    (RECT { split, body, boundsPx.right, hexEnd }, scaler);
    m_fields->Layout (RECT { split, hexEnd + pad, boundsPx.right, bottom }, scaler);
    m_statusRect     = RECT { boundsPx.left, bottom + pad, boundsPx.right, boundsPx.bottom };
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorPanel::Paint
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorPanel::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    float         font  = m_scaler.ToPxf (kFontDip);
    std::wstring  count = L"of " + std::to_wstring (m_model != nullptr ? m_model->GetBlockCount() : 0);
    HRESULT       hr    = S_OK;



    auto  draw = [&] (const std::wstring & value, const RECT & rect, uint32_t color, DxuiTextHAlign align)
    {
        hr = text.DrawString (value.c_str(), (float) rect.left, (float) rect.top, (float) (rect.right - rect.left), (float) (rect.bottom - rect.top),
                              color, font, s_kpszFont, align, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
        IGNORE_RETURN_VALUE (hr, S_OK);
    };



    DxuiPanel::Paint (painter, text, theme);

    draw (L"Block", m_labelRect, theme.Foreground(), DxuiTextHAlign::Center);
    draw (count, m_countRect, theme.ForegroundMuted(), DxuiTextHAlign::Center);
    draw (m_hoverText.empty() ? std::wstring (L"Point at the map to see a block's role and files.") : m_hoverText,
          m_hoverRect, theme.ForegroundMuted(), DxuiTextHAlign::Left);
    draw (m_status, m_statusRect, theme.Foreground(), DxuiTextHAlign::Left);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorPanel::OnMouse
//
//  A press on the block number or the hex view focuses it.
//
////////////////////////////////////////////////////////////////////////////////

bool BlockInspectorPanel::OnMouse (const DxuiMouseEvent & ev)
{
    if (ev.kind == DxuiMouseEventKind::Down && m_onPressed)
    {
        for (IDxuiControl * control : { (IDxuiControl *) m_input, (IDxuiControl *) m_hex })
        {
            RECT  bounds = control->GetBounds();

            if (PtInRect (&bounds, ev.positionDip))
            {
                m_onPressed (control);
            }
        }
    }

    return DxuiPanel::OnMouse (ev);
}
