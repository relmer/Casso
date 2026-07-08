#pragma once

#include "Pch.h"




////////////////////////////////////////////////////////////////////////////////
//
//  InkPrimary
//
//  The four ImageWriter II ribbon primaries as a 4-bit field. A raster cell
//  stores the OR of every primary struck at that position; the composites
//  (orange, green, purple) and black-dominance are derived at render time
//  from the bitfield, exactly as overprinting produced them on paper -- they
//  are never stored.
//
////////////////////////////////////////////////////////////////////////////////

enum class InkPrimary : Byte
{
    None   = 0x0,
    Black  = 0x1,
    Yellow = 0x2,
    Red    = 0x4,
    Blue   = 0x8,
};




////////////////////////////////////////////////////////////////////////////////
//
//  DotStyle
//
//  Delivery-renderer dot shape. Ink stamps round overlapping pin impressions
//  with overprint mixing and weave texture; Plain stamps a cell-sized square.
//  User-selectable on the Printing settings tab.
//
////////////////////////////////////////////////////////////////////////////////

enum class DotStyle
{
    Ink,
    Plain,
};




////////////////////////////////////////////////////////////////////////////////
//
//  PrinterGrid
//
//  Native raster geometry -- the maximum documented ImageWriter II dot
//  placement. Every addressable dot maps to exactly one cell. The delivery
//  renderer resamples this non-square grid to true square-pixel geometry.
//
////////////////////////////////////////////////////////////////////////////////

struct PrinterGrid
{
    static constexpr int   kDotsPerInchH = 160;                            // horizontal density
    static constexpr int   kRowsPerInch  = 144;                           // vertical density
    static constexpr int   kDotsPerRow   = 1280;                          // 8" printable * 160 dpi
    static constexpr int   kPageRows     = 1584;                          // 11" page * 144 rows/inch
    static constexpr int   kMaxFormPages = 60;                            // strip cap (~55 feet)
    static constexpr int   kMaxStripRows = kPageRows * kMaxFormPages;     // hard raster ceiling
};




////////////////////////////////////////////////////////////////////////////////
//
//  PrinterEventType
//
//  Ordered presentation events emitted by the interpreter alongside its
//  raster strikes. The presenter replays them at real ImageWriter speed to
//  drive the panel paper animation and the audio voices.
//
////////////////////////////////////////////////////////////////////////////////

enum class PrinterEventType
{
    HeadBurst,
    LineFeed,
    FormFeed,
    ColorChange,
    ResetSeen,
    UnknownCommand,
};




////////////////////////////////////////////////////////////////////////////////
//
//  PrinterEvent
//
//  Flat POD carried from the interpreter to the presenter. Only the fields
//  relevant to `type` are meaningful; the rest stay at their defaults.
//
////////////////////////////////////////////////////////////////////////////////

struct PrinterEvent
{
    PrinterEventType   type     = PrinterEventType::ResetSeen;
    int                fromDot  = 0;                  // HeadBurst: first struck column
    int                toDot    = 0;                  // HeadBurst: last struck column
    int                row      = 0;                  // HeadBurst: strip-relative row
    int                rows     = 0;                  // LineFeed: rows advanced
    InkPrimary         color    = InkPrimary::None;   // ColorChange: newly selected primary
    Byte               leadByte = 0;                  // UnknownCommand: leading command byte
};
