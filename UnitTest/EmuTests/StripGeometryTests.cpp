#include "Pch.h"

#include "Devices/Disk/Inspector/TrackAnalyzer.h"
#include "Ui/DiskInspector/StripGeometry.h"
#include "InspectorTrackBuilder.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  StripGeometryTests
//
//  The unrolled track (FR-033, FR-036): turns map across the strip from its
//  start, wrapping past the index, and a stretch across the view's start
//  shows in two parts.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (StripGeometryTests)
{
public:

    TEST_METHOD (TurnsMapAcrossTheStripAndBack)
    {
        StripGeometry  g (0.9, 0.2, 100.0f, 400.0f);



        Assert::AreEqual (100.0f, g.GetX (0.9),  0.01f);
        Assert::AreEqual (300.0f, g.GetX (0.0),  0.01f, L"the index is halfway across");
        Assert::AreEqual (400.0f, g.GetX (0.05), 0.01f);
        Assert::AreEqual (0.05,   g.GetTurn (400.0f), 1e-9);
        Assert::AreEqual (0.95,   g.GetTurn (200.0f), 1e-9);
    }



    TEST_METHOD (AStretchAcrossTheViewsStartShowsInTwoParts)
    {
        StripGeometry                g (0.0, 1.0, 0.0f, 1000.0f);
        std::array<StripSegment, 2>  segments = {};



        Assert::AreEqual (2, g.GetSegments (0.95, 0.1, segments));
        Assert::AreEqual (950.0f,  segments[0].x0, 0.01f);
        Assert::AreEqual (1000.0f, segments[0].x1, 0.01f);
        Assert::AreEqual (0.0f,    segments[1].x0, 0.01f);
        Assert::AreEqual (50.0f,   segments[1].x1, 0.01f);

        Assert::AreEqual (1, g.GetSegments (0.2, 0.1, segments));
        Assert::AreEqual (0, StripGeometry (0.5, 0.1, 0.0f, 100.0f).GetSegments (0.2, 0.1, segments), L"out of view");
    }



    TEST_METHOD (ZoomKeepsTheTurnUnderTheAnchor)
    {
        double  start = StripGeometry::GetStartForZoom (0.25, 0.2, 0.1, 0.5);



        Assert::AreEqual (0.30, start, 1e-9, L"the middle stays at 0.35");
        Assert::AreEqual (0.35, StripGeometry (start, 0.1, 0.0f, 100.0f).GetTurn (50.0f), 1e-9);
    }



    TEST_METHOD (NibbleTurnsMatchTheAnalyzersAngles)
    {
        InspectorTrackBuilder  builder;
        TrackAnalysis          analysis;
        TrackContext           context;
        vector<double>         turns;
        size_t                 i        = 0;



        builder.AppendStandardTrack (DiskFieldKind::Sixteen, 254, 17, InspectorTrackBuilder::FillPattern);
        TrackAnalyzer::Analyze (*builder.MakeFluxCopy(), context, DecodeSettings::MakeStandard(), analysis);
        StripGeometry::BuildNibbleTurns (analysis, turns);

        Assert::AreEqual (analysis.framed.nibbles.size() + 1, turns.size());

        for (i = 0; i < analysis.framed.nibbles.size(); i += 97)
        {
            Assert::AreEqual (TrackAnalyzer::GetAngle (analysis, analysis.framed.nibbles[i].startCell), turns[i], 1e-9);
        }

        Assert::AreEqual (turns.front() + 1.0, turns.back(), 1e-12);
    }
};
