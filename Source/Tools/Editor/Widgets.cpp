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

#include <Lutefisk3D/IO/Log.h>
#include <Lutefisk3D/Resource/ResourceCache.h>
#include <Lutefisk3D/Core/Context.h>
#include <Lutefisk3D/SystemUI/SystemUI.h>
#include <Toolbox/SystemUI/Widgets.h>
#include "EditorIconCache.h"
#include "Widgets.h"

using namespace Urho3D;

namespace ImGui
{

void Image(const QString& name)
{
    auto* icons = ui::GetSystemUI()->GetSubsystem<EditorIconCache>();

    if (auto iconData = icons->GetIconData(name))
    {
        IntRect rect = iconData->rect_;
        ResourceCache* cache = ui::GetSystemUI()->GetContext()->m_ResourceCache.get();
        auto* texture = cache->GetResource<Texture2D>(iconData->textureRef_.name_);
        ui::Image(texture, {pdpx(rect.Width()), pdpy(rect.Height())},
            {(float) rect.left_ / texture->GetWidth(), (float) rect.top_ / texture->GetHeight()},
            {(float) rect.right_ / texture->GetWidth(), (float) rect.bottom_ / texture->GetHeight()});
    }
    else
        URHO3D_LOGERROR(QString::asprintf("Editor icon \"%s\" does not exist.", qPrintable(name)));
}

bool ImageButton(const QString& name)
{
    auto* icons = ui::GetSystemUI()->GetSubsystem<EditorIconCache>();

    if (auto iconData = icons->GetIconData(name))
    {
        IntRect rect = iconData->rect_;
        ResourceCache* cache = ui::GetSystemUI()->GetContext()->m_ResourceCache.get();
        auto* texture = cache->GetResource<Texture2D>(iconData->textureRef_.name_);
        return ui::ImageButton(texture, {pdpx(rect.Width()), pdpy(rect.Height())},
            {(float) rect.left_ / texture->GetWidth(), (float) rect.top_ / texture->GetHeight()},
            {(float) rect.right_ / texture->GetWidth(), (float) rect.bottom_ / texture->GetHeight()});
    }
    else
        URHO3D_LOGERROR(QString::asprintf("Editor icon \"%s\" does not exist.", qPrintable(name)));

    return false;
}

}
