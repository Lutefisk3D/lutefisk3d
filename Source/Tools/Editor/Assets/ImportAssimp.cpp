//
// Copyright (c) 2018 Rokas Kupstys
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

#include <Lutefisk3D/Core/ProcessUtils.h>
#include <Lutefisk3D/IO/FileSystem.h>
#include <Lutefisk3D/IO/Log.h>
#include <QProcess>
#include "Project.h"
#include "ImportAssimp.h"


namespace Urho3D
{

ImportAssimp::ImportAssimp(Context* context)
    : ImportAsset(context)
{
}

bool ImportAssimp::Accepts(const QString& path)
{
    QString extension = GetExtension(path);
    return extension == ".fbx" || extension == ".blend";
}

bool ImportAssimp::Convert(const QString& path)
{
    bool importedAny = false;
    auto* project = GetSubsystem<Project>();
    assert(path.startsWith(project->GetResourcePath()));

    const auto& cachePath = project->GetCachePath();
    auto resourceName = path.mid(project->GetResourcePath().length());
    auto resourceFileName = GetFileName(path);
    auto outputDir = cachePath + AddTrailingSlash(resourceName);
    GetFileSystem()->CreateDirsRecursive(outputDir);

    // Import models
    {
        QString outputPath = outputDir + resourceFileName + ".mdl";

        QStringList args{"model", path, outputPath, "-na", "-ns"};
        QProcess process;
        process.setProgram(GetFileSystem()->GetProgramDir() + "AssetImporter");
        process.setArguments(args);
        process.start();
        process.waitForFinished();
        process.error();
        if (process.error() == 0 && GetFileSystem()->FileExists(outputPath))
            importedAny = true;
    }

    // Import animations
    {
        QString outputPath = cachePath + resourceName;

        QStringList args{"anim", path, outputPath, "-nm", "-nt", "-nc", "-ns"};
        QProcess process;
        process.setProgram(GetFileSystem()->GetProgramDir() + "AssetImporter");
        process.setArguments(args);
        process.start();
        process.waitForFinished();
        process.error();
        if (GetFileSystem()->FileExists(outputPath))
            importedAny = true;
    }

    return importedAny;
}

}
