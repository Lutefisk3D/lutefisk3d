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

#include <IconFontCppHeaders/IconsFontAwesome5.h>

#include <Toolbox/Scene/DebugCameraController.h>
#include <Toolbox/SystemUI/Widgets.h>
#include <ImGui/imgui_internal.h>
#include <ImGuizmo/ImGuizmo.h>

#include "SceneTab.h"
#include "Editor.h"
#include "Widgets.h"
#include "SceneSettings.h"
#include "Tabs/InspectorTab.h"
#include "Tabs/PreviewTab.h"
#include "Assets/Inspector/MaterialInspector.h"

#include <Lutefisk3D/Core/CoreEvents.h>
#include <Lutefisk3D/Engine/EngineEvents.h>
#include <Lutefisk3D/IO/File.h>
#include <Lutefisk3D/IO/FileSystem.h>
#include <Lutefisk3D/IO/Log.h>
#include <Lutefisk3D/Resource/JSONFile.h>
#include <Lutefisk3D/Resource/ResourceCache.h>
#include <Lutefisk3D/Graphics/View.h>
#include <Lutefisk3D/Graphics/Camera.h>
#include <Lutefisk3D/Graphics/Viewport.h>
#include <Lutefisk3D/Graphics/RenderSurface.h>
#include <Lutefisk3D/Graphics/BillboardSet.h>
#include <Lutefisk3D/Graphics/OctreeQuery.h>
#include <Lutefisk3D/Graphics/RenderPath.h>
#include <Lutefisk3D/Graphics/Octree.h>
#include <Lutefisk3D/Graphics/DebugRenderer.h>

namespace Urho3D
{

static const IntVector2 cameraPreviewSize{320, 200};

SceneTab::SceneTab(Context* context)
    : BaseClassName(context)
    , view_(context, {0, 0, 1024, 768})
    , gizmo_(context)
    , undo_(context)
{
    SetTitle("New Scene");
    windowFlags_ = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

    // Camera preview objects
    cameraPreviewViewport_ = new Viewport(context_);
    cameraPreviewViewport_->SetScene(view_.GetScene());
    cameraPreviewViewport_->SetRect(IntRect{{0, 0}, cameraPreviewSize});
    cameraPreviewViewport_->SetDrawDebug(false);
    cameraPreviewtexture_ = new Texture2D(context_);
    cameraPreviewtexture_->SetSize(cameraPreviewSize.x_, cameraPreviewSize.y_, Graphics::GetRGBFormat(), TEXTURE_RENDERTARGET);
    cameraPreviewtexture_->GetRenderSurface()->SetUpdateMode(SURFACE_UPDATEALWAYS);
    cameraPreviewtexture_->GetRenderSurface()->SetViewport(0, cameraPreviewViewport_);

    // Events
    connect(getEditorInstance(),&Editor::EditorSelectionChanged,this,&SceneTab::OnNodeSelectionChanged);
    g_coreSignals.update.Connect(this,&SceneTab::OnUpdate);
    GetScene()->componentAdded.Connect(this,&SceneTab::OnComponentAdded);
    GetScene()->componentRemoved.Connect(this,&SceneTab::OnComponentRemoved);
    // Components for custom scene settings
    SceneSettings *settings = GetScene()->GetOrCreateComponent<SceneSettings>(LOCAL, FIRST_INTERNAL_ID);
    connect(settings,&SceneSettings::SceneSettingModified,[this](Scene *s,const QString &name,const Variant &value) {
        // TODO: Stinks.
        if (GetScene()->GetComponent<SceneSettings>() != sender())
        {
            return;
        }
        if (name == QLatin1String("Editor Viewport RenderPath"))
        {
            const ResourceRef& renderPathResource = value.GetResourceRef();
            if (renderPathResource.type_ == XMLFile::GetTypeStatic())
            {
                if (XMLFile* renderPathFile = GetCache()->GetResource<XMLFile>(renderPathResource.name_))
                {
                    auto setRenderPathToViewport = [this, renderPathFile](Viewport* viewport)
                    {
                        if (!viewport->SetRenderPath(renderPathFile))
                            return;

                        RenderPath* path = viewport->GetRenderPath();
                        for (auto& command: path->commands_)
                        {
                            if (command.pixelShaderName_.startsWith("PBR"))
                            {
                                XMLFile* gammaCorrection = GetCache()->GetResource<XMLFile>(
                                    "PostProcess/GammaCorrection.xml");
                                path->Append(gammaCorrection);
                                return;
                            }
                        }
                    };
                    setRenderPathToViewport(GetSceneView()->GetViewport());
                    setRenderPathToViewport(cameraPreviewViewport_);
                }
            }
        }
    });
    connect(&inspector_,&AttributeInspector::InspectorRenderStart,this,[](Serializable *serializable) {
        if (serializable->GetType() == Node::GetTypeStatic())
        {
            UI_UPIDSCOPE(1)
                ui::Columns(2);
            Node* node = static_cast<Node*>(serializable);
            ui::TextUnformatted("ID");
            ui::NextColumn();
            ui::Text("%u (%s)", node->GetID(), node->IsReplicated() ? "Replicated" : "Local");
            ui::NextColumn();
        }
    });

    undo_.Connect(GetScene());
    undo_.Connect(&inspector_);
    undo_.Connect(&gizmo_);
    GetScene()->asyncLoadFinished.ConnectL([&](Scene *) {
        undo_.Clear();
    });

    // Scene is updated manually.
    GetScene()->SetUpdateEnabled(false);

    CreateObjects();
    undo_.Clear();

    UpdateUniqueTitle();
}

SceneTab::~SceneTab() = default;

bool SceneTab::RenderWindowContent()
{
    auto& style = ui::GetStyle();
    if (GetInput()->IsMouseVisible())
        lastMousePosition_ = GetInput()->GetMousePosition();
    bool open = true;

    // Focus window when appearing
    if (!isRendered_)
        ui::SetWindowFocus();

    RenderToolbarButtons();
    if (!ui::IsDockDocked())
    {
        // Without this workaround undocked scene tabs have extra empty line under toolbar buttons.
        ui::SameLine();
        ui::SetCursorPosY(ui::GetCursorPosY() + ui::GetStyle().ItemSpacing.y);
    }
    IntRect tabRect = UpdateViewRect();

    ui::SetCursorScreenPos(ToImGui(tabRect.Min()));
    ui::Image(view_.GetTexture(), ToImGui(tabRect.Size()));
    gizmo_.ManipulateSelection(view_.GetCamera());

    if (GetInput()->IsMouseVisible())
        mouseHoversViewport_ = ui::IsItemHovered();

    bool isClickedLeft = GetInput()->GetMouseButtonClick(MOUSEB_LEFT) && ui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup);
    bool isClickedRight = GetInput()->GetMouseButtonClick(MOUSEB_RIGHT) && ui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup);

    // Render camera preview
    if (cameraPreviewViewport_->GetCamera() != nullptr)
    {
        float borderSize = ui::GetStyle().FrameBorderSize;
        ui::SetCursorScreenPos(ToImGui(tabRect.Max() - cameraPreviewSize - IntVector2{10, 10}));
        ui::RenderFrameBorder(ui::GetCursorScreenPos() - ImVec2{borderSize, borderSize},
            ui::GetCursorScreenPos() + ToImGui(cameraPreviewSize) + ImVec2{borderSize, borderSize});

        ui::Image(cameraPreviewtexture_.Get(), ToImGui(cameraPreviewSize));
    }

    // Prevent dragging window when scene view is clicked.
    if (ui::IsWindowHovered())
        windowFlags_ |= ImGuiWindowFlags_NoMove;
    else
        windowFlags_ &= ~ImGuiWindowFlags_NoMove;

    if (!gizmo_.IsActive() && (isClickedLeft || isClickedRight) && GetInput()->IsMouseVisible())
    {
        // Handle object selection.
        IntVector2 pos = GetInput()->GetMousePosition();
        pos -= tabRect.Min();

        Ray cameraRay = view_.GetCamera()->GetScreenRay((float)pos.x_ / tabRect.Width(), (float)pos.y_ / tabRect.Height());
        // Pick only geometry objects, not eg. zones or lights, only get the first (closest) hit
        std::vector<RayQueryResult> results;

        RayOctreeQuery query(results, cameraRay, RAY_TRIANGLE, M_INFINITY, DRAWABLE_GEOMETRY);
        GetScene()->GetComponent<Octree>()->RaycastSingle(query);

        if (!results.size())
        {
            // When object geometry was not hit by a ray - query for object bounding box.
            RayOctreeQuery query2(results, cameraRay, RAY_OBB, M_INFINITY, DRAWABLE_GEOMETRY);
            GetScene()->GetComponent<Octree>()->RaycastSingle(query2);
        }

        if (results.size())
        {
            WeakPtr<Node> clickNode(results[0].drawable_->GetNode());
            // Temporary nodes can not be selected.
            while (!clickNode.Expired() && clickNode->HasTag("__EDITOR_OBJECT__"))
                clickNode = clickNode->GetParent();

            if (!clickNode.Expired())
            {
                bool appendSelection = GetInput()->GetQualifierDown(QUAL_CTRL);
                if (!appendSelection)
                    UnselectAll();
                ToggleSelection(clickNode);

                if (isClickedRight && undo_.IsTrackingEnabled())
                    ui::OpenPopupEx(ui::GetID("Node context menu"));
            }
        }
        else
            UnselectAll();
    }

    RenderNodeContextMenu();

    const auto tabContextMenuTitle = "SceneTab context menu";
    if (ui::IsDockTabHovered() && GetInput()->GetMouseButtonPress(MOUSEB_RIGHT))
        ui::OpenPopup(tabContextMenuTitle);
    if (ui::BeginPopup(tabContextMenuTitle))
    {
        if (ui::MenuItem("Save"))
            SaveResource();

        ui::Separator();

        if (ui::MenuItem("Close"))
            open = false;

        ui::EndPopup();
    }

    return open;
}

bool SceneTab::LoadResource(const QString& resourcePath)
{
    if (!BaseClassName::LoadResource(resourcePath))
        return false;

    if (resourcePath.endsWith(".xml", Qt::CaseInsensitive))
    {
        auto* file = GetCache()->GetResource<XMLFile>(resourcePath);
        if (file && GetScene()->LoadXML(file->GetRoot()))
        {
            CreateObjects();
        }
        else
        {
            URHO3D_LOGERRORF("Loading scene %s failed", qPrintable(GetFileName(resourcePath)));
            return false;
        }
    }
    else if (resourcePath.endsWith(".json", Qt::CaseInsensitive))
    {
        auto* file = GetCache()->GetResource<JSONFile>(resourcePath);
        if (file && GetScene()->LoadJSON(file->GetRoot()))
        {
            CreateObjects();
        }
        else
        {
            URHO3D_LOGERRORF("Loading scene %s failed", qPrintable(GetFileName(resourcePath)));
            return false;
        }
    }
    else
    {
        URHO3D_LOGERRORF("Unknown scene file format %s", qPrintable(GetExtension(resourcePath)));
        return false;
    }

    SetTitle(GetFileName(resourcePath));
    return true;
}

bool SceneTab::SaveResource()
{
    if (!BaseClassName::SaveResource())
        return false;

    GetCache()->IgnoreResourceReload(resourceName_);

    QString fullPath = GetCache()->GetResourceFileName(resourceName_);
    if (fullPath.isEmpty())
        return false;

    File file(context_, fullPath, FILE_WRITE);
    bool result = false;

    float elapsed = GetScene()->GetElapsedTime();
    GetScene()->SetElapsedTime(0);
    GetScene()->SetUpdateEnabled(true);
    if (fullPath.endsWith(".xml", Qt::CaseInsensitive))
        result = GetScene()->SaveXML(file);
    else if (fullPath.endsWith(".json", Qt::CaseInsensitive))
        result = GetScene()->SaveJSON(file);
    GetScene()->SetUpdateEnabled(false);
    GetScene()->SetElapsedTime(elapsed);

    if (result)
        emit getEditorInstance()->EditorResourceSaved();
    else
        URHO3D_LOGERRORF("Saving scene to %s failed.", qPrintable(resourceName_));

    return result;
}

void SceneTab::CreateObjects()
{
    auto isTracking = undo_.IsTrackingEnabled();
    undo_.SetTrackingEnabled(false);
    view_.CreateObjects();
    view_.GetCamera()->GetNode()->GetOrCreateComponent<DebugCameraController>();
    undo_.SetTrackingEnabled(isTracking);
}

void SceneTab::Select(Node* node)
{
    if (gizmo_.Select(node))
    {
        emit getEditorInstance()->EditorSelectionChanged(GetScene());
    }
}

void SceneTab::Select(std::vector<Node *> nodes)
{
    if (gizmo_.Select(nodes))
    {
        emit getEditorInstance()->EditorSelectionChanged(GetScene());
    }
}

void SceneTab::Unselect(Node* node)
{
    if (gizmo_.Unselect(node))
    {
        emit getEditorInstance()->EditorSelectionChanged(GetScene());
    }
}

void SceneTab::ToggleSelection(Node* node)
{
    gizmo_.ToggleSelection(node);
    emit getEditorInstance()->EditorSelectionChanged(GetScene());
}

void SceneTab::UnselectAll()
{
    if (gizmo_.UnselectAll())
    {
        emit getEditorInstance()->EditorSelectionChanged(GetScene());
    }
}

const QSet<WeakPtr<Node> > &SceneTab::GetSelection() const
{
    return gizmo_.GetSelection();
}

void SceneTab::RenderToolbarButtons()
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

    if (ui::EditorToolbarButton(ICON_FA_ARROWS_ALT, "Translate", gizmo_.GetOperation() == GIZMOOP_TRANSLATE))
        gizmo_.SetOperation(GIZMOOP_TRANSLATE);
    if (ui::EditorToolbarButton(ICON_FA_SYNC, "Rotate", gizmo_.GetOperation() == GIZMOOP_ROTATE))
        gizmo_.SetOperation(GIZMOOP_ROTATE);
    if (ui::EditorToolbarButton(ICON_FA_EXPAND_ARROWS_ALT, "Scale", gizmo_.GetOperation() == GIZMOOP_SCALE))
        gizmo_.SetOperation(GIZMOOP_SCALE);

    ui::SameLine(0, 3.f);

    if (ui::EditorToolbarButton(ICON_FA_ARROWS_ALT, "World", gizmo_.GetTransformSpace() == TS_WORLD))
        gizmo_.SetTransformSpace(TS_WORLD);
    if (ui::EditorToolbarButton(ICON_FA_EXPAND_ARROWS_ALT, "Local", gizmo_.GetTransformSpace() == TS_LOCAL))
        gizmo_.SetTransformSpace(TS_LOCAL);

    ui::SameLine(0, 3.f);

    if (auto* camera = view_.GetCamera())
    {
        if (auto* light = camera->GetNode()->GetComponent<Light>())
        {
            if (ui::EditorToolbarButton(ICON_FA_LIGHTBULB, "Camera Headlight", light->IsEnabled()))
                light->SetEnabled(!light->IsEnabled());
        }
    }

    ui::SameLine(0, 3.f);

    emit getEditorInstance()->EditorToolbarButtons(GetScene());

    ui::NewLine();
    style.FrameRounding = oldRounding;
}

bool SceneTab::IsSelected(Node* node) const
{
    return gizmo_.IsSelected(node);
}

void SceneTab::OnNodeSelectionChanged(Scene *s)
{
    UpdateCameraPreview();
    selectedComponent_ = nullptr;
}

void SceneTab::RenderInspector(const char* filter)
{
    // TODO: inspector for multi-selection.
    if (GetSelection().size() == 1)
    {
        auto node = *GetSelection().begin();
        if (node.Expired())
            return;

        std::vector<Serializable*> items;
        inspector_.RenderAttributes(node.Get(), filter);

        for (Component* component : node->GetComponents())
        {
            if (component->IsTemporary())
                continue;

            inspector_.RenderAttributes(component, filter);
        }
    }
}

void SceneTab::RenderHierarchy()
{
    auto oldSpacing = ui::GetStyle().IndentSpacing;
    ui::GetStyle().IndentSpacing = 10;
    RenderNodeTree(GetScene());
    ui::GetStyle().IndentSpacing = oldSpacing;
}

void SceneTab::RenderNodeTree(Node* node)
{
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow;
    if (node->GetParent() == nullptr)
        flags |= ImGuiTreeNodeFlags_DefaultOpen;

    if (node->IsTemporary())
        return;

    if (node == scrollTo_.Get())
        ui::SetScrollHere();

    QString name = node->GetName().isEmpty() ? QString::asprintf("%s %d", qPrintable(node->GetTypeName()), node->GetID()) : node->GetName();
    bool isSelected = IsSelected(node) && selectedComponent_.Expired();

    if (isSelected)
        flags |= ImGuiTreeNodeFlags_Selected;

    ui::Image("Node");
    ui::SameLine();
    ui::PushID((void*)node);
    auto opened = ui::TreeNodeEx(qPrintable(name), flags);
    auto it = std::find(openHierarchyNodes_.begin(),openHierarchyNodes_.end(),node);
    if (it != openHierarchyNodes_.end())
    {
        if (!opened)
        {
            ui::OpenTreeNode(ui::GetCurrentWindow()->GetID(qPrintable(name)));
            opened = true;
        }
        openHierarchyNodes_.erase(it);
    }

    if (ui::BeginDragDropSource())
    {
        ui::SetDragDropVariant("ptr", node);
        ui::Text("%s", qPrintable(name));
        ui::EndDragDropSource();
    }

    if (ui::BeginDragDropTarget())
    {
        const Variant& payload = ui::AcceptDragDropVariant("ptr");
        if (!payload.IsEmpty())
        {
            SharedPtr<Node> child(dynamic_cast<Node*>(payload.GetPtr()));
            if (child.NotNull() && child != node)
            {
                node->AddChild(child);
                if (!opened)
                    openHierarchyNodes_.push_back(node);
            }
        }
        ui::EndDragDropTarget();
    }

    if (!opened)
    {
        // If TreeNode above is opened, it pushes it's label as an ID to the stack. However if it is not open then no
        // ID is pushed. This creates a situation where context menu is not properly attached to said tree node due to
        // missing ID on the stack. To correct this we ensure that ID is always pushed. This allows us to show context
        // menus even for closed tree nodes.
        ui::PushID(qPrintable(name));
    }

    // Popup may delete node. Weak reference will convey that information.
    WeakPtr<Node> nodeRef(node);

    if (ui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup))
    {
        if (ui::IsMouseClicked(MOUSEB_LEFT))
        {
            if (!GetInput()->GetQualifierDown(QUAL_CTRL))
                UnselectAll();
            ToggleSelection(node);
        }
        else if (ui::IsMouseClicked(MOUSEB_RIGHT) && undo_.IsTrackingEnabled())
        {
            UnselectAll();
            ToggleSelection(node);
            ui::OpenPopupEx(ui::GetID("Node context menu"));
        }
    }

    RenderNodeContextMenu();

    if (opened)
    {
        if (!nodeRef.Expired())
        {
            std::vector<SharedPtr<Component>> components = node->GetComponents();
            for (const auto& component: components)
            {
                if (component->IsTemporary())
                    continue;

                ui::PushID(component);

                ui::Image(component->GetTypeName());
                ui::SameLine();

                bool selected = selectedComponent_ == component;
                selected = ui::Selectable(qPrintable(component->GetTypeName()), selected);

                if (ui::IsMouseClicked(MOUSEB_RIGHT) && ui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup))
                {
                    selected = true;
                    ui::OpenPopupEx(ui::GetID("Component context menu"));
                }

                if (selected)
                {
                    UnselectAll();
                    ToggleSelection(node);
                    selectedComponent_ = component;
                }

                if (ui::BeginPopup("Component context menu"))
                {
                    if (ui::MenuItem("Delete", "Del"))
                        component->Remove();
                    ui::EndPopup();
                }

                ui::PopID();
            }

            // Do not use element->GetChildren() because child may be deleted during this loop.
            std::vector<Node*> children;
            node->GetChildren(children);
            for (Node* child: children)
                RenderNodeTree(child);
        }
        ui::TreePop();
    }
    else
        ui::PopID();
    ui::PopID();
}

void SceneTab::OnLoadProject(const JSONValue& tab)
{
    undo_.Clear();
    auto isTracking = undo_.IsTrackingEnabled();
    undo_.SetTrackingEnabled(false);

    BaseClassName::OnLoadProject(tab);

    const auto& camera = tab["camera"];
    if (camera.IsObject())
    {
        Node* cameraNode = view_.GetCamera()->GetNode();
        cameraNode->SetPosition(camera["position"].GetVariant().GetVector3());
        cameraNode->SetRotation(camera["rotation"].GetVariant().GetQuaternion());
        if (auto* lightComponent = cameraNode->GetComponent<Light>())
            lightComponent->SetEnabled(camera["light"].GetBool());
    }

    undo_.SetTrackingEnabled(isTracking);
}

void SceneTab::OnSaveProject(JSONValue& tab)
{
    BaseClassName::OnSaveProject(tab);

    auto& camera = tab["camera"];
    Node* cameraNode = view_.GetCamera()->GetNode();
    camera["position"].SetVariant(cameraNode->GetPosition());
    camera["rotation"].SetVariant(cameraNode->GetRotation());
    camera["light"] = cameraNode->GetComponent<Light>()->IsEnabled();
}

void SceneTab::OnActiveUpdate()
{
}

void SceneTab::RemoveSelection()
{
    if (!selectedComponent_.Expired())
        selectedComponent_->Remove();
    else
    {
        for (auto& selected : GetSelection())
        {
            if (!selected.Expired())
                selected->Remove();
        }
    }
    UnselectAll();
}

IntRect SceneTab::UpdateViewRect()
{
    IntRect tabRect = BaseClassName::UpdateViewRect();
    view_.SetSize(tabRect);
    gizmo_.SetScreenRect(tabRect);
    return tabRect;
}

void SceneTab::OnUpdate(float timeStep)
{
    if (auto component = view_.GetCamera()->GetComponent<DebugCameraController>())
    {
        if (mouseHoversViewport_)
            component->Update(timeStep);
    }

    if (ui::IsWindowFocused())
    {
        if (!ui::IsAnyItemActive() && undo_.IsTrackingEnabled())
        {
            // Global view hotkeys
            if (GetInput()->GetKeyDown(KEY_DELETE))
                RemoveSelection();
        }
    }
    // Render editor camera rotation guide
    if (auto* debug = GetScene()->GetComponent<DebugRenderer>())
    {
        Vector3 guideRoot = ScreenToWorldPoint(*GetSceneView()->GetCamera(),{0.95f, 0.1f, 1.0f});
        debug->AddLine(guideRoot, guideRoot + Vector3::RIGHT * 0.05f, Color::RED, false);
        debug->AddLine(guideRoot, guideRoot + Vector3::UP * 0.05f, Color::GREEN, false);
        debug->AddLine(guideRoot, guideRoot + Vector3::FORWARD * 0.05f, Color::BLUE, false);
    }
}

void SceneTab::SceneStateSave(VectorBuffer& destination)
{
    Undo::SetTrackingScoped tracking(undo_, false);

    for (auto& node : GetSelection())
    {
        if (node)
            node->AddTag("__EDITOR_SELECTED__");
    }

    // Ensure that editor objects are saved.
    std::vector<Node*> nodes;
    GetScene()->GetNodesWithTag(nodes, "__EDITOR_OBJECT__");
    for (auto* node : nodes)
        node->SetTemporary(false);

    destination.clear();
    GetScene()->Save(destination);

    // Prevent marker tags from showing up in UI
    for (auto& node : GetSelection())
    {
        if (node)
            node->RemoveTag("__EDITOR_SELECTED__");
    }

    // Now that editor objects are saved make sure UI does not expose them
    for (auto* node : nodes)
        node->SetTemporary(true);
}

void SceneTab::SceneStateRestore(VectorBuffer& source)
{
    Undo::SetTrackingScoped tracking(undo_, false);

    source.Seek(0);
    GetScene()->Load(source);

    CreateObjects();

    // Ensure that editor objects are not saved in user scene.
    std::vector<Node*> nodes;
    GetScene()->GetNodesWithTag(nodes, "__EDITOR_OBJECT__");
    for (auto* node : nodes)
        node->SetTemporary(true);

    source.clear();

    gizmo_.UnselectAll();
    for (auto node : GetScene()->GetChildrenWithTag("__EDITOR_SELECTED__", true))
    {
        gizmo_.Select(node);
        node->RemoveTag("__EDITOR_SELECTED__");
    }
    UpdateCameraPreview();
}

void SceneTab::RenderNodeContextMenu()
{
    if (undo_.IsTrackingEnabled() && ui::BeginPopup("Node context menu"))
    {
        Input* input = context_->m_InputSystem.get();
        if (input->GetKeyPress(KEY_ESCAPE) || !input->IsMouseVisible())
        {
            // Close when interacting with scene camera.
            ui::CloseCurrentPopup();
            ui::EndPopup();
            return;
        }

        bool alternative = input->GetQualifierDown(QUAL_SHIFT);

        if (ui::MenuItem(alternative ? "Create Child (Local)" : "Create Child"))
        {
            std::vector<Node*> newNodes;
            for (auto& selectedNode : GetSelection())
            {
                if (!selectedNode.Expired())
                {
                    newNodes.push_back(selectedNode->CreateChild(QString(), alternative ? LOCAL : REPLICATED));
                    openHierarchyNodes_.push_back(selectedNode);
                    openHierarchyNodes_.push_back(newNodes.back());
                    scrollTo_ = newNodes.back();
                }
            }

            UnselectAll();
            Select(newNodes);
        }

        if (ui::BeginMenu(alternative ? "Create Component (Local)" : "Create Component"))
        {
            auto* editor = getEditorInstance();
            for (const auto& category_val : context_->GetObjectCategories())
            {
                if(category_val.first=="UI")
                    continue;

                auto components = editor->GetObjectsByCategory(category_val.first);
                if (components.empty())
                    continue;

                if (ui::BeginMenu(qPrintable(category_val.first)))
                {
                    qSort(components);

                    for (const QString& component : components)
                    {
                        ui::Image(component);
                        ui::SameLine();
                        if (ui::MenuItem(qPrintable(component)))
                        {
                            for (auto& selectedNode : GetSelection())
                            {
                                if (!selectedNode.Expired())
                                {
                                    if (selectedNode->CreateComponent(StringHash(component),
                                        alternative ? LOCAL : REPLICATED))
                                        openHierarchyNodes_.push_back(selectedNode);
                                }
                            }
                        }
                    }
                    ui::EndMenu();
                }
            }
            ui::EndMenu();
        }

        ui::Separator();

        if (ui::MenuItem("Remove"))
            RemoveSelection();

        ui::EndPopup();
    }
}

void SceneTab::OnComponentAdded(Scene *,Node *node,Component *component)
{

    if (node->IsTemporary() || node->HasTag("__EDITOR_OBJECT__"))
        return;

    auto* material = GetCache()->GetResource<Material>("Materials/Editor/DebugIcon" + component->GetTypeName() + ".xml", false);
    if (material != nullptr)
    {
        if (node->GetChildrenWithTag("DebugIcon" + component->GetTypeName()).size() > 0)
            return;

        auto iconTag = "DebugIcon" + component->GetTypeName();
        if (node->GetChildrenWithTag(iconTag).empty())
        {
            Undo::SetTrackingScoped tracking(undo_, false);
            int count = node->GetChildrenWithTag("DebugIcon").size();
            node = node->CreateChild();
            node->AddTag("DebugIcon");
            node->AddTag("DebugIcon" + component->GetTypeName());
            node->AddTag("__EDITOR_OBJECT__");
            node->SetTemporary(true);

            auto* billboard = node->CreateComponent<BillboardSet>();
            billboard->SetFaceCameraMode(FaceCameraMode::FC_LOOKAT_XYZ);
            billboard->SetNumBillboards(1);
            billboard->SetMaterial(material);
            billboard->SetViewMask(EDITOR_VIEW_LAYER);
            if (auto* bb = billboard->GetBillboard(0))
            {
                bb->size_ = Vector2::ONE * 0.2f;
                bb->enabled_ = true;
                bb->position_ = {0, count * 0.4f, 0};
            }
            billboard->Commit();
        }
    }

    UpdateCameraPreview();
}

void SceneTab::OnComponentRemoved(Scene *,Node *node,Component *component)
{
    if (!node->IsTemporary())
    {
        Undo::SetTrackingScoped tracking(undo_, false);

        for (auto* icon : node->GetChildrenWithTag("DebugIcon" + component->GetTypeName()))
            icon->Remove();

        int index = 0;
        for (auto* icon : node->GetChildrenWithTag("DebugIcon"))
        {
            if (auto* billboard = icon->GetComponent<BillboardSet>())
            {
                billboard->GetBillboard(0)->position_ = {0, index * 0.4f, 0};
                billboard->Commit();
                index++;
            }
        }
    }

    UpdateCameraPreview();
}

void SceneTab::OnFocused()
{
    if (InspectorTab* inspector = getEditorInstance()->GetTab<InspectorTab>())
    {
        if (auto* inspectorProvider = dynamic_cast<MaterialInspector*>(inspector->GetInspector(IC_RESOURCE)))
            inspectorProvider->SetEffectSource(GetSceneView()->GetViewport()->GetRenderPath());
    }
}

void SceneTab::UpdateCameraPreview()
{
    cameraPreviewViewport_->SetCamera(nullptr);

    if (!GetSelection().size())
        return;
    if (Node* node = *GetSelection().begin())
    {
        if (Camera* camera = node->GetComponent<Camera>())
        {
            camera->SetViewMask(camera->GetViewMask() & ~EDITOR_VIEW_LAYER);
            cameraPreviewViewport_->SetCamera(camera);
            cameraPreviewViewport_->SetRenderPath(GetSceneView()->GetViewport()->GetRenderPath());
        }
    }
}

}
