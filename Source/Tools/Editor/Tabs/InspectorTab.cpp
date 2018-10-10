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

#include "Editor.h"
#include "InspectorTab.h"
#include <ImGui/imgui_stl.h>
#include <Lutefisk3D/IO/Log.h>


namespace Urho3D
{

InspectorTab::InspectorTab(Context* context)
    : Tab(context)
{
    SetTitle("Inspector");
    isUtility_ = true;
    connect(getEditorInstance(),&Editor::EditorRenderInspector,[&](unsigned category,RefCounted *instance) {
        inspectables_[category].Update(instance);
    });
}

bool InspectorTab::RenderWindowContent()
{
    ui::PushItemWidth(-1);
    ui::InputText("###Filter", &filter_);
    ui::PopItemWidth();
    if (ui::IsItemHovered())
        ui::SetTooltip("Filter attributes by name.");

    // Handle tab switching/closing
    if (Tab* tab = getEditorInstance()->GetActiveTab())
        tabInspector_.Update(tab);

    // Render main tab inspectors
    if (tabInspector_)
        tabInspector_->RenderInspector(filter_.c_str());

    // Secondary inspectables
    for (auto i = 0; i < IC_MAX; i++)
    {
        if (inspectables_[i])
            inspectables_[i]->RenderInspector(filter_.c_str());
    }
    return true;
}

IInspectorProvider* InspectorTab::GetInspector(InspectorCategory category)
{
    if (inspectables_[category])
        return &inspectables_[category];
    return nullptr;
}


}
