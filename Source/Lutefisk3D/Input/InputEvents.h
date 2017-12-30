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
#include <jlsignal/Signal.h>


namespace Urho3D
{
enum MouseMode : uint8_t;
enum class MouseButton : int;
struct InputSignals
{
    /// gestureRecorded - A touch gesture finished recording.
    /// gestureInput - A recognized touch gesture was input by the user.
    /// multiGesture - Pinch/rotate multi-finger touch gesture motion update.
    /// keyDown - Key pressed.
    /// keyUp - Key released.
    /// mouseButtonDown - Mouse button pressed.
    /// mouseButtonUp - Mouse button released.
    /// moueMove - Mouse moved.
    /// mouseWheel - Mouse wheel moved.
    /// joystickConnected - Joystick connected.
    /// joystickDisconnected - Joystick disconnected.
    /// joystickButtonDown - Joystick button pressed.
    /// joystickButtonUp - Joystick button released.
    /// joystickAxisMove - Joystick axis moved.
    /// joystickHatMove - Joystick POV hat moved.

    /// textInput - Text input event.
    /// inputBegin - Input handling begins.
    /// inputEnd - Input handling ends.
    /// mouseVisibleChanged - OS mouse cursor visibility changed.
    /// inputFocus - Application input focus or minimization changed.
    /// dropFile - A file was drag-dropped into the application window.
    /// exitRequested - Application exit requested.
    /// mouseModeChanged - Mouse mode changed.

    jl::Signal<int, int, unsigned, int, bool> keyDown; // int key,int scancode,unsigned buttons,int qualifiers, bool repeat
    jl::Signal<int, int, unsigned, int> keyUp;         // int key,int scancode,unsigned buttons,int qualifiers
    jl::Signal<MouseButton, unsigned, int> mouseButtonDown;    // Button,unsigned Buttons,int Qualifiers
    jl::Signal<MouseButton, unsigned, int> mouseButtonUp;      // Button,unsigned Buttons,int Qualifiers
    jl::Signal<int, int, int, int, unsigned, int> mouseMove; // int X,int Y,int DX,int DY,unsigned Buttons,int
    // Qualifiers
    jl::Signal<int, unsigned, int> mouseWheel;               // int Wheel,unsigned Buttons,int Qualifiers
    jl::Signal<bool> mouseVisibleChanged;                    // bool Visible
    jl::Signal<MouseMode, bool> mouseModeChanged;            // MouseMode mode,bool mouseLocked

    jl::Signal<const QString &> textInput;                   // const QString &text

    jl::Signal<> inputBegin;
    jl::Signal<> inputEnd;
    jl::Signal<bool,bool> inputFocus; // bool Focus,bool Minimized

    jl::Signal<const QString &> dropFile; // const QString &fileName
    jl::Signal<> exitRequested;

    jl::Signal<int> joystickConnected; //int JoystickID
    jl::Signal<int> joystickDisconnected; //int JoystickID
    jl::Signal<int,unsigned> joystickButtonDown; //int JoystickID,unsigned button
    jl::Signal<int,unsigned> joystickButtonUp; //int JoystickID,unsigned button
    jl::Signal<int,int,float> joystickAxisMove; //int JoystickID,int axis,float position
    jl::Signal<int,int,int> joystickHatMove; //int JoystickID, int button,int position

    void init(jl::ScopedAllocator *alloctor) {
        keyDown.SetAllocator(alloctor);
        keyUp.SetAllocator(alloctor);
        mouseButtonDown.SetAllocator(alloctor);
        mouseButtonUp.SetAllocator(alloctor);
        mouseMove.SetAllocator(alloctor);
        mouseWheel.SetAllocator(alloctor);
        mouseVisibleChanged.SetAllocator(alloctor);
        mouseModeChanged.SetAllocator(alloctor);

        textInput.SetAllocator(alloctor);

        inputBegin.SetAllocator(alloctor);
        inputEnd.SetAllocator(alloctor);
        inputFocus.SetAllocator(alloctor);

        dropFile.SetAllocator(alloctor);
        exitRequested.SetAllocator(alloctor);

        joystickConnected.SetAllocator(alloctor);
        joystickDisconnected.SetAllocator(alloctor);
        joystickButtonDown.SetAllocator(alloctor);
        joystickButtonUp.SetAllocator(alloctor);
        joystickAxisMove.SetAllocator(alloctor);
        joystickHatMove.SetAllocator(alloctor);
    }


};
extern LUTEFISK3D_EXPORT InputSignals g_inputSignals;

enum class MouseButton : int {
    LEFT=0,
    RIGHT,
    MIDDLE,
    X1,
    X2
};

static const int QUAL_SHIFT = 1;
static const int QUAL_CTRL = 2;
static const int QUAL_ALT = 4;
static const int QUAL_ANY = 8;
//WARNING: the key codes are copied from the GLFW header
enum InputNames : int32_t
{
    KEY_UNKNOWN      = -1,
    KEY_SPACE               = 32,
    KEY_APOSTROPHE          = 39,  /* ' */
    KEY_COMMA               = 44,  /* , */
    KEY_MINUS               = 45,  /* - */
    KEY_PERIOD              = 46,  /* . */
    KEY_SLASH               = 47,  /* / */
    KEY_0                   = 48,
    KEY_1                   = 49,
    KEY_2                   = 50,
    KEY_3                   = 51,
    KEY_4                   = 52,
    KEY_5                   = 53,
    KEY_6                   = 54,
    KEY_7                   = 55,
    KEY_8                   = 56,
    KEY_9                   = 57,
    KEY_SEMICOLON           = 59,  /* ; */
    KEY_EQUAL               = 61,  /* = */
    KEY_A                   = 65,
    KEY_B                   = 66,
    KEY_C                   = 67,
    KEY_D                   = 68,
    KEY_E                   = 69,
    KEY_F                   = 70,
    KEY_G                   = 71,
    KEY_H                   = 72,
    KEY_I                   = 73,
    KEY_J                   = 74,
    KEY_K                   = 75,
    KEY_L                   = 76,
    KEY_M                   = 77,
    KEY_N                   = 78,
    KEY_O                   = 79,
    KEY_P                   = 80,
    KEY_Q                   = 81,
    KEY_R                   = 82,
    KEY_S                   = 83,
    KEY_T                   = 84,
    KEY_U                   = 85,
    KEY_V                   = 86,
    KEY_W                   = 87,
    KEY_X                   = 88,
    KEY_Y                   = 89,
    KEY_Z                   = 90,
    KEY_LEFT_BRACKET        = 91,  /* [ */
    KEY_BACKSLASH           = 92,  /* \ */
    KEY_RIGHT_BRACKET       = 93,  /* ] */
    KEY_GRAVE_ACCENT        = 96,  /* ` */
    KEY_WORLD_1             = 161, /* non-US #1 */
    KEY_WORLD_2             = 162, /* non-US #2 */

    /* Function keys */
    KEY_ESCAPE              = 256,
    KEY_ENTER               = 257,
    KEY_TAB                 = 258,
    KEY_BACKSPACE           = 259,
    KEY_INSERT              = 260,
    KEY_DELETE              = 261,
    KEY_RIGHT               = 262,
    KEY_LEFT                = 263,
    KEY_DOWN                = 264,
    KEY_UP                  = 265,
    KEY_PAGE_UP             = 266,
    KEY_PAGE_DOWN           = 267,
    KEY_HOME                = 268,
    KEY_END                 = 269,
    KEY_CAPS_LOCK           = 280,
    KEY_SCROLL_LOCK         = 281,
    KEY_NUM_LOCK            = 282,
    KEY_PRINT_SCREEN        = 283,
    KEY_PAUSE               = 284,
    KEY_F1                  = 290,
    KEY_F2                  = 291,
    KEY_F3                  = 292,
    KEY_F4                  = 293,
    KEY_F5                  = 294,
    KEY_F6                  = 295,
    KEY_F7                  = 296,
    KEY_F8                  = 297,
    KEY_F9                  = 298,
    KEY_F10                 = 299,
    KEY_F11                 = 300,
    KEY_F12                 = 301,
    KEY_F13                 = 302,
    KEY_F14                 = 303,
    KEY_F15                 = 304,
    KEY_F16                 = 305,
    KEY_F17                 = 306,
    KEY_F18                 = 307,
    KEY_F19                 = 308,
    KEY_F20                 = 309,
    KEY_F21                 = 310,
    KEY_F22                 = 311,
    KEY_F23                 = 312,
    KEY_F24                 = 313,
    KEY_F25                 = 314,
    KEY_KP_0                = 320,
    KEY_KP_1                = 321,
    KEY_KP_2                = 322,
    KEY_KP_3                = 323,
    KEY_KP_4                = 324,
    KEY_KP_5                = 325,
    KEY_KP_6                = 326,
    KEY_KP_7                = 327,
    KEY_KP_8                = 328,
    KEY_KP_9                = 329,
    KEY_KP_PERIOD           = 330,
    KEY_KP_DIVIDE           = 331,
    KEY_KP_MULTIPLY         = 332,
    KEY_KP_MINUS            = 333,
    KEY_KP_PLUS             = 334,
    KEY_KP_ENTER            = 335,
    KEY_KP_EQUAL            = 336,
    KEY_LEFT_SHIFT          = 340,
    KEY_LEFT_CONTROL        = 341,
    KEY_LEFT_ALT            = 342,
    KEY_LEFT_SUPER          = 343,
    KEY_RIGHT_SHIFT         = 344,
    KEY_RIGHT_CONTROL       = 345,
    KEY_RIGHT_ALT           = 346,
    KEY_RIGHT_SUPER         = 347,
    KEY_MENU                = 348,
};
enum class HatPosition : uint8_t {
    CENTERED=0,
    UP,
    RIGHT,
    DOWN,
    LEFT
};

//static const int CONTROLLER_BUTTON_A = SDL_CONTROLLER_BUTTON_A;
//static const int CONTROLLER_BUTTON_B = SDL_CONTROLLER_BUTTON_B;
//static const int CONTROLLER_BUTTON_X = SDL_CONTROLLER_BUTTON_X;
//static const int CONTROLLER_BUTTON_Y = SDL_CONTROLLER_BUTTON_Y;
//static const int CONTROLLER_BUTTON_BACK = SDL_CONTROLLER_BUTTON_BACK;
//static const int CONTROLLER_BUTTON_GUIDE = SDL_CONTROLLER_BUTTON_GUIDE;
//static const int CONTROLLER_BUTTON_START = SDL_CONTROLLER_BUTTON_START;
//static const int CONTROLLER_BUTTON_LEFTSTICK = SDL_CONTROLLER_BUTTON_LEFTSTICK;
//static const int CONTROLLER_BUTTON_RIGHTSTICK = SDL_CONTROLLER_BUTTON_RIGHTSTICK;
//static const int CONTROLLER_BUTTON_LEFTSHOULDER = SDL_CONTROLLER_BUTTON_LEFTSHOULDER;
//static const int CONTROLLER_BUTTON_RIGHTSHOULDER = SDL_CONTROLLER_BUTTON_RIGHTSHOULDER;
//static const int CONTROLLER_BUTTON_DPAD_UP = SDL_CONTROLLER_BUTTON_DPAD_UP;
//static const int CONTROLLER_BUTTON_DPAD_DOWN = SDL_CONTROLLER_BUTTON_DPAD_DOWN;
//static const int CONTROLLER_BUTTON_DPAD_LEFT = SDL_CONTROLLER_BUTTON_DPAD_LEFT;
//static const int CONTROLLER_BUTTON_DPAD_RIGHT = SDL_CONTROLLER_BUTTON_DPAD_RIGHT;

//static const int CONTROLLER_AXIS_LEFTX = SDL_CONTROLLER_AXIS_LEFTX;
//static const int CONTROLLER_AXIS_LEFTY = SDL_CONTROLLER_AXIS_LEFTY;
//static const int CONTROLLER_AXIS_RIGHTX = SDL_CONTROLLER_AXIS_RIGHTX;
//static const int CONTROLLER_AXIS_RIGHTY = SDL_CONTROLLER_AXIS_RIGHTY;
//static const int CONTROLLER_AXIS_TRIGGERLEFT = SDL_CONTROLLER_AXIS_TRIGGERLEFT;
//static const int CONTROLLER_AXIS_TRIGGERRIGHT = SDL_CONTROLLER_AXIS_TRIGGERRIGHT;

}
