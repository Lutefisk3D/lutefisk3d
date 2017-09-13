//
// Copyright (c) 2008-2016 the Urho3D project.
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

#include "Lutefisk3D/Core/Lutefisk3D.h"
#include "Lutefisk3D/Core/Mutex.h"
#include "Lutefisk3D/Container/HashMap.h"
#include "Lutefisk3D/Container/Ptr.h"      // for SharedPtr
#include "Lutefisk3D/Math/StringHash.h"    // for StringHash
#include <jlsignal/SignalBase.h>
#include <QtCore/QSet>
#include <deque>

namespace Urho3D
{
class Context;
class BackgroundLoader;
class FileWatcher;
class PackageFile;
class LUTEFISK3D_EXPORT Resource;
class LUTEFISK3D_EXPORT File;
/// Sets to priority so that a package or file is pushed to the end of the vector.
static const unsigned PRIORITY_LAST = 0xffffffff;

/// Container of resources with specific type.
struct ResourceGroup
{
    uint64_t memoryBudget_ = 0;                          ///< Memory budget.
    uint64_t memoryUse_    = 0;                          ///< Current memory use.
    HashMap<StringHash, SharedPtr<Resource>> resources_; ///< Resources.
};

/// Resource request types.
enum ResourceRequest
{
    RESOURCE_CHECKEXISTS = 0,
    RESOURCE_GETFILE = 1
};

/// Optional resource request processor. Can deny requests, re-route resource file names, or perform other processing per request.
class LUTEFISK3D_EXPORT ResourceRouter : public RefCounted
{
public:
    /// Construct.
    ResourceRouter(Context* context) :
        m_context(context)
    {
    }

    /// Process the resource request and optionally modify the resource name string. Empty name string means the resource is not found or not allowed.
    virtual void Route(QString& name, ResourceRequest requestType) = 0;
protected:
    Context *m_context;
};

/// %Resource cache subsystem. Loads resources on demand and stores them for later access.
class LUTEFISK3D_EXPORT ResourceCache : public jl::SignalObserver
{
public:
    /// Construct.
    ResourceCache(Context* context);
    /// Destruct. Free all resources.
    virtual ~ResourceCache();

    bool AddResourceDir(const QString& pathName, unsigned priority = PRIORITY_LAST);
    bool AddPackageFile(PackageFile* package, unsigned priority = PRIORITY_LAST);
    bool AddPackageFile(const QString& fileName, unsigned priority = PRIORITY_LAST);
    bool AddManualResource(Resource* resource);

    void RemoveResourceDir(const QString& pathName);
    void RemovePackageFile(PackageFile* package, bool releaseResources = true, bool forceRelease = false);
    void RemovePackageFile(const QString& fileName, bool releaseResources = true, bool forceRelease = false);
    void ReleaseResource(StringHash type, const QString& name, bool force = false);
    void ReleaseResources(StringHash type, bool force = false);
    void ReleaseResources(StringHash type, const QString& partialName, bool force = false);
    void ReleaseResources(const QString& partialName, bool force = false);
    void ReleaseAllResources(bool force = false);
    bool ReloadResource(Resource* resource);
    void ReloadResourceWithDependencies(const QString &fileName);
    void SetMemoryBudget(StringHash type, uint64_t budget);
    void SetAutoReloadResources(bool enable);
    /// Enable or disable returning resources that failed to load. Default false. This may be useful in editing to not lose resource ref attributes.
    void SetReturnFailedResources(bool enable) { returnFailedResources_ = enable; }
    /// Define whether when getting resources should check package files or directories first. True for packages, false for directories.
    void SetSearchPackagesFirst(bool value) { searchPackagesFirst_ = value; }
    /// Set how many milliseconds maximum per frame to spend on finishing background loaded resources.
    void SetFinishBackgroundResourcesMs(int ms) { finishBackgroundResourcesMs_ = std::max(ms, 1); }
    void AddResourceRouter(ResourceRouter* router, bool addAsFirst = false);
    void RemoveResourceRouter(ResourceRouter* router);
    std::unique_ptr<Urho3D::File> GetFile(const QString& name, bool sendEventOnFailure = true);
    Resource* GetResource(StringHash type, const QString& name, bool sendEventOnFailure = true);
    SharedPtr<Resource> GetTempResource(StringHash type, const QString& name, bool sendEventOnFailure = true);
    bool BackgroundLoadResource(StringHash type, const QString& name, bool sendEventOnFailure = true, Resource* caller = nullptr);
    unsigned GetNumBackgroundLoadResources() const;
    void GetResources(std::vector<Resource*>& result, StringHash type) const;
    Resource* GetExistingResource(StringHash type, const QString& name);
    /// Return all loaded resources.
    const HashMap<StringHash, ResourceGroup>& GetAllResources() const { return resourceGroups_; }
    /// Return added resource load directories.
    const QStringList& GetResourceDirs() const { return resourceDirs_; }
    /// Return added package files.
    const std::vector<SharedPtr<PackageFile> >& GetPackageFiles() const { return packages_; }
    /// Template version of returning a resource by name.
    template <class T> T* GetResource(const QString& name, bool sendEventOnFailure = true);
    /// Template version of returning an existing resource by name.
    template <class T> T* GetExistingResource(const QString& name);
    /// Template version of loading a resource without storing it to the cache.
    template <class T> SharedPtr<T> GetTempResource(const QString& name, bool sendEventOnFailure = true);
    /// Template version of releasing a resource by name.
    template <class T> void ReleaseResource(const QString& name, bool force = false);
    /// Template version of queueing a resource background load.
    template <class T> bool BackgroundLoadResource(const QString& name, bool sendEventOnFailure = true, Resource* caller = nullptr);
    /// Template version of returning loaded resources of a specific type.
    template <class T> void GetResources(std::vector<T*>& result) const;
    bool Exists(const QString& name) const;
    uint64_t GetMemoryBudget(StringHash type) const;
    uint64_t GetMemoryUse(StringHash type) const;
    uint64_t GetTotalMemoryUse() const;
    QString GetResourceFileName(const QString& name) const;
    /// Return whether automatic resource reloading is enabled.
    bool GetAutoReloadResources() const { return autoReloadResources_; }
    /// Return whether resources that failed to load are returned.
    bool GetReturnFailedResources() const { return returnFailedResources_; }
    /// Return whether when getting resources should check package files or directories first.
    bool GetSearchPackagesFirst() const { return searchPackagesFirst_; }
    /// Return how many milliseconds maximum to spend on finishing background loaded resources.
    int GetFinishBackgroundResourcesMs() const { return finishBackgroundResourcesMs_; }
    /// Return a resource router by index.
    ResourceRouter* GetResourceRouter(unsigned index) const;

    /// Return either the path itself or its parent, based on which of them has recognized resource subdirectories.
    QString GetPreferredResourceDir(const QString& path) const;
    /// Remove unsupported constructs from the resource name to prevent ambiguity, and normalize absolute filename to resource path relative if possible.
    QString SanitateResourceName(const QString& name) const;
    /// Remove unnecessary constructs from a resource directory name and ensure it to be an absolute path.
    QString SanitateResourceDirName(const QString& name) const;
    /// Store a dependency for a resource. If a dependency file changes, the resource will be reloaded.
    void StoreResourceDependency(Resource* resource, const QString& dependency);
    /// Reset dependencies for a resource.
    void ResetDependencies(Resource* resource);

    /// Returns a formatted string containing the memory actively used.
    QString PrintMemoryUsage() const;
    Context *GetContext() const { return m_context; }
private:
    const SharedPtr<Resource>& FindResource(StringHash type, StringHash nameHash);
    const SharedPtr<Resource>& FindResource(StringHash nameHash);
    void ReleasePackageResources(PackageFile* package, bool force = false);
    void UpdateResourceGroup(StringHash type);
    void HandleBeginFrame(unsigned FrameNumber, float timeStep);
    File* SearchResourceDirs(const QString& nameIn);
    File* SearchPackages(const QString& nameIn);

    Context *m_context;
    /// Mutex for thread-safe access to the resource directories, resource packages and resource dependencies.
    mutable Mutex resourceMutex_;
    /// Resources by type.
    HashMap<StringHash, ResourceGroup> resourceGroups_;
    /// Resource load directories.
    QStringList resourceDirs_;
    /// File watchers for resource directories, if automatic reloading enabled.
    std::vector<SharedPtr<FileWatcher> > fileWatchers_;
    /// Package files.
    std::vector<SharedPtr<PackageFile> > packages_;
    /// Dependent resources. Only used with automatic reload to eg. trigger reload of a cube texture when any of its faces change.
    HashMap<StringHash, QSet<StringHash> > dependentResources_;
    /// Resource background loader.
    std::unique_ptr<BackgroundLoader> backgroundLoader_;
    /// Resource routers.
    std::deque<SharedPtr<ResourceRouter> > resourceRouters_;

    bool autoReloadResources_; ///< Automatic resource reloading flag.
    bool returnFailedResources_; ///< Return failed resources flag.
    bool searchPackagesFirst_; ///< Search priority flag.
    mutable bool isRouting_; ///< Resource routing flag to prevent endless recursion.
    /// How many milliseconds maximum per frame to spend on finishing background loaded resources.
    int finishBackgroundResourcesMs_;
};

template <class T> T* ResourceCache::GetExistingResource(const QString& name)
{
    return static_cast<T*>(GetExistingResource(T::GetTypeStatic(), name));
}
template <class T> T* ResourceCache::GetResource(const QString& name, bool sendEventOnFailure)
{
    return static_cast<T*>(GetResource(T::GetTypeStatic(), name, sendEventOnFailure));
}
template <class T> void ResourceCache::ReleaseResource(const QString& name, bool force)
{
    ReleaseResource(T::GetTypeStatic(), name, force);
}
template <class T> SharedPtr<T> ResourceCache::GetTempResource(const QString& name, bool sendEventOnFailure)
{
    return StaticCast<T>(GetTempResource(T::GetTypeStatic(), name, sendEventOnFailure));
}

template <class T> bool ResourceCache::BackgroundLoadResource(const QString& name, bool sendEventOnFailure, Resource* caller)
{
    return BackgroundLoadResource(T::GetTypeStatic(), name, sendEventOnFailure, caller);
}

template <class T> void ResourceCache::GetResources(std::vector<T*>& result) const
{
    std::vector<Resource*>& resources = reinterpret_cast<std::vector<Resource*>&>(result);
    GetResources(resources, T::GetTypeStatic());

    // Perform conversion of the returned pointers
    for (unsigned i = 0; i < result.size(); ++i)
    {
        Resource* resource = resources[i];
        result[i] = static_cast<T*>(resource);
    }
}

/// Register Resource library subsystems and objects.
void LUTEFISK3D_EXPORT RegisterResourceLibrary(Context* context);

}
