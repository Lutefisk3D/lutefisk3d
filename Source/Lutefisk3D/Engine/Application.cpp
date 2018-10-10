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

#include "Application.h"
#include "Engine.h"
#include "EngineEvents.h"
#include "Lutefisk3D/Core/Variant.h"
#include "Lutefisk3D/Core/Context.h"
#include "Lutefisk3D/IO/IOEvents.h"
#include "Lutefisk3D/IO/Log.h"
#include "Lutefisk3D/Core/ProcessUtils.h"
#include "jlsignal/StaticSignalConnectionAllocators.h"
#include <exception>

/**
  \class Urho3D::Application
  \brief Base class for creating applications which initialize the Urho3D engine and run a main loop until exited.
*/

using namespace Urho3D;
/// Construct. Parse default engine parameters from the command line, and create the engine in an uninitialized state.
Application::Application(const QString &appName, Context* context) :
    Object(context),
    m_appName(appName),
    exitCode_(EXIT_SUCCESS)
{
    engineParameters_ = Engine::ParseParameters(GetArguments());

    // Create the Engine, but do not initialize it yet. Subsystems except Graphics & Renderer are registered at this point
    engine_ = new Engine(context);
    SetConnectionAllocator(context->observerAllocator());
    // Subscribe to log messages so that can show errors if ErrorExit() is called with empty message
    g_LogSignals.logMessageSignal.Connect(this, &Application::HandleLogMessage);

}
// This destructor is implemented here, to fix the problem when the compiler was trying to generate
// a SharedPtr<Engine>::~SharedPtr<Engine>  when Application.h was included.
Application::~Application()
{
    //engine_ was registered in context_ as a subsystem, it will be destroyed when context_ is destroyed
}

int Application::Run()
{
    try
    {
        Setup();
        if (exitCode_)
            return exitCode_;

        if (!engine_->Initialize(engineParameters_))
        {
            ErrorExit();
            return exitCode_;
        }

        Start();
        if (exitCode_)
            return exitCode_;

        g_engineSignals.applicationStarted();

        // Platforms other than iOS and Emscripten run a blocking main loop
        while (!engine_->IsExiting())
            engine_->RunFrame();

        Stop();
        return exitCode_;
    }
    catch (std::bad_alloc& e)
    {
        ErrorDialog(m_appName, "An out-of-memory error occurred. The application will now exit.");
        return EXIT_FAILURE;
    }
}

void Application::ErrorExit(const QString& message)
{
    engine_->Exit(); // Close the rendering window
    exitCode_ = EXIT_FAILURE;

    if (message.isEmpty())
    {
        ErrorDialog(m_appName, startupErrors_.length() ? startupErrors_ :
                                                         "Application has been terminated due to unexpected error.");
    }
    else
        ErrorDialog(m_appName, message);
}

void Application::HandleLogMessage(LogLevels level, const QString & message)
{
    if (level == LOG_ERROR)
    {
        // Strip the timestamp if necessary
        QString error = message;
        int bracketPos = error.indexOf(']');
        if (bracketPos != -1)
            error = error.mid(bracketPos + 2);

        startupErrors_ += error + "\n";
    }
}
