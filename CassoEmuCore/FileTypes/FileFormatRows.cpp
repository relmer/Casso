#include "Pch.h"

#include "FileTypes/FileFormatRows.h"
#include "FileTypes/AppleWorks.h"
#include "FileTypes/HiResFormats.h"
#include "FileTypes/IigsFormats.h"
#include "FileTypes/MerlinSource.h"
#include "FileTypes/OmfFile.h"
#include "FileTypes/PascalText.h"
#include "FileTypes/RandomText.h"
#include "FileTypes/ScAssemblerSource.h"
#include "FileTypes/SuperHiRes.h"
#include "FileTypes/TeachDocument.h"
#include "FileTypes/VisiCalcSheet.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FileFormatRows::Make
//
////////////////////////////////////////////////////////////////////////////////

std::vector<FileFormatTable::Row> FileFormatRows::Make()
{
    using Row = FileFormatTable::Row;



    return
    {
        Row { FileFormat::Merlin,         &MerlinSource::Matches,      &MerlinSource::Decode,      &MerlinSource::Import, L".Merlin.txt",           false },
        Row { FileFormat::PascalText,     &PascalText::Matches,        &PascalText::Decode,        &PascalText::Import,   L".Pascal text.txt",      false },
        Row { FileFormat::RandomText,     &RandomText::Matches,        &RandomText::Decode,        nullptr,               L".Random text.csv",      false },
        Row { FileFormat::ScAssembler,    &ScAssemblerSource::Matches, &ScAssemblerSource::Decode, nullptr,               L".S-C Assembler.txt",    false },
        Row { FileFormat::AppleWorksWp,   &AppleWorksWp::Matches,      &AppleWorksWp::Decode,      nullptr,               L".AppleWorks WP.rtf",    false },
        Row { FileFormat::AppleWorksDb,   &AppleWorksDb::Matches,      &AppleWorksDb::Decode,      nullptr,               L".AppleWorks DB.csv",    false },
        Row { FileFormat::AppleWorksSs,   &AppleWorksSs::Matches,      &AppleWorksSs::Decode,      nullptr,               L".AppleWorks SS.csv",    false },
        Row { FileFormat::AppleWorksGsWp, &AppleWorksGsWp::Matches,    &AppleWorksGsWp::Decode,    nullptr,               L".AppleWorks GS WP.rtf", true },
        Row { FileFormat::Teach,          &TeachDocument::Matches,     &TeachDocument::Decode,     nullptr,               L".Teach.rtf",            true },
        Row { FileFormat::Omf,            &OmfFile::Matches,           &OmfFile::Decode,           nullptr,               L".OMF.txt",              true },
        Row { FileFormat::VisiCalc,       &VisiCalcSheet::Matches,     &VisiCalcSheet::Decode,     nullptr,               L".VisiCalc.csv",         false },

        Row { FileFormat::SuperHiRes,         &SuperHiRes::MatchesScreen,          &SuperHiRes::DecodeScreen,          nullptr, L".Super Hi-Res.png", true },
        Row { FileFormat::PackedSuperHiRes,   &SuperHiRes::MatchesPacked,          &SuperHiRes::DecodePacked,          nullptr, L".Super Hi-Res.png", true },
        Row { FileFormat::Paintworks,         &SuperHiRes::MatchesPaintworks,      &SuperHiRes::DecodePaintworks,      nullptr, L".Super Hi-Res.png", true },
        Row { FileFormat::SuperHiRes3200,     &SuperHiRes3200::MatchesBrooks,      &SuperHiRes3200::DecodeBrooks,      nullptr, L".3200 Color.png",   true },
        Row { FileFormat::DreamGrafix,        &SuperHiRes3200::MatchesDreamGrafix, &SuperHiRes3200::DecodeDreamGrafix, nullptr, L".3200 Color.png",   true },
        Row { FileFormat::SuperHiRes3201,     &SuperHiRes3200::Matches3201,        &SuperHiRes3200::Decode3201,        nullptr, L".3200 Color.png",   true },
        Row { FileFormat::Lz4fh,              &Lz4fh::Matches,                     &Lz4fh::Decode,                     nullptr, L".LZ4FH.png",        false },
        Row { FileFormat::PrintShopGsClipArt, &PrintShopGraphic::MatchesGs,        &PrintShopGraphic::Decode,          nullptr, L".Print Shop.png",   true },
        Row { FileFormat::PrintShopGraphic,   &PrintShopGraphic::Matches,          &PrintShopGraphic::Decode,          nullptr, L".Print Shop.png",   false },
        Row { FileFormat::PrintShopFont,      &PrintShopFont::Matches,             &PrintShopFont::Decode,             nullptr, L".Font.png",         false },
        Row { FileFormat::HiResFont,          &HiResFont::MatchesApple2,           &HiResFont::Decode,                 nullptr, L".Font.png",         false },
        Row { FileFormat::Apple3Font,         &HiResFont::MatchesApple3,           &HiResFont::Decode,                 nullptr, L".Font.png",         false },
        Row { FileFormat::ShapeTable,         &ShapeTable::Matches,                &ShapeTable::Decode,                nullptr, L".Shape table.png",  false },
        Row { FileFormat::IigsFont,           &IigsFont::Matches,                  &IigsFont::Decode,                  nullptr, L".Font.png",         true },
        Row { FileFormat::IigsIcons,          &IigsIcons::Matches,                 &IigsIcons::Decode,                 nullptr, L".IIgs Icons.png",   true },
        Row { FileFormat::IigsAnimation,      &IigsAnimation::Matches,             &IigsAnimation::Decode,             nullptr, L".Animation.png",    true },
    };
}
