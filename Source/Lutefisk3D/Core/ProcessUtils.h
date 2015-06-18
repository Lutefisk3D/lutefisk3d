//
// Copyright (c) 2008-2015 the Urho3D project.
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

#include "../Container/Str.h"

#include <cstdlib>

namespace Urho3D
{

class Mutex;

/// Initialize the FPU to round-to-nearest, single precision mode.
void InitFPU();
/// Display an error dialog with the specified title and message.
void ErrorDialog(const QString& title, const QString& message);
/// Exit the application with an error message to the console.
void ErrorExit(const QString& message = QString::null, int exitCode = EXIT_FAILURE);
/// Open a console window.
void OpenConsoleWindow();
/// Print Unicode text to the console. Will not be printed to the MSVC output window.
void PrintUnicode(const QString& str, bool error = false);
/// Print Unicode text to the console with a newline appended. Will not be printed to the MSVC output window.
void PrintUnicodeLine(const QString& str, bool error = false);
/// Print ASCII text to the console with a newline appended. Uses printf() to allow printing into the MSVC output window.
void PrintLine(const QString& str, bool error = false);
/// Parse arguments from the command line. First argument is by default assumed to be the executable name and is skipped.
const QStringList& ParseArguments(const QString& cmdLine, bool skipFirstArgument = true);
/// Parse arguments from the command line.
const QStringList& ParseArguments(const char* cmdLine);
/// Parse arguments from argc & argv.
const QStringList& ParseArguments(int argc, char** argv);
/// Return previously parsed arguments.
const QStringList& GetArguments();
/// Read input from the console window. Return empty if no input.
QString GetConsoleInput();
/// Return the runtime platform identifier, one of "Windows", "Linux", "Mac OS X", "Android", "iOS" or "Raspberry Pi".
QString GetPlatform();
/// Return the number of physical CPU cores.
unsigned GetNumPhysicalCPUs();
/// Return the number of logical CPUs (different from physical if hyperthreading is used.)
unsigned GetNumLogicalCPUs();

}
