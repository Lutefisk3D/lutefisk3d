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

#pragma once

#include "Lutefisk3D/Container/Str.h"
#include "Lutefisk3D/Core/Thread.h"
#include "Lutefisk3D/Core/Timer.h"
#include <limits>
#include <vector>
namespace Urho3D
{

/// Profiling data for one block in the profiling tree.
class LUTEFISK3D_EXPORT ProfilerBlock
{
public:
    /// Construct with the specified parent block and name.
    ProfilerBlock(ProfilerBlock* parent, const char* name) :
        parent_(parent)
    {
        if (name)
        {
            unsigned nameLength = QLatin1String(name).size();
            name_ = new char[nameLength + 1];
            memcpy(name_, name, nameLength + 1);
        }
    }

    /// Destruct. Free the child blocks.
    virtual ~ProfilerBlock()
    {
        for (ProfilerBlock*& i : children_)
        {
            delete i;
            i = nullptr;
        }

        delete [] name_;
    }

    /// Begin timing.
    void Begin()
    {
        timer_.Reset();
        ++count_;
    }

    /// End timing.
    void End()
    {
        int64_t time = timer_.GetUSecS();
        if (time > maxTime_)
            maxTime_ = time;
        time_ += time;
    }

    /// End profiling frame and update interval and total values.
    void EndFrame()
    {
        frameTime_ = time_;
        frameMaxTime_ = maxTime_;
        frameCount_ = count_;
        intervalTime_ += time_;
        if (maxTime_ > intervalMaxTime_)
            intervalMaxTime_ = maxTime_;
        intervalCount_ += count_;
        totalTime_ += time_;
        if (maxTime_ > totalMaxTime_)
            totalMaxTime_ = maxTime_;
        totalCount_ += count_;
        time_ = 0;
        maxTime_ = 0;
        count_ = 0;

        for (ProfilerBlock* elem : children_)
            elem->EndFrame();
    }

    /// Begin new profiling interval.
    void BeginInterval()
    {
        intervalTime_ = 0;
        intervalMaxTime_ = 0;
        intervalCount_ = 0;

        for (ProfilerBlock* elem : children_)
            elem->BeginInterval();
    }

    /// Return child block with the specified name.
    ProfilerBlock* GetChild(const char* name)
    {
        for (ProfilerBlock* elem : children_)
        {
            if (!QString(elem->name_).compare(name))
                return elem;
        }

        ProfilerBlock* newBlock = new ProfilerBlock(this, name);
        children_.push_back(newBlock);

        return newBlock;
    }

    /// Block name.
    char* name_ = nullptr;
    /// High-resolution timer for measuring the block duration.
    HiresTimer timer_;
    /// Time on current frame.
    int64_t time_=0;
    /// Maximum time on current frame.
    int64_t maxTime_=0;
    /// Calls on current frame.
    unsigned count_=0;
    /// Parent block.
    ProfilerBlock* parent_;
    /// Child blocks.
    std::vector<ProfilerBlock*> children_;
    /// Time on the previous frame.
    int64_t frameTime_=0;
    /// Maximum time on the previous frame.
    int64_t frameMaxTime_=0;
    /// Calls on the previous frame.
    unsigned frameCount_=0;
    /// Time during current profiler interval.
    int64_t intervalTime_=0;
    /// Maximum time during current profiler interval.
    int64_t intervalMaxTime_=0;
    /// Calls during current profiler interval.
    unsigned intervalCount_=0;
    /// Total accumulated time.
    int64_t totalTime_=0;
    /// All-time maximum time.
    int64_t totalMaxTime_=0;
    /// Total accumulated calls.
    unsigned totalCount_=0;
};

/// Hierarchical performance profiler subsystem.
class LUTEFISK3D_EXPORT Profiler : public RefCounted
{
public:
    /// Construct.
    Profiler(Context* context);
    /// Destruct.
    virtual ~Profiler();

    /// Begin timing a profiling block.
    void BeginBlock(const char* name)
    {
        // Profiler supports only the main thread currently
        if (!Thread::IsMainThread())
            return;

        current_ = current_->GetChild(name);
        current_->Begin();
    }

    /// End timing the current profiling block.
    void EndBlock()
    {
        if (!Thread::IsMainThread())
            return;

            current_->End();
        if (current_->parent_)
            current_ = current_->parent_;
        }

    /// Begin the profiling frame. Called by HandleBeginFrame().
    void BeginFrame();
    /// End the profiling frame. Called by HandleEndFrame().
    void EndFrame();
    /// Begin a new interval.
    void BeginInterval();

    /// Return profiling data as text output. This method is not thread-safe.
    QString PrintData(bool showUnused = false, bool showTotal = false, unsigned maxDepth = std::numeric_limits<unsigned>::max()) const;
    /// Return the current profiling block.
    const ProfilerBlock* GetCurrentBlock() { return current_; }
    /// Return the root profiling block.
    const ProfilerBlock* GetRootBlock() { return root_; }

protected:
    /// Return profiling data as text output for a specified profiling block.
    void PrintData(ProfilerBlock* block, QString& output, unsigned depth, unsigned maxDepth, bool showUnused, bool showTotal) const;

    /// Current profiling block.
    ProfilerBlock* current_;
    /// Root profiling block.
    ProfilerBlock* root_;
    /// Frames in the current interval.
    unsigned intervalFrames_;
};

/// Helper class for automatically beginning and ending a profiling block
class LUTEFISK3D_EXPORT AutoProfileBlock
{
public:
    /// Construct. Begin a profiling block with the specified name and optional call count.
    AutoProfileBlock(Profiler* profiler, const char* name) :
        profiler_(profiler)
    {
        if (profiler_)
            profiler_->BeginBlock(name);
    }

    /// Destruct. End the profiling block.
    ~AutoProfileBlock()
    {
        if (profiler_)
            profiler_->EndBlock();
    }

private:
    /// Profiler.
    Profiler* profiler_;
};

#ifdef LUTEFISK3D_PROFILING
#define URHO3D_PROFILE(name) Urho3D::AutoProfileBlock profile_ ## name (context_->m_ProfilerSystem.get(), #name)
#define URHO3D_PROFILE_CTX(ctx,name) Urho3D::AutoProfileBlock profile_ ## name (ctx->m_ProfilerSystem.get(), #name)
#else
#define URHO3D_PROFILE(name)
#endif

}
