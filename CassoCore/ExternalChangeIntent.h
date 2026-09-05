#pragma once





////////////////////////////////////////////////////////////////////////////////
//
//  ExternalChangeIntent
//
//  What a change to a mounted disk image should do to the machine running it.
//
//  IN CassoCore BECAUSE BOTH LAYERS SPEAK IT. The command line parses it and
//  the emulator's disk policy acts on it, and CassoEmuCore depends on CassoCore
//  rather than the other way round -- so the shared word lives at the bottom
//  where both can reach it without either one reaching upwards.
//
//  IT IS THE ONE THING THEY SHARE. Everything else about deciding what to do
//  with a changed image stays in ExternalChangePolicy, which no command line
//  has any business knowing about.
//
//  `Unstated` IS A REAL VALUE, not a missing one. A change written by a text
//  editor, a copy, or a second emulator carries no intent, and that is the
//  ordinary case for everything except this project's own command line.
//  Holding it as a distinct value is what keeps "nobody said" from being
//  confused with "somebody said reload it".
//
////////////////////////////////////////////////////////////////////////////////

enum class ExternalChangeIntent
{
    Unstated,
    ReloadInPlace,
    Restart,
};
