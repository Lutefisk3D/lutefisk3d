//
// Copyright (c) 2008-2018 the Urho3D project.
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

#include "Lutefisk3D/Container/FlagSet.h"
#include "Lutefisk3D/Core/Object.h"
#include "Lutefisk3D/Core/Timer.h"
#include "Lutefisk3D/Math/Vector2.h"
#include "Lutefisk3D/Math/Rect.h"
#include "Lutefisk3D/Container/Str.h"

namespace Urho3D
{

enum DebugHudMode : unsigned
{
    DEBUGHUD_SHOW_NONE = 0x0,
    DEBUGHUD_SHOW_STATS = 0x1,
    DEBUGHUD_SHOW_MODE = 0x2,
    DEBUGHUD_SHOW_ALL = 0x7,
};
URHO3D_FLAGSET(DebugHudMode, DebugHudModeFlags);

/// Displays rendering stats and profiling information.
class LUTEFISK3D_EXPORT DebugHud : public Object
{
    URHO3D_OBJECT(DebugHud, Object)

public:
    /// Construct.
    explicit DebugHud(Context* context);
    /// Destruct.
    ~DebugHud() override;

    /// Set elements to show.
    /// \param mode is a combination of DEBUGHUD_SHOW_* flags.
    void SetMode(DebugHudModeFlags mode);
    /// Cycle through elements
    void CycleMode();
    /// Set whether to show 3D geometry primitive/batch count only. Default false.
    void SetUseRendererStats(bool enable);
    /// Toggle elements.
    /// \param mode is a combination of DEBUGHUD_SHOW_* flags.
    void Toggle(DebugHudModeFlags mode);
    /// Toggle all elements.
    void ToggleAll();
    /// Return currently shown elements.
    /// \return a combination of DEBUGHUD_SHOW_* flags.
    DebugHudModeFlags GetMode() const { return mode_; }
    /// Return whether showing 3D geometry primitive/batch count only.
    bool GetUseRendererStats() const { return useRendererStats_; }
    /// Set application-specific stats.
    /// \param label a title of stat to be displayed.
    /// \param stats a variant value to be displayed next to the specified label.
    void SetAppStats(const QString& label, const Variant& stats);
    /// Set application-specific stats.
    /// \param label a title of stat to be displayed.
    /// \param stats a string value to be displayed next to the specified label.
    void SetAppStats(const QString& label, const QString& stats);
    /// Reset application-specific stats. Return true if it was erased successfully.
    /// \param label a title of stat to be reset.
    bool ResetAppStats(const QString& label);
    /// Clear all application-specific stats.
    void ClearAppStats();
    /// Limit rendering area of debug hud.
    /// \param position of debug hud from top-left corner of the screen.
    /// \param size specifies size of debug hud rect. Pass zero vector to occupy entire screen and automatically resize
    /// debug hud to the size of the screen later. Calling this method with non-zero size parameter requires user to manually resize debug hud on screen size changes later.
    void SetExtents(const IntVector2& position = IntVector2::ZERO, IntVector2 size = IntVector2::ZERO);

private:
    /// Render system ui.
    void RenderUi(float ts);
    /// Update positions debug hud elements. Called on intializaton or when window size changes.
    void RecalculateWindowPositions();
    /// Snap position to the extents of debug hud rendering rect set by SetExtents().
    Vector2 WithinExtents(Vector2 pos);
    void screenModeChanged(int, int, bool, bool, bool, bool, int, int);

    /// Hashmap containing application specific stats.
    HashMap<QString, QString> appStats_;
    /// Show 3D geometry primitive/batch count flag.
    bool useRendererStats_;
    /// Current shown-element mode.
    DebugHudModeFlags mode_;
    /// FPS timer.
    Timer fpsTimer_;
    /// Calculated fps
    unsigned fps_;
    /// DebugHud extents that data will be rendered in.
    IntRect extents_;
    /// Cached position (bottom-left corner) of mode information.
    Vector2 posMode_;
    /// Cached position (top-left corner) of stats.
    Vector2 posStats_;
};

}
