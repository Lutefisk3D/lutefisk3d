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

#include "../Core/Context.h"
#include "../Core/CoreEvents.h"
#include "../IO/FileSystem.h"
#include "../Graphics/Graphics.h"
#include "../Graphics/GraphicsEvents.h"
#include "../Graphics/GraphicsImpl.h"
#include "../IO/Log.h"
#include "../Core/Mutex.h"
#include "../Core/ProcessUtils.h"
#include "../Core/Profiler.h"
#include "../Resource/ResourceCache.h"
#include "../IO/RWOpsWrapper.h"
#include "../Core/StringUtils.h"
//#include "../UI/Text.h"
#include "../UI/UI.h"

#include <cstring>

#include <SDL2/SDL.h>

extern "C" int SDL_AddTouch(SDL_TouchID touchID, const char *name);

// Use a "click inside window to focus" mechanism on desktop platforms when the mouse cursor is hidden
// TODO: For now, in this particular case only, treat all the ARM on Linux as "desktop" (e.g. RPI, odroid, etc), revisit this again when we support "mobile" ARM on Linux
#if defined(_WIN32) || (defined(__APPLE__) && !defined(IOS)) || (defined(__linux__) && !defined(__ANDROID__))
#define REQUIRE_CLICK_TO_FOCUS
#endif

namespace Urho3D
{

const int SCREEN_JOYSTICK_START_ID = 0x40000000;
const StringHash VAR_BUTTON_KEY_BINDING("VAR_BUTTON_KEY_BINDING");
const StringHash VAR_BUTTON_MOUSE_BUTTON_BINDING("VAR_BUTTON_MOUSE_BUTTON_BINDING");
const StringHash VAR_LAST_KEYSYM("VAR_LAST_KEYSYM");
const StringHash VAR_SCREEN_JOYSTICK_ID("VAR_SCREEN_JOYSTICK_ID");

const unsigned TOUCHID_MAX = 32;

/// Convert SDL keycode if necessary.
int ConvertSDLKeyCode(int keySym, int scanCode)
{
    if (scanCode == SCANCODE_AC_BACK)
        return KEY_ESCAPE;
    else
        return SDL_tolower(keySym);
}

UIElement* TouchState::GetTouchedElement()
{
    return touchedElement_.Get();
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
    Object(context),
    mouseButtonDown_(0),
    mouseButtonPress_(0),
    lastVisibleMousePosition_(MOUSE_POSITION_OFFSCREEN),
    mouseMoveWheel_(0),
    windowID_(0),
    toggleFullscreen_(true),
    mouseVisible_(false),
    lastMouseVisible_(false),
    mouseGrabbed_(false),
    lastMouseGrabbed_(false),
    mouseMode_(MM_ABSOLUTE),
    lastMouseMode_(MM_ABSOLUTE),
    sdlMouseRelative_(false),
    touchEmulation_(false),
    inputFocus_(false),
    minimized_(false),
    focusedThisFrame_(false),
    suppressNextMouseMove_(false),
    mouseMoveScaled_(false),
    initialized_(false)
{
    context_->RequireSDL(SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER);
    for (int i = 0; i < TOUCHID_MAX; i++)
        availableTouchIDs_.push_back(i);

    SubscribeToEvent(E_SCREENMODE, URHO3D_HANDLER(Input, HandleScreenMode));

    // Try to initialize right now, but skip if screen mode is not yet set
    Initialize();
}

Input::~Input()
{
    context_->ReleaseSDL();
}

void Input::Update()
{
    assert(initialized_);

    URHO3D_PROFILE(UpdateInput);

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
        if (inputFocus_ && !graphics_->GetExternalWindow() && (flags & SDL_WINDOW_INPUT_FOCUS) == 0)
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

    if (!touchEmulation_ && (graphics_->GetExternalWindow() || ((!sdlMouseRelative_ && !mouseVisible_ && mouseMode_ != MM_FREE) && inputFocus_ && (flags & SDL_WINDOW_MOUSE_FOCUS))))
    {
        const IntVector2 mousePosition = GetMousePosition();
        mouseMove_ = mousePosition - lastMousePosition_;
        mouseMoveScaled_ = true; // Already in backbuffer scale, since GetMousePosition() operates in that

        if (graphics_->GetExternalWindow())
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
                using namespace MouseMove;

                VariantMap& eventData = GetEventDataMap();

                eventData[P_X] = mousePosition.x_;
                eventData[P_Y] = mousePosition.y_;
                eventData[P_DX] = mouseMove_.x_;
                eventData[P_DY] = mouseMove_.y_;
                eventData[P_BUTTONS] = mouseButtonDown_;
                eventData[P_QUALIFIERS] = GetQualifiers();
                SendEvent(E_MOUSEMOVE, eventData);
            }
        }
    }
    else if (!touchEmulation_ && !mouseVisible_ && sdlMouseRelative_ && inputFocus_ && (flags & SDL_WINDOW_MOUSE_FOCUS))
    {
        // Keep the cursor trapped in window.
        CenterMousePosition();
    }
}

void Input::SetMouseVisible(bool enable, bool suppressEvent)
{
    const bool startMouseVisible = mouseVisible_;

    // In touch emulation mode only enabled mouse is allowed
    if (touchEmulation_)
        enable = true;

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
            if (graphics_->GetExternalWindow())
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
                UI* ui = GetSubsystem<UI>();
                Cursor* cursor = ui->GetCursor();
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
                using namespace MouseVisibleChanged;

                VariantMap& eventData = GetEventDataMap();
                eventData[P_VISIBLE] = mouseVisible_;
                SendEvent(E_MOUSEVISIBLECHANGED, eventData);
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

        UI* const ui = GetSubsystem<UI>();
        Cursor* const cursor = ui->GetCursor();

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
            VariantMap& eventData = GetEventDataMap();
            eventData[MouseModeChanged::P_MODE] = mode;
            eventData[MouseModeChanged::P_MOUSELOCKED] = IsMouseLocked();
            SendEvent(E_MOUSEMODECHANGED, eventData);
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
    if (!keyBindingMap.isEmpty())
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
    if (!mouseButtonBindingMap.isEmpty())
        return;
    mouseButtonBindingMap.emplace("LEFT", SDL_BUTTON_LEFT);
    mouseButtonBindingMap.emplace("MIDDLE", SDL_BUTTON_MIDDLE);
    mouseButtonBindingMap.emplace("RIGHT", SDL_BUTTON_RIGHT);
    mouseButtonBindingMap.emplace("X1", SDL_BUTTON_X1);
    mouseButtonBindingMap.emplace("X2", SDL_BUTTON_X2);
}

void Input::SetScreenKeyboardVisible(bool enable)
{
    if (enable != SDL_IsTextInputActive())
    {
        if (enable)
            SDL_StartTextInput();
        else
            SDL_StopTextInput();
    }
}

void Input::SetTouchEmulation(bool enable)
{
    if (enable != touchEmulation_)
    {
        if (enable)
        {
            // Touch emulation needs the mouse visible
            if (!mouseVisible_)
                SetMouseVisible(true);

            // Add a virtual touch device the first time we are enabling emulated touch
            if (!SDL_GetNumTouchDevices())
                SDL_AddTouch(0, "Emulated Touch");
        }
        else
            ResetTouches();

        touchEmulation_ = enable;
    }
}

bool Input::RecordGesture()
{
    // If have no touch devices, fail
    if (!SDL_GetNumTouchDevices())
    {
        URHO3D_LOGERROR("Can not record gesture: no touch devices");
        return false;
    }

    return SDL_RecordGesture(-1) != 0;
}

bool Input::SaveGestures(Serializer& dest)
{
    RWOpsWrapper<Serializer> wrapper(dest);
    return SDL_SaveAllDollarTemplates(wrapper.GetRWOps()) != 0;
}

bool Input::SaveGesture(Serializer& dest, unsigned gestureID)
{
    RWOpsWrapper<Serializer> wrapper(dest);
    return SDL_SaveDollarTemplate(gestureID, wrapper.GetRWOps()) != 0;
}

unsigned Input::LoadGestures(Deserializer& source)
{
    // If have no touch devices, fail
    if (!SDL_GetNumTouchDevices())
    {
        URHO3D_LOGERROR("Can not load gestures: no touch devices");
        return 0;
    }

    RWOpsWrapper<Deserializer> wrapper(source);
    return (unsigned)SDL_LoadDollarTemplates(-1, wrapper.GetRWOps());
}


bool Input::RemoveGesture(unsigned gestureID)
{
    return false;
}

void Input::RemoveAllGestures()
{
    assert(false);
    //SDL_RemoveAllDollarTemplates();
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
    return QString(SDL_GetKeyName(key));
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

TouchState* Input::GetTouch(unsigned index) const
{
    if (index >= touches_.size())
        return nullptr;

    auto i = touches_.begin();
    while (index--)
        ++i;

    return const_cast<TouchState*>(&MAP_VALUE(i));
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

bool Input::IsScreenJoystickVisible(SDL_JoystickID id) const
{
    assert(false);
    //    auto i = joysticks_.find(id);
    //    return i != joysticks_.end() && MAP_VALUE(i).screenJoystick_ && MAP_VALUE(i).screenJoystick_->IsVisible();
    return false;
}

bool Input::GetScreenKeyboardSupport() const
{
    return SDL_HasScreenKeyboardSupport();
}

bool Input::IsScreenKeyboardVisible() const
{
    return SDL_IsTextInputActive();
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
    Graphics* graphics = GetSubsystem<Graphics>();
    if (!graphics || !graphics->IsInitialized())
        return;

    graphics_ = graphics;

    // In external window mode only visible mouse is supported
    if (graphics_->GetExternalWindow())
        mouseVisible_ = true;

    // Set the initial activation
    initialized_ = true;
    GainFocus();

    ResetJoysticks();
    ResetState();

    SubscribeToEvent(E_BEGINFRAME, URHO3D_HANDLER(Input, HandleBeginFrame));

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

    // Reset touch delta movement
    for(auto i = touches_.begin(), fin = touches_.end(); i != fin; ++i)
    {
        TouchState& state(MAP_VALUE(i));
        state.lastPosition_ = state.position_;
        state.delta_ = IntVector2::ZERO;
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

    ResetTouches();

    // Use SetMouseButton() to reset the state so that mouse events will be sent properly
    SetMouseButton(MOUSEB_LEFT, false);
    SetMouseButton(MOUSEB_RIGHT, false);
    SetMouseButton(MOUSEB_MIDDLE, false);

    mouseMove_ = IntVector2::ZERO;
    mouseMoveWheel_ = 0;
    mouseButtonPress_ = 0;
}

void Input::ResetTouches()
{
    for (auto & elem : touches_)
    {
        TouchState & state(ELEMENT_VALUE(elem));
        using namespace TouchEnd;

        VariantMap& eventData = GetEventDataMap();
        eventData[P_TOUCHID] = state.touchID_;
        eventData[P_X] = state.position_.x_;
        eventData[P_Y] = state.position_.y_;
        SendEvent(E_TOUCHEND, eventData);
    }

    touches_.clear();
    touchIDMap_.clear();
    availableTouchIDs_.clear();
    for (unsigned i = 0; i < TOUCHID_MAX; i++)
        availableTouchIDs_.push_back(i);

}

unsigned Input::GetTouchIndexFromID(unsigned touchID)
{
    auto i = touchIDMap_.find(touchID);
    if (i != touchIDMap_.end())
    {
        return MAP_VALUE(i);
    }

    unsigned index = PopTouchIndex();
    touchIDMap_[touchID] = index;
    return index;
}

unsigned Input::PopTouchIndex()
{
    if (availableTouchIDs_.isEmpty())
        return 0;

    unsigned index = (unsigned)availableTouchIDs_.front();
    availableTouchIDs_.pop_front();
    return index;
}

void Input::PushTouchIndex(unsigned touchID)
{
    if (!touchIDMap_.contains(touchID))
        return;

    unsigned index = touchIDMap_[touchID];
    touchIDMap_.remove(touchID);

    // Sorted insertion
    bool inserted = false;
    for (auto i = availableTouchIDs_.begin(); i != availableTouchIDs_.end(); ++i)
    {
        if (*i == index)
        {
            // This condition can occur when TOUCHID_MAX is reached.
            inserted = true;
            break;
        }

        if (*i > index)
        {
            availableTouchIDs_.insert(i, index);
            inserted = true;
            break;
        }
    }

    // If empty, or the lowest value then insert at end.
    if (!inserted)
        availableTouchIDs_.push_back(index);
}

void Input::SendInputFocusEvent()
{
    using namespace InputFocus;

    VariantMap& eventData = GetEventDataMap();
    eventData[P_FOCUS] = HasFocus();
    eventData[P_MINIMIZED] = IsMinimized();
    SendEvent(E_INPUTFOCUS, eventData);
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

    using namespace MouseButtonDown;

    VariantMap& eventData = GetEventDataMap();
    eventData[P_BUTTON] = button;
    eventData[P_BUTTONS] = mouseButtonDown_;
    eventData[P_QUALIFIERS] = GetQualifiers();
    SendEvent(newState ? E_MOUSEBUTTONDOWN : E_MOUSEBUTTONUP, eventData);
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

    using namespace KeyDown;

    VariantMap& eventData = GetEventDataMap();
    eventData[P_KEY] = key;
    eventData[P_SCANCODE] = scancode;
    eventData[P_BUTTONS] = mouseButtonDown_;
    eventData[P_QUALIFIERS] = GetQualifiers();
    if (newState)
        eventData[P_REPEAT] = repeat;
    SendEvent(newState ? E_KEYDOWN : E_KEYUP, eventData);

    if ((key == KEY_RETURN || key == KEY_RETURN2 || key == KEY_KP_ENTER) && newState && !repeat && toggleFullscreen_ &&
            (GetKeyDown(KEY_LALT) || GetKeyDown(KEY_RALT)))
        graphics_->ToggleFullscreen();
}

void Input::SetMouseWheel(int delta)
{

    if (delta)
    {
        mouseMoveWheel_ += delta;

        using namespace MouseWheel;

        VariantMap& eventData = GetEventDataMap();
        eventData[P_WHEEL] = delta;
        eventData[P_BUTTONS] = mouseButtonDown_;
        eventData[P_QUALIFIERS] = GetQualifiers();
        SendEvent(E_MOUSEWHEEL, eventData);
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
        using namespace SDLRawInput;

        VariantMap eventData = GetEventDataMap();
        eventData[P_SDLEVENT] = &evt;
        eventData[P_CONSUMED] = false;
        SendEvent(E_SDLRAWINPUT, eventData);

        if (eventData[P_CONSUMED].GetBool())
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
            using namespace TextInput;

            VariantMap textInputEventData;

            textInputEventData[P_TEXT] = textInput_;
            SendEvent(E_TEXTINPUT, textInputEventData);
        }
    }
        break;
    case SDL_TEXTEDITING:
        {
            textInput_ = QString::fromUtf8(evt.text.text);
            using namespace TextEditing;

            VariantMap textEditingEventData;
            textEditingEventData[P_COMPOSITION] = textInput_;
            textEditingEventData[P_CURSOR] = evt.edit.start;
            textEditingEventData[P_SELECTION_LENGTH] = evt.edit.length;
            SendEvent(E_TEXTEDITING, textEditingEventData);
        }
        break;
    case SDL_MOUSEBUTTONDOWN:
        if (!touchEmulation_)
            SetMouseButton(1 << (evt.button.button - 1), true);
        else
        {
            int x, y;
            SDL_GetMouseState(&x, &y);
            x = (int)(x * inputScale_.x_);
            y = (int)(y * inputScale_.y_);

            SDL_Event event;
            event.type = SDL_FINGERDOWN;
            event.tfinger.touchId = 0;
            event.tfinger.fingerId = evt.button.button - 1;
            event.tfinger.pressure = 1.0f;
            event.tfinger.x = (float)x / (float)graphics_->GetWidth();
            event.tfinger.y = (float)y / (float)graphics_->GetHeight();
            event.tfinger.dx = 0;
            event.tfinger.dy = 0;
            SDL_PushEvent(&event);
        }
        break;

    case SDL_MOUSEBUTTONUP:
        if (!touchEmulation_)
            SetMouseButton(1 << (evt.button.button - 1), false);
        else
        {
            int x, y;
            SDL_GetMouseState(&x, &y);
            x = (int)(x * inputScale_.x_);
            y = (int)(y * inputScale_.y_);

            SDL_Event event;
            event.type = SDL_FINGERUP;
            event.tfinger.touchId = 0;
            event.tfinger.fingerId = evt.button.button - 1;
            event.tfinger.pressure = 0.0f;
            event.tfinger.x = (float)x / (float)graphics_->GetWidth();
            event.tfinger.y = (float)y / (float)graphics_->GetHeight();
            event.tfinger.dx = 0;
            event.tfinger.dy = 0;
            SDL_PushEvent(&event);
        }
        break;

    case SDL_MOUSEMOTION:
        if ((sdlMouseRelative_ || mouseVisible_ || mouseMode_ == MM_FREE) && !touchEmulation_)
        {
            // Accumulate without scaling for accuracy, needs to be scaled to backbuffer coordinates when asked
            mouseMove_.x_ += evt.motion.xrel;
            mouseMove_.y_ += evt.motion.yrel;
            mouseMoveScaled_ = false;

            if (!suppressNextMouseMove_)
            {
                using namespace MouseMove;

                VariantMap& eventData = GetEventDataMap();
                eventData[P_X] = (int)(evt.motion.x * inputScale_.x_);
                eventData[P_Y] = (int)(evt.motion.y * inputScale_.y_);
                // The "on-the-fly" motion data needs to be scaled now, though this may reduce accuracy
                eventData[P_DX] = (int)(evt.motion.xrel * inputScale_.x_);
                eventData[P_DY] = (int)(evt.motion.yrel * inputScale_.y_);
                eventData[P_BUTTONS] = mouseButtonDown_;
                eventData[P_QUALIFIERS] = GetQualifiers();
                SendEvent(E_MOUSEMOVE, eventData);
            }
        }
        // Only the left mouse button "finger" moves along with the mouse movement
        else if (touchEmulation_ && touches_.contains(0))
        {
            int x, y;
            SDL_GetMouseState(&x, &y);
            x = (int)(x * inputScale_.x_);
            y = (int)(y * inputScale_.y_);

            SDL_Event event;
            event.type = SDL_FINGERMOTION;
            event.tfinger.touchId = 0;
            event.tfinger.fingerId = 0;
            event.tfinger.pressure = 1.0f;
            event.tfinger.x = (float)x / (float)graphics_->GetWidth();
            event.tfinger.y = (float)y / (float)graphics_->GetHeight();
            event.tfinger.dx = (float)evt.motion.xrel * inputScale_.x_ / (float)graphics_->GetWidth();
            event.tfinger.dy = (float)evt.motion.yrel * inputScale_.y_ / (float)graphics_->GetHeight();
            SDL_PushEvent(&event);
        }
        break;

    case SDL_MOUSEWHEEL:
        if (!touchEmulation_)
            SetMouseWheel(evt.wheel.y);
        break;

    case SDL_FINGERDOWN:
        if (evt.tfinger.touchId != SDL_TOUCH_MOUSEID)
        {
            unsigned touchID = GetTouchIndexFromID(evt.tfinger.fingerId & 0x7ffffff);
            TouchState& state = touches_[touchID];
            state.touchID_ = touchID;
            state.lastPosition_ = state.position_ = IntVector2((int)(evt.tfinger.x * graphics_->GetWidth()),
                (int)(evt.tfinger.y * graphics_->GetHeight()));
            state.delta_ = IntVector2::ZERO;
            state.pressure_ = evt.tfinger.pressure;

            using namespace TouchBegin;

            VariantMap& eventData = GetEventDataMap();
            eventData[P_TOUCHID] = touchID;
            eventData[P_X] = state.position_.x_;
            eventData[P_Y] = state.position_.y_;
            eventData[P_PRESSURE] = state.pressure_;
            SendEvent(E_TOUCHBEGIN, eventData);

            // Finger touch may move the mouse cursor. Suppress next mouse move when cursor hidden to prevent jumps
            if (!mouseVisible_)
                SuppressNextMouseMove();
        }
        break;

    case SDL_FINGERUP:
        if (evt.tfinger.touchId != SDL_TOUCH_MOUSEID)
        {
            unsigned touchID = GetTouchIndexFromID(evt.tfinger.fingerId & 0x7ffffff);
            TouchState& state = touches_[touchID];

            using namespace TouchEnd;

            VariantMap& eventData = GetEventDataMap();
            // Do not trust the position in the finger up event. Instead use the last position stored in the
            // touch structure
            eventData[P_TOUCHID] = touchID;
            eventData[P_X] = state.position_.x_;
            eventData[P_Y] = state.position_.y_;
            SendEvent(E_TOUCHEND, eventData);

            // Add touch index back to list of available touch Ids
            PushTouchIndex(evt.tfinger.fingerId & 0x7ffffff);

            touches_.remove(touchID);
        }
        break;

    case SDL_FINGERMOTION:
        if (evt.tfinger.touchId != SDL_TOUCH_MOUSEID)
        {
            unsigned touchID = GetTouchIndexFromID(evt.tfinger.fingerId & 0x7ffffff);
            // We don't want this event to create a new touches_ event if it doesn't exist (touchEmulation)
            if (touchEmulation_ && !touches_.contains(touchID))
                break;
            TouchState& state = touches_[touchID];
            state.touchID_ = touchID;
            state.position_ = IntVector2((int)(evt.tfinger.x * graphics_->GetWidth()),
                (int)(evt.tfinger.y * graphics_->GetHeight()));
            state.delta_ = state.position_ - state.lastPosition_;
            state.pressure_ = evt.tfinger.pressure;

            using namespace TouchMove;

            VariantMap& eventData = GetEventDataMap();
            eventData[P_TOUCHID] = touchID;
            eventData[P_X] = state.position_.x_;
            eventData[P_Y] = state.position_.y_;
            eventData[P_DX] = (int)(evt.tfinger.dx * graphics_->GetWidth());
            eventData[P_DY] = (int)(evt.tfinger.dy * graphics_->GetHeight());
            eventData[P_PRESSURE] = state.pressure_;
            SendEvent(E_TOUCHMOVE, eventData);

            // Finger touch may move the mouse cursor. Suppress next mouse move when cursor hidden to prevent jumps
            if (!mouseVisible_)
                SuppressNextMouseMove();
        }
        break;

    case SDL_DOLLARRECORD:
    {
        using namespace GestureRecorded;

        VariantMap& eventData = GetEventDataMap();
        eventData[P_GESTUREID] = (int)evt.dgesture.gestureId;
        SendEvent(E_GESTURERECORDED, eventData);
    }
        break;

    case SDL_DOLLARGESTURE:
    {
        using namespace GestureInput;

        VariantMap& eventData = GetEventDataMap();
        eventData[P_GESTUREID] = (int)evt.dgesture.gestureId;
        eventData[P_CENTERX] = (int)(evt.dgesture.x * graphics_->GetWidth());
        eventData[P_CENTERY] = (int)(evt.dgesture.y * graphics_->GetHeight());
        eventData[P_NUMFINGERS] = (int)evt.dgesture.numFingers;
        eventData[P_ERROR] = evt.dgesture.error;
        SendEvent(E_GESTUREINPUT, eventData);
    }
        break;

    case SDL_MULTIGESTURE:
    {
        using namespace MultiGesture;

        VariantMap& eventData = GetEventDataMap();
        eventData[P_CENTERX] = (int)(evt.mgesture.x * graphics_->GetWidth());
        eventData[P_CENTERY] = (int)(evt.mgesture.y * graphics_->GetHeight());
        eventData[P_NUMFINGERS] = (int)evt.mgesture.numFingers;
        eventData[P_DTHETA] = M_RADTODEG * evt.mgesture.dTheta;
        eventData[P_DDIST] = evt.mgesture.dDist;
        SendEvent(E_MULTIGESTURE, eventData);
    }
        break;

    case SDL_JOYDEVICEADDED:
    {
        using namespace JoystickConnected;

            SDL_JoystickID joystickID = OpenJoystick((unsigned)evt.jdevice.which);

        VariantMap& eventData = GetEventDataMap();
        eventData[P_JOYSTICKID] = joystickID;
        SendEvent(E_JOYSTICKCONNECTED, eventData);
    }
        break;

    case SDL_JOYDEVICEREMOVED:
    {
        using namespace JoystickDisconnected;

        joysticks_.remove(evt.jdevice.which);

        VariantMap& eventData = GetEventDataMap();
        eventData[P_JOYSTICKID] = evt.jdevice.which;
        SendEvent(E_JOYSTICKDISCONNECTED, eventData);
    }
        break;

    case SDL_JOYBUTTONDOWN:
    {
        using namespace JoystickButtonDown;

        unsigned button = evt.jbutton.button;
        SDL_JoystickID joystickID = evt.jbutton.which;
        JoystickState& state = joysticks_[joystickID];

        // Skip ordinary joystick event for a controller
        if (!state.controller_)
        {
            VariantMap& eventData = GetEventDataMap();
            eventData[P_JOYSTICKID] = joystickID;
            eventData[P_BUTTON] = button;

            if (button < state.buttons_.size())
            {
                state.buttons_[button] = true;
                state.buttonPress_[button] = true;
                SendEvent(E_JOYSTICKBUTTONDOWN, eventData);
            }
        }
    }
        break;

    case SDL_JOYBUTTONUP:
    {
        using namespace JoystickButtonUp;

        unsigned button = evt.jbutton.button;
        SDL_JoystickID joystickID = evt.jbutton.which;
        JoystickState& state = joysticks_[joystickID];

        if (!state.controller_)
        {
            VariantMap& eventData = GetEventDataMap();
            eventData[P_JOYSTICKID] = joystickID;
            eventData[P_BUTTON] = button;

            if (button < state.buttons_.size())
            {
                if (!state.controller_)
                    state.buttons_[button] = false;
                SendEvent(E_JOYSTICKBUTTONUP, eventData);
            }
        }
    }
        break;

    case SDL_JOYAXISMOTION:
    {
        using namespace JoystickAxisMove;

        SDL_JoystickID joystickID = evt.jaxis.which;
        JoystickState& state = joysticks_[joystickID];

        if (!state.controller_)
        {
            VariantMap& eventData = GetEventDataMap();
            eventData[P_JOYSTICKID] = joystickID;
            eventData[P_AXIS] = evt.jaxis.axis;
            eventData[P_POSITION] = Clamp((float)evt.jaxis.value / 32767.0f, -1.0f, 1.0f);

            if (evt.jaxis.axis < state.axes_.size())
            {
                // If the joystick is a controller, only use the controller axis mappings
                // (we'll also get the controller event)
                if (!state.controller_)
                    state.axes_[evt.jaxis.axis] = eventData[P_POSITION].GetFloat();
                SendEvent(E_JOYSTICKAXISMOVE, eventData);
            }
        }
    }
        break;

    case SDL_JOYHATMOTION:
    {
        using namespace JoystickHatMove;

        SDL_JoystickID joystickID = evt.jaxis.which;
        JoystickState& state = joysticks_[joystickID];

        VariantMap& eventData = GetEventDataMap();
        eventData[P_JOYSTICKID] = joystickID;
        eventData[P_HAT] = evt.jhat.hat;
        eventData[P_POSITION] = evt.jhat.value;

        if (evt.jhat.hat < state.hats_.size())
        {
            state.hats_[evt.jhat.hat] = evt.jhat.value;
            SendEvent(E_JOYSTICKHATMOVE, eventData);
        }
    }
        break;

    case SDL_CONTROLLERBUTTONDOWN:
    {
        using namespace JoystickButtonDown;

        unsigned button = evt.cbutton.button;
        SDL_JoystickID joystickID = evt.cbutton.which;
        JoystickState& state = joysticks_[joystickID];

        VariantMap& eventData = GetEventDataMap();
        eventData[P_JOYSTICKID] = joystickID;
        eventData[P_BUTTON] = button;

        if (button < state.buttons_.size())
        {
            state.buttons_[button] = true;
            state.buttonPress_[button] = true;
            SendEvent(E_JOYSTICKBUTTONDOWN, eventData);
        }
    }
        break;

    case SDL_CONTROLLERBUTTONUP:
    {
        using namespace JoystickButtonUp;

        unsigned button = evt.cbutton.button;
        SDL_JoystickID joystickID = evt.cbutton.which;
        JoystickState& state = joysticks_[joystickID];

        VariantMap& eventData = GetEventDataMap();
        eventData[P_JOYSTICKID] = joystickID;
        eventData[P_BUTTON] = button;

        if (button < state.buttons_.size())
        {
            state.buttons_[button] = false;
            SendEvent(E_JOYSTICKBUTTONUP, eventData);
        }
    }
        break;

    case SDL_CONTROLLERAXISMOTION:
    {
        using namespace JoystickAxisMove;

        SDL_JoystickID joystickID = evt.caxis.which;
        JoystickState& state = joysticks_[joystickID];

        VariantMap& eventData = GetEventDataMap();
        eventData[P_JOYSTICKID] = joystickID;
        eventData[P_AXIS] = evt.caxis.axis;
        eventData[P_POSITION] = Clamp((float)evt.caxis.value / 32767.0f, -1.0f, 1.0f);

        if (evt.caxis.axis < state.axes_.size())
        {
            state.axes_[evt.caxis.axis] = eventData[P_POSITION].GetFloat();
            SendEvent(E_JOYSTICKAXISMOVE, eventData);
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
        using namespace DropFile;

        VariantMap& eventData = GetEventDataMap();
        eventData[P_FILENAME] = GetInternalPath(QString(evt.drop.file));
        SDL_free(evt.drop.file);

        SendEvent(E_DROPFILE, eventData);
    }
        break;

    case SDL_QUIT:
        SendEvent(E_EXITREQUESTED);
        break;
    default: break;
    }
}

void Input::HandleScreenMode(StringHash eventType, VariantMap& eventData)
{
    if (!initialized_)
        Initialize();

    // Re-enable cursor clipping, and re-center the cursor (if needed) to the new screen size, so that there is no erroneous
    // mouse move event. Also get new window ID if it changed
    SDL_Window* window = graphics_->GetWindow();
    windowID_ = SDL_GetWindowID(window);


    // Resize screen joysticks to new screen size
    for (auto i : joysticks_)
    {
        UIElement* screenjoystick = ELEMENT_VALUE(i).screenJoystick_;
        if (screenjoystick) {
            assert(false);
            //screenjoystick->SetSize(graphics_->GetWidth(), graphics_->GetHeight());
        }
    }

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

void Input::HandleBeginFrame(StringHash eventType, VariantMap& eventData)
{
    // Update input right at the beginning of the frame
    SendEvent(E_INPUTBEGIN);
    Update();
    SendEvent(E_INPUTEND);
}

void Input::HandleScreenJoystickTouch(StringHash eventType, VariantMap& eventData)
{
    assert(!"On screen joystick support removed");
#if 0
    using namespace TouchBegin;

    // Only interested in events from screen joystick(s)
    TouchState& state = touches_[eventData[P_TOUCHID].GetInt()];
    IntVector2 position(int(state.position_.x_ / GetSubsystem<UI>()->GetScale()), int(state.position_.y_ / GetSubsystem<UI>()->GetScale()));
    UIElement* element = eventType == E_TOUCHBEGIN ? GetSubsystem<UI>()->GetElementAt(position) : state.touchedElement_;
    if (!element)
        return;
    Variant variant = element->GetVar(VAR_SCREEN_JOYSTICK_ID);
    if (variant.IsEmpty())
        return;
    SDL_JoystickID joystickID = variant.GetInt();

    if (eventType == E_TOUCHEND)
        state.touchedElement_.Reset();
    else
        state.touchedElement_ = element;

    // Prepare a fake SDL event
    SDL_Event evt;

    const QString& name = element->GetName();
    if (name.startsWith("Button"))
    {
        if (eventType == E_TOUCHMOVE)
            return;

        // Determine whether to inject a joystick event or keyboard/mouse event
        Variant keyBindingVar = element->GetVar(VAR_BUTTON_KEY_BINDING);
        Variant mouseButtonBindingVar = element->GetVar(VAR_BUTTON_MOUSE_BUTTON_BINDING);
        if (keyBindingVar.IsEmpty() && mouseButtonBindingVar.IsEmpty())
        {
            evt.type = eventType == E_TOUCHBEGIN ? SDL_JOYBUTTONDOWN : SDL_JOYBUTTONUP;
            evt.jbutton.which = joystickID;
            evt.jbutton.button = (Uint8)name.midRef(0,6).toUInt();
        }
        else
        {
            if (!keyBindingVar.IsEmpty())
            {
                evt.type = eventType == E_TOUCHBEGIN ? SDL_KEYDOWN : SDL_KEYUP;
                evt.key.keysym.sym = ToLower(keyBindingVar.GetInt());
                evt.key.keysym.scancode = SDL_SCANCODE_UNKNOWN;
            }
            if (!mouseButtonBindingVar.IsEmpty())
            {
                // Mouse button are sent as extra events besides key events
                // Disable touch emulation handling during this to prevent endless loop
                bool oldTouchEmulation = touchEmulation_;
                touchEmulation_ = false;

                SDL_Event mouseEvent;
                mouseEvent.type = eventType == E_TOUCHBEGIN ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
                mouseEvent.button.button = (Uint8)mouseButtonBindingVar.GetInt();
                HandleSDLEvent(&mouseEvent);

                touchEmulation_ = oldTouchEmulation;
            }
        }
    }
    else if (name.StartsWith("Hat"))
    {
        Variant keyBindingVar = element->GetVar(VAR_BUTTON_KEY_BINDING);
        if (keyBindingVar.IsEmpty())
        {
            evt.type = SDL_JOYHATMOTION;
            evt.jaxis.which = joystickID;
            evt.jhat.hat = (Uint8)ToUInt(name.Substring(3));
            evt.jhat.value = HAT_CENTER;
            if (eventType != E_TOUCHEND)
            {
                IntVector2 relPosition = position - element->GetScreenPosition() - element->GetSize() / 2;
                if (relPosition.y_ < 0 && Abs(relPosition.x_ * 3 / 2) < Abs(relPosition.y_))
                    evt.jhat.value |= HAT_UP;
                if (relPosition.y_ > 0 && Abs(relPosition.x_ * 3 / 2) < Abs(relPosition.y_))
                    evt.jhat.value |= HAT_DOWN;
                if (relPosition.x_ < 0 && Abs(relPosition.y_ * 3 / 2) < Abs(relPosition.x_))
                    evt.jhat.value |= HAT_LEFT;
                if (relPosition.x_ > 0 && Abs(relPosition.y_ * 3 / 2) < Abs(relPosition.x_))
                    evt.jhat.value |= HAT_RIGHT;
            }
        }
        else
        {
            // Hat is binded by 4 integers representing keysyms for 'w', 's', 'a', 'd' or something similar
            IntRect keyBinding = keyBindingVar.GetIntRect();

            if (eventType == E_TOUCHEND)
            {
                evt.type = SDL_KEYUP;
                evt.key.keysym.sym = element->GetVar(VAR_LAST_KEYSYM).GetInt();
                if (!evt.key.keysym.sym)
                    return;

                element->SetVar(VAR_LAST_KEYSYM, 0);
            }
            else
            {
                evt.type = SDL_KEYDOWN;
                IntVector2 relPosition = position - element->GetScreenPosition() - element->GetSize() / 2;
                if (relPosition.y_ < 0 && Abs(relPosition.x_ * 3 / 2) < Abs(relPosition.y_))
                    evt.key.keysym.sym = keyBinding.left_;      // The integers are encoded in WSAD order to l-t-r-b
                else if (relPosition.y_ > 0 && Abs(relPosition.x_ * 3 / 2) < Abs(relPosition.y_))
                    evt.key.keysym.sym = keyBinding.top_;
                else if (relPosition.x_ < 0 && Abs(relPosition.y_ * 3 / 2) < Abs(relPosition.x_))
                    evt.key.keysym.sym = keyBinding.right_;
                else if (relPosition.x_ > 0 && Abs(relPosition.y_ * 3 / 2) < Abs(relPosition.x_))
                    evt.key.keysym.sym = keyBinding.bottom_;
                else
                    return;

                if (eventType == E_TOUCHMOVE && evt.key.keysym.sym != element->GetVar(VAR_LAST_KEYSYM).GetInt())
                {
                    // Dragging past the directional boundary will cause an additional key up event for previous key symbol
                    SDL_Event keyEvent;
                    keyEvent.type = SDL_KEYUP;
                    keyEvent.key.keysym.sym = element->GetVar(VAR_LAST_KEYSYM).GetInt();
                    if (keyEvent.key.keysym.sym)
                    {
                        keyEvent.key.keysym.scancode = SDL_SCANCODE_UNKNOWN;
                        HandleSDLEvent(&keyEvent);
                    }

                    element->SetVar(VAR_LAST_KEYSYM, 0);
                }

                evt.key.keysym.scancode = SDL_SCANCODE_UNKNOWN;

                element->SetVar(VAR_LAST_KEYSYM, evt.key.keysym.sym);
            }
        }
    }
    else
        return;

    // Handle the fake SDL event to turn it into Urho3D genuine event
    HandleSDLEvent(&evt);
#endif
}

}
