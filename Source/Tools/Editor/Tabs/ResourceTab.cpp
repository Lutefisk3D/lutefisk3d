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
#include "ResourceTab.h"

#include "Editor.h"

#include <IconFontCppHeaders/IconsFontAwesome5.h>
#include <Toolbox/IO/ContentUtilities.h>
#include <Toolbox/SystemUI/ResourceBrowser.h>
#include "Assets/Inspector/MaterialInspector.h"
#include "Tabs/Scene/SceneTab.h"
#include "Tabs/UI/UITab.h"
#include "Tabs/InspectorTab.h"
#include <Lutefisk3D/Resource/ResourceCache.h>
#include <Lutefisk3D/Container/HashMap.h>
#include <Lutefisk3D/IO/File.h>
#include <Lutefisk3D/IO/FileSystem.h>
#include <Lutefisk3D/IO/Log.h>
#include <Lutefisk3D/Graphics/Octree.h>


namespace std
{
template<>
struct hash<Urho3D::ContentType>
{
    inline unsigned int operator()(Urho3D::ContentType key) const
    {
        return std::hash<int>()(int(key));
    }

};
} //
namespace Urho3D
{

static HashMap<ContentType, QString> contentToTabType = {
    {CTYPE_SCENE, QStringLiteral("SceneTab")},
    {CTYPE_UILAYOUT, QStringLiteral("UITab")}
};

unsigned MakeHash(ContentType value)
{
    return (unsigned)value;
}

ResourceTab::ResourceTab(Context* context)
    : Tab(context)
{
    isUtility_ = true;
    SetTitle("Resources");
    m_browser.connect(&m_browser,&ResourceBrowser::ResourceBrowserRename, [&](QString from,QString to) {
        auto* project = GetSubsystem<Project>();
        auto sourceName = project->GetResourcePath() + from;
        auto destName = project->GetResourcePath() + to;

        if (GetCache()->RenameResource(sourceName, destName))
            resourceSelection_ = GetFileNameAndExtension(destName);
        else
            URHO3D_LOGERRORF("Renaming '%s' to '%s' failed.", qPrintable(sourceName), qPrintable(destName));
    });
    m_browser.connect(&m_browser,&ResourceBrowser::ResourceBrowserDelete, [&](QString name) {
        auto* project = GetSubsystem<Project>();
        auto fileName = project->GetResourcePath() + name;
        if (GetFileSystem()->FileExists(fileName))
            GetFileSystem()->Delete(fileName);
        else if (GetFileSystem()->DirExists(fileName))
            GetFileSystem()->RemoveDir(fileName, true);
    });
}

bool ResourceTab::RenderWindowContent()
{
    ResourceBrowser browser;
    auto action = browser.updateAndRender(resourcePath_, resourceSelection_, flags_);
    if (action == RBR_ITEM_OPEN)
    {
        QString selected = resourcePath_ + resourceSelection_;
        auto it = contentToTabType.find(GetContentType(selected));
        if (it != contentToTabType.end())
            getEditorInstance()->GetOrCreateTab(it->second, selected)->Activate();
        else
        {
            QString resourcePath = GetSubsystem<Project>()->GetResourcePath() + selected;
            if (!GetFileSystem()->Exists(resourcePath))
                resourcePath = GetSubsystem<Project>()->GetCachePath() + selected;

            if (GetFileSystem()->Exists(resourcePath))
                GetFileSystem()->SystemOpen(resourcePath);
        }
    }
    else if (action == RBR_ITEM_CONTEXT_MENU)
        ui::OpenPopup("Resource Context Menu");
    else if (action == RBR_ITEM_SELECTED)
    {
        QString selected = resourcePath_ + resourceSelection_;
        switch (GetContentType(selected))
        {
//        case CTYPE_UNKNOWN:break;
//        case CTYPE_SCENE:break;
//        case CTYPE_SCENEOBJECT:break;
//        case CTYPE_UILAYOUT:break;
//        case CTYPE_UISTYLE:break;
//        case CTYPE_MODEL:break;
//        case CTYPE_ANIMATION:break;
        case CTYPE_MATERIAL:
            OpenMaterialInspector(selected);
            break;
//        case CTYPE_PARTICLE:break;
//        case CTYPE_RENDERPATH:break;
//        case CTYPE_SOUND:break;
//        case CTYPE_TEXTURE:break;
//        case CTYPE_TEXTUREXML:break;
        default:
            break;
        }
    }

    flags_ = RBF_NONE;

    if (ui::BeginPopup("Resource Context Menu"))
    {
        if (ui::BeginMenu("Create"))
        {
            if (ui::MenuItem(ICON_FA_FOLDER " Folder"))
            {
                QString newFolderName("New Folder");
                QString path = GetNewResourcePath(resourcePath_ + newFolderName);
                if (GetFileSystem()->CreateDir(path))
                {
                    flags_ |= RBF_RENAME_CURRENT | RBF_SCROLL_TO_CURRENT;
                    resourceSelection_ = newFolderName;
                }
                else
                    URHO3D_LOGERRORF("Failed creating folder '%s'.", qPrintable(path));
            }

            if (ui::MenuItem("Scene"))
            {
                auto path = GetNewResourcePath(resourcePath_ + "New Scene.xml");
                GetFileSystem()->CreateDirsRecursive(GetPath(path));

                SharedPtr<Scene> scene(new Scene(context_));
                scene->CreateComponent<Octree>();
                File file(context_, path, FILE_WRITE);
                if (file.IsOpen())
                {
                    scene->SaveXML(file);
                    flags_ |= RBF_RENAME_CURRENT | RBF_SCROLL_TO_CURRENT;
                    resourceSelection_ = GetFileNameAndExtension(path);
                }
                else
                    URHO3D_LOGERRORF("Failed opening file '%s'.", qPrintable(path));
            }

            if (ui::MenuItem("Material"))
            {
                auto path = GetNewResourcePath(resourcePath_ + "New Material.xml");
                GetFileSystem()->CreateDirsRecursive(GetPath(path));

                SharedPtr<Material> material(new Material(context_));
                File file(context_, path, FILE_WRITE);
                if (file.IsOpen())
                {
                    material->Save(file);
                    flags_ |= RBF_RENAME_CURRENT | RBF_SCROLL_TO_CURRENT;
                    resourceSelection_ = GetFileNameAndExtension(path);
                }
                else
                    URHO3D_LOGERRORF("Failed opening file '%s'.", qPrintable(path));
            }

            if (ui::MenuItem("UI Layout"))
            {
                auto path = GetNewResourcePath(resourcePath_ + "New UI Layout.xml");
                GetFileSystem()->CreateDirsRecursive(GetPath(path));

                SharedPtr<UIElement> scene(new UIElement(context_));
                XMLFile layout(context_);
                auto root = layout.GetOrCreateRoot("element");
                if (scene->SaveXML(root) && layout.SaveFile(path))
                {
                    flags_ |= RBF_RENAME_CURRENT | RBF_SCROLL_TO_CURRENT;
                    resourceSelection_ = GetFileNameAndExtension(path);
                }
                else
                    URHO3D_LOGERRORF("Failed opening file '%s'.", qPrintable(path));
            }

            ui::EndMenu();
        }

        if (ui::MenuItem("Copy Path"))
        {
            UI* ui_sys = context_->m_UISystem.get();
            ui_sys->SetClipboardText(resourcePath_ + resourceSelection_);
        }

        if (ui::MenuItem("Rename", "F2"))
            flags_ |= RBF_RENAME_CURRENT;

        if (ui::MenuItem("Delete", "Del"))
            flags_ |= RBF_DELETE_CURRENT;

        ui::EndPopup();
    }

    return true;
}

void ResourceTab::OpenMaterialInspector(const QString &resourcePath)
{
    auto inspector = new MaterialInspector(context_,GetCache()->GetResource<Material>(resourcePath));
    connect(inspector,&MaterialInspector::InspectorLocateResource,this,[&](const QString& resourceName) {
        resourcePath_ = GetPath(resourceName);
        resourceSelection_ = GetFileNameAndExtension(resourceName);
        flags_ |= RBF_SCROLL_TO_CURRENT;
    });
    
    getEditorInstance()->EditorRenderInspector(IC_RESOURCE,inspector);
}

QString ResourceTab::GetNewResourcePath(const QString& name)
{
    auto* project = GetSubsystem<Project>();
    if (!GetFileSystem()->FileExists(project->GetResourcePath() + name))
        return project->GetResourcePath() + name;

    auto basePath = GetPath(name);
    auto baseName = GetFileName(name);
    auto ext = GetExtension(name, false);
    for (auto i = 1; i < M_MAX_INT; i++)
    {
        auto newName = project->GetResourcePath() + QString("%1%2 %3%4").arg(basePath, baseName).arg(i).arg(ext);
        if (!GetFileSystem()->FileExists(newName))
            return newName;
    }

    std::abort();
}

}
