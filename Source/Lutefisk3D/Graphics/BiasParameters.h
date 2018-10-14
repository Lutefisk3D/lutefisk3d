#pragma once
#include "Lutefisk3D/Core/Lutefisk3D.h"
namespace Urho3D
{

/// Depth bias parameters. Used both by lights (for shadow mapping) and materials.
struct LUTEFISK3D_EXPORT BiasParameters
{
    /// Construct undefined.
    BiasParameters()
    {
    }

    /// Construct with initial values.
    BiasParameters(float constantBias, float slopeScaledBias, float normalOffset = 0.0f) :
        constantBias_(constantBias),
        slopeScaledBias_(slopeScaledBias),
        normalOffset_(normalOffset)
    {
    }

    /// Validate parameters.
    void Validate();

    /// Constant bias.
    float constantBias_;
    /// Slope scaled bias.
    float slopeScaledBias_;
    /// Normal offset multiplier.
    float normalOffset_;
};
/// Shadow map focusing parameters.
struct LUTEFISK3D_EXPORT FocusParameters
{
    /// Construct undefined.
    FocusParameters() = default;

    /// Construct with initial values.
    FocusParameters(bool focus, bool nonUniform, bool autoSize, float quantize, float minView) :
        quantize_(quantize),
        minView_(minView),
        focus_(focus),
        nonUniform_(nonUniform),
        autoSize_(autoSize)
    {
    }

    /// Validate parameters.
    void Validate();

    /// Focus quantization.
    float quantize_;
    /// Minimum view size.
    float minView_;
    /// Focus flag.
    bool focus_;
    /// Non-uniform focusing flag.
    bool nonUniform_;
    /// Auto-size (reduce resolution when far away) flag.
    bool autoSize_;
};
}
