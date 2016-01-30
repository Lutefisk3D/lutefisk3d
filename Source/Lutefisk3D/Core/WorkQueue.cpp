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

#include "../Core/CoreEvents.h"
#include "../IO/Log.h"
#include "../Core/ProcessUtils.h"
#include "../Core/Profiler.h"
#include "../Core/Thread.h"
#include "../Core/Timer.h"
#include "../Core/WorkQueue.h"

namespace Urho3D
{

/// Worker thread managed by the work queue.
class WorkerThread : public Thread, public RefCounted
{
public:
    /// Construct.
    WorkerThread(WorkQueue* owner, unsigned index) :
        owner_(owner),
        index_(index)
    {
    }

    /// Process work items until stopped.
    virtual void ThreadFunction() override
    {
        // Init FPU state first
        InitFPU();
        owner_->ProcessItems(index_);
    }

    /// Return thread index.
    unsigned GetIndex() const { return index_; }

private:
    /// Work queue.
    WorkQueue* owner_;
    /// Thread index.
    unsigned index_;
};

WorkQueue::WorkQueue(Context* context) :
    Object(context),
    shutDown_(false),
    pausing_(false),
    paused_(false),
    completing_(false),
    tolerance_(10),
    lastSize_(0),
    maxNonThreadedWorkMs_(5)
{
    SubscribeToEvent(E_BEGINFRAME, URHO3D_HANDLER(WorkQueue, HandleBeginFrame));
}

WorkQueue::~WorkQueue()
{
    // Stop the worker threads. First make sure they are not waiting for work items
    shutDown_ = true;
    Resume();

    for (unsigned i = 0; i < threads_.size(); ++i)
        threads_[i]->Stop();
}

void WorkQueue::CreateThreads(unsigned numThreads)
{
    // Other subsystems may initialize themselves according to the number of threads.
    // Therefore allow creating the threads only once, after which the amount is fixed
    if (!threads_.empty())
        return;

    // Start threads in paused mode
    Pause();

    for (unsigned i = 0; i < numThreads; ++i)
    {
        SharedPtr<WorkerThread> thread(new WorkerThread(this, i + 1));
        thread->Run();
        threads_.push_back(thread);
    }
}

SharedPtr<WorkItem> WorkQueue::GetFreeItem()
{
    if (poolItems_.size() > 0)
    {
        SharedPtr<WorkItem> item(poolItems_.front());
        poolItems_.pop_front();
        return item;
    }
    else
    {
        // No usable items found, create a new one set it as pooled and return it.
        SharedPtr<WorkItem> item(new WorkItem());
        item->pooled_ = true;
        return item;
    }
}

void WorkQueue::AddWorkItem(SharedPtr<WorkItem> item)
{
    if (!item)
    {
        URHO3D_LOGERROR("Null work item submitted to the work queue");
        return;
    }

    // Check for duplicate items.
    assert(std::find(workItems_.begin(),workItems_.end(),item)==workItems_.end());

    // Push to the main thread list to keep item alive
    // Clear completed flag in case item is reused
    workItems_.insert(item);
    item->completed_ = false;

    // Make sure worker threads' list is safe to modify
    if (threads_.size() && !paused_)
        queueMutex_.Acquire();

    queue_.insert(item);

    if (threads_.size())
    {
        queueMutex_.Release();
        paused_ = false;
    }
}

bool WorkQueue::RemoveWorkItem(SharedPtr<WorkItem> item)
{
    if (!item)
        return false;

    MutexLock lock(queueMutex_);

    // Can only remove successfully if the item was not yet taken by threads for execution
    imsWorkitem i = queue_.find(item.Get());
    if (i != queue_.end())
    {
        auto j = workItems_.find(item);
        if (j != workItems_.end())
        {
            queue_.erase(i);
            ReturnToPool(item);
            workItems_.erase(j);
            return true;
        }
    }

    return false;
}

unsigned WorkQueue::RemoveWorkItems(const std::vector<SharedPtr<WorkItem> >& items)
{
    MutexLock lock(queueMutex_);
    unsigned removed = 0;

    for (std::vector<SharedPtr<WorkItem> >::const_iterator i = items.begin(); i != items.end(); ++i)
    {
        imsWorkitem j = queue_.find(i->Get());
        if (j != queue_.end())
        {
            std::multiset<SharedPtr<WorkItem>,comparePrioritySharedPtr>::iterator k = workItems_.find(*i);
            if (k != workItems_.end())
            {
                queue_.erase(j);
                SharedPtr<WorkItem> itm(*k);
                ReturnToPool(itm);
                workItems_.erase(k);
                ++removed;
            }
        }
    }

    return removed;
}
void WorkQueue::Pause()
{
    if (!paused_)
    {
        pausing_ = true;

        queueMutex_.Acquire();
        paused_ = true;

        pausing_ = false;
    }
}

void WorkQueue::Resume()
{
    if (paused_)
    {
        queueMutex_.Release();
        paused_ = false;
    }
}


void WorkQueue::Complete(unsigned priority)
{
    completing_ = true;

    if (threads_.size())
    {
        Resume();

        // Take work items also in the main thread until queue empty or no high-priority items anymore
        while (!queue_.empty())
        {
            queueMutex_.Acquire();
            if (!queue_.empty() && (*queue_.begin())->priority_ >= priority)
            {
                WorkItem* item = (*queue_.begin());
                queue_.erase(queue_.begin());
                queueMutex_.Release();
                item->workFunction_(item, 0);
                item->completed_ = true;
            }
            else
            {
                queueMutex_.Release();
                break;
            }
        }

        // Wait for threaded work to complete
        while (!IsCompleted(priority))
        {
            Time::Sleep(0); // TODO: check this Sleep
        }

        // If no work at all remaining, pause worker threads by leaving the mutex locked
        if (queue_.empty())
            Pause();
    }
    else
    {
        // No worker threads: ensure all high-priority items are completed in the main thread
        while (!queue_.empty() && (*queue_.begin())->priority_ >= priority)
        {
            WorkItem* item = (*queue_.begin());
            queue_.erase(queue_.begin());
            item->workFunction_(item, 0);
            item->completed_ = true;
        }
    }

    PurgeCompleted(priority);
    completing_ = false;
}

bool WorkQueue::IsCompleted(unsigned priority) const
{
    for (const SharedPtr<WorkItem> & elem : workItems_)
    {
        if ((elem)->priority_ >= priority && !(elem)->completed_)
            return false;
    }

    return true;
}

void WorkQueue::ProcessItems(unsigned threadIndex)
{
    bool wasActive = false;

    for (;;)
    {
        if (shutDown_)
            return;

        if (pausing_ && !wasActive)
            Time::Sleep(0);
        else
        {
            queueMutex_.Acquire();
            if (!queue_.empty())
            {
                wasActive = true;

                WorkItem* item = (*queue_.begin());
                queue_.erase(queue_.begin());
                queueMutex_.Release();
                item->workFunction_(item, threadIndex);
                item->completed_ = true;
            }
            else
            {
                wasActive = false;

                queueMutex_.Release();
                Time::Sleep(0);
            }
        }
    }
}

void WorkQueue::PurgeCompleted(unsigned priority)
{
    // Purge completed work items and send completion events. Do not signal items lower than priority threshold,
    // as those may be user submitted and lead to eg. scene manipulation that could happen in the middle of the
    // render update, which is not allowed
    for (auto i = workItems_.begin(); i != workItems_.end();)
    {
        SharedPtr<WorkItem> workitem(*i);
        if (workitem->completed_ && workitem->priority_ >= priority)
        {
            if (workitem->sendEvent_)
            {
                using namespace WorkItemCompleted;

                VariantMap& eventData = GetEventDataMap();
                eventData[P_ITEM] = i->Get();
                SendEvent(E_WORKITEMCOMPLETED, eventData);
            }

            ReturnToPool(workitem);

            i = workItems_.erase(i);
        }
        else
            ++i;
    }
}

void WorkQueue::PurgePool()
{
    unsigned currentSize = poolItems_.size();
    int difference = lastSize_ - currentSize;

    // Difference tolerance, should be fairly significant to reduce the pool size.
    for (unsigned i = 0; poolItems_.size() > 0 && difference > tolerance_ && i < (unsigned)difference; i++)
        poolItems_.pop_front();

    lastSize_ = currentSize;
}

void WorkQueue::ReturnToPool(SharedPtr<WorkItem>& item)
{
    // Check if this was a pooled item and set it to usable
    if (item->pooled_)
    {
        // Reset the values to their defaults. This should
        // be safe to do here as the completed event has
        // already been handled and this is part of the
        // internal pool.
        item->start_ = nullptr;
        item->end_ = nullptr;
        item->aux_ = nullptr;
        item->workFunction_ = nullptr;
        item->priority_ = std::numeric_limits<unsigned int>::max();
        item->sendEvent_ = false;
        item->completed_ = false;

        poolItems_.push_back(item);
    }
}
void WorkQueue::HandleBeginFrame(StringHash eventType, VariantMap& eventData)
{
    // If no worker threads, complete low-priority work here
    if (threads_.empty() && !queue_.empty())
    {
        URHO3D_PROFILE(CompleteWorkNonthreaded);

        HiresTimer timer;

        while (!queue_.empty() && timer.GetUSecS() < maxNonThreadedWorkMs_ * 1000)
        {
            WorkItem* item = (*queue_.begin());
            queue_.erase(queue_.begin());
            item->workFunction_(item, 0);
            item->completed_ = true;
        }
    }

    // Complete and signal items down to the lowest priority
    PurgeCompleted(0);
    PurgePool();
}

}
