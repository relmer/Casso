#pragma once

#include "Pch.h"

#include "Debugger/DiagnosticsSnapshot.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SoundBoardTips
//
//  The tip of each part of the Mockingboard view: what the part is, the
//  register behind it, and how the value becomes what is drawn. A tip is one
//  paragraph. It doesn't begin with the part's label or repeat a value drawn
//  beside it, and where a value means more than itself, the tip has what it
//  means. Pure text, so each is checked without a window.
//
////////////////////////////////////////////////////////////////////////////////

class SoundBoardTips
{
public:
    //  A channel's parts, by its index from 0 for A.
    static std::wstring  GetNote   (int channel, const DiagnosticsSoundChannel & state);
    static std::wstring  GetSource (int channel, bool isNoise, const DiagnosticsSoundChannel & state, Byte mixer);
    static std::wstring  GetLevel  (int channel, const DiagnosticsSoundChannel & state, Byte amplitude);
    static std::wstring  GetScope  (int channel);

    //  The envelope's parts; a bit by its index from 0 for CONT.
    static std::wstring  GetEnvelopePicture (const DiagnosticsSoundPsg & psg);
    static std::wstring  GetRamp            (const DiagnosticsSoundPsg & psg);
    static std::wstring  GetShape           (const DiagnosticsSoundPsg & psg);
    static std::wstring  GetNoise           (const DiagnosticsSoundPsg & psg);
    static std::wstring  GetShapeBit        (int bit, Byte shape);

    //  An AY register, R0 to R15, with its value.
    static std::wstring  GetRegister (int reg, Byte value);

    //  A 6522 field or lamp, by the label it is drawn with.
    static std::wstring  GetVia (const std::wstring & label, const DiagnosticsSoundHalf & half);

    //  The speech chip's parts, by the label each is drawn with, and one
    //  phoneme of the recent ones.
    static std::wstring  GetVoice   (const std::wstring & label, const DiagnosticsSoundVoice & voice);
    static std::wstring  GetPhoneme (const DiagnosticsSoundVoice::Phoneme & phoneme);

    static constexpr int  kRegisterCount = 16;

private:
    static wchar_t          GetChannelName     (int channel);
    static std::wstring     GetHex             (int value, int digits);
    static const wchar_t *  GetPattern         (Byte shape);
    static const wchar_t *  GetRegisterPurpose (int reg);
    static std::wstring     GetSentence        (const wchar_t * text);
    static std::wstring     GetViaBits         (Byte value);
    static std::wstring     GetMode            (const std::string & mode);
};
