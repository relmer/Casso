#include "Pch.h"

#include "Devices/Disk/Inspector/FieldLocator.h"
#include "InspectorTrackBuilder.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  FieldLocatorTests
//
//  Address and data fields in both formats, how a data field pairs with the
//  address field before it, and fields that span the index.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (FieldLocatorTests)
{
public:

    using SectorBytes = std::array<Byte, DiskFieldFormat::kSectorBytes>;



    static vector<LocatedField> Locate (const InspectorTrackBuilder & builder)
    {
        FramedTrack           track;
        vector<LocatedField>  fields;



        LatchFramer::Frame (*builder.MakeBitCopy(), track);
        FieldLocator::Locate (track, FieldMarks::MakeStandard(), fields);

        return fields;
    }



    static int CountRole (const vector<LocatedField> & fields, FieldRole role)
    {
        return static_cast<int> (std::count_if (fields.begin(), fields.end(), [role] (const LocatedField & f) { return f.role == role; }));
    }



    static void AssertStandardTrack (DiskFieldKind kind, int sectors)
    {
        InspectorTrackBuilder  builder;
        vector<LocatedField>   fields;
        SectorBytes            expected = {};
        int                    sector   = 0;



        builder.AppendStandardTrack (kind, 254, 17, InspectorTrackBuilder::FillPattern);
        fields = Locate (builder);

        Assert::AreEqual (sectors, CountRole (fields, FieldRole::Address));
        Assert::AreEqual (sectors, CountRole (fields, FieldRole::Data));

        for (const LocatedField & f : fields)
        {
            if (f.role != FieldRole::Address)
            {
                continue;
            }

            Assert::AreEqual (static_cast<int> (kind), static_cast<int> (f.kind));
            Assert::AreEqual (sector, static_cast<int> (f.sector));
            Assert::AreEqual (254, static_cast<int> (f.volume));
            Assert::AreEqual (17, static_cast<int> (f.track));
            Assert::IsTrue (f.isAddressChecksumGood);
            Assert::IsTrue (f.isEpilogueStandard && f.hasEpilogueTail);
            Assert::IsTrue (f.pairedField >= 0);

            InspectorTrackBuilder::FillPattern (sector, expected);

            Assert::IsTrue (fields[f.pairedField].data.isChecksumGood);
            Assert::IsTrue (fields[f.pairedField].data.bytes == expected);
            Assert::IsFalse (fields[f.pairedField].hasNoise);

            sector++;
        }
    }



    TEST_METHOD (SixteenSectorFieldsDecode)
    {
        AssertStandardTrack (DiskFieldKind::Sixteen, 16);
    }



    TEST_METHOD (ThirteenSectorFieldsDecode)
    {
        AssertStandardTrack (DiskFieldKind::Thirteen, 13);
    }



    TEST_METHOD (BothKindsOnOneTrackDecode)
    {
        InspectorTrackBuilder  builder;
        vector<LocatedField>   fields;
        SectorBytes            bytes = {};



        InspectorTrackBuilder::FillPattern (1, bytes);

        builder.AppendSync         (40);
        builder.AppendAddressField (DiskFieldKind::Thirteen, 254, 0, 1);
        builder.AppendSync         (6);
        builder.AppendDataField    (DiskFieldKind::Thirteen, bytes);
        builder.AppendSync         (20);
        builder.AppendAddressField (DiskFieldKind::Sixteen, 254, 0, 1);
        builder.AppendSync         (6);
        builder.AppendDataField    (DiskFieldKind::Sixteen, bytes);
        builder.AppendSync         (40);

        fields = Locate (builder);

        Assert::AreEqual (4, static_cast<int> (fields.size()));
        Assert::AreEqual (static_cast<int> (DiskFieldKind::Thirteen), static_cast<int> (fields[1].kind));
        Assert::AreEqual (static_cast<int> (DiskFieldKind::Sixteen),  static_cast<int> (fields[3].kind));
        Assert::IsTrue (fields[1].data.bytes == bytes && fields[3].data.bytes == bytes);
    }



    static vector<LocatedField> MakeGap (int syncAfterAddress)
    {
        InspectorTrackBuilder  builder;
        SectorBytes            bytes = {};



        builder.AppendSync         (40);
        builder.AppendAddressField (DiskFieldKind::Sixteen, 254, 0, 0);
        builder.AppendSync         (syncAfterAddress);
        builder.AppendDataField    (DiskFieldKind::Sixteen, bytes);
        builder.AppendSync         (40);

        return Locate (builder);
    }



    TEST_METHOD (DataJustInsideTheWindowPairs)
    {
        vector<LocatedField>  fields = MakeGap (FieldLocator::kDataSearchWindowNibbles - 1);



        Assert::AreEqual (1, fields[0].pairedField);
    }



    TEST_METHOD (DataJustOutsideTheWindowDoesNotPair)
    {
        vector<LocatedField>  fields = MakeGap (FieldLocator::kDataSearchWindowNibbles);



        Assert::AreEqual (-1, fields[0].pairedField);
        Assert::AreEqual (-1, fields[1].pairedField);
    }



    TEST_METHOD (TheSearchEndsAtTheNextAddressField)
    {
        InspectorTrackBuilder  builder;
        vector<LocatedField>   fields;
        SectorBytes            bytes = {};



        builder.AppendSync         (40);
        builder.AppendAddressField (DiskFieldKind::Sixteen, 254, 0, 3);
        builder.AppendSync         (4);
        builder.AppendAddressField (DiskFieldKind::Sixteen, 254, 0, 4);
        builder.AppendSync         (6);
        builder.AppendDataField    (DiskFieldKind::Sixteen, bytes);
        builder.AppendSync         (40);

        fields = Locate (builder);

        Assert::AreEqual (-1, fields[0].pairedField, L"sector 3 has no data field");
        Assert::AreEqual (2,  fields[1].pairedField, L"the data field is sector 4's");
    }



    TEST_METHOD (AFailedAddressChecksumStillPairs)
    {
        InspectorTrackBuilder  builder;
        vector<LocatedField>   fields;
        SectorBytes            bytes = {};



        builder.AppendSync         (40);
        builder.AppendAddressField (DiskFieldKind::Sixteen, 254, 0, 7, true);
        builder.AppendSync         (6);
        builder.AppendDataField    (DiskFieldKind::Sixteen, bytes, true);
        builder.AppendSync         (40);

        fields = Locate (builder);

        Assert::IsFalse (fields[0].isAddressChecksumGood);
        Assert::AreEqual (1, fields[0].pairedField);
        Assert::IsFalse (fields[1].data.isChecksumGood);
        Assert::IsTrue (fields[1].data.bytes == bytes, L"a bad checksum nibble leaves the bytes intact");
    }



    TEST_METHOD (AFieldSpanningTheIndexIsWhole)
    {
        InspectorTrackBuilder  builder;
        InspectorTrackBuilder  wrapped;
        vector<LocatedField>   fields;
        SectorBytes            bytes = {};
        vector<Byte>           cells;



        InspectorTrackBuilder::FillPattern (9, bytes);

        builder.AppendSync         (40);
        builder.AppendAddressField (DiskFieldKind::Sixteen, 254, 0, 9);
        builder.AppendSync         (6);
        builder.AppendDataField    (DiskFieldKind::Sixteen, bytes);
        builder.AppendSync         (40);

        //  Move the first 1,500 cells to the end: the data field's body now
        //  crosses the index.
        cells = builder.GetCells();
        std::rotate (cells.begin(), cells.begin() + 1500, cells.end());
        wrapped.AppendRawCells (cells);

        fields = Locate (wrapped);

        Assert::AreEqual (2, static_cast<int> (fields.size()));

        for (const LocatedField & f : fields)
        {
            if (f.role == FieldRole::Data)
            {
                Assert::IsTrue (f.data.isChecksumGood);
                Assert::IsTrue (f.data.bytes == bytes);
                Assert::IsTrue (f.pairedField >= 0);
                Assert::IsTrue (f.endCell < f.startCell, L"the field ends after the index");
            }
        }
    }


    TEST_METHOD (AnOrphanDataFieldTakesTheEncodingItsNibblesFit)
    {
        InspectorTrackBuilder  builder;
        vector<LocatedField>   fields;
        SectorBytes            bytes = {};



        InspectorTrackBuilder::FillPattern (2, bytes);

        builder.AppendSync      (40);
        builder.AppendDataField (DiskFieldKind::Thirteen, bytes);
        builder.AppendSync      (40);

        fields = Locate (builder);

        Assert::AreEqual (1, static_cast<int> (fields.size()));
        Assert::AreEqual (static_cast<int> (DiskFieldKind::Thirteen), static_cast<int> (fields[0].kind));
        Assert::AreEqual (-1, fields[0].pairedField);
        Assert::IsTrue (fields[0].data.bytes == bytes);
    }
};
