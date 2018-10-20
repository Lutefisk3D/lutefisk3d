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

#include "Lutefisk3D/Engine/jlsignal/Signal.h"

class QString;

namespace Urho3D
{
struct ConsoleSignals
{
    /// A command has been entered on the console.
    jl::Signal<const QString &,const QString &> consoleCommand; // QString command, QString Id
    void init(jl::ScopedAllocator *alloc) {
        consoleCommand.SetAllocator(alloc);
    }
};
extern LUTEFISK3D_EXPORT ConsoleSignals g_consoleSignals;
struct EngineSignals
{
    /// Engine finished initialization, but Application::Start() was not called yet.
    jl::Signal<> initialized;
    jl::Signal<> applicationStarted;
    void init(jl::ScopedAllocator *alloc) {
        initialized.SetAllocator(alloc);
        applicationStarted.SetAllocator(alloc);
    }
};
extern EngineSignals LUTEFISK3D_EXPORT g_engineSignals;
}
