//
// Copyright (c) 2008-2018 the Urho3D project.
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
#include <QtCore/QString>
#include <cstdlib>

namespace Urho3D
{

class Mutex;

/// Initialize the FPU to round-to-nearest, single precision mode.
LUTEFISK3D_EXPORT void InitFPU();
/// Display an error dialog with the specified title and message.
LUTEFISK3D_EXPORT void ErrorDialog(const QString& title, const QString& message);
/// Exit the application with an error message to the console.
LUTEFISK3D_EXPORT void ErrorExit(const QString& message = QString::null, int exitCode = EXIT_FAILURE);
/// Open a console window.
LUTEFISK3D_EXPORT void OpenConsoleWindow();
/// Print Unicode text to the console. Will not be printed to the MSVC output window.
LUTEFISK3D_EXPORT void PrintUnicode(const QString& str, bool error = false);
/// Print Unicode text to the console with a newline appended. Will not be printed to the MSVC output window.
LUTEFISK3D_EXPORT void PrintUnicodeLine(const QString& str, bool error = false);
/// Print ASCII text to the console with a newline appended. Uses printf() to allow printing into the MSVC output window.
LUTEFISK3D_EXPORT void PrintLine(const QString& str, bool error = false);
/// Parse arguments from the command line. First argument is by default assumed to be the executable name and is skipped.
LUTEFISK3D_EXPORT const QStringList& ParseArguments(const QString& cmdLine, bool skipFirstArgument = true);
/// Parse arguments from the command line.
LUTEFISK3D_EXPORT const QStringList& ParseArguments(const char* cmdLine);
/// Parse arguments from argc & argv.
LUTEFISK3D_EXPORT const QStringList& ParseArguments(int argc, char** argv);
/// Return previously parsed arguments.
LUTEFISK3D_EXPORT const QStringList& GetArguments();
/// Read input from the console window. Return empty if no input.
LUTEFISK3D_EXPORT QString GetConsoleInput();
/// Return the runtime platform identifier, one of "Windows", "Linux", "Mac OS X", "Android", "iOS", "Web" or "Raspberry Pi".
LUTEFISK3D_EXPORT QString GetPlatform();
/// Return the number of physical CPU cores.
LUTEFISK3D_EXPORT unsigned GetNumPhysicalCPUs();
/// Return the number of logical CPUs (different from physical if hyperthreading is used.)
LUTEFISK3D_EXPORT unsigned GetNumLogicalCPUs();
/// Set minidump write location as an absolute path. If empty, uses default (UserProfile/AppData/Roaming/urho3D/crashdumps)
/// Minidumps are only supported on MSVC compiler.
LUTEFISK3D_EXPORT void SetMiniDumpDir(const QString& pathName);
/// Return minidump write location.
LUTEFISK3D_EXPORT QString GetMiniDumpDir();
/// Return the total amount of useable memory.
LUTEFISK3D_EXPORT uint64_t GetTotalMemory();
/// Return a random UUID.
LUTEFISK3D_EXPORT QString GenerateUUID();


}
