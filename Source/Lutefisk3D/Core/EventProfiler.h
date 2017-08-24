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

#include "Lutefisk3D/Core/Profiler.h"
#include "Lutefisk3D/Math/StringHash.h"
#include "Lutefisk3D/Core/EventNameRegistrar.h"
namespace Urho3D
{

class URHO3D_API EventProfilerBlock : public ProfilerBlock
{
public:
    EventProfilerBlock(EventProfilerBlock* parent, StringHash eventID) :
        ProfilerBlock(parent, EventNameRegistrar::GetEventName(eventID)),
        eventID_(eventID)
    {
    }

    EventProfilerBlock* GetChild(StringHash eventID)
    {
        for (std::vector<ProfilerBlock*>::iterator i = children_.begin(); i != children_.end(); ++i)
        {
            EventProfilerBlock* eventProfilerBlock = static_cast<EventProfilerBlock*>(*i);
            if (eventProfilerBlock->eventID_ == eventID)
                return eventProfilerBlock;
        }

        EventProfilerBlock* newBlock = new EventProfilerBlock(this, eventID);
        children_.push_back(newBlock);

        return newBlock;
    }
    StringHash eventID_;
};

/// Hierarchical performance event profiler subsystem.
class URHO3D_API EventProfiler : public Profiler
{
public:
    /// Construct.
    EventProfiler(Context* context);

    /// Activate the event profiler to collect information. This incurs slight performance hit on each SendEvent. By default inactive.
    static void SetActive(bool newActive) { active = newActive; }
    /// Return true if active.
    static bool IsActive() { return active; }

    /// Begin timing a profiling block based on an event ID.
    void BeginBlock(StringHash eventID)
    {
        // Profiler supports only the main thread currently
        if (!Thread::IsMainThread())
            return;

        current_ = static_cast<EventProfilerBlock*>(current_)->GetChild(eventID);
        current_->Begin();
    }

private:
    /// Profiler active. Default false.
    static bool active;
};

}
