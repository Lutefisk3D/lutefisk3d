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
#include "Lutefisk3D/Engine/jlsignal/SignalBase.h"
#include <list>

namespace Urho3D
{
class File;
enum LogLevels : int32_t
{
    LOG_RAW     = -1, //!< Fictional message level to indicate a stored raw message.
    LOG_DEBUG   = 0,  //!< Debug message level. By default only shown in debug mode.
    LOG_INFO    = 1,  //!< Informative message level.
    LOG_WARNING = 2,  //!< Warning message level.
    LOG_ERROR   = 3,  //!< Error message level.
    LOG_NONE    = 4,  //!< Disable all log messages.
};

/// Stored log message from another thread.
struct StoredLogMessage
{
    QString   message_; //!< Message text.
    LogLevels level_;   //!< Message level. LOG_RAW for raw messages.
    bool      error_;   //!< Error flag for raw messages.
};

/// Logging subsystem.
class LUTEFISK3D_EXPORT Log : public jl::SignalObserver
{
    using LogMessageContainer = std::list<StoredLogMessage>;
public:
    Log(Context *ctx);
    virtual ~Log();

    void SetTargetFilename(const QString &fileName);
    void CloseTargetFile();
    void SetLoggingLevel(int level);
    int  GetLoggingLevel() const { return level_; }
    void SetTimeStamp(bool enable);
    bool GetTimeStamp() const { return timeStamp_; }
    void SetQuiet(bool quiet);
    QString GetLastMessage() const { return lastMessage_; }
    bool    IsQuiet() const { return quiet_; }

    static void Write(LogLevels level, const QString& message);
    static void WriteRaw(const QString& message, bool error = false);
private:
    void HandleEndFrame();

    Context *             m_context;       //!< Current context this logger is bound to.
    Mutex                 logMutex_;       //!< Mutex for threaded operation.
    LogMessageContainer   threadMessages_; //!< Log messages from other threads.
    std::unique_ptr<File> logFile_;        //!< Log output file.
    QString               lastMessage_;    //!< Last log message.
    int                   level_;          //!< Logging level. Messages below that level will not be logged.
    bool                  timeStamp_;      //!< Timestamp log messages flag.
    bool                  inWrite_;        //!< In write flag to prevent recursion.
    bool                  quiet_;          //!< Quiet mode flag, if true errors are only printed to standard error stream
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
