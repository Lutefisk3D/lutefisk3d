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

#include "Lutefisk3D/Core/Mutex.h"
#include "Lutefisk3D/Container/Ptr.h"
#include "Lutefisk3D/Engine/jlsignal/Signal.h"
#include <vector>
#include <deque>
#include <set>
namespace Urho3D
{
class Context;
class WorkerThread;
struct LUTEFISK3D_EXPORT WorkItem;

struct WorkQueueSignals {
    /// Work item completed event.
    jl::Signal<WorkItem *> workItemCompleted; //WorkItem ptr
};


/// Work queue item.
struct LUTEFISK3D_EXPORT WorkItem : public RefCounted
{
    friend class WorkQueue;

public:
    /// Work function. Called with the work item and thread index (0 = main thread) as parameters.
    void (*workFunction_)(const WorkItem*, unsigned);
    /// Data start pointer.
    void* start_;
    /// Data end pointer.
    void* end_;
    /// Auxiliary data pointer.
    void* aux_;
    /// Priority. Higher value = will be completed first.
    unsigned priority_=0;
    /// Whether to send event on completion.
    bool sendEvent_=false;
    /// Completed flag.
    volatile bool completed_=false;

private:
    bool pooled_=false;
};
struct comparePriority {
    bool operator()(const WorkItem *a,const WorkItem *b) const {
        return a->priority_ < b->priority_;
    }
    bool operator()(const SharedPtr<WorkItem> &a,const SharedPtr<WorkItem> &b) const {
        return a->priority_ < b->priority_;
    }
    bool operator()(const WorkItem *a,unsigned int b) const {
        return a->priority_ < b;
    }
    bool operator()(unsigned int b,const WorkItem *a) const {
        return b < a->priority_;
    }
};
struct comparePrioritySharedPtr {
    bool operator()(const SharedPtr<WorkItem> &a,const SharedPtr<WorkItem> &b) const {
        return a->priority_ < b->priority_;
    }
};
/// Work queue subsystem for multithreading.
class WorkQueue : public RefCounted,public jl::SignalObserver,public WorkQueueSignals
{
    friend class WorkerThread;

public:
    WorkQueue(Context* context);
    ~WorkQueue();

    /// Create worker threads. Can only be called once.
    void CreateThreads(unsigned numThreads);
    /// Get pointer to an usable WorkItem from the item pool. Allocate one if no more free items.
    SharedPtr<WorkItem> GetFreeItem();
    /// Add a work item and resume worker threads.
    void AddWorkItem(SharedPtr<WorkItem> item);
    /// Remove a work item before it has started executing. Return true if successfully removed.
    bool RemoveWorkItem(SharedPtr<WorkItem> item);
    /// Remove a number of work items before they have started executing. Return the number of items successfully removed.
    unsigned RemoveWorkItems(const std::vector<SharedPtr<WorkItem> >& items);
    /// Pause worker threads.
    void Pause();
    /// Resume worker threads.
    void Resume();
    /// Finish all queued work which has at least the specified priority. Main thread will also execute priority work. Pause worker threads if no more work remains.
    void Complete(unsigned priority);
    /// Set the pool telerance before it starts deleting pool items.
    void SetTolerance(int tolerance) { tolerance_ = tolerance; }
    /// Set how many milliseconds maximum per frame to spend on low-priority work, when there are no worker threads.
    void SetNonThreadedWorkMs(int ms) { maxNonThreadedWorkMs_ = std::max(ms, 1); }

    /// Return number of worker threads.
    unsigned GetNumThreads() const { return threads_.size(); }
    /// Return whether all work with at least the specified priority is finished.
    bool IsCompleted(unsigned priority) const;
    /// Return whether the queue is currently completing work in the main thread.
    bool IsCompleting() const { return completing_; }
    /// Return the pool tolerance.
    int GetTolerance() const { return tolerance_; }
    /// Return how many milliseconds maximum to spend on non-threaded low-priority work.
    int GetNonThreadedWorkMs() const { return maxNonThreadedWorkMs_; }

private:
    /// Process work items until shut down. Called by the worker threads.
    void ProcessItems(unsigned threadIndex);
    /// Purge completed work items which have at least the specified priority, and send completion events as necessary.
    void PurgeCompleted(unsigned priority);
    /// Purge the pool to reduce allocation where its unneeded.
    void PurgePool();
    /// Return a work item to the pool.
    void ReturnToPool(SharedPtr<WorkItem>& item);
    /// Handle frame start event. Purge completed work from the main thread queue, and perform work if no threads at all.
    void HandleBeginFrame(unsigned, float);

    Context *m_context;
    /// Worker threads.
    std::vector<SharedPtr<WorkerThread> > threads_;
    /// Work item pool for reuse to cut down on allocation. The bool is a flag for item pooling and whether it is available or not.
    std::deque<SharedPtr<WorkItem> > poolItems_;
    /// Work item collection. Accessed only by the main thread.
    std::multiset<SharedPtr<WorkItem>,comparePrioritySharedPtr> workItems_;
    /// Work item prioritized queue for worker threads. Pointers are guaranteed to be valid (point to workItems.)
    std::multiset<WorkItem*,comparePriority> queue_;
    typedef std::multiset<WorkItem*,comparePriority>::iterator iQueue;
    /// Worker queue mutex.
    Mutex queueMutex_;
    /// Shutting down flag.
    volatile bool shutDown_;
    /// Pausing flag. Indicates the worker threads should not contend for the queue mutex.
    volatile bool pausing_;
    /// Paused flag. Indicates the queue mutex being locked to prevent worker threads using up CPU time.
    bool paused_;
    /// Completing work in the main thread flag.
    bool completing_;
    /// Tolerance for the shared pool before it begins to deallocate.
    int tolerance_;
    /// Last size of the shared pool.
    unsigned lastSize_;
    /// Maximum milliseconds per frame to spend on low-priority work, when there are no worker threads.
    int maxNonThreadedWorkMs_;
};
typedef std::multiset<WorkItem*,comparePriority> msWorkitem;
typedef msWorkitem::iterator imsWorkitem;
}
