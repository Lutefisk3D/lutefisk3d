//
// Copyright (c) 2008-2016 the Urho3D project.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

#pragma once

#include "../Sample.h"

namespace Urho3D
{

class Node;
class BufferedSoundStream;

}

/// Sound synthesis example.
/// This sample demonstrates:
///     - Playing back a sound stream produced on-the-fly by a simple CPU synthesis algorithm
class SoundSynthesis : public Sample
{
public:
    /// Construct.
    SoundSynthesis(Context* context);

    /// Setup before engine initialization. Modifies the engine parameters.
    virtual void Setup() override;
    /// Setup after engine initialization and before running the main loop.
    virtual void Start() override;

private:
    /// Construct the sound stream and start playback.
    void CreateSound();
    /// Buffer more sound data.
    void UpdateSound();
    /// Construct an instruction text to the UI.
    void CreateInstructions();
    /// Subscribe to application-wide logic update events.
    void SubscribeToEvents();
    /// Handle the logic update event.
    void HandleUpdate(float timeStep);

    /// Scene node for the sound component.
    SharedPtr<Node> node_;
    /// Sound stream that we update.
    SharedPtr<BufferedSoundStream> soundStream_;
    /// Instruction text.
    SharedPtr<Text> instructionText_;
    /// Filter coefficient for the sound.
    float filter_;
    /// Synthesis accumulator.
    float accumulator_;
    /// First oscillator.
    float osc1_;
    /// Second oscillator.
    float osc2_;
};
