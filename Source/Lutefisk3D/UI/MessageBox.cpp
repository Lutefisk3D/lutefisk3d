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

#include "Lutefisk3D/UI/Button.h"
#include "Lutefisk3D/Core/Context.h"
#include "Lutefisk3D/Graphics/Graphics.h"
#include "Lutefisk3D/IO/Log.h"
#include "Lutefisk3D/UI/MessageBox.h"
#include "Lutefisk3D/Resource/ResourceCache.h"
#include "Lutefisk3D/UI/Text.h"
#include "Lutefisk3D/UI/UI.h"
#include "Lutefisk3D/UI/UIEvents.h"
#include "Lutefisk3D/UI/Window.h"
#include "Lutefisk3D/Resource/XMLFile.h"

namespace Urho3D
{

MessageBox::MessageBox(Context* context, const QString& messageString, const QString& titleString, XMLFile* layoutFile, XMLFile* styleFile) :
    Object(context),
    SignalObserver(context->m_observer_allocator),
    window_(nullptr),
    titleText_(nullptr),
    messageText_(nullptr),
    okButton_(nullptr)
{
    // If layout file is not given, use the default message box layout
    if (!layoutFile)
    {
        layoutFile = context_->m_ResourceCache->GetResource<XMLFile>("UI/MessageBox.xml");
        if (!layoutFile)    // Error is already logged
            return;         // Note: windowless MessageBox should not be used!
    }

    UI* ui = context_->m_UISystem.get();
    UIElement* root = ui->GetRoot();
    {
        SharedPtr<UIElement> holder = ui->LoadLayout(layoutFile, styleFile);
        if (!holder)    // Error is already logged
        return;
        window_ = holder;
        root->AddChild(window_);    // Take ownership of the object before SharedPtr goes out of scope
    }

    // Set the title and message strings if they are given
    titleText_ = window_->GetChildDynamicCast<Text>("TitleText", true);
    if (titleText_ && !titleString.isEmpty())
        titleText_->SetText(titleString);
    messageText_ = window_->GetChildDynamicCast<Text>("MessageText", true);
    if (messageText_ && !messageString.isEmpty())
        messageText_->SetText(messageString);

    // Center window after the message is set
    Window* window = dynamic_cast<Window*>(window_);
    if (window)
    {
        const IntVector2& size = window->GetSize();
        window->SetPosition((root->GetWidth() - size.x_) / 2, (root->GetHeight() - size.y_) / 2);
        window->SetModal(true);
        window->modalChanged.Connect(this,&MessageBox::HandleModalChange);
    }

    // Bind the buttons (if any in the loaded UI layout) to event handlers
    okButton_ = window_->GetChildDynamicCast<Button>("OkButton", true);
    if (okButton_)
    {
        ui->SetFocusElement(okButton_);
        okButton_->released.Connect(this,&MessageBox::HandleMessageAcknowledged);
    }
    Button* cancelButton = window_->GetChildDynamicCast<Button>("CancelButton", true);
    if (cancelButton)
        cancelButton->released.Connect(this,&MessageBox::HandleMessageAcknowledged);
    Button* closeButton = window_->GetChildDynamicCast<Button>("CloseButton", true);
    if (closeButton)
        closeButton->released.Connect(this,&MessageBox::HandleMessageAcknowledged);

    // Increase reference count to keep Self alive
    AddRef();
}

MessageBox::~MessageBox()
{
    // This would remove the UI-element regardless of whether it is parented to UI's root or UI's modal-root
    if (window_)
        window_->Remove();
}

void MessageBox::RegisterObject(Context* context)
{
    context->RegisterFactory<MessageBox>();
}

void MessageBox::SetTitle(const QString& text)
{
    if (titleText_)
        titleText_->SetText(text);
}

void MessageBox::SetMessage(const QString& text)
{
    if (messageText_)
        messageText_->SetText(text);
}

const QString& MessageBox::GetTitle() const
{
    return titleText_ ? titleText_->GetText() : s_dummy;
}

const QString& MessageBox::GetMessage() const
{
    return messageText_ ? messageText_->GetText() : s_dummy;
}
void MessageBox::HandleModalChange(UIElement *,bool ) {
    //TODO: this assumes any change in modality shoud ACK the dialog
    messageACK(false);
    ReleaseRef();
}
void MessageBox::HandleMessageAcknowledged(UIElement *elem)
{
    messageACK(elem == okButton_);
    // Self destruct
    ReleaseRef();
}

}
