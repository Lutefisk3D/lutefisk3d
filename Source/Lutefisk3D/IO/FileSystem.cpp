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

#include "../Container/ArrayPtr.h"
#include "../Core/Context.h"
#include "../Core/CoreEvents.h"
#include "../Engine/EngineEvents.h"
#include "../IO/File.h"
#include "../IO/FileSystem.h"
#include "../IO/IOEvents.h"
#include "../IO/Log.h"
#include "../Core/Thread.h"

//#include <SDL/SDL_filesystem.h>
#include <QtCore/QProcess>
#include <QtCore/QDir>
#include <QtCore/QDateTime>
#include <QtCore/QCoreApplication>
#include <QtCore/QUrl>
#include <QtGui/QDesktopServices>
#include <cstdio>
#include <cstring>
#include <sys/stat.h>

#ifdef WIN32
#ifndef _MSC_VER
#define _WIN32_IE 0x501
#endif
#include <windows.h>
#include <shellapi.h>
#include <direct.h>
#include <shlobj.h>
#include <sys/types.h>
#include <sys/utime.h>
#else
#include <dirent.h>
#include <errno.h>
#include <unistd.h>
#include <utime.h>
#include <sys/wait.h>
#define MAX_PATH 256
#endif

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

namespace Urho3D
{

int DoSystemCommand(const QString& commandLine, bool redirectToLog, Context* context)
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
        requestID_(requestID),
        completed_(false)
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
    volatile bool completed_;
};

/// Async system command operation.
class AsyncSystemCommand : public AsyncExecRequest
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
        exitCode_ = DoSystemCommand(commandLine_, false, nullptr);
        completed_ = true;
    }

private:
    /// Command line.
    QString commandLine_;
};

/// Async system run operation.
class AsyncSystemRun : public AsyncExecRequest
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

FileSystem::FileSystem(Context* context) :
    Object(context),
    nextAsyncExecID_(1),
    executeConsoleCommands_(false)
{
    SubscribeToEvent(E_BEGINFRAME, URHO3D_HANDLER(FileSystem, HandleBeginFrame));

    // Subscribe to console commands
    SetExecuteConsoleCommands(true);
}

FileSystem::~FileSystem()
{
    // If any async exec items pending, delete them
    if (asyncExecQueue_.size())
    {
        for (auto & elem : asyncExecQueue_)
            delete(elem);

        asyncExecQueue_.clear();
    }
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
    if (enable == executeConsoleCommands_)
        return;

    executeConsoleCommands_ = enable;
    if (enable)
        SubscribeToEvent(E_CONSOLECOMMAND, URHO3D_HANDLER(FileSystem, HandleConsoleCommand));
    else
        UnsubscribeFromEvent(E_CONSOLECOMMAND);
}

int FileSystem::SystemCommand(const QString& commandLine, bool redirectStdOutToLog)
{
    if (allowedPaths_.isEmpty())
        return DoSystemCommand(commandLine, redirectStdOutToLog, context_);
    else
    {
        URHO3D_LOGERROR("Executing an external command is not allowed");
        return -1;
    }
}

int FileSystem::SystemRun(const QString& fileName, const QStringList& arguments)
{
    if (allowedPaths_.isEmpty())
        return DoSystemRun(fileName, arguments);
    else
    {
        URHO3D_LOGERROR("Executing an external command is not allowed");
        return -1;
    }
}

unsigned FileSystem::SystemCommandAsync(const QString& commandLine)
{
    if (allowedPaths_.isEmpty())
    {
        unsigned requestID = nextAsyncExecID_;
        AsyncSystemCommand* cmd = new AsyncSystemCommand(nextAsyncExecID_, commandLine);
        asyncExecQueue_.push_back(cmd);
        return requestID;
    }
    else
    {
        URHO3D_LOGERROR("Executing an external command is not allowed");
        return M_MAX_UNSIGNED;
    }
}

unsigned FileSystem::SystemRunAsync(const QString& fileName, const QStringList& arguments)
{
    if (allowedPaths_.isEmpty())
    {
        unsigned requestID = nextAsyncExecID_;
        AsyncSystemRun* cmd = new AsyncSystemRun(nextAsyncExecID_, fileName, arguments);
        asyncExecQueue_.push_back(cmd);
        return requestID;
    }
    else
    {
        URHO3D_LOGERROR("Executing an external command is not allowed");
        return M_MAX_UNSIGNED;
    }
}

bool FileSystem::SystemOpen(const QString& fileName, const QString& mode)
{
    if (allowedPaths_.isEmpty())
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

    SharedPtr<File> srcFile(new File(context_, srcFileName, FILE_READ));
    if (!srcFile->IsOpen())
        return false;
    SharedPtr<File> destFile(new File(context_, destFileName, FILE_WRITE));
    if (!destFile->IsOpen())
        return false;

    unsigned fileSize = srcFile->GetSize();
    SharedArrayPtr<unsigned char> buffer(new unsigned char[fileSize]);

    unsigned bytesRead = srcFile->Read(buffer.Get(), fileSize);
    unsigned bytesWritten = destFile->Write(buffer.Get(), fileSize);
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

bool FileSystem::CheckAccess(const QString& pathName) const
{
    QString fixedPath = AddTrailingSlash(pathName);

    // If no allowed directories defined, succeed always
    if (allowedPaths_.isEmpty())
        return true;

    // If there is any attempt to go to a parent directory, disallow
    if (fixedPath.contains(".."))
        return false;

    // Check if the path is a partial match of any of the allowed directories
    for (const QString &i : allowedPaths_)
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

    #ifdef ANDROID
    if (fixedName.StartsWith("/apk/"))
    {
        SDL_RWops* rwOps = SDL_RWFromFile(fileName.mid(5).CString(), "rb");
        if (rwOps)
        {
            SDL_RWclose(rwOps);
            return true;
        }
        else
            return false;
    }
    #endif

    QFileInfo fi(fixedName);
    if(!fi.exists() || fi.isDir())
        return false;

    return true;
}

bool FileSystem::DirExists(const QString& pathName) const
{
    if (!CheckAccess(pathName))
        return false;


    #ifdef ANDROID
    /// \todo Actually check for existence, now true is always returned for directories within the APK
    if (fixedName.StartsWith("/apk/"))
        return true;
    #endif

    QFileInfo fi(pathName);
    return fi.exists() && fi.isDir();
}

void FileSystem::ScanDir(QStringList& result, const QString& pathName, const QString& filter, unsigned flags, bool recursive) const
{
    result.clear();

    if (CheckAccess(pathName))
    {
        QString initialPath = AddTrailingSlash(pathName);
        ScanDirInternal(result, initialPath, initialPath, filter, flags, recursive);
    }
}

QString FileSystem::GetProgramDir() const
{
    // Return cached value if possible
    if (!programDir_.isEmpty())
        return programDir_;

    #if defined(ANDROID)
    // This is an internal directory specifier pointing to the assets in the .apk
    // Files from this directory will be opened using special handling
    programDir_ = APK;
    return programDir_;
    #elif defined(IOS)
    programDir_ = AddTrailingSlash(SDL_IOS_GetResourceDir());
    return programDir_;
    #elif defined(__APPLE__)
    char exeName[MAX_PATH];
    memset(exeName, 0, MAX_PATH);
    unsigned size = MAX_PATH;
    _NSGetExecutablePath(exeName, &size);
    programDir_ = GetPath(String(exeName));
    #elif defined(__linux__)
    char exeName[MAX_PATH];
    memset(exeName, 0, MAX_PATH);
    pid_t pid = getpid();
    QString link = "/proc/" + QString::number(pid) + "/exe";
    readlink(qPrintable(link), exeName, MAX_PATH);
    programDir_ = GetPath(QString(exeName));
    #endif
    if(QCoreApplication::instance()) // an QApplication has been allocated, use it
        programDir_ = QCoreApplication::applicationDirPath();

    // If the executable directory does not contain CoreData & Data directories, but the current working directory does, use the
    // current working directory instead
    /// \todo Should not rely on such fixed convention
    QString currentDir = GetCurrentDir();
    if (!DirExists(programDir_ + "CoreData") && !DirExists(programDir_ + "Data") && (DirExists(currentDir + "CoreData") ||
        DirExists(currentDir + "Data")))
        programDir_ = currentDir;

    // Sanitate /./ construct away
    programDir_.replace("/./", "/");

    return programDir_;
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

    allowedPaths_.insert(AddTrailingSlash(pathName));
}

bool FileSystem::SetLastModifiedTime(const QString& fileName, unsigned newTime)
{
    if (fileName.isEmpty() || !CheckAccess(fileName))
        return false;

    #ifdef WIN32
    struct _stat oldTime;
    struct _utimbuf newTimes;
    if (_stat(qPrintable(fileName), &oldTime) != 0)
        return false;
    newTimes.actime = oldTime.st_atime;
    newTimes.modtime = newTime;
    return _utime(qPrintable(fileName), &newTimes) == 0;
    #else
    struct stat oldTime;
    struct utimbuf newTimes;
    if (stat(qPrintable(fileName), &oldTime) != 0)
        return false;
    newTimes.actime = oldTime.st_atime;
    newTimes.modtime = newTime;
    return utime(qPrintable(fileName), &newTimes) == 0;
    #endif
}

void FileSystem::ScanDirInternal(QStringList& result, QString path, const QString& startPath,
    const QString& filter, unsigned flags, bool recursive) const
{
    path = AddTrailingSlash(path);
    QString deltaPath;
    if (path.length() > startPath.length())
        deltaPath = path.mid(startPath.length());

    QString filterExtension = filter.mid(filter.indexOf('.'));
    if (filterExtension.contains('*'))
        filterExtension.clear();

#ifdef ANDROID
    if (IS_ASSET(path))
    {
        QString assetPath(ASSET(path));
        assetPath.Resize(assetPath.Length() - 1);       // AssetManager.list() does not like trailing slash
        int count;
        char** list = SDL_Android_GetFileList(assetPath.CString(), &count);
        for (int i = 0; i < count; ++i)
        {
            QString fileName(list[i]);
            if (!(flags & SCAN_HIDDEN) && fileName.StartsWith("."))
                continue;

#ifdef ASSET_DIR_INDICATOR
            // Patch the directory name back after retrieving the directory flag
            bool isDirectory = fileName.EndsWith(ASSET_DIR_INDICATOR);
            if (isDirectory)
            {
                fileName.Resize(fileName.Length() - sizeof(ASSET_DIR_INDICATOR) / sizeof(char) + 1);
                if (flags & SCAN_DIRS)
                    result.Push(deltaPath + fileName);
                if (recursive)
                    ScanDirInternal(result, path + fileName, startPath, filter, flags, recursive);
            }
            else if (flags & SCAN_FILES)
#endif
            {
                if (filterExtension.Empty() || fileName.EndsWith(filterExtension))
                    result.Push(deltaPath + fileName);
            }
        }
        SDL_Android_FreeFileList(&list, &count);
        return;
    }
#endif
    #ifdef WIN32
    WIN32_FIND_DATAW info;
    std::wstring path_w(path.toStdWString());
    HANDLE handle = FindFirstFileW((path_w + wchar_t('*')).c_str(), &info);
    if (handle != INVALID_HANDLE_VALUE)
    {
        do
        {
            QString fileName = QString::fromWCharArray(info.cFileName);
            if (!fileName.isEmpty())
            {
                if (info.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN && !(flags & SCAN_HIDDEN))
                    continue;
                if (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                {
                    if (flags & SCAN_DIRS)
                        result.push_back(deltaPath + fileName);
                    if (recursive && fileName != "." && fileName != "..")
                        ScanDirInternal(result, path + fileName, startPath, filter, flags, recursive);
                }
                else if (flags & SCAN_FILES)
                {
                    if (filterExtension.isEmpty() || fileName.endsWith(filterExtension))
                        result.push_back(deltaPath + fileName);
                }
            }
        }
        while (FindNextFileW(handle, &info));

        FindClose(handle);
    }
    #else
    DIR *dir;
    struct dirent *de;
    struct stat st;
    dir = opendir(qPrintable(GetNativePath(path)));
    if (dir)
    {
        while ((de = readdir(dir)))
        {
            /// \todo Filename may be unnormalized Unicode on Mac OS X. Re-normalize as necessary
            QString fileName(de->d_name);
            bool normalEntry = fileName != "." && fileName != "..";
            if (normalEntry && !(flags & SCAN_HIDDEN) && fileName.startsWith("."))
                continue;
            QString pathAndName = path + fileName;
            if (!stat(qPrintable(pathAndName), &st))
            {
                if (st.st_mode & S_IFDIR)
                {
                    if (flags & SCAN_DIRS)
                        result.push_back(deltaPath + fileName);
                    if (recursive && normalEntry)
                        ScanDirInternal(result, path + fileName, startPath, filter, flags, recursive);
                }
                else if (flags & SCAN_FILES)
                {
                    if (filterExtension.isEmpty() || fileName.endsWith(filterExtension))
                        result.push_back(deltaPath + fileName);
                }
            }
        }
        closedir(dir);
    }
    #endif
}

void FileSystem::HandleBeginFrame(StringHash eventType, VariantMap& eventData)
{
    /// Go through the execution queue and post + remove completed requests
    for (std::list<AsyncExecRequest*>::iterator i = asyncExecQueue_.begin(); i != asyncExecQueue_.end();)
    {
        AsyncExecRequest* request = *i;
        if (request->IsCompleted())
        {
            using namespace AsyncExecFinished;

            VariantMap& newEventData = GetEventDataMap();
            newEventData[P_REQUESTID] = request->GetRequestID();
            newEventData[P_EXITCODE] = request->GetExitCode();
            SendEvent(E_ASYNCEXECFINISHED, newEventData);

            delete request;
            i = asyncExecQueue_.erase(i);
        }
        else
            ++i;
    }
}

void FileSystem::HandleConsoleCommand(StringHash eventType, VariantMap& eventData)
{
    using namespace ConsoleCommand;
    if (eventData[P_ID].GetString() == GetTypeName())
        SystemCommand(eventData[P_COMMAND].GetString(), true);
}

void SplitPath(const QString& fullPath, QString& pathName, QString& fileName, QString& extension, bool lowercaseExtension)
{
    QString fullPathCopy = GetInternalPath(fullPath);

    unsigned extPos = fullPathCopy.lastIndexOf('.');
    unsigned pathPos = fullPathCopy.lastIndexOf('/');

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
    unsigned pos = RemoveTrailingSlash(path).lastIndexOf('/');
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
