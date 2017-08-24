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

#include "Lutefisk3D/Core/Mutex.h"
#include "Lutefisk3D/Core/Object.h"
#include "jlsignal/SignalBase.h"
#include <list>

namespace Urho3D
{
enum LogLevels : int32_t {
    /// Fictional message level to indicate a stored raw message.
    LOG_RAW = -1,
    /// Debug message level. By default only shown in debug mode.
    LOG_DEBUG = 0,
    /// Informative message level.
    LOG_INFO = 1,
    /// Warning message level.
    LOG_WARNING = 2,
    /// Error message level.
    LOG_ERROR = 3,
    /// Disable all log messages.
    LOG_NONE = 4,
};
class File;

/// Stored log message from another thread.
struct StoredLogMessage
{
    /// Construct undefined.
    StoredLogMessage()
    {
    }

    /// Construct with parameters.
    StoredLogMessage(const QString& message, LogLevels level, bool error) :
        message_(message),
        level_(level),
        error_(error)
    {
    }

    /// Message text.
    QString message_;
    /// Message level. -1 for raw messages.
    LogLevels level_;
    /// Error flag for raw messages.
    bool error_;
};

/// Logging subsystem.
class URHO3D_API Log : public jl::SignalObserver
{
public:
    Log(Context *ctx);
    /// Destruct. Close the log file if open.
    virtual ~Log();

    /// Open the log file.
    void Open(const QString& fileName);
    /// Close the log file.
    void Close();
    /// Set logging level.
    void SetLevel(int level);
    /// Set whether to timestamp log messages.
    void SetTimeStamp(bool enable);
    /// Set quiet mode ie. only print error entries to standard error stream (which is normally redirected to console also). Output to log file is not affected by this mode.
    void SetQuiet(bool quiet);

    /// Return logging level.
    int GetLevel() const { return level_; }
    /// Return whether log messages are timestamped.
    bool GetTimeStamp() const { return timeStamp_; }
    /// Return last log message.
    QString GetLastMessage() const { return lastMessage_; }
    /// Return whether log is in quiet mode (only errors printed to standard error stream).
    bool IsQuiet() const { return quiet_; }

    /// Write to the log. If logging level is higher than the level of the message, the message is ignored.
    static void Write(LogLevels level, const QString& message);
    /// Write raw output to the log.
    static void WriteRaw(const QString& message, bool error = false);
private:
    /// Handle end of frame. Process the threaded log messages.
    void HandleEndFrame();

    /// Current context this logger is bound to.
    Context *m_context;
    /// Mutex for threaded operation.
    Mutex logMutex_;
    /// Log messages from other threads.
    std::list<StoredLogMessage> threadMessages_;
    /// Log file.
    SharedPtr<File> logFile_;
    /// Last log message.
    QString lastMessage_;
    /// Logging level.
    int level_;
    /// Timestamp log messages flag.
    bool timeStamp_;
    /// In write flag to prevent recursion.
    bool inWrite_;
    /// Quiet mode flag.
    bool quiet_;
};
extern unsigned logLevelNameToIndex(const QString &name);
#ifdef LUTEFISK3D_LOGGING
#define URHO3D_LOGDEBUG(message) Urho3D::Log::Write(Urho3D::LOG_DEBUG, message)
#define URHO3D_LOGINFO(message) Urho3D::Log::Write(Urho3D::LOG_INFO, message)
#define URHO3D_LOGWARNING(message) Urho3D::Log::Write(Urho3D::LOG_WARNING, message)
#define URHO3D_LOGERROR(message) Urho3D::Log::Write(Urho3D::LOG_ERROR, message)
#define URHO3D_LOGRAW(message) Urho3D::Log::WriteRaw(message)
#else
#define URHO3D_LOGDEBUG(message) ((void)0)
#define URHO3D_LOGINFO(message) ((void)0)
#define URHO3D_LOGWARNING(message) ((void)0)
#define URHO3D_LOGERROR(message) ((void)0)
#define URHO3D_LOGRAW(message) ((void)0)
#define URHO3D_LOGDEBUGF(...) ((void)0)
#define URHO3D_LOGINFOF(...) ((void)0)
#define URHO3D_LOGWARNINGF(...) ((void)0)
#define URHO3D_LOGERRORF(...) ((void)0)
#define URHO3D_LOGRAWF(...) ((void)0)
#endif
}
