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

#include "File.h"

#include "FileSystem.h"
#include "Log.h"
#include "Lutefisk3D/Math/MathDefs.h"
#include "MemoryBuffer.h"
#include "PackageFile.h"
#include "Lutefisk3D/Core/Profiler.h"
#include "Lutefisk3D/Core/Context.h"
#include "Lutefisk3D/Container/Str.h"
#include "Lutefisk3D/Container/ArrayPtr.h"

#include <QFile>
#include <QDebug>
#include <cstdio>
#include <LZ4/lz4.h>

namespace Urho3D
{
template class SharedPtr<File>;

QFile::OpenMode openMode[] = {
   QFile::ReadOnly,
   QFile::WriteOnly,
   QFile::ReadWrite|QFile::Append,
   QFile::ReadWrite|QFile::Truncate,
};

static const unsigned SKIP_BUFFER_SIZE = 1024;

File::File(Context* context) :
    context_(context)
{
}

File::File(Context* context, const QString& fileName, FileMode mode) :
    context_(context)
{
    Open(fileName, mode);
}

File::File(Context* context, PackageFile* package, const QString& fileName) :
    context_(context)
{
    Open(package, fileName);
}

File::~File()
{
    Close();
}

bool File::Open(const QString& fileName, FileMode mode)
{
    Close();

    FileSystem* fileSystem = context_->m_FileSystem.get();
    if (fileSystem && !fileSystem->CheckAccess(GetPath(fileName)))
    {
        URHO3D_LOGERROR(QString("Access denied to %1").arg(fileName));
        return false;
    }

    if (fileName.isEmpty())
    {
        URHO3D_LOGERROR("Could not open file with empty name");
        return false;
    }

    QFile *tmp = new QFile(fileName);
    handle_=nullptr;
    if(!tmp->open(openMode[mode])) {
        qDebug() << tmp->errorString();
        delete tmp;
    }
    else
        handle_ = tmp;

    // If file did not exist in readwrite mode, retry with write-update mode
    if (mode == FILE_READWRITE && !handle_)
    {
        tmp = new QFile(fileName);
        if(!tmp->open(openMode[mode+1])) {
            delete tmp;
        }
        else
            handle_ = tmp;
    }

    if (!handle_)
    {
        URHO3D_LOGERROR(QString("Could not open file ") + fileName);
        return false;
    }

    fileName_ = fileName;
    mode_ = mode;
    position_ = 0;
    offset_ = 0;
    checksum_ = 0;
    compressed_ = false;
    readSyncNeeded_ = false;
    writeSyncNeeded_ = false;

    qint64 size = ((QFile *)handle_)->size();
    if (size > M_MAX_UNSIGNED)
    {
        URHO3D_LOGERROR(QString("Could not open file %1 which is larger than 4GB").arg(fileName));
        Close();
        size_ = 0;
        return false;
    }
    size_ = (unsigned)size;
    return true;
}

bool File::Open(PackageFile* package, const QString& fileName)
{
    Close();

    if (!package)
        return false;

    const PackageEntry* entry = package->GetEntry(fileName);
    if (!entry)
        return false;

    QFile *tmp = new QFile(package->GetName());
    handle_=nullptr;
    if(!tmp->open(QFile::ReadOnly)) {
        delete tmp;
    }
    else
        handle_ = tmp;
    if (!handle_)
    {
        URHO3D_LOGERROR("Could not open package file " + fileName);
        return false;
    }

    fileName_ = fileName;
    mode_ = FILE_READ;
    offset_ = entry->offset_;
    checksum_ = entry->checksum_;
    position_ = 0;
    size_ = entry->size_;
    compressed_ = package->IsCompressed();
    readSyncNeeded_ = false;
    writeSyncNeeded_ = false;

    // Seek to beginning of package entry's file data
    ((QFile *)handle_)->seek(offset_);
    return true;
}

unsigned File::Read(void* dest, unsigned size)
{
    if (!handle_)
    {
        // If file not open, do not log the error further here to prevent spamming the stderr stream
        return 0;
    }

    if (mode_ == FILE_WRITE)
    {
        URHO3D_LOGERROR("File not opened for reading");
        return 0;
    }

    if (size + position_ > size_)
        size = size_ - position_;
    if (!size)
        return 0;

    if (compressed_)
    {
        unsigned sizeLeft = size;
        uint8_t* destPtr = (uint8_t*)dest;

        while (sizeLeft)
        {
            if (!readBuffer_ || readBufferOffset_ >= readBufferSize_)
            {
                uint8_t blockHeaderBytes[4];
                ((QFile *)handle_)->read((char *)blockHeaderBytes, sizeof blockHeaderBytes);

                MemoryBuffer blockHeader(&blockHeaderBytes[0], sizeof blockHeaderBytes);
                unsigned unpackedSize = blockHeader.ReadUShort();
                unsigned packedSize = blockHeader.ReadUShort();

                if (!readBuffer_)
                {
                    readBuffer_.reset(new uint8_t[unpackedSize]);
                    inputBuffer_.reset(new uint8_t[LZ4_compressBound(unpackedSize)]);
                }

                /// \todo Handle errors
                ((QFile *)handle_)->read((char *)inputBuffer_.get(), packedSize);
                LZ4_decompress_fast((const char*)inputBuffer_.get(), (char *)readBuffer_.get(), unpackedSize);

                readBufferSize_ = unpackedSize;
                readBufferOffset_ = 0;
            }

            unsigned copySize = std::min((int)(readBufferSize_ - readBufferOffset_), (int)sizeLeft);
            memcpy(destPtr, readBuffer_.get() + readBufferOffset_, copySize);
            destPtr += copySize;
            sizeLeft -= copySize;
            readBufferOffset_ += copySize;
            position_ += copySize;
        }

        return size;
    }

    // Need to reassign the position due to internal buffering when transitioning from writing to reading
    if (readSyncNeeded_)
    {
        ((QFile *)handle_)->seek(position_ + offset_);
        readSyncNeeded_ = false;
    }

    size_t ret = ((QFile *)handle_)->read((char *)dest, size);
    if (ret != size)
    {
        // Return to the position where the read began
        ((QFile *)handle_)->seek(position_ + offset_);
        URHO3D_LOGERROR("Error while reading from file " + GetName());
        return 0;
    }

    writeSyncNeeded_ = true;
    position_ += size;
    return size;
}

unsigned File::Seek(unsigned position)
{
    if (!handle_)
    {
        // If file not open, do not log the error further here to prevent spamming the stderr stream
        return 0;
    }

    // Allow sparse seeks if writing
    if (mode_ == FILE_READ && position > size_)
        position = size_;

    if (compressed_)
    {
        // Start over from the beginning
        if (position == 0)
        {
            position_ = 0;
            readBufferOffset_ = 0;
            readBufferSize_ = 0;
            ((QFile *)handle_)->seek(offset_);
        }
        // Skip bytes
        else if (position >= position_)
        {
            uint8_t skipBuffer[SKIP_BUFFER_SIZE];
            while (position > position_)
                Read(skipBuffer, std::min((int)(position - position_), (int)SKIP_BUFFER_SIZE));
        }
        else
            URHO3D_LOGERROR("Seeking backward in a compressed file is not supported");

        return position_;
    }

    ((QFile*)handle_)->seek(position + offset_);
    position_ = position;
    readSyncNeeded_ = false;
    writeSyncNeeded_ = false;
    return position_;
}

unsigned File::Write(const void* data, unsigned size)
{
    if (!handle_)
    {
        // If file not open, do not log the error further here to prevent spamming the stderr stream
        return 0;
    }

    if (mode_ == FILE_READ)
    {
        URHO3D_LOGERROR("File not opened for writing");
        return 0;
    }

    if (!size)
        return 0;

    // Need to reassign the position due to internal buffering when transitioning from reading to writing
    if (writeSyncNeeded_)
    {
        ((QFile *)handle_)->seek(position_ + offset_);
        writeSyncNeeded_ = false;
    }

    if (((QFile *)handle_)->write((const char *)data, size) != size)
    {
        // Return to the position where the write began
        ((QFile *)handle_)->seek(position_ + offset_);
        URHO3D_LOGERROR("Error while writing to file " + GetName());
        return 0;
    }

    readSyncNeeded_ = true;
    position_ += size;
    if (position_ > size_)
        size_ = position_;

    return size;
}

unsigned File::GetChecksum()
{
    if (offset_ || checksum_)
        return checksum_;
    if (!handle_ || mode_ == FILE_WRITE)
        return 0;

    URHO3D_PROFILE(CalculateFileChecksum);

    unsigned oldPos = position_;
    checksum_ = 0;

    Seek(0);
    while (!IsEof())
    {
        uint8_t block[1024];
        unsigned readBytes = Read(block, 1024);
        for (unsigned i = 0; i < readBytes; ++i)
            checksum_ = SDBMHash(checksum_, block[i]);
    }

    Seek(oldPos);
    return checksum_;
}

void File::Close()
{
    readBuffer_.reset();
    inputBuffer_.reset();

    if (handle_)
    {
        ((QFile *)handle_)->close();
        delete ((QFile *)handle_);
        handle_ = nullptr;
        position_ = 0;
        size_ = 0;
        offset_ = 0;
        checksum_ = 0;
    }
}

void File::Flush()
{
    if (handle_)
        ((QFile *)handle_)->flush();
}

void File::SetName(const QString& name)
{
    fileName_ = name;
}

bool File::IsOpen() const
{
    return handle_ != nullptr;
}
void File::ReadText(QString& text)
{
    text.clear();

    if (!size_)
        return;

    QByteArray tgt;
    tgt.resize(size_);
    Read((void*)tgt.data(), size_);
    text = tgt;
}

bool File::Copy(File* srcFile)
{
    if (!srcFile || !srcFile->IsOpen() || srcFile->GetMode() != FILE_READ)
        return false;

    if (!IsOpen() || GetMode() != FILE_WRITE)
        return false;

    unsigned fileSize = srcFile->GetSize();
    SharedArrayPtr<uint8_t> buffer(new uint8_t[fileSize]);

    unsigned bytesRead = srcFile->Read(buffer.get(), fileSize);
    unsigned bytesWritten = Write(buffer.get(), fileSize);
    return bytesRead == fileSize && bytesWritten == fileSize;
}
}
