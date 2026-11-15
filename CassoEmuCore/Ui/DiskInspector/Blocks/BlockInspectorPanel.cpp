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

void BlockInspectorPanel::Init (BlockInspectorModel * model, BlockInspectorWorker * worker, const IDxuiTheme * theme, HWND hwnd, IDxuiTextRenderer * text)
{
    constexpr uint32_t  kChangedColor = 0xFFE0A030;
    constexpr uint32_t  kDiffersColor = 0xFFF06060;
    constexpr uint32_t  kFieldColor   = 0xFF60B0F0;
    constexpr uint32_t  kLinkColor    = 0xFF60D090;
    constexpr size_t    kInputChars   = 8;



    m_model  = model;
    m_worker = worker;

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
    m_cancel    = CreateChild<DxuiButton> (L"Cancel");

    m_cancel->SetVisible (false);
    m_cancel->SetOnClick ([this]()
    {
        if (m_worker != nullptr)
        {
            m_worker->Cancel();
        }
    });

    m_back->SetOnClick     ([this]() { GoBack(); });
    m_forward->SetOnClick  ([this]() { GoForward(); });
    m_previous->SetOnClick ([this]() { Step (-1); });
    m_next->SetOnClick     ([this]() { Step (1); });

    m_highlight->SetOnClick ([this]()
    {
        if (IsBusy())
        {
            return;
        }

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
        if (IsBusy())
        {
            return;
        }

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
        if (!IsBusy() && !m_isHeader && m_model->IsLoaded (m_model->GetBlock()) && m_model->TryFollowLink ((int) offset))
        {
            m_isTyping = false;
            Refresh();
        }
    });

    m_hex->SetMarkColor ([] (uint8_t mark, uint32_t & outArgb)
    {
        outArgb = mark == BlockHexSource::kMarkChanged ? kChangedColor
                : mark == BlockHexSource::kMarkDiffers ? kDiffersColor
                : mark == BlockHexSource::kMarkField   ? kFieldColor
                :                                        kLinkColor;

        return mark != BlockHexSource::kMarkNone;
    });

    m_fields->SetOnHover  ([this] (const BlockField * field)
    {
        if (IsBusy())
        {
            return;
        }

        MarkField (field);

        if (m_onChanged)
        {
            m_onChanged();
        }
    });

    m_fields->SetOnFollow ([this] (const BlockField * field)
    {
        if (field != nullptr && field->link != BlockField::kNoLink && !m_isHeader && !IsBusy())
        {
            SelectBlock ((uint32_t) field->link);
        }
    });

    InitSecondRow();
    Refresh();
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorPanel::InitSecondRow
//
//  The differences while comparing, and the leftover data in free blocks.
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorPanel::InitSecondRow()
{
    m_prevDiff  = CreateChild<DxuiButton> (L"Previous difference");
    m_nextDiff  = CreateChild<DxuiButton> (L"Next difference");
    m_leftovers = CreateChild<DxuiButton> (L"Find leftover data");
    m_saveRun   = CreateChild<DxuiButton> (L"Save leftover as file");

    for (const auto & [button, isForward] : { std::pair<DxuiButton *, bool> (m_prevDiff, false), std::pair<DxuiButton *, bool> (m_nextDiff, true) })
    {
        button->SetOnClick ([this, isForward]()
        {
            uint32_t  block = 0;

            if (!IsBusy() && m_model->FindDifference (isForward, block))
            {
                SelectBlock (block);
            }
        });
    }

    m_leftovers->SetOnClick ([this]()
    {
        BlockInspectorModel  * model = m_model;

        if (IsBusy())
        {
            return;
        }

        if (m_model->HasLeftovers())
        {
            m_model->ClearLeftovers();
            Refresh();
            return;
        }

        QueueJob ({ L"Finding leftover data", true,
                    [model] (BlockJobProgress & progress) { model->FindLeftovers (&progress); },
                    [this] () { Refresh(); } });
    });

    m_saveRun->SetOnClick ([this]()
    {
        const LeftoverRun  * run = IsBusy() ? nullptr : m_model->FindLeftoverRun (m_model->GetBlock());

        if (run != nullptr && m_onSaveRun)
        {
            m_onSaveRun (*run);
        }
    });
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



    if (IsBusy() || !m_model->IsOpen() || !TryParseBlock (text, block) || block >= m_model->GetBlockCount())
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

    Refresh();
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorPanel::SelectBlock
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorPanel::SelectBlock (uint32_t block)
{
    if (IsBusy() || !m_model->IsOpen())
    {
        return;
    }

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
    if (IsBusy() || !m_model->IsOpen())
    {
        return;
    }

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
    if (IsBusy() || !m_model->IsOpen())
    {
        return;
    }

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
    if (IsBusy() || !m_model->IsOpen())
    {
        return;
    }

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
//  The header isn't edited here, and a block is edited only once it is
//  loaded, so typing reads the block from memory.
//
////////////////////////////////////////////////////////////////////////////////

bool BlockInspectorPanel::OnType (uint64_t offset, bool isHexColumn, bool isHighNibble, wchar_t ch)
{
    bool  isChanged = false;



    if (m_isHeader || IsBusy() || !m_model->IsOpen() || !m_model->IsLoaded (m_model->GetBlock()))
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
    BlockInspectorModel::Block  other  = {};
    vector<BlockField>          fields;
    uint32_t                    block  = m_model->GetBlock();
    int64_t                     shown  = m_isHeader ? -2 : (int64_t) block;
    HRESULT                     hr     = S_OK;
    size_t                      i      = 0;



    if (m_isHeader)
    {
        m_source.SetBytes (m_model->GetFile().GetHeader());
        fields = m_model->GetHeaderFields();
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

        if (m_model->IsDifferent (block))
        {
            hr = m_model->ReadOtherBlock (block, other);
            IGNORE_RETURN_VALUE (hr, S_OK);

            for (i = 0; i < bytes.size(); i++)
            {
                if (bytes[i] != other[i])
                {
                    m_source.SetMark (i, BlockHexSource::kMarkDiffers);
                }
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
    BlockInspectorModel::Tags                                tags   = {};
    int                                                      edits  = m_model->GetEdits().CountChangedBytes();
    wchar_t                                                  hex[4] = {};
    LPCWSTR                                                  label  = m_model->GetMapLabel (block);



    std::replace (owners.begin(), owners.end(), L'\n', L',');

    if (label == nullptr)
    {
        label = BlockMapGrid::GetRoleName (m_model->GetRole (block));
    }

    m_status = L"Block " + std::to_wstring (block) + L": " + label;

    if (!owners.empty())
    {
        m_status += L". " + owners;
    }

    if (m_model->TryGetTags (block, tags))
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

    AppendSecondRowStatus (block);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorPanel::AppendSecondRowStatus
//
//  Whether the block differs from the other image, and the leftover run it
//  is in.
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorPanel::AppendSecondRowStatus (uint32_t block)
{
    const LeftoverRun  * run    = m_model->FindLeftoverRun (block);
    std::wstring         other  = m_model->IsComparing() ? fs::path (m_model->GetOther()->GetPath()).filename().wstring() : std::wstring();
    size_t               differ = m_model->IsComparing() ? m_model->CountDifferent() : 0;
    size_t               runs   = m_model->HasLeftovers() ? m_model->GetLeftovers().size() : 0;



    if (m_model->IsComparing())
    {
        m_status += m_model->IsDifferent (block) ? L". Differs from " + other : L". Same as " + other;
        m_status += differ == 0 ? std::wstring (L" (no blocks differ)") : differ == 1 ? std::wstring (L" (1 block differs)") : std::format (L" ({} blocks differ)", differ);
    }

    if (run != nullptr)
    {
        m_status += L". Leftover " + std::wstring (BlockLeftovers::GetKindName (run->kind)) + L" in free blocks "
                  + std::to_wstring (run->first) + L" to " + std::to_wstring (run->first + run->count - 1);
    }
    else if (m_model->HasLeftovers())
    {
        m_status += runs == 0 ? std::wstring (L". No leftover data in the free blocks") : runs == 1 ? std::wstring (L". 1 run of leftover data") : std::format (L". {} runs of leftover data", runs);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorPanel::Refresh
//
//  A block not loaded yet is loaded by a job, and drawn when it ends. The
//  last block drawn stays on screen until then.
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorPanel::Refresh()
{
    BlockInspectorModel  * model = m_model;
    uint32_t               block = 0;



    if (m_model == nullptr || IsBusy() || !m_model->IsOpen())
    {
        return;
    }

    block = m_model->GetBlock();

    if (!m_isHeader && !m_model->IsLoaded (block))
    {
        QueueJob ({ L"", false,
                    [model, block] (BlockJobProgress & progress) { (void) progress; model->LoadBlock (block); },
                    [this] () { Refresh(); } });
        return;
    }

    Present();
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorPanel::QueueJob
//
//  With no worker, as in a test, the job runs on the calling thread.
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorPanel::QueueJob (BlockInspectorWorker::Job job)
{
    BlockJobProgress  progress;



    if (m_worker == nullptr)
    {
        job.work (progress);
        job.finish();
        return;
    }

    m_worker->Queue (std::move (job));
    UpdateBusy();

    if (m_onQueued)
    {
        m_onQueued();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorPanel::UpdateBusy
//
//  While a long job is queued or running, every control but Cancel is
//  disabled and the map isn't laid out again from the model. A short job,
//  such as loading a block, changes nothing on screen.
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorPanel::UpdateBusy()
{
    bool  canCancel = false;
    bool  wasLong   = m_isLongJob;



    if (m_worker == nullptr)
    {
        return;
    }

    m_isLongJob = m_worker->TryGetLongJob (m_jobLabel, canCancel, m_jobDone, m_jobTotal);

    m_map->SetFrozen     (IsBusy());
    m_cancel->SetVisible (m_isLongJob && canCancel);

    if (m_isLongJob)
    {
        for (DxuiButton * button : { m_back, m_forward, m_previous, m_next, m_highlight, m_header, m_prevDiff, m_nextDiff, m_leftovers, m_saveRun })
        {
            button->SetEnabled (false);
        }

        m_input->SetEnabled (false);
    }
    else if (wasLong)
    {
        m_input->SetEnabled     (true);
        m_highlight->SetEnabled (true);
        m_header->SetEnabled    (true);
        m_leftovers->SetEnabled (true);
    }

    if (m_onChanged && (m_isLongJob || wasLong))
    {
        m_onChanged();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorPanel::Present
//
//  The current block, loaded, and the controls set from the model.
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorPanel::Present()
{
    ShowBytes();
    BuildStatus();

    m_blockCount = m_model->GetBlockCount();

    if (!m_input->IsFocused() || !m_isTyping)
    {
        m_input->SetText (std::to_wstring (m_model->GetBlock()));
    }

    m_back->SetEnabled      (m_model->CanGoBack());
    m_forward->SetEnabled   (m_model->CanGoForward());
    m_previous->SetEnabled  (m_model->GetBlock() > 0);
    m_next->SetEnabled      (m_model->GetBlock() + 1 < m_model->GetBlockCount());
    m_prevDiff->SetVisible  (m_model->IsComparing());
    m_nextDiff->SetVisible  (m_model->IsComparing());
    m_prevDiff->SetEnabled  (m_model->CountDifferent() > 0);
    m_nextDiff->SetEnabled  (m_model->CountDifferent() > 0);
    m_leftovers->SetLabel   (m_model->HasLeftovers() ? L"Clear leftover marks" : L"Find leftover data");
    m_saveRun->SetEnabled   (m_model->FindLeftoverRun (m_model->GetBlock()) != nullptr);
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
    constexpr float  kDiffDip      = 140.0f;
    constexpr float  kLeftoverDip  = 150.0f;
    constexpr float  kSaveDip      = 160.0f;



    LONG  pad     = scaler.ToPx (kPadDip);
    LONG  toolbar = scaler.ToPx (kToolbarDip);
    LONG  gap     = scaler.ToPx (kButtonGap);
    LONG  line    = scaler.ToPx (kLineDip);
    LONG  x       = boundsPx.left;
    LONG  top     = boundsPx.top;
    LONG  body    = top + (toolbar + pad) * 2;
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

    x    = boundsPx.left;
    top += toolbar + pad;

    place (m_prevDiff,  kDiffDip);
    place (m_nextDiff,  kDiffDip);
    place (m_leftovers, kLeftoverDip);
    place (m_saveRun,   kSaveDip);

    m_map->Layout    (RECT { boundsPx.left, body, split - pad, bottom - line }, scaler);
    m_hoverRect      = RECT { boundsPx.left, bottom - line, split - pad, bottom };
    m_hex->Layout    (RECT { split, body, boundsPx.right, hexEnd }, scaler);
    m_fields->Layout (RECT { split, hexEnd + pad, boundsPx.right, bottom }, scaler);
    m_statusRect     = RECT { boundsPx.left, bottom + pad, boundsPx.right, boundsPx.bottom };

    m_cancel->Layout (RECT { boundsPx.right - scaler.ToPx (kCancelDip), bottom + pad, boundsPx.right, boundsPx.bottom }, scaler);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorPanel::Paint
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorPanel::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    float         font  = m_scaler.ToPxf (kFontDip);
    std::wstring  count = L"of " + std::to_wstring (m_blockCount);
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

    if (m_isLongJob)
    {
        PaintProgress (painter, text, theme);
    }
    else
    {
        draw (m_status, m_statusRect, theme.Foreground(), DxuiTextHAlign::Left);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorPanel::PaintProgress
//
//  A long job's bar and label in place of the status line. A job with no
//  count yet has an empty bar.
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorPanel::PaintProgress (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    constexpr float  kBarHighDip = 8.0f;
    constexpr float  kGapDip     = 10.0f;
    constexpr int    kPercent    = 100;



    float         left    = (float) m_statusRect.left;
    float         top     = (float) m_statusRect.top;
    float         height  = (float) (m_statusRect.bottom - m_statusRect.top);
    float         barW    = m_scaler.ToPxf (kBarDip);
    float         barH    = m_scaler.ToPxf (kBarHighDip);
    float         barTop  = top + (height - barH) / 2.0f;
    double        share   = m_jobTotal > 0 ? std::min (1.0, (double) m_jobDone / (double) m_jobTotal) : 0.0;
    std::wstring  label   = m_jobTotal > 0 ? std::format (L"{} ({}%)", m_jobLabel, (int) (share * kPercent)) : m_jobLabel;
    float         textX   = left + barW + m_scaler.ToPxf (kGapDip);
    float         textW   = (float) m_statusRect.right - textX - m_scaler.ToPxf (kCancelDip + kGapDip);
    HRESULT       hr      = S_OK;



    painter.FillRoundedRect    (left, barTop, barW, barH, barH / 2, theme.ButtonIdle());
    painter.FillRoundedRect    (left, barTop, (float) (barW * share), barH, barH / 2, theme.Accent());
    painter.OutlineRoundedRect (left, barTop, barW, barH, barH / 2, 1.0f, theme.ButtonBorder());

    hr = text.DrawString (label.c_str(), textX, top, std::max (0.0f, textW), height, theme.Foreground(), m_scaler.ToPxf (kFontDip), s_kpszFont,
                          DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorPanel::OnMouse
//
//  A press on the block number or the hex view focuses it. While a job is
//  queued or running, only Cancel gets mouse events, so pointing at the map
//  or a field doesn't read the model.
//
////////////////////////////////////////////////////////////////////////////////

bool BlockInspectorPanel::OnMouse (const DxuiMouseEvent & ev)
{
    RECT  cancel = m_cancel->GetBounds();



    if (IsBusy())
    {
        return m_cancel->IsVisible() && PtInRect (&cancel, ev.positionDip) ? DxuiPanel::OnMouse (ev) : true;
    }

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
