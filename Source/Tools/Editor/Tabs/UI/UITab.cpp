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

#include <Lutefisk3D/Graphics/Graphics.h>
#include <Lutefisk3D/Graphics/RenderSurface.h>
#include <Lutefisk3D/IO/File.h>
#include <Lutefisk3D/IO/FileSystem.h>
#include <Lutefisk3D/IO/Log.h>
#include <Lutefisk3D/Input/Input.h>
#include <Lutefisk3D/Resource/ResourceCache.h>
#include <Toolbox/IO/ContentUtilities.h>
#include <Toolbox/SystemUI/Widgets.h>
#include <IconFontCppHeaders/IconsFontAwesome5.h>
#include <Lutefisk3D/UI/Window.h>
#include "Editor.h"
#include "Widgets.h"
#include "UITab.h"
#include "Tabs/InspectorTab.h"

using namespace ui::litterals;

namespace Urho3D
{

UITab::UITab(Context* context)
    : BaseResourceTab(context)
    , undo_(context)
{
    SetTitle("New UI Layout");
    windowFlags_ = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

    rootElement_ = new RootUIElement(context_);

    undo_.Connect(rootElement_);
    undo_.Connect(&inspector_);
    connect(&inspector_,&AttributeInspector::AttributeInspectorMenu,this,&UITab::AttributeMenu);
    connect(&inspector_,&AttributeInspector::AttributeInspectorAttribute,this,&UITab::AttributeCustomize);
    AutoLoadDefaultStyle();
}

void UITab::RenderHierarchy()
{
    auto oldSpacing = ui::GetStyle().IndentSpacing;
    ui::GetStyle().IndentSpacing = 10;
    RenderNodeTree(rootElement_);
    ui::GetStyle().IndentSpacing = oldSpacing;
}

void UITab::RenderNodeTree(UIElement* element)
{
    SharedPtr<UIElement> elementRef(element);
    QString name = element->GetName();
    QString type = element->GetTypeName();
    QString tooltip = "Type: " + type;
    if (name.isEmpty())
        name = type;
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;
    bool isInternal = element->IsInternal();
    if (isInternal && !showInternal_)
        return;
    else
        flags |= ImGuiTreeNodeFlags_DefaultOpen;

    if (showInternal_)
        tooltip += QString("\nInternal: ") + (isInternal ? "true" : "false");

    if (element == selectedElement_)
        flags |= ImGuiTreeNodeFlags_Selected;

    ui::Image(element->GetTypeName());
    ui::SameLine();

    auto treeExpanded = ui::TreeNodeEx(element, flags, "%s", qPrintable(name));

    if (ui::BeginDragDropSource())
    {
        ui::SetDragDropVariant("ptr", (void*)element);
        ui::Text("%s", qPrintable(name));
        ui::EndDragDropSource();
    }

    if (ui::BeginDragDropTarget())
    {
        // Reparent by drag&drop, insert as first item
        const Variant& payload = ui::AcceptDragDropVariant("ptr");
        if (!payload.IsEmpty())
        {
            SharedPtr<UIElement> child((UIElement*)payload.GetVoidPtr());
            if (child.NotNull() && child != element)
            {
                child->Remove();    // Needed for reordering under the same parent.
                element->InsertChild(0, child);
            }
        }
        ui::EndDragDropTarget();
    }

    if (treeExpanded)
    {
        if (ui::IsItemHovered())
            ui::SetTooltip("%s", qPrintable(tooltip));

        if (ui::IsItemHovered())
        {
            if (ui::IsMouseClicked(0) || ui::IsMouseClicked(2))
            {
                SelectItem(element);
                if (ui::IsMouseClicked(2))
                    ui::OpenPopup("Element Context Menu");
            }
        }

        RenderElementContextMenu();

        // Context menu may delete this element
        bool wasDeleted = (flags & ImGuiTreeNodeFlags_Selected) && selectedElement_.Null();
        if (!wasDeleted)
        {
            // Do not use element->GetChildren() because child may be deleted during this loop.
            std::vector<UIElement*> children;
            element->GetChildren(children);
            for (const auto& child: children)
                RenderNodeTree(child);
        }

        ui::TreePop();
    }

    ImRect bb{ui::GetItemRectMin(), ui::GetItemRectMax()};
    bb.Min.y = bb.Max.y;
    bb.Max.y += 2_dpy;
    if (ui::BeginDragDropTargetCustom(bb, ui::GetID("reorder")))
    {
        // Reparent by drag&drop between elements, insert after current item
        const Variant& payload = ui::AcceptDragDropVariant("ptr");
        if (!payload.IsEmpty())
        {
            SharedPtr<UIElement> child((UIElement*)payload.GetVoidPtr());
            if (child.NotNull() && child != element)
            {
                child->Remove();    // Needed for reordering under the same parent.
                auto index = element->GetParent()->FindChild(element) + 1;
                element->GetParent()->InsertChild(index, child);
            }
        }
        ui::EndDragDropTarget();
    }
}

void UITab::RenderInspector(const char* filter)
{
    if (auto selected = GetSelected())
        inspector_.RenderAttributes(selected, filter);
}

bool UITab::RenderWindowContent()
{
    RenderToolbarButtons();
    IntRect tabRect = UpdateViewRect();

    ui::SetCursorScreenPos(ToImGui(tabRect.Min()));
    ui::Image(texture_, ToImGui(tabRect.Size()));

    if (auto selected = GetSelected())
    {
        // Render element selection rect, resize handles, and handle element transformations.
        IntRect delta;
        IntRect screenRect(selected->GetScreenPosition() + tabRect.Min(), selected->GetScreenPosition() + selected->GetSize() + tabRect.Min());
        ui::TransformSelectorFlags flags = ui::TSF_NONE;
        if (hideResizeHandles_)
            flags |= ui::TSF_HIDEHANDLES;
        if (selected->GetMinSize().x_ == selected->GetMaxSize().x_)
            flags |= ui::TSF_NOHORIZONTAL;
        if (selected->GetMinSize().y_ == selected->GetMaxSize().y_)
            flags |= ui::TSF_NOVERTICAL;

        struct State
        {
            bool resizeActive_ = false;
            IntVector2 resizeStartPos_;
            IntVector2 resizeStartSize_;
        };
        auto* s = ui::GetUIState<State>();

        if (ui::TransformRect(screenRect, delta, flags))
        {
            if (!s->resizeActive_)
            {
                s->resizeActive_ = true;
                s->resizeStartPos_ = selected->GetPosition();
                s->resizeStartSize_ = selected->GetSize();
            }
            selected->SetPosition(selected->GetPosition() + delta.Min());
            selected->SetSize(selected->GetSize() + delta.Size());
        }

        if (s->resizeActive_ && !ui::IsItemActive())
        {
            s->resizeActive_ = false;
            undo_.Track<Undo::EditAttributeAction>(selected, "Position", s->resizeStartPos_, selected->GetPosition());
            undo_.Track<Undo::EditAttributeAction>(selected, "Size", s->resizeStartSize_, selected->GetSize());
        }
    }

    RenderRectSelector();

    return true;
}

void UITab::RenderToolbarButtons()
{
    auto& style = ui::GetStyle();
    auto oldRounding = style.FrameRounding;
    style.FrameRounding = 0;

    if (ui::EditorToolbarButton(ICON_FA_SAVE, "Save"))
        SaveResource();

    ui::SameLine(0, 3.f);

//    if (ui::EditorToolbarButton(ICON_FA_UNDO, "Undo"))
//        undo_.Undo();
//    if (ui::EditorToolbarButton(ICON_FA_REDO, "Redo"))
//        undo_.Redo();

    ui::SameLine(0, 3.f);

    ui::Checkbox("Show Internal", &showInternal_);
    ui::SameLine();
    ui::Checkbox("Hide Resize Handles", &hideResizeHandles_);

    style.FrameRounding = oldRounding;
}

void UITab::OnActiveUpdate()
{
    Input* input = context_->m_InputSystem.get();
    if (!ui::IsAnyItemActive())
    {
        if (auto selected = GetSelected())
        {
            if (input->GetKeyPress(KEY_DELETE))
            {
                selected->Remove();
                SelectItem(nullptr);    // Undo system still holds a reference to removed element therefore we must
                                        // manually clear selectedElement_
            }
        }
    }

    if (!ui::IsAnyItemActive() && !ui::IsAnyItemHovered())
    {
        if (input->GetMouseButtonPress(MOUSEB_LEFT) || input->GetMouseButtonPress(MOUSEB_RIGHT))
        {
            auto pos = input->GetMousePosition();
            auto clicked = context_->m_UISystem->GetElementAt(pos, false);
            if (!clicked && rootElement_->GetCombinedScreenRect().IsInside(pos) == INSIDE && !ui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow))
                clicked = rootElement_;

            if (clicked)
            {
                SelectItem(clicked);

                if (input->GetMouseButtonPress(MOUSEB_RIGHT))
                    ui::OpenPopup("Element Context Menu");
            }
        }
    }

    RenderElementContextMenu();
}

IntRect UITab::UpdateViewRect()
{
    if (texture_.Null())
    {
        // TODO: Stinks. These need to be initialized after at least one SystemUI frame has rendered. However project may be loaded from command line and that would call initializing code too early.
        texture_ = new Texture2D(context_);
        texture_->SetFilterMode(FILTER_BILINEAR);
        texture_->SetAddressMode(COORD_U, ADDRESS_CLAMP);
        texture_->SetAddressMode(COORD_V, ADDRESS_CLAMP);
        texture_->SetNumLevels(1);
        rootElement_->SetRenderTexture(texture_);
        rootElement_->SetEnabled(true);
    }

    IntRect rect = BaseClassName::UpdateViewRect();

    if (rect.Width() != texture_->GetWidth() || rect.Height() != texture_->GetHeight())
    {
        if (texture_->SetSize(rect.Width(), rect.Height(), context_->m_Graphics->GetRGBAFormat(),
                              TEXTURE_RENDERTARGET))
        {
            rootElement_->SetSize(rect.Width(), rect.Height());
            rootElement_->SetOffset(rect.Min());
            texture_->GetRenderSurface()->SetUpdateMode(SURFACE_UPDATEALWAYS);
        }
        else
            URHO3D_LOGERROR("UITab: resizing texture failed.");
    }

    return rect;
}

bool UITab::LoadResource(const QString& resourcePath)
{
    if (!BaseClassName::LoadResource(resourcePath))
        return false;

    if (GetContentType(resourcePath) != CTYPE_UILAYOUT)
    {
        URHO3D_LOGERRORF("%s is not a UI layout.", qPrintable(resourcePath));
        return false;
    }

    undo_.Clear();
    undo_.SetTrackingEnabled(false);

    auto cache = context_->m_ResourceCache.get();
    rootElement_->RemoveAllChildren();

    UIElement* layoutElement = nullptr;
    if (resourcePath.endsWith(".xml"))
    {
        SharedPtr<XMLFile> file(cache->GetResource<XMLFile>(resourcePath));
        if (file.NotNull())
        {
            QString type = file->GetRoot().GetAttribute("type");
            if (type.isEmpty())
                type = "UIElement";
            auto* child = rootElement_->CreateChild(StringHash(type));
            if (child->LoadXML(file->GetRoot()))
                layoutElement = child;
            else
                child->Remove();
        }
        else
        {
            URHO3D_LOGERRORF("Loading file %s failed.", qPrintable(resourcePath));
            return false;
        }
    }
    else if (resourcePath.endsWith(".json"))
    {
        URHO3D_LOGERROR("Unsupported format.");
        return false;
    }
    else if (resourcePath.endsWith(".ui"))
    {
        URHO3D_LOGERROR("Unsupported format.");
        return false;
    }

    if (layoutElement != nullptr)
    {
        layoutElement->SetStyleAuto();

        // Must be disabled because it interferes with ui element resizing
        if (auto window = dynamic_cast<Window*>(layoutElement))
        {
            window->SetMovable(false);
            window->SetResizable(false);
        }
    }
    else
    {
        URHO3D_LOGERRORF("Loading UI layout %s failed.", qPrintable(resourcePath));
        return false;
    }

    undo_.SetTrackingEnabled(true);
    return true;
}

bool UITab::SaveResource()
{
    if (!BaseClassName::SaveResource())
        return false;

    if (rootElement_->GetNumChildren() < 1)
        return false;

    auto styleFile = rootElement_->GetDefaultStyle();
    if (styleFile == nullptr)
        return false;

    ResourceCache* cache = context_->m_ResourceCache.get();
    QString savePath = cache->GetResourceFileName(resourceName_);

    if (resourceName_.endsWith(".xml"))
    {
        XMLFile xml(context_);
        XMLElement root = xml.CreateRoot("element");
        if (rootElement_->GetChild(0)->SaveXML(root))
        {
            // Remove internal UI elements
            auto result = root.SelectPrepared(XPathQuery("//element[@internal=\"true\"]"));
            for (auto el = result.FirstResult(); el.NotNull(); el = el.NextResult())
            {
                // Remove only top level internal elements.
                bool internalParent = false;
                auto parent = el.GetParent();
                do
                {
                    internalParent = parent.HasAttribute("internal") && parent.GetAttribute("internal") == "true";
                    parent = parent.GetParent();
                } while (!internalParent && parent.NotNull());

                if (!internalParent)
                    el.Remove();
            }

            // Remove style="none"
            result = root.SelectPrepared(XPathQuery("//element[@style=\"none\"]"));
            for (auto el = result.FirstResult(); el.NotNull(); el = el.NextResult())
                el.RemoveAttribute("style");

            // TODO: remove attributes with values matching style
            // TODO: remove attributes with default values

            File saveFile(context_, savePath, FILE_WRITE);
            if (!xml.Save(saveFile))
                return false;
        }
        else
            return false;
    }
    else if (resourceName_.endsWith(".json"))
    {
        URHO3D_LOGERROR("Unsupported format.");
        return false;
    }
    else if (resourceName_.endsWith(".ui"))
    {
        URHO3D_LOGERROR("Unsupported format.");
        return false;
    }

    // Save style
    savePath = cache->GetResourceFileName(styleFile->GetName());
    File saveFile(context_, savePath, FILE_WRITE);
    if (!styleFile->Save(saveFile))
        return false;

    emit getEditorInstance()->EditorResourceSaved();
    return true;
}

UIElement* UITab::GetSelected() const
{
    // Can not select root widget
    if (selectedElement_ == rootElement_)
        return nullptr;

    return selectedElement_;
}

void UITab::SelectItem(UIElement* current)
{
    if (current == nullptr)
        textureSelectorAttribute_.clear();

    selectedElement_ = current;
}

void UITab::AutoLoadDefaultStyle()
{
    styleNames_.clear();
    auto cache = context_->m_ResourceCache.get();
    auto fs = context_->m_FileSystem.get();
    for (const auto& dir: cache->GetResourceDirs())
    {
        QStringList items;
        fs->ScanDir(items, dir + "UI", "", SCAN_FILES, false);

        for (const auto& fileName : items)
        {
            auto resourcePath = dir + "UI/" + fileName;
            // Icons file is also a style file. Without this ugly workaround sometimes wrong style gets applied.
            if (GetContentType(resourcePath) == CTYPE_UISTYLE && !resourcePath.endsWith("Icons.xml"))
            {
                auto* style = cache->GetResource<XMLFile>(resourcePath);
                rootElement_->SetDefaultStyle(style);

                auto styles = style->GetRoot().SelectPrepared(XPathQuery("/elements/element"));
                for (auto i = 0; i < styles.Size(); i++)
                {
                    auto type = styles[i].GetAttribute("type");
                    if (type.length() && !styleNames_.contains(type) &&
                        styles[i].GetAttribute("auto").toLower() == "false")
                        styleNames_.push_back(type);
                }
                break;
            }
        }
    }
    qSort(styleNames_);
}

void UITab::RenderElementContextMenu()
{
    if (ui::BeginPopup("Element Context Menu"))
    {
        if (ui::BeginMenu("Create Child"))
        {
            auto components = getEditorInstance()->GetObjectsByCategory("UI");
            qSort(components.begin(), components.end());

            for (const QString& component : components)
            {
                // TODO: element creation with custom styles more usable.
                if (context_->m_InputSystem->GetQualifierDown(QUAL_SHIFT))
                {
                    ui::Image(component);
                    ui::SameLine();
                    if (ui::BeginMenu(qPrintable(component)))
                    {
                        for (auto j = 0; j < styleNames_.size(); j++)
                        {
                            if (ui::MenuItem(qPrintable(styleNames_[j])))
                            {
                                SelectItem(selectedElement_->CreateChild(StringHash(component)));
                                selectedElement_->SetStyle(styleNames_[j]);
                            }
                        }
                        ui::EndMenu();
                    }
                }
                else
                {
                    ui::Image(component);
                    ui::SameLine();
                    if (ui::MenuItem(qPrintable(component)))
                    {
                        SelectItem(selectedElement_->CreateChild(StringHash(component)));
                        selectedElement_->SetStyleAuto();
                    }
                }
            }
            ui::EndMenu();
        }

        if (auto selected = GetSelected())
        {
            if (ui::MenuItem("Delete Element"))
            {
                selected->Remove();
                SelectItem(nullptr);
            }

            if (ui::MenuItem("Bring To Front"))
                selected->BringToFront();
        }
        ui::EndPopup();
    }
}

void UITab::OnSaveProject(JSONValue& tab)
{
    BaseClassName::OnSaveProject(tab);
}

void UITab::OnLoadProject(const JSONValue& tab)
{
    BaseClassName::OnLoadProject(tab);
}

QString UITab::GetAppliedStyle(UIElement* element)
{
    if (element == nullptr)
        element = selectedElement_;

    if (element == nullptr)
        return "";

    auto appliedStyle = selectedElement_->GetAppliedStyle();
    if (appliedStyle.isEmpty())
        appliedStyle = selectedElement_->GetTypeName();
    return appliedStyle;
}

void UITab::RenderRectSelector()
{
    auto* selected = dynamic_cast<BorderImage*>(GetSelected());

    if (textureSelectorAttribute_.isEmpty() || selected == nullptr)
        return;

    struct State
    {
        bool isResizing_ = false;
        IntRect startRect_;
        int textureScale_ = 1;
        int windowFlags_ = ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoScrollbar;
        IntRect rectWindowDeltaAccumulator_;
    };
    auto* s = ui::GetUIState<State>();

    bool open = true;
    auto texture = selected->GetTexture();
    texture->SetFilterMode(FILTER_NEAREST);    // Texture is better visible this way when zoomed in.
    auto padding = ImGui::GetStyle().WindowPadding;
    ui::SetNextWindowPos(ImVec2(texture->GetWidth() + padding.x * 2, texture->GetHeight() + padding.y * 2),
        ImGuiCond_FirstUseEver);
    if (ui::Begin("Select Rect", &open, s->windowFlags_))
    {
        ui::SliderInt("Zoom", &s->textureScale_, 1, 5);
        auto windowPos = ui::GetWindowPos();
        auto imagePos = ui::GetCursorPos();
        ui::Image(texture, ImVec2(texture->GetWidth() * s->textureScale_,
            texture->GetHeight() * s->textureScale_));

        // Disable dragging of window if mouse is hovering texture.
        if (ui::IsItemHovered())
            s->windowFlags_ |= ImGuiWindowFlags_NoMove;
        else
            s->windowFlags_ &= ~ImGuiWindowFlags_NoMove;

        IntRect rect = selectedElement_->GetAttribute(textureSelectorAttribute_).GetIntRect();
        IntRect originalRect = rect;
        // Upscale selection rect if texture is upscaled.
        rect *= s->textureScale_;

        ui::TransformSelectorFlags flags = ui::TSF_NONE;
        if (hideResizeHandles_)
            flags |= ui::TSF_HIDEHANDLES;

        IntRect screenRect(
            rect.Min() + ToIntVector2(imagePos) + ToIntVector2(windowPos),
            IntVector2(rect.right_ - rect.left_, rect.bottom_ - rect.top_)
        );
        // Essentially screenRect().Max() += screenRect().Min()
        screenRect.bottom_ += screenRect.top_;
        screenRect.right_ += screenRect.left_;

        IntRect delta;
        if (ui::TransformRect(screenRect, delta, flags))
        {
            if (!s->isResizing_)
            {
                s->isResizing_ = true;
                s->startRect_ = originalRect;
            }
            // Accumulate delta value. This is required because resizing upscaled rect does not work
            // with small increments when rect values are integers.
            s->rectWindowDeltaAccumulator_ += delta;
        }

        if (ui::IsItemActive())
        {
            // Downscale and add accumulated delta to the original rect value
            rect = originalRect + s->rectWindowDeltaAccumulator_ / s->textureScale_;

            // If downscaled rect size changed compared to original value - set attribute and
            // reset delta accumulator.
            if (rect != originalRect)
            {
                selectedElement_->SetAttribute(textureSelectorAttribute_, rect);
                // Keep remainder in accumulator, otherwise resizing will cause cursor to drift from
                // the handle over time.
                s->rectWindowDeltaAccumulator_.left_ %= s->textureScale_;
                s->rectWindowDeltaAccumulator_.top_ %= s->textureScale_;
                s->rectWindowDeltaAccumulator_.right_ %= s->textureScale_;
                s->rectWindowDeltaAccumulator_.bottom_ %= s->textureScale_;
            }
        }
        else if (s->isResizing_)
        {
            s->isResizing_ = false;
            undo_.Track<Undo::EditAttributeAction>(selected, textureSelectorAttribute_, s->startRect_,
                selected->GetAttribute(textureSelectorAttribute_));
        }
    }
    ui::End();

    if (!open)
        textureSelectorAttribute_.clear();
}

Variant UITab::GetVariantFromXML(const XMLElement& attribute, const AttributeInfo& info) const
{
    Variant value = attribute.GetVariantValue(info.enumNames_ ? VAR_STRING : info.type_);
    if (info.enumNames_)
    {
        for (auto i = 0; info.enumNames_[i]; i++)
        {
            if (value.GetString() == info.enumNames_[i])
            {
                value = i;
                break;
            }
        }
    }
    return value;
}

void UITab::GetStyleData(const AttributeInfo& info, XMLElement& style, XMLElement& attribute, Variant& value)
{
    auto styleFile = selectedElement_->GetDefaultStyle();
    if (styleFile == nullptr)
        return;

    static XPathQuery xpAttribute("attribute[@name=$name]", "name:String");
    static XPathQuery xpStyle("/elements/element[@type=$type]", "type:String");

    value = Variant();
    xpAttribute.SetVariable("name", info.name_);

    auto styleName = GetAppliedStyle();

    do
    {
        // Get current style
        xpStyle.SetVariable("type", styleName);
        style = styleFile->GetRoot().SelectSinglePrepared(xpStyle);
        // Look for attribute in current style
        attribute = style.SelectSinglePrepared(xpAttribute);
        // Go up in style hierarchy
        styleName = style.GetAttribute("Style");
    } while (attribute.IsNull() && !styleName.isEmpty() && !style.IsNull());


    if (!attribute.IsNull() && attribute.GetAttribute("type") != "None")
        value = GetVariantFromXML(attribute, info);
}

void UITab::AttributeMenu(Serializable *item,const AttributeInfo *info)
{
    auto selected = GetSelected();
    if (!selected)
        return;
    
    Variant value = item->GetAttribute(info->name_);
    XMLElement styleAttribute;
    XMLElement styleXml;
    Variant styleVariant;
    GetStyleData(*info, styleXml, styleAttribute, styleVariant);

    if (styleVariant != value)
    {
        if (!styleVariant.IsEmpty())
        {
            if (ui::MenuItem("Reset to style"))
            {
                item->SetAttribute(info->name_, styleVariant);
                item->ApplyAttributes();
                undo_.Track<Undo::EditAttributeAction>(item, info->name_, value, item->GetAttribute(info->name_));
            }
        }

        if (styleXml.NotNull())
        {
            if (ui::MenuItem("Save to style"))
            {
                if (styleAttribute.IsNull())
                {
                    styleAttribute = styleXml.CreateChild("attribute");
                    styleAttribute.SetAttribute("name", info->name_);
                }
                // To save some writing undo system performs value update action as well.
                undo_.Track<Undo::EditUIStyleAction>(selected, styleAttribute, value);
            }
        }
    }

    if (styleAttribute.NotNull() && !styleVariant.IsEmpty())
    {
        if (ui::MenuItem("Remove from style"))
        {
            // To save some writing undo system performs value update action as well. Empty variant means removal.
            undo_.Track<Undo::EditUIStyleAction>(selected, styleAttribute, Variant::EMPTY);
        }
    }

    if (info->type_ == VAR_INTRECT && dynamic_cast<BorderImage*>(selected) != nullptr)
    {
        if (ui::MenuItem("Select in UI Texture"))
            textureSelectorAttribute_ = info->name_;
    }
}

void UITab::AttributeCustomize(Serializable *item,const AttributeInfo *info,Color *c,bool *hidden,QString *tooltip)
{
    if (GetSelected() == nullptr)
        return;

    Variant value = item->GetAttribute(info->name_);
    XMLElement styleAttribute;
    XMLElement styleXml;
    Variant styleVariant;
    GetStyleData(*info, styleXml, styleAttribute, styleVariant);

    if (!styleVariant.IsEmpty())
    {
        if (styleVariant == value)
        {
            *c = Color::GRAY;
            *tooltip = "Value inherited from style.";
        }
        else
        {
            *c = Color::GREEN;
            *tooltip = "Style value was modified.";
        }
    }
}

void UITab::OnFocused()
{
}

}
