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

#include "../Container/HashMap.h"
#include "../Core/Object.h"


namespace Urho3D
{

class AsyncExecRequest;

/// Return files.
static const unsigned SCAN_FILES = 0x1;
/// Return directories.
static const unsigned SCAN_DIRS = 0x2;
/// Return also hidden files.
static const unsigned SCAN_HIDDEN = 0x4;

/// Subsystem for file and directory operations and access control.
class FileSystem : public Object
{
    OBJECT(FileSystem);

public:
    /// Construct.
    FileSystem(Context* context);
    /// Destruct.
    ~FileSystem();

    /// Set the current working directory.
    bool SetCurrentDir(const QString& pathName);
    /// Create a directory.
    bool CreateDir(const QString& pathName);
    /// Set whether to execute engine console commands as OS-specific system command.
    void SetExecuteConsoleCommands(bool enable);
    /// Run a program using the command interpreter, block until it exits and return the exit code. Will fail if any allowed paths are defined.
    int SystemCommand(const QString& commandLine, bool redirectStdOutToLog = false);
    /// Run a specific program, block until it exits and return the exit code. Will fail if any allowed paths are defined.
    int SystemRun(const QString& fileName, const QStringList& arguments);
    /// Run a program using the command interpreter asynchronously. Return a request ID or M_MAX_UNSIGNED if failed. The exit code will be posted together with the request ID in an AsyncExecFinished event. Will fail if any allowed paths are defined.
    unsigned SystemCommandAsync(const QString& commandLine);
    /// Run a specific program asynchronously. Return a request ID or M_MAX_UNSIGNED if failed. The exit code will be posted together with the request ID in an AsyncExecFinished event. Will fail if any allowed paths are defined.
    unsigned SystemRunAsync(const QString& fileName, const QStringList& arguments);
    /// Open a file in an external program, with mode such as "edit" optionally specified. Will fail if any allowed paths are defined.
    bool SystemOpen(const QString& fileName, const QString& mode = QString::null);
    /// Copy a file. Return true if successful.
    bool Copy(const QString& srcFileName, const QString& destFileName);
    /// Rename a file. Return true if successful.
    bool Rename(const QString& srcFileName, const QString& destFileName);
    /// Delete a file. Return true if successful.
    bool Delete(const QString& fileName);
    /// Register a path as allowed to access. If no paths are registered, all are allowed. Registering allowed paths is considered securing the Urho3D execution environment: running programs and opening files externally through the system will fail afterward.
    void RegisterPath(const QString& pathName);
    /// Set a file's last modified time as seconds since 1.1.1970. Return true on success.
    bool SetLastModifiedTime(const QString& fileName, unsigned newTime);

    /// Return the absolute current working directory.
    QString GetCurrentDir() const;
    /// Return whether is executing engine console commands as OS-specific system command.
    bool GetExecuteConsoleCommands() const { return executeConsoleCommands_; }
    /// Return whether paths have been registered.
    bool HasRegisteredPaths() const { return !allowedPaths_.empty(); }
    /// Check if a path is allowed to be accessed. If no paths are registered, all are allowed.
    bool CheckAccess(const QString& pathName) const;
    /// Returns the file's last modified time as seconds since 1.1.1970, or 0 if can not be accessed.
    unsigned GetLastModifiedTime(const QString& fileName) const;
    /// Check if a file exists.
    bool FileExists(const QString& fileName) const;
    /// Check if a directory exists.
    bool DirExists(const QString& pathName) const;
    /// Scan a directory for specified files.
    void ScanDir(QStringList& result, const QString& pathName, const QString& filter, unsigned flags, bool recursive) const;
    /// Return the program's directory. If it does not contain the Urho3D default CoreData and Data directories, and the current working directory does, return the working directory instead.
    QString GetProgramDir() const;
    /// Return the user documents directory.
    QString GetUserDocumentsDir() const;
    /// Return the application preferences directory.
    QString GetAppPreferencesDir(const QString& org, const QString& app) const;

private:
    /// Scan directory, called internally.
    void ScanDirInternal(QStringList& result, QString path, const QString& startPath, const QString& filter, unsigned flags, bool recursive) const;
    /// Handle begin frame event to check for completed async executions.
    void HandleBeginFrame(StringHash eventType, VariantMap& eventData);
    /// Handle a console command event.
    void HandleConsoleCommand(StringHash eventType, VariantMap& eventData);

    /// Allowed directories.
    HashSet<QString> allowedPaths_;
    /// Cached program directory.
    mutable QString programDir_;
    /// Async execution queue.
    std::list<AsyncExecRequest*> asyncExecQueue_;
    /// Next async execution ID.
    unsigned nextAsyncExecID_;
    /// Flag for executing engine console commands as OS-specific system command. Default to true.
    bool executeConsoleCommands_;
};

/// Split a full path to path, filename and extension. The extension will be converted to lowercase by default.
void SplitPath(const QString& fullPath, QString& pathName, QString& fileName, QString& extension, bool lowercaseExtension = true);
/// Return the path from a full path.
QString GetPath(const QString& fullPath);
/// Return the filename from a full path.
QString GetFileName(const QString& fullPath);
/// Return the extension from a full path, converted to lowercase by default.
QString GetExtension(const QString& fullPath, bool lowercaseExtension = true);
/// Return the filename and extension from a full path. The case of the extension is preserved by default, so that the file can be opened in case-sensitive operating systems.
QString GetFileNameAndExtension(const QString& fullPath, bool lowercaseExtension = false);
/// Replace the extension of a file name with another.
QString ReplaceExtension(const QString& fullPath, const QString& newExtension);
/// Add a slash at the end of the path if missing and convert to internal format (use slashes.)
QString AddTrailingSlash(const QString& pathName);
/// Remove the slash from the end of a path if exists and convert to internal format (use slashes.)
QString RemoveTrailingSlash(const QString& pathName);
/// Return the parent path, or the path itself if not available.
QString GetParentPath(const QString& pathName);
/// Convert a path to internal format (use slashes.)
QString GetInternalPath(const QString& pathName);
/// Convert a path to the format required by the operating system.
QString GetNativePath(const QString& pathName);
/// Convert a path to the format required by the operating system in wide characters.
//WString GetWideNativePath(const String& pathName);
/// Return whether a path is absolute.
bool IsAbsolutePath(const QString& pathName);

}
