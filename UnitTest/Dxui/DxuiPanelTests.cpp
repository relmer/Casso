#include "Pch.h"

#include "MockDxuiControl.h"
#include "MockDxuiPainter.h"
#include "MockDxuiTextRenderer.h"
#include "MockDxuiTheme.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPanelTests
//
//  The panel tree's contracts: child ownership, adoption, and the paint and
//  layout fan-outs.
//
//  The FAN-OUT is a documented guarantee, not an implementation detail -- a
//  panel paints every visible child in order and lays out every child, and
//  widgets are written assuming it. So the tests use stub children with no
//  bounds and assert they are still visited: adding a bounds guard here to
//  paper over a widget's own bug would silently change the framework's
//  promise, and these are what catch that.
//
//  Adoption is covered separately from ownership because adopted children are
//  raw pointers the panel does not own -- the walks must include them while the
//  destruction must not.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiPanelTests)
{
public:

    //  Each Paint (false) and PaintAfterSiblings (true), in the order they ran.
    using PaintLog = std::vector<std::pair<const MockDxuiControl *, bool>>;

    RECT  MakeRect (LONG l, LONG t, LONG r, LONG b)
    {
        RECT  out = {};
        out.left = l; out.top = t; out.right = r; out.bottom = b;
        return out;
    }

    TEST_METHOD (Add_ReturnsReferenceToConstructedChild)
    {
        DxuiPanel          panel;
        MockDxuiControl &  child = panel.Add<MockDxuiControl>();


        Assert::AreEqual ((size_t) 1, panel.GetChildCount());
        Assert::AreEqual (static_cast<void *> (&child), static_cast<void *> (panel.GetChild (0)));
        Assert::AreEqual (static_cast<void *> (&panel), static_cast<void *> (child.GetParent()));
    }


    TEST_METHOD (Remove_NullFails)
    {
        DxuiPanel  panel;

        HRESULT  hr = panel.Remove (nullptr);

        Assert::AreEqual (E_INVALIDARG, hr);
    }


    TEST_METHOD (Remove_UnknownFails)
    {
        DxuiPanel        panel;
        MockDxuiControl  stranger;

        HRESULT  hr = panel.Remove (&stranger);

        Assert::IsTrue (FAILED (hr));
    }


    TEST_METHOD (Remove_KnownSucceedsAndDestroysChild)
    {
        DxuiPanel          panel;
        MockDxuiControl &  child = panel.Add<MockDxuiControl>();

        HRESULT  hr = panel.Remove (&child);

        Assert::IsTrue   (SUCCEEDED (hr));
        Assert::AreEqual ((size_t) 0, panel.GetChildCount());
    }


    //  R-023: a pane moves between dock sites, and between windows, as the same
    //  control, keeping whatever state it holds.
    TEST_METHOD (DetachChild_MovesTheSameControlToAnotherPanel)
    {
        DxuiPanel                      from;
        DxuiPanel                      to;
        DxuiDpiScaler                  scaler;
        MockDxuiControl              * child    = from.CreateChild<MockDxuiControl>();
        std::unique_ptr<IDxuiControl>  detached;



        child->tickCount = 7;
        detached         = from.DetachChild (child);

        Assert::IsTrue   (detached.get() == child,     L"the same control, not a copy");
        Assert::AreEqual ((size_t) 0, from.GetChildCount());
        Assert::IsNull   (child->GetParent());

        to.AttachChild (std::move (detached));
        to.Layout      (RECT { 0, 0, 100, 50 }, scaler);

        Assert::IsTrue   (to.GetChild (0) == child);
        Assert::IsTrue   (child->GetParent() == &to);
        Assert::AreEqual (7, child->tickCount,         L"its state came with it");
        Assert::IsTrue   (child->layoutCount > 0,      L"laid out by the new panel");
    }


    TEST_METHOD (DetachChild_OfAChildNotOwnedIsNull)
    {
        DxuiPanel        panel;
        MockDxuiControl  adopted;
        MockDxuiControl  stranger;



        panel.Adopt (adopted);

        Assert::IsNull   (panel.DetachChild (&adopted).get(), L"an adopted child belongs to its caller");
        Assert::IsNull   (panel.DetachChild (&stranger).get());
        Assert::AreEqual ((size_t) 1, panel.GetChildCount());
        panel.ClearAdopted();
    }


    TEST_METHOD (Clear_DropsAllChildren)
    {
        DxuiPanel  panel;

        panel.Add<MockDxuiControl>();
        panel.Add<MockDxuiControl>();
        panel.Add<MockDxuiControl>();
        panel.Clear();

        Assert::AreEqual ((size_t) 0, panel.GetChildCount());
    }


    TEST_METHOD (Paint_FanoutsToVisibleChildrenInOrder)
    {
        DxuiPanel             panel;
        MockDxuiControl &     a       = panel.Add<MockDxuiControl>();
        MockDxuiControl &     b       = panel.Add<MockDxuiControl>();
        MockDxuiControl &     c       = panel.Add<MockDxuiControl>();
        MockDxuiPainter       painter;
        MockDxuiTextRenderer  text;
        MockDxuiTheme         theme;


        b.SetVisible (false);
        panel.Paint (painter, text, theme);

        Assert::AreEqual (1, a.paintCount);
        Assert::AreEqual (0, b.paintCount);
        Assert::AreEqual (1, c.paintCount);
    }


    TEST_METHOD (OnMouse_DispatchesFrontToBackAndStopsAtConsumer)
    {
        DxuiPanel          panel;
        MockDxuiControl &  a  = panel.Add<MockDxuiControl>();
        MockDxuiControl &  b  = panel.Add<MockDxuiControl>();
        MockDxuiControl &  c  = panel.Add<MockDxuiControl>();
        DxuiMouseEvent     ev;


        b.consumeMouse = true;
        Assert::IsTrue (panel.OnMouse (ev));

        // c is rear-most insertion, but front-to-back means last-added first.
        Assert::AreEqual (1, c.mouseCount);
        Assert::AreEqual (1, b.mouseCount);
        Assert::AreEqual (0, a.mouseCount);
    }


    TEST_METHOD (SetVisibleFalse_SkipsChildInPaintAndInput)
    {
        DxuiPanel             panel;
        MockDxuiControl &     hidden  = panel.Add<MockDxuiControl>();
        MockDxuiPainter       painter;
        MockDxuiTextRenderer  text;
        MockDxuiTheme         theme;
        DxuiMouseEvent        ev;


        hidden.SetVisible (false);
        panel.Paint   (painter, text, theme);
        panel.OnMouse (ev);

        Assert::AreEqual (0, hidden.paintCount);
        Assert::AreEqual (0, hidden.mouseCount);
    }


    TEST_METHOD (SetVisibleFalse_MarksPanelDirty)
    {
        DxuiPanel          panel;
        MockDxuiControl &  child = panel.Add<MockDxuiControl>();


        // Add already marked dirty; clear it then toggle visibility.
        panel.ClearDirty();
        Assert::IsFalse (panel.IsDirty());

        child.SetVisible (false);
        Assert::IsTrue (panel.IsDirty());
    }


    TEST_METHOD (Layout_AsksLayoutPolicyToArrangeOnlyVisibleChildren)
    {
        DxuiPanel          panel;
        DxuiDpiScaler      scaler;
        RECT               bounds = MakeRect (0, 0, 200, 100);


        panel.Add<MockDxuiControl>();
        panel.Add<MockDxuiControl>().SetVisible (false);
        panel.Add<MockDxuiControl>();
        panel.SetLayout (std::make_unique<DxuiStackLayout> (DxuiStackLayout::Orientation::Horizontal,
                                                            0.0f,
                                                            DxuiStackLayout::Align::Stretch));

        panel.Layout (bounds, scaler);

        // With no weights, the natural sizes are zero for default-constructed
        // controls; the visible children stack at the same x without errors.
        Assert::AreEqual ((size_t) 3, panel.GetChildCount());
        Assert::IsFalse (panel.IsDirty());
    }


    TEST_METHOD (OnThemeChanged_FanoutsToAllChildren)
    {
        DxuiPanel          panel;
        MockDxuiControl &  a = panel.Add<MockDxuiControl>();
        MockDxuiControl &  b = panel.Add<MockDxuiControl>();


        panel.OnThemeChanged();

        Assert::AreEqual (1, a.themeChangedCount);
        Assert::AreEqual (1, b.themeChangedCount);
    }


    TEST_METHOD (Tick_FanoutsToAllChildrenIncludingHidden)
    {
        DxuiPanel          panel;
        MockDxuiControl &  a = panel.Add<MockDxuiControl>();
        MockDxuiControl &  b = panel.Add<MockDxuiControl>();


        b.SetVisible (false);
        panel.Tick (1000);

        Assert::AreEqual (1, a.tickCount);
        Assert::AreEqual (1, b.tickCount);
    }


    TEST_METHOD (Adopt_AddsNonOwnedChildToWalks)
    {
        DxuiPanel             panel;
        MockDxuiControl       caller;
        MockDxuiPainter       painter;
        MockDxuiTextRenderer  text;
        MockDxuiTheme         theme;


        panel.Adopt (caller);

        Assert::AreEqual ((size_t) 1, panel.GetChildCount());
        Assert::AreEqual (static_cast<void *> (&caller), static_cast<void *> (panel.GetChild (0)));
        Assert::AreEqual (static_cast<void *> (&panel),  static_cast<void *> (caller.GetParent()));

        panel.Paint (painter, text, theme);
        Assert::AreEqual (1, caller.paintCount);
    }


    TEST_METHOD (Adopt_DestructionLeavesAdoptedChildAlive)
    {
        MockDxuiControl  caller;


        {
            DxuiPanel  panel;
            panel.Adopt (caller);
        }

        // If the panel had taken ownership the dtor would have freed
        // caller; we still have a live local, so its destructor must
        // run after this scope on its own terms. (Test simply asserts
        // the panel destructor compiled and ran without UAF.)
        caller.SetEnabled (true);
        Assert::IsTrue (caller.IsEnabled());
    }


    TEST_METHOD (Adopt_DuplicateIsNoOp)
    {
        DxuiPanel        panel;
        MockDxuiControl  caller;


        panel.Adopt (caller);
        panel.Adopt (caller);

        Assert::AreEqual ((size_t) 1, panel.GetChildCount());
    }


    TEST_METHOD (RemoveAdopted_DropsRegistrationLeavesPointerLive)
    {
        DxuiPanel        panel;
        MockDxuiControl  caller;
        HRESULT          hr     = S_OK;


        panel.Adopt (caller);

        hr = panel.RemoveAdopted (caller);

        Assert::IsTrue   (SUCCEEDED (hr));
        Assert::AreEqual ((size_t) 0, panel.GetChildCount());
        Assert::IsNull   (caller.GetParent());
    }


    TEST_METHOD (RemoveAdopted_UnknownFails)
    {
        DxuiPanel        panel;
        MockDxuiControl  stranger;


        HRESULT  hr = panel.RemoveAdopted (stranger);

        Assert::IsTrue (FAILED (hr));
    }


    TEST_METHOD (RemoveAdopted_RefusesToDropOwnedChild)
    {
        DxuiPanel          panel;
        MockDxuiControl &  owned = panel.Add<MockDxuiControl>();


        HRESULT  hr = panel.RemoveAdopted (owned);

        Assert::IsTrue   (FAILED (hr));
        Assert::AreEqual ((size_t) 1, panel.GetChildCount());
    }


    TEST_METHOD (ClearAdopted_LeavesOwnedChildrenAlone)
    {
        DxuiPanel          panel;
        MockDxuiControl    adoptedA;
        MockDxuiControl    adoptedB;
        MockDxuiControl &  owned = panel.Add<MockDxuiControl>();


        panel.Adopt (adoptedA);
        panel.Adopt (adoptedB);
        Assert::AreEqual ((size_t) 3, panel.GetChildCount());

        panel.ClearAdopted();

        Assert::AreEqual ((size_t) 1, panel.GetChildCount());
        Assert::AreEqual (static_cast<void *> (&owned), static_cast<void *> (panel.GetChild (0)));
        Assert::IsNull   (adoptedA.GetParent());
        Assert::IsNull   (adoptedB.GetParent());
    }


    TEST_METHOD (AdoptAndAdd_CoexistInInsertionOrder)
    {
        DxuiPanel          panel;
        MockDxuiControl    adoptedFirst;
        MockDxuiControl    adoptedLast;


        panel.Adopt (adoptedFirst);
        MockDxuiControl &  ownedMiddle = panel.Add<MockDxuiControl>();
        panel.Adopt (adoptedLast);

        Assert::AreEqual ((size_t) 3, panel.GetChildCount());
        Assert::AreEqual (static_cast<void *> (&adoptedFirst), static_cast<void *> (panel.GetChild (0)));
        Assert::AreEqual (static_cast<void *> (&ownedMiddle),  static_cast<void *> (panel.GetChild (1)));
        Assert::AreEqual (static_cast<void *> (&adoptedLast),  static_cast<void *> (panel.GetChild (2)));
    }


    TEST_METHOD (Adopt_MouseDispatchesFrontToBackAcrossOwnedAndAdopted)
    {
        DxuiPanel          panel;
        MockDxuiControl    adoptedFront;
        DxuiMouseEvent     ev;


        panel.Add<MockDxuiControl>();              // owned, rear
        panel.Adopt (adoptedFront);                // adopted, front (last-inserted)
        adoptedFront.consumeMouse = true;

        Assert::IsTrue   (panel.OnMouse (ev));
        Assert::AreEqual (1, adoptedFront.mouseCount);
        Assert::AreEqual (static_cast<void *> (&adoptedFront), static_cast<void *> (panel.GetChild (1)));
    }


    TEST_METHOD (StackLayout_AssignsBoundsToLeafWidgetChild)
    {
        DxuiPanel       panel;
        DxuiDpiScaler   scaler;
        MockDxuiControl a;
        MockDxuiControl b;
        RECT            bounds = MakeRect (0, 0, 200, 50);


        scaler.SetDpi (96);
        a.SetBounds (MakeRect (0, 0, 60, 50));
        b.SetBounds (MakeRect (0, 0, 80, 50));
        panel.Adopt (a);
        panel.Adopt (b);
        panel.SetLayout (std::make_unique<DxuiStackLayout> (DxuiStackLayout::Orientation::Horizontal,
                                                            0.0f,
                                                            DxuiStackLayout::Align::Stretch));

        panel.Layout (bounds, scaler);

        // Layout-policy positioning must be visible via the leaf
        // widget's IDxuiControl::GetBounds() — i.e., the policy's
        // SetBounds calls actually take effect on the widget. This
        // is the regression check for the rect-duality fix.
        Assert::AreEqual ((LONG) 0,   a.GetBounds().left);
        Assert::AreEqual ((LONG) 60,  a.GetBounds().right);
        Assert::AreEqual ((LONG) 60,  b.GetBounds().left);
        Assert::AreEqual ((LONG) 140, b.GetBounds().right);
    }


    //  The clips a painter is given, how many times the child had painted
    //  when each was pushed and popped, and how many times it had run its
    //  after pass when each was popped.
    class ClipRecordingPainter : public MockDxuiPainter
    {
    public:
        explicit ClipRecordingPainter (const MockDxuiControl & child) : m_child (child) {}

        void  PushClip (float xPx, float yPx, float widthPx, float heightPx) override
        {
            MockDxuiPainter::PushClip (xPx, yPx, widthPx, heightPx);
            pushed.push_back (RECT { (LONG) xPx, (LONG) yPx, (LONG) (xPx + widthPx), (LONG) (yPx + heightPx) });
            paintsAtPush = m_child.paintCount;
        }

        void  PopClip() override
        {
            MockDxuiPainter::PopClip();
            popped++;
            paintsAtPop = m_child.paintCount;
            aftersAtPop = m_child.afterCount;
        }

        std::vector<RECT>  pushed;
        int                popped       = 0;
        int                paintsAtPush = -1;
        int                paintsAtPop  = -1;
        int                aftersAtPop  = -1;

    private:
        const MockDxuiControl  & m_child;
    };


    TEST_METHOD (ChildClip_WrapsTheChildsPaintOnly)
    {
        DxuiPanel              panel;
        MockDxuiControl      & clipped   = panel.Add<MockDxuiControl>();
        MockDxuiControl      & unclipped = panel.Add<MockDxuiControl>();
        ClipRecordingPainter   painter (clipped);
        MockDxuiTextRenderer   text;
        MockDxuiTheme          theme;
        RECT                   clip      = MakeRect (10, 20, 110, 70);



        panel.SetChildClip (&clipped, clip);
        panel.Paint (painter, text, theme);

        Assert::AreEqual ((size_t) 1, painter.pushed.size(),          L"one clip, for the one clipped child");
        Assert::IsTrue   (EqualRect (&clip, &painter.pushed[0]) != FALSE);
        Assert::AreEqual (1, painter.popped);
        Assert::AreEqual (0, painter.paintsAtPush,                    L"pushed before the child paints");
        Assert::AreEqual (1, painter.paintsAtPop,                     L"popped after it");
        Assert::AreEqual (1, unclipped.paintCount);

        panel.ClearChildClip (&clipped);
        painter.pushed.clear();
        panel.Paint (painter, text, theme);

        Assert::AreEqual ((size_t) 0, painter.pushed.size(),          L"cleared");
    }


    TEST_METHOD (NestedPushClipsIntersect)
    {
        MockDxuiPainter  painter;
        RECT             inner   = MakeRect (50, 20, 100, 100);
        RECT             outer   = MakeRect (0, 0, 100, 100);



        painter.PushClip (0.0f, 0.0f, 100.0f, 100.0f);
        painter.PushClip (50.0f, 20.0f, 100.0f, 100.0f);
        painter.FillRect (0.0f, 0.0f, 10.0f, 10.0f, MockDxuiTheme::s_kForeground);

        painter.PopClip();
        painter.FillRect (0.0f, 0.0f, 10.0f, 10.0f, MockDxuiTheme::s_kForeground);

        painter.PopClip();
        painter.FillRect (0.0f, 0.0f, 10.0f, 10.0f, MockDxuiTheme::s_kForeground);

        Assert::AreEqual ((size_t) 3, painter.Calls().size());
        Assert::IsTrue   (painter.Calls()[0].isClipped);
        Assert::IsTrue   (EqualRect (&inner, &painter.Calls()[0].clip) != FALSE, L"inside both clips");
        Assert::IsTrue   (painter.Calls()[1].isClipped);
        Assert::IsTrue   (EqualRect (&outer, &painter.Calls()[1].clip) != FALSE, L"the outer clip once the inner is popped");
        Assert::IsFalse  (painter.Calls()[2].isClipped,                          L"no clip once both are popped");
    }


    TEST_METHOD (PaintAfterSiblings_RunsAfterEverySibling)
    {
        DxuiPanel               panel;
        MockDxuiControl       & a        = panel.Add<MockDxuiControl>();
        MockDxuiControl       & b        = panel.Add<MockDxuiControl>();
        MockDxuiPainter         painter;
        MockDxuiTextRenderer    text;
        MockDxuiTheme           theme;
        PaintLog                log;
        PaintLog                expected = { { &a, false }, { &b, false }, { &a, true }, { &b, true } };



        a.paintLog = &log;
        b.paintLog = &log;
        panel.Paint (painter, text, theme);

        Assert::AreEqual ((size_t) 4, log.size());
        Assert::IsTrue   (log == expected, L"every child paints before any child's after pass");
    }


    TEST_METHOD (PaintAfterSiblings_SkipsHiddenAndTopLayerChildren)
    {
        DxuiPanel               panel;
        MockDxuiControl       & shown    = panel.Add<MockDxuiControl>();
        MockDxuiControl       & hidden   = panel.Add<MockDxuiControl>();
        MockDxuiControl       & top      = panel.Add<MockDxuiControl>();
        MockDxuiControl       & raised   = panel.Add<MockDxuiControl>();
        MockDxuiPainter         painter;
        MockDxuiTextRenderer    text;
        MockDxuiTheme           theme;
        PaintLog                log;
        PaintLog                expected = { { &top, false }, { &top, true }, { &raised, false }, { &raised, true } };



        hidden.SetVisible     (false);
        panel.SetTopLayer     ({ &top });
        panel.SetRaisedChild  (&raised);
        panel.Paint           (painter, text, theme);

        Assert::AreEqual (1, shown.afterCount,  L"a child the page paints");
        Assert::AreEqual (0, hidden.afterCount, L"not a hidden child");
        Assert::AreEqual (0, top.afterCount,    L"not a top-layer child, which the page passes over");
        Assert::AreEqual (0, raised.afterCount, L"nor the raised child");

        top.paintLog    = &log;
        raised.paintLog = &log;
        panel.PaintTopLayer (painter, text, theme);

        Assert::IsTrue   (log == expected,      L"the top layer runs each one's after pass, after its Paint");
        Assert::AreEqual (1, shown.afterCount,  L"and not the page's children again");
    }


    TEST_METHOD (PaintAfterSiblings_IsOutsideTheChildClip)
    {
        DxuiPanel               panel;
        MockDxuiControl       & clipped  = panel.Add<MockDxuiControl>();
        ClipRecordingPainter    painter (clipped);
        MockDxuiTextRenderer    text;
        MockDxuiTheme           theme;



        panel.SetChildClip (&clipped, MakeRect (10, 20, 110, 70));
        panel.Paint        (painter, text, theme);

        Assert::AreEqual ((size_t) 1, painter.pushed.size(), L"one clip, around the child's Paint only");
        Assert::AreEqual (1, painter.paintsAtPop,            L"popped after the child's Paint");
        Assert::AreEqual (0, painter.aftersAtPop,            L"and before its after pass");
        Assert::AreEqual (1, clipped.afterCount);
    }
};

