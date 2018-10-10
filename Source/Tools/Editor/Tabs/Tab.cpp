//
// Copyright (c) 2018 Rokas Kupstys
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
#include "Tab.h"
#include "Editor.h"

#include <Lutefisk3D/Core/ProcessUtils.h>
#include <Lutefisk3D/Input/Input.h>


namespace Urho3D
{


Tab::Tab(Context* context)
    : Object(context)
    , inspector_(context)
{
    SetID(GenerateUUID());
    connect(getEditorInstance(),&Editor::EditorProjectSaving,[&](void *root_ptr) {
        JSONValue& root = *((JSONValue*)root_ptr);
        auto& tabs = root["tabs"];
        JSONValue tab;
        OnSaveProject(tab);
        tabs.Push(tab);
    });
}

void Tab::Initialize(const QString& title, const Vector2& initSize, ui::DockSlot initPosition, const QString& afterDockName)
{
    initialSize_ = initSize;
    placePosition_ = initPosition;
    placeAfter_ = afterDockName;
    title_ = title;
    SetID(GenerateUUID());
}

Tab::~Tab()
{
    emit getEditorInstance()->EditorTabClosed(this);
}

bool Tab::RenderWindow()
{
    Input* input = context_->m_InputSystem.get();
    if (input->IsMouseVisible())
        lastMousePosition_ = input->GetMousePosition();

    bool wasRendered = isRendered_;
    ui::SetNextDockPos(placeAfter_.isEmpty() ? nullptr : qPrintable(placeAfter_), placePosition_, ImGuiCond_FirstUseEver);
    if (ui::BeginDock(qPrintable(uniqueTitle_), &open_, windowFlags_, ToImGui(initialSize_)))
    {
        if (open_)
        {
            if (!ui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows))
            {
                if (!wasRendered)                                                                                       // Just activated
                    ui::SetWindowFocus();
                else if (input->IsMouseVisible() && ui::IsAnyMouseDown())
                {
                    if (ui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows) || ui::IsDockTabHovered())                  // Interacting
                        ui::SetWindowFocus();
                }
            }

            isActive_ = ui::IsWindowFocused() && ui::IsDockActive();
            if (ui::BeginChild("Tab Content", {0, 0}, false, windowFlags_))
            open_ = RenderWindowContent();
            ui::EndChild();
            isRendered_ = true;
        }
    }
    else
    {
        isActive_ = false;
        isRendered_ = false;
    }

    if (activateTab_)
    {
        ui::SetDockActive();
        ui::SetWindowFocus();
        open_ = true;
        isActive_ = true;
        activateTab_ = false;
    }

    ui::EndDock();

    return open_;
}

void Tab::SetTitle(const QString& title)
{
    title_ = title;
    UpdateUniqueTitle();
}

void Tab::UpdateUniqueTitle()
{
    uniqueTitle_ = title_+"###"+id_;
}

IntRect Tab::UpdateViewRect()
{
    IntRect tabRect = ToIntRect(ui::GetCurrentWindow()->InnerClipRect);
    tabRect += IntRect(0, static_cast<int>(ui::GetCursorPosY()), 0, 0);
    return tabRect;
}

void Tab::OnSaveProject(JSONValue& tab)
{
    tab["type"] = GetTypeName();
    tab["uuid"] = GetID();
}

void Tab::OnLoadProject(const JSONValue& tab)
{
    SetID(tab["uuid"].GetString());
}

void Tab::AutoPlace()
{
    QString afterTabName;
    auto placement = ui::Slot_None;
    auto tabs = getEditorInstance()->GetContentTabs();

    // Need a separate loop because we prefer consile (as per default layout) but it may come after hierarchy in tabs list.
    for (const auto& openTab : tabs)
    {
        if (openTab == this)
            continue;

        if (openTab->GetTitle() == "Console")
        {
            if (afterTabName.isEmpty())
            {
                // Place after hierarchy if no content tab exist
                afterTabName = openTab->GetUniqueTitle();
                placement = ui::Slot_Top;
            }
        }
    }

    for (const auto& openTab : tabs)
    {
        if (openTab == this)
            continue;

        if (openTab->GetTitle() == "Hierarchy")
        {
            if (afterTabName.isEmpty())
            {
                // Place after hierarchy if no content tab exist
                afterTabName = openTab->GetUniqueTitle();
                placement = ui::Slot_Right;
            }
        }
        else if (!openTab->IsUtility())
        {
            // Place after content tab
            afterTabName = openTab->GetUniqueTitle();
            placement = ui::Slot_Tab;
        }
    }

    initialSize_ = {-1, GetContextGraphics()->GetHeight() * 0.9f};
    placeAfter_ = afterTabName;
    placePosition_ = placement;
}

}
