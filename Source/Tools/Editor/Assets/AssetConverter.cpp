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

#include <regex>

#include "Editor.h"
#include "AssetConverter.h"
#include "ImportAssimp.h"
#include <Lutefisk3D/Core/CoreEvents.h>
#include <Lutefisk3D/Core/WorkQueue.h>
#include <Lutefisk3D/Resource/ResourceCache.h>
#include <Lutefisk3D/IO/FileSystem.h>
#include <Lutefisk3D/IO/Log.h>
#include <Lutefisk3D/Engine/EngineEvents.h>

namespace Urho3D
{

AssetConverter::AssetConverter(Context* context)
    : Object(context)
{
    assetImporters_.push_back(std::make_shared<ImportAssimp>(context_));

    g_coreSignals.endFrame.Connect(this,&AssetConverter::DispatchChangedAssets);
    g_consoleSignals.consoleCommand.Connect(this,&AssetConverter::OnConsoleCommand);
}

AssetConverter::~AssetConverter()
{
    for (auto& watcher : watchers_)
        watcher->StopWatching();
}

void AssetConverter::AddAssetDirectory(const QString& path)
{
    SharedPtr<FileWatcher> watcher(new FileWatcher(context_));
    watcher->StartWatching(path, true);
    watchers_.push_back(watcher);
}

void AssetConverter::RemoveAssetDirectory(const QString& path)
{
    QString realPath = AddTrailingSlash(path);
    for (auto it = watchers_.begin(); it != watchers_.end();)
    {
        if ((*it)->GetPath() == realPath)
        {
            (*it)->StopWatching();
            it = watchers_.erase(it);
        }
        else
            ++it;
    }
}

void AssetConverter::SetCachePath(const QString& cachePath)
{
    GetFileSystem()->CreateDirsRecursive(cachePath);
    cachePath_ = cachePath;
}

QString AssetConverter::GetCachePath() const
{
    return cachePath_;
}

void AssetConverter::VerifyCacheAsync()
{
    GetWorkQueue()->AddWorkItem([=]() {
        for (const auto& watcher : watchers_)
        {
            QStringList files;
            GetFileSystem()->ScanDir(files, watcher->GetPath(), "*", SCAN_FILES, true);

            for (const auto& file : files)
                ConvertAsset(file);
        }
    });
}

void AssetConverter::ConvertAssetAsync(const QString& resourceName)
{
    GetWorkQueue()->AddWorkItem(std::bind(&AssetConverter::ConvertAsset, this, resourceName));
}

bool AssetConverter::ConvertAsset(const QString& resourceName)
{
    if (!IsCacheOutOfDate(resourceName))
        return true;

    // Ensure that no resources are left over from previous version
    GetFileSystem()->RemoveDir(cachePath_ + resourceName, true);
    QString resourceFileName = GetCache()->GetResourceFileName(resourceName);
    bool convertedAny = false;

    for (auto& importer : assetImporters_)
    {
        if (importer->Accepts(resourceFileName))
        {
            if (importer->Convert(resourceFileName))
                convertedAny = true;
        }
    }

    auto convertedAssets = GetCacheAssets(resourceName);
    if (!convertedAssets.empty())
    {
        unsigned mtime = GetFileSystem()->GetLastModifiedTime(resourceFileName);
        for (const auto& path : GetCacheAssets(resourceName))
        {
            GetFileSystem()->SetLastModifiedTime(path, mtime);
            URHO3D_LOGINFOF("Imported %s", qPrintable(path));
        }
    }

    return convertedAny;
}

void AssetConverter::DispatchChangedAssets()
{
    if (checkTimer_.GetMSec(false) < 3000)
        return;
    checkTimer_.Reset();

    for (auto& watcher : watchers_)
    {
        QString path;
        while (watcher->GetNextChange(path))
            ConvertAssetAsync(path);
    }
}

bool AssetConverter::IsCacheOutOfDate(const QString& resourceName)
{
    unsigned mtime = GetFileSystem()->GetLastModifiedTime(GetCache()->GetResourceFileName(resourceName));

    auto files = GetCacheAssets(resourceName);
    for (const auto& path : files)
    {
        if (GetFileSystem()->GetLastModifiedTime(path) != mtime)
            return true;
    }

    return files.empty();
}

QStringList AssetConverter::GetCacheAssets(const QString& resourceName)
{
    QStringList files;
    QString assetCacheDirectory = cachePath_ + resourceName;
    if (GetFileSystem()->DirExists(assetCacheDirectory))
        GetFileSystem()->ScanDir(files, assetCacheDirectory, "", SCAN_FILES, true);
    for (auto& fileName : files)
        fileName = AddTrailingSlash(assetCacheDirectory) + fileName;
    return files;
}

void AssetConverter::OnConsoleCommand(const QString &command,const QString &id)
{
    if (command == "cache.sync")
        VerifyCacheAsync();
}

}
