#include "Pch.h"

#include "Controllers/MappingEvaluator.h"

#include "Controllers/DeadzoneShaper.h"
#include "Controllers/XInputSampleDecoder.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Evaluate
//
//  Axes first, a pair at a time, so a pair driven by one stick can be shaped
//  together, then the three buttons.
//
//  Only the first axisCount axes are evaluated; the rest are left absent,
//  which is how a mapping that binds PDL2 and PDL3 plays on a machine with two
//  axes without faulting and without moving a rate paddle nobody can read.
//
////////////////////////////////////////////////////////////////////////////////

GamePortContribution MappingEvaluator::Evaluate (
    const ControllerSample  & sample,
    const ControlMapping    & mapping,
    float                     deadzone,
    float                     elapsedSeconds,
    size_t                    axisCount)
{
    GamePortContribution  contribution;
    float                 step  = std::clamp (elapsedSeconds, 0.0f, kMaxRateStep);
    size_t                count = std::min (axisCount, GamePortContribution::kAxisCount);



    m_isRateMoving = false;

    EvaluatePair (sample, mapping.pdl0, mapping.pdl1, 0, count, deadzone, step, contribution);
    EvaluatePair (sample, mapping.pdl2, mapping.pdl3, 2, count, deadzone, step, contribution);

    contribution.buttons.set (0, IsButtonListHeld (sample, mapping.pb0));
    contribution.buttons.set (1, IsButtonListHeld (sample, mapping.pb1));
    contribution.buttons.set (2, IsButtonListHeld (sample, mapping.pb2));

    return contribution;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EvaluatePair
//
//  One pair of axis targets, PDL0/PDL1 or PDL2/PDL3, into the contribution.
//  A pair on the two axes of one stick is shaped with a round deadzone.
//
////////////////////////////////////////////////////////////////////////////////

void MappingEvaluator::EvaluatePair (
    const ControllerSample          & sample,
    const std::vector<AxisBinding>  & xBindings,
    const std::vector<AxisBinding>  & yBindings,
    size_t                            firstAxis,
    size_t                            axisCount,
    float                             deadzone,
    float                             step,
    GamePortContribution            & contribution)
{
    const AxisBinding *  winnerX = nullptr;
    const AxisBinding *  winnerY = nullptr;
    float                rawX    = 0.0f;
    float                rawY    = 0.0f;
    float                shapedX = 0.0f;
    float                shapedY = 0.0f;



    if (firstAxis >= axisCount)
    {
        return;
    }

    rawX = EvaluateAxis (sample, xBindings, winnerX);
    rawY = EvaluateAxis (sample, yBindings, winnerY);

    if (IsOneStick (xBindings, yBindings))
    {
        DeadzoneShaper::ShapeStick (rawX, rawY, deadzone, shapedX, shapedY);
    }
    else
    {
        shapedX = DeadzoneShaper::ShapeAxis (rawX, deadzone);
        shapedY = DeadzoneShaper::ShapeAxis (rawY, deadzone);
    }

    contribution.paddle[firstAxis] = ToAxisPaddle (firstAxis, shapedX, winnerX, step);

    if (firstAxis + 1 < axisCount)
    {
        contribution.paddle[firstAxis + 1] = ToAxisPaddle (firstAxis + 1, shapedY, winnerY, step);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ResetRate
//
////////////////////////////////////////////////////////////////////////////////

void MappingEvaluator::ResetRate()
{
    m_rateValue.fill (kRateCenter);
    m_isRateMoving = false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsRateMoving
//
////////////////////////////////////////////////////////////////////////////////

bool MappingEvaluator::IsRateMoving() const
{
    return m_isRateMoving;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ToAxisPaddle
//
//  An absolute binding's deflection is the paddle's position. A rate
//  binding's moves the paddle at a speed proportional to it, up to the
//  binding's maximum, and leaves the paddle where it is when the stick comes
//  back to center, the way a paddle knob stays where it is turned (FR-021a).
//
////////////////////////////////////////////////////////////////////////////////

Byte MappingEvaluator::ToAxisPaddle (size_t axis, float shaped, const AxisBinding * winner, float elapsedSeconds)
{
    constexpr float  kPaddleMax = 255.0f;
    bool             isRate     = winner != nullptr &&
                                  winner->kind     == AxisBindingKind::Analog &&
                                  winner->response == AxisResponse::Rate;



    if (!isRate)
    {
        return DeadzoneShaper::ToPaddle (shaped);
    }

    if (shaped != 0.0f)
    {
        m_isRateMoving = true;
    }

    m_rateValue[axis] = std::clamp (m_rateValue[axis] + shaped * winner->maxSpeed * elapsedSeconds, 0.0f, kPaddleMax);

    return static_cast<Byte> (m_rateValue[axis]);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadAnalog
//
//  An axis reads over its full travel; a trigger rests at 0 and is stretched
//  over the same range so it can drive an axis target.
//
////////////////////////////////////////////////////////////////////////////////

float MappingEvaluator::ReadAnalog (const ControllerSample & sample, const ControlId & control)
{
    constexpr float  kTriggerScale  = 2.0f;
    constexpr float  kTriggerOffset = 1.0f;
    float            value          = 0.0f;



    if (control.kind == ControlKind::Axis && control.index >= 0 && control.index < ControllerSample::kAxisCount)
    {
        value = sample.axes[(size_t) control.index];
    }
    else if (control.kind == ControlKind::Trigger && control.index >= 0 && control.index < ControllerSample::kTriggerCount)
    {
        value = sample.triggers[(size_t) control.index] * kTriggerScale - kTriggerOffset;
    }

    return value;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsHeld
//
//  A digital control: a button, or one direction of a hat.
//
////////////////////////////////////////////////////////////////////////////////

bool MappingEvaluator::IsHeld (const ControllerSample & sample, const ControlId & control)
{
    bool  isHeld  = false;
    Byte  hatBits = 0;



    if (control.kind == ControlKind::Button)
    {
        isHeld = control.index >= 0 && control.index < ControllerSample::kButtonCount &&
                 sample.buttons.test ((size_t) control.index);
    }
    else if (control.index >= 0 && control.index < ControllerSample::kHatCount)
    {
        hatBits = sample.hats[(size_t) control.index];

        switch (control.kind)
        {
            case ControlKind::DpadUp:    isHeld = (hatBits & ControllerSample::kHatUp)    != 0; break;
            case ControlKind::DpadDown:  isHeld = (hatBits & ControllerSample::kHatDown)  != 0; break;
            case ControlKind::DpadLeft:  isHeld = (hatBits & ControllerSample::kHatLeft)  != 0; break;
            case ControlKind::DpadRight: isHeld = (hatBits & ControllerSample::kHatRight) != 0; break;
            default: break;
        }
    }

    return isHeld;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsButtonHeld
//
////////////////////////////////////////////////////////////////////////////////

bool MappingEvaluator::IsButtonHeld (const ControllerSample & sample, const ButtonBinding & binding)
{
    bool   isAnalog = binding.control.kind == ControlKind::Axis || binding.control.kind == ControlKind::Trigger;
    float  value    = 0.0f;



    if (!isAnalog)
    {
        return IsHeld (sample, binding.control);
    }

    value = ReadAnalog (sample, binding.control);

    if (binding.control.kind == ControlKind::Trigger)
    {
        // A trigger reads -1 at rest once stretched, so compare on its own
        // 0-to-1 scale rather than the stretched one.
        value = (value + 1.0f) / 2.0f;

        return value >= binding.threshold;
    }

    return binding.negativeDirection ? value <= -binding.threshold : value >= binding.threshold;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsButtonListHeld
//
////////////////////////////////////////////////////////////////////////////////

bool MappingEvaluator::IsButtonListHeld (const ControllerSample & sample, const std::vector<ButtonBinding> & bindings)
{
    bool  isHeld = false;



    for (const ButtonBinding & binding : bindings)
    {
        isHeld = isHeld || IsButtonHeld (sample, binding);
    }

    return isHeld;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EvaluateAxisBinding
//
////////////////////////////////////////////////////////////////////////////////

float MappingEvaluator::EvaluateAxisBinding (const ControllerSample & sample, const AxisBinding & binding)
{
    bool   isNegative = false;
    bool   isPositive = false;
    float  value      = 0.0f;



    if (binding.kind == AxisBindingKind::DigitalPair)
    {
        isNegative = IsHeld (sample, binding.negative);
        isPositive = IsHeld (sample, binding.positive);

        // Both directions held is no direction at all, the same answer the
        // arrow keys give.
        value = (isNegative == isPositive) ? 0.0f : (isNegative ? -1.0f : 1.0f);

        return value;
    }

    value = ReadAnalog (sample, binding.analog);

    return binding.inverted ? -value : value;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EvaluateAxis
//
//  Whichever control is furthest from center wins, so a stick sitting at rest
//  does not dilute a D-pad press mapped to the same axis. With every control
//  at center the first binding is the winner, so a rate binding at rest still
//  holds its paddle rather than handing the axis to an absolute center.
//
////////////////////////////////////////////////////////////////////////////////

float MappingEvaluator::EvaluateAxis (const ControllerSample & sample, const std::vector<AxisBinding> & bindings, const AxisBinding *& outWinner)
{
    float  winner = 0.0f;



    outWinner = bindings.empty() ? nullptr : &bindings.front();

    for (const AxisBinding & binding : bindings)
    {
        float  value = EvaluateAxisBinding (sample, binding);

        if (std::abs (value) > std::abs (winner))
        {
            winner    = value;
            outWinner = &binding;
        }
    }

    return winner;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsOneStick
//
//  True when both axes are single absolute bindings on the two axes of one
//  physical stick, which is when a round deadzone is the right shape. A rate
//  binding moves one paddle on its own, so each is shaped on its own.
//
////////////////////////////////////////////////////////////////////////////////

bool MappingEvaluator::IsOneStick (const std::vector<AxisBinding> & xBindings, const std::vector<AxisBinding> & yBindings)
{
    bool  isPair = xBindings.size() == 1 && yBindings.size() == 1;
    int   xIndex = 0;
    int   yIndex = 0;



    if (!isPair)
    {
        return false;
    }

    if (xBindings[0].kind != AxisBindingKind::Analog || yBindings[0].kind != AxisBindingKind::Analog)
    {
        return false;
    }

    if (xBindings[0].analog.kind != ControlKind::Axis || yBindings[0].analog.kind != ControlKind::Axis)
    {
        return false;
    }

    if (xBindings[0].response != AxisResponse::Absolute || yBindings[0].response != AxisResponse::Absolute)
    {
        return false;
    }

    xIndex = xBindings[0].analog.index;
    yIndex = yBindings[0].analog.index;

    return (xIndex == XInputSampleDecoder::kLeftStickX  && yIndex == XInputSampleDecoder::kLeftStickY) ||
           (xIndex == XInputSampleDecoder::kRightStickX && yIndex == XInputSampleDecoder::kRightStickY);
}
