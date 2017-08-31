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

#include <Lutefisk3D/UI/Button.h>
#include <Lutefisk3D/UI/BorderImage.h>
#include <Lutefisk3D/UI/CheckBox.h>
#include <Lutefisk3D/Core/CoreEvents.h>
#include <Lutefisk3D/Engine/Engine.h>
#include <Lutefisk3D/Graphics/Graphics.h>
#include <Lutefisk3D/Input/Input.h>
#include <Lutefisk3D/UI/LineEdit.h>
#include <Lutefisk3D/Resource/ResourceCache.h>
#include <Lutefisk3D/UI/Text.h>
#include <Lutefisk3D/Graphics/Texture2D.h>
#include <Lutefisk3D/UI/ToolTip.h>
#include <Lutefisk3D/UI/UI.h>
#include <Lutefisk3D/UI/UIElement.h>
#include <Lutefisk3D/UI/UIEvents.h>
#include <Lutefisk3D/UI/Window.h>

#include "HelloGUI.h"

URHO3D_DEFINE_APPLICATION_MAIN(HelloGUI)

HelloGUI::HelloGUI(Context* context) :
    Sample("HelloGUI",context),
    uiRoot_(context->m_UISystem->GetRoot()),
    dragBeginPosition_(IntVector2::ZERO)
{
}

void HelloGUI::Start()
{
    // Execute base class startup
    Sample::Start();

    // Enable OS cursor
    m_context->m_InputSystem->SetMouseVisible(true);

    // Load XML file containing default UI style sheet
    ResourceCache* cache = m_context->m_ResourceCache.get();
    XMLFile* style = cache->GetResource<XMLFile>("UI/DefaultStyle.xml");

    // Set the loaded style as default style
    uiRoot_->SetDefaultStyle(style);

    // Initialize Window
    InitWindow();

    // Create and add some controls to the Window
    InitControls();

    // Create a draggable Fish
    CreateDraggableFish();
}

void HelloGUI::InitControls()
{
    // Create a CheckBox
    CheckBox* checkBox = new CheckBox(m_context);
    checkBox->SetName("CheckBox");

    // Create a Button
    Button* button = new Button(m_context);
    button->SetName("Button");
    button->SetMinHeight(24);

    // Create a LineEdit
    LineEdit* lineEdit = new LineEdit(m_context);
    lineEdit->SetName("LineEdit");
    lineEdit->SetMinHeight(24);

    // Add controls to Window
    window_->AddChild(checkBox);
    window_->AddChild(button);
    window_->AddChild(lineEdit);

    // Apply previously set default style
    checkBox->SetStyleAuto();
    button->SetStyleAuto();
    lineEdit->SetStyleAuto();
}

void HelloGUI::InitWindow()
{
    // Create the Window and add it to the UI's root node
    window_ = new Window(m_context);
    uiRoot_->AddChild(window_);

    // Set Window size and layout settings
    window_->SetMinWidth(384);
    window_->SetLayout(LM_VERTICAL, 6, IntRect(6, 6, 6, 6));
    window_->SetAlignment(HA_CENTER, VA_CENTER);
    window_->SetName("Window");

    // Create Window 'titlebar' container
    UIElement* titleBar = new UIElement(m_context);
    titleBar->SetMinSize(0, 24);
    titleBar->SetVerticalAlignment(VA_TOP);
    titleBar->SetLayoutMode(LM_HORIZONTAL);

    // Create the Window title Text
    Text* windowTitle = new Text(m_context);
    windowTitle->SetName("WindowTitle");
    windowTitle->SetText("Hello GUI!");

    // Create the Window's close button
    Button* buttonClose = new Button(m_context);
    buttonClose->SetName("CloseButton");

    // Add the controls to the title bar
    titleBar->AddChild(windowTitle);
    titleBar->AddChild(buttonClose);

    // Add the title bar to the Window
    window_->AddChild(titleBar);

    // Apply styles
    window_->SetStyleAuto();
    windowTitle->SetStyleAuto();
    buttonClose->SetStyle("CloseButton");

    // Subscribe to buttonClose release (following a 'press') events
    buttonClose->released.Connect(this,&HelloGUI::HandleClosePressed);
    // Subscribe also to all UI mouse clicks just to see where we have clicked
    g_uiSignals.mouseClickUI.Connect(this,&HelloGUI::HandleControlClicked);
}

void HelloGUI::CreateDraggableFish()
{
    ResourceCache* cache = m_context->m_ResourceCache.get();
    Graphics* graphics = m_context->m_Graphics.get();

    // Create a draggable Fish button
    Button* draggableFish = new Button(m_context);
    draggableFish->SetTexture(cache->GetResource<Texture2D>("Textures/UrhoDecal.dds")); // Set texture
    draggableFish->SetBlendMode(BLEND_ADD);
    draggableFish->SetSize(128, 128);
    draggableFish->SetPosition((graphics->GetWidth() - draggableFish->GetWidth()) / 2, 200);
    draggableFish->SetName("Fish");
    uiRoot_->AddChild(draggableFish);

    // Add a tooltip to Fish button
    ToolTip* toolTip = new ToolTip(m_context);
    draggableFish->AddChild(toolTip);
    toolTip->SetPosition(IntVector2(draggableFish->GetWidth() + 5, draggableFish->GetWidth() / 2)); // slightly offset from close button
    BorderImage* textHolder = new BorderImage(m_context);
    toolTip->AddChild(textHolder);
    textHolder->SetStyle("ToolTipBorderImage");
    Text* toolTipText = new Text(m_context);
    textHolder->AddChild(toolTipText);
    toolTipText->SetStyle("ToolTipText");
    toolTipText->SetText("Please drag me!");

    // Subscribe draggableFish to Drag Events (in order to make it draggable)
    // See "Event list" in documentation's Main Page for reference on available Events and their eventData
    draggableFish->dragBegin.Connect(this,&HelloGUI::HandleDragBegin);
    draggableFish->dragMove.Connect(this,&HelloGUI::HandleDragMove);
    draggableFish->dragEnd.Connect(this,&HelloGUI::HandleDragEnd);
}

void HelloGUI::HandleDragBegin(UIElement *,int,int,int elemX,int elemY,int,int)
{
    // Get UIElement relative position where input (touch or click) occured (top-left = IntVector2(0,0))
    dragBeginPosition_ = IntVector2(elemX, elemY);
}

void HelloGUI::HandleDragMove(UIElement *draggedElement,int X,int Y,IntVector2,int elemX,int elemY,int,int)
{
    IntVector2 dragCurrentPosition = IntVector2(X, Y);
    draggedElement->SetPosition(dragCurrentPosition - dragBeginPosition_);
}

void HelloGUI::HandleDragEnd(UIElement *,int,int,int,int,int,int) // For reference (not used here)
{
}

void HelloGUI::HandleClosePressed(UIElement *)
{
    engine_->Exit();
}

void HelloGUI::HandleControlClicked(UIElement *clicked,int, int, int, unsigned, int)
{
    // Get the Text control acting as the Window's title
    Text* windowTitle = static_cast<Text*>(window_->GetChild("WindowTitle", true));

    QString name = "...?";
    if (clicked)
    {
        // Get the name of the control that was clicked
        name = clicked->GetName();
    }

    // Update the Window's title text
    windowTitle->SetText("Hello " + name + "!");
}
