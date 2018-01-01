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

#include "Log.h"
#include "File.h"
#include "IOEvents.h"
#include "Lutefisk3D/Core/Context.h"
#include "Lutefisk3D/Core/CoreEvents.h"
#include "Lutefisk3D/Core/Mutex.h"
#include "Lutefisk3D/Core/ProcessUtils.h"
#include "Lutefisk3D/Core/Thread.h"
#include "Lutefisk3D/Core/Timer.h"
#include "Lutefisk3D/Core/StringUtils.h"

#include <cstdio>

namespace Urho3D
{

static const char* logLevelPrefixes[] =
{
    "DEBUG",
    "INFO",
    "WARNING",
    "ERROR",
    nullptr
};

static Log* logInstance = nullptr;
static bool threadErrorDisplayed = false;

Log::Log(Context *ctx) :
    SignalObserver(ctx->m_observer_allocator),
    m_context(ctx),
#ifdef _DEBUG
    level_(LOG_DEBUG),
#else
    level_(LOG_INFO),
#endif
    timeStamp_(true),
    inWrite_(false),
    quiet_(false)
{
    logInstance = this;
    g_coreSignals.endFrame.Connect(this,&Log::HandleEndFrame);
}
/// Destruct. Close the log file if open.
Log::~Log()
{
    logInstance = nullptr;
}

/// Will open a new file and start logging to it.
/// If \a fileName is empty or same as current logFile_ does nothing.
void Log::SetTargetFilename(const QString& fileName)
{
    if (fileName.isEmpty())
        return;
    if (logFile_ && logFile_->IsOpen())
    {
        if (logFile_->GetName() == fileName)
            return;
        else
            CloseTargetFile();
    }

    logFile_.reset(new File(m_context));
    if (logFile_->Open(fileName, FILE_WRITE))
        Write(LOG_INFO, "Opened log file " + fileName);
    else
    {
        logFile_ = nullptr;
        Write(LOG_ERROR, "Failed to create log file " + fileName);
    }
}
/// If logging file is open, close it and release underlying File
void Log::CloseTargetFile()
{
    if (logFile_ && logFile_->IsOpen())
    {
        logFile_->Close();
        logFile_ = nullptr;
    }
}
/// Set logging level.
void Log::SetLoggingLevel(int level)
{
    assert(level >= LOG_DEBUG && level <= LOG_NONE);

    level_ = level;
}
/// Set whether to timestamp log messages.
void Log::SetTimeStamp(bool enable)
{
    timeStamp_ = enable;
}

/// Set quiet mode ie. only print error entries to standard error stream (which is normally redirected to console also).
/// Output to log file is not affected by this mode.
void Log::SetQuiet(bool quiet)
{
    quiet_ = quiet;
}
/// Write to the log. If logging level is higher than the level of the message, the message is ignored.
void Log::Write(LogLevels level, const QString& message)
{
    // Special case for LOG_RAW level
    if (level == LOG_RAW)
    {
        WriteRaw(message, false);
        return;
    }
    assert(level >= LOG_DEBUG && level <= LOG_NONE);

    // If not in the main thread, store message for later processing
    if (!Thread::IsMainThread())
    {
        if (logInstance)
        {
            MutexLock lock(logInstance->logMutex_);
            logInstance->threadMessages_.emplace_back(StoredLogMessage {message, level, false});
        }

        return;
    }

    // Do not log if message level excluded or if currently sending a log event
    if (!logInstance || logInstance->level_ > level || logInstance->inWrite_)
        return;

    QString formattedMessage = logLevelPrefixes[level];
    formattedMessage += ": " + message;
    logInstance->lastMessage_ = message;

    if (logInstance->timeStamp_)
        formattedMessage = "[" + Time::GetTimeStamp() + "] " + formattedMessage;

    if (logInstance->quiet_)
    {
        // If in quiet mode, still print the error message to the standard error stream
        if (level == LOG_ERROR)
            PrintUnicodeLine(formattedMessage, true);
    }
    else
        PrintUnicodeLine(formattedMessage, level == LOG_ERROR);

    if (logInstance->logFile_)
    {
        logInstance->logFile_->WriteLine(formattedMessage);
        logInstance->logFile_->Flush();
    }

    logInstance->inWrite_ = true;
    g_LogSignals.logMessageSignal(level,formattedMessage);
    logInstance->inWrite_ = false;
}
/// Write raw output to the log.
void Log::WriteRaw(const QString& message, bool error)
{
    // If not in the main thread, store message for later processing
    if (!Thread::IsMainThread())
    {
        if (logInstance)
        {
            MutexLock lock(logInstance->logMutex_);
            logInstance->threadMessages_.emplace_back(StoredLogMessage {message, LOG_RAW, error});
        }

        return;
    }

    // Prevent recursion during log event
    if (!logInstance || logInstance->inWrite_)
        return;

    logInstance->lastMessage_ = message;

    if (logInstance->quiet_)
    {
        // If in quiet mode, still print the error message to the standard error stream
        if (error)
            PrintUnicode(message, true);
    }
    else
        PrintUnicode(message, error);

    if (logInstance->logFile_)
    {
        logInstance->logFile_->Write(qPrintable(message), message.length());
        logInstance->logFile_->Flush();
    }

    logInstance->inWrite_ = true;
    g_LogSignals.logMessageSignal(error ? LOG_ERROR : LOG_INFO,message);
    logInstance->inWrite_ = false;
}
/// Handle end of frame will process the threaded log messages.
void Log::HandleEndFrame()
{
    // If the MainThreadID is not valid, processing this loop can potentially be endless
    if (!Thread::IsMainThread())
    {
        if (!threadErrorDisplayed)
        {
            fprintf(stderr, "Thread::mainThreadID is not setup correctly! Threaded log handling disabled\n");
            threadErrorDisplayed = true;
        }
        return;
    }

    MutexLock lock(logMutex_);

    // Process messages accumulated from other threads (if any)
    while (!threadMessages_.empty())
    {
        const StoredLogMessage& stored = threadMessages_.front();

        if (stored.level_ != LOG_RAW)
            Write(stored.level_, stored.message_);
        else
            WriteRaw(stored.message_, stored.error_);

        threadMessages_.pop_front();
    }
}

unsigned logLevelNameToIndex(const QString &name)
{
    return GetStringListIndex(name.toUpper(), logLevelPrefixes, M_MAX_UNSIGNED);
}

}
