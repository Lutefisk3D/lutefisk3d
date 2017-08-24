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

#include <jlsignal/Signal.h>

class QString;

namespace Urho3D
{
class StringHash;
class Resource;
struct ResourceSignals {
    /// Language changed.
    jl::Signal<> changeLanguage;
    /// Resource loading failed.
    jl::Signal<const QString &> loadFailed; //const QString & ResourceName
    /// Resource not found.
    jl::Signal<const QString &> resourceNotFound; //const QString & ResourceName
    /// Unknown resource type.
    jl::Signal<StringHash> unknownResourceType; //StringHash resourceType
    /// Resource background loading finished.
    jl::Signal<const QString &,bool,Resource *> resourceBackgroundLoaded; //const QString &ResourceName, bool Success,Resource *
    /// Tracked file changed in the resource directories.
    jl::Signal<const QString &,const QString &> fileChanged; //const QString &fileName, const QString &ResourceName
    void init(jl::ScopedAllocator *allocator)
    {
        changeLanguage.SetAllocator(allocator);
        loadFailed.SetAllocator(allocator);
        resourceNotFound.SetAllocator(allocator);
        unknownResourceType.SetAllocator(allocator);
        resourceBackgroundLoaded.SetAllocator(allocator);
        fileChanged.SetAllocator(allocator);
    }
};
struct SingleResourceSignals {
    /// reloadStarted - Resource reloading started.
    jl::Signal<> reloadStarted;
    /// Resource reloading finished successfully.
    jl::Signal<> reloadFinished;
    /// Resource reloading failed.
    jl::Signal<> reloadFailed;
};
extern ResourceSignals g_resourceSignals;
}
