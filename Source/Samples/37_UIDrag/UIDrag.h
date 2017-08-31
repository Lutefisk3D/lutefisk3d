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
    class Scene;
    class UIElement;
}

/// GUI test example.
/// This sample demonstrates:
///     - Creating GUI elements from C++
///     - Loading GUI Style from xml
///     - Subscribing to GUI drag events and handling them
///     - Working with GUI elements with specific tags.
class UIDrag : public Sample
{
public:
    /// Construct.
    UIDrag(Context* context);

    /// Setup after engine initialization and before running the main loop.
    virtual void Start() override;

private:
    /// Construct the GUI.
    void CreateGUI();
    /// Construct an instruction text to the UI.
    void CreateInstructions();
    /// Subscribe to application-wide logic update events.
    void SubscribeToEvents();

    void HandleUpdate(float timeStep);
    void HandleDragBegin(UIElement *elem,int lx,int ly,int,int,int buttons,int btncount);
    void HandleDragMove(UIElement *element, int x, int y, IntVector2 delta, int, int, int btns, int);
    void HandleDragCancel(UIElement *elem, int, int, int, int, int, int);
    void HandleDragEnd(UIElement *, int, int, int, int, int, int);
    void HandleTextFinished(StringHash eventType, VariantMap& eventData);
    void HandleLineEditDragBegin(StringHash eventType, VariantMap& eventData);
};
