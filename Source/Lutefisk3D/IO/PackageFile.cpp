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

#include "Lutefisk3D/IO/File.h"
#include "Lutefisk3D/IO/Log.h"
#include "Lutefisk3D/IO/PackageFile.h"
#include "Lutefisk3D/Core/Variant.h"

#include <QFileInfo>
namespace Urho3D
{
template class SharedPtr<PackageFile>;

PackageFile::PackageFile(Context* context) :
    Object(context),
    totalSize_(0),
    totalDataSize_(0),
    checksum_(0),
    compressed_(false)
{
}

PackageFile::PackageFile(Context* context, const QString& fileName, unsigned startOffset) :
    Object(context),
    totalSize_(0),
    totalDataSize_(0),
    checksum_(0),
    compressed_(false)
{
    Open(fileName, startOffset);
}

PackageFile::~PackageFile()
{
}

bool PackageFile::Open(const QString& fileName, unsigned startOffset)
{
    SharedPtr<File> file(new File(context_, fileName));
    if (!file->IsOpen())
        return false;

    // Check ID, then read the directory
    file->Seek(startOffset);
    QString id = file->ReadFileID();
    if (id != "UPAK" && id != "ULZ4")
    {
        // If start offset has not been explicitly specified, also try to read package size from the end of file
        // to know how much we must rewind to find the package start
        if (!startOffset)
        {
            unsigned fileSize = file->GetSize();
            file->Seek(fileSize - sizeof(unsigned));
            unsigned newStartOffset = fileSize - file->ReadUInt();
            if (newStartOffset < fileSize)
            {
                startOffset = newStartOffset;
                file->Seek(startOffset);
                id = file->ReadFileID();
            }
        }

        if (id != "UPAK" && id != "ULZ4")
        {
            URHO3D_LOGERROR(fileName + " is not a valid package file");
            return false;
        }
    }

    fileName_ = fileName;
    nameHash_ = fileName_;
    totalSize_ = file->GetSize();
    compressed_ = id == "ULZ4";

    unsigned numFiles = file->ReadUInt();
    checksum_ = file->ReadUInt();

    for (unsigned i = 0; i < numFiles; ++i)
    {
        QString entryName = file->ReadString();
        PackageEntry newEntry;
        newEntry.offset_ = file->ReadUInt() + startOffset;
        totalDataSize_ += (newEntry.size_ = file->ReadUInt());
        newEntry.checksum_ = file->ReadUInt();
        if (!compressed_ && newEntry.offset_ + newEntry.size_ > totalSize_)
        {
            URHO3D_LOGERROR("File entry " + entryName + " outside package file");
            return false;
        }
        else
            entries_[entryName] = newEntry;
    }

    return true;
}

bool PackageFile::Exists(const QString& fileName) const
{
    return entries_.find(fileName.toLower()) != entries_.end();
}

const PackageEntry* PackageFile::GetEntry(const QString& fileName) const
{
    auto i = entries_.find(fileName.toLower());
    if (i != entries_.end())
        return &MAP_VALUE(i);
    return nullptr;
}

std::vector<QString> PackageFile::GetEntryNames() const
{
    std::vector<QString> res;
    res.reserve(entries_.size());
    for(auto &v : entries_)
        res.emplace_back(v.first);
    return res;
}
void PackageFile::Scan(QStringList &result, const QString& pathName, const QString& filter, bool recursive) const
{
    result.clear();

    QString sanitizedPath = QFileInfo(pathName).filePath();
    QString filterExtension = filter.mid(filter.lastIndexOf('.'));
    if (filterExtension.contains('*'))
        filterExtension.clear();

    auto caseSensitive = Qt::CaseSensitive;
#ifdef _WIN32
    // On Windows ignore case in string comparisons
    caseSensitive = Qt::CaseInsensitive;
#endif

    const auto & entryNames = GetEntryNames();
    for (const auto & i : entryNames)
    {
        QString entryName = QFileInfo(i).filePath();
        if ((filterExtension.isEmpty() || entryName.endsWith(filterExtension, caseSensitive)) &&
            entryName.startsWith(sanitizedPath, Qt::CaseSensitive))
        {
            QString fileName = entryName.mid(sanitizedPath.length());
            if (fileName.startsWith("\\") || fileName.startsWith("/"))
                fileName = fileName.mid(1, fileName.length() - 1);
            if (!recursive && (fileName.contains("\\") || fileName.contains("/")))
                continue;

            result.push_back(fileName);
        }
    }
}
}
