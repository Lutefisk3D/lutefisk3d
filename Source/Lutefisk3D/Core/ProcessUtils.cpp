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

#include "../Core/Mutex.h"
#include "../Core/ProcessUtils.h"
#include "../Math/MathDefs.h"
#include "../IO/FileSystem.h" // used for minidump support functions

#ifndef MINI_URHO
#include <SDL2/SDL.h>
#endif

#include <cstdio>
#include <fcntl.h>

#ifdef __APPLE__
#include "TargetConditionals.h"
#endif

#if defined(IOS)
#include "../Math/MathDefs.h"
#include <mach/mach_host.h>
#elif !defined(ANDROID) && !defined(RPI) && !defined(__EMSCRIPTEN__)
#include <LibCpuId/libcpuid.h>
#endif

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#else
#include <unistd.h>
#endif

#if defined(__EMSCRIPTEN__) && defined(__EMSCRIPTEN_PTHREADS__)
#include <emscripten/threading.h>
#endif

#if defined(_MSC_VER)
#include <float.h>
#elif !defined(ANDROID) && !defined(IOS) && !defined(RPI) && !defined(__EMSCRIPTEN__)
// From http://stereopsis.com/FPU.html

#define FPU_CW_PREC_MASK        0x0300
#define FPU_CW_PREC_SINGLE      0x0000
#define FPU_CW_PREC_DOUBLE      0x0200
#define FPU_CW_PREC_EXTENDED    0x0300
#define FPU_CW_ROUND_MASK       0x0c00
#define FPU_CW_ROUND_NEAR       0x0000
#define FPU_CW_ROUND_DOWN       0x0400
#define FPU_CW_ROUND_UP         0x0800
#define FPU_CW_ROUND_CHOP       0x0c00

inline unsigned GetFPUState()
{
    unsigned control = 0;
    __asm__ __volatile__ ("fnstcw %0" : "=m" (control));
    return control;
}

inline void SetFPUState(unsigned control)
{
    __asm__ __volatile__ ("fldcw %0" : : "m" (control));
}
#endif

namespace Urho3D
{

#ifdef _WIN32
static bool consoleOpened = false;
#endif
static QString currentLine;
static QStringList arguments;
static QString miniDumpDir;

#if defined(IOS)
static void GetCPUData(host_basic_info_data_t* data)
{
    mach_msg_type_number_t infoCount;
    infoCount = HOST_BASIC_INFO_COUNT;
    host_info(mach_host_self(), HOST_BASIC_INFO, (host_info_t)data, &infoCount);
}
#elif !defined(ANDROID) && !defined(RPI) && !defined(__EMSCRIPTEN__)
static void GetCPUData(struct cpu_id_t* data)
{
    if (cpu_identify(nullptr, data) < 0)
    {
        data->num_logical_cpus = 1;
        data->num_cores = 1;
    }
}
#endif

void InitFPU()
{
#if !defined(URHO3D_LUAJIT) && !defined(ANDROID) && !defined(IOS) && !defined(RPI) && !defined(__x86_64__) && !defined(_M_AMD64) && !defined(__EMSCRIPTEN__)
    // Make sure FPU is in round-to-nearest, single precision mode
    // This ensures Direct3D and OpenGL behave similarly, and all threads behave similarly
#ifdef _MSC_VER
    _controlfp(_RC_NEAR | _PC_24, _MCW_RC | _MCW_PC);
#else
    unsigned control = GetFPUState();
    control &= ~(FPU_CW_PREC_MASK | FPU_CW_ROUND_MASK);
    control |= (FPU_CW_PREC_SINGLE | FPU_CW_ROUND_NEAR);
    SetFPUState(control);
#endif
#endif
}

void ErrorDialog(const QString& title, const QString& message)
{
#ifndef MINI_URHO
    // TODO: use qt widgets here ?
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, qPrintable(title), qPrintable(message), 0);
#endif
}

void ErrorExit(const QString& message, int exitCode)
{
    if (!message.isEmpty())
        PrintLine(message, true);

    exit(exitCode);
}

void OpenConsoleWindow()
{
#ifdef _WIN32
    if (consoleOpened)
        return;

    AllocConsole();

    freopen("CONIN$", "r", stdin);
    freopen("CONOUT$", "w", stdout);

    consoleOpened = true;
#endif
}

void PrintUnicode(const QString& str, bool error)
{
#if !defined(ANDROID) && !defined(IOS)
#ifdef _WIN32
    // If the output stream has been redirected, use fprintf instead of WriteConsoleW,
    // though it means that proper Unicode output will not work
    FILE* out = error ? stderr : stdout;
    if (!_isatty(_fileno(out)))
        fprintf(out, "%s", qPrintable(str));
    else
    {
        HANDLE stream = GetStdHandle(error ? STD_ERROR_HANDLE : STD_OUTPUT_HANDLE);
        if (stream == INVALID_HANDLE_VALUE)
            return;
        std::wstring strW(str.toStdWString());
        DWORD charsWritten;
        WriteConsoleW(stream, strW.c_str(), strW.size(), &charsWritten, 0);
    }
#else
    fprintf(error ? stderr : stdout, "%s", qPrintable(str));
#endif
#endif
}

void PrintUnicodeLine(const QString& str, bool error)
{
    PrintUnicode(str + '\n', error);
}

void PrintLine(const QString& str, bool error)
{
#if !defined(ANDROID) && !defined(IOS)
    fprintf(error ? stderr: stdout, "%s\n", qPrintable(str));
#endif
}

const QStringList& ParseArguments(const QString& cmdLine, bool skipFirstArgument)
{
    arguments.clear();

    unsigned cmdStart = 0, cmdEnd = 0;
    bool inCmd = false;
    bool inQuote = false;

    for (unsigned i = 0; i < cmdLine.length(); ++i)
    {
        if (cmdLine[i] == '\"')
            inQuote = !inQuote;
        if (cmdLine[i] == ' ' && !inQuote)
        {
            if (inCmd)
            {
                inCmd = false;
                cmdEnd = i;
                // Do not store the first argument (executable name)
                if (!skipFirstArgument)
                    arguments.push_back(cmdLine.mid(cmdStart, cmdEnd - cmdStart));
                skipFirstArgument = false;
            }
        }
        else
        {
            if (!inCmd)
            {
                inCmd = true;
                cmdStart = i;
            }
        }
    }
    if (inCmd)
    {
        cmdEnd = cmdLine.length();
        if (!skipFirstArgument)
            arguments.push_back(cmdLine.mid(cmdStart, cmdEnd - cmdStart));
    }

    // Strip double quotes from the arguments
    for (unsigned i = 0; i < arguments.size(); ++i)
        arguments[i].replace("\"", "");

    return arguments;
}

const QStringList& ParseArguments(const char* cmdLine)
{
    return ParseArguments(QString(cmdLine));
}

const QStringList& ParseArguments(int argc, char** argv)
{
    QString cmdLine;

    for (int i = 0; i < argc; ++i)
        cmdLine += QString("\"%1\" ").arg(argv[i]);

    return ParseArguments(cmdLine);
}

const QStringList& GetArguments()
{
    return arguments;
}

QString GetConsoleInput()
{
    QString ret;
#ifdef LUTEFISK3D_TESTING
    // When we are running automated tests, reading the console may block. Just return empty in that case
    return ret;
#endif

#ifdef _WIN32
    HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
    HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
    if (input == INVALID_HANDLE_VALUE || output == INVALID_HANDLE_VALUE)
        return ret;

    // Use char-based input
    SetConsoleMode(input, ENABLE_PROCESSED_INPUT);

    INPUT_RECORD record;
    DWORD events = 0;
    DWORD readEvents = 0;

    if (!GetNumberOfConsoleInputEvents(input, &events))
        return ret;

    while (events--)
    {
        ReadConsoleInputW(input, &record, 1, &readEvents);
        if (record.EventType == KEY_EVENT && record.Event.KeyEvent.bKeyDown)
        {
            unsigned c = record.Event.KeyEvent.uChar.UnicodeChar;
            if (c)
            {
                if (c == '\b')
                {
                    PrintUnicode("\b \b");
                    int length = currentLine.length();
                    if (length)
                        currentLine = currentLine.mid(0, length - 1);
                }
                else if (c == '\r')
                {
                    PrintUnicode("\n");
                    ret = currentLine;
                    currentLine.clear();
                    return ret;
                }
                else
                {
                    // We have disabled echo, so echo manually
                    wchar_t out = c;
                    DWORD charsWritten;
                    WriteConsoleW(output, &out, 1, &charsWritten, 0);
                    currentLine+=QChar(c);
                }
            }
        }
    }
#elif !defined(ANDROID) && !defined(IOS)
    int flags = fcntl(STDIN_FILENO, F_GETFL);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
    for (;;)
    {
        int ch = fgetc(stdin);
        if (ch >= 0 && ch != '\n')
            ret += (char)ch;
        else
            break;
    }
#endif

    return ret;
}

QString GetPlatform()
{
#if defined(ANDROID)
    return "Android";
#elif defined(IOS)
    return "iOS";
#elif defined(_WIN32)
    return "Windows";
#elif defined(__APPLE__)
    return "Mac OS X";
#elif defined(RPI)
    return "Raspberry Pi";
#elif defined(__EMSCRIPTEN__)
    return "Web";
#elif defined(__linux__)
    return "Linux";
#else
    return String::null;
#endif
}

#if defined(ANDROID) || defined(RPI)
static unsigned GetArmCPUCount()
{
    FILE* fp;
    int res, i = -1, j = -1;

    fp = fopen("/sys/devices/system/cpu/present", "r");
    // If failed, return at least 1
    if (fp == 0)
        return 1;

    res = fscanf(fp, "%d-%d", &i, &j);
    fclose(fp);

    if (res == 1 && i == 0)
        return 1;
    else if (res == 2 && i == 0)
        return j + 1;

    // If failed, return at least 1
    return 1;
}
#endif

unsigned GetNumPhysicalCPUs()
{
#if defined(IOS)
    host_basic_info_data_t data;
    GetCPUData(&data);
#if defined(TARGET_IPHONE_SIMULATOR)
    // Hardcoded to dual-core on simulator mode even if the host has more
    return Min(2, data.physical_cpu);
#else
    return data.physical_cpu;
#endif
#elif defined(ANDROID) || defined(RPI)
    return GetArmCPUCount();
#elif defined(__EMSCRIPTEN__)
#ifdef __EMSCRIPTEN_PTHREADS__
    return emscripten_num_logical_cores();
#else
    return 1; // Targeting a single-threaded Emscripten build.
#endif
#else
    struct cpu_id_t data;
    GetCPUData(&data);
    return (unsigned)data.num_cores;
#endif
}

unsigned GetNumLogicalCPUs()
{
#if defined(IOS)
    host_basic_info_data_t data;
    GetCPUData(&data);
#if defined(TARGET_IPHONE_SIMULATOR)
    return Min(2, data.logical_cpu);
#else
    return data.logical_cpu;
#endif
#elif defined(ANDROID) || defined (RPI)
    return GetArmCPUCount();
#elif defined(__EMSCRIPTEN__)
#ifdef __EMSCRIPTEN_PTHREADS__
    return emscripten_num_logical_cores();
#else
    return 1; // Targeting a single-threaded Emscripten build.
#endif
#else
    struct cpu_id_t data;
    GetCPUData(&data);
    return (unsigned)data.num_logical_cpus;
#endif
}
void SetMiniDumpDir(const QString& pathName)
{
    miniDumpDir = AddTrailingSlash(pathName);
}

QString GetMiniDumpDir()
{
#ifndef MINI_URHO
    if (miniDumpDir.isEmpty())
    {
        char* pathName = SDL_GetPrefPath("urho3d", "crashdumps");
        if (pathName)
        {
            QString ret(pathName);
            SDL_free(pathName);
            return ret;
        }
    }
#endif

    return miniDumpDir;
}

}
