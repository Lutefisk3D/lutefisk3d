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
#ifndef _WIN32
#include <pthread.h>
typedef pthread_t ThreadID;
#else
typedef unsigned ThreadID;
#endif

namespace Urho3D
{

/// Operating system thread.
class LUTEFISK3D_EXPORT Thread
{
public:
    /// Construct. Does not start the thread yet.
    Thread();
    /// Destruct. If running, stop and wait for thread to finish.
    virtual ~Thread();

    /// The function to run in the thread.
    virtual void ThreadFunction() = 0;

    bool Run();
    void Stop();
    void SetPriority(int priority);
    /// Return whether thread exists.
    bool IsStarted() const { return handle_ != nullptr; }
    ThreadID ThreadId() const;
    static void SetMainThread();
    static ThreadID GetCurrentThreadID();
    static bool IsMainThread();

protected:
    void *          handle_;      ///< Thread handle.
    volatile bool   shouldRun_;   ///< Running flag.
    static ThreadID mainThreadID; ///< Main thread's thread ID.
};
}
