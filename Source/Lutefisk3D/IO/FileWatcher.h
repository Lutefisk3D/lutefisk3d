//
// Copyright (c) 2008-2017 the Urho3D project.
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
#include "Lutefisk3D/Core/Thread.h"
#include "Lutefisk3D/Core/Timer.h"
#include "Lutefisk3D/Container/Str.h"

namespace Urho3D
{
class FileSystem;
#define LUTEFISK3D_FILEWATCHER
/// Watches a directory and its subdirectories for files being modified.
class LUTEFISK3D_EXPORT FileWatcher : public Object, public Thread
{
    URHO3D_OBJECT(FileWatcher, Object)

public:
    /// Construct.
    FileWatcher(Context* context);
    /// Destruct.
    virtual ~FileWatcher();

    /// Directory watching loop.
    void ThreadFunction() override;

    /// Start watching a directory. Return true if successful.
    bool StartWatching(const QString& pathName, bool watchSubDirs);
    /// Stop watching the directory.
    void StopWatching();
    /// Set the delay in seconds before file changes are notified. This (hopefully) avoids notifying when a file save is still in progress. Default 1 second.
    void SetDelay(float interval);
    /// Add a file change into the changes queue.
    void AddChange(const QString& fileName);
    /// Return a file change (true if was found, false if not.)
    bool GetNextChange(QString& dest);

    /// Return the path being watched, or empty if not watching.
    const QString& GetPath() const { return path_; }
    /// Return the delay in seconds for notifying file changes.
    float GetDelay() const { return delay_;}

private:
    /// Filesystem.
    FileSystem *fileSystem_;
    /// The path being watched.
    QString path_;
    /// Pending changes. These will be returned and removed from the list when their timer has exceeded the delay.
    HashMap<QString, Timer> changes_;
    /// Mutex for the change buffer.
    Mutex changesMutex_;
    /// Delay in seconds for notifying changes.
    float delay_;
    /// Watch subdirectories flag.
    bool watchSubDirs_;

#ifdef _WIN32

    /// Directory handle for the path being watched.
    void* dirHandle_;

#elif __linux__

    /// HashMap for the directory and sub-directories (needed for inotify's int handles).
    HashMap<int, QString> dirHandle_;
    /// Linux inotify needs a handle.
    int watchHandle_;

#elif defined(__APPLE__) && !defined(IOS)

    /// Flag indicating whether the running OS supports individual file watching.
    bool supported_;
    /// Pointer to internal MacFileWatcher delegate.
    void* watcher_;

#endif
};

}
