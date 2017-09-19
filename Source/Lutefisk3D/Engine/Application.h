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

#include "Lutefisk3D/Core/Object.h"
#include "Lutefisk3D/Core/Main.h"
#include "Lutefisk3D/Core/Variant.h"
#include "jlsignal/Signal.h"

namespace Urho3D
{

class Engine;
enum LogLevels : int32_t;

class LUTEFISK3D_EXPORT Application : public jl::SignalObserver
{
public:
    Application(const QString &appName,Context* context);
    virtual ~Application();
    /// Setup before engine initialization. This is a chance to eg. modify the engine parameters. Call ErrorExit() to terminate without initializing the engine. Called by Application.
    virtual void Setup() {}
    /// Setup after engine initialization and before running the main loop. Call ErrorExit() to terminate without running the main loop. Called by Application.
    virtual void Start() {}
    /// Cleanup after the main loop. Called by Application.
    virtual void Stop() {}

    /// Initialize the engine and run the main loop, then return the application exit code. Catch out-of-memory exceptions while running.
    int Run();
    /// Show an error message (last log message if empty), terminate the main loop, and set failure exit code.
    void ErrorExit(const QString& message = QString());

protected:
    /// Handle log message.
    void HandleLogMessage(LogLevels level, const QString &message);

    Context *         m_context;
    QString           m_appName; //!< Application name.
    Engine *          engine_;
    VariantMap        engineParameters_; //!< Engine parameters map.
    QString           startupErrors_;    //!< Collected startup error log messages.
    int               exitCode_;         //!< Application exit code.
};

// Macro for defining a main function which creates a Context and the application, then runs it
#define URHO3D_DEFINE_APPLICATION_MAIN(className) \
int RunApplication() \
{ \
    std::unique_ptr<Urho3D::Context> context(new Urho3D::Context()); \
    std::unique_ptr<className> application(new className(context.get())); \
    return application->Run(); \
} \
URHO3D_DEFINE_MAIN(RunApplication());

}
