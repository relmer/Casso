#include "Pch.h"

#include "Ui/DiskInspector/InspectorWalkthrough.h"
#include "Ui/DiskInspector/DiskInspectorPalette.h"





static constexpr float     s_kPadDip       = 16.0f;
static constexpr float     s_kGapDip       = 12.0f;
static constexpr float     s_kTitleDip     = 16.0f;
static constexpr float     s_kBodyDip      = 13.0f;
static constexpr float     s_kButtonWDip   = 84.0f;
static constexpr float     s_kButtonHDip   = 28.0f;
static constexpr float     s_kOutlineDip   = 2.0f;
static constexpr uint32_t  s_kDimArgb      = 0xA0000000u;

static const InspectorWalkthrough::Step  s_kSteps[] =
{
    { WalkthroughArea::Platter, L"The platter",
      L"This is the disk, seen from above. Each ring is one quarter track: track 0 is the outermost ring and the innermost rings are near the hub. "
      L"Along each ring, colors mark what is written there: dark sync between sectors, then each sector's address field and data field.\n\n"
      L"Click a ring to select that track and the sector under the pointer. Use the mouse wheel to zoom in, drag to move around, and "
      L"double-click to see the whole disk again. Click Legend for a key to every color." },

    { WalkthroughArea::Toolbar, L"The toolbar",
      L"Click Drive 1 or Drive 2 to see the disk in that drive. Choose Structure to color the platter by what is written on it, or Timing to "
      L"color it by how fast or slow each stretch spins, on disk images that have that timing. Turn on Follow head to select the track the "
      L"drive's head is on.\n\n"
      L"Use Find to search the disk for bytes, nibbles or text, Compare with to set this disk beside another, and Decode settings to "
      L"change how sectors are decoded." },

    { WalkthroughArea::Track, L"The track and its sectors",
      L"The header has the selected track's number, what kind of track it is, and its size. Below it is one button per sector, in the "
      L"order the sectors pass under the head. A check mark means the sector was read without an error, and a red mark means it was not.\n\n"
      L"Click a sector to see its bytes." },

    { WalkthroughArea::Tabs, L"The tabs",
      L"Sector data has the selected sector's 256 bytes. Nibbles has every nibble on the track, in order. Fields has each "
      L"address and data field, and Flux timing has a graph of how long each bit cell takes.\n\n"
      L"The other tabs are about the whole disk: Tracks has every quarter track, Findings every problem found, File map which file owns "
      L"each sector, and Image the image file itself." },

    { WalkthroughArea::Strip, L"The strip",
      L"The strip is the selected track unrolled into a straight line, starting at the index. Each block is one nibble, in the same colors "
      L"as the platter.\n\n"
      L"Zoom in with + to see each nibble's value and, further in, each bit cell. Drag across the strip to select nibbles." },

    { WalkthroughArea::None, L"Tracks and quarter tracks",
      L"A 5.25-inch disk has 35 tracks: rings of magnetic recording around the disk. The drive's head can also stop a quarter of the way "
      L"between two tracks, so there is a ring for every quarter track, 160 in all.\n\n"
      L"On most disks only the whole tracks are written, and the head reads the same data from the quarter tracks beside each one." },

    { WalkthroughArea::None, L"Sectors",
      L"Each track is divided into sectors: 16 on a DOS 3.3 or ProDOS disk, and 13 on an older DOS 3.2 disk. A sector has 256 bytes "
      L"of data.\n\n"
      L"A file is stored as a list of sectors, so one file can be spread across many tracks. File map has which file owns each sector." },

    { WalkthroughArea::None, L"Nibbles",
      L"The drive cannot record every byte value as it is, so each byte is encoded as nibbles: 8-bit values whose top bit is always set "
      L"and that never have two zero bits in a row.\n\n"
      L"The 6-and-2 encoding turns a sector's 256 bytes into 342 nibbles. The 5-and-3 encoding, on 13-sector disks, turns them into 410." },

    { WalkthroughArea::None, L"Sync",
      L"Between sectors are runs of $FF sync nibbles, each followed by extra zero bits. They are there so the drive's hardware can line up "
      L"with the nibbles before a sector starts.\n\n"
      L"The longest run on a track usually marks the write seam: where the track was last written from start to end." },

    { WalkthroughArea::None, L"Address and data fields",
      L"Each sector has two fields. The address field starts with D5 AA 96 and has the volume, track and sector numbers and a checksum. "
      L"The data field starts with D5 AA AD and has the encoded bytes and their checksum. Both fields end with DE AA.\n\n"
      L"Copy protection often changes these marks or checksums, which is why a protected disk can look wrong here." },

    { WalkthroughArea::None, L"What the colors mean",
      L"Each kind of recording has its color: sync, address marks, address fields, data marks, data fields, failed checksums, noise and "
      L"random bits.\n\n"
      L"Click Legend for the key to every color, and hover over the platter or the strip for the colors under the pointer. Click ? at the "
      L"bottom of the window to see this walkthrough again." },
};





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorWalkthrough::GetSteps
//
////////////////////////////////////////////////////////////////////////////////

std::span<const InspectorWalkthrough::Step> InspectorWalkthrough::GetSteps()
{
    return s_kSteps;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorWalkthrough::Open
//
////////////////////////////////////////////////////////////////////////////////

void InspectorWalkthrough::Open()
{
    m_isOpen = true;
    m_step   = 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorWalkthrough::Back
//
////////////////////////////////////////////////////////////////////////////////

void InspectorWalkthrough::Back()
{
    m_step = std::max (m_step - 1, 0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorWalkthrough::Next
//
////////////////////////////////////////////////////////////////////////////////

void InspectorWalkthrough::Next()
{
    m_isOpen = m_step + 1 < static_cast<int> (std::size (s_kSteps));
    m_step   = m_isOpen ? m_step + 1 : 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorWalkthrough::GetArea
//
////////////////////////////////////////////////////////////////////////////////

WalkthroughArea InspectorWalkthrough::GetArea() const
{
    return s_kSteps[std::clamp (m_step, 0, static_cast<int> (std::size (s_kSteps)) - 1)].area;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorWalkthrough::Paint
//
//  The window dimmed in four bands around the area, an outline around it,
//  then the card: the title, the step count, the text, and Close, Back and
//  Next along the bottom.
//
////////////////////////////////////////////////////////////////////////////////

void InspectorWalkthrough::Paint (IDxuiTextRenderer & text, const IDxuiTheme & theme, const DxuiDpiScaler & scaler, const RECT & windowPx, const RECT & areaPx)
{
    const Step &  step    = s_kSteps[std::clamp (m_step, 0, static_cast<int> (std::size (s_kSteps)) - 1)];
    bool          hasArea = !IsRectEmpty (&areaPx);
    RECT          hole    = hasArea ? areaPx : RECT { windowPx.left, windowPx.top, windowPx.left, windowPx.top };
    float         pad     = scaler.ToPxf (s_kPadDip);
    float         width   = scaler.ToPxf (kCardWidthDip);
    float         buttonW = scaler.ToPxf (s_kButtonWDip);
    float         buttonH = scaler.ToPxf (s_kButtonHDip);
    float         titleH  = scaler.ToPxf (s_kTitleDip) * 1.6f;
    float         bodyW   = 0.0f;
    float         bodyH   = 0.0f;
    float         line    = scaler.ToPxf (s_kOutlineDip);
    std::wstring  count   = std::format (L"{} of {}", m_step + 1, std::size (s_kSteps));
    RECT          card    = {};
    float         x       = 0.0f;
    float         y       = 0.0f;
    bool          isLast  = m_step + 1 == static_cast<int> (std::size (s_kSteps));



    text.MeasureStringWrapped (step.body, scaler.ToPxf (s_kBodyDip), DxuiTheme::kBodyFace, width - 2 * pad, bodyW, bodyH);

    if (hasArea)
    {
        text.FillRect (static_cast<float> (windowPx.left), static_cast<float> (windowPx.top), static_cast<float> (windowPx.right - windowPx.left),
                       static_cast<float> (hole.top - windowPx.top), s_kDimArgb);
        text.FillRect (static_cast<float> (windowPx.left), static_cast<float> (hole.bottom), static_cast<float> (windowPx.right - windowPx.left),
                       static_cast<float> (windowPx.bottom - hole.bottom), s_kDimArgb);
        text.FillRect (static_cast<float> (windowPx.left), static_cast<float> (hole.top), static_cast<float> (hole.left - windowPx.left),
                       static_cast<float> (hole.bottom - hole.top), s_kDimArgb);
        text.FillRect (static_cast<float> (hole.right), static_cast<float> (hole.top), static_cast<float> (windowPx.right - hole.right),
                       static_cast<float> (hole.bottom - hole.top), s_kDimArgb);

        for (float edge : { static_cast<float> (hole.top), static_cast<float> (hole.bottom) - line })
        {
            text.FillRect (static_cast<float> (hole.left), edge, static_cast<float> (hole.right - hole.left), line, theme.Accent());
        }

        for (float edge : { static_cast<float> (hole.left), static_cast<float> (hole.right) - line })
        {
            text.FillRect (edge, static_cast<float> (hole.top), line, static_cast<float> (hole.bottom - hole.top), theme.Accent());
        }
    }
    else
    {
        text.FillRect (static_cast<float> (windowPx.left), static_cast<float> (windowPx.top), static_cast<float> (windowPx.right - windowPx.left),
                       static_cast<float> (windowPx.bottom - windowPx.top), s_kDimArgb);
    }

    card = PlaceCard (windowPx, hasArea ? areaPx : RECT {}, static_cast<int> (width), static_cast<int> (pad * 3 + titleH + bodyH + buttonH), static_cast<int> (scaler.ToPxf (s_kGapDip)));

    text.FillRect (static_cast<float> (card.left), static_cast<float> (card.top), static_cast<float> (card.right - card.left), static_cast<float> (card.bottom - card.top),
                   theme.BackgroundElevated());

    for (float edge : { static_cast<float> (card.top), static_cast<float> (card.bottom) - 1.0f })
    {
        text.FillRect (static_cast<float> (card.left), edge, static_cast<float> (card.right - card.left), 1.0f, theme.Border());
    }

    for (float edge : { static_cast<float> (card.left), static_cast<float> (card.right) - 1.0f })
    {
        text.FillRect (edge, static_cast<float> (card.top), 1.0f, static_cast<float> (card.bottom - card.top), theme.Border());
    }

    x = card.left + pad;
    y = card.top + pad;

    text.DrawString (step.title, x, y, width - 2 * pad, titleH, theme.HeadingForeground(), scaler.ToPxf (s_kTitleDip), DxuiTheme::kBodyFace,
                     DxuiTextHAlign::Left, DxuiTextVAlign::Top, DxuiFontWeight::SemiBold, false);
    text.DrawString (count.c_str(), x, y, width - 2 * pad, titleH, theme.ForegroundMuted(), scaler.ToPxf (s_kBodyDip), DxuiTheme::kBodyFace,
                     DxuiTextHAlign::Right, DxuiTextVAlign::Top, DxuiFontWeight::Normal, false);
    text.DrawString (step.body, x, y + titleH, width - 2 * pad, bodyH + pad, theme.Foreground(), scaler.ToPxf (s_kBodyDip), DxuiTheme::kBodyFace,
                     DxuiTextHAlign::Left, DxuiTextVAlign::Top, DxuiFontWeight::Normal, true);

    y         = card.bottom - pad - buttonH;
    m_closePx = { static_cast<LONG> (x), static_cast<LONG> (y), static_cast<LONG> (x + buttonW), static_cast<LONG> (y + buttonH) };
    m_nextPx  = { static_cast<LONG> (card.right - pad - buttonW), static_cast<LONG> (y), static_cast<LONG> (card.right - pad), static_cast<LONG> (y + buttonH) };
    m_backPx  = { static_cast<LONG> (m_nextPx.left - pad / 2 - buttonW), static_cast<LONG> (y), static_cast<LONG> (m_nextPx.left - pad / 2), static_cast<LONG> (y + buttonH) };

    for (const auto & [rect, label, isEnabled] : { std::tuple { m_closePx, L"Close", true },
                                                   std::tuple { m_backPx,  L"Back",  m_step > 0 },
                                                   std::tuple { m_nextPx,  isLast ? L"Done" : L"Next", true } })
    {
        text.FillRect (static_cast<float> (rect.left), static_cast<float> (rect.top), static_cast<float> (rect.right - rect.left), static_cast<float> (rect.bottom - rect.top),
                       (rect.left == m_nextPx.left) ? theme.Accent() : theme.ButtonIdle());
        text.DrawString (label, static_cast<float> (rect.left), static_cast<float> (rect.top), static_cast<float> (rect.right - rect.left),
                         static_cast<float> (rect.bottom - rect.top), (rect.left == m_nextPx.left) ? DiskInspectorPalette::GetTextColorOn (theme.Accent()) : (isEnabled ? theme.Foreground() : theme.ForegroundMuted()),
                         scaler.ToPxf (s_kBodyDip), DxuiTheme::kBodyFace, DxuiTextHAlign::Center, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorWalkthrough::HitTest
//
////////////////////////////////////////////////////////////////////////////////

WalkthroughButton InspectorWalkthrough::HitTest (POINT pointPx) const
{
    WalkthroughButton  button = WalkthroughButton::None;



    if (PtInRect (&m_closePx, pointPx))
    {
        button = WalkthroughButton::Close;
    }
    else if (PtInRect (&m_backPx, pointPx) && m_step > 0)
    {
        button = WalkthroughButton::Back;
    }
    else if (PtInRect (&m_nextPx, pointPx))
    {
        button = WalkthroughButton::Next;
    }

    return button;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorWalkthrough::PlaceCard
//
////////////////////////////////////////////////////////////////////////////////

RECT InspectorWalkthrough::PlaceCard (const RECT & windowPx, const RECT & areaPx, int widthPx, int heightPx, int gapPx)
{
    int   midX     = (windowPx.left + windowPx.right - widthPx) / 2;
    int   midY     = (windowPx.top + windowPx.bottom - heightPx) / 2;
    RECT  centered = { midX, midY, midX + widthPx, midY + heightPx };
    RECT  placed   = centered;
    int   y        = std::clamp (static_cast<int> (areaPx.top), static_cast<int> (windowPx.top) + gapPx, static_cast<int> (windowPx.bottom) - heightPx - gapPx);
    int   x        = std::clamp (static_cast<int> (areaPx.left), static_cast<int> (windowPx.left) + gapPx, static_cast<int> (windowPx.right) - widthPx - gapPx);



    if (IsRectEmpty (&areaPx))
    {
        placed = centered;
    }
    else if (areaPx.right + gapPx + widthPx <= windowPx.right - gapPx)
    {
        placed = { areaPx.right + gapPx, y, areaPx.right + gapPx + widthPx, y + heightPx };
    }
    else if (areaPx.left - gapPx - widthPx >= windowPx.left + gapPx)
    {
        placed = { areaPx.left - gapPx - widthPx, y, areaPx.left - gapPx, y + heightPx };
    }
    else if (areaPx.bottom + gapPx + heightPx <= windowPx.bottom - gapPx)
    {
        placed = { x, areaPx.bottom + gapPx, x + widthPx, areaPx.bottom + gapPx + heightPx };
    }
    else if (areaPx.top - gapPx - heightPx >= windowPx.top + gapPx)
    {
        placed = { x, areaPx.top - gapPx - heightPx, x + widthPx, areaPx.top - gapPx };
    }

    return placed;
}
