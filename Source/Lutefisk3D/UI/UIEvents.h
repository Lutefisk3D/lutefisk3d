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
#include "Lutefisk3D/Math/Vector2.h"
#include "Lutefisk3D/Engine/jlsignal/Signal.h"

namespace Urho3D
{
class UIElement;
enum class  MouseButton : int;
struct UISignals {
    /// Global mouse click in the UI. Sent by the UI subsystem.
    jl::Signal<UIElement *,int, int, MouseButton, unsigned, int> mouseClickUI; //Element,x,y,Button,Buttons,Qualifiers
    /// Global mouse click end in the UI. Sent by the UI subsystem.
    //Element,BeginElement,x,y,Button,Buttons,Qualifiers
    jl::Signal<UIElement *,UIElement *,int, int, MouseButton, unsigned, int> mouseClickEndUI;
    /// Global mouse double click in the UI. Sent by the UI subsystem.
    jl::Signal<UIElement *,int, int, MouseButton, unsigned, int> mouseDoubleClickUI; //Element,x,y,Button,Buttons,Qualifiers

    /// Drag and drop finish.
    jl::Signal<UIElement *,UIElement *,bool> dragDropFinish; // Source,Target,Accept
    /// Drag and drop test.
    jl::Signal<UIElement *,UIElement *,bool &> dragDropTest; // Source,Target,Accept
    /// Focus element changed.
    /// Since focus can be passed over - Element points to part that actually gets the focus
    /// And ClickedElement is element that was actually 'clicked'/'activated'
    jl::Signal<UIElement *,UIElement *> focusChanged; // Element ,ClickedElement

    /// A file was drag-dropped into the application window. Includes also coordinates and UI element if applicable
    jl::Signal<const QString &,UIElement *,int,int,int,int> dropFileUI; //FileName,Element,X,Y,ElementX,ElementY

    void init(jl::ScopedAllocator *allocator)
    {
        mouseClickUI.SetAllocator(allocator);
        mouseClickEndUI.SetAllocator(allocator);
        mouseDoubleClickUI.SetAllocator(allocator);
        dragDropFinish.SetAllocator(allocator);
        dragDropTest.SetAllocator(allocator);
        focusChanged.SetAllocator(allocator);
        dropFileUI.SetAllocator(allocator);
    }
};
extern LUTEFISK3D_EXPORT UISignals g_uiSignals;

/// UI element positioned.
URHO3D_EVENT(E_POSITIONED, Positioned)
{
    URHO3D_PARAM(P_ELEMENT, Element);              // UIElement pointer
    URHO3D_PARAM(P_X, X);                          // int
    URHO3D_PARAM(P_Y, Y);                          // int
}

/// UI element visibility changed.
URHO3D_EVENT(E_VISIBLECHANGED, VisibleChanged)
{
    URHO3D_PARAM(P_ELEMENT, Element);              // UIElement pointer
    URHO3D_PARAM(P_VISIBLE, Visible);              // bool
}
struct UiElementSignals  {
    /// UI element name changed.
    jl::Signal<UIElement *> nameChanged;
    /// UI element resized Width,Height  DX,DY
    jl::Signal<UIElement *,int,int,int,int> resized;

    /// Mouse click on a UI element. Parameters are same as in UIMouseClick event, but is sent by the element.
    jl::Signal<UIElement *,int, int, MouseButton, unsigned, int> click; //Element,x,y,Button,Buttons,Qualifiers
    /// Mouse click end on a UI element. Parameters are same as in UIMouseClickEnd event, but is sent by the element.
    //Element,BeginElement,x,y,Button,Buttons,Qualifiers
    jl::Signal<UIElement *,UIElement *,int, int, MouseButton, unsigned, int> clickEnd;
    /// Mouse double click on a UI element. Parameters are same as in UIMouseDoubleClick event, but is sent by the element.
    jl::Signal<UIElement *,int, int, MouseButton, unsigned, int> doubleClick; //Element,x,y,Button,Buttons,Qualifiers

    /// UI element layout updated.
    jl::Signal<UIElement *> layoutUpdated;

    /// UI element focused.
    jl::Signal<UIElement *,bool> focused; //UIElement *focusedElement,bool byKey
    /// UI element defocused.
    jl::Signal<UIElement *> defocused; //UIElement *defocusedElement

    /// Hovering on an UI element has started
    jl::Signal<UIElement *,int,int,int,int> hoverBegin; // Element,int x,int y,int elemX,int elemY
    /// Hovering on an UI element has ended
    jl::Signal<UIElement *> hoverEnd; // Element

    /// Drag behavior of a UI Element has started
    // Element,int x,int y,int elemX,int elemY,int btns,int b_count
    jl::Signal<UIElement *,int,int,int,int,int,int> dragBegin;
    /// Drag behavior of a UI Element has finished
    // UIElement *element,int x,int y,int elemX,int elemY,int btns,int b_count
    jl::Signal<UIElement *,int,int,int,int,int,int> dragEnd;
    /// Drag behavior of a UI Element when the input device has moved
    // Element,int x,int y,IntVector2 delta,int elemX,int elemY,int btns,int b_count
    jl::Signal<UIElement *,int,int,IntVector2,int,int,int,int> dragMove;
    /// Drag of a UI Element was canceled by pressing ESC
    jl::Signal<UIElement *,int,int,int,int,int,int> dragCancel;
    void initSignals(jl::ScopedAllocator *allocator)
    {
        nameChanged.SetAllocator(allocator);
        resized.SetAllocator(allocator);
        click.SetAllocator(allocator);
        clickEnd.SetAllocator(allocator);
        doubleClick.SetAllocator(allocator);
        layoutUpdated.SetAllocator(allocator);
        focused.SetAllocator(allocator);
        defocused.SetAllocator(allocator);
        hoverBegin.SetAllocator(allocator);
        hoverEnd.SetAllocator(allocator);
        dragBegin.SetAllocator(allocator);
        dragEnd.SetAllocator(allocator);
        dragMove.SetAllocator(allocator);
        dragCancel.SetAllocator(allocator);
    }
};

struct UIButtonSignals {
    /// UI button pressed.
    jl::Signal<UIElement *> pressed;
    /// UI button was pressed, then released.
    jl::Signal<UIElement *> released;
};
struct UIWindowSignals {
    /// UI modal changed (currently only Window has modal flag).
    jl::Signal<UIElement *,bool> modalChanged; // UIElement *,bool Modal
};
/// UI checkbox toggled.
URHO3D_EVENT(E_TOGGLED, Toggled)
{
    URHO3D_PARAM(P_ELEMENT, Element);              // UIElement pointer
    URHO3D_PARAM(P_STATE, State);                  // bool
}

struct UISliderSignals {
    /// UI slider value changed
    jl::Signal<UIElement *,float> sliderChanged; // UIElement *,float
    /// UI slider being paged.
    jl::Signal<UIElement *,int,bool> sliderPaged; // UIElement *,int offset,bool pressed
};

struct UIScrollbarSignals {
    /// UI scrollbar value changed.
    jl::Signal<UIElement *,float> scrollBarChanged; // UIElement *,float

};

/// UI scrollview position changed.
URHO3D_EVENT(E_VIEWCHANGED, ViewChanged)
{
    URHO3D_PARAM(P_ELEMENT, Element);              // UIElement pointer
    URHO3D_PARAM(P_X, X);                          // int
    URHO3D_PARAM(P_Y, Y);                          // int
}

struct LineEditSignals {
    /// Text editing finished (enter pressed on a LineEdit)
    jl::Signal<UIElement *,const QString &,float> textFinished; // Element,Text,Value
};
/// Text entry into a LineEdit. The text can be modified in the event data.
URHO3D_EVENT(E_TEXTENTRY, TextEntry)
{
    URHO3D_PARAM(P_ELEMENT, Element);              // UIElement pointer
    URHO3D_PARAM(P_TEXT, Text);                    // String [in/out]
}

/// Editable text changed
URHO3D_EVENT(E_TEXTCHANGED, TextChanged)
{
    URHO3D_PARAM(P_ELEMENT, Element);              // UIElement pointer
    URHO3D_PARAM(P_TEXT, Text);                    // String
}

/// Menu selected.
URHO3D_EVENT(E_MENUSELECTED, MenuSelected)
{
    URHO3D_PARAM(P_ELEMENT, Element);              // UIElement pointer
}

/// Listview or DropDownList item selected.
URHO3D_EVENT(E_ITEMSELECTED, ItemSelected)
{
    URHO3D_PARAM(P_ELEMENT, Element);              // UIElement pointer
    URHO3D_PARAM(P_SELECTION, Selection);          // int
}

/// Listview item deselected.
URHO3D_EVENT(E_ITEMDESELECTED, ItemDeselected)
{
    URHO3D_PARAM(P_ELEMENT, Element);              // UIElement pointer
    URHO3D_PARAM(P_SELECTION, Selection);          // int
}

/// Listview selection change finished.
URHO3D_EVENT(E_SELECTIONCHANGED, SelectionChanged)
{
    URHO3D_PARAM(P_ELEMENT, Element);              // UIElement pointer
}

/// Listview item clicked. If this is a left-click, also ItemSelected event will be sent. If this is a right-click, only this event is sent.
URHO3D_EVENT(E_ITEMCLICKED, ItemClicked)
{
    URHO3D_PARAM(P_ELEMENT, Element);              // UIElement pointer
    URHO3D_PARAM(P_ITEM, Item);                    // UIElement pointer
    URHO3D_PARAM(P_SELECTION, Selection);          // int
    URHO3D_PARAM(P_BUTTON, Button);                // int
    URHO3D_PARAM(P_BUTTONS, Buttons);              // int
    URHO3D_PARAM(P_QUALIFIERS, Qualifiers);        // int
}

/// Listview item double clicked.
URHO3D_EVENT(E_ITEMDOUBLECLICKED, ItemDoubleClicked)
{
    URHO3D_PARAM(P_ELEMENT, Element);              // UIElement pointer
    URHO3D_PARAM(P_ITEM, Item);                    // UIElement pointer
    URHO3D_PARAM(P_SELECTION, Selection);          // int
    URHO3D_PARAM(P_BUTTON, Button);                // int
    URHO3D_PARAM(P_BUTTONS, Buttons);              // int
    URHO3D_PARAM(P_QUALIFIERS, Qualifiers);        // int
}

/// LineEdit or ListView unhandled key pressed.
URHO3D_EVENT(E_UNHANDLEDKEY, UnhandledKey)
{
    URHO3D_PARAM(P_ELEMENT, Element);              // UIElement pointer
    URHO3D_PARAM(P_KEY, Key);                      // int
    URHO3D_PARAM(P_BUTTONS, Buttons);              // int
    URHO3D_PARAM(P_QUALIFIERS, Qualifiers);        // int
}

/// Fileselector choice.
URHO3D_EVENT(E_FILESELECTED, FileSelected)
{
    URHO3D_PARAM(P_FILENAME, FileName);            // String
    URHO3D_PARAM(P_FILTER, Filter);                // String
    URHO3D_PARAM(P_OK, Ok);                        // bool
}
struct UIMessageBoxSignals {
    /// MessageBox acknowlegement.
    jl::Signal<bool> messageACK; // bool OK
};

/// A child element has been added to an element. Sent by the UI root element, or element-event-sender if set.
URHO3D_EVENT(E_ELEMENTADDED, ElementAdded)
{
    URHO3D_PARAM(P_ROOT, Root);                    // UIElement pointer
    URHO3D_PARAM(P_PARENT, Parent);                // UIElement pointer
    URHO3D_PARAM(P_ELEMENT, Element);              // UIElement pointer
}

/// A child element is about to be removed from an element. Sent by the UI root element, or element-event-sender if set.
URHO3D_EVENT(E_ELEMENTREMOVED, ElementRemoved)
{
    URHO3D_PARAM(P_ROOT, Root);                    // UIElement pointer
    URHO3D_PARAM(P_PARENT, Parent);                // UIElement pointer
    URHO3D_PARAM(P_ELEMENT, Element);              // UIElement pointer
}

}
