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

#include "Lutefisk3D/Container/ArrayPtr.h"
#include "Lutefisk3D/Core/Context.h"
#include "Lutefisk3D/Core/CoreEvents.h"
#include "Lutefisk3D/Math/MathDefs.h"
#include "Lutefisk3D/Engine/EngineEvents.h"
#include "Lutefisk3D/IO/File.h"
#include "Lutefisk3D/IO/FileSystem.h"
#include "Lutefisk3D/IO/IOEvents.h"
#include "Lutefisk3D/IO/Log.h"
#include "Lutefisk3D/Core/Thread.h"
#include "Lutefisk3D/Container/HashMap.h"
#include "Lutefisk3D/Container/Str.h"
#include "Lutefisk3D/Math/StringHash.h"
#include "Lutefisk3D/Engine/jlsignal/SignalBase.h"

#include <QtCore/QProcess>
#include <QtCore/QDir>
#include <QtCore/QDirIterator>
#include <QtCore/QDateTime>
#include <QtCore/QCoreApplication>
#include <QtCore/QUrl>
#include <QtGui/QDesktopServices>
#include <cstdio>
#include <cstring>
#include <sys/stat.h>

namespace Urho3D
{

namespace {
int DoSystemCommand(const QString& commandLine, bool redirectToLog)
{
    if (!redirectToLog)
        return system(qPrintable(commandLine));
    QProcess pr;
    pr.start(commandLine,QProcess::ReadOnly);
    pr.waitForFinished(-1);
    int exitCode = pr.exitCode();
    Log::WriteRaw(pr.readAllStandardError(), true);

    return exitCode;
}

int DoSystemRun(const QString& fileName, const QStringList& arguments)
{
    return QProcess::execute(fileName,arguments);
}

/// Base class for async execution requests.
class AsyncExecRequest : public Thread
{
public:
    /// Construct.
    AsyncExecRequest(unsigned& requestID) :
        requestID_(requestID)
    {
        // Increment ID for next request
        ++requestID;
        if (requestID == M_MAX_UNSIGNED)
            requestID = 1;
    }

    /// Return request ID.
    unsigned GetRequestID() const { return requestID_; }
    /// Return exit code. Valid when IsCompleted() is true.
    int GetExitCode() const { return exitCode_; }
    /// Return completion status.
    bool IsCompleted() const { return completed_; }

protected:
    /// Request ID.
    unsigned requestID_;
    /// Exit code.
    int exitCode_;
    /// Completed flag.
    volatile bool completed_=false;
};

/// Async system command operation.
class AsyncSystemCommand final : public AsyncExecRequest
{
public:
    /// Construct and run.
    AsyncSystemCommand(unsigned requestID, const QString& commandLine) :
        AsyncExecRequest(requestID),
        commandLine_(commandLine)
    {
        Run();
    }

    /// The function to run in the thread.
    virtual void ThreadFunction() override
    {
        exitCode_ = DoSystemCommand(commandLine_, false);
        completed_ = true;
    }

private:
    /// Command line.
    QString commandLine_;
};

/// Async system run operation.
class AsyncSystemRun final : public AsyncExecRequest
{
public:
    /// Construct and run.
    AsyncSystemRun(unsigned requestID, const QString& fileName, const QStringList& arguments) :
        AsyncExecRequest(requestID),
        fileName_(fileName),
        arguments_(arguments)
    {
        Run();
    }

    /// The function to run in the thread.
    virtual void ThreadFunction() override
    {
        exitCode_ = DoSystemRun(fileName_, arguments_);
        completed_ = true;
    }

private:
    /// File to run.
    QString fileName_;
    /// Command line split in arguments.
    const QStringList& arguments_;
};

}
struct FileSystemPrivate : public jl::SignalObserver
{
    /// Allowed directories.
    HashSet<QString> allowedPaths_;
    /// Async execution queue.
    std::list<AsyncExecRequest*> asyncExecQueue_;
    /// Next async execution ID.
    unsigned nextAsyncExecID_=1;
    /// Flag for executing engine console commands as OS-specific system command. set to true in constructor.
    bool executeConsoleCommands_=false;
    FileSystemPrivate(Context *ctx) : SignalObserver(ctx->observerAllocator()) {}
    ~FileSystemPrivate() {
    // If any async exec items pending, delete them
        if (!asyncExecQueue_.empty())
    {
        for (auto & elem : asyncExecQueue_)
            delete(elem);

        asyncExecQueue_.clear();
    }
    }
    unsigned addAsyncSysCommand(const QString &commandLine)
    {
        unsigned requestID = nextAsyncExecID_;
        AsyncSystemCommand* cmd = new AsyncSystemCommand(nextAsyncExecID_, commandLine);
        asyncExecQueue_.push_back(cmd);
        return requestID;
    }
    unsigned addAsyncSysRun(const QString &fileName,const QStringList &arguments)
    {
        unsigned requestID = nextAsyncExecID_;
        AsyncSystemRun* cmd = new AsyncSystemRun(nextAsyncExecID_, fileName, arguments);
        asyncExecQueue_.push_back(cmd);
        return requestID;
    }
    //////////////////////////////////////////////////////
    // Signal handlers
    //////////////////////////////////////////////////////
    void HandleBeginFrame(unsigned /*frameNumber*/,float /*timeStep*/)
    {
        /// Go through the execution queue and post + remove completed requests
        for (std::list<AsyncExecRequest*>::iterator i = asyncExecQueue_.begin(); i != asyncExecQueue_.end();)
        {
            AsyncExecRequest* request = *i;
            if (request->IsCompleted())
            {
                g_ioSignals.asyncExecFinished(request->GetRequestID(),request->GetExitCode());
                delete request;
                i = asyncExecQueue_.erase(i);
            }
            else
                ++i;
        }
    }
    void HandleConsoleCommand(const QString &cmd, const QString &id)
    {
        if (id == "FileSystem")
            SystemCommand(cmd, true);
    }
    void SetExecuteConsoleCommands(bool enable)
    {
        if (enable == executeConsoleCommands_)
            return;

        executeConsoleCommands_ = enable;
        if (enable)
            g_consoleSignals.consoleCommand.Connect(this,&FileSystemPrivate::HandleConsoleCommand);
        else
            g_consoleSignals.consoleCommand.Disconnect(this,&FileSystemPrivate::HandleConsoleCommand);
    }
    int SystemCommand(const QString& commandLine, bool redirectStdOutToLog)
    {
        if (allowedPaths_.isEmpty())
            return DoSystemCommand(commandLine, redirectStdOutToLog);
        else
        {
            URHO3D_LOGERROR("Executing an external command is not allowed");
            return -1;
        }
    }
};



FileSystem::FileSystem(Context* context) : m_context(context), d(new FileSystemPrivate(context))
{
    g_coreSignals.beginFrame.Connect(d.get(),&FileSystemPrivate::HandleBeginFrame);

    // Subscribe to console commands
    d->SetExecuteConsoleCommands(true);
}

FileSystem::~FileSystem()
{
}

bool FileSystem::SetCurrentDir(const QString& pathName)
{
    if (!CheckAccess(pathName))
    {
        URHO3D_LOGERROR("Access denied to " + pathName);
        return false;
    }
    if (!QDir::setCurrent(pathName))
    {
        URHO3D_LOGERROR("Failed to change directory to " + pathName);
        return false;
    }

    return true;
}

bool FileSystem::CreateDir(const QString& pathName)
{
    if (!CheckAccess(pathName))
    {
        URHO3D_LOGERROR("Access denied to " + pathName);
        return false;
    }

    QDir dir(GetNativePath(RemoveTrailingSlash(pathName)));

    bool success = dir.mkpath("."); //NOTE: If directory exists already, this will return true

    if (success)
        URHO3D_LOGDEBUG("Created directory " + pathName);
    else
        URHO3D_LOGERROR("Failed to create directory " + pathName);

    return success;
}

void FileSystem::SetExecuteConsoleCommands(bool enable)
{
    d->SetExecuteConsoleCommands(enable);
}

int FileSystem::SystemCommand(const QString& commandLine, bool redirectStdOutToLog)
{
    return d->SystemCommand(commandLine,redirectStdOutToLog);
}

int FileSystem::SystemRun(const QString& fileName, const QStringList& arguments)
{
    if (d->allowedPaths_.isEmpty())
        return DoSystemRun(fileName, arguments);
    else
    {
        URHO3D_LOGERROR("Executing an external command is not allowed");
        return -1;
    }
}

unsigned FileSystem::SystemCommandAsync(const QString& commandLine)
{
    if (d->allowedPaths_.isEmpty())
    {
        return d->addAsyncSysCommand(commandLine);
    }
    else
    {
        URHO3D_LOGERROR("Executing an external command is not allowed");
        return M_MAX_UNSIGNED;
    }
}

unsigned FileSystem::SystemRunAsync(const QString& fileName, const QStringList& arguments)
{
    if (d->allowedPaths_.isEmpty())
    {
        return d->addAsyncSysRun(fileName,arguments);
    }
    else
    {
        URHO3D_LOGERROR("Executing an external command is not allowed");
        return M_MAX_UNSIGNED;
    }
}

bool FileSystem::SystemOpen(const QString& fileName, const QString& mode)
{
    if (d->allowedPaths_.isEmpty())
    {
        if (!FileExists(fileName) && !DirExists(fileName))
        {
            URHO3D_LOGERROR("File or directory " + fileName + " not found");
            return false;
        }

        bool success = QDesktopServices::openUrl(QUrl("file:///"+fileName, QUrl::TolerantMode));
        if (!success)
            URHO3D_LOGERROR("Failed to open " + fileName + " externally");
        return success;
    }
    else
    {
        URHO3D_LOGERROR("Opening a file externally is not allowed");
        return false;
    }
}

bool FileSystem::Copy(const QString& srcFileName, const QString& destFileName)
{
    if (!CheckAccess(GetPath(srcFileName)))
    {
        URHO3D_LOGERROR("Access denied to " + srcFileName);
        return false;
    }
    if (!CheckAccess(GetPath(destFileName)))
    {
        URHO3D_LOGERROR("Access denied to " + destFileName);
        return false;
    }

    std::unique_ptr<File> srcFile(new File(m_context, srcFileName, FILE_READ));
    if (!srcFile->IsOpen())
        return false;
    std::unique_ptr<File> destFile(new File(m_context, destFileName, FILE_WRITE));
    if (!destFile->IsOpen())
        return false;

    unsigned fileSize = srcFile->GetSize();
    std::unique_ptr<uint8_t> buffer(new unsigned char[fileSize]);

    unsigned bytesRead = srcFile->Read(buffer.get(), fileSize);
    unsigned bytesWritten = destFile->Write(buffer.get(), fileSize);
    return bytesRead == fileSize && bytesWritten == fileSize;
}

bool FileSystem::Rename(const QString& srcFileName, const QString& destFileName)
{
    if (!CheckAccess(GetPath(srcFileName)))
    {
        URHO3D_LOGERROR("Access denied to " + srcFileName);
        return false;
    }
    if (!CheckAccess(GetPath(destFileName)))
    {
        URHO3D_LOGERROR("Access denied to " + destFileName);
        return false;
    }

    return QFile::rename(srcFileName,destFileName);
}

bool FileSystem::Delete(const QString& fileName)
{
    if (!CheckAccess(GetPath(fileName)))
    {
        URHO3D_LOGERROR("Access denied to " + fileName);
        return false;
    }

    return QFile::remove(fileName);
}

QString FileSystem::GetCurrentDir() const
{
    return AddTrailingSlash(QDir::currentPath());
}

bool FileSystem::GetExecuteConsoleCommands() const
{
    return d->executeConsoleCommands_;
}

bool FileSystem::HasRegisteredPaths() const
{
    return !d->allowedPaths_.empty();
}

bool FileSystem::CheckAccess(const QString& pathName) const
{
    QString fixedPath = AddTrailingSlash(pathName);

    // If no allowed directories defined, succeed always
    if (d->allowedPaths_.isEmpty())
        return true;

    // If there is any attempt to go to a parent directory, disallow
    if (fixedPath.contains(".."))
        return false;

    // Check if the path is a partial match of any of the allowed directories
    for (const QString &i : d->allowedPaths_)
    {
        if (fixedPath.startsWith(i))
            return true;
    }

    // Not found, so disallow
    return false;
}

unsigned FileSystem::GetLastModifiedTime(const QString& fileName) const
{
    if (fileName.isEmpty() || !CheckAccess(fileName))
        return 0;

    QFileInfo fi(fileName);
    if(!fi.exists())
        return 0;
    return fi.lastModified().toTime_t();
}

bool FileSystem::FileExists(const QString& fileName) const
{
    if (!CheckAccess(GetPath(fileName)))
        return false;

    QString fixedName = GetNativePath(RemoveTrailingSlash(fileName));


    QFileInfo fi(fixedName);
    if(!fi.exists() || fi.isDir())
        return false;

    return true;
}

bool FileSystem::DirExists(const QString& pathName) const
{
    if (!CheckAccess(pathName))
        return false;
    QFileInfo fi(pathName);
    return fi.exists() && fi.isDir();
}

void FileSystem::ScanDir(QStringList& result, const QString& pathName, const QString& filter, unsigned flags, bool recursive) const
{
    result.clear();

    if (CheckAccess(pathName))
    {
        QDir::Filters filters;
        if(flags & SCAN_HIDDEN)
            filters |= QDir::Hidden;
        if(flags & SCAN_FILES)
            filters |= QDir::Files;
        if(flags & SCAN_DIRS)
            filters |= QDir::Dirs;

        QString initialPath = AddTrailingSlash(pathName);
        QDirIterator diriter(initialPath,QStringList {filter},filters,recursive ? QDirIterator::Subdirectories : QDirIterator::NoIteratorFlags);
        while(diriter.hasNext())
            result.push_back(diriter.next());
    }
}

QString FileSystem::GetProgramDir() const
{
    if(QCoreApplication::instance()) // an QApplication has been allocated, use it
        return QCoreApplication::applicationDirPath();

    return GetCurrentDir();
}

QString FileSystem::GetUserDocumentsDir() const
{
    return AddTrailingSlash(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation));
}

QString FileSystem::GetAppPreferencesDir(const QString& org, const QString& app) const
{
    return AddTrailingSlash(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation));
}

void FileSystem::RegisterPath(const QString& pathName)
{
    if (pathName.isEmpty())
        return;

    d->allowedPaths_.insert(AddTrailingSlash(pathName));
}

void FileSystem::HandleConsoleCommand(const QString &cmd, const QString &id)
{
    if (id == "FileSystem")
        SystemCommand(cmd, true);
}

void SplitPath(const QString& fullPath, QString& pathName, QString& fileName, QString& extension, bool lowercaseExtension)
{
    QString fullPathCopy = GetInternalPath(fullPath);

    int extPos = fullPathCopy.lastIndexOf('.');
    int pathPos = fullPathCopy.lastIndexOf('/');

    if (extPos != -1 && (pathPos == -1 || extPos > pathPos))
    {
        extension = fullPathCopy.mid(extPos);
        if (lowercaseExtension)
            extension = extension.toLower();
        fullPathCopy = fullPathCopy.mid(0, extPos);
    }
    else
        extension.clear();

    pathPos = fullPathCopy.lastIndexOf('/');
    if (pathPos != -1)
    {
        fileName = fullPathCopy.mid(pathPos + 1);
        pathName = fullPathCopy.mid(0, pathPos + 1);
    }
    else
    {
        fileName = fullPathCopy;
        pathName.clear();
    }
}

QString GetPath(const QString& fullPath)
{
    QString path, file, extension;
    SplitPath(fullPath, path, file, extension);
    return path;
}

QString GetFileName(const QString& fullPath)
{
    QString path, file, extension;
    SplitPath(fullPath, path, file, extension);
    return file;
}

QString GetExtension(const QString& fullPath, bool lowercaseExtension)
{
    QString path, file, extension;
    SplitPath(fullPath, path, file, extension, lowercaseExtension);
    return extension;
}

QString GetFileNameAndExtension(const QString& fileName, bool lowercaseExtension)
{
    QString path, file, extension;
    SplitPath(fileName, path, file, extension, lowercaseExtension);
    return file + extension;
}

QString ReplaceExtension(const QString& fullPath, const QString& newExtension)
{
    QString path, file, extension;
    SplitPath(fullPath, path, file, extension);
    return path + file + newExtension;
}

QString AddTrailingSlash(const QString& pathName)
{
    QString ret = pathName.trimmed();
    ret.replace('\\', '/');
    if (!ret.isEmpty() && !ret.endsWith('/') )
        ret += '/';
    return ret;
}

QString RemoveTrailingSlash(const QString& pathName)
{
    QString ret = pathName.trimmed();
    ret.replace('\\', '/');
    if (!ret.isEmpty() && ret.endsWith('/') )
        ret.resize(ret.length() - 1);
    return ret;
}

QString GetParentPath(const QString& path)
{
    int pos = RemoveTrailingSlash(path).lastIndexOf('/');
    if (pos != -1)
        return path.mid(0, pos + 1);
    else
        return QString();
}

QString GetInternalPath(const QString& pathName)
{
    return QString(pathName).replace('\\', '/');
}

QString GetNativePath(const QString& pathName)
{
    QString res(pathName);
    return res.replace('/', QDir::separator());
}

bool IsAbsolutePath(const QString& pathName)
{
    if (pathName.isEmpty())
        return false;

    return QDir::isAbsolutePath(pathName);
}

}
