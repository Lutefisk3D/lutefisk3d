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

#include "Lutefisk3D/Container/RefCounted.h"
#include "Lutefisk3D/Container/ArrayPtr.h"
#include "Lutefisk3D/IO/AbstractFile.h"
#include <QtCore/QString>

namespace Urho3D
{
class Context;
/// File open mode.
enum FileMode
{
    FILE_READ = 0,
    FILE_WRITE,
    FILE_READWRITE
};

class PackageFile;

/// %File opened either through the filesystem or from within a package file.
class LUTEFISK3D_EXPORT File : public RefCounted, public AbstractFile
{
public:
    /// Construct.
    File(Context* context);
    /// Construct and open a filesystem file.
    File(Context* context, const QString& fileName, FileMode mode = FILE_READ);
    /// Construct and open from a package file.
    File(Context* context, PackageFile* package, const QString& fileName);
    /// Destruct. Close the file if open.
    virtual ~File();

    /// Read bytes from the file. Return number of bytes actually read.
    virtual unsigned Read(void* dest, unsigned size) override;
    /// Set position from the beginning of the file.
    virtual unsigned Seek(unsigned position) override;
    /// Write bytes to the file. Return number of bytes actually written.
    virtual unsigned Write(const void* data, unsigned size) override;
    /// Return the file name.
    virtual const QString& GetName() const override { return fileName_; }
    /// Return a checksum of the file contents using the SDBM hash algorithm.
    virtual unsigned GetChecksum() override;

    /// Open a filesystem file. Return true if successful.
    bool Open(const QString& fileName, FileMode mode = FILE_READ);
    /// Open from within a package file. Return true if successful.
    bool Open(PackageFile* package, const QString& fileName);
    /// Close the file.
    void Close();
    /// Flush any buffered output to the file.
    void Flush();
    /// Change the file name. Used by the resource system.
    void SetName(const QString& name);

    /// Return the open mode.
    FileMode GetMode() const { return mode_; }
    /// Return whether is open.
    bool IsOpen() const;
    /// Return the file handle.
    void* GetHandle() const { return handle_; }
    /// Return whether the file originates from a package.
    bool IsPackaged() const { return offset_ != 0; }

private:
    /// Open file internally using either C standard IO functions or SDL RWops for Android asset files. Return true if successful.
    bool OpenInternal(const QString& fileName, FileMode mode, bool fromPackage = false);
    /// Perform the file read internally using either C standard IO functions or SDL RWops for Android asset files. Return true if successful. This does not handle compressed package file reading.
    bool ReadInternal(void* dest, unsigned size);
    /// Seek in file internally using either C standard IO functions or SDL RWops for Android asset files.
    void SeekInternal(unsigned newPosition);
    Context* context_;
    /// File name.
    QString fileName_;
    /// Open mode.
    FileMode mode_;
    /// File handle.
    void* handle_;
    /// Read buffer for Android asset or compressed file loading.
    SharedArrayPtr<uint8_t> readBuffer_;
    /// Decompression input buffer for compressed file loading.
    SharedArrayPtr<uint8_t> inputBuffer_;
    /// Read buffer position.
    unsigned readBufferOffset_;
    /// Bytes in the current read buffer.
    unsigned readBufferSize_;
    /// Start position within a package file, 0 for regular files.
    unsigned offset_;
    /// Content checksum.
    unsigned checksum_;
    /// Compression flag.
    bool compressed_;
    /// Synchronization needed before read -flag.
    bool readSyncNeeded_;
    /// Synchronization needed before write -flag.
    bool writeSyncNeeded_;
};

}
