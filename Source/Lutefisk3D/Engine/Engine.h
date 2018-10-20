//
// Copyright (c) 2008-2017 the Urho3D project.
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

#include "Lutefisk3D/Core/Object.h"
#include "Lutefisk3D/Core/Timer.h"
#include "Lutefisk3D/Engine/jlsignal/SignalBase.h"
#include <algorithm>
namespace Urho3D
{

class Console;
class DebugHud;

/// Urho3D engine. Creates the other subsystems.
class LUTEFISK3D_EXPORT Engine : public Object
{
    URHO3D_OBJECT(Engine,Object)

public:
    /// Construct.
    explicit Engine(Context* context);
    /// Destruct. Free all subsystems.
    ~Engine() override;

    bool Initialize(const VariantMap& parameters);
    bool InitializeResourceCache(const VariantMap& parameters, bool removeOld = true);
    void RunFrame();
    Console* CreateConsole();
    DebugHud* CreateDebugHud();
    void SetMinFps(int fps);
    void SetMaxFps(unsigned fps);
    void SetMaxInactiveFps(unsigned fps);
    void SetTimeStepSmoothing(int frames);
    /// Set whether to pause update events and audio when minimized.
    void SetPauseMinimized(bool enable) { pauseMinimized_ = enable; }
    /// Set whether to exit automatically on exit request (window close button.)
    void SetAutoExit(bool enable) { autoExit_ = enable; }
    /// Override timestep of the next frame. Should be called in between RunFrame() calls.
    void SetNextTimeStep(float seconds) {  timeStep_ = std::max(seconds, 0.0f); }
    /// Close the graphics window and set the exit flag. No-op on iOS, as an iOS application can not legally exit.
    void Exit();
    /// Dump information of all resources to the log.
    void DumpResources(bool dumpFileName = false);
    /// Dump information of all memory allocations to the log. Supported in MSVC debug mode only.
    void DumpMemory();

    /// Get timestep of the next frame. Updated by ApplyFrameLimit().
    float GetNextTimeStep() const { return timeStep_; }
    /// Return the minimum frames per second.
    unsigned GetMinFps() const { return minFps_; }
    /// Return the maximum frames per second.
    unsigned GetMaxFps() const { return maxFps_; }
    /// Return the maximum frames per second when the application does not have input focus.
    unsigned GetMaxInactiveFps() const { return maxInactiveFps_; }
    /// Return how many frames to average for timestep smoothing.
    unsigned GetTimeStepSmoothing() const { return timeStepSmoothing_; }
    /// Return whether to pause update events and audio when minimized.
    bool GetPauseMinimized() const { return pauseMinimized_; }
    /// Return whether to exit automatically on exit request.
    bool GetAutoExit() const { return autoExit_; }
    /// Return whether engine has been initialized.
    bool IsInitialized() const { return initialized_; }
    /// Return whether exit has been requested.
    bool IsExiting() const { return exiting_; }
    /// Return whether the engine has been created in headless mode.
    bool IsHeadless() const { return headless_; }


    void Update();
    void Render();
    void ApplyFrameLimit();

    /// Parse the engine startup parameters map from command line arguments.
    static VariantMap ParseParameters(const QStringList& arguments);
    /// Return whether startup parameters contains a specific parameter.
    static bool HasParameter(const VariantMap& parameters, const QString& parameter);

private:
    void HandleExitRequested();
    void DoExit();

    HiresTimer         frameTimer_;        ///< Frame update timer.
    std::vector<float> lastTimeSteps_;     ///< Previous timesteps for smoothing.
    float              timeStep_;          ///< Next frame timestep in seconds.
    unsigned           timeStepSmoothing_; ///< How many frames to average for the smoothed timestep.
    unsigned           minFps_;            ///< Minimum frames per second.
    unsigned           maxFps_;            ///< Maximum frames per second.
    unsigned           maxInactiveFps_;    ///< Maximum frames per second when the application does not have input focus.
    bool               pauseMinimized_;    ///< Pause when minimized flag.
    bool               autoExit_;          ///< Auto-exit flag.
    bool               initialized_;       ///< Initialized flag.
    bool               exiting_;           ///< Exiting flag.
    bool               headless_;          ///< Headless mode flag.
    bool               audioPaused_;       ///< Audio paused flag.
#ifdef LUTEFISK3D_TESTING
    /// Time out counter for testing.
    long long timeOut_;
#endif
};
}
