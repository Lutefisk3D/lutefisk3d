// OgreBatchConverter.cpp : Defines the entry point for the console application.
//

#include <Lutefisk3D/Core/Context.h>
#include <Lutefisk3D/Core/ProcessUtils.h>
#include <Lutefisk3D/IO/FileSystem.h>

#include <cstdio>
#include <QtCore/QDebug>
using namespace Urho3D;

std::unique_ptr<Context> context(new Context());
std::unique_ptr<FileSystem> fileSystem(new FileSystem(context.get()));

int main(int argc, char** argv)
{
    // Take in account args and place on OgreImporter args
    const auto & args = ParseArguments(argc, argv);
    QStringList files;
    QString currentDir = fileSystem->GetCurrentDir();

    // Try to execute OgreImporter from same directory as this executable
    QString ogreImporterName = fileSystem->GetProgramDir() + "OgreImporter";

    qDebug("\n\nOgreBatchConverter requires OgreImporter.exe on same directory");
    qDebug() <<"Searching Ogre file in Xml format in"<<currentDir;
    fileSystem->ScanDir(files, currentDir, "*.xml", SCAN_FILES, true);
    printf("\nFound %d files\n", files.size());
    #ifdef WIN32
    if (!files.empty())
        fileSystem->SystemCommand("pause");
    #endif

    for (unsigned i = 0 ; i < files.size(); i++)
    {
        QStringList cmdArgs;
        cmdArgs.push_back(files[i]);
        cmdArgs.push_back(ReplaceExtension(files[i], ".mdl"));
        cmdArgs.append(args);

        QString cmdPreview = ogreImporterName;
        for (const auto & cmdArg : cmdArgs)
            cmdPreview += " " + cmdArg;

        qDebug() << cmdPreview;
        fileSystem->SystemRun(ogreImporterName, cmdArgs);
    }

    printf("\nExit\n");
    #ifdef WIN32
    fileSystem->SystemCommand("pause");
    #endif

    return 0;
}

