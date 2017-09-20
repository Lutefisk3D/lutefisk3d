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

#include "Input.h"

#include "Lutefisk3D/Core/Context.h"
#include "Lutefisk3D/Core/CoreEvents.h"
#include "Lutefisk3D/IO/FileSystem.h"
#include "Lutefisk3D/Graphics/Graphics.h"
#include "Lutefisk3D/Graphics/GraphicsEvents.h"
#include "Lutefisk3D/IO/Log.h"
#include "Lutefisk3D/Core/Mutex.h"
#include "Lutefisk3D/Core/ProcessUtils.h"
#include "Lutefisk3D/Core/Profiler.h"
#include "Lutefisk3D/Core/StringUtils.h"
#include "Lutefisk3D/UI/UI.h"

#include <cstring>

#include <SDL2/SDL.h>

// Use a "click inside window to focus" mechanism on desktop platforms when the mouse cursor is hidden
// TODO: For now, in this particular case only, treat all the ARM on Linux as "desktop" (e.g. RPI, odroid, etc), revisit this again when we support "mobile" ARM on Linux
#if defined(_WIN32) || (defined(__APPLE__)) || (defined(__linux__))
#define REQUIRE_CLICK_TO_FOCUS
#endif

namespace Urho3D
{
const int SCREEN_JOYSTICK_START_ID = 0x40000000;
const StringHash VAR_BUTTON_KEY_BINDING("VAR_BUTTON_KEY_BINDING");
const StringHash VAR_BUTTON_MOUSE_BUTTON_BINDING("VAR_BUTTON_MOUSE_BUTTON_BINDING");
const StringHash VAR_LAST_KEYSYM("VAR_LAST_KEYSYM");

/// Convert SDL keycode if necessary.
int ConvertSDLKeyCode(int keySym, int scanCode)
{
    if (scanCode == SCANCODE_AC_BACK)
        return KEY_ESCAPE;
    else
        return SDL_tolower(keySym);
}

void JoystickState::Initialize(unsigned numButtons, unsigned numAxes, unsigned numHats)
{
    buttons_.resize(numButtons);
    buttonPress_.resize(numButtons);
    axes_.resize(numAxes);
    hats_.resize(numHats);

    Reset();
}

void JoystickState::Reset()
{
    for (unsigned i = 0; i < buttons_.size(); ++i)
    {
        buttons_[i] = false;
        buttonPress_[i] = false;
    }
    for (unsigned i = 0; i < axes_.size(); ++i)
        axes_[i] = 0.0f;
    for (unsigned i = 0; i < hats_.size(); ++i)
        hats_[i] = HAT_CENTER;
}

Input::Input(Context* context) :
    m_context(context),
    mouseButtonDown_(0),
    mouseButtonPress_(0),
    lastVisibleMousePosition_(MOUSE_POSITION_OFFSCREEN),
    mouseMoveWheel_(0),
    inputScale_(Vector2::ONE),
    windowID_(0),
    toggleFullscreen_(true),
    mouseVisible_(false),
    lastMouseVisible_(false),
    mouseGrabbed_(false),
    lastMouseGrabbed_(false),
    mouseMode_(MM_ABSOLUTE),
    lastMouseMode_(MM_ABSOLUTE),
    sdlMouseRelative_(false),
    inputFocus_(false),
    minimized_(false),
    focusedThisFrame_(false),
    suppressNextMouseMove_(false),
    mouseMoveScaled_(false),
    initialized_(false)
{
    m_context->RequireSDL(SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER);

    g_graphicsSignals.newScreenMode.Connect(this,&Input::HandleScreenMode);

    // Try to initialize right now, but skip if screen mode is not yet set
    Initialize();
}

Input::~Input()
{
    m_context->ReleaseSDL();
}

void Input::Update()
{
    assert(initialized_);

    URHO3D_PROFILE_CTX(m_context,UpdateInput);

    bool mouseMoved = false;
    if (mouseMove_ != IntVector2::ZERO)
        mouseMoved = true;

    ResetInputAccumulation();

    SDL_Event evt;
    while (SDL_PollEvent(&evt))
        HandleSDLEvent(&evt);

    if (suppressNextMouseMove_ && (mouseMove_ != IntVector2::ZERO || mouseMoved))
        UnsuppressMouseMove();

    // Check for focus change this frame
    SDL_Window* window = graphics_->GetWindow();
    unsigned flags = window ? SDL_GetWindowFlags(window) & (SDL_WINDOW_INPUT_FOCUS | SDL_WINDOW_MOUSE_FOCUS) : 0;
    if (window)
    {
#ifdef REQUIRE_CLICK_TO_FOCUS
        // When using the "click to focus" mechanism, only focus automatically in fullscreen or non-hidden mouse mode
        if (!inputFocus_ && ((mouseVisible_ || mouseMode_ == MM_FREE) || graphics_->GetFullscreen()) && (flags & SDL_WINDOW_INPUT_FOCUS))
#else
        if (!inputFocus_ && (flags & SDL_WINDOW_INPUT_FOCUS))
#endif
            focusedThisFrame_ = true;

        if (focusedThisFrame_)
            GainFocus();

        // Check for losing focus. The window flags are not reliable when using an external window, so prevent losing focus in that case
        if (inputFocus_ && (flags & SDL_WINDOW_INPUT_FOCUS) == 0)
            LoseFocus();
    }
    else
        return;

    // Handle mouse mode MM_WRAP
    if (mouseVisible_ && mouseMode_ == MM_WRAP)
    {
        IntVector2 windowPos = graphics_->GetWindowPosition();
        IntVector2 mpos;
        SDL_GetGlobalMouseState(&mpos.x_, &mpos.y_);
        mpos -= windowPos;

        const int buffer = 5;
        const int width = graphics_->GetWidth() - buffer * 2;
        const int height = graphics_->GetHeight() - buffer * 2;
        // SetMousePosition utilizes backbuffer coordinate system, scale now from window coordinates
        mpos.x_ = (int)(mpos.x_ * inputScale_.x_);
        mpos.y_ = (int)(mpos.y_ * inputScale_.y_);

        bool warp = false;
        if (mpos.x_ < buffer)
        {
            warp = true;
            mpos.x_ += width;
        }

        if (mpos.x_ > buffer + width)
        {
            warp = true;
            mpos.x_ -= width;
        }

        if (mpos.y_ < buffer)
        {
            warp = true;
            mpos.y_ += height;
        }

        if (mpos.y_ > buffer + height)
        {
            warp = true;
            mpos.y_ -= height;
        }

        if (warp)
        {
            SetMousePosition(mpos);
            SuppressNextMouseMove();
        }
    }

    if (graphics_->WeAreEmbedded() || ((!sdlMouseRelative_ && !mouseVisible_ && mouseMode_ != MM_FREE) &&
                                           inputFocus_ && (flags & SDL_WINDOW_MOUSE_FOCUS)))
    {
        const IntVector2 mousePosition = GetMousePosition();
        mouseMove_ = mousePosition - lastMousePosition_;
        mouseMoveScaled_ = true; // Already in backbuffer scale, since GetMousePosition() operates in that

        if (graphics_->WeAreEmbedded())
            lastMousePosition_ = mousePosition;
        else
        {
            // Recenter the mouse cursor manually after move
            CenterMousePosition();
        }
        // Send mouse move event if necessary
        if (mouseMove_ != IntVector2::ZERO)
        {
            if (!suppressNextMouseMove_)
            {
                g_inputSignals.mouseMove.Emit(mousePosition.x_, mousePosition.y_, mouseMove_.x_, mouseMove_.y_,
                                              mouseButtonDown_, GetQualifiers());
            }
        }
    }
    else if (!mouseVisible_ && sdlMouseRelative_ && inputFocus_ && (flags & SDL_WINDOW_MOUSE_FOCUS))
    {
        // Keep the cursor trapped in window.
        CenterMousePosition();
    }
}

void Input::SetMouseVisible(bool enable, bool suppressEvent)
{
    const bool startMouseVisible = mouseVisible_;

    // In mouse mode relative, the mouse should be invisible
    if (mouseMode_ == MM_RELATIVE)
    {
        if (!suppressEvent)
            lastMouseVisible_ = enable;

        enable = false;
    }

    // SDL Raspberry Pi "video driver" does not have proper OS mouse support yet, so no-op for now
    if (enable != mouseVisible_)
    {
        if (initialized_)
        {
            // External windows can only support visible mouse cursor
            if (graphics_->WeAreEmbedded())
            {
                mouseVisible_ = true;
                if (!suppressEvent)
                    lastMouseVisible_ = true;
                return;
            }

            if (!enable && inputFocus_)
            {
                if (mouseVisible_)
                    lastVisibleMousePosition_ = GetMousePosition();

                if (mouseMode_ == MM_ABSOLUTE)
                    SetMouseModeAbsolute(SDL_TRUE);
                SDL_ShowCursor(SDL_FALSE);
                mouseVisible_ = false;
            }
            else if (mouseMode_ != MM_RELATIVE)
            {
                SetMouseGrabbed(false, suppressEvent);

                SDL_ShowCursor(SDL_TRUE);
                mouseVisible_ = true;

                if (mouseMode_ == MM_ABSOLUTE)
                    SetMouseModeAbsolute(SDL_FALSE);

                // Update cursor position
                Cursor* cursor = m_context->m_UISystem->GetCursor();
                // If the UI Cursor was visible, use that position instead of last visible OS cursor position
                if (cursor && cursor->IsVisible())
                {
                    IntVector2 pos = cursor->GetScreenPosition();
                    if (pos != MOUSE_POSITION_OFFSCREEN)
                    {
                        SetMousePosition(pos);
                        lastMousePosition_ = pos;
                    }
                }
                else
                {
                    if (lastVisibleMousePosition_ != MOUSE_POSITION_OFFSCREEN)
                    {
                        SetMousePosition(lastVisibleMousePosition_);
                        lastMousePosition_ = lastVisibleMousePosition_;
                    }
                }
            }
        }
        else
        {
            // Allow to set desired mouse visibility before initialization
            mouseVisible_ = enable;
        }

        if (mouseVisible_ != startMouseVisible)
        {
            SuppressNextMouseMove();
            if (!suppressEvent)
            {
                lastMouseVisible_ = mouseVisible_;
                g_inputSignals.mouseVisibleChanged.Emit(mouseVisible_);

            }
        }
    }
}

void Input::ResetMouseVisible()
{
    SetMouseVisible(lastMouseVisible_, false);
}

void Input::SetMouseGrabbed(bool grab, bool suppressEvent)
{
    // To not interfere with touch UI operation, never report the mouse as grabbed on Android / iOS
    mouseGrabbed_ = grab;

    if (!suppressEvent)
        lastMouseGrabbed_ = grab;
}

void Input::ResetMouseGrabbed()
{
    SetMouseGrabbed(lastMouseGrabbed_, true);
}


void Input::SetMouseModeAbsolute(SDL_bool enable)
{
    SDL_Window* const window = graphics_->GetWindow();

    SDL_SetWindowGrab(window, enable);
}

void Input::SetMouseModeRelative(SDL_bool enable)
{
    SDL_Window* const window = graphics_->GetWindow();

    int result = SDL_SetRelativeMouseMode(enable);
    sdlMouseRelative_ = enable && (result == 0);

    if (result == -1)
        SDL_SetWindowGrab(window, enable);
}
/** Set the mouse mode behaviour.
 *  MM_ABSOLUTE is the default behaviour, allowing the toggling of operating system cursor visibility and allowing the cursor to escape the window when visible.
 *  When the operating system cursor is invisible in absolute mouse mode, the mouse is confined to the window.
 *  If the operating system and UI cursors are both invisible, interaction with the Urho UI will be limited (eg: drag move / drag end events will not trigger).
 *  SetMouseMode(MM_ABSOLUTE) will call SetMouseGrabbed(false).
 *
 *  MM_RELATIVE sets the operating system cursor to invisible and confines the cursor to the window.
 *  The operating system cursor cannot be set to be visible in this mode via SetMouseVisible(), however changes are tracked and will be restored when another mouse mode is set.
 *  When the virtual cursor is also invisible, UI interaction will still function as normal (eg: drag events will trigger).
 *  SetMouseMode(MM_RELATIVE) will call SetMouseGrabbed(true).
 *
 *  MM_WRAP grabs the mouse from the operating system and confines the operating system cursor to the window, wrapping the cursor when it is near the edges.
 *  SetMouseMode(MM_WRAP) will call SetMouseGrabbed(true).
 *
 *  MM_FREE does not grab/confine the mouse cursor even when it is hidden. This can be used for cases where the cursor should render using the operating system
 *  outside the window, and perform custom rendering (with SetMouseVisible(false)) inside.
*/
void Input::SetMouseMode(MouseMode mode, bool suppressEvent)
{
    const MouseMode previousMode = mouseMode_;

    if (mode != mouseMode_)
    {
        if (initialized_)
    {
        SuppressNextMouseMove();

        mouseMode_ = mode;
        SDL_Window* const window = graphics_->GetWindow();

        Cursor* const cursor = m_context->m_UISystem->GetCursor();

        // Handle changing from previous mode
        if (previousMode == MM_ABSOLUTE)
        {
            if (!mouseVisible_)
                SetMouseModeAbsolute(SDL_FALSE);
        }
        if (previousMode == MM_RELATIVE)
        {
            SetMouseModeRelative(SDL_FALSE);
            ResetMouseVisible();
        }
        else if (previousMode == MM_WRAP)
            SDL_SetWindowGrab(window, SDL_FALSE);

        // Handle changing to new mode
        if (mode == MM_ABSOLUTE)
        {
            if (!mouseVisible_)
                SetMouseModeAbsolute(SDL_TRUE);
        }
        else if (mode == MM_RELATIVE)
        {
            SetMouseVisible(false, true);
            SetMouseModeRelative(SDL_TRUE);
        }
        else if (mode == MM_WRAP)
        {
            SetMouseGrabbed(true, suppressEvent);
            SDL_SetWindowGrab(window, SDL_TRUE);
        }

        if (mode != MM_WRAP)
            SetMouseGrabbed(!(mouseVisible_ || (cursor && cursor->IsVisible())), suppressEvent);
    }
        else
        {
            // Allow to set desired mouse mode before initialization
            mouseMode_ = mode;
        }
    }

    if (!suppressEvent)
    {
        lastMouseMode_ = mode;
        if (mouseMode_ != previousMode)
        {
            g_inputSignals.mouseModeChanged.Emit(mode,IsMouseLocked());
        }
    }
}

void Input::ResetMouseMode()
{
    SetMouseMode(lastMouseMode_, false);
}

void Input::SetToggleFullscreen(bool enable)
{
    toggleFullscreen_ = enable;
}

static void PopulateKeyBindingMap(HashMap<QString, int>& keyBindingMap)
{
    if (!keyBindingMap.empty())
        return;
    keyBindingMap.emplace("SPACE", KEY_SPACE);
    keyBindingMap.emplace("LCTRL", KEY_LCTRL);
    keyBindingMap.emplace("RCTRL", KEY_RCTRL);
    keyBindingMap.emplace("LSHIFT", KEY_LSHIFT);
    keyBindingMap.emplace("RSHIFT", KEY_RSHIFT);
    keyBindingMap.emplace("LALT", KEY_LALT);
    keyBindingMap.emplace("RALT", KEY_RALT);
    keyBindingMap.emplace("LGUI", KEY_LGUI);
    keyBindingMap.emplace("RGUI", KEY_RGUI);
    keyBindingMap.emplace("TAB", KEY_TAB);
    keyBindingMap.emplace("RETURN", KEY_RETURN);
    keyBindingMap.emplace("RETURN2", KEY_RETURN2);
    keyBindingMap.emplace("ENTER", KEY_KP_ENTER);
    keyBindingMap.emplace("SELECT", KEY_SELECT);
    keyBindingMap.emplace("LEFT", KEY_LEFT);
    keyBindingMap.emplace("RIGHT", KEY_RIGHT);
    keyBindingMap.emplace("UP", KEY_UP);
    keyBindingMap.emplace("DOWN", KEY_DOWN);
    keyBindingMap.emplace("PAGEUP", KEY_PAGEUP);
    keyBindingMap.emplace("PAGEDOWN", KEY_PAGEDOWN);
    keyBindingMap.emplace("F1", KEY_F1);
    keyBindingMap.emplace("F2", KEY_F2);
    keyBindingMap.emplace("F3", KEY_F3);
    keyBindingMap.emplace("F4", KEY_F4);
    keyBindingMap.emplace("F5", KEY_F5);
    keyBindingMap.emplace("F6", KEY_F6);
    keyBindingMap.emplace("F7", KEY_F7);
    keyBindingMap.emplace("F8", KEY_F8);
    keyBindingMap.emplace("F9", KEY_F9);
    keyBindingMap.emplace("F10", KEY_F10);
    keyBindingMap.emplace("F11", KEY_F11);
    keyBindingMap.emplace("F12", KEY_F12);
}

static void PopulateMouseButtonBindingMap(HashMap<QString, int>& mouseButtonBindingMap)
{
    if (!mouseButtonBindingMap.empty())
        return;
    mouseButtonBindingMap.emplace("LEFT", SDL_BUTTON_LEFT);
    mouseButtonBindingMap.emplace("MIDDLE", SDL_BUTTON_MIDDLE);
    mouseButtonBindingMap.emplace("RIGHT", SDL_BUTTON_RIGHT);
    mouseButtonBindingMap.emplace("X1", SDL_BUTTON_X1);
    mouseButtonBindingMap.emplace("X2", SDL_BUTTON_X2);
}

SDL_JoystickID Input::OpenJoystick(unsigned index)
{
    SDL_Joystick* joystick = SDL_JoystickOpen(index);
    if (!joystick)
    {
        URHO3D_LOGERROR(QString("Cannot open joystick #%1").arg(index) );
        return -1;
    }

    // Create joystick state for the new joystick
    int joystickID = SDL_JoystickInstanceID(joystick);
    JoystickState& state = joysticks_[joystickID];
    state.joystick_ = joystick;
    state.joystickID_ = joystickID;
    state.name_ = SDL_JoystickName(joystick);
    if (SDL_IsGameController(index))
        state.controller_ = SDL_GameControllerOpen(index);

    unsigned numButtons = (unsigned)SDL_JoystickNumButtons(joystick);
    unsigned numAxes = (unsigned)SDL_JoystickNumAxes(joystick);
    unsigned numHats = (unsigned)SDL_JoystickNumHats(joystick);

    // When the joystick is a controller, make sure there's enough axes & buttons for the standard controller mappings
    if (state.controller_)
    {
        if (numButtons < SDL_CONTROLLER_BUTTON_MAX)
            numButtons = SDL_CONTROLLER_BUTTON_MAX;
        if (numAxes < SDL_CONTROLLER_AXIS_MAX)
            numAxes = SDL_CONTROLLER_AXIS_MAX;
    }

    state.Initialize(numButtons, numAxes, numHats);

    return joystickID;
}

int Input::GetKeyFromName(const QString& name) const
{
    return SDL_GetKeyFromName(qPrintable(name));
}

int Input::GetKeyFromScancode(int scancode) const
{
    return SDL_GetKeyFromScancode((SDL_Scancode)scancode);
}

QString Input::GetKeyName(int key) const
{
    return SDL_GetKeyName(key);
}

int Input::GetScancodeFromKey(int key) const
{
    return SDL_GetScancodeFromKey(key);
}

int Input::GetScancodeFromName(const QString& name) const
{
    return SDL_GetScancodeFromName(qPrintable(name));
}

QString Input::GetScancodeName(int scancode) const
{
    return SDL_GetScancodeName((SDL_Scancode)scancode);
}

bool Input::GetKeyDown(int key) const
{
    return keyDown_.contains(SDL_tolower(key));
}

bool Input::GetKeyPress(int key) const
{
    return keyPress_.contains(SDL_tolower(key));
}

bool Input::GetScancodeDown(int scancode) const
{
    return scancodeDown_.contains(scancode);
}

bool Input::GetScancodePress(int scancode) const
{
    return scancodePress_.contains(scancode);
}

bool Input::GetMouseButtonDown(unsigned button) const
{
    return (mouseButtonDown_ & button) != 0;
}

bool Input::GetMouseButtonPress(unsigned button) const
{
    return (mouseButtonPress_ & button) != 0;
}

bool Input::GetQualifierDown(int qualifier) const
{
    if (qualifier == QUAL_SHIFT)
        return GetKeyDown(KEY_LSHIFT) || GetKeyDown(KEY_RSHIFT);
    if (qualifier == QUAL_CTRL)
        return GetKeyDown(KEY_LCTRL) || GetKeyDown(KEY_RCTRL);
    if (qualifier == QUAL_ALT)
        return GetKeyDown(KEY_LALT) || GetKeyDown(KEY_RALT);

    return false;
}

bool Input::GetQualifierPress(int qualifier) const
{
    if (qualifier == QUAL_SHIFT)
        return GetKeyPress(KEY_LSHIFT) || GetKeyPress(KEY_RSHIFT);
    if (qualifier == QUAL_CTRL)
        return GetKeyPress(KEY_LCTRL) || GetKeyPress(KEY_RCTRL);
    if (qualifier == QUAL_ALT)
        return GetKeyPress(KEY_LALT) || GetKeyPress(KEY_RALT);

    return false;
}

int Input::GetQualifiers() const
{
    int ret = 0;
    if (GetQualifierDown(QUAL_SHIFT))
        ret |= QUAL_SHIFT;
    if (GetQualifierDown(QUAL_CTRL))
        ret |= QUAL_CTRL;
    if (GetQualifierDown(QUAL_ALT))
        ret |= QUAL_ALT;

    return ret;
}

IntVector2 Input::GetMousePosition() const
{
    IntVector2 ret = IntVector2::ZERO;

    if (!initialized_)
        return ret;

    SDL_GetMouseState(&ret.x_, &ret.y_);
    ret.x_ = (int)(ret.x_ * inputScale_.x_);
    ret.y_ = (int)(ret.y_ * inputScale_.y_);

    return ret;
}
IntVector2 Input::GetMouseMove() const
{
    if (!suppressNextMouseMove_)
        return mouseMoveScaled_ ? mouseMove_ : IntVector2((int)(mouseMove_.x_ * inputScale_.x_), (int)(mouseMove_.y_ * inputScale_.y_));
    else
        return IntVector2::ZERO;
}

int Input::GetMouseMoveX() const
{
    if (!suppressNextMouseMove_)
        return mouseMoveScaled_ ? mouseMove_.x_ : (int)(mouseMove_.x_ * inputScale_.x_);
    else
        return 0;
}

int Input::GetMouseMoveY() const
{
    if (!suppressNextMouseMove_)
        return mouseMoveScaled_ ? mouseMove_.y_ : mouseMove_.y_ * inputScale_.y_;
    else
        return 0;
}

JoystickState* Input::GetJoystickByIndex(unsigned index)
{
    unsigned compare = 0;
    for (auto & elem : joysticks_)
    {
        if (compare++ == index)
            return &ELEMENT_VALUE(elem);
    }

    return nullptr;
}

JoystickState* Input::GetJoystickByName(const QString& name)
{
    for (auto i = joysticks_.begin(); i != joysticks_.end(); ++i)
    {
        if (MAP_VALUE(i).name_ == name)
            return &(MAP_VALUE(i));
    }

    return 0;
}

JoystickState* Input::GetJoystick(SDL_JoystickID id)
{
    auto i = joysticks_.find(id);
    return i != joysticks_.end() ? &MAP_VALUE(i) : nullptr;
}

bool Input::IsMouseLocked() const
{
    return !((mouseMode_ == MM_ABSOLUTE && mouseVisible_) || mouseMode_ == MM_FREE);
}

bool Input::IsMinimized() const
{
    // Return minimized state also when unfocused in fullscreen
    if (!inputFocus_ && graphics_ && graphics_->GetFullscreen())
        return true;
    else
        return minimized_;
}

void Input::Initialize()
{
    Graphics* graphics = m_context->m_Graphics.get();
    if (!graphics || !graphics->IsInitialized())
        return;

    graphics_ = graphics;

    // In external window mode only visible mouse is supported
    if (graphics_->WeAreEmbedded())
        mouseVisible_ = true;

    // Set the initial activation
    initialized_ = true;
    GainFocus();

    ResetJoysticks();
    ResetState();
    g_coreSignals.beginFrame.Connect(this,&Input::HandleBeginFrame);
    URHO3D_LOGINFO("Initialized input");
}

void Input::ResetJoysticks()
{
    joysticks_.clear();

    // Open each detected joystick automatically on startup
    unsigned size = static_cast<unsigned>(SDL_NumJoysticks());
    for (unsigned i = 0; i < size; ++i)
        OpenJoystick(i);
}

void Input::ResetInputAccumulation()
{
    // Reset input accumulation for this frame
    keyPress_.clear();
    scancodePress_.clear();
    mouseButtonPress_ = 0;
    mouseMove_ = IntVector2::ZERO;
    mouseMoveWheel_ = 0;
    for (auto i = joysticks_.begin(); i != joysticks_.end(); ++i)
    {
        for (unsigned j = 0; j < MAP_VALUE(i).buttonPress_.size(); ++j)
            MAP_VALUE(i).buttonPress_[j] = false;
    }
}


void Input::GainFocus()
{
    ResetState();

    inputFocus_ = true;
    focusedThisFrame_ = false;

    // Restore mouse mode
    const MouseMode mm = mouseMode_;
    mouseMode_ = MM_FREE;
    SetMouseMode(mm, true);

    SuppressNextMouseMove();

    // Re-establish mouse cursor hiding as necessary
    if (!mouseVisible_)
        SDL_ShowCursor(SDL_FALSE);

    SendInputFocusEvent();
}

void Input::LoseFocus()
{
    ResetState();

    inputFocus_ = false;
    focusedThisFrame_ = false;

    // Show the mouse cursor when inactive
    SDL_ShowCursor(SDL_TRUE);

    // Change mouse mode -- removing any cursor grabs, etc.
    const MouseMode mm = mouseMode_;
    SetMouseMode(MM_FREE, true);
    // Restore flags to reflect correct mouse state.
    mouseMode_ = mm;

    SendInputFocusEvent();
}

void Input::ResetState()
{
    keyDown_.clear();
    keyPress_.clear();
    scancodeDown_.clear();
    scancodePress_.clear();

    /// \todo Check if resetting joystick state on input focus loss is even necessary
    for (auto & elem : joysticks_)
        ELEMENT_VALUE(elem).Reset();

    // Use SetMouseButton() to reset the state so that mouse events will be sent properly
    SetMouseButton(MOUSEB_LEFT, false);
    SetMouseButton(MOUSEB_RIGHT, false);
    SetMouseButton(MOUSEB_MIDDLE, false);

    mouseMove_ = IntVector2::ZERO;
    mouseMoveWheel_ = 0;
    mouseButtonPress_ = 0;
}

void Input::SendInputFocusEvent()
{
    g_inputSignals.inputFocus.Emit(HasFocus(),IsMinimized());
}

void Input::SetMouseButton(unsigned button, bool newState)
{
    if (newState)
    {
        if (!(mouseButtonDown_ & button))
            mouseButtonPress_ |= button;

        mouseButtonDown_ |= button;
    }
    else
    {
        if (!(mouseButtonDown_ & button))
            return;

        mouseButtonDown_ &= ~button;
    }
    if(newState)
        g_inputSignals.mouseButtonDown.Emit(button,mouseButtonDown_,GetQualifiers());
    else
        g_inputSignals.mouseButtonUp.Emit(button,mouseButtonDown_,GetQualifiers());
}

void Input::SetKey(int key, int scancode, bool newState)
{

    bool repeat = false;

    if (newState)
    {
        scancodeDown_.insert(scancode);
        scancodePress_.insert(scancode);

        if (!keyDown_.contains(key))
        {
            keyDown_.insert(key);
            keyPress_.insert(key);
        }
        else
            repeat = true;
    }
    else
    {
        scancodeDown_.remove(scancode);

        if (!keyDown_.remove(key))
            return;
    }
    if(newState)
        g_inputSignals.keyDown.Emit(key,scancode,mouseButtonDown_,GetQualifiers(),repeat);
    else
        g_inputSignals.keyUp.Emit(key,scancode,mouseButtonDown_,GetQualifiers());

    if ((key == KEY_RETURN || key == KEY_RETURN2 || key == KEY_KP_ENTER) && newState && !repeat && toggleFullscreen_ &&
            (GetKeyDown(KEY_LALT) || GetKeyDown(KEY_RALT)))
        graphics_->ToggleFullscreen();
}

void Input::SetMouseWheel(int delta)
{

    if (delta)
    {
        mouseMoveWheel_ += delta;
        g_inputSignals.mouseWheel.Emit(delta,mouseButtonDown_,GetQualifiers());
    }
}

void Input::SetMousePosition(const IntVector2& position)
{
    if (!graphics_)
        return;

    SDL_WarpMouseInWindow(graphics_->GetWindow(), (int)(position.x_ / inputScale_.x_), (int)(position.y_ / inputScale_.y_));
}

void Input::CenterMousePosition()
{
    const IntVector2 center(graphics_->GetWidth() / 2, graphics_->GetHeight() / 2);
    if (GetMousePosition() != center)
    {
        SetMousePosition(center);
        lastMousePosition_ = center;
    }
}

void Input::SuppressNextMouseMove()
{
    suppressNextMouseMove_ = true;
    mouseMove_ = IntVector2::ZERO;
}

void Input::UnsuppressMouseMove()
{
    suppressNextMouseMove_ = false;
    mouseMove_ = IntVector2::ZERO;
    lastMousePosition_ = GetMousePosition();
}

void Input::HandleSDLEvent(void* sdlEvent)
{
    SDL_Event& evt = *static_cast<SDL_Event*>(sdlEvent);

    // While not having input focus, skip key/mouse/touch/joystick events, except for the "click to focus" mechanism
    if (!inputFocus_ && evt.type >= SDL_KEYDOWN && evt.type <= SDL_MULTIGESTURE)
    {
#ifdef REQUIRE_CLICK_TO_FOCUS
        // Require the click to be at least 1 pixel inside the window to disregard clicks in the title bar
        if (evt.type == SDL_MOUSEBUTTONDOWN && evt.button.x > 0 && evt.button.y > 0 && evt.button.x < graphics_->GetWidth() - 1 &&
                evt.button.y < graphics_->GetHeight() - 1)
        {
            focusedThisFrame_ = true;
            // Do not cause the click to actually go throughfin
            return;
        }
        else if (evt.type == SDL_FINGERDOWN)
        {
            // When focusing by touch, call GainFocus() immediately as it resets the state; a touch has sustained state
            // which should be kept
            GainFocus();
        }
        else
#endif
            return;
    }

    // Possibility for custom handling or suppression of default handling for the SDL event
    {
        bool wasConsumed = false;
        g_inputSignals.rawSDLInput.Emit(&evt,wasConsumed);
        if (wasConsumed)
            return;
    }

    switch (evt.type)
    {
    case SDL_KEYDOWN:
        SetKey(ConvertSDLKeyCode(evt.key.keysym.sym, evt.key.keysym.scancode), evt.key.keysym.scancode, true);
        break;

    case SDL_KEYUP:
        SetKey(ConvertSDLKeyCode(evt.key.keysym.sym, evt.key.keysym.scancode), evt.key.keysym.scancode, false);
        break;

    case SDL_TEXTINPUT:
    {
        textInput_ = QString::fromUtf8(evt.text.text);
        if (!textInput_.isEmpty())
        {
            g_inputSignals.textInput.Emit(textInput_);
        }
    }
        break;
    case SDL_TEXTEDITING:
        {
            textInput_ = QString::fromUtf8(evt.text.text);
            g_inputSignals.textEditing.Emit(textInput_,evt.edit.start,evt.edit.length);
        }
        break;
    case SDL_MOUSEBUTTONDOWN:
        SetMouseButton(1 << (evt.button.button - 1), true);
        break;

    case SDL_MOUSEBUTTONUP:
        SetMouseButton(1 << (evt.button.button - 1), false);
        break;

    case SDL_MOUSEMOTION:
        if ((sdlMouseRelative_ || mouseVisible_ || mouseMode_ == MM_FREE))
        {
            // Accumulate without scaling for accuracy, needs to be scaled to backbuffer coordinates when asked
            mouseMove_.x_ += evt.motion.xrel;
            mouseMove_.y_ += evt.motion.yrel;
            mouseMoveScaled_ = false;

            if (!suppressNextMouseMove_)
            {
                g_inputSignals.mouseMove.Emit(
                            (int)(evt.motion.x * inputScale_.x_),(int)(evt.motion.y * inputScale_.y_),
                            // The "on-the-fly" motion data needs to be scaled now, though this may reduce accuracy
                            (int)(evt.motion.xrel * inputScale_.x_),(int)(evt.motion.yrel * inputScale_.y_),
                            mouseButtonDown_,GetQualifiers()
                            );
            }
        }
        break;

    case SDL_MOUSEWHEEL:
        SetMouseWheel(evt.wheel.y);
        break;

    case SDL_DOLLARRECORD:
    {
        g_inputSignals.gestureRecorded.Emit((unsigned)evt.dgesture.gestureId);
    }
        break;

    case SDL_DOLLARGESTURE:
    {
        g_inputSignals.gestureInput.Emit((unsigned)evt.dgesture.gestureId,
                                         int(evt.dgesture.x * graphics_->GetWidth()),
                                         int(evt.dgesture.y * graphics_->GetHeight()),
                                         int(evt.dgesture.numFingers),
                                         evt.dgesture.error
                                         );
    }
        break;

    case SDL_MULTIGESTURE:
    {
        g_inputSignals.multiGesture.Emit(
                    int(evt.mgesture.x * graphics_->GetWidth()),
                    int(evt.mgesture.y * graphics_->GetHeight()),
                    int(evt.mgesture.numFingers),
                    M_RADTODEG * evt.mgesture.dTheta,
                    evt.mgesture.dDist
                    );
    }
        break;

    case SDL_JOYDEVICEADDED:
    {
        SDL_JoystickID joystickID = OpenJoystick((unsigned)evt.jdevice.which);

        g_inputSignals.joystickConnected.Emit(joystickID);
    }
        break;

    case SDL_JOYDEVICEREMOVED:
    {
        joysticks_.erase(evt.jdevice.which);
        g_inputSignals.joystickDisconnected.Emit(evt.jdevice.which);
    }
        break;

    case SDL_JOYBUTTONDOWN:
    {
        unsigned button = evt.jbutton.button;
        SDL_JoystickID joystickID = evt.jbutton.which;
        JoystickState& state = joysticks_[joystickID];

        // Skip ordinary joystick event for a controller
        if (!state.controller_)
        {
            if (button < state.buttons_.size())
            {
                state.buttons_[button] = true;
                state.buttonPress_[button] = true;
                g_inputSignals.joystickButtonDown.Emit(joystickID,button);
            }
        }
    }
        break;

    case SDL_JOYBUTTONUP:
    {
        unsigned button = evt.jbutton.button;
        SDL_JoystickID joystickID = evt.jbutton.which;
        JoystickState& state = joysticks_[joystickID];

        if (!state.controller_)
        {
            if (button < state.buttons_.size())
            {
                if (!state.controller_)
                    state.buttons_[button] = false;
                g_inputSignals.joystickButtonUp.Emit(joystickID,button);
            }
        }
    }
        break;

    case SDL_JOYAXISMOTION:
    {
        SDL_JoystickID joystickID = evt.jaxis.which;
        JoystickState& state = joysticks_[joystickID];

        if (!state.controller_)
        {
            float clampedPosition = Clamp((float)evt.jaxis.value / 32767.0f, -1.0f, 1.0f);

            if (evt.jaxis.axis < state.axes_.size())
            {
                // If the joystick is a controller, only use the controller axis mappings
                // (we'll also get the controller event)
                if (!state.controller_)
                    state.axes_[evt.jaxis.axis] = clampedPosition;
                g_inputSignals.joystickAxisMove.Emit(joystickID,evt.jaxis.axis,clampedPosition);
            }
        }
    }
        break;

    case SDL_JOYHATMOTION:
    {
        SDL_JoystickID joystickID = evt.jaxis.which;
        JoystickState& state = joysticks_[joystickID];

        if (evt.jhat.hat < state.hats_.size())
        {
            state.hats_[evt.jhat.hat] = evt.jhat.value;
            g_inputSignals.joystickHatMove.Emit(joystickID,evt.jhat.hat,evt.jhat.value);
        }
    }
        break;

    case SDL_CONTROLLERBUTTONDOWN:
    {
        unsigned button = evt.cbutton.button;
        SDL_JoystickID joystickID = evt.cbutton.which;
        JoystickState& state = joysticks_[joystickID];

        if (button < state.buttons_.size())
        {
            state.buttons_[button] = true;
            state.buttonPress_[button] = true;
            g_inputSignals.joystickButtonDown.Emit(joystickID,button);
        }
    }
        break;

    case SDL_CONTROLLERBUTTONUP:
    {
        unsigned button = evt.cbutton.button;
        SDL_JoystickID joystickID = evt.cbutton.which;
        JoystickState& state = joysticks_[joystickID];

        if (button < state.buttons_.size())
        {
            state.buttons_[button] = false;
            g_inputSignals.joystickButtonUp.Emit(joystickID,button);
        }
    }
        break;

    case SDL_CONTROLLERAXISMOTION:
    {
        SDL_JoystickID joystickID = evt.caxis.which;
        JoystickState& state = joysticks_[joystickID];

        float clampedPosition = Clamp((float)evt.caxis.value / 32767.0f, -1.0f, 1.0f);

        if (evt.caxis.axis < state.axes_.size())
        {
            state.axes_[evt.caxis.axis] = clampedPosition;
            g_inputSignals.joystickAxisMove.Emit(joystickID,evt.jaxis.axis,clampedPosition);
        }
    }
        break;

    case SDL_WINDOWEVENT:
    {
        switch (evt.window.event)
        {
        case SDL_WINDOWEVENT_MINIMIZED:
            minimized_ = true;
            SendInputFocusEvent();
            break;

        case SDL_WINDOWEVENT_MAXIMIZED:
        case SDL_WINDOWEVENT_RESTORED:
                minimized_ = false;
                SendInputFocusEvent();
            break;

        case SDL_WINDOWEVENT_RESIZED:
            graphics_->OnWindowResized();
            break;
        case SDL_WINDOWEVENT_MOVED:
            graphics_->OnWindowMoved();
            break;
        default: break;
        }
    }
        break;

    case SDL_DROPFILE:
    {
        QString path = GetInternalPath(QString(evt.drop.file));
        SDL_free(evt.drop.file);
        g_inputSignals.dropFile.Emit(path);
    }
        break;

    case SDL_QUIT:
        g_inputSignals.exitRequested.Emit();
        break;
    default: break;
    }
}

void Input::HandleScreenMode(int Width, int Height, bool Fullscreen, bool Borderless, bool Resizable, bool HighDPI,
                             int Monitor, int RefreshRate)
{
    if (!initialized_)
        Initialize();

    // Re-enable cursor clipping, and re-center the cursor (if needed) to the new screen size, so that there is no erroneous
    // mouse move event. Also get new window ID if it changed
    SDL_Window* window = graphics_->GetWindow();
    windowID_ = SDL_GetWindowID(window);

    if (graphics_->GetFullscreen() || !mouseVisible_)
        focusedThisFrame_ = true;

    // After setting a new screen mode we should not be minimized
    minimized_ = (SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED) != 0;
    // Calculate input coordinate scaling from SDL window to backbuffer ratio
    int winWidth, winHeight;
    int gfxWidth = graphics_->GetWidth();
    int gfxHeight = graphics_->GetHeight();
    SDL_GetWindowSize(window, &winWidth, &winHeight);
    if (winWidth > 0 && winHeight > 0 && gfxWidth > 0 && gfxHeight > 0)
    {
        inputScale_.x_ = (float)gfxWidth / (float)winWidth;
        inputScale_.y_ = (float)gfxHeight / (float)winHeight;
    }
    else
        inputScale_ = Vector2::ONE;
}

void Input::HandleBeginFrame(unsigned frameno,float ts)
{
    // Update input right at the beginning of the frame
    g_inputSignals.inputBegin.Emit();
    Update();
    g_inputSignals.inputEnd.Emit();
}

}
