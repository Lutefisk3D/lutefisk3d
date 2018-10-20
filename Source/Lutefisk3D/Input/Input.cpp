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
#ifdef LUTEFISK3D_UI
#include "Lutefisk3D/UI/UI.h"
#endif
#include <cstring>
#include <QtCore/QDebug>
#include <GLFW/glfw3.h>
// Use a "click inside window to focus" mechanism on desktop platforms when the mouse cursor is hidden
// TODO: For now, in this particular case only, treat all the ARM on Linux as "desktop" (e.g. RPI, odroid, etc), revisit this again when we support "mobile" ARM on Linux
#if defined(_WIN32) || (defined(__APPLE__)) || (defined(__linux__))
#define REQUIRE_CLICK_TO_FOCUS
#endif

namespace Urho3D
{
namespace
{
} // end of anonymous namespace
const StringHash VAR_BUTTON_KEY_BINDING("VAR_BUTTON_KEY_BINDING");
const StringHash VAR_BUTTON_MOUSE_BUTTON_BINDING("VAR_BUTTON_MOUSE_BUTTON_BINDING");
const StringHash VAR_LAST_KEYSYM("VAR_LAST_KEYSYM");

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
        hats_[i] = HatPosition::CENTERED;
}

Input::Input(Context* context) :
    SignalObserver(context->observerAllocator()),
    m_context(context),
    mouseButtonDown_(0),
    mouseButtonPress_(0),
    lastVisibleMousePosition_(MOUSE_POSITION_OFFSCREEN),
    mouseMoveWheel_(0),
    inputScale_(Vector2::ONE),
    toggleFullscreen_(true),
    mouseVisible_(false),
    lastMouseVisible_(false),
    mouseGrabbed_(false),
    lastMouseGrabbed_(false),
    mouseMode_(MM_FREE),
    lastMouseMode_(MM_ABSOLUTE),
    inputFocus_(false),
    minimized_(false),
    focusedThisFrame_(false),
    suppressNextMouseMove_(false),
    mouseMoveScaled_(false),
    initialized_(false)
{
    g_graphicsSignals.newScreenMode.Connect(this,&Input::HandleScreenMode);

    // Try to initialize right now, but skip if screen mode is not yet set
    Initialize();
}

Input::~Input()
{
}
static void onGlfwFocus(GLFWwindow *w,int focus)
{
    if(focus==GLFW_TRUE)
    {

    }
}
static void onGlfwKey(GLFWwindow *w,int key,int scancode,int action,int modbits)
{
    GLFW_KEY_UNKNOWN;
    Input *inp = static_cast<Input *>(glfwGetWindowUserPointer(w));
    if(!inp)
        return;
    if(action==GLFW_PRESS || action==GLFW_RELEASE)
        inp->SetKey(key, scancode, action==GLFW_PRESS);

}
static void onGlfwChar(GLFWwindow *w,unsigned codep)
{
    g_inputSignals.textInput(QChar(codep));
}
static void onGlfwMouseButton(GLFWwindow *w,int button,int action,int mods)
{
    Input *inp = static_cast<Input *>(glfwGetWindowUserPointer(w));
    if(!inp)
        return;
    inp->SetMouseButton(MouseButton(1<<button),action==GLFW_PRESS);
}
void Input::mouseMovedInWindow(GLFWwindow *w,double x,double y)
{
    Input *inp = static_cast<Input *>(glfwGetWindowUserPointer(w));
    if(!inp)
        return;
    inp->mousePosLastUpdate_ = {static_cast<float>(x),static_cast<float>(y)};
//    if ((inp->IsMouseVisible() || inp->GetMouseMode() == MM_FREE))
//    {
        inp->lastMousePosition_.x_ = x;
        inp->lastMousePosition_.y_ = y;
        inp->mouseMoveScaled_ = false;

        if (!inp->suppressNextMouseMove_)
        {
            Vector2 deltafrombase = inp->mousePosLastUpdate_-inp->mouseMove_;
            g_inputSignals.mouseMove(
                        (int)(x * inp->inputScale_.x_),(int)(y * inp->inputScale_.y_),
                        // The "on-the-fly" motion data needs to be scaled now, though this may reduce accuracy
                        (int)(deltafrombase.x_ * inp->inputScale_.x_),(int)(deltafrombase.y_ * inp->inputScale_.y_),
                        inp->mouseButtonDown_,inp->GetQualifiers()
                        );
        }
//    }
}

static void mouseScrolledInWindow(GLFWwindow *w, double xoffset, double yoffset)
{
    Input *inp = static_cast<Input *>(glfwGetWindowUserPointer(w));
    if(!inp)
        return;
    inp->SetMouseWheel(xoffset);

}
static void joysticConfigurationChanged(int joyId,int state)
{
    if(state==GLFW_CONNECTED)
        g_inputSignals.joystickConnected(joyId);
    if(state==GLFW_DISCONNECTED)
        g_inputSignals.joystickDisconnected(joyId);
}
void Input::updateJostickStates()
{
    for(int joy_id = GLFW_JOYSTICK_1; joy_id<GLFW_JOYSTICK_LAST; ++joy_id)
    {
        if( 0==glfwJoystickPresent(joy_id) )
            continue;
        JoystickState& state(joysticks_[joy_id]);
        int button_count = 0;
        const uint8_t *states=glfwGetJoystickButtons(joy_id,&button_count);
        if(!states)
            continue;
        if(state.buttons_.size()!=button_count)
        {
            state.buttons_.resize(button_count);
            state.buttonPress_.resize(button_count);
        }
        for(unsigned button=0; button<button_count; ++button)
        {
            if(state.buttons_[button] != bool(states[button]==GLFW_PRESS))
            {
                if(states[button]==GLFW_PRESS)
                {
                    state.buttons_[button] = true;
                    state.buttonPress_[button] = true;
                    g_inputSignals.joystickButtonDown(joy_id,button);
                }
                else
                {
                    state.buttons_[button] = false;
                    g_inputSignals.joystickButtonUp(joy_id,button);
                }
            }
        }
        int axis_count = 0;
        const float *axis_states = glfwGetJoystickAxes(joy_id,&axis_count);
        if(axis_count>=0 && state.axes_.size()!=axis_count)
            state.axes_.resize(axis_count);
        for(unsigned axis=0; axis<axis_count; ++axis)
        {
            if(state.axes_[axis]!=axis_states[axis])
            {
                state.axes_[axis] = axis_states[axis];
                g_inputSignals.joystickAxisMove(joy_id,axis,axis_states[axis]);
            }
        }
        //TODO: waiting for GLFW 3.3 to properly handle joystick Hat events.
        //  if (evt.jhat.hat < state.hats_.size())
        //  {
        //  state.hats_[evt.jhat.hat] = evt.jhat.value;
        //  g_inputSignals.joystickHatMove(joystickID,evt.jhat.hat,evt.jhat.value);
        //  }
    }
}
void Input::iconificationChanged(GLFWwindow *w,int state)
{
    Input *inp = static_cast<Input *>(glfwGetWindowUserPointer(w));
    if(!inp)
        return;
    if(GLFW_TRUE==state)
    {
        inp->minimized_=true;
        inp->SendInputFocusEvent();
    }
    else
    {
        inp->minimized_=false;
        inp->SendInputFocusEvent();
    }
}
void Input::windowMoved(GLFWwindow *w,int x,int y)
{
    Input *inp = static_cast<Input *>(glfwGetWindowUserPointer(w));
    if(!inp)
        return;
    inp->graphics_->OnWindowMoved();
}
void Input::windowResized(GLFWwindow *w,int width,int h)
{
    Input *inp = static_cast<Input *>(glfwGetWindowUserPointer(w));
    if(!inp)
        return;
    inp->graphics_->OnWindowResized();

}
//TODO: handle per-window drop
static void onFileDropped(GLFWwindow *w,int count,const char **names)
{
    for(int i=0; i<count; ++i)
    {
        QString path = GetInternalPath(QString(names[i]));
        g_inputSignals.dropFile(path);
    }
}
//TODO: handle per-window close
static void onWindowClosed(GLFWwindow *w)
{
    g_inputSignals.exitRequested();
}
void Input::Update()
{
    assert(initialized_);

    URHO3D_PROFILE(UpdateInput);

    bool mouseMoved = false;
    if (mouseMove_ != mousePosLastUpdate_)
        mouseMoved = true;

    // Check for focus change this frame
    GLFWwindow* window = graphics_->GetWindow();
    ResetInputAccumulation();
    if(window && !glfwGetWindowUserPointer(window))
    {
        glfwSetWindowUserPointer(window,this);
        glfwSetWindowFocusCallback(window,onGlfwFocus);
        glfwSetKeyCallback(window,onGlfwKey);
        glfwSetCharCallback(window,onGlfwChar);
        glfwSetMouseButtonCallback(window,onGlfwMouseButton);
        glfwSetCursorPosCallback(window,mouseMovedInWindow);
        glfwSetScrollCallback(window,&mouseScrolledInWindow);
        glfwSetJoystickCallback(joysticConfigurationChanged);
        glfwSetWindowIconifyCallback(window,iconificationChanged);
        glfwSetWindowPosCallback(window,windowMoved);
        glfwSetFramebufferSizeCallback(window,windowResized);
        glfwSetDropCallback(window,onFileDropped);
        glfwSetWindowCloseCallback(window,onWindowClosed);

    }
    glfwPollEvents();
    updateJostickStates();

    if (suppressNextMouseMove_ && (mouseMove_ != mousePosLastUpdate_ || mouseMoved))
        UnsuppressMouseMove();
    window = graphics_->GetWindow();
    bool flags = window ? glfwGetWindowAttrib(window,GLFW_FOCUSED) : 0;
    if (window)
    {
#ifdef REQUIRE_CLICK_TO_FOCUS
        // When using the "click to focus" mechanism, only focus automatically in fullscreen or non-hidden mouse mode
        if (!inputFocus_ && ((mouseVisible_ || mouseMode_ == MM_FREE) || graphics_->GetFullscreen()) && flags)
#else
        if (!inputFocus_ && flags)
#endif
            focusedThisFrame_ = true;

        if (focusedThisFrame_)
            GainFocus();

        // Check for losing focus. The window flags are not reliable when using an external window, so prevent losing focus in that case
        if (inputFocus_ && !flags)
            LoseFocus();
    }
    else
        return;

    if (graphics_->WeAreEmbedded())
    {
        const IntVector2 mousePosition = GetMousePosition();
        mouseMove_ = Vector2(mousePosition - lastMousePosition_);
        mouseMoveScaled_ = true; // Already in backbuffer scale, since GetMousePosition() operates in that

        if (graphics_->WeAreEmbedded())
            lastMousePosition_ = mousePosition;
        else
        {
            // Recenter the mouse cursor manually after move
            CenterMousePosition();
        }
        // Send mouse move event if necessary
        if (mouseMove_ != mousePosLastUpdate_)
        {
            if (!suppressNextMouseMove_)
            {
                g_inputSignals.mouseMove(mousePosition.x_, mousePosition.y_, mouseMove_.x_, mouseMove_.y_,
                                              mouseButtonDown_, GetQualifiers());
            }
        }
    }
}

void Input::SetMouseVisible(bool enable, bool suppressEvent)
{
    const bool startMouseVisible = mouseVisible_;
    GLFWwindow* window = graphics_->GetWindow();

    // In mouse mode relative, the mouse should be invisible
    if (mouseMode_ == MM_RELATIVE)
    {
        if (!suppressEvent)
            lastMouseVisible_ = enable;

        enable = false;
    }

    if (enable == mouseVisible_)
        return;
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
            glfwSetInputMode(window, GLFW_CURSOR,
                             mouseMode_ == MM_RELATIVE ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_HIDDEN);
            mouseVisible_ = false;
        }
        else if (mouseMode_ != MM_RELATIVE)
        {
            glfwSetInputMode(window,GLFW_CURSOR,GLFW_CURSOR_NORMAL);

            mouseVisible_ = true;

            // Update cursor position
#ifdef LUTEFISK3D_UI
            Cursor *cursor = m_context->m_UISystem->GetCursor();
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
#endif
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
            g_inputSignals.mouseVisibleChanged(mouseVisible_);

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
            GLFWwindow* const window = graphics_->GetWindow();
#ifdef LUTEFISK3D_UI
            Cursor* const cursor = m_context->m_UISystem->GetCursor();
#endif
            // Handle changing from previous mode
            if (previousMode == MM_ABSOLUTE)
            {
                if (!mouseVisible_ && window)
                {
                    glfwSetInputMode(window,GLFW_CURSOR,GLFW_CURSOR_NORMAL);
                }
            }
            if (previousMode == MM_RELATIVE)
            {
                qDebug() << "Unhandled mouse  mode switch";
                //SetMouseModeRelative(SDL_FALSE);
                ResetMouseVisible();
            }

            // Handle changing to new mode
            if (mode == MM_ABSOLUTE && window)
            {
                if (!mouseVisible_)
                    glfwSetInputMode(window,GLFW_CURSOR,GLFW_CURSOR_DISABLED);
                else
                    glfwSetInputMode(window,GLFW_CURSOR,GLFW_CURSOR_NORMAL);

            }
            else if (mode == MM_RELATIVE && window)
            {
                SetMouseVisible(false, true);
                glfwSetInputMode(window,GLFW_CURSOR,GLFW_CURSOR_DISABLED);
                //qDebug() << "Unhandled mouse mode switch";
                //SetMouseModeRelative(SDL_TRUE);
            }
            else if (mode == MM_FREE && window)
            {
                if (!mouseVisible_)
                    glfwSetInputMode(window,GLFW_CURSOR,GLFW_CURSOR_HIDDEN);
                else
                    glfwSetInputMode(window,GLFW_CURSOR,GLFW_CURSOR_NORMAL);
            }
#ifdef LUTEFISK3D_UI
            SetMouseGrabbed(!(mouseVisible_ || (cursor && cursor->IsVisible())), suppressEvent);
#else
            SetMouseGrabbed(!mouseVisible_, suppressEvent);
#endif
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
            g_inputSignals.mouseModeChanged(mode,IsMouseLocked());
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
    keyBindingMap.emplace("LCTRL", KEY_LEFT_CONTROL);
    keyBindingMap.emplace("RCTRL", KEY_RIGHT_CONTROL);
    keyBindingMap.emplace("LSHIFT", KEY_LEFT_SHIFT);
    keyBindingMap.emplace("RSHIFT", KEY_RIGHT_SHIFT);
    keyBindingMap.emplace("LALT", KEY_LEFT_ALT);
    keyBindingMap.emplace("RALT", KEY_RIGHT_ALT);
    keyBindingMap.emplace("LGUI", KEY_LEFT_SUPER);
    keyBindingMap.emplace("RGUI", KEY_RIGHT_SUPER);
    keyBindingMap.emplace("TAB", KEY_TAB);
    keyBindingMap.emplace("RETURN", KEY_ENTER);
    keyBindingMap.emplace("ENTER", KEY_KP_ENTER);
    keyBindingMap.emplace("LEFT", KEY_LEFT);
    keyBindingMap.emplace("RIGHT", KEY_RIGHT);
    keyBindingMap.emplace("UP", KEY_UP);
    keyBindingMap.emplace("DOWN", KEY_DOWN);
    keyBindingMap.emplace("PAGEUP", KEY_PAGE_UP);
    keyBindingMap.emplace("PAGEDOWN", KEY_PAGE_DOWN);
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
    mouseButtonBindingMap.emplace("LEFT", GLFW_MOUSE_BUTTON_LEFT);
    mouseButtonBindingMap.emplace("MIDDLE", GLFW_MOUSE_BUTTON_MIDDLE);
    mouseButtonBindingMap.emplace("RIGHT", GLFW_MOUSE_BUTTON_RIGHT);
    mouseButtonBindingMap.emplace("X1", GLFW_MOUSE_BUTTON_4);
    mouseButtonBindingMap.emplace("X2", GLFW_MOUSE_BUTTON_5);
}

int Input::OpenJoystick(unsigned index)
{
    int joy_exists = glfwJoystickPresent(index);
    if (!joy_exists)
    {
        URHO3D_LOGERROR(QString("Cannot open joystick #%1").arg(index) );
        return -1;
    }

    // Create joystick state for the new joystick
    JoystickState& state = joysticks_[index];
    state.joystickID_ = index;

    state.name_ = glfwGetJoystickName(index);
    //    if (SDL_IsGameController(index))
    //        state.controller_ = SDL_GameControllerOpen(index);

    int numButtons=0;
    int numAxes = 0;
    int numHats = 0;
    glfwGetJoystickButtons(index,&numButtons);
    glfwGetJoystickAxes(index,&numAxes);
    //TODO: waiting for glfw 3.3 glfwGetJoystickHats

    // When the joystick is a controller, make sure there's enough axes & buttons for the standard controller mappings

    state.Initialize(numButtons, numAxes, numHats);

    return index;
}

QString Input::GetKeyName(int key) const
{

    return glfwGetKeyName(key,0);
}

QString Input::GetScancodeName(int scancode) const
{
    return glfwGetKeyName(GLFW_KEY_UNKNOWN,scancode);
}

bool Input::GetKeyDown(int key) const
{
    return keyDown_.contains(key);
}

bool Input::GetKeyPress(int key) const
{
    return keyPress_.contains(key);
}

bool Input::GetScancodeDown(int scancode) const
{
    return scancodeDown_.contains(scancode);
}

bool Input::GetScancodePress(int scancode) const
{
    return scancodePress_.contains(scancode);
}

bool Input::GetMouseButtonDown(MouseButtonFlags button) const
{
    return mouseButtonDown_ & button;
}

bool Input::GetMouseButtonPress(MouseButtonFlags button) const
{
    return (mouseButtonPress_ & button);
}
bool Input::GetMouseButtonClick(MouseButtonFlags button) const
{
    return mouseButtonClick_ & button;
}

bool Input::GetQualifierDown(Qualifier qualifier) const
{
    if (qualifier == QUAL_SHIFT)
        return GetKeyDown(KEY_LEFT_SHIFT) || GetKeyDown(KEY_RIGHT_SHIFT);
    if (qualifier == QUAL_CTRL)
        return GetKeyDown(KEY_LEFT_CONTROL) || GetKeyDown(KEY_RIGHT_CONTROL);
    if (qualifier == QUAL_ALT)
        return GetKeyDown(KEY_LEFT_ALT) || GetKeyDown(KEY_RIGHT_ALT);
    if (qualifier == QUAL_SUPER)
        return GetKeyDown(KEY_LEFT_SUPER) || GetKeyDown(KEY_RIGHT_SUPER);

    return false;
}

bool Input::GetQualifierPress(Qualifier qualifier) const
{
    if (qualifier == QUAL_SHIFT)
        return GetKeyPress(KEY_LEFT_SHIFT) || GetKeyPress(KEY_RIGHT_SHIFT);
    if (qualifier == QUAL_CTRL)
        return GetKeyPress(KEY_LEFT_CONTROL) || GetKeyPress(KEY_RIGHT_CONTROL);
    if (qualifier == QUAL_ALT)
        return GetKeyPress(KEY_LEFT_ALT) || GetKeyPress(KEY_RIGHT_ALT);

    return false;
}

QualifierFlags Input::GetQualifiers() const
{
    QualifierFlags ret = QUAL_NONE;
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

    double x=0,y=0;
    GLFWwindow *win = graphics_->GetWindow();
    if(win)
        glfwGetCursorPos(win,&x, &y);
    ret.x_ = (int)(x * inputScale_.x_);
    ret.y_ = (int)(y * inputScale_.y_);

    return ret;
}
IntVector2 Input::GetMouseMove() const
{
    if (!suppressNextMouseMove_)
    {
        Vector2 posdelta = mousePosLastUpdate_ - mouseMove_;
        return mouseMoveScaled_ ? IntVector2(posdelta.x_,posdelta.y_) : IntVector2((int)(posdelta.x_ * inputScale_.x_), (int)(posdelta.y_ * inputScale_.y_));
    }
    else
        return IntVector2::ZERO;
}

int Input::GetMouseMoveX() const
{

    if (!suppressNextMouseMove_)
    {
        Vector2 posdelta = mousePosLastUpdate_ - mouseMove_;
        return int(mouseMoveScaled_ ? posdelta.x_ : (posdelta.x_ * inputScale_.x_));
    }
    else
        return 0;
}

int Input::GetMouseMoveY() const
{
    if (!suppressNextMouseMove_) {
        Vector2 posdelta = mousePosLastUpdate_ - mouseMove_;
        return int(mouseMoveScaled_ ? posdelta.y_ : posdelta.y_ * inputScale_.y_);
    }
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

    return nullptr;
}

JoystickState* Input::GetJoystick(int id)
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
    unsigned size = GLFW_JOYSTICK_LAST;
    for (unsigned i = 0; i < size; ++i)
    {
        if(0!=glfwJoystickPresent(i))
            OpenJoystick(i);
    }
}

void Input::ResetInputAccumulation()
{
    // Reset input accumulation for this frame
    keyPress_.clear();
    scancodePress_.clear();
    mouseButtonPress_ = MOUSEB_NONE;
    mouseButtonClick_ = MOUSEB_NONE;
    ResetMousePos();
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
    //    if (!mouseVisible_)
    //        SDL_ShowCursor(SDL_FALSE);

    SendInputFocusEvent();
}

void Input::LoseFocus()
{
    ResetState();

    inputFocus_ = false;
    focusedThisFrame_ = false;

    // Show the mouse cursor when inactive
    //    SDL_ShowCursor(SDL_TRUE);

    // Change mouse mode -- removing any cursor grabs, etc.
    const MouseMode mm = mouseMode_;
    SetMouseMode(MM_FREE, true);
    // Restore flags to reflect correct mouse state.
    mouseMode_ = mm;

    SendInputFocusEvent();
}
void Input::ResetMousePos()
{
    Graphics* graphics = m_context->m_Graphics.get();
    double mx=0;
    double my=0;
    if(graphics && graphics->GetWindow())
    {
        glfwGetCursorPos(graphics->GetWindow(),&mx,&my);
    }
    mouseMove_ = mousePosLastUpdate_;
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

    ResetMousePos();
    mouseMoveWheel_ = 0;
    mouseButtonPress_ = MOUSEB_NONE;
    mouseButtonClick_ = MOUSEB_NONE;
}

void Input::SendInputFocusEvent()
{
    g_inputSignals.inputFocus(HasFocus(),IsMinimized());
}

void Input::SetMouseButton(MouseButton button, bool newState)
{
    if (newState)
    {
        if (!(mouseButtonDown_ & button))
            mouseButtonPress_ |= button;

        mouseButtonDown_ |= button;
        mousePressTimer_.Reset();
        mousePressPosition_ = GetMousePosition();
    }
    else
    {
        if (mousePressTimer_.GetMSec(false) < 250 && mousePressPosition_ == GetMousePosition())
            mouseButtonClick_ |= button;
        if (!(mouseButtonDown_ & button))
            return;

        mouseButtonDown_ &= ~button;
    }
    if(newState)
        g_inputSignals.mouseButtonDown(button,mouseButtonDown_,GetQualifiers());
    else
        g_inputSignals.mouseButtonUp(button,mouseButtonDown_,GetQualifiers());
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
        g_inputSignals.keyDown(key,scancode,mouseButtonDown_,GetQualifiers(),repeat);
    else
        g_inputSignals.keyUp(key,scancode,mouseButtonDown_,GetQualifiers());

    if ((key == KEY_ENTER || key == KEY_KP_ENTER) && newState && !repeat && toggleFullscreen_ &&
            (GetKeyDown(KEY_LEFT_ALT) || GetKeyDown(KEY_RIGHT_ALT)))
        graphics_->ToggleFullscreen();
}

void Input::SetMouseWheel(int delta)
{

    if (delta)
    {
        mouseMoveWheel_ += delta;
        g_inputSignals.mouseWheel(delta,mouseButtonDown_,GetQualifiers());
    }
}

void Input::SetMousePosition(const IntVector2& position)
{
    if (!graphics_)
        return;
    glfwSetCursorPos(graphics_->GetWindow(),(int)(position.x_ / inputScale_.x_), (int)(position.y_ / inputScale_.y_));
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
    ResetMousePos();
}

void Input::UnsuppressMouseMove()
{
    suppressNextMouseMove_ = false;
    ResetMousePos();
    lastMousePosition_ = GetMousePosition();
}

void Input::HandleScreenMode(int Width, int Height, bool Fullscreen, bool Borderless, bool Resizable, bool HighDPI,
                             int Monitor, int RefreshRate)
{
    if (!initialized_)
        Initialize();

    // Re-enable cursor clipping, and re-center the cursor (if needed) to the new screen size, so that there is no erroneous
    // mouse move event. Also get new window ID if it changed
    GLFWwindow* window = graphics_->GetWindow();

    if (graphics_->GetFullscreen() || !mouseVisible_)
        focusedThisFrame_ = true;

    // After setting a new screen mode we should not be minimized
    minimized_ = glfwGetWindowAttrib(window,GLFW_ICONIFIED);
    // Calculate input coordinate scaling from SDL window to backbuffer ratio
    int winWidth, winHeight;
    int gfxWidth = graphics_->GetWidth();
    int gfxHeight = graphics_->GetHeight();
    glfwGetWindowSize(window, &winWidth, &winHeight);
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
    g_inputSignals.inputBegin();
    Update();
    g_inputSignals.inputEnd();
}

}
