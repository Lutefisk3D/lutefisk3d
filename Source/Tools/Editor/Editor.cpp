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
#include "EditorIconCache.h"
#include "Tabs/Scene/SceneTab.h"
#include "Tabs/Scene/SceneSettings.h"
#include "Tabs/UI/UITab.h"
#include "Tabs/InspectorTab.h"
#include "Tabs/HierarchyTab.h"
#include "Tabs/ConsoleTab.h"
#include "Tabs/ResourceTab.h"
#include "Tabs/PreviewTab.h"
#include "Assets/AssetConverter.h"
#include "Assets/Inspector/MaterialInspector.h"

//#include <CLI11/CLI11.hpp>
#include <Toolbox/IO/ContentUtilities.h>
#include <Toolbox/SystemUI/ResourceBrowser.h>
#include <Toolbox/ToolboxAPI.h>
#include <IconFontCppHeaders/IconsFontAwesome5.h>
#include <Toolbox/SystemUI/Widgets.h>


#include <Lutefisk3D/Core/CoreEvents.h>
#include <Lutefisk3D/Engine/Application.h>
#include <Lutefisk3D/Engine/Engine.h>
#include <Lutefisk3D/Engine/EngineDefs.h>
#include <Lutefisk3D/Engine/EngineEvents.h>
#include <Lutefisk3D/IO/FileSystem.h>
#include <Lutefisk3D/Resource/ResourceCache.h>
#include <Lutefisk3D/IO/Log.h>
#include <Lutefisk3D/Input/Input.h>
#include <Lutefisk3D/Input/InputConstants.h>
#include <Lutefisk3D/SystemUI/Console.h>
#include <QtWidgets/QFileDialog>
using namespace Urho3D;
URHO3D_DEFINE_APPLICATION_MAIN(Editor)

Editor *g_editor_instance;
static std::string defaultProjectPath;

Editor::Editor(Context* context)
    : Application("Editor",context)
{
    g_editor_instance = this;
    connect(this,&Editor::SaveProject,this,&Editor::OnSaveProject);
    connect(this,&Editor::OpenOrCreateProject,this,&Editor::OnOpenOrCreateProject);
    connect(this,&Editor::Exit,[this]() { engine_->Exit(); });
    connect(this,&Editor::CloseProject,this,&Editor::OnCloseProject);
}

void Editor::Setup()
{
#ifdef _WIN32
    // Required until SDL supports hdpi on windows
    if (HMODULE hLibrary = LoadLibraryA("Shcore.dll"))
    {
        typedef HRESULT(WINAPI*SetProcessDpiAwarenessType)(size_t value);
        if (auto fn = GetProcAddress(hLibrary, "SetProcessDpiAwareness"))
            ((SetProcessDpiAwarenessType)fn)(2);    // PROCESS_PER_MONITOR_DPI_AWARE
        FreeLibrary(hLibrary);
    }
#endif

    // Discover resource prefix path by looking for CoreData and going up.
    for (coreResourcePrefixPath_ = GetContext()->m_FileSystem->GetProgramDir();;
        coreResourcePrefixPath_ = GetParentPath(coreResourcePrefixPath_))
    {
        if (GetContext()->m_FileSystem->DirExists(coreResourcePrefixPath_ + "CoreData"))
            break;
        else
        {
#if _WIN32
            if (coreResourcePrefixPath_.size() <= 3)   // Root path of any drive
#else
            if (coreResourcePrefixPath_ == "/")          // Filesystem root
#endif
            {
                URHO3D_LOGERROR("Prefix path not found, unable to continue. Prefix path must contain all of your data "
                                "directories (including CoreData).");
                engine_->Exit();
            }
        }
    }

    engineParameters_[EP_HEADLESS] = false;
    engineParameters_[EP_FULL_SCREEN] = false;
    engineParameters_[EP_WINDOW_HEIGHT] = 1080;
    engineParameters_[EP_WINDOW_WIDTH] = 1920;
    engineParameters_[EP_LOG_LEVEL] = LOG_DEBUG;
    engineParameters_[EP_WINDOW_RESIZABLE] = true;
    engineParameters_[EP_AUTOLOAD_PATHS] = "";
    engineParameters_[EP_RESOURCE_PATHS] = "CoreData;EditorData";
    engineParameters_[EP_RESOURCE_PREFIX_PATHS] = coreResourcePrefixPath_;

    SetRandomSeed(Time::GetTimeSinceEpoch());

    // Define custom command line parameters here
    //auto &commandLine = GetCommandLineParser();
    //commandLine.add_option("project", defaultProjectPath, "Project to open or create on startup.")
    //    ->set_custom_option("dir");
}

void Editor::Start()
{
    GetContext()->RegisterFactory<EditorIconCache>();
    GetContext()->RegisterFactory<SceneTab>();
    GetContext()->RegisterFactory<UITab>();
    GetContext()->RegisterFactory<ConsoleTab>();
    GetContext()->RegisterFactory<HierarchyTab>();
    GetContext()->RegisterFactory<InspectorTab>();
    GetContext()->RegisterFactory<ResourceTab>();
    GetContext()->RegisterFactory<PreviewTab>();

    Inspectable::Material::RegisterObject(GetContext());

    GetContext()->RegisterSubsystem(new EditorIconCache(GetContext()));
    GetContext()->m_InputSystem->SetMouseMode(MM_ABSOLUTE);
    GetContext()->m_InputSystem->SetMouseVisible(true);
    RegisterToolboxTypes(GetContext());
    //m_context->RegisterFactory<Editor>();
    //m_context->RegisterSubsystem(this);
    SceneSettings::RegisterObject(GetContext());
    auto sys_ui = GetContext()->m_SystemUI.get();
    std::vector<uint16_t> icon_ranges={ICON_MIN_FA, ICON_MAX_FA, 0};
    sys_ui->ApplyStyleDefault(true, 1.0f);
    sys_ui->AddFont(QStringLiteral("Fonts/NotoSans-Regular.ttf"), {}, 16.f);
    sys_ui->AddFont("Fonts/" FONT_ICON_FILE_NAME_FAS, icon_ranges, 0, true);
    ui::GetStyle().WindowRounding = 3;
    // Disable imgui saving ui settings on it's own. These should be serialized to project file.
    ui::GetIO().IniFilename = nullptr;

    GetContext()->m_ResourceCache->SetAutoReloadResources(true);
    g_coreSignals.update.Connect(this,&Editor::OnUpdate);

    // Creates console but makes sure it's UI is not rendered. Console rendering is done manually in editor.
    auto* console = engine_->CreateConsole();
    console->SetAutoVisibleOnError(false);
    GetContext()->m_FileSystem->SetExecuteConsoleCommands(false);
    g_consoleSignals.consoleCommand.Connect(this,&Editor::OnConsoleCommand);
    console->RefreshInterpreters();

    // Prepare editor for loading new project.
    connect(this,&Editor::EditorProjectLoadingStart,[this]() {tabs_.clear();});
    g_coreSignals.endFrame.Connect(this,&Editor::OnEndFrame);
}
void Editor::OnEndFrame()
{
    if (!defaultProjectPath.empty())
    {
        OpenProject(defaultProjectPath.c_str());
        defaultProjectPath.clear();
    }
    g_coreSignals.endFrame.Disconnect(this,&Editor::OnEndFrame);
}
void Editor::Stop()
{
    OnCloseProject();
    ui::ShutdownDock();
}

void Editor::OnUpdate(float ts)
{
    qApp->processEvents();
    RenderMenuBar();

    ui::RootDock({0, 20}, ui::GetIO().DisplaySize - ImVec2(0, 20));

    auto tabsCopy = tabs_;
    for (auto& tab : tabsCopy)
    {
        if (tab->RenderWindow())
        {
            if (tab->IsRendered() && (activeTab_ != tab))
            {
                // Only active window may override another active window
                if (tab->IsActive())
                {
                    activeTab_ = tab;
                    tab->OnFocused();
                }
            }
        }
        else if (!tab->IsUtility())
            // Content tabs get closed permanently
            tabs_.removeAll(tab);
    }

    if (!activeTab_.Expired())
    {
        activeTab_->OnActiveUpdate();
    }

    HandleHotkeys();
}

void Editor::OnSaveProject()
{
    for (auto& tab : tabs_)
        tab->SaveResource();
    project_->SaveProject();
}

void Editor::OnOpenOrCreateProject()
{
    QString dirPath = QFileDialog::getExistingDirectory(nullptr,"Select a project directory");
    if (!dirPath.isEmpty())
    {
        if (OpenProject(dirPath) == nullptr)
            URHO3D_LOGERROR("Loading project failed.");
    }
}
struct SimpleMenuItem {
    const char *name;
    std::function<bool()> enabled;
    void (Editor::*sig)();
};
struct MenuWithItems {
    const char *name;
    std::vector<SimpleMenuItem> items;
};
void Editor::renderAndEmitSignals(const MenuWithItems &menu)
{
    if (ui::BeginMenu(menu.name))
    {
        for(const SimpleMenuItem &item : menu.items)
        {
            if(item.name==nullptr)
                ui::Separator();
            else if(ui::MenuItem(item.name,"",false,item.enabled()))
                emit (this->*item.sig)();
        }
        ui::EndMenu();
    }
}

void Editor::RenderMenuBar()
{
    static const MenuWithItems file_menu = {
        "File",
        {
            {"Save Project",[this]()->bool {return !project_.NotNull();},&Editor::SaveProject},
            {"Open/Create Project",[]()->bool {return true;},&Editor::OpenOrCreateProject},
            {nullptr,[]()->bool {return true;},nullptr},
            {"Close Project",[this]()->bool {return !project_.NotNull();},&Editor::CloseProject},
            {"Exit",[]()->bool {return true;},&Editor::Exit},
        }
    };
    if (ui::BeginMainMenuBar())
    {
        renderAndEmitSignals(file_menu);
        if (project_.NotNull())
        {
            if (ui::BeginMenu("View"))
            {
                for (auto& tab : tabs_)
                {
                    if (tab->IsUtility())
                    {
                        // Tabs that can not be closed permanently
                        auto open = tab->IsOpen();
                        if (ui::MenuItem(qPrintable(tab->GetUniqueTitle()), nullptr, &open))
                            tab->SetOpen(open);
                    }
                }
                ui::EndMenu();
            }

            if (ui::BeginMenu("Project"))
            {
                if (ui::BeginMenu("Plugins"))
                {
                    RenderProjectPluginsMenu();
                    ui::EndMenu();
                }
                ui::EndMenu();
            }
        }
        emit EditorApplicationMenu();
        // Scene simulation buttons.
        if (project_.NotNull())
        {
            // Copied from ToolbarButton()
            auto& g = *ui::GetCurrentContext();
            float dimension = g.FontBaseSize + g.Style.FramePadding.y * 2.0f;
            ui::SetCursorScreenPos({ui::GetIO().DisplaySize.x / 2 - dimension * 4 / 2, ui::GetCursorScreenPos().y});
            if (auto* previewTab = GetTab<PreviewTab>())
                previewTab->RenderButtons();
        }
        ui::EndMainMenuBar();
    }
}

Tab* Editor::CreateTab(StringHash type)
{
    auto tab = DynamicCast<Tab>(GetContext()->CreateObject(type));
    tabs_.push_back(tab);
    if(auto uitab = DynamicCast<UITab>(tab))
    {
        connect(this,&Editor::Undo,&uitab->GetUndo(),&Undo::Manager::onUndo);
        connect(this,&Editor::Redo,&uitab->GetUndo(),&Undo::Manager::onRedo);
    }
    else if(auto uitab = DynamicCast<SceneTab>(tab))
    {
        connect(this,&Editor::Undo,&uitab->GetUndo(),&Undo::Manager::onUndo);
        connect(this,&Editor::Redo,&uitab->GetUndo(),&Undo::Manager::onRedo);
    }
    return tab.Get();
}

Tab* Editor::GetOrCreateTab(StringHash type, const QString& resourceName)
{
    for (auto& tab : tabs_)
    {
        auto resourceTab = DynamicCast<BaseResourceTab>(tab);
        if (resourceTab.NotNull())
        {
            if (resourceTab->GetResourceName() == resourceName)
                return tab.Get();
        }
    }

    auto* tab = CreateTab(type);
    tab->AutoPlace();
    tab->LoadResource(resourceName);
    return tab;
}

QStringList Editor::GetObjectsByCategory(const QString& category)
{
    QStringList result;
    const auto& factories = GetContext()->GetObjectFactories();
    auto it = GetContext()->GetObjectCategories().find(category);
    if (it != GetContext()->GetObjectCategories().end())
    {
        for (const StringHash& type : it->second)
        {
            auto jt = factories.find(type);
            if (jt != factories.end())
                result.push_back(jt->second->GetTypeName());
        }
    }
    return result;
}

QString Editor::GetResourceAbsolutePath(const QString& resourceName, const QString& defaultResult, const char* patterns,
    const QString& dialogTitle)
{
    QString resourcePath = resourceName.isEmpty() ? defaultResult : resourceName;
    QString fullPath;
    if (!resourcePath.isEmpty())
        fullPath = GetContext()->m_ResourceCache->GetResourceFileName(resourcePath);

    if (!fullPath.isEmpty())
        return fullPath;

    return QFileDialog::getSaveFileName(nullptr,dialogTitle,QString(),patterns);
}

void Editor::OnConsoleCommand(const QString &cmd,const QString &src)
{
    if (cmd == QLatin1String("revision"))
        URHO3D_LOGINFOF("Engine revision: %s", "BLOBLLOB");
}

void Editor::LoadDefaultLayout()
{
    tabs_.clear();

    ui::LoadDock(JSONValue::EMPTY);

    // These dock sizes are bullshit. Actually visible sizes do not match these numbers. Instead of spending
    // time implementing this functionality in ImGuiDock proper values were written down and then tweaked until
    // they looked right. Insertion order is also important here when specifying dock placement location.
    auto screenSize = GetContext()->m_Graphics->GetSize();
    auto* inspector = new InspectorTab(GetContext());
    inspector->Initialize("Inspector", {screenSize.x_ * 0.6f, (float)screenSize.y_ * 0.9f}, ui::Slot_Right);
    auto* hierarchy = new HierarchyTab(GetContext());
    hierarchy->Initialize("Hierarchy", {screenSize.x_ * 0.05f, screenSize.y_ * 0.5f}, ui::Slot_Left);
    auto* resources = new ResourceTab(GetContext());
    resources->Initialize("Resources", {screenSize.x_ * 0.05f, screenSize.y_ * 0.15f}, ui::Slot_Bottom, hierarchy->GetUniqueTitle());
    auto* console = new ConsoleTab(GetContext());
    console->Initialize("Console", {screenSize.x_ * 0.6f, screenSize.y_ * 0.4f}, ui::Slot_Left, inspector->GetUniqueTitle());
    auto* preview = new PreviewTab(GetContext());
    preview->Initialize("Game", {screenSize.x_ * 0.6f, (float)screenSize.y_ * 0.1f}, ui::Slot_Bottom, inspector->GetUniqueTitle());

    tabs_.push_back(SharedPtr<Tab>(inspector));
    tabs_.push_back(SharedPtr<Tab>(hierarchy));
    tabs_.push_back(SharedPtr<Tab>(resources));
    tabs_.push_back(SharedPtr<Tab>(console));
    tabs_.push_back(SharedPtr<Tab>(preview));
}

Project* Editor::OpenProject(const QString& projectPath)
{
    OnCloseProject();
    project_ = new Project(GetContext());
    GetContext()->RegisterSubsystem(project_);
    if (!project_->LoadProject(projectPath))
        OnCloseProject();
    return project_.Get();
}

void Editor::OnCloseProject()
{
    GetContext()->RemoveSubsystem<Project>();
    project_.Reset();
    tabs_.clear();
}

void Editor::HandleHotkeys()
{
    if (ui::IsAnyItemActive())
        return;

    auto* input = GetContext()->m_InputSystem.get();
    if (input->GetQualifierDown(QUAL_CTRL))
    {
        if (input->GetKeyPress(KEY_Y) || (input->GetQualifierDown(QUAL_SHIFT) && input->GetKeyPress(KEY_Z)))
        {
            emit Redo(M_MAX_UNSIGNED);
        }
        else if (input->GetKeyPress(KEY_Z))
        {
            emit Undo(0);
        }
    }
}

void Editor::RenderProjectPluginsMenu()
{
    unsigned possiblePluginCount = 0;
    QStringList files;
    GetContext()->m_FileSystem->ScanDir(files, GetContext()->m_FileSystem->GetProgramDir(), "*.*", SCAN_FILES, false);
    for (auto & file : files)
    {
        if (!file.endsWith(".so")
            // TODO: .net/windows && !it->EndsWith(".dll")
            // TODO: MacOS && !it->EndsWith("dylib")
        )
            continue;

        int dotIndex = file.lastIndexOf('.');
        if (dotIndex == 0 || dotIndex == -1)
            continue;

        QString baseName = GetFileName(file);
        if (baseName.back().isDigit())
            // Native plugins will rename main file and append version after base name.
            continue;

        if (baseName.endsWith("CSharp"))
            // Libraries for C# interop
            continue;

#if __linux__ || __APPLE__
        if (baseName.startsWith("lib"))
            baseName = baseName.mid(3);
#endif

        if (baseName == "Urho3D" || baseName == "Toolbox")
            // Internal engine libraries
            continue;

        ++possiblePluginCount;

        PluginManager* plugins = project_->GetPlugins();
        Plugin* plugin = plugins->GetPlugin(file);
        bool loaded = plugin != nullptr;
        if (ui::Checkbox(qPrintable(file), &loaded))
        {
            if (loaded)
                plugins->Load(file);
            else
                plugins->Unload(plugin);
        }
    }

    if (possiblePluginCount == 0)
    {
        ui::TextUnformatted("No available files.");
        ui::SetHelpTooltip("Plugins are shared libraries that have a class inheriting from PluginApplication and "
                           "define a plugin entry point. Look at Samples/103_GamePlugin for more information.");
    }
}

Tab* Editor::GetTab(StringHash type)
{
    for (auto& tab : tabs_)
    {
        if (tab->GetType() == type)
            return tab.Get();
    }
    return nullptr;
}
namespace Urho3D {
Editor *getEditorInstance()
{
    return g_editor_instance;
}
}
