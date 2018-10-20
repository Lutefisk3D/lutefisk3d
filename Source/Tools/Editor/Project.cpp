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

#include <Lutefisk3D/Resource/XMLFile.h>
#include <Lutefisk3D/Core/StringUtils.h>
#include <Lutefisk3D/Graphics/Graphics.h>
#include <Lutefisk3D/IO/Log.h>
#include <Lutefisk3D/IO/FileSystem.h>
#include <Lutefisk3D/Resource/ResourceCache.h>
#include <Lutefisk3D/Resource/JSONFile.h>
#include <Lutefisk3D/Core/StringHashRegister.h>
#include <Lutefisk3D/SystemUI/SystemUI.h>
#include <Toolbox/SystemUI/ImGuiDock.h>
#include <Tabs/ConsoleTab.h>
#include <Tabs/ResourceTab.h>
#include <Tabs/HierarchyTab.h>
#include <Tabs/InspectorTab.h>

#include "Editor.h"
#include "Project.h"
#include "Tabs/Scene/SceneTab.h"
#include "Tabs/UI/UITab.h"

#undef GetObject
namespace Urho3D
{

Project::Project(Context* context)
    : Object(context)
    , assetConverter_(context)
#if LUTEFISK3D_PLUGINS
    , plugins_(context)
#endif
{
    connect(getEditorInstance(),&Editor::EditorResourceSaved,this,&Project::SaveProject);
}

Project::~Project()
{
    ui::GetIO().IniFilename = nullptr;

    if (auto* cache = GetCache())
    {
        cache->RemoveResourceDir(GetCachePath());
        cache->RemoveResourceDir(GetResourcePath());

        for (const auto& path : cachedEngineResourcePaths_)
            GetCache()->AddResourceDir(path);
    }

    // Clear dock state
    ui::LoadDock(JSONValue::EMPTY);
}

bool Project::LoadProject(const QString& projectPath)
{
    if (!projectFileDir_.isEmpty())
        // Project is already loaded.
        return false;

    if (projectPath.isEmpty())
        return false;

    projectFileDir_ = AddTrailingSlash(projectPath);

    if (!GetFileSystem()->Exists(GetCachePath()))
        GetFileSystem()->CreateDirsRecursive(GetCachePath());

    if (!GetFileSystem()->Exists(GetResourcePath()))
    {
        // Initialize new project
        GetFileSystem()->CreateDirsRecursive(GetResourcePath());

        for (const auto& path : GetCache()->GetResourceDirs())
        {
            if (path.endsWith("/EditorData/") || path.contains("/Autoload/"))
                continue;

            QStringList names;

            URHO3D_LOGINFOF("Importing resources from '%s'", qPrintable(path));

            // Copy default resources to the project.
            GetFileSystem()->ScanDir(names, path, "*", SCAN_FILES, false);
            for (const auto& name : names)
                GetFileSystem()->Copy(path + name, GetResourcePath() + name);

            GetFileSystem()->ScanDir(names, path, "*", SCAN_DIRS, false);
            for (const auto& name : names)
            {
                if (name == "." || name == "..")
                    continue;
                GetFileSystem()->CopyDir(path + name, GetResourcePath() + name);
            }
        }
    }

    // Register asset dirs
    GetCache()->AddResourceDir(GetCachePath(), 0);
    GetCache()->AddResourceDir(GetResourcePath(), 1);
    assetConverter_.SetCachePath(GetCachePath());
    assetConverter_.AddAssetDirectory(GetResourcePath());
    assetConverter_.VerifyCacheAsync();

    // Unregister engine dirs
    auto enginePrefixPath = getEditorInstance()->GetCoreResourcePrefixPath();
    auto pathsCopy = GetCache()->GetResourceDirs();
    cachedEngineResourcePaths_.clear();
    for (const auto& path : pathsCopy)
    {
        if (path.startsWith(enginePrefixPath) && !path.endsWith("/EditorData/"))
        {
            cachedEngineResourcePaths_.push_back(path);
            GetCache()->RemoveResourceDir(path);
        }
    }
    static std::string ini_path_storage;
    uiConfigPath_ = projectPath + "/.ui.ini";
    ini_path_storage=uiConfigPath_.toStdString();
    ui::GetIO().IniFilename = ini_path_storage.c_str();
    QString userSessionPath(projectFileDir_ + ".user.json");

    if (GetFileSystem()->Exists(userSessionPath))
    {
        // Load user session
        JSONFile file(context_);
        if (!file.LoadFile(userSessionPath))
            return false;

        const auto& root = file.GetRoot();
        if (root.IsObject())
        {
            emit getEditorInstance()->EditorProjectLoadingStart();

            const auto& window = root["window"];
            if (window.IsObject())
            {
                auto size = window["size"].GetVariant().GetIntVector2();
                GetContextGraphics()->SetMode(size.x_, size.y_);
                GetContextGraphics()->SetWindowPosition(window["position"].GetVariant().GetIntVector2());
            }

            const auto& tabs = root["tabs"];
            if (tabs.IsArray())
            {
                auto* editor = getEditorInstance();
                for (auto i = 0; i < tabs.Size(); i++)
                {
                    const auto& tab = tabs[i];

                    auto tabType = tab["type"].GetString();
                    auto* runtimeTab = editor->CreateTab(tabType);

                    if (runtimeTab == nullptr)
                        URHO3D_LOGERRORF("Unknown tab type '%s'", qPrintable(tabType));
                    else
                        runtimeTab->OnLoadProject(tab);
                }
            }

            ui::LoadDock(root["docks"]);

            // Plugins may load state by subscribing to this event
            emit getEditorInstance()->EditorProjectLoading(&root);
        }
    }
    else
    {
        // Load default layout if no user session exists
        getEditorInstance()->LoadDefaultLayout();
    }

    // Project.json
    {
        QString filePath(projectFileDir_ + "Project.json");
        if (GetFileSystem()->Exists(filePath))
        {
            JSONFile file(context_);
            if (!file.LoadFile(filePath))
                return false;

            const auto& root = file.GetRoot().GetObject();
            if (hashContains(root,"plugins"))
            {
                const auto& plugins = root.find("plugins")->second.GetArray();
                for (const auto& plugin : plugins)
                    plugins_.Load(plugin.GetString());
            }

            for (const auto& value : file.GetRoot().GetArray())
            {
                // Seed global string hash to name map.
                StringHash hash(value.GetString());
                (void) (hash);
            }
        }
    }

#if LUTEFISK3D_HASH_DEBUG
    // StringHashNames.json
    {
        QString filePath(projectFileDir_ + "StringHashNames.json");
        if (GetFileSystem()->Exists(filePath))
        {
            JSONFile file(context_);
            if (!file.LoadFile(filePath))
                return false;

            for (const auto& value : file.GetRoot().GetArray())
            {
                // Seed global string hash to name map.
                StringHash hash(value.GetString());
                (void) (hash);
            }
        }
    }
#endif

    return true;
}

bool Project::SaveProject()
{
    // Saving project data of tabs may trigger saving resources, which in turn triggers saving editor project. Avoid
    // that loop.
    disconnect(getEditorInstance(),&Editor::EditorResourceSaved,this,&Project::SaveProject);

    if (projectFileDir_.isEmpty())
    {
        URHO3D_LOGERROR("Unable to save project. Project path is empty.");
        return false;
    }

    // .user.json
    {
        JSONFile file(context_);
        JSONValue& root = file.GetRoot();
        root["version"] = 0;

        JSONValue& window = root["window"];
        {
            window["size"].SetVariant(GetContextGraphics()->GetSize());
            window["position"].SetVariant(GetContextGraphics()->GetWindowPosition());
        }

        // Plugins may save state by subscribing to this event
        emit getEditorInstance()->EditorProjectSaving(&root);

        ui::SaveDock(root["docks"]);

        QString filePath(projectFileDir_ + ".user.json");
        if (!file.SaveFile(filePath))
        {
            projectFileDir_.clear();
            URHO3D_LOGERRORF("Saving project to '%s' failed", qPrintable(filePath));
            return false;
        }
    }

    // Project.json
    {
        JSONFile file(context_);
        JSONValue& root = file.GetRoot();

        root["version"] = 0;

        // Plugins
        {
            JSONArray plugins{};
            for (const auto& plugin : plugins_.GetPlugins())
                plugins.push_back(plugin->GetFileName());
            std::sort(plugins.begin(), plugins.end(), [](JSONValue& a, JSONValue& b) {
                return a.GetString().compare(b.GetString())<0;
            });
            root["plugins"] = plugins;
        }

        QString filePath(projectFileDir_ + "Project.json");
        if (!file.SaveFile(filePath))
        {
            projectFileDir_.clear();
            URHO3D_LOGERRORF("Saving project to '%s' failed", qPrintable(filePath));
            return false;
        }
    }

#if LUTEFISK3D_HASH_DEBUG
    // StringHashNames.json
    {
        auto hashNames = StringHash::GetGlobalStringHashRegister()->GetInternalMap().values();
        qSort(hashNames);
        JSONFile file(context_);
        JSONArray names;
        for (const auto& string : hashNames)
            names.push_back(string);
        file.GetRoot() = names;

        QString filePath(projectFileDir_ + "StringHashNames.json");
        if (!file.SaveFile(filePath))
        {
            projectFileDir_.clear();
            URHO3D_LOGERRORF("Saving StringHash names to '%s' failed", qPrintable(filePath));
            return false;
        }
    }
#endif
    connect(getEditorInstance(),&Editor::EditorResourceSaved,this,&Project::SaveProject);

    return true;
}

QString Project::GetCachePath() const
{
    if (projectFileDir_.isEmpty())
        return QString();
    return projectFileDir_ + "Cache/";
}

QString Project::GetResourcePath() const
{
    if (projectFileDir_.isEmpty())
        return QString();
    return projectFileDir_ + "Resources/";
}

}
