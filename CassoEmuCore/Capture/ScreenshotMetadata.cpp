#include "Pch.h"

#include "Capture/ScreenshotMetadata.h"




// PNG-registered keywords, carrying their registered meanings.
//
// NOT "Source". The specification defines it as the DEVICE used to create the
// image -- a scanner or a camera -- and nothing captured this: Casso
// synthesized it, and the device that wrote the file is the host PC. The
// emulated machine goes in a Casso keyword below, where it claims nothing the
// format did not intend.
static constexpr char  s_kKeySoftware[]     = "Software";
static constexpr char  s_kKeyCreationTime[] = "Creation Time";

// Casso's own, prefixed so they cannot collide with a keyword the format
// registers later.
//
// SENTENCE CASE, with CRT kept upper because it is an initialism. The
// registered keywords above are title case because the specification spells
// them that way, not because Casso chose it.
//
// ONE VALUE PER ENTRY, deliberately. The contract promises that entries may
// be added but never renamed or repurposed, and that promise operates on
// KEYWORDS -- a composite value's internal grammar sits outside it, so adding
// a CRT parameter later would silently change the shape of something a reader
// had learned to parse. A parameter per keyword puts every one of them under
// the guarantee that is actually written down.
static constexpr char  s_kKeyCapture[]      = "Casso capture";
static constexpr char  s_kKeyMachine[]      = "Casso machine";
static constexpr char  s_kKeyMonitor[]      = "Casso monitor";

static constexpr char  s_kKeySceneYaw[]     = "Casso scene yaw";
static constexpr char  s_kKeyScenePitch[]   = "Casso scene pitch";
static constexpr char  s_kKeySceneZoom[]    = "Casso scene zoom";
static constexpr char  s_kKeyScenePanX[]    = "Casso scene pan X";
static constexpr char  s_kKeyScenePanY[]    = "Casso scene pan Y";

static constexpr char  s_kKeyCrtBrightness[]    = "Casso CRT brightness";
static constexpr char  s_kKeyCrtContrast[]      = "Casso CRT contrast";
static constexpr char  s_kKeyCrtGamma[]         = "Casso CRT gamma";
static constexpr char  s_kKeyCrtScanlines[]     = "Casso CRT scanlines";
static constexpr char  s_kKeyCrtBloomStrength[] = "Casso CRT bloom strength";
static constexpr char  s_kKeyCrtBloomRadius[]   = "Casso CRT bloom radius";
static constexpr char  s_kKeyCrtBleed[]         = "Casso CRT bleed";
static constexpr char  s_kKeyCrtPersistence[]   = "Casso CRT persistence";

// RFC 1123 spells both in English regardless of the machine's locale, so
// these are tables rather than anything locale-aware.
static constexpr const char *  s_kpszDays[]   = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
static constexpr const char *  s_kpszMonths[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                                  "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

static constexpr int   s_kMinutesPerHour = 60;





////////////////////////////////////////////////////////////////////////////////
//
//  FormatFloat
//
//  Fixed decimals, so a column of CRT values lines up and two captures of the
//  same settings produce byte-identical text. std::format's default would
//  print 1 for 1.0f and 0.5 for 0.5f, which reads as noise in a list.
//
////////////////////////////////////////////////////////////////////////////////

string ScreenshotMetadata::FormatFloat (float value, int decimals)
{
    const char *   spec = "{:.2f}";



    if (decimals == 1)
    {
        spec = "{:.1f}";
    }
    else if (decimals == 3)
    {
        spec = "{:.3f}";
    }

    return std::vformat (spec, std::make_format_args (value));
}





////////////////////////////////////////////////////////////////////////////////
//
//  FormatCreationTime
//
//  RFC 1123, which is what the PNG specification names for Creation Time.
//
//  The offset is signed and expressed as +HHMM: a negative offset west of
//  Greenwich must keep its sign on the HOUR while the minutes stay positive,
//  which is the part that goes wrong when this is written with a bare
//  division.
//
////////////////////////////////////////////////////////////////////////////////

string ScreenshotMetadata::FormatCreationTime (const SYSTEMTIME & when, int utcOffsetMinutes)
{
    const char *  day     = "Sun";
    const char *  month   = "Jan";
    int           total   = utcOffsetMinutes;
    char          sign    = '+';
    int           hours   = 0;
    int           minutes = 0;



    if (when.wDayOfWeek < std::size (s_kpszDays))
    {
        day = s_kpszDays[when.wDayOfWeek];
    }

    if (when.wMonth >= 1 && when.wMonth <= std::size (s_kpszMonths))
    {
        month = s_kpszMonths[when.wMonth - 1];
    }

    if (total < 0)
    {
        sign  = '-';
        total = -total;
    }

    hours   = total / s_kMinutesPerHour;
    minutes = total % s_kMinutesPerHour;

    return std::format ("{}, {:02} {} {:04} {:02}:{:02}:{:02} {}{:02}{:02}",
                        day, (int) when.wDay, month, (int) when.wYear,
                        (int) when.wHour, (int) when.wMinute, (int) when.wSecond,
                        sign, hours, minutes);
}





////////////////////////////////////////////////////////////////////////////////
//
//  RadiansToDegrees
//
//  The scene holds radians; a person restoring a view thinks in degrees.
//
////////////////////////////////////////////////////////////////////////////////

float ScreenshotMetadata::RadiansToDegrees (float radians)
{
    return radians * 180.0f / (float) std::numbers::pi;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FormatScenePose
//
//  The pose as one line, FOR THE ON-SCREEN READOUT ONLY. The file records the
//  same view as separate numeric entries, because a reader restoring a pose
//  wants values rather than a sentence to parse.
//
//  The precisions match the entries': a tenth of a degree is the finest orbit
//  step a drag produces, and three decimals of pan is what distinguishes two
//  positions that look the same but frame differently.
//
////////////////////////////////////////////////////////////////////////////////

string ScreenshotMetadata::FormatScenePose (float yawRad, float pitchRad,
                                            float zoom, float panX, float panY)
{
    return std::format ("yaw {:.1f}  pitch {:.1f}  zoom {:.2f}  pan {:.3f} {:.3f}",
                        RadiansToDegrees (yawRad), RadiansToDegrees (pitchRad),
                        zoom, panX, panY);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Compose
//
//  The contract's entry set for this mode, in the contract's order.
//
//  Five entries are unconditional -- what wrote the file and when, then the
//  setup it came from: which mode, which machine, which monitor under which
//  color mode. The two GROUPS after them are conditional, and the conditions
//  are the whole point:
//
//    scene *   scene captures only. The other modes have no scene, and a pose
//              recorded against a picture would be a fact about nothing.
//    CRT *     the two processed modes. Raw had no CRT parameters applied, so
//              emitting them would assert something false about the image the
//              reader is holding.
//
//  A pose the scene has not composed yet is skipped whole rather than emitted
//  as five zeros, which would read as a real view pointing at the origin.
//
//  Angles are degrees to a tenth -- the finest step a drag produces -- and pan
//  to three decimals, which is what separates two positions that look alike
//  but frame differently. Fixed decimals throughout, so two captures of one
//  setup produce byte-identical text.
//
////////////////////////////////////////////////////////////////////////////////

vector<MetadataEntry> ScreenshotMetadata::Compose (const ScreenshotFacts & facts)
{
    vector<MetadataEntry>   entries;



    entries.push_back ({ s_kKeySoftware,     facts.versionString });
    entries.push_back ({ s_kKeyCreationTime, FormatCreationTime (facts.when, facts.utcOffsetMinutes) });
    entries.push_back ({ s_kKeyCapture,      ScreenshotModeToken::Format (facts.mode) });
    entries.push_back ({ s_kKeyMachine,      facts.machineDisplayName });
    entries.push_back ({ s_kKeyMonitor,      facts.monitorKey });

    if (facts.mode == ScreenshotMode::Scene && facts.hasScenePose)
    {
        entries.push_back ({ s_kKeySceneYaw,   FormatFloat (RadiansToDegrees (facts.orbitYawRad),   1) });
        entries.push_back ({ s_kKeyScenePitch, FormatFloat (RadiansToDegrees (facts.orbitPitchRad), 1) });
        entries.push_back ({ s_kKeySceneZoom,  FormatFloat (facts.zoom, 2) });
        entries.push_back ({ s_kKeyScenePanX,  FormatFloat (facts.panX, 3) });
        entries.push_back ({ s_kKeyScenePanY,  FormatFloat (facts.panY, 3) });
    }

    if (facts.mode != ScreenshotMode::Raw)
    {
        entries.push_back ({ s_kKeyCrtBrightness,    FormatFloat (facts.crt.brightness, 2) });
        entries.push_back ({ s_kKeyCrtContrast,      FormatFloat (facts.crt.contrast, 2) });
        entries.push_back ({ s_kKeyCrtGamma,         FormatFloat (facts.crt.gamma, 2) });
        entries.push_back ({ s_kKeyCrtScanlines,     FormatFloat (facts.crt.scanlineIntensity, 2) });
        entries.push_back ({ s_kKeyCrtBloomStrength, FormatFloat (facts.crt.bloomStrength, 2) });
        entries.push_back ({ s_kKeyCrtBloomRadius,   FormatFloat (facts.crt.bloomRadius, 2) });
        entries.push_back ({ s_kKeyCrtBleed,         FormatFloat (facts.crt.colorBleedWidth, 2) });
        entries.push_back ({ s_kKeyCrtPersistence,   FormatFloat (facts.crt.persistence, 2) });
    }

    return entries;
}
