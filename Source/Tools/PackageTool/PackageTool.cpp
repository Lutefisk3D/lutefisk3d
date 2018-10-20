//
// Copyright (c) 2008-2018 the Urho3D project.
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

#include <Lutefisk3D/Core/Context.h>
#include <Lutefisk3D/Container/ArrayPtr.h>
#include <Lutefisk3D/Core/ProcessUtils.h>
#include <Lutefisk3D/IO/File.h>
#include <Lutefisk3D/IO/FileSystem.h>
#include <Lutefisk3D/IO/PackageFile.h>

#ifdef WIN32
#include <windows.h>
#endif

#include <LZ4/lz4.h>
#include <LZ4/lz4hc.h>

using namespace Urho3D;

static const unsigned COMPRESSED_BLOCK_SIZE = 32768;

struct FileEntry
{
    QString name_;
    unsigned offset_{};
    unsigned size_{};
    unsigned checksum_{};
};

std::unique_ptr<Context> context_(new Context());
std::unique_ptr<FileSystem> fileSystem_(new FileSystem(context_.get()));
QString basePath_;
std::vector<FileEntry> entries_;
unsigned checksum_ = 0;
bool compress_ = false;
bool quiet_ = false;
unsigned blockSize_ = COMPRESSED_BLOCK_SIZE;

QString ignoreExtensions_[] = {
    ".bak",
    ".rule",
    ""
};

int main(int argc, char** argv);
void Run(const QStringList &arguments);
void ProcessFile(const QString& fileName, const QString &rootDir);
void WritePackageFile(const QString &fileName, const QString& rootDir);
void WriteHeader(File& dest);

int main(int argc, char** argv)
{
    QStringList arguments;

    #ifdef WIN32
    arguments = ParseArguments(GetCommandLine());
    #else
    arguments = ParseArguments(argc, argv);
    #endif

    Run(arguments);
    return 0;
}

void Run(const QStringList &arguments)
{
    if (arguments.size() < 2)
        ErrorExit(
            "Usage: PackageTool <directory to process> <package name> [basepath] [options]\n"
            "\n"
            "Options:\n"
            "-c      Enable package file LZ4 compression\n"
            "-q      Enable quiet mode\n"
            "\n"
            "Basepath is an optional prefix that will be added to the file entries.\n\n"
            "Alternative output usage: PackageTool <output option> <package name>\n"
            "Output option:\n"
            "-i      Output package file information\n"
            "-l      Output file names (including their paths) contained in the package\n"
            "-L      Similar to -l but also output compression ratio (compressed package file only)\n"
        );

    const QString& dirName = arguments[0];
    const QString& packageName = arguments[1];
    bool isOutputMode = arguments[0].length() == 2 && arguments[0][0] == '-';
    if (arguments.size() > 2)
    {
        for (unsigned i = 2; i < arguments.size(); ++i)
        {
            if (arguments[i][0] != '-')
                basePath_ = AddTrailingSlash(arguments[i]);
            else
            {
                if (arguments[i].length() > 1)
                {
                    switch (arguments[i][1].toLatin1())
                    {
                    case 'c':
                        compress_ = true;
                        break;
                    case 'q':
                        quiet_ = true;
                        break;
                    default:
                        ErrorExit("Unrecognized option");
                    }
                }
            }
        }
    }

    if (!isOutputMode)
    {
        if (!quiet_)
            PrintLine("Scanning directory " + dirName + " for files");

        // Get the file list recursively
        QStringList fileNames;
        fileSystem_->ScanDir(fileNames, dirName, "*.*", SCAN_FILES, true);
        if (!fileNames.size())
            ErrorExit("No files found");

        // Check for extensions to ignore
        for (unsigned i = fileNames.size() - 1; i < fileNames.size(); --i)
        {
            QString extension = GetExtension(fileNames[i]);
            for (unsigned j = 0; j < ignoreExtensions_[j].length(); ++j)
            {
                if (extension == ignoreExtensions_[j])
                {
                    fileNames.erase(fileNames.begin() + i);
                    break;
                }
            }
        }

        for (unsigned i = 0; i < fileNames.size(); ++i)
            ProcessFile(fileNames[i], dirName);

        WritePackageFile(packageName, dirName);
    }
    else
    {
        SharedPtr<PackageFile> packageFile(new PackageFile(context_.get(), packageName));
        bool outputCompressionRatio = false;
        switch (arguments[0][1].toLatin1())
        {
        case 'i':
            PrintLine("Number of files: " + QString::number(packageFile->GetNumFiles()));
            PrintLine("File data size: " + QString::number(packageFile->GetTotalDataSize()));
            PrintLine("Package size: " + QString::number(packageFile->GetTotalSize()));
            PrintLine("Checksum: " + QString::number(packageFile->GetChecksum()));
            PrintLine("Compressed: " + QString(packageFile->IsCompressed() ? "yes" : "no"));
            break;
        case 'L':
            if (!packageFile->IsCompressed())
                ErrorExit("Invalid output option: -L is applicable for compressed package file only");
            outputCompressionRatio = true;
            // Fallthrough
        case 'l':
            {
                const HashMap<QString, PackageEntry>& entries = packageFile->GetEntries();
                for (auto i = entries.begin(); i != entries.end();)
                {
                    auto current = i++;
                    QString fileEntry(current->first);
                    if (outputCompressionRatio)
                    {
                        unsigned compressedSize =
                            (i == entries.end() ? packageFile->GetTotalSize() - sizeof(unsigned) : i->second.offset_) -
                            current->second.offset_;
                        fileEntry+=QString::asprintf("\tin: %u\tout: %u\tratio: %f", current->second.size_, compressedSize,
                            compressedSize ? 1.f * current->second.size_ / compressedSize : 0.f);
                    }
                    PrintLine(fileEntry);
                }
            }
            break;
        default:
            ErrorExit("Unrecognized output option");
        }
    }
}

void ProcessFile(const QString &fileName, const QString& rootDir)
{
    QString fullPath = rootDir + "/" + fileName;
    File file(context_.get());
    if (!file.Open(fullPath))
        ErrorExit("Could not open file " + fileName);
    if (!file.GetSize())
        return;

    FileEntry newEntry;
    newEntry.name_ = fileName;
    newEntry.offset_ = 0; // Offset not yet known
    newEntry.size_ = file.GetSize();
    newEntry.checksum_ = 0; // Will be calculated later
    entries_.push_back(newEntry);
}

void WritePackageFile(const QString& fileName, const QString &rootDir)
{
    if (!quiet_)
        PrintLine("Writing package");

    File dest(context_.get());
    if (!dest.Open(fileName, FILE_WRITE))
        ErrorExit("Could not open output file " + fileName);

    // Write ID, number of files & placeholder for checksum
    WriteHeader(dest);

    for (unsigned i = 0; i < entries_.size(); ++i)
    {
        // Write entry (correct offset is still unknown, will be filled in later)
        dest.WriteString(basePath_ + entries_[i].name_);
        dest.WriteUInt(entries_[i].offset_);
        dest.WriteUInt(entries_[i].size_);
        dest.WriteUInt(entries_[i].checksum_);
    }

    unsigned totalDataSize = 0;
    unsigned lastOffset;

    // Write file data, calculate checksums & correct offsets
    for (unsigned i = 0; i < entries_.size(); ++i)
    {
        lastOffset = entries_[i].offset_ = dest.GetSize();
        QString fileFullPath = rootDir + "/" + entries_[i].name_;

        File srcFile(context_.get(), fileFullPath);
        if (!srcFile.IsOpen())
            ErrorExit("Could not open file " + fileFullPath);

        unsigned dataSize = entries_[i].size_;
        totalDataSize += dataSize;
        SharedArrayPtr<unsigned char> buffer(new unsigned char[dataSize]);

        if (srcFile.Read(&buffer[0], dataSize) != dataSize)
            ErrorExit("Could not read file " + fileFullPath);
        srcFile.Close();

        for (unsigned j = 0; j < dataSize; ++j)
        {
            checksum_ = SDBMHash(checksum_, buffer[j]);
            entries_[i].checksum_ = SDBMHash(entries_[i].checksum_, buffer[j]);
        }

        if (!compress_)
        {
            if (!quiet_)
                PrintLine(entries_[i].name_ + " size " + QString::number(dataSize));
            dest.Write(&buffer[0], entries_[i].size_);
        }
        else
        {
            SharedArrayPtr<unsigned char> compressBuffer(new unsigned char[LZ4_compressBound(blockSize_)]);

            unsigned pos = 0;

            while (pos < dataSize)
            {
                unsigned unpackedSize = blockSize_;
                if (pos + unpackedSize > dataSize)
                    unpackedSize = dataSize - pos;

                auto packedSize = (unsigned)LZ4_compress_HC((const char*)&buffer[pos], (char*)compressBuffer.get(), unpackedSize, LZ4_compressBound(unpackedSize), 0);
                if (!packedSize)
                    ErrorExit("LZ4 compression failed for file " + entries_[i].name_ + " at offset " + QString::number(pos));

                dest.WriteUShort((unsigned short)unpackedSize);
                dest.WriteUShort((unsigned short)packedSize);
                dest.Write(compressBuffer.get(), packedSize);

                pos += unpackedSize;
            }

            if (!quiet_)
            {
                unsigned totalPackedBytes = dest.GetSize() - lastOffset;
                QString fileEntry(entries_[i].name_);
                fileEntry+=QString::asprintf("\tin: %u\tout: %u\tratio: %f", dataSize, totalPackedBytes,
                    totalPackedBytes ? 1.f * dataSize / totalPackedBytes : 0.f);
                PrintLine(fileEntry);
            }
        }
    }

    // Write package size to the end of file to allow finding it linked to an executable file
    unsigned currentSize = dest.GetSize();
    dest.WriteUInt(currentSize + sizeof(unsigned));

    // Write header again with correct offsets & checksums
    dest.Seek(0);
    WriteHeader(dest);

    for (unsigned i = 0; i < entries_.size(); ++i)
    {
        dest.WriteString(basePath_ + entries_[i].name_);
        dest.WriteUInt(entries_[i].offset_);
        dest.WriteUInt(entries_[i].size_);
        dest.WriteUInt(entries_[i].checksum_);
    }

    if (!quiet_)
    {
        PrintLine("Number of files: " + QString::number(entries_.size()));
        PrintLine("File data size: " + QString::number(totalDataSize));
        PrintLine("Package size: " + QString::number(dest.GetSize()));
        PrintLine("Checksum: " + QString::number(checksum_));
        PrintLine("Compressed: " + QString(compress_ ? "yes" : "no"));
    }
}

void WriteHeader(File& dest)
{
    if (!compress_)
        dest.WriteFileID("UPAK");
    else
        dest.WriteFileID("ULZ4");
    dest.WriteUInt(entries_.size());
    dest.WriteUInt(checksum_);
}
