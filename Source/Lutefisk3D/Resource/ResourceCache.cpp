//
// Copyright (c) 2008-2015 the Urho3D project.
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

#include "../Resource/BackgroundLoader.h"
#include "../Core/Context.h"
#include "../Core/CoreEvents.h"
#include "../IO/FileSystem.h"
#include "../IO/FileWatcher.h"
#include "../Resource/Image.h"
#include "../Resource/JSONFile.h"
#include "../IO/Log.h"
#include "../IO/PackageFile.h"
#include "../Resource/PListFile.h"
#include "../Core/Profiler.h"
#include "../Resource/ResourceCache.h"
#include "../Resource/ResourceEvents.h"
#include "../Core/WorkQueue.h"
#include "../Resource/XMLFile.h"

namespace Urho3D
{

static const char* checkDirs[] = {
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
    Object(context),
    autoReloadResources_(false),
    returnFailedResources_(false),
    searchPackagesFirst_(true),
    finishBackgroundResourcesMs_(5)
{
    // Register Resource library object factories
    RegisterResourceLibrary(context_);

    // Create resource background loader. Its thread will start on the first background request
    backgroundLoader_ = new BackgroundLoader(this);

    // Subscribe BeginFrame for handling directory watchers and background loaded resource finalization
    SubscribeToEvent(E_BEGINFRAME, HANDLER(ResourceCache, HandleBeginFrame));
}

ResourceCache::~ResourceCache()
{
    // Shut down the background loader first
    backgroundLoader_.Reset();
}

bool ResourceCache::AddResourceDir(const QString& pathName, unsigned priority)
{
    MutexLock lock(resourceMutex_);

    FileSystem* fileSystem = GetSubsystem<FileSystem>();
    if (!fileSystem || !fileSystem->DirExists(pathName))
    {
        LOGERROR("Could not open directory " + pathName);
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
        SharedPtr<FileWatcher> watcher(new FileWatcher(context_));
        watcher->StartWatching(fixedPath, true);
        fileWatchers_.push_back(watcher);
    }

    LOGINFO("Added resource path " + fixedPath);
    return true;
}

bool ResourceCache::AddPackageFile(PackageFile* package, unsigned priority)
{
    MutexLock lock(resourceMutex_);

    // Do not add packages that failed to load
    if (!package || !package->GetNumFiles())
        return false;

    if (priority < packages_.size())
        packages_.insert(packages_.begin()+priority, SharedPtr<PackageFile>(package));
    else
        packages_.push_back(SharedPtr<PackageFile>(package));

    LOGINFO("Added resource package " + package->GetName());
    return true;
}

bool ResourceCache::AddPackageFile(const QString& fileName, unsigned priority)
{
    SharedPtr<PackageFile> package(new PackageFile(context_));
    return package->Open(fileName) && AddPackageFile(package);
}

bool ResourceCache::AddManualResource(Resource* resource)
{
    if (!resource)
    {
        LOGERROR("Null manual resource");
        return false;
    }

    const QString& name = resource->GetName();
    if (name.isEmpty())
    {
        LOGERROR("Manual resource with empty name, can not add");
        return false;
    }

    resource->ResetUseTimer();
    resourceGroups_[resource->GetType()].resources_[resource->GetNameHash()] = resource;
    UpdateResourceGroup(resource->GetType());
    return true;
}

void ResourceCache::RemoveResourceDir(const QString& pathName)
{
    MutexLock lock(resourceMutex_);

    QString fixedPath = SanitateResourceDirName(pathName);
    QStringList::iterator i = resourceDirs_.begin(),
            fin = resourceDirs_.end();
    std::vector<SharedPtr<FileWatcher> >::iterator j,fin_j = fileWatchers_.end();
    while (i != fin)
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
            LOGINFO("Removed resource path " + fixedPath);
            return;
        }
        ++i;
    }
}

void ResourceCache::RemovePackageFile(PackageFile* package, bool releaseResources, bool forceRelease)
{
    MutexLock lock(resourceMutex_);

    for (std::vector<SharedPtr<PackageFile> >::iterator i = packages_.begin(); i != packages_.end(); ++i)
    {
        if (*i == package)
        {
            if (releaseResources)
                ReleasePackageResources(*i, forceRelease);
            LOGINFO("Removed resource package " + (*i)->GetName());
            packages_.erase(i);
            return;
        }
    }
}

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
            LOGINFO("Removed resource package " + (*i)->GetName());
            packages_.erase(i);
            return;
        }
    }
}

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

bool ResourceCache::ReloadResource(Resource* resource)
{
    if (!resource)
        return false;

    resource->SendEvent(E_RELOADSTARTED);

    bool success = false;
    SharedPtr<File> file = GetFile(resource->GetName());
    if (file)
        success = resource->Load(*(file.Get()));

    if (success)
    {
        resource->ResetUseTimer();
        UpdateResourceGroup(resource->GetType());
        resource->SendEvent(E_RELOADFINISHED);
        return true;
    }

    // If reloading failed, do not remove the resource from cache, to allow for a new live edit to
    // attempt loading again
    resource->SendEvent(E_RELOADFAILED);
    return false;
}

void ResourceCache::ReloadResourceWithDependencies(const QString& fileName)
{
    StringHash fileNameHash(fileName);
    // If the filename is a resource we keep track of, reload it
    const SharedPtr<Resource>& resource = FindResource(fileNameHash);
    if (resource)
    {
        LOGDEBUG("Reloading changed resource " + fileName);
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
            LOGDEBUG("Reloading resource " + dependents[k]->GetName() + " depending on " + fileName);
            ReloadResource(dependents[k]);
        }
    }
}

void ResourceCache::SetMemoryBudget(StringHash type, unsigned budget)
{
    resourceGroups_[type].memoryBudget_ = budget;
}

void ResourceCache::SetAutoReloadResources(bool enable)
{
    if (enable != autoReloadResources_)
    {
        if (enable)
        {
            for (unsigned i = 0; i < resourceDirs_.size(); ++i)
            {
                SharedPtr<FileWatcher> watcher(new FileWatcher(context_));
                watcher->StartWatching(resourceDirs_[i], true);
                fileWatchers_.push_back(watcher);
            }
        }
        else
            fileWatchers_.clear();

        autoReloadResources_ = enable;
    }
}

void ResourceCache::SetReturnFailedResources(bool enable)
{
    returnFailedResources_ = enable;
}

SharedPtr<File> ResourceCache::GetFile(const QString& nameIn, bool sendEventOnFailure)
{
    MutexLock lock(resourceMutex_);

    QString name = SanitateResourceName(nameIn);
    if (resourceRouter_)
        resourceRouter_->Route(name, RESOURCE_GETFILE);

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
        if (resourceRouter_ && name.isEmpty() && !nameIn.isEmpty())
            LOGERROR("Resource request " + nameIn + " was blocked");
        else
            LOGERROR("Could not find resource " + name);

        if (Thread::IsMainThread())
        {
            using namespace ResourceNotFound;

            VariantMap& eventData = GetEventDataMap();
            eventData[P_RESOURCENAME] = name.length() ? name : nameIn;
            SendEvent(E_RESOURCENOTFOUND, eventData);
        }
    }

    return SharedPtr<File>();
}

Resource* ResourceCache::GetExistingResource(StringHash type, const QString& nameIn)
{
    QString name = SanitateResourceName(nameIn);

    if (!Thread::IsMainThread())
    {
        LOGERROR("Attempted to get resource " + name + " from outside the main thread");
        return 0;
    }

    // If empty name, return null pointer immediately
    if (name.isEmpty())
        return 0;

    StringHash nameHash(name);

    const SharedPtr<Resource>& existing = FindResource(type, nameHash);
    return existing;
}
Resource* ResourceCache::GetResource(StringHash type, const QString& nameIn, bool sendEventOnFailure)
{
    QString name = SanitateResourceName(nameIn);

    if (!Thread::IsMainThread())
    {
        LOGERROR("Attempted to get resource " + name + " from outside the main thread");
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
    resource = DynamicCast<Resource>(context_->CreateObject(type));
    if (!resource)
    {
        LOGERROR(QString("Could not load unknown resource type ") + type.ToString());

        if (sendEventOnFailure)
        {
            using namespace UnknownResourceType;

            VariantMap& eventData = GetEventDataMap();
            eventData[P_RESOURCETYPE] = type;
            SendEvent(E_UNKNOWNRESOURCETYPE, eventData);
        }

        return nullptr;
    }

    // Attempt to load the resource
    SharedPtr<File> file = GetFile(name, sendEventOnFailure);
    if (!file)
        return nullptr;   // Error is already logged

    LOGDEBUG("Loading resource " + name);
    resource->SetName(name);

    if (!resource->Load(*(file.Get())))
    {
        // Error should already been logged by corresponding resource descendant class
        if (sendEventOnFailure)
        {
            using namespace LoadFailed;

            VariantMap& eventData = GetEventDataMap();
            eventData[P_RESOURCENAME] = name;
            SendEvent(E_LOADFAILED, eventData);
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

SharedPtr<Resource> ResourceCache::GetTempResource(StringHash type, const QString& nameIn, bool sendEventOnFailure)
{
    QString name = SanitateResourceName(nameIn);

    // If empty name, return null pointer immediately
    if (name.isEmpty())
        return SharedPtr<Resource>();

    SharedPtr<Resource> resource;
    // Make sure the pointer is non-null and is a Resource subclass
    resource = DynamicCast<Resource>(context_->CreateObject(type));
    if (!resource)
    {
        LOGERROR("Could not load unknown resource type " + type.ToString());

        if (sendEventOnFailure)
        {
            using namespace UnknownResourceType;

            VariantMap& eventData = GetEventDataMap();
            eventData[P_RESOURCETYPE] = type;
            SendEvent(E_UNKNOWNRESOURCETYPE, eventData);
        }

        return SharedPtr<Resource>();
    }

    // Attempt to load the resource
    SharedPtr<File> file = GetFile(name, sendEventOnFailure);
    if (!file)
        return SharedPtr<Resource>();  // Error is already logged

    LOGDEBUG("Loading temporary resource " + name);
    resource->SetName(file->GetName());

    if (!resource->Load(*(file.Get())))
    {
        // Error should already been logged by corresponding resource descendant class
        if (sendEventOnFailure)
        {
            using namespace LoadFailed;

            VariantMap& eventData = GetEventDataMap();
            eventData[P_RESOURCENAME] = name;
            SendEvent(E_LOADFAILED, eventData);
        }

        return SharedPtr<Resource>();
    }

    return resource;
}

unsigned ResourceCache::GetNumBackgroundLoadResources() const
{
    return backgroundLoader_->GetNumQueuedResources();
}

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

bool ResourceCache::Exists(const QString& nameIn) const
{
    MutexLock lock(resourceMutex_);

    QString name = SanitateResourceName(nameIn);
    if (resourceRouter_)
        resourceRouter_->Route(name, RESOURCE_CHECKEXISTS);

    if (name.isEmpty())
        return false;

    for (unsigned i = 0; i < packages_.size(); ++i)
    {
        if (packages_[i]->Exists(name))
            return true;
    }

    FileSystem* fileSystem = GetSubsystem<FileSystem>();
    for (unsigned i = 0; i < resourceDirs_.size(); ++i)
    {
        if (fileSystem->FileExists(resourceDirs_[i] + name))
            return true;
    }

    // Fallback using absolute path
    if (fileSystem->FileExists(name))
        return true;

    return false;
}

unsigned ResourceCache::GetMemoryBudget(StringHash type) const
{
    HashMap<StringHash, ResourceGroup>::const_iterator i = resourceGroups_.find(type);
    if (i != resourceGroups_.end())
        return MAP_VALUE(i).memoryBudget_;
    else
        return 0;
}

unsigned ResourceCache::GetMemoryUse(StringHash type) const
{
    HashMap<StringHash, ResourceGroup>::const_iterator i = resourceGroups_.find(type);
    if (i != resourceGroups_.end())
        return MAP_VALUE(i).memoryUse_;
    else
        return 0;
}

unsigned ResourceCache::GetTotalMemoryUse() const
{
    unsigned total = 0;
    for (const auto & elem : resourceGroups_)
        total += ELEMENT_VALUE(elem).memoryUse_;
    return total;
}

QString ResourceCache::GetResourceFileName(const QString& name) const
{
    MutexLock lock(resourceMutex_);

    FileSystem* fileSystem = GetSubsystem<FileSystem>();
    for (unsigned i = 0; i < resourceDirs_.size(); ++i)
    {
        if (fileSystem->FileExists(resourceDirs_[i] + name))
            return resourceDirs_[i] + name;
    }

    return QString();
}

QString ResourceCache::GetPreferredResourceDir(const QString& path) const
{
    QString fixedPath = AddTrailingSlash(path);

    bool pathHasKnownDirs = false;
    bool parentHasKnownDirs = false;

    FileSystem* fileSystem = GetSubsystem<FileSystem>();

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
    FileSystem* fileSystem = GetSubsystem<FileSystem>();
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
        fixedPath = GetSubsystem<FileSystem>()->GetCurrentDir() + fixedPath;

    // Sanitate away /./ construct
    fixedPath.replace("/./", "/");

    return fixedPath.trimmed();
}

void ResourceCache::StoreResourceDependency(Resource* resource, const QString& dependency)
{
    // If resource reloading is not on, do not create the dependency data structure (saves memory)
    if (!resource || !autoReloadResources_)
        return;

    MutexLock lock(resourceMutex_);

    StringHash nameHash(resource->GetName());
    QSet<StringHash>& dependents = dependentResources_[dependency];
    dependents.insert(nameHash);
}

void ResourceCache::ResetDependencies(Resource* resource)
{
    if (!resource || !autoReloadResources_)
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
            LOGDEBUG("Resource group " + MAP_VALUE(oldestResource)->GetTypeName() +
                     " over memory budget, releasing resource " + MAP_VALUE(oldestResource)->GetName());
            resources.erase(oldestResource);
        }
        else
            break;
    }
}

void ResourceCache::HandleBeginFrame(StringHash eventType, VariantMap& eventData)
{
    for (unsigned i = 0; i < fileWatchers_.size(); ++i)
    {
        QString fileName;
        while (fileWatchers_[i]->GetNextChange(fileName))
        {
            ReloadResourceWithDependencies(fileName);

            // Finally send a general file changed event even if the file was not a tracked resource
            using namespace FileChanged;

            VariantMap& eventData = GetEventDataMap();
            eventData[P_FILENAME] = fileWatchers_[i]->GetPath() + fileName;
            eventData[P_RESOURCENAME] = fileName;
            SendEvent(E_FILECHANGED, eventData);
        }
    }

    // Check for background loaded resources that can be finished
    {
        PROFILE(FinishBackgroundResources);
        backgroundLoader_->FinishResources(finishBackgroundResourcesMs_);
    }
}

File* ResourceCache::SearchResourceDirs(const QString& nameIn)
{
    FileSystem* fileSystem = GetSubsystem<FileSystem>();
    for (unsigned i = 0; i < resourceDirs_.size(); ++i)
    {
        if (fileSystem->FileExists(resourceDirs_[i] + nameIn))
        {
            // Construct the file first with full path, then rename it to not contain the resource path,
            // so that the file's name can be used in further GetFile() calls (for example over the network)
            File* file(new File(context_, resourceDirs_[i] + nameIn));
            file->SetName(nameIn);
            return file;
        }
    }

    // Fallback using absolute path
    if (fileSystem->FileExists(nameIn))
        return new File(context_, nameIn);

    return nullptr;
}

File* ResourceCache::SearchPackages(const QString& nameIn)
{
    for (unsigned i = 0; i < packages_.size(); ++i)
    {
        if (packages_[i]->Exists(nameIn))
            return new File(context_, packages_[i], nameIn);
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
