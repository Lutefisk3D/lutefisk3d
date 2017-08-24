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
#include "Lutefisk3D/UI/UIEvents.h"

namespace Urho3D
{

class Button;
class Text;
class UIElement;
class XMLFile;

/// Message box dialog. Manages its lifetime automatically, so the application does not need to hold a reference to it, and shouldn't attempt to destroy it manually.
class URHO3D_API MessageBox : public Object,public UIMessageBoxSignals, public jl::SignalObserver
{
    URHO3D_OBJECT(MessageBox,Object)

public:
    /// Construct. If layout file is not given, use the default message box layout. If style file is not given, use the default style file from root UI element.
    MessageBox(Context* context, const QString& messageString = QString(), const QString& titleString = QString(), XMLFile* layoutFile = 0, XMLFile* styleFile = 0);
    /// Destruct.
    virtual ~MessageBox();
    /// Register object factory.
    static void RegisterObject(Context* context);

    /// Set title text. No-ops if there is no title text element.
    void SetTitle(const QString& text);
    /// Set message text. No-ops if there is no message text element.
    void SetMessage(const QString& text);

    /// Return title text. Return empty string if there is no title text element.
    const QString& GetTitle() const;
    /// Return message text. Return empty string if there is no message text element.
    const QString& GetMessage() const;
    /// Return dialog window.
    UIElement* GetWindow() const { return window_; }

private:
    /// Handle events that dismiss the message box.
    void HandleMessageAcknowledged(Urho3D::UIElement *elem);
    void HandleModalChange(UIElement *, bool);

    /// UI element containing the whole UI layout. Typically it is a Window element type.
    UIElement* window_;
    /// Title text element.
    Text* titleText_;
    /// Message text element.
    Text* messageText_;
    /// OK button element.
    Button* okButton_;
};

}
