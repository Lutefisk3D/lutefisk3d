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

#include <limits>

#include <Lutefisk3D/SystemUI/SystemUI.h>
#include <Lutefisk3D/Core/StringUtils.h>
#include <Lutefisk3D/Core/CoreEvents.h>
#include <Lutefisk3D/Core/StringHashRegister.h>
#include <Lutefisk3D/Scene/Serializable.h>
#include <Lutefisk3D/Scene/Scene.h>
#include <Lutefisk3D/Resource/ResourceCache.h>
#include <Lutefisk3D/IO/FileSystem.h>
#include <Lutefisk3D/IO/Log.h>
#include <Lutefisk3D/Input/Input.h>
#include <Lutefisk3D/Graphics/Material.h>
#include <Lutefisk3D/Graphics/Technique.h>
#include <Lutefisk3D/Graphics/Camera.h>
#include <Lutefisk3D/Graphics/Model.h>
#include <Lutefisk3D/Graphics/Renderer.h>
#include <Lutefisk3D/Graphics/RenderPath.h>
#include "AttributeInspector.h"
#include "ImGuiDock.h"
#include "Widgets.h"

#include <IconFontCppHeaders/IconsFontAwesome5.h>
#include <ImGui/imgui_internal.h>
#include <ImGui/imgui_stl.h>
#include <Lutefisk3D/Graphics/StaticModel.h>
#include <Lutefisk3D/Graphics/Octree.h>
#include <Lutefisk3D/Graphics/Graphics.h>
#include <Graphics/SceneView.h>

using namespace ui::litterals;

namespace Urho3D
{

VariantType supportedVariantTypes[] = {
    VAR_INT,
    VAR_BOOL,
    VAR_FLOAT,
    VAR_VECTOR2,
    VAR_VECTOR3,
    VAR_VECTOR4,
    VAR_QUATERNION,
    VAR_COLOR,
    VAR_STRING,
    //VAR_BUFFER,
    //VAR_RESOURCEREF,
    //VAR_RESOURCEREFLIST,
    //VAR_VARIANTVECTOR,
    //VAR_VARIANTMAP,
    VAR_INTRECT,
    VAR_INTVECTOR2,
    VAR_MATRIX3,
    VAR_MATRIX3X4,
    VAR_MATRIX4,
    VAR_DOUBLE,
    //VAR_STRINGVECTOR,
    VAR_RECT,
    VAR_INTVECTOR3,
    VAR_INT64,
};
const int MAX_SUPPORTED_VAR_TYPES = (sizeof(supportedVariantTypes)/sizeof(supportedVariantTypes[0]));

const char* supportedVariantNames[] = {
    "Int",
    "Bool",
    "Float",
    "Vector2",
    "Vector3",
    "Vector4",
    "Quaternion",
    "Color",
    "String",
    //"Buffer",
    //"ResourceRef",
    //"ResourceRefList",
    //"VariantVector",
    //"VariantMap",
    "IntRect",
    "IntVector2",
    "Matrix3",
    "Matrix3x4",
    "Matrix4",
    "Double",
    //"StringVector",
    "Rect",
    "IntVector3",
    "Int64",
};
static float buttonWidth()
{
    return 26_dpx;  // TODO: this should not exist
}
bool RenderResourceRef(Object* eventNamespace, StringHash type, const QString& name, QString& result)
{
    SharedPtr<Resource> resource;
    auto returnValue = false;
    QByteArray data = name.toUtf8();
    UI_ITEMWIDTH(eventNamespace != nullptr ? -44_dpx : -buttonWidth())
        ui::InputText("", data.data(), data.size(), ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_ReadOnly);

    if (eventNamespace != nullptr)
    {
        bool dropped = false;
        if (ui::BeginDragDropTarget())
        {
            const Variant& payload = ui::AcceptDragDropVariant("path");
            if (!payload.IsEmpty())
            {
                resource = eventNamespace->GetCache()->GetResource(type, payload.GetString());
                dropped = resource.NotNull();
            }
            ui::EndDragDropTarget();
        }
        ui::SetHelpTooltip("Drag resource here.");

        if (dropped)
        {
            result = resource->GetName();
            returnValue = true;
        }

        ui::SameLine(VAR_RESOURCEREF);
        if (ui::IconButton(ICON_FA_CROSSHAIRS))
        {
            assert(false);
            //eventNamespace->SendEvent(E_INSPECTORLOCATERESOURCE, InspectorLocateResource::P_NAME, name);
        }
        ui::SetHelpTooltip("Locate resource.");
    }

    ui::SameLine(VAR_RESOURCEREF);
    if (ui::IconButton(ICON_FA_TRASH))
    {
        result.clear();
        returnValue = true;
    }
    ui::SetHelpTooltip("Stop using resource.");

    return returnValue;
}

bool AttributeInspector::RenderSingleAttribute(Object* eventNamespace, const AttributeInfo* info, Variant& value)
{
    const float floatMin = -std::numeric_limits<float>::infinity();
    const float floatMax = std::numeric_limits<float>::infinity();
    const double doubleMin = -std::numeric_limits<double>::infinity();
    const double doubleMax = std::numeric_limits<double>::infinity();
    const float floatStep = 0.01f;
    const float power = 3.0f;

    bool modified = false;
    auto comboValuesNum = 0;
    if (info != nullptr)
    {
        for (; info->enumNames_ && info->enumNames_[++comboValuesNum];);
    }

    if (comboValuesNum > 0 && info != nullptr)
    {
        int current = 0;
        if (info->type_ == VAR_INT)
            current = value.GetInt();
        else if (info->type_ == VAR_STRING)
            current = GetStringListIndex(value.GetString(), info->enumNames_, 0);
        else
            assert(false);

        modified |= ui::Combo("", &current, info->enumNames_, comboValuesNum);
        if (modified)
        {
            if (info->type_ == VAR_INT)
                value = current;
            else if (info->type_ == VAR_STRING)
                value = info->enumNames_[current];
        }
    }
    else
    {
        switch (info ? info->type_ : value.GetType())
        {
        case VAR_NONE:
            ui::TextUnformatted("None");
            break;
        case VAR_INT:
        {
            if (info && info->name_.endsWith(" Mask"))
            {
                auto v = value.GetUInt();
                modified |= ui::MaskSelector(&v);
                if (modified)
                    value = v;
            }
            else
            {
                auto v = value.GetInt();
                modified |= ui::DragInt("", &v, 1, M_MIN_INT, M_MAX_INT);
                if (modified)
                    value = v;
            }
            break;
        }
        case VAR_BOOL:
        {
            auto v = value.GetBool();
            modified |= ui::Checkbox("", &v);
            if (modified)
                value = v;
            break;
        }
        case VAR_FLOAT:
        {
            auto v = value.GetFloat();
            modified |= ui::DragFloat("", &v, floatStep, floatMin, floatMax, "%.3f", power);
            if (modified)
                value = v;
            break;
        }
        case VAR_VECTOR2:
        {
            auto& v = value.GetVector2();
            modified |= ui::DragFloat2("", const_cast<float*>(&v.x_), floatStep, floatMin, floatMax, "%.3f", power);
            ui::SetHelpTooltip("xy");
            break;
        }
        case VAR_VECTOR3:
        {
            auto& v = value.GetVector3();
            modified |= ui::DragFloat3("", const_cast<float*>(&v.x_), floatStep, floatMin, floatMax, "%.3f", power);
            ui::SetHelpTooltip("xyz");
            break;
        }
        case VAR_VECTOR4:
        {
            auto& v = value.GetVector4();
            modified |= ui::DragFloat4("", const_cast<float*>(&v.x_), floatStep, floatMin, floatMax, "%.3f", power);
            ui::SetHelpTooltip("xyzw");
            break;
        }
        case VAR_QUATERNION:
        {
            auto v = value.GetQuaternion().EulerAngles();
            modified |= ui::DragFloat3("", const_cast<float*>(&v.x_), floatStep, floatMin, floatMax, "%.3f", power);
            ui::SetHelpTooltip("xyz");
            if (modified)
                value = Quaternion(v.x_, v.y_, v.z_);
            break;
        }
        case VAR_COLOR:
        {
            auto& v = value.GetColor();
            modified |= ui::ColorEdit4("", const_cast<float*>(&v.r_));
            ui::SetHelpTooltip("rgba");
            break;
        }
        case VAR_STRING:
        {
            QString v = value.GetString();
            auto* buffer = ui::GetUIState<std::string>(v.toStdString());
            bool dirty = v.compare(buffer->c_str()) != 0;
            if (dirty)
                ui::PushStyleColor(ImGuiCol_Text, ui::GetStyle().Colors[ImGuiCol_TextDisabled]);
            modified |= ui::InputText("", buffer, ImGuiInputTextFlags_EnterReturnsTrue);
            if (dirty)
            {
                ui::PopStyleColor();
                if (ui::IsItemHovered())
                    ui::SetTooltip("Press [Enter] to commit changes.");
            }
            if (modified)
                value = QString::fromStdString(*buffer);
            break;
        }
//            case VAR_BUFFER:
        case VAR_VOIDPTR:
            ui::Text("%p", value.GetVoidPtr());
            break;
        case VAR_RESOURCEREF:
        {
            const auto& ref = value.GetResourceRef();
            auto refType = ref.type_;

            if (refType == StringHash::ZERO && info)
                refType = info->defaultValue_.GetResourceRef().type_;

            QString result;
            if (RenderResourceRef(eventNamespace, refType, ref.name_, result))
            {
                value = ResourceRef {refType, result};
                modified = true;
            }
            break;
        }
        case VAR_RESOURCEREFLIST:
        {
            auto& refList = value.GetResourceRefList();
            for (auto i = 0; i < refList.names_.size(); i++)
            {
                UI_ID(i)
                {
                    QString result;

                    auto refType = refList.type_;
                    if (refType == StringHash::ZERO && info)
                        refType = info->defaultValue_.GetResourceRef().type_;

                    modified |= RenderResourceRef(eventNamespace, refType, refList.names_[i], result);
                    if (modified)
                    {
                        ResourceRefList newRefList(refList);
                        newRefList.names_[i] = result;
                        value = newRefList;
                        break;
                    }
                }

            }
            if (refList.names_.empty())
            {
                ui::SetCursorPosY(ui::GetCursorPosY() + 5_dpy);
                ui::TextUnformatted("...");
            }
            break;
        }
//            case VAR_VARIANTVECTOR:
        case VAR_VARIANTMAP:
        {
            struct VariantMapState
            {
                std::string fieldName;
                int variantTypeIndex = 0;
                bool insertingNew = false;
            };

            ui::IdScope idScope(VAR_VARIANTMAP);

            auto* mapState = ui::GetUIState<VariantMapState>();
            auto* map = value.GetVariantMapPtr();
            if (ui::Button(ICON_FA_PLUS))
                mapState->insertingNew = true;

            if (!map->empty())
                ui::NextColumn();

            unsigned index = 0;
            for (auto it = map->begin(); it != map->end(); it++)
            {
                if (it->second.GetType() == VAR_RESOURCEREFLIST || it->second.GetType() == VAR_VARIANTMAP || it->second.GetType() == VAR_VARIANTVECTOR)
                    // TODO: Support nested collections.
                    continue;

#if LUTEFISK3D_HASH_DEBUG
                const QString& name = StringHash::GetGlobalStringHashRegister()->GetString(it->first);
                // Column-friendly indent
                ui::NewLine();
                ui::SameLine(20_dpx);
                ui::TextUnformatted(qPrintable(name.isEmpty() ? it->first.ToString() : name));
#else
                // Column-friendly indent
                ui::NewLine();
                ui::SameLine(20_dpx);
                ui::TextUnformatted(qPrintable(it->first.ToString()));
#endif

                ui::NextColumn();
                ui::IdScope entryIdScope(index++);
                UI_ITEMWIDTH(-buttonWidth()) // Space for trashcan button. TODO: trashcan goes out of screen a little for matrices.
                    modified |= RenderSingleAttribute(eventNamespace, nullptr, it->second);
                ui::SameLine(it->second.GetType());
                if (ui::Button(ICON_FA_TRASH))
                {
                    it = map->erase(it);
                    modified |= true;
                    break;
                }
                auto nextv=it;
                nextv++;
                if (nextv!=map->end())
                    ui::NextColumn();
            }

            if (mapState->insertingNew)
            {
                ui::NextColumn();
                UI_ITEMWIDTH(-1)
                    ui::InputText("###Key", &mapState->fieldName);
                ui::NextColumn();
                UI_ITEMWIDTH(-buttonWidth()) // Space for OK button
                    ui::Combo("###Type", &mapState->variantTypeIndex, supportedVariantNames, MAX_SUPPORTED_VAR_TYPES);
                ui::SameLine(0, 4_dpx);
                if (ui::Button(ICON_FA_CHECK))
                {
                    if (map->find(mapState->fieldName.c_str()) == map->end())   // TODO: Show warning about duplicate name
                    {
                        map->insert({mapState->fieldName.c_str(), Variant{supportedVariantTypes[mapState->variantTypeIndex]}});
                        mapState->fieldName.clear();
                        mapState->variantTypeIndex = 0;
                        mapState->insertingNew = false;
                        modified = true;
                    }
                }
            }
            break;
        }
        case VAR_INTRECT:
        {
            auto& v = value.GetIntRect();
            modified |= ui::DragInt4("", const_cast<int*>(&v.left_), 1, M_MIN_INT, M_MAX_INT);
            ui::SetHelpTooltip("ltbr");
            break;
        }
        case VAR_INTVECTOR2:
        {
            auto& v = value.GetIntVector2();
            modified |= ui::DragInt2("", const_cast<int*>(&v.x_), 1, M_MIN_INT, M_MAX_INT);
            ui::SetHelpTooltip("xy");
            break;
        }
        case VAR_PTR:
            ui::Text("%p (Void Pointer)", static_cast<void*>(value.GetPtr()));
            break;
        case VAR_MATRIX3:
        {
            ui::NewLine();
            auto& v = value.GetMatrix3();
            modified |= ui::DragFloat3("###m0", const_cast<float*>(&v.m00_), floatStep, floatMin, floatMax, "%.3f", power);
            ui::SetHelpTooltip("m0");
            modified |= ui::DragFloat3("###m1", const_cast<float*>(&v.m10_), floatStep, floatMin, floatMax, "%.3f", power);
            ui::SetHelpTooltip("m1");
            modified |= ui::DragFloat3("###m2", const_cast<float*>(&v.m20_), floatStep, floatMin, floatMax, "%.3f", power);
            ui::SetHelpTooltip("m2");
            break;
        }
        case VAR_MATRIX3X4:
        {
            ui::NewLine();
            auto& v = value.GetMatrix3x4();
            modified |= ui::DragFloat4("###m0", const_cast<float*>(&v.m00_), floatStep, floatMin, floatMax, "%.3f", power);
            ui::SetHelpTooltip("m0");
            modified |= ui::DragFloat4("###m1", const_cast<float*>(&v.m10_), floatStep, floatMin, floatMax, "%.3f", power);
            ui::SetHelpTooltip("m1");
            modified |= ui::DragFloat4("###m2", const_cast<float*>(&v.m20_), floatStep, floatMin, floatMax, "%.3f", power);
            ui::SetHelpTooltip("m2");
            break;
        }
        case VAR_MATRIX4:
        {
            ui::NewLine();
            auto& v = value.GetMatrix4();
            modified |= ui::DragFloat4("###m0", const_cast<float*>(&v.m00_), floatStep, floatMin, floatMax, "%.3f", power);
            ui::SetHelpTooltip("m0");
            modified |= ui::DragFloat4("###m1", const_cast<float*>(&v.m10_), floatStep, floatMin, floatMax, "%.3f", power);
            ui::SetHelpTooltip("m1");
            modified |= ui::DragFloat4("###m2", const_cast<float*>(&v.m20_), floatStep, floatMin, floatMax, "%.3f", power);
            ui::SetHelpTooltip("m2");
            modified |= ui::DragFloat4("###m3", const_cast<float*>(&v.m30_), floatStep, floatMin, floatMax, "%.3f", power);
            ui::SetHelpTooltip("m3");
            break;
        }
        case VAR_DOUBLE:
        {
            auto v = value.GetDouble();
            modified |= ui::DragScalar("", ImGuiDataType_Double, &v, floatStep, &doubleMin, &doubleMax, "%.3f", power);
            if (modified)
                value = v;
            break;
        }
        case VAR_STRINGVECTOR:
        {
            auto v = value.GetStringVector();

            // Insert new item.
            {
                auto* buffer = ui::GetUIState<std::string>();
                if (ui::InputText("", buffer, ImGuiInputTextFlags_EnterReturnsTrue))
                {
                    v.push_back(QString::fromStdString(*buffer));
                    buffer->clear();
                    modified = true;

                    // Expire buffer of this new item just in case other item already used it.
                    UI_ID(v.size())
                        ui::ExpireUIState<std::string>();
                }
                if (ui::IsItemHovered())
                    ui::SetTooltip("Press [Enter] to insert new item.");
            }

            // List of current items.
            unsigned index = 0;
            for (auto it = v.begin(); it != v.end();)
            {
                QString& sv = *it;

                ui::IdScope idScope(++index);
                auto* buffer = ui::GetUIState<std::string>(sv.toStdString());
                if (ui::Button(ICON_FA_TRASH))
                {
                    it = v.erase(it);
                    modified = true;
                    ui::ExpireUIState<std::string>();
                }
                else if (modified)
                {
                    // After modification of the vector all buffers are expired and recreated because their indexes
                    // changed. Index is used as id in this loop.
                    ui::ExpireUIState<std::string>();
                    ++it;
                }
                else
                {
                    ui::SameLine();

                    bool dirty = sv.compare(buffer->c_str()) != 0;
                    if (dirty)
                        ui::PushStyleColor(ImGuiCol_Text, ui::GetStyle().Colors[ImGuiCol_TextDisabled]);
                    modified |= ui::InputText("", buffer, ImGuiInputTextFlags_EnterReturnsTrue);
                    if (dirty)
                    {
                        ui::PopStyleColor();
                        if (ui::IsItemHovered())
                            ui::SetTooltip("Press [Enter] to commit changes.");
                    }
                    if (modified)
                        sv = QString::fromStdString(*buffer);
                    ++it;
                }
            }

            if (modified)
                value = v;

            break;
        }
        case VAR_RECT:
        {
            auto& v = value.GetRect();
            modified |= ui::DragFloat4("###minmax", const_cast<float*>(&v.min_.x_), floatStep, floatMin,
                                       floatMax, "%.3f", power);
            ui::SetHelpTooltip("min xy, max xy");
            break;
        }
        case VAR_INTVECTOR3:
        {
            auto& v = value.GetIntVector3();
            modified |= ui::DragInt3("xyz", const_cast<int*>(&v.x_), 1, M_MIN_INT, M_MAX_INT);
            ui::SetHelpTooltip("xyz");
            break;
        }
        case VAR_INT64:
        {
            auto minVal = std::numeric_limits<long long int>::min();
            auto maxVal = std::numeric_limits<long long int>::max();
            auto v = value.GetInt64();
            modified |= ui::DragScalar("", ImGuiDataType_S64, &v, 1, &minVal, &maxVal);
            if (modified)
                value = v;
            break;
        }
        default:
            break;
        }
    }
    return modified;
}

bool AttributeInspector::RenderAttributes(Serializable* item, const char* filter)
{
    auto isOpen = ui::CollapsingHeader(qPrintable(item->GetTypeName()), ImGuiTreeNodeFlags_DefaultOpen);
    if (isOpen)
    {
        const std::vector<AttributeInfo>* attributes = item->GetAttributes();
        if (attributes == nullptr)
            return false;

        ui::PushID(item);
        emit InspectorRenderStart(item);

        UI_UPIDSCOPE(1)
        {
            // Show coolumns after custom widgets at inspector start, but have them in a global context. Columns of all
            // components will be resized simultaneously.
            // [/!\ WARNING /!\]
            // Adding new ID scopes here will break code in custom inspector widgets if that code uses ui::Columns() calls.
            // [/!\ WARNING /!\]
            ui::Columns(2);
        }

        for (const AttributeInfo& info: *attributes)
        {
            if (info.mode_ & AM_NOEDIT)
                continue;

            bool hidden = false;
            Color color = Color::WHITE;
            QString tooltip;

            Variant value = item->GetAttribute(info.name_);

            if (info.defaultValue_.GetType() != VAR_NONE && value == info.defaultValue_)
                color = Color::GRAY;

            if (info.mode_ & AM_NOEDIT)
                hidden = true;
            else if (filter != nullptr && *filter && !info.name_.contains(filter, Qt::CaseInsensitive))
                hidden = true;

            if (info.type_ == VAR_BUFFER || info.type_ == VAR_VARIANTVECTOR)
                hidden = true;

            // Customize attribute rendering
            emit AttributeInspectorAttribute(item,&info,&color,&hidden,&tooltip);

            if (hidden)
                continue;

            ui::PushID(qPrintable(info.name_));

            ui::TextColored(ToImGui(color), "%s", qPrintable(info.name_));

            if (!tooltip.isEmpty() && ui::IsItemHovered())
                ui::SetTooltip("%s", qPrintable(tooltip));

            if (ui::IsItemHovered() && ui::IsMouseClicked(MOUSEB_RIGHT))
                ui::OpenPopup("Attribute Menu");

            bool modified = false;
            bool expireBuffers = false;
            if (ui::BeginPopup("Attribute Menu"))
            {
                if (info.defaultValue_.GetType() != VAR_NONE)
                {
                    if (value == info.defaultValue_)
                    {
                        ui::PushStyleColor(ImGuiCol_Text, ui::GetStyle().Colors[ImGuiCol_TextDisabled]);
                        ui::MenuItem("Reset to default");
                        ui::PopStyleColor();
                    }
                    else
                    {
                        if (ui::MenuItem("Reset to default"))
                        {
                            item->SetAttribute(info.name_, info.defaultValue_);
                            item->ApplyAttributes();
                            value = info.defaultValue_;     // For current frame to render correctly
                            expireBuffers = true;
                            modified = true;
                        }
                    }
                }

                if (value.GetType() == VAR_INT && info.name_.endsWith(" Mask"))
                {
                    if (ui::MenuItem("Enable All"))
                    {
                        value = M_MAX_UNSIGNED;
                        modified = true;
                    }
                    if (ui::MenuItem("Disable All"))
                    {
                        value = 0;
                        modified = true;
                    }
                    if (ui::MenuItem("Toggle"))
                    {
                        value = value.GetUInt() ^ M_MAX_UNSIGNED;
                        modified = true;
                    }
                }

                // Allow customization of attribute menu.
                emit AttributeInspectorMenu(item,&info);
                ImGui::EndPopup();
            }

            // Buffers have to be expired outside of popup, because popup has it's own id stack. Careful when pushing
            // new IDs in code below, buffer expiring will break!
            if (expireBuffers)
                ui::ExpireUIState<std::string>();

            ui::NextColumn();

            ui::PushItemWidth(-1);

            // Value widget rendering
            bool nonVariantValue = false;
            {
                bool arghandled=false;
                bool argmodified=false;
                // Rendering of custom widgets for values that do not map to Variant.
                emit InspectorRenderAttribute(item,&info,&arghandled,&argmodified);
                nonVariantValue = arghandled;
                if (nonVariantValue)
                    modified |= argmodified;
                else
                    // Rendering of default widgets for Variant values.
                    modified |= RenderSingleAttribute(this, &info, value);
            }

            // Normal attributes
            auto* modification = ui::GetUIState<ModifiedStateTracker<Variant>>();
            if (modification->TrackModification(modified, [item, &info]() {
                auto previousValue = item->GetAttribute(info.name_);
                if (previousValue.GetType() == VAR_NONE)
                    return info.defaultValue_;
                return previousValue;
            }))
            {
                // This attribute was modified on last frame, but not on this frame. Continuous attribute value modification
                // has ended and we can fire attribute modification event.
                emit AttributeInspectorValueModified(item,&info,&modification->GetInitialValue(),&value);
            }

            if (!nonVariantValue && modified)
            {
                // Update attribute value and do nothing else for now.
                item->SetAttribute(info.name_, value);
                item->ApplyAttributes();
            }

            ui::PopItemWidth();
            ui::PopID();

            ui::NextColumn();
        }
        ui::Columns();
        emit InspectorRenderEnd();
        ui::PopID();
    }

    return isOpen;
}

bool AttributeInspector::RenderSingleAttribute(Variant& value)
{
    return RenderSingleAttribute(nullptr, nullptr, value);
}

}

void ImGui::SameLine(Urho3D::VariantType type)
{
    using namespace Urho3D;

    float spacingFix;
    switch (type)
    {
    case VAR_VECTOR2:
    case VAR_VECTOR3:
    case VAR_VECTOR4:
    case VAR_QUATERNION:
    case VAR_COLOR:
    case VAR_INTRECT:
    case VAR_INTVECTOR2:
    case VAR_MATRIX3:
    case VAR_MATRIX3X4:
    case VAR_MATRIX4:
    case VAR_RECT:
    case VAR_INTVECTOR3:
        spacingFix = 0;
        break;
    default:
        spacingFix = 4_dpx;
        break;
    }

    ui::SameLine(0, spacingFix);
}
