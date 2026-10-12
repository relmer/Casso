#include "Pch.h"

#include "Ui/Chrome/DriveWidget.h"
#include "Core/UnicodeSymbols.h"
#include "../Dxui/MockDxuiPainter.h"
#include "../Dxui/MockDxuiTextRenderer.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DriveWidgetNameRowTests
//
//  Where the 2D name row puts the badge before the name, the name, and the
//  info icon after it. With neither part the name keeps the whole strip, as
//  it always had; with the badge alone the arithmetic is the padlock's own,
//  unchanged.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DriveWidgetNameRowTests)
{
public:

    static constexpr float  kLeft  = 100.0f;
    static constexpr float  kWidth = 140.0f;
    static constexpr float  kGap   = 4.0f;
    static constexpr float  kBadge = 13.0f;
    static constexpr float  kIcon  = 12.0f;


    TEST_METHOD (NoPartsLeavesTheWholeStrip)
    {
        DriveNameRowLayout  row = DriveWidget::LayoutNameRow (kLeft, kWidth, 60.0f, 0.0f, 0.0f, kGap);

        Assert::IsTrue   (row.fits);
        Assert::AreEqual (kLeft,  row.nameLeft);
        Assert::AreEqual (kWidth, row.nameW);
    }


    TEST_METHOD (BadgeAloneCentersThePairAsBefore)
    {
        DriveNameRowLayout  row = DriveWidget::LayoutNameRow (kLeft, kWidth, 60.0f, kBadge, 0.0f, kGap);

        Assert::IsTrue   (row.fits);
        Assert::AreEqual (kLeft + (kWidth - (kBadge + kGap + 60.0f)) * 0.5f, row.badgeX);
        Assert::AreEqual (row.badgeX + kBadge + kGap,                        row.nameLeft);
        Assert::AreEqual (60.0f,                                              row.nameW);
    }


    TEST_METHOD (IconFollowsTheNameAndTheGroupCenters)
    {
        DriveNameRowLayout  row = DriveWidget::LayoutNameRow (kLeft, kWidth, 60.0f, 0.0f, kIcon, kGap);
        float               end = row.iconX + kIcon;

        Assert::IsTrue   (row.fits);
        Assert::AreEqual (row.nameLeft + 60.0f + kGap, row.iconX, L"the icon sits a gap after the name");
        Assert::AreEqual (kLeft - row.nameLeft, end - (kLeft + kWidth), 0.001f,
                          L"the name and icon center together in the strip");
    }


    TEST_METHOD (BothPartsCenterAsAGroup)
    {
        DriveNameRowLayout  row = DriveWidget::LayoutNameRow (kLeft, kWidth, 60.0f, kBadge, kIcon, kGap);

        Assert::IsTrue   (row.fits);
        Assert::AreEqual (row.badgeX + kBadge + kGap,  row.nameLeft);
        Assert::AreEqual (row.nameLeft + 60.0f + kGap, row.iconX);
        Assert::AreEqual (row.badgeX - kLeft, (kLeft + kWidth) - (row.iconX + kIcon), 0.001f);
    }


    TEST_METHOD (OverflowPinsTheIconRightAndMarqueesBetween)
    {
        DriveNameRowLayout  row = DriveWidget::LayoutNameRow (kLeft, kWidth, 400.0f, kBadge, kIcon, kGap);

        Assert::IsFalse  (row.fits);
        Assert::AreEqual (kLeft,                         row.badgeX);
        Assert::AreEqual (kLeft + kBadge + kGap,         row.nameLeft);
        Assert::AreEqual (kLeft + kWidth - kIcon,        row.iconX);
        Assert::AreEqual (row.iconX - kGap,              row.nameLeft + row.nameW,
                          L"the marquee's clip ends a gap before the icon");
    }
};





////////////////////////////////////////////////////////////////////////////////
//
//  DriveWidgetInfoIconTests
//
//  The info icon on a painted widget: it appears only for a conflict, it is
//  its own click and hover target inside the eject band, and its target goes
//  away with it.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DriveWidgetInfoIconTests)
{
public:

    static void PaintWith (DriveWidget & drive, MockDxuiTextRenderer & text, const wchar_t * path, bool conflict)
    {
        DriveWidgetState      state;
        DxuiDpiScaler         scaler;
        MockDxuiPainter       painter;
        CassoTheme            theme  = CassoTheme::MakeDarkModern();
        RECT                  anchor = { 100, 200, 100, 200 };

        scaler.SetDpi (96);
        drive.Initialize (6, 0, nullptr);
        drive.Layout (anchor, scaler);

        state.mountedImagePath = path;
        state.wozConflict      = conflict;

        // The first sync only records the name; a second one with the same
        // name starts no roll, so the paint below is the steady label.
        drive.SyncFromState (state);
        drive.SyncFromState (state);
        drive.Paint (painter, text, theme);
    }


    static bool DrewInfoGlyph (const MockDxuiTextRenderer & text)
    {
        for (const RecordedTextCall & call : text.Calls())
        {
            if (call.kind == RecordedTextKind::DrawString && call.text == s_kpszMdl2Info)
            {
                return true;
            }
        }

        return false;
    }


    static bool FilledWith (const MockDxuiPainter & painter, uint32_t argb)
    {
        for (const RecordedPaintCall & call : painter.Calls())
        {
            if (call.kind == RecordedPaintKind::FillRect && call.argb == argb)
            {
                return true;
            }
        }

        return false;
    }


    TEST_METHOD (ConflictDrawsTheIconAndClaimsItsTarget)
    {
        DriveWidget           drive;
        MockDxuiTextRenderer  text;
        RECT                  icon  = {};
        RECT                  eject = {};

        PaintWith (drive, text, L"C:\\Disks\\Choplifter.woz", true);

        icon  = drive.GetInfoIconRect();
        eject = drive.GetEjectRect();

        Assert::IsTrue  (DrewInfoGlyph (text));
        Assert::IsFalse (IsRectEmpty (&icon) != FALSE);
        Assert::IsTrue  (drive.HitTest ((icon.left + icon.right) / 2, (icon.top + icon.bottom) / 2) == DriveWidgetRegion::Info,
                         L"a click on the icon must not eject the disk");
        Assert::IsTrue  (drive.HitTest (eject.left + 1, (eject.top + eject.bottom) / 2) == DriveWidgetRegion::Eject,
                         L"the rest of the band still ejects");
    }


    TEST_METHOD (NoConflictShowsNoIcon)
    {
        DriveWidget           drive;
        MockDxuiTextRenderer  text;
        RECT                  icon = {};

        PaintWith (drive, text, L"C:\\Disks\\Choplifter.woz", false);
        icon = drive.GetInfoIconRect();

        Assert::IsFalse (DrewInfoGlyph (text));
        Assert::IsTrue  (IsRectEmpty (&icon) != FALSE);
    }


    TEST_METHOD (EmptyDriveShowsNoIconEvenWithAStaleConflict)
    {
        DriveWidget           drive;
        MockDxuiTextRenderer  text;
        RECT                  icon = {};

        PaintWith (drive, text, L"", true);
        icon = drive.GetInfoIconRect();

        Assert::IsFalse (DrewInfoGlyph (text));
        Assert::IsTrue  (IsRectEmpty (&icon) != FALSE);
    }


    TEST_METHOD (HideDropsTheTarget)
    {
        DriveWidget           drive;
        MockDxuiTextRenderer  text;
        RECT                  icon = {};

        PaintWith (drive, text, L"C:\\Disks\\Choplifter.woz", true);
        drive.Hide();
        icon = drive.GetInfoIconRect();

        Assert::IsTrue (IsRectEmpty (&icon) != FALSE);
    }


    TEST_METHOD (ARepaintAfterTheConflictClearsDropsTheTarget)
    {
        DriveWidget           drive;
        MockDxuiTextRenderer  text;
        MockDxuiPainter       painter;
        DriveWidgetState      cleared;
        CassoTheme            theme = CassoTheme::MakeDarkModern();
        RECT                  shown = {};
        RECT                  icon  = {};

        PaintWith (drive, text, L"C:\\Disks\\Choplifter.woz", true);
        shown = drive.GetInfoIconRect();

        // A switch to a machine the image fits clears the conflict, and the
        // same widget paints again.
        cleared.mountedImagePath = L"C:\\Disks\\Choplifter.woz";
        drive.SyncFromState (cleared);
        drive.Paint (painter, text, theme);
        icon = drive.GetInfoIconRect();

        Assert::IsFalse (IsRectEmpty (&shown) != FALSE);
        Assert::IsTrue  (IsRectEmpty (&icon) != FALSE);
        Assert::IsTrue  (drive.HitTest ((shown.left + shown.right) / 2, (shown.top + shown.bottom) / 2) == DriveWidgetRegion::Eject,
                         L"where the icon was, a click ejects again");
    }


    TEST_METHOD (ThePointerOnTheIconLightsNoBand)
    {
        DriveWidget           drive;
        MockDxuiTextRenderer  text;
        MockDxuiPainter       onBand;
        MockDxuiPainter       onIcon;
        CassoTheme            theme = CassoTheme::MakeDarkModern();

        PaintWith (drive, text, L"C:\\Disks\\Choplifter.woz", true);

        Assert::IsTrue (drive.UpdateMarqueeHover (true, false, 100), L"on the band, the highlight comes on");
        drive.Paint (onBand, text, theme);

        Assert::IsTrue (drive.UpdateMarqueeHover (true, true, 110), L"moving onto the icon takes it off");
        drive.Paint (onIcon, text, theme);

        Assert::IsTrue  (FilledWith (onBand, theme.buttonHover));
        Assert::IsFalse (FilledWith (onIcon, theme.buttonHover), L"a click on the icon does nothing, so nothing promises one");
    }
};
