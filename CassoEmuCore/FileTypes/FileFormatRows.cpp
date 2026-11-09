#include "Pch.h"

#include "FileTypes/FileFormatRows.h"
#include "FileTypes/MerlinSource.h"
#include "FileTypes/PascalText.h"
#include "FileTypes/RandomText.h"
#include "FileTypes/ScAssemblerSource.h"





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
        Row { FileFormat::Merlin,      &MerlinSource::Matches,      &MerlinSource::Decode,      &MerlinSource::Import, L".Merlin.txt",       false },
        Row { FileFormat::PascalText,  &PascalText::Matches,        &PascalText::Decode,        &PascalText::Import,   L".Pascal text.txt",  false },
        Row { FileFormat::RandomText,  &RandomText::Matches,        &RandomText::Decode,        nullptr,               L".Random text.csv",  false },
        Row { FileFormat::ScAssembler, &ScAssemblerSource::Matches, &ScAssemblerSource::Decode, nullptr,               L".S-C Assembler.txt", false },
    };
}
