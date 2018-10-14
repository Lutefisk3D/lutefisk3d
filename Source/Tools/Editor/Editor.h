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

#pragma once

#include "Project.h"

#include <Toolbox/SystemUI/AttributeInspector.h>
#include <Lutefisk3D/Engine/Application.h>

#include <QObject>
#include <QVector>

struct MenuWithItems;
namespace Urho3D
{
class Scene;
class Tab;
class SceneTab;
class AssetConverter;

static const unsigned EDITOR_VIEW_LAYER = 1U << 31;

class Editor : public QObject,public Application
{
    Q_OBJECT
    URHO3D_OBJECT(Editor, Object)
public:
    /// Construct.
    explicit Editor(Context* context);
    /// Set up editor application.
    void Setup() override;
    /// Initialize editor application.
    void Start() override;
    /// Tear down editor application.
    void Stop() override;

    /// Renders UI elements.
    void OnUpdate(float ts);
    /// Renders menu bar at the top of the screen.
    void RenderMenuBar();
    /// Create a new tab of specified type.
    template<typename T> T* CreateTab() { return (T*)CreateTab(T::GetTypeStatic()); }
    /// Create a new tab of specified type.
    Tab* CreateTab(StringHash type);
    /// Get tab that has resource opened or create new one and open said resource.
    Tab* GetOrCreateTab(StringHash type, const QString& resourceName);
    /// Return tab of specified type.
    Tab* GetTab(StringHash type);
    /// Return tab of specified type.
    template<typename T>
    T* GetTab() { return static_cast<T*>(GetTab(T::GetTypeStatic())); }
    /// Return active scene tab.
    Tab* GetActiveTab() { return activeTab_; }
    /// Return currently open scene tabs.
    const QVector<SharedPtr<Tab>>& GetSceneViews() const { return tabs_; }
    /// Return a map of names and type hashes from specified category.
    QStringList GetObjectsByCategory(const QString& category);
    /// Get absolute path of `resourceName`. If it is empty, use `defaultResult`. If no resource is found then save file
    /// dialog will be invoked for selecting a new path.
    QString GetResourceAbsolutePath(const QString& resourceName, const QString& defaultResult, const char* patterns,
        const QString& dialogTitle);
    /// Returns a list of open content tabs/docks/windows. This list does not include utility docks/tabs/windows.
    const QVector<SharedPtr<Tab>>& GetContentTabs() const { return tabs_; }
    /// Opens project or creates new one.
    Project* OpenProject(const QString& projectPath);
    /// Close current project.
    void OnCloseProject();
    /// Return path containing data directories of engine.
    const QString& GetCoreResourcePrefixPath() const { return coreResourcePrefixPath_; }
    /// Load default tab layout.
    void LoadDefaultLayout();
signals:
    /// Event sent during construction of toolbar buttons. Subscribe to it to add new buttons.
    void EditorToolbarButtons(Scene *);

    /// Event sent when node selection in scene view changes.
    void EditorSelectionChanged(Scene *);

    /// Event sent when rendering top menu bar of editor.
    void EditorApplicationMenu();

    /// Event sent when editor is about to save a project.
    void EditorProjectSaving(void *Root); // Raw pointer to JSONValue.

    /// Event sent when editor is about to load a new project.
    void EditorProjectLoading(const void *Root); // Raw pointer to JSONValue.

    /// Notify inspector window that this instance would like to render inspector content.
    void EditorRenderInspector(unsigned Category,RefCounted *Inspectable);

    /// Notify inspector window that this instance would like to render hierarchy content.
    void EditorRenderHierarchy(RefCounted *Inspectable);

    /// Notify subsystems about closed editor tab.
    void EditorTabClosed(RefCounted *Tab);
//private  signals
    /// Event sent when editor successfully saves a resource.
    void EditorResourceSaved();
    /// Event sent right before reloading user components.
    void EditorUserCodeReloadStart();
    /// Event sent right after reloading user components.
    void EditorUserCodeReloadEnd();
    /// Event sent when editor is about to load a new project.
    void EditorProjectLoadingStart();
signals:
    void Redo(uint32_t time_val);
    void Undo(uint32_t time_val);
    void SaveProject();
    void OpenOrCreateProject();
    void CloseProject();
    void Exit();
protected:
    void OnEndFrame();
    /// Process console commands.
    void OnConsoleCommand(const QString &cmd, const QString &src);
    /// Process any global hotkeys.
    void HandleHotkeys();
    /// Renders a project plugins submenu.
    void RenderProjectPluginsMenu();
    void renderAndEmitSignals(const MenuWithItems &menu);

    /// List of active scene tabs.
    QVector<SharedPtr<Tab>> tabs_;
    /// Last focused scene tab.
    WeakPtr<Tab> activeTab_;
    /// Prefix path of CoreData and EditorData.
    QString coreResourcePrefixPath_;
    /// Currently loaded project.
    SharedPtr<Project> project_;
protected slots:
    void OnSaveProject();
    void OnOpenOrCreateProject();
};
Editor *getEditorInstance();
}
