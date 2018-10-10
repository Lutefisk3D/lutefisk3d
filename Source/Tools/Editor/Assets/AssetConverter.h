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

#include "ImportAsset.h"

#include <Lutefisk3D/Core/Object.h>
#include <Lutefisk3D/IO/FileWatcher.h>
#include <Lutefisk3D/Resource/XMLFile.h>

#include <memory>

namespace Urho3D
{

class AssetConversionParameters;

class AssetConverter : public Object
{
    URHO3D_OBJECT(AssetConverter, Object);
public:
    /// Construct.
    explicit AssetConverter(Context* context);
    /// Destruct.
    ~AssetConverter() override;

    /// Set cache path. Converted assets will be placed there.
    void SetCachePath(const QString& cachePath);
    /// Returns asset cache path.
    QString GetCachePath() const;
    /// Watch directory for changed assets and automatically convert them.
    void AddAssetDirectory(const QString& path);
    /// Stop watching directory for changed assets.
    void RemoveAssetDirectory(const QString& path);
    /// Request checking of all assets and convert out of date assets.
    void VerifyCacheAsync();
    /// Request conversion of single asset.
    void ConvertAssetAsync(const QString& resourceName);

protected:
    /// Converts asset. Blocks calling thread.
    bool ConvertAsset(const QString& resourceName);
    /// Returns true if asset in the cache folder is missing or out of date.
    bool IsCacheOutOfDate(const QString& resourceName);
    /// Return a list of converted assets in the cache.
    QStringList GetCacheAssets(const QString& resourceName);
    /// Watches for changed files and requests asset conversion if needed.
    void DispatchChangedAssets();
    /// Handle console commands.
    void OnConsoleCommand(const QString &command,const QString &id);

    /// List of file watchers responsible for watching game data folders for asset changes.
    std::vector<SharedPtr<FileWatcher>> watchers_;
    /// Timer used for delaying out of date asset checks.
    Timer checkTimer_;
    /// Absolute path to asset cache.
    QString cachePath_;
    /// Registered asset importers.
    std::vector<std::shared_ptr<ImportAsset>> assetImporters_;
};

}
