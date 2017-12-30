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

class Window;

}

/// A simple 'HelloWorld' GUI created purely from code.
/// This sample demonstrates:
///     - Creation of controls and building a UI hierarchy
///     - Loading UI style from XML and applying it to controls
///     - Handling of global and per-control events
/// For more advanced users (beginners can skip this section):
///     - Dragging UIElements
///     - Displaying tooltips
///     - Accessing available Events data (eventData)
class HelloGUI : public Sample
{
public:
    /// Construct.
    HelloGUI(Context* context);

    /// Setup after engine initialization and before running the main loop.
    virtual void Start() override;

private:
    /// Create and initialize a Window control.
    void InitWindow();
    /// Create and add various common controls for demonstration purposes.
    void InitControls();
    /// Create a draggable fish button.
    void CreateDraggableFish();
    /// Handle drag begin for the fish button.
    void HandleDragBegin(UIElement *, int X, int Y, int elemX, int elemY, int, int);
    /// Handle drag move for the fish button.
    void HandleDragMove(UIElement *draggedElement, int X, int Y, IntVector2 dlta, int elemX, int elemY, int, int);
    /// Handle drag end for the fish button.
    void HandleDragEnd(UIElement *, int, int, int, int, int, int);
    /// Handle any UI control being clicked.
    void HandleControlClicked(UIElement *clicked, int, int, MouseButton, unsigned, int);
    /// Handle close button pressed and released.
    void HandleClosePressed(UIElement *);

    /// The Window.
    SharedPtr<Window> window_;
    /// The UI's root UIElement.
    SharedPtr<UIElement> uiRoot_;
    /// Remembered drag begin position.
    IntVector2 dragBeginPosition_;
};


