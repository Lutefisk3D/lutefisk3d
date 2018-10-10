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

#include "UIDrag.h"

#include <Lutefisk3D/UI/Button.h>
#include <Lutefisk3D/Core/CoreEvents.h>
#include <Lutefisk3D/UI/Font.h>
#include <Lutefisk3D/UI/Text.h>
#include <Lutefisk3D/UI/UIEvents.h>

URHO3D_DEFINE_APPLICATION_MAIN(UIDrag)

UIDrag::UIDrag(Context* context) :
    Sample("UIDrag",context)
{
}

void UIDrag::Start()
{
    // Execute base class startup
    Sample::Start();

    // Set mouse visible
    QString platform = GetPlatform();
    if (platform != "Android" && platform != "iOS")
        GetContext()->m_InputSystem.get()->SetMouseVisible(true);

    // Create the UI content
    CreateGUI();
    CreateInstructions();

    // Hook up to the frame update events
    SubscribeToEvents();
}

void UIDrag::CreateGUI()
{
    ResourceCache* cache = GetContext()->m_ResourceCache.get();
    UI* ui = GetContext()->m_UISystem.get();

    UIElement* root = ui->GetRoot();
    // Load the style sheet from xml
    root->SetDefaultStyle(cache->GetResource<XMLFile>("UI/DefaultStyle.xml"));

    for (int i=0; i < 10; i++)
    {
        Button* b = new Button(GetContext());
        root->AddChild(b);
        // Reference a style from the style sheet loaded earlier:
        b->SetStyle("Button");
        b->SetSize(300, 100);
        b->SetPosition(IntVector2(50*i, 50*i));

        if (i % 2 == 0)
            b->AddTag("SomeTag");
        b->dragMove.Connect(this,&UIDrag::HandleDragMove);
        b->dragBegin.Connect(this,&UIDrag::HandleDragBegin);
        b->dragCancel.Connect(this,&UIDrag::HandleDragCancel);
        b->dragEnd.Connect(this,&UIDrag::HandleDragEnd);

        {
            Text* t = new Text(GetContext());
            b->AddChild(t);
            t->SetStyle("Text");
            t->SetHorizontalAlignment(HA_CENTER);
            t->SetVerticalAlignment(VA_CENTER);
            t->SetName("Text");
        }

        {
            Text* t = new Text(GetContext());
            b->AddChild(t);
            t->SetStyle("Text");
            t->SetName("Event Touch");
            t->SetHorizontalAlignment(HA_CENTER);
            t->SetVerticalAlignment(VA_BOTTOM);
        }

        {
            Text* t = new Text(GetContext());
            b->AddChild(t);
            t->SetStyle("Text");
            t->SetName("Num Touch");
            t->SetHorizontalAlignment(HA_CENTER);
            t->SetVerticalAlignment(VA_TOP);
        }
    }

    for (int i = 0; i < 10; i++)
    {
        Text* t = new Text(GetContext());
        root->AddChild(t);
        t->SetStyle("Text");
        t->SetName("Touch "+ QString::number(i));
        t->SetVisible(false);
    }
}

void UIDrag::CreateInstructions()
{
    ResourceCache* cache = GetContext()->m_ResourceCache.get();
    UI* ui = GetContext()->m_UISystem.get();

    // Construct new Text object, set string to display and font to use
    Text* instructionText = ui->GetRoot()->CreateChild<Text>();
    instructionText->SetText("Drag on the buttons to move them around.\n"
                             "Touch input allows also multi-drag.\n"
                             "Press SPACE to show/hide tagged UI elements.");
    instructionText->SetFont(cache->GetResource<Font>("Fonts/Anonymous Pro.ttf"), 15);
    instructionText->SetTextAlignment(HA_CENTER);

    // Position the text relative to the screen center
    instructionText->SetHorizontalAlignment(HA_CENTER);
    instructionText->SetVerticalAlignment(VA_CENTER);
    instructionText->SetPosition(0, ui->GetRoot()->GetHeight() / 4);
}

void UIDrag::SubscribeToEvents()
{
    g_coreSignals.update.Connect(this,&UIDrag::HandleUpdate);
}

void UIDrag::HandleDragBegin(UIElement *elem,int lx,int ly,int,int,int buttons,int btncount)
{
    Button* element = (Button*)elem;

    IntVector2 p = element->GetPosition();
    element->SetVar("START", p);
    element->SetVar("DELTA", IntVector2(p.x_ - lx, p.y_ - ly));
    element->SetVar("BUTTONS", buttons);

    Text* t = (Text*)element->GetChild(QString("Text"));
    t->SetText("Drag Begin Buttons: " + QString::number(buttons));

    t = (Text*)element->GetChild(QString("Num Touch"));
    t->SetText("Number of buttons: " + QString::number(btncount));
}

void UIDrag::HandleDragMove(UIElement *element,int x,int y,IntVector2 d,int,int,int buttons,int)
{
    Button* button = (Button*)element;
    int X = x + d.x_;
    int Y = y + d.y_;
    int BUTTONS = button->GetVar("BUTTONS").GetInt();

    Text* t = (Text*)button->GetChild(QString("Event Touch"));
    t->SetText("Drag Move Buttons: " + QString::number(buttons));

    if (buttons == BUTTONS)
        button->SetPosition(IntVector2(X, Y));
}

void UIDrag::HandleDragCancel(UIElement *elem,int,int,int,int,int,int)
{
    Button* element = (Button*)elem;
    IntVector2 P = element->GetVar("START").GetIntVector2();
    element->SetPosition(P);
}

void UIDrag::HandleDragEnd(UIElement *elem,int,int,int,int,int,int)
{
    Button* element = (Button*)elem;
}

void UIDrag::HandleUpdate(float timeStep)
{
    UI* ui = GetContext()->m_UISystem.get();
    UIElement* root = ui->GetRoot();

    Input* input = GetContext()->m_InputSystem.get();

    if (input->GetKeyPress(KEY_SPACE))
    {
        std::vector<UIElement*> elements;
        root->GetChildrenWithTag(elements, "SomeTag");
        for (std::vector<UIElement*>::const_iterator i = elements.begin(); i != elements.end(); ++i)
        {
            UIElement* element = *i;
            element->SetVisible(!element->IsVisible());
        }
    }
}
