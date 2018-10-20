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
#include "Lutefisk3D/Engine/jlsignal/Signal.h"

namespace Urho3D
{
enum MouseMode : uint8_t;
enum MouseButton : unsigned;
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
