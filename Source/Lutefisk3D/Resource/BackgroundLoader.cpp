//
// Copyright (c) 2008-2017 the Urho3D project.
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

#include "BackgroundLoader.h"

#include "ResourceCache.h"
#include "ResourceEvents.h"
#include "Lutefisk3D/Core/Context.h"
#include "Lutefisk3D/Core/Profiler.h"
#include "Lutefisk3D/Core/Timer.h"
#include "Lutefisk3D/IO/Log.h"
#include "Lutefisk3D/IO/File.h"
#include "Lutefisk3D/Resource/Resource.h"
namespace Urho3D
{

BackgroundLoader::BackgroundLoader(ResourceCache* owner) :
    owner_(owner)
{
}

BackgroundLoader::~BackgroundLoader()
{
    MutexLock lock(backgroundLoadMutex_);

    backgroundLoadQueue_.clear();
}
void BackgroundLoader::ThreadFunction()
{
    while (shouldRun_)
    {
        backgroundLoadMutex_.Acquire();

        // Search for a queued resource that has not been loaded yet
        auto i = backgroundLoadQueue_.begin();
        while (i != backgroundLoadQueue_.end())
        {
            if (MAP_VALUE(i).resource_->GetAsyncLoadState() == ASYNC_QUEUED)
                break;
            else
                ++i;
        }

        if (i == backgroundLoadQueue_.end())
        {
            // No resources to load found
            backgroundLoadMutex_.Release();
            Time::Sleep(5);
        }
        else
        {
            BackgroundLoadItem& item = MAP_VALUE(i);
            Resource* resource = item.resource_;
            // We can be sure that the item is not removed from the queue as long as it is in the
            // "queued" or "loading" state
            backgroundLoadMutex_.Release();

            bool success = false;
            std::unique_ptr<File> file = owner_->GetFile(resource->GetName(), item.sendEventOnFailure_);
            if (file)
            {
                resource->SetAsyncLoadState(ASYNC_LOADING);
                success = resource->BeginLoad(*file);
            }

            // Process dependencies now
            // Need to lock the queue again when manipulating other entries
            std::pair<StringHash, StringHash> key(resource->GetType(), resource->GetNameHash());
            backgroundLoadMutex_.Acquire();
            if (item.dependents_.size())
            {
                for (const std::pair<StringHash, StringHash> &dependent : item.dependents_)
                {
                    auto j = backgroundLoadQueue_.find(dependent);
                    if (j != backgroundLoadQueue_.end())
                        MAP_VALUE(j).dependencies_.remove(key);
                }

                item.dependents_.clear();
            }

            resource->SetAsyncLoadState(success ? ASYNC_SUCCESS : ASYNC_FAIL);
            backgroundLoadMutex_.Release();
        }
    }
}

bool BackgroundLoader::QueueResource(StringHash type, const QString& name, bool sendEventOnFailure, Resource* caller)
{
    StringHash nameHash(name);
    std::pair<StringHash, StringHash> key(type, nameHash);

    MutexLock lock(backgroundLoadMutex_);

    // Check if already exists in the queue
    if (backgroundLoadQueue_.find(key) != backgroundLoadQueue_.end())
        return false;

    BackgroundLoadItem& item = backgroundLoadQueue_[key];
    item.sendEventOnFailure_ = sendEventOnFailure;

    // Make sure the pointer is non-null and is a Resource subclass
    item.resource_ = DynamicCast<Resource>(owner_->GetContext()->CreateObject(type));
    if (!item.resource_)
    {
        URHO3D_LOGERROR(QString("Could not load unknown resource type ") + type.ToString());

        if (sendEventOnFailure && Thread::IsMainThread())
        {
            g_resourceSignals.unknownResourceType(type);
        }

        backgroundLoadQueue_.erase(key);
        return false;
    }

    URHO3D_LOGDEBUG("Background loading resource " + name);

    item.resource_->SetName(name);
    item.resource_->SetAsyncLoadState(ASYNC_QUEUED);

    // If this is a resource calling for the background load of more resources, mark the dependency as necessary
    if (caller)
    {
        std::pair<StringHash, StringHash> callerKey(caller->GetType(), caller->GetNameHash());
        auto j = backgroundLoadQueue_.find(callerKey);
        if (j != backgroundLoadQueue_.end())
        {
            BackgroundLoadItem& callerItem = MAP_VALUE(j);
            item.dependents_.insert(callerKey);
            callerItem.dependencies_.insert(key);
        }
        else
            URHO3D_LOGWARNING("Resource " + caller->GetName() + " requested for a background loaded resource but was not in the background load queue");
    }

    // Start the background loader thread now
    if (!IsStarted())
        Run();

    return true;
}

void BackgroundLoader::WaitForResource(StringHash type, StringHash nameHash)
{
    backgroundLoadMutex_.Acquire();

    // Check if the resource in question is being background loaded
    std::pair<StringHash, StringHash> key(type, nameHash);
    auto i = backgroundLoadQueue_.find(key);
    if (i != backgroundLoadQueue_.end())
    {
        backgroundLoadMutex_.Release();

        {
            Resource* resource = MAP_VALUE(i).resource_;
            HiresTimer waitTimer;
            bool didWait = false;

            for (;;)
            {
                unsigned numDeps = MAP_VALUE(i).dependencies_.size();
                AsyncLoadState state = resource->GetAsyncLoadState();
                if (numDeps > 0 || state == ASYNC_QUEUED || state == ASYNC_LOADING)
                {
                    didWait = true;
                    Time::Sleep(1);
                }
                else
                    break;
            }

            if (didWait)
                URHO3D_LOGDEBUG("Waited " + QString::number(waitTimer.GetUSecS() / 1000) + " ms for background loaded resource " + resource->GetName());
        }

        // This may take a long time and may potentially wait on other resources, so it is important we do not hold the mutex during this
        FinishBackgroundLoading(MAP_VALUE(i));

        backgroundLoadMutex_.Acquire();
        backgroundLoadQueue_.erase(i);
        backgroundLoadMutex_.Release();
    }
    else
        backgroundLoadMutex_.Release();
}

void BackgroundLoader::FinishResources(int maxMs)
{
    if (IsStarted())
    {
        HiresTimer timer;

        backgroundLoadMutex_.Acquire();

        for (auto i = backgroundLoadQueue_.begin(); i != backgroundLoadQueue_.end();)
        {
            Resource* resource = MAP_VALUE(i).resource_;
            unsigned numDeps = MAP_VALUE(i).dependencies_.size();
            AsyncLoadState state = resource->GetAsyncLoadState();
            if (numDeps > 0 || state == ASYNC_QUEUED || state == ASYNC_LOADING)
                ++i;
            else
            {
                // Finishing a resource may need it to wait for other resources to load, in which case we can not
                // hold on to the mutex
                backgroundLoadMutex_.Release();
                FinishBackgroundLoading(MAP_VALUE(i));
                backgroundLoadMutex_.Acquire();
                i = backgroundLoadQueue_.erase(i);
            }

            // Break when the time limit passed so that we keep sufficient FPS
            if (timer.GetUSecS() >= maxMs * 1000)
                break;
        }

        backgroundLoadMutex_.Release();
    }
}

unsigned BackgroundLoader::GetNumQueuedResources() const
{
    MutexLock lock(backgroundLoadMutex_);
    return backgroundLoadQueue_.size();
}

void BackgroundLoader::FinishBackgroundLoading(BackgroundLoadItem& item)
{
    Resource* resource = item.resource_;

    bool success = resource->GetAsyncLoadState() == ASYNC_SUCCESS;
    // If BeginLoad() phase was successful, call EndLoad() and get the final success/failure result
    if (success)
    {
#ifdef LUTEFISK3D_PROFILING
        QString profileBlockName("Finish" + resource->GetTypeName());

        Profiler* profiler = owner_->GetContext()->m_ProfilerSystem.get();
        if (profiler)
            profiler->BeginBlock(qPrintable(profileBlockName));
#endif
        URHO3D_LOGDEBUG("Finishing background loaded resource " + resource->GetName());
        success = resource->EndLoad();

#ifdef LUTEFISK3D_PROFILING
        if (profiler)
            profiler->EndBlock();
#endif
    }
    resource->SetAsyncLoadState(ASYNC_DONE);

    if (!success && item.sendEventOnFailure_)
    {
        g_resourceSignals.loadFailed(resource->GetName());
    }

    // Store to the cache just before sending the event; use same mechanism as for manual resources
    if (success || owner_->GetReturnFailedResources())
        owner_->AddManualResource(resource);

    // Send event, either success or failure
    g_resourceSignals.resourceBackgroundLoaded(resource->GetName(),success,resource);

}

}

