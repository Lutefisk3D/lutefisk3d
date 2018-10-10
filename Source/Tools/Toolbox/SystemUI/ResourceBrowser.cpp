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

#include "ResourceBrowser.h"
#include "ImGuiDock.h"
#include <Lutefisk3D/SystemUI/SystemUI.h>
#include <Lutefisk3D/Core/Context.h>
#include <Lutefisk3D/Resource/ResourceCache.h>
#include <Lutefisk3D/Resource/ResourceEvents.h>
#include <Lutefisk3D/IO/FileSystem.h>
#include <Lutefisk3D/Input/Input.h>
#include <IconFontCppHeaders/IconsFontAwesome5.h>
#include <Lutefisk3D/IO/Log.h>
#include "Widgets.h"
#include "IO/ContentUtilities.h"

#include <QVector>


namespace Urho3D
{

ResourceBrowserResult ResourceBrowser::updateAndRender(QString& path, QString& selected, ResourceBrowserFlags flags)
{
    struct State
    {
        bool isEditing = false;
        bool wasEditing = false;
        bool deletionPending = false;
        char editBuffer[250]{};
        QString editStartItem;
    };

    auto result = RBR_NOOP;
    auto systemUI = (SystemUI*)ui::GetIO().UserData;
    auto context = systemUI->GetContext();
    auto fs = context->m_FileSystem.get();
    auto& state = *ui::GetUIState<State>();

    if (!selected.isEmpty() && !ui::IsAnyItemActive() && ui::IsWindowFocused())
    {
        if (context->m_InputSystem->GetKeyPress(KEY_F2) || flags & RBF_RENAME_CURRENT)
        {
            state.isEditing = true;
            state.deletionPending = false;
            state.editStartItem = selected;
            strcpy(state.editBuffer, qPrintable(selected));
        }
        if (context->m_InputSystem->GetKeyPress(KEY_DELETE) || flags & RBF_DELETE_CURRENT)
        {
            state.isEditing = false;
            state.deletionPending = true;
            state.editStartItem = selected;
        }
    }
    if (context->m_InputSystem->GetKeyPress(KEY_ESCAPE) || state.editStartItem != selected)
    {
        state.isEditing = false;
        state.deletionPending = false;
    }

    if (state.deletionPending)
    {
        if (ui::Begin("Delete?", &state.deletionPending))
        {
            ui::Text("Would you like to delete '%s%s'?", qPrintable(path), qPrintable(selected));
            ui::TextUnformatted(ICON_FA_EXCLAMATION_TRIANGLE " This action can not be undone!");
            ui::NewLine();

            if (ui::Button("Delete Permanently"))
            {
                emit ResourceBrowserDelete(path+selected);
                state.deletionPending = false;
            }
        }
        ui::End();
    }

    QVector<QString> mergedDirs;
    QVector<QString> mergedFiles;

    QString cacheDir;
    for (const auto& dir: systemUI->GetCache()->GetResourceDirs())
    {
        if (dir.endsWith(QLatin1String("/EditorData/")))
            continue;

        if (dir.endsWith(QLatin1String("/Cache/")))
        {
            cacheDir = dir;
            continue;
        }

        QStringList items;
        fs->ScanDir(items, dir + path, "", SCAN_FILES, false);
        for (const auto& item: items)
        {
            if (!mergedFiles.contains(item))
                mergedFiles.push_back(item);
        }

        items.clear();
        fs->ScanDir(items, dir + path, "", SCAN_DIRS, false);
        items.removeAll(".");
        items.removeAll("..");
        for (const auto& item: items)
        {
            if (!mergedDirs.contains(item))
                mergedDirs.push_back(item);
        }
    }

    auto moveFileDropTarget = [&](const QString& item) {
        if (ui::BeginDragDropTarget())
        {
            auto dropped = ui::AcceptDragDropVariant("path");
            if (dropped.GetType() == VAR_STRING)
            {
                auto newName = AddTrailingSlash(item) + GetFileNameAndExtension(dropped.GetString());
                if (dropped != newName)
                    emit ResourceBrowserRename(dropped.ToString(), newName);
            }
            ui::EndDragDropTarget();
        }
    };

    if (!path.isEmpty())
    {
        switch (ui::DoubleClickSelectable("..", selected == ".."))
        {
        case 1:
            selected = "..";
            break;
        case 2:
            path = GetParentPath(path);
            break;
        default:
            break;
        }

        moveFileDropTarget(GetParentPath(path));
    }

    auto renameWidget = [&](const QString& item, const QString& icon) {
        if (selected == item && state.isEditing)
        {
            ui::TextUnformatted(icon.toUtf8().data());
            ui::SameLine();

            ui::PushStyleVar(ImGuiStyleVar_FramePadding, {0, 0});
            ui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0);

            if (ui::InputText("", state.editBuffer, sizeof(state.editBuffer), ImGuiInputTextFlags_EnterReturnsTrue))
            {
                auto oldName = path + selected;
                auto newName = path + state.editBuffer;
                if (oldName != newName)
                    emit ResourceBrowserRename(oldName, newName);
                state.isEditing = false;
            }

            if (!state.wasEditing)
                ui::GetCurrentWindow()->FocusIdxTabRequestNext = ui::GetCurrentContext()->ActiveId;

            ui::PopStyleVar(2);

            return true;
        }
        return false;
    };
    qSort(mergedDirs);
    for (const auto& item: mergedDirs)
    {
        if (!renameWidget(item, ICON_FA_FOLDER))
        {
            auto isSelected = selected == item;

            if (flags & RBF_SCROLL_TO_CURRENT && isSelected)
                ui::SetScrollHere();

            switch (ui::DoubleClickSelectable((ICON_FA_FOLDER " " + item).toUtf8().data(), isSelected))
            {
            case 1:
                selected = item;
                break;
            case 2:
                path += AddTrailingSlash(item);
                selected.clear();
                break;
            default:
                break;
            }

            if (ui::IsItemActive())
            {
                if (ui::BeginDragDropSource())
                {
                    ui::SetDragDropVariant("path", path + item);

                    // TODO: show actual preview of a resource.
                    ui::Text("%s%s", qPrintable(path), qPrintable(item));

                    ui::EndDragDropSource();
                }
            }

            moveFileDropTarget(path + item);
        }
    }

    auto renderAssetEntry = [&](const QString& item) {
        auto icon = GetFileIcon(item);
        if (!renameWidget(item, icon))
        {
            if (flags & RBF_SCROLL_TO_CURRENT && selected == item)
                ui::SetScrollHere();
            auto title = icon + " " + GetFileNameAndExtension(item);
            switch (ui::DoubleClickSelectable(title.toUtf8().data(), selected == item))
            {
            case 1:
                selected = item;
                result = RBR_ITEM_SELECTED;
                break;
            case 2:
                result = RBR_ITEM_OPEN;
                break;
            default:
                break;
            }

            if (ui::IsItemActive())
            {
                if (ui::BeginDragDropSource())
                {
                    ui::SetDragDropVariant("path", path + item);

                    // TODO: show actual preview of a resource.
                    ui::Text("%s%s", path.toUtf8().data(), item.toUtf8().data());

                    ui::EndDragDropSource();
                }
            }
        }
    };

    qSort(mergedFiles);
    for (const auto& item: mergedFiles)
    {
        if (fs->DirExists(cacheDir + path + item))
        {
            // File is converted asset.
            std::function<void(const QString&)> renderCacheAssetTree = [&](const QString& subPath)
            {
                QString targetPath = cacheDir + path + subPath;

                if (fs->DirExists(targetPath))
                {
                    ui::TextUnformatted(ICON_FA_FOLDER_OPEN);
                    ui::SameLine();
                    if (ui::TreeNode(qPrintable(GetFileNameAndExtension(subPath))))
                    {
                        QStringList files;
                        QStringList dirs;
                        fs->ScanDir(files, targetPath, "", SCAN_FILES, false);
                        fs->ScanDir(dirs, targetPath, "", SCAN_DIRS, false);
                        dirs.removeAll(".");
                        dirs.removeAll("..");
                        qSort(files);
                        qSort(dirs);

                        for (const auto& dir : dirs)
                            renderCacheAssetTree(subPath + "/" + dir);

                        for (const auto& file : files)
                            renderAssetEntry(subPath + "/" + file);

                        ui::TreePop();
                    }
                }
                else
                    renderAssetEntry(subPath);
            };
            renderCacheAssetTree(item);
        }
        else
        {
            // File exists only in data directories.
            renderAssetEntry(item);
        }
    }

    if (ui::IsWindowHovered())
    {
        if (ui::IsMouseClicked(1))
            result = RBR_ITEM_CONTEXT_MENU;

        if ((ui::IsMouseClicked(0) || ui::IsMouseClicked(1)) && !ui::IsAnyItemHovered())
            // Clicking empty area unselects item.
            selected.clear();
    }

    state.wasEditing = state.isEditing;

    return result;
}

}
