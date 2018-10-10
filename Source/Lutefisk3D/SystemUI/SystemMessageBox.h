//
// Copyright (c) 2017 the Urho3D project.
// Copyright (c) 2008-2015 the Urho3D project.
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
#include "SystemUI.h"

namespace Urho3D
{

/// Message box dialog.
class LUTEFISK3D_EXPORT SystemMessageBox : public Object
{
    URHO3D_OBJECT(SystemMessageBox, Object)

public:
    /// Construct. If layout file is not given, use the default message box layout. If style file is not given, use the default style file from root UI element.
    SystemMessageBox(Context* context, const QString& messageString = QString(), const QString& titleString = QString());
    /// Destruct.
    ~SystemMessageBox() override;
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
    /// Returns true if message box is open.
    bool IsOpen() const { return isOpen_; }
    //////////////////////////////////////////////////////////
    // SIGNALS
    //////////////////////////////////////////////////////////
    jl::Signal<bool> messageAck;

private:
    /// Render message box ui.
    void RenderFrame(float s);

    /// Title text element.
    QString titleText_;
    /// Message text element.
    QString messageText_;
    /// Is message box window open.
    bool isOpen_;
    /// Initial message box window position.
    ImVec2 windowPosition_;
    /// Initial message box window size.
    ImVec2 windowSize_;
};

}
