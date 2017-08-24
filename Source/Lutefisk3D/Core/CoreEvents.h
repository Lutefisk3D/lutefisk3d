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

#include <jlsignal/Signal.h>

namespace Urho3D
{

struct CoreSignals
{
    /// Application-wide logic update event.
    jl::Signal<float> update; // float TimeStep
    /// Application-wide logic post-update event.
    jl::Signal<float> postUpdate; // float TimeStep
    /// Render update event.
    jl::Signal<float> renderUpdate; // float TimeStep
    /// Post-render update event.
    jl::Signal<float> postRenderUpdate; // float TimeStep
    /// Frame end event.
    jl::Signal<> endFrame;
    /// Frame begin event.
    jl::Signal<unsigned,float> beginFrame; //unsigned FrameNumber,float timeStep
    void init(jl::ScopedAllocator *alloc) {
        update.SetAllocator(alloc);
        postUpdate.SetAllocator(alloc);
        renderUpdate.SetAllocator(alloc);
        postRenderUpdate.SetAllocator(alloc);
        endFrame.SetAllocator(alloc);
        beginFrame.SetAllocator(alloc);
    }
};
extern CoreSignals g_coreSignals;

}
