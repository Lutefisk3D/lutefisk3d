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

#include "Lutefisk3D/Input/InputEvents.h"
#include "Lutefisk3D/Core/Mutex.h"
#include "Lutefisk3D/Core/Object.h"
#include "Lutefisk3D/Math/Vector2.h"
#include "Lutefisk3D/Engine/jlsignal/SignalBase.h"
#include <QtCore/QSet>

struct GLFWwindow;
namespace Urho3D
{

/// %Input Mouse Modes.
enum MouseMode : uint8_t
{
    MM_ABSOLUTE = 0,
    MM_RELATIVE,
    MM_FREE,
    MM_INVALID,
};

class Deserializer;
class Graphics;
class Serializer;
class UIElement;
class XMLFile;

const IntVector2 MOUSE_POSITION_OFFSCREEN = IntVector2(M_MIN_INT, M_MIN_INT);

/// %Input state for a joystick.
struct JoystickState
{
    /// Initialize the number of buttons, axes and hats and set them to neutral state.
    void Initialize(unsigned numButtons, unsigned numAxes, unsigned numHats);
    /// Reset button, axis and hat states to neutral.
    void Reset();

    /// Return whether is a game controller. Game controllers will use standardized axis and button mappings.
    bool IsController() const { return false; }
    /// Return number of buttons.
    unsigned GetNumButtons() const { return buttons_.size(); }
    /// Return number of axes.
    unsigned GetNumAxes() const { return axes_.size(); }
    /// Return number of hats.
    unsigned GetNumHats() const { return hats_.size(); }
    /// Check if a button is held down.
    bool GetButtonDown(unsigned index) const { return index < buttons_.size() ? buttons_[index] : false; }
    /// Check if a button has been pressed on this frame.
    bool GetButtonPress(unsigned index) const { return index < buttonPress_.size() ? buttonPress_[index] : false; }
    /// Return axis position.
    float GetAxisPosition(unsigned index) const { return index < axes_.size() ? axes_[index] : 0.0f; }
    /// Return hat position.
    HatPosition GetHatPosition(unsigned index) const
    {
        return index < hats_.size() ? hats_[index] : HatPosition::CENTERED;
    }

    /// Joystick name.
    QString name_;
    /// Button up/down state.
    std::vector<bool> buttons_;
    /// Button pressed on this frame.
    std::vector<bool> buttonPress_;
    /// Axis position from -1 to 1.
    std::vector<float> axes_;
    /// POV hat bits.
    std::vector<HatPosition> hats_;
    int joystickID_;
};

/// %Input subsystem. Converts operating system window messages to input state and events.
class LUTEFISK3D_EXPORT Input : public jl::SignalObserver
{
public:
    /// Construct.
    Input(Context* context);
    virtual ~Input();

    /// Poll for window messages. Called by HandleBeginFrame().
    void Update();
    /// Set whether ALT-ENTER fullscreen toggle is enabled.
    void SetToggleFullscreen(bool enable);
    /// Set whether the operating system mouse cursor is visible. When not visible (default), is kept centered to prevent leaving the window. Mouse visibility event can be suppressed-- this also recalls any unsuppressed SetMouseVisible which can be returned by ResetMouseVisible().
    void SetMouseVisible(bool enable, bool suppressEvent = false);
    /// Reset last mouse visibility that was not suppressed in SetMouseVisible.
    void ResetMouseVisible();
    /// Set whether the mouse is currently being grabbed by an operation.
    void SetMouseGrabbed(bool grab, bool suppressEvent = false);
    /// Reset the mouse grabbed to the last unsuppressed SetMouseGrabbed call
    void ResetMouseGrabbed();
    /// Set the mouse mode.
    void SetMouseMode(MouseMode mode, bool suppressEvent = false);
    /// Reset the last mouse mode that wasn't suppressed in SetMouseMode
    void ResetMouseMode();
    /// Set the mouse cursor position. Uses the backbuffer (Graphics width/height) coordinates.
    void SetMousePosition(const IntVector2& position);
    /// Center the mouse position.
    void CenterMousePosition();

    /// Return name of key from keycode.
    QString GetKeyName(int key) const;
    /// Return name of key from scancode.
    QString GetScancodeName(int scancode) const;
    /// Check if a key is held down.
    bool GetKeyDown(int key) const;
    /// Check if a key has been pressed on this frame.
    bool GetKeyPress(int key) const;
    /// Check if a key is held down by scancode.
    bool GetScancodeDown(int scancode) const;
    /// Check if a key has been pressed on this frame by scancode.
    bool GetScancodePress(int scancode) const;
    /// Check if a mouse button is held down.
    bool GetMouseButtonDown(MouseButton button) const;
    /// Check if a mouse button has been pressed on this frame.
    bool GetMouseButtonPress(MouseButton button) const;
    /// Check if a qualifier key is held down.
    bool GetQualifierDown(int qualifier) const;
    /// Check if a qualifier key has been pressed on this frame.
    bool GetQualifierPress(int qualifier) const;
    /// Return the currently held down qualifiers.
    int GetQualifiers() const;
    /// Return mouse position within window. Should only be used with a visible mouse cursor. Uses the backbuffer (Graphics width/height) coordinates.
    IntVector2 GetMousePosition() const;
    /// Return mouse movement since last frame.
    IntVector2 GetMouseMove() const;
    /// Return horizontal mouse movement since last frame.
    int GetMouseMoveX() const;
    /// Return vertical mouse movement since last frame.
    int GetMouseMoveY() const;
    /// Return mouse wheel movement since last frame.
    int GetMouseMoveWheel() const { return mouseMoveWheel_; }
    /// Return input coordinate scaling. Should return non-unity on High DPI display.
    Vector2 GetInputScale() const { return inputScale_; }
    /// Return number of connected joysticks.
    unsigned GetNumJoysticks() const { return joysticks_.size(); }
    /// Return joystick state by ID, or null if does not exist.
    JoystickState* GetJoystick(int id);
    /// Return joystick state by index, or null if does not exist. 0 = first connected joystick.
    JoystickState* GetJoystickByIndex(unsigned index);
    /// Return joystick state by name, or null if does not exist.
    JoystickState* GetJoystickByName(const QString& name);

    /// Return whether fullscreen toggle is enabled.
    bool GetToggleFullscreen() const { return toggleFullscreen_; }
    /// Return whether the operating system mouse cursor is visible.
    bool IsMouseVisible() const { return mouseVisible_; }
    /// Return whether the mouse is currently being grabbed by an operation.
    bool IsMouseGrabbed() const { return mouseGrabbed_; }
    /// Return whether the mouse is locked to the window
    bool IsMouseLocked() const;
    /// Return the mouse mode.
    MouseMode GetMouseMode() const { return mouseMode_; }
    /// Return whether application window has input focus.
    bool HasFocus() { return inputFocus_; }
    /// Return whether application window is minimized.
    bool IsMinimized() const;

    // These methods allow for raising driving the input externally.
    /// Prepare input state for application gaining input focus.
    void GainFocus();
    /// Prepare input state for application losing input focus.
    void LoseFocus();
    /// Handle a mouse button change.
    void SetMouseButton(MouseButton button, bool newState);
    /// Handle a key change.
    void SetKey(int key, int scancode, bool newState);
    /// Handle mouse wheel change.
    void SetMouseWheel(int delta);
private:
    /// Initialize when screen mode initially set.
    void Initialize();
    /// Open a joystick and return its ID. Return -1 if no joystick.
    int OpenJoystick(unsigned index);
    /// Setup internal joystick structures.
    void ResetJoysticks();
    /// Clear input state.
    void ResetState();
    /// Reset input accumulation.
    void ResetInputAccumulation();
    /// Send an input focus or window minimization change event.
    void SendInputFocusEvent();
    /// Suppress next mouse movement.
    void SuppressNextMouseMove();
    /// Unsuppress mouse movement.
    void UnsuppressMouseMove();
    /// Handle screen mode event.
    void HandleScreenMode(int Width, int Height, bool Fullscreen, bool Borderless, bool Resizable, bool HighDPI, int Monitor, int RefreshRate);
    /// Handle frame start event.
    void HandleBeginFrame(unsigned frameno, float ts);

    void ResetMousePos();
    void updateJostickStates();
    static void mouseMovedInWindow(GLFWwindow *win,double x,double y);
    static void iconificationChanged(GLFWwindow *w, int state);
    static void windowMoved(GLFWwindow *w,int xpos,int ypos);
    static void windowResized(GLFWwindow *w,int width,int h);
    Context *m_context;
    /// Graphics subsystem.
    WeakPtr<Graphics> graphics_;
    /// Key down state.
    QSet<int> keyDown_;
    /// Key pressed state.
    QSet<int> keyPress_;
    /// Key down state by scancode.
    QSet<int> scancodeDown_;
    /// Key pressed state by scancode.
    QSet<int> scancodePress_;
    /// String for text input.
    QString textInput_;
    /// Opened joysticks.
    HashMap<int, JoystickState> joysticks_;
    /// Mouse buttons' down state.
    unsigned mouseButtonDown_;
    /// Mouse buttons' pressed state.
    unsigned mouseButtonPress_;
    /// Last mouse position for calculating movement.
    IntVector2 lastMousePosition_;
    /// Last mouse position before being set to not visible.
    IntVector2 lastVisibleMousePosition_;
    /// Mouse movement since last frame.
    Vector2 mouseMove_;
    Vector2 mousePosLastUpdate_;
    /// Mouse wheel movement since last frame.
    int mouseMoveWheel_;
    /// Input coordinate scaling. Non-unity when window and backbuffer have different sizes (e.g. Retina display.)
    Vector2 inputScale_;
    /// Fullscreen toggle flag.
    bool toggleFullscreen_;
    /// Operating system mouse cursor visible flag.
    bool mouseVisible_;
    /// The last operating system mouse cursor visible flag set by end use call to SetMouseVisible.
    bool lastMouseVisible_;
    /// Flag to indicate the mouse is being grabbed by an operation. Subsystems like UI that uses mouse should temporarily ignore the mouse hover or click events.
    bool mouseGrabbed_;
    /// The last mouse grabbed set by SetMouseGrabbed.
    bool lastMouseGrabbed_;
    /// Determines the mode of mouse behaviour.
    MouseMode mouseMode_;
    /// The last mouse mode set by SetMouseMode.
    MouseMode lastMouseMode_;
    /// Flag to determine whether SDL mouse relative was used.
    bool sdlMouseRelative_;
    /// Input focus flag.
    bool inputFocus_;
    /// Minimized flag.
    bool minimized_;
    /// Gained focus on this frame flag.
    bool focusedThisFrame_;
    /// Next mouse move suppress flag.
    bool suppressNextMouseMove_;
    /// Whether mouse move is accumulated in backbuffer scale or not (when using events directly).
    bool mouseMoveScaled_;
    /// Initialized flag.
    bool initialized_;
};
}
