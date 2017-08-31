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

#include "Lutefisk3D/Resource/BackgroundLoader.h"
#include "Lutefisk3D/Core/Context.h"
#include "Lutefisk3D/Core/CoreEvents.h"
#include "Lutefisk3D/IO/FileSystem.h"
#include "Lutefisk3D/IO/FileWatcher.h"
#include "Lutefisk3D/Resource/Image.h"
#include "Lutefisk3D/Resource/JSONFile.h"
#include "Lutefisk3D/IO/Log.h"
#include "Lutefisk3D/IO/File.h"
#include "Lutefisk3D/IO/PackageFile.h"
#include "Lutefisk3D/Resource/PListFile.h"
#include "Lutefisk3D/Core/Profiler.h"
#include "Lutefisk3D/Resource/ResourceCache.h"
#include "Lutefisk3D/Resource/ResourceEvents.h"
#include "Lutefisk3D/Core/WorkQueue.h"
#include "Lutefisk3D/Resource/XMLFile.h"
#include "Lutefisk3D/Core/StringUtils.h"
#include <QtCore/QString>
namespace Urho3D
{

static const char* checkDirs[] =
{
    "Fonts",
    "Materials",
    "Models",
    "Music",
    "Objects",
    "Particle",
    "PostProcess",
    "RenderPaths",
    "Scenes",
    "Scripts",
    "Sounds",
    "Shaders",
    "Techniques",
    "Textures",
    "UI",
    nullptr
};

static const SharedPtr<Resource> noResource;

ResourceCache::ResourceCache(Context* context) :
    m_context(context),
    autoReloadResources_(false),
    returnFailedResources_(false),
    searchPackagesFirst_(true),
    isRouting_(false),
    finishBackgroundResourcesMs_(5)
{
    // Register Resource library object factories
    RegisterResourceLibrary(m_context);

    // Create resource background loader. Its thread will start on the first background request
    backgroundLoader_ = new BackgroundLoader(this);

    // Subscribe BeginFrame for handling directory watchers and background loaded resource finalization
    g_coreSignals.beginFrame.Connect(this,&ResourceCache::HandleBeginFrame);
}

ResourceCache::~ResourceCache()
{
    // Shut down the background loader first
    backgroundLoader_.Reset();
}
/// Add a resource load directory. Optional priority parameter which will control search order.
bool ResourceCache::AddResourceDir(const QString& pathName, unsigned priority)
{
    MutexLock lock(resourceMutex_);

    FileSystem* fileSystem = m_context->m_FileSystem.get();
    if (!fileSystem || !fileSystem->DirExists(pathName))
    {
        URHO3D_LOGERROR("Could not open directory " + pathName);
        return false;
    }

    // Convert path to absolute
    QString fixedPath = SanitateResourceDirName(pathName);

    // Check that the same path does not already exist
    for (unsigned i = 0; i < resourceDirs_.size(); ++i)
    {
        if (!resourceDirs_[i].compare(fixedPath, Qt::CaseInsensitive))
            return true;
    }

    if (priority < resourceDirs_.size())
        resourceDirs_.insert(resourceDirs_.begin()+priority, fixedPath);
    else
        resourceDirs_.push_back(fixedPath);

    // If resource auto-reloading active, create a file watcher for the directory
    if (autoReloadResources_)
    {
        SharedPtr<FileWatcher> watcher(new FileWatcher(m_context));
        watcher->StartWatching(fixedPath, true);
        fileWatchers_.push_back(watcher);
    }

    URHO3D_LOGINFO("Added resource path " + fixedPath);
    return true;
}
/// Add a package file for loading resources from. Optional priority parameter which will control search order.
bool ResourceCache::AddPackageFile(PackageFile* package, unsigned priority)
{
    MutexLock lock(resourceMutex_);

    // Do not add packages that failed to load
    if (!package || !package->GetNumFiles())
    {
        URHO3D_LOGERROR(QString("Could not add package file %1 due to load failure").arg(package->GetName()));
        return false;
    }

    if (priority < packages_.size())
        packages_.insert(packages_.begin()+priority, SharedPtr<PackageFile>(package));
    else
        packages_.push_back(SharedPtr<PackageFile>(package));

    URHO3D_LOGINFO("Added resource package " + package->GetName());
    return true;
}
/// Add a package file for loading resources from by name. Optional priority parameter which will control search order.
bool ResourceCache::AddPackageFile(const QString& fileName, unsigned priority)
{
    SharedPtr<PackageFile> package(new PackageFile(m_context));
    return package->Open(fileName) && AddPackageFile(package);
}
/// Add a manually created resource. Must be uniquely named within its type.
bool ResourceCache::AddManualResource(Resource* resource)
{
    if (!resource)
    {
        URHO3D_LOGERROR("Null manual resource");
        return false;
    }

    const QString& name = resource->GetName();
    if (name.isEmpty())
    {
        URHO3D_LOGERROR("Manual resource with empty name, can not add");
        return false;
    }

    resource->ResetUseTimer();
    resourceGroups_[resource->GetType()].resources_[resource->GetNameHash()] = resource;
    UpdateResourceGroup(resource->GetType());
    return true;
}
/// Remove a resource load directory.
void ResourceCache::RemoveResourceDir(const QString& pathName)
{
    MutexLock lock(resourceMutex_);

    QString fixedPath = SanitateResourceDirName(pathName);
    QStringList::iterator i = resourceDirs_.begin(),
            fin = resourceDirs_.end();
    std::vector<SharedPtr<FileWatcher> >::iterator j,fin_j = fileWatchers_.end();
    for (;i != fin;++i)
    {
        if (!i->compare(fixedPath, Qt::CaseInsensitive)) {
            resourceDirs_.erase(i);
            // Remove the filewatcher with the matching path
            for (j = fileWatchers_.begin(); j!=fin_j; ++j)
            {
                if (!(*j)->GetPath().compare(fixedPath, Qt::CaseInsensitive))
                {
                    fileWatchers_.erase(j);
                    break;
                }
            }
            URHO3D_LOGINFO("Removed resource path " + fixedPath);
            return;
        }
    }
}
/// Remove a package file. Optionally release the resources loaded from it.
void ResourceCache::RemovePackageFile(PackageFile* package, bool releaseResources, bool forceRelease)
{
    MutexLock lock(resourceMutex_);

    for (std::vector<SharedPtr<PackageFile> >::iterator i = packages_.begin(); i != packages_.end(); ++i)
    {
        if (*i == package)
        {
            if (releaseResources)
                ReleasePackageResources(*i, forceRelease);
            URHO3D_LOGINFO("Removed resource package " + (*i)->GetName());
            packages_.erase(i);
            return;
        }
    }
}
/// Remove a package file by name. Optionally release the resources loaded from it.
void ResourceCache::RemovePackageFile(const QString& fileName, bool releaseResources, bool forceRelease)
{
    MutexLock lock(resourceMutex_);

    // Compare the name and extension only, not the path
    QString fileNameNoPath = GetFileNameAndExtension(fileName);

    for (std::vector<SharedPtr<PackageFile> >::iterator i = packages_.begin(); i != packages_.end(); ++i)
    {
        if (!GetFileNameAndExtension((*i)->GetName()).compare(fileNameNoPath, Qt::CaseInsensitive))
        {
            if (releaseResources)
                ReleasePackageResources(*i, forceRelease);
            URHO3D_LOGINFO("Removed resource package " + (*i)->GetName());
            packages_.erase(i);
            return;
        }
    }
}
/// Release a resource by name.
void ResourceCache::ReleaseResource(StringHash type, const QString& name, bool force)
{
    StringHash nameHash(name);
    const SharedPtr<Resource>& existingRes = FindResource(type, nameHash);
    if (!existingRes)
        return;

    // If other references exist, do not release, unless forced
    if ((existingRes.Refs() == 1 && existingRes.WeakRefs() == 0) || force)
    {
        resourceGroups_[type].resources_.remove(nameHash);
        UpdateResourceGroup(type);
    }
}
/// Release all resources of a specific type.
void ResourceCache::ReleaseResources(StringHash type, bool force)
{
    bool released = false;

    HashMap<StringHash, ResourceGroup>::iterator i = resourceGroups_.find(type);
    if (i == resourceGroups_.end())
        return;
    HashMap<StringHash,SharedPtr<Resource> > &resources(MAP_VALUE(i).resources_);
    for (HashMap<StringHash, SharedPtr<Resource> >::iterator j = resources.begin();
         j != resources.end();)
    {
        // If other references exist, do not release, unless forced
        if ((MAP_VALUE(j)->Refs() == 1 && MAP_VALUE(j)->WeakRefs() == 0) || force)
        {
            j = resources.erase(j);
            released = true;
        }
        else
            ++j;
    }

    if (released)
        UpdateResourceGroup(type);
}
/// Release resources of a specific type and partial name.
void ResourceCache::ReleaseResources(StringHash type, const QString& partialName, bool force)
{
    bool released = false;

    HashMap<StringHash, ResourceGroup>::iterator i = resourceGroups_.find(type);

    if (i != resourceGroups_.end())
    {
        HashMap<StringHash, SharedPtr<Resource> > &resource(MAP_VALUE(i).resources_);
        for (HashMap<StringHash, SharedPtr<Resource> >::iterator j = resource.begin(); j != resource.end();)
        {
            if (MAP_VALUE(j)->GetName().contains(partialName))
            {
                // If other references exist, do not release, unless forced
                if ((MAP_VALUE(j).Refs() == 1 && MAP_VALUE(j).WeakRefs() == 0) || force)
                {
                    j = resource.erase(j);
                    released = true;
                    continue;
                }
            }
            ++j;
        }
    }

    if (released)
        UpdateResourceGroup(type);
}
/// Release resources of all types by partial name.
void ResourceCache::ReleaseResources(const QString& partialName, bool force)
{
    // Some resources refer to others, like materials to textures. Release twice to ensure these get released.
    // This is not necessary if forcing release
    unsigned repeat = force ? 1 : 2;

    while (repeat--)
    {
        for (auto iter = resourceGroups_.begin(),fin=resourceGroups_.end(); iter!=fin; ++iter)
        {
            HashMap<StringHash, SharedPtr<Resource> > & resources(MAP_VALUE(iter).resources_);
            bool released = false;

            for (HashMap<StringHash, SharedPtr<Resource> >::iterator j = resources.begin(); j != resources.end();)
            {
                if (MAP_VALUE(j)->GetName().contains(partialName))
                {
                    // If other references exist, do not release, unless forced
                    if ((MAP_VALUE(j).Refs() == 1 && MAP_VALUE(j).WeakRefs() == 0) || force)
                    {
                        j = resources.erase(j);
                        released = true;
                        continue;
                    }
                }
                ++j;
            }
            if (released)
                UpdateResourceGroup(MAP_KEY(iter));
        }
    }
}
/// Release all resources. When called with the force flag false, releases all currently unused resources.
void ResourceCache::ReleaseAllResources(bool force)
{
    unsigned repeat = force ? 1 : 2;

    while (repeat--)
    {
        for (auto iter = resourceGroups_.begin(),fin=resourceGroups_.end(); iter!=fin; ++iter)
        {
            ResourceGroup & elem(MAP_VALUE(iter));
            bool released = false;

            for (HashMap<StringHash, SharedPtr<Resource> >::iterator j = elem.resources_.begin();
                 j != elem.resources_.end();)
            {
                // If other references exist, do not release, unless forced
                if ((MAP_VALUE(j).Refs() == 1 && MAP_VALUE(j).WeakRefs() == 0) || force)
                {
                    j=elem.resources_.erase(j);
                    released = true;
                }
                else
                    ++j;
            }
            if (released)
                UpdateResourceGroup(MAP_KEY(iter));
        }
    }
}
/// Reload a resource. Return true on success. The resource will not be removed from the cache in case of failure.
bool ResourceCache::ReloadResource(Resource* resource)
{
    if (!resource)
        return false;

    resource->reloadStarted.Emit();

    bool success = false;
    SharedPtr<File> file = GetFile(resource->GetName());
    if (file)
        success = resource->Load(*(file.Get()));

    if (success)
    {
        resource->ResetUseTimer();
        UpdateResourceGroup(resource->GetType());
        resource->reloadFinished.Emit();
        return true;
    }

    // If reloading failed, do not remove the resource from cache, to allow for a new live edit to
    // attempt loading again
    resource->reloadFailed.Emit();
    return false;
}
/// Reload a resource based on filename. Causes also reload of dependent resources if necessary.
void ResourceCache::ReloadResourceWithDependencies(const QString& fileName)
{
    StringHash fileNameHash(fileName);
    // If the filename is a resource we keep track of, reload it
    const SharedPtr<Resource>& resource = FindResource(fileNameHash);
    if (resource)
    {
        URHO3D_LOGDEBUG("Reloading changed resource " + fileName);
        ReloadResource(resource);
    }
    // Always perform dependency resource check for resource loaded from XML file as it could be used in inheritance
    if (!resource || GetExtension(resource->GetName()) == ".xml")
    {
        // Check if this is a dependency resource, reload dependents
        auto j = dependentResources_.find(fileNameHash);
        if (j == dependentResources_.end())
            return;

        // Reloading a resource may modify the dependency tracking structure. Therefore collect the
        // resources we need to reload first
        std::vector<SharedPtr<Resource> > dependents;
        dependents.reserve(MAP_VALUE(j).size());

        for (const StringHash &k : MAP_VALUE(j))
        {
            const SharedPtr<Resource>& dependent = FindResource(k);
            if (dependent)
                dependents.push_back(dependent);
        }

        for (unsigned k = 0; k < dependents.size(); ++k)
        {
            URHO3D_LOGDEBUG("Reloading resource " + dependents[k]->GetName() + " depending on " + fileName);
            ReloadResource(dependents[k]);
        }
    }
}
/// Set memory budget for a specific resource type, default 0 is unlimited.
void ResourceCache::SetMemoryBudget(StringHash type, uint64_t budget)
{
    resourceGroups_[type].memoryBudget_ = budget;
}
/// Enable or disable automatic reloading of resources as files are modified. Default false.
void ResourceCache::SetAutoReloadResources(bool enable)
{
    if (enable != autoReloadResources_)
    {
        if (enable)
        {
            for (unsigned i = 0; i < resourceDirs_.size(); ++i)
            {
                SharedPtr<FileWatcher> watcher(new FileWatcher(m_context));
                watcher->StartWatching(resourceDirs_[i], true);
                fileWatchers_.push_back(watcher);
            }
        }
        else
            fileWatchers_.clear();

        autoReloadResources_ = enable;
    }
}
/// Add a resource router object. By default there is none, so the routing process is skipped.
void ResourceCache::AddResourceRouter(ResourceRouter* router, bool addAsFirst)
{
    // Check for duplicate
    for (unsigned i = 0; i < resourceRouters_.size(); ++i)
    {
        if (resourceRouters_[i] == router)
            return;
    }

    if (addAsFirst)
        resourceRouters_.push_front(SharedPtr<ResourceRouter>(router));
    else
        resourceRouters_.push_back(SharedPtr<ResourceRouter>(router));
}
/// Remove a resource router object.
void ResourceCache::RemoveResourceRouter(ResourceRouter* router)
{
    for (auto iter = resourceRouters_.begin(),fin=resourceRouters_.end(); iter!=fin; ++iter)
    {
        if (*iter == router)
        {
            resourceRouters_.erase(iter);
            return;
        }
    }
}

///
/// \brief Open and return a file from the resource load paths or from inside a package file.
/// If not found, use a fallback search with absolute path.
/// Return null if fails. Can be called from outside the main thread.
/// \param nameIn
/// \param sendEventOnFailure
/// \return
///
SharedPtr<File> ResourceCache::GetFile(const QString& nameIn, bool sendEventOnFailure)
{
    MutexLock lock(resourceMutex_);

    QString name = SanitateResourceName(nameIn);
    if (!isRouting_)
    {
        isRouting_ = true;
        for (unsigned i = 0; i < resourceRouters_.size(); ++i)
            resourceRouters_[i]->Route(name, RESOURCE_GETFILE);
        isRouting_ = false;
    }

    if (name.length())
    {
        File* file = nullptr;

        if (searchPackagesFirst_)
        {
            file = SearchPackages(name);
            if (!file)
                file = SearchResourceDirs(name);
        }
        else
        {
            file = SearchResourceDirs(name);
            if (!file)
                file = SearchPackages(name);
        }

        if (file)
            return SharedPtr<File>(file);
    }

    if (sendEventOnFailure)
    {
        if (!resourceRouters_.empty() && name.isEmpty() && !nameIn.isEmpty())
            URHO3D_LOGERROR("Resource request " + nameIn + " was blocked");
        else
            URHO3D_LOGERROR("Could not find resource " + name);

        if (Thread::IsMainThread())
        {
            g_resourceSignals.resourceNotFound.Emit(name.isEmpty()? nameIn:name);
        }
    }

    return SharedPtr<File>();
}
/// Return an already loaded resource of specific type & name, or null if not found. Will not load if does not exist.
Resource* ResourceCache::GetExistingResource(StringHash type, const QString& nameIn)
{
    QString name = SanitateResourceName(nameIn);

    if (!Thread::IsMainThread())
    {
        URHO3D_LOGERROR("Attempted to get resource " + name + " from outside the main thread");
        return 0;
    }

    // If empty name, return null pointer immediately
    if (name.isEmpty())
        return 0;

    StringHash nameHash(name);

    const SharedPtr<Resource>& existing = FindResource(type, nameHash);
    return existing;
}
///
/// \brief Return a resource by type and name. Load if not loaded yet.
/// Return null if not found or if fails, unless SetReturnFailedResources(true) has been called.
/// Can be called only from the main thread.
/// \param type - resource type
/// \param nameIn - resource location/name
/// \param sendEventOnFailure
/// \return
///
Resource* ResourceCache::GetResource(StringHash type, const QString& nameIn, bool sendEventOnFailure)
{
    QString name = SanitateResourceName(nameIn);

    if (!Thread::IsMainThread())
    {
        URHO3D_LOGERROR("Attempted to get resource " + name + " from outside the main thread");
        return nullptr;
    }

    // If empty name, return null pointer immediately
    if (name.isEmpty())
        return nullptr;

    StringHash nameHash(name);

    // Check if the resource is being background loaded but is now needed immediately
    backgroundLoader_->WaitForResource(type, nameHash);

    const SharedPtr<Resource>& existing = FindResource(type, nameHash);
    if (existing)
        return existing;

    SharedPtr<Resource> resource;
    // Make sure the pointer is non-null and is a Resource subclass
    resource = DynamicCast<Resource>(m_context->CreateObject(type));
    if (!resource)
    {
        URHO3D_LOGERROR(QString("Could not load unknown resource type ") + type.ToString());

        if (sendEventOnFailure)
        {
            g_resourceSignals.unknownResourceType.Emit(type);
        }

        return nullptr;
    }

    // Attempt to load the resource
    SharedPtr<File> file = GetFile(name, sendEventOnFailure);
    if (!file)
        return nullptr;   // Error is already logged

    URHO3D_LOGDEBUG("Loading resource " + name);
    resource->SetName(name);

    if (!resource->Load(*(file.Get())))
    {
        // Error should already been logged by corresponding resource descendant class
        if (sendEventOnFailure)
        {
            g_resourceSignals.loadFailed.Emit(name);
        }

        if (!returnFailedResources_)
            return nullptr;
    }

    // Store to cache
    resource->ResetUseTimer();
    resourceGroups_[type].resources_[nameHash] = resource;
    UpdateResourceGroup(type);

    return resource;
}
///
/// \brief Background load a resource. An event will be sent when complete.
/// Return true if successfully stored to the load queue, false if eg. already exists.
/// Can be called from outside the main thread.
/// \param type
/// \param nameIn
/// \param sendEventOnFailure
/// \param caller
/// \return
///
bool ResourceCache::BackgroundLoadResource(StringHash type, const QString& nameIn, bool sendEventOnFailure, Resource* caller)
{
    // If empty name, fail immediately
    QString name = SanitateResourceName(nameIn);
    if (name.isEmpty())
        return false;

    // First check if already exists as a loaded resource
    StringHash nameHash(name);
    if (FindResource(type, nameHash) != noResource)
        return false;

    return backgroundLoader_->QueueResource(type, name, sendEventOnFailure, caller);
}
///
/// \brief Load a resource without storing it in the resource cache.
/// Can be called from outside the main thread if the resource itself is safe to load completely (it does not possess for example GPU data.)
/// \param type
/// \param nameIn
/// \param sendEventOnFailure
/// \return null if not found or if fails
///
SharedPtr<Resource> ResourceCache::GetTempResource(StringHash type, const QString& nameIn, bool sendEventOnFailure)
{
    QString name = SanitateResourceName(nameIn);

    // If empty name, return null pointer immediately
    if (name.isEmpty())
        return SharedPtr<Resource>();

    SharedPtr<Resource> resource;
    // Make sure the pointer is non-null and is a Resource subclass
    resource = DynamicCast<Resource>(m_context->CreateObject(type));
    if (!resource)
    {
        URHO3D_LOGERROR("Could not load unknown resource type " + type.ToString());

        if (sendEventOnFailure)
        {
            g_resourceSignals.unknownResourceType.Emit(type);
        }

        return SharedPtr<Resource>();
    }

    // Attempt to load the resource
    SharedPtr<File> file = GetFile(name, sendEventOnFailure);
    if (!file)
        return SharedPtr<Resource>();  // Error is already logged

    URHO3D_LOGDEBUG("Loading temporary resource " + name);
    resource->SetName(file->GetName());

    if (!resource->Load(*(file.Get())))
    {
        // Error should already been logged by corresponding resource descendant class
        if (sendEventOnFailure)
        {
            g_resourceSignals.loadFailed.Emit(name);
        }

        return SharedPtr<Resource>();
    }

    return resource;
}
/// Return number of pending background-loaded resources.
unsigned ResourceCache::GetNumBackgroundLoadResources() const
{
    return backgroundLoader_->GetNumQueuedResources();
}
/// Return all loaded resources of a specific type.
void ResourceCache::GetResources(std::vector<Resource*>& result, StringHash type) const
{
    result.clear();
    HashMap<StringHash, ResourceGroup>::const_iterator i = resourceGroups_.find(type);
    if (i != resourceGroups_.end())
    {
        for (const auto & elem : MAP_VALUE(i).resources_)
            result.push_back(ELEMENT_VALUE(elem));
    }
}
/// Return whether a file exists in the resource directories or package files. Does not check manually added
/// in-memory resources.
bool ResourceCache::Exists(const QString& nameIn) const
{
    MutexLock lock(resourceMutex_);

    QString name = SanitateResourceName(nameIn);
    if (!isRouting_)
    {
        isRouting_ = true;
        for (unsigned i = 0; i < resourceRouters_.size(); ++i)
            resourceRouters_[i]->Route(name, RESOURCE_CHECKEXISTS);
        isRouting_ = false;
    }

    if (name.isEmpty())
        return false;

    for (unsigned i = 0; i < packages_.size(); ++i)
    {
        if (packages_[i]->Exists(name))
            return true;
    }

    FileSystem* fileSystem = m_context->m_FileSystem.get();
    for (unsigned i = 0; i < resourceDirs_.size(); ++i)
    {
        if (fileSystem->FileExists(resourceDirs_[i] + name))
            return true;
    }

    // Fallback using absolute path
    return fileSystem->FileExists(name);
}
/// Return memory budget for a resource type.
uint64_t ResourceCache::GetMemoryBudget(StringHash type) const
{
    HashMap<StringHash, ResourceGroup>::const_iterator i = resourceGroups_.find(type);
    return i != resourceGroups_.end() ? MAP_VALUE(i).memoryBudget_ : 0;
}
/// Return total memory use for a resource type.
uint64_t ResourceCache::GetMemoryUse(StringHash type) const
{
    HashMap<StringHash, ResourceGroup>::const_iterator i = resourceGroups_.find(type);
    return i != resourceGroups_.end() ? MAP_VALUE(i).memoryUse_ : 0;
}
/// Return total memory use for all resources.
uint64_t ResourceCache::GetTotalMemoryUse() const
{
    uint64_t total = 0;
    for (const auto & elem : resourceGroups_)
        total += ELEMENT_VALUE(elem).memoryUse_;
    return total;
}
/// Return full absolute file name of resource if possible, or empty if not found.
QString ResourceCache::GetResourceFileName(const QString& name) const
{

    FileSystem* fileSystem = m_context->m_FileSystem.get();
    for (unsigned i = 0; i < resourceDirs_.size(); ++i)
    {
        if (fileSystem->FileExists(resourceDirs_[i] + name))
            return resourceDirs_[i] + name;
    }

    if (IsAbsolutePath(name) && fileSystem->FileExists(name))
        return name;
    else
    return QString();
}

ResourceRouter* ResourceCache::GetResourceRouter(unsigned index) const
{
    return index < resourceRouters_.size() ? resourceRouters_[index] : (ResourceRouter*)0;
}

QString ResourceCache::GetPreferredResourceDir(const QString& path) const
{
    QString fixedPath = AddTrailingSlash(path);

    bool pathHasKnownDirs = false;
    bool parentHasKnownDirs = false;

    FileSystem* fileSystem = m_context->m_FileSystem.get();

    for (unsigned i = 0; checkDirs[i] != nullptr; ++i)
    {
        if (fileSystem->DirExists(fixedPath + checkDirs[i]))
        {
            pathHasKnownDirs = true;
            break;
        }
    }
    if (!pathHasKnownDirs)
    {
        QString parentPath = GetParentPath(fixedPath);
        for (unsigned i = 0; checkDirs[i] != nullptr; ++i)
        {
            if (fileSystem->DirExists(parentPath + checkDirs[i]))
            {
                parentHasKnownDirs = true;
                break;
            }
        }
        // If path does not have known subdirectories, but the parent path has, use the parent instead
        if (parentHasKnownDirs)
            fixedPath = parentPath;
    }

    return fixedPath;
}

QString ResourceCache::SanitateResourceName(const QString& nameIn) const
{
    // Sanitate unsupported constructs from the resource name
    QString name = GetInternalPath(nameIn);
    name.replace("../", "");
    name.replace("./", "");

    // If the path refers to one of the resource directories, normalize the resource name
    FileSystem* fileSystem = m_context->m_FileSystem.get();
    if (resourceDirs_.size())
    {
        QString namePath = GetPath(name);
        QString exePath = fileSystem->GetProgramDir();
        for (unsigned i = 0; i < resourceDirs_.size(); ++i)
        {
            QString relativeResourcePath = resourceDirs_[i];
            if (relativeResourcePath.startsWith(exePath))
                relativeResourcePath = relativeResourcePath.mid(exePath.length());

            if (namePath.startsWith(resourceDirs_[i], Qt::CaseInsensitive))
                namePath = namePath.mid(resourceDirs_[i].length());
            else if (namePath.startsWith(relativeResourcePath, Qt::CaseInsensitive))
                namePath = namePath.mid(relativeResourcePath.length());
        }

        name = namePath + GetFileNameAndExtension(name);
    }

    return name.trimmed();
}

QString ResourceCache::SanitateResourceDirName(const QString& nameIn) const
{
    QString fixedPath = AddTrailingSlash(nameIn);
    if (!IsAbsolutePath(fixedPath))
        fixedPath = m_context->m_FileSystem->GetCurrentDir() + fixedPath;

    // Sanitate away /./ construct
    fixedPath.replace("/./", "/");

    return fixedPath.trimmed();
}

void ResourceCache::StoreResourceDependency(Resource* resource, const QString& dependency)
{
    if (!resource)
        return;

    MutexLock lock(resourceMutex_);

    StringHash nameHash(resource->GetName());
    QSet<StringHash>& dependents = dependentResources_[dependency];
    dependents.insert(nameHash);
}

void ResourceCache::ResetDependencies(Resource* resource)
{
    if (!resource)
        return;

    MutexLock lock(resourceMutex_);

    StringHash nameHash(resource->GetName());

    for (auto i = dependentResources_.begin(); i !=dependentResources_.end();)
    {
        QSet<StringHash>& dependents = MAP_VALUE(i);
        dependents.remove(nameHash);
        if (dependents.isEmpty())
            i = dependentResources_.erase(i);
        else
            ++i;
    }
}

QString ResourceCache::PrintMemoryUsage() const
{
    QString output = "Resource Type                 Cnt       Avg       Max    Budget     Total\n\n";
    char outputLine[256];

    unsigned totalResourceCt = 0;
    unsigned long long totalLargest = 0;
    unsigned long long totalAverage = 0;
    unsigned long long totalUse = GetTotalMemoryUse();

    for (const auto & entry : resourceGroups_)
    {
        const unsigned resourceCt = ELEMENT_VALUE(entry).resources_.size();
        unsigned long long average = 0;
        if (resourceCt > 0)
            average = ELEMENT_VALUE(entry).memoryUse_ / resourceCt;
        else
            average = 0;
        unsigned long long largest = 0;
        for (auto resIt : ELEMENT_VALUE(entry).resources_)
        {
            if (ELEMENT_VALUE(resIt)->GetMemoryUse() > largest)
                largest = ELEMENT_VALUE(resIt)->GetMemoryUse();
            if (largest > totalLargest)
                totalLargest = largest;
        }

        totalResourceCt += resourceCt;

        const QString countString = QString::number(ELEMENT_VALUE(entry).resources_.size());
        const QString memUseString = GetFileSizeString(average);
        const QString memMaxString = GetFileSizeString(largest);
        const QString memBudgetString = GetFileSizeString(ELEMENT_VALUE(entry).memoryBudget_);
        const QString memTotalString = GetFileSizeString(ELEMENT_VALUE(entry).memoryUse_);
        const QString resTypeName = m_context->GetTypeName(ELEMENT_KEY(entry));

        memset(outputLine, ' ', 256);
        outputLine[255] = 0;
        sprintf(outputLine, "%-28s %4s %9s %9s %9s %9s\n", qPrintable(resTypeName), qPrintable(countString),
                qPrintable(memUseString),
                qPrintable(memMaxString),
                qPrintable(memBudgetString), qPrintable(memTotalString));

        output += ((const char*)outputLine);
    }

    if (totalResourceCt > 0)
        totalAverage = totalUse / totalResourceCt;

    const QString countString(QString::number(totalResourceCt));
    const QString memUseString = GetFileSizeString(totalAverage);
    const QString memMaxString = GetFileSizeString(totalLargest);
    const QString memTotalString = GetFileSizeString(totalUse);

    memset(outputLine, ' ', 256);
    outputLine[255] = 0;
    sprintf(outputLine, "%-28s %4s %9s %9s %9s %9s\n", "All", qPrintable(countString), qPrintable(memUseString),
            qPrintable(memMaxString), "-", qPrintable(memTotalString));
    output += ((const char*)outputLine);

    return output;
}
/// Find a resource.
const SharedPtr<Resource>& ResourceCache::FindResource(StringHash type, StringHash nameHash)
{
    MutexLock lock(resourceMutex_);

    HashMap<StringHash, ResourceGroup>::iterator i = resourceGroups_.find(type);
    if (i == resourceGroups_.end())
        return noResource;
    HashMap<StringHash, SharedPtr<Resource> >::iterator j = MAP_VALUE(i).resources_.find(nameHash);
    if (j == MAP_VALUE(i).resources_.end())
        return noResource;

    return MAP_VALUE(j);
}
/// Find a resource by name only. Searches all type groups.
const SharedPtr<Resource>& ResourceCache::FindResource(StringHash nameHash)
{
    MutexLock lock(resourceMutex_);

    for (auto & elem : resourceGroups_)
    {
        HashMap<StringHash, SharedPtr<Resource> >::iterator j = ELEMENT_VALUE(elem).resources_.find(nameHash);
        if (j != ELEMENT_VALUE(elem).resources_.end())
            return MAP_VALUE(j);
    }

    return noResource;
}
/// Release resources loaded from a package file.
void ResourceCache::ReleasePackageResources(PackageFile* package, bool force)
{
    QSet<StringHash> affectedGroups;

    const HashMap<QString, PackageEntry>& entries = package->GetEntries();
    for (auto nameHash : entries.keys())
    {
        // We do not know the actual resource type, so search all type containers
        for (auto iter = resourceGroups_.begin(),fin=resourceGroups_.end(); iter!=fin; ++iter)
        {
            ResourceGroup & elem(MAP_VALUE(iter));
            HashMap<StringHash, SharedPtr<Resource> >::iterator k = elem.resources_.find(nameHash);
            if (k != elem.resources_.end())
            {
                // If other references exist, do not release, unless forced
                if ((MAP_VALUE(k).Refs() == 1 && MAP_VALUE(k).WeakRefs() == 0) || force)
                {
                    elem.resources_.erase(k);
                    affectedGroups.insert(MAP_KEY(iter));
                }
                break;
            }
        }
    }

    for (StringHash group : affectedGroups)
        UpdateResourceGroup(group);
}
/// Update a resource group. Recalculate memory use and release resources if over memory budget.
void ResourceCache::UpdateResourceGroup(StringHash type)
{
    HashMap<StringHash, ResourceGroup>::iterator i = resourceGroups_.find(type);
    if (i == resourceGroups_.end())
        return;
    HashMap<StringHash, SharedPtr<Resource> > &resources(MAP_VALUE(i).resources_);
    for (;;)
    {
        unsigned totalSize = 0;
        unsigned oldestTimer = 0;
        HashMap<StringHash, SharedPtr<Resource> >::iterator oldestResource = resources.end();

        for (HashMap<StringHash, SharedPtr<Resource> >::iterator j = resources.begin(); j != resources.end(); ++j)
        {
            totalSize += MAP_VALUE(j)->GetMemoryUse();
            unsigned useTimer = MAP_VALUE(j)->GetUseTimer();
            if (useTimer > oldestTimer)
            {
                oldestTimer = useTimer;
                oldestResource = j;
            }
        }

        MAP_VALUE(i).memoryUse_ = totalSize;

        // If memory budget defined and is exceeded, remove the oldest resource and loop again
        // (resources in use always return a zero timer and can not be removed)
        if (MAP_VALUE(i).memoryBudget_ && MAP_VALUE(i).memoryUse_ > MAP_VALUE(i).memoryBudget_ &&
                oldestResource != resources.end())
        {
            URHO3D_LOGDEBUG("Resource group " + MAP_VALUE(oldestResource)->GetTypeName() +
                     " over memory budget, releasing resource " + MAP_VALUE(oldestResource)->GetName());
            resources.erase(oldestResource);
        }
        else
            break;
    }
}
/// Handle begin frame event. Automatic resource reloads and the finalization of background loaded resources are processed here.
void ResourceCache::HandleBeginFrame(unsigned FrameNumber,float timeStep)
{
    for (unsigned i = 0; i < fileWatchers_.size(); ++i)
    {
        QString fileName;
        while (fileWatchers_[i]->GetNextChange(fileName))
        {
            ReloadResourceWithDependencies(fileName);

            // Finally send a general file changed event even if the file was not a tracked resource
            g_resourceSignals.fileChanged.Emit(fileWatchers_[i]->GetPath() + fileName,fileName);
        }
    }

    // Check for background loaded resources that can be finished
    {
        URHO3D_PROFILE_CTX(m_context,FinishBackgroundResources);
        backgroundLoader_->FinishResources(finishBackgroundResourcesMs_);
    }
}
/// Search FileSystem for file.
File* ResourceCache::SearchResourceDirs(const QString& nameIn)
{
    FileSystem* fileSystem = m_context->m_FileSystem.get();
    for (unsigned i = 0; i < resourceDirs_.size(); ++i)
    {
        if (fileSystem->FileExists(resourceDirs_[i] + nameIn))
        {
            // Construct the file first with full path, then rename it to not contain the resource path,
            // so that the file's name can be used in further GetFile() calls (for example over the network)
            File* file(new File(m_context, resourceDirs_[i] + nameIn));
            file->SetName(nameIn);
            return file;
        }
    }

    // Fallback using absolute path
    if (fileSystem->FileExists(nameIn))
        return new File(m_context, nameIn);

    return nullptr;
}
/// Search resource packages for file.
File* ResourceCache::SearchPackages(const QString& nameIn)
{
    for (unsigned i = 0; i < packages_.size(); ++i)
    {
        if (packages_[i]->Exists(nameIn))
            return new File(m_context, packages_[i], nameIn);
    }

    return nullptr;
}

void RegisterResourceLibrary(Context* context)
{
    Image::RegisterObject(context);
    JSONFile::RegisterObject(context);
    PListFile::RegisterObject(context);
    XMLFile::RegisterObject(context);
}

}
