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

#if LUTEFISK3D_PLUGINS
#define CR_HOST 1

#include "Editor.h"

#include <Lutefisk3D/Core/CoreEvents.h>
#include <Lutefisk3D/IO/File.h>
#include <Lutefisk3D/IO/FileSystem.h>
#include <Lutefisk3D/IO/Log.h>
#include "PluginManager.h"
#include "EditorEventsPrivate.h"


namespace Urho3D
{

Plugin::Plugin(Context* context)
    : Object(context)
{
}

PluginManager::PluginManager(Context* context)
    : Object(context)
{
    CleanUp();
    g_coreSignals.endFrame.Connect(this,&PluginManager::OnEndFrame);
}

PluginType PluginManager::GetPluginType(const QString& path)
{
    File file(context_);
    if (!file.Open(path, FILE_READ))
        return PLUGIN_INVALID;

    // This function implements a naive check for plugin validity. Proper check would parse executable headers and look
    // for relevant exported function names.

#if __linux__
    // ELF magic
    if (path.endsWith(".so"))
    {
        if (file.ReadUInt() == 0x464C457F)
        {
            file.Seek(0);
            QByteArray buf;
            buf.resize(file.GetSize());
            file.Read(buf.data(), file.GetSize());
            auto pos = buf.indexOf("cr_main");
            // Function names are preceeded with 0 in elf files.
            if (pos != -1 && buf.data()[pos - 1] == 0)
                return PLUGIN_NATIVE;
        }
    }
#endif
    file.Seek(0);
    if (path.endsWith(".dll"))
    {
        if (file.ReadShort() == 0x5A4D)
        {
#if _WIN32
            // But only on windows we check if PE file is a native plugin
            file.Seek(0);
            QByteArray buf;
            buf.resize(file.GetSize());
            file.Read(buf.data(), file.GetSize());
            auto pos = buf.indexOf("cr_main");
            // Function names are preceeded with 2 byte hint which is preceeded with 0 in PE files.
            if (pos != -1 && buf.data()[pos - 3] == 0)
                return PLUGIN_NATIVE;
#endif
            // PE files are handled on all platforms because managed executables are PE files.
            file.Seek(0x3C);
            auto e_lfanew = file.ReadUInt();
#if LUTEFISK3D_64BIT
            const auto netMetadataRvaOffset = 0xF8;
#else
            const auto netMetadataRvaOffset = 0xE8;
#endif
            file.Seek(e_lfanew + netMetadataRvaOffset);  // Seek to .net metadata directory rva

            if (file.ReadUInt() != 0)
                return PLUGIN_MANAGED;
        }
    }

    if (path.endsWith(".dylib"))
    {
        // TODO: MachO file support.
    }

    return PLUGIN_INVALID;
}

Plugin* PluginManager::Load(const QString& path)
{
    if (Plugin* loaded = GetPlugin(path))
        return loaded;

    CleanUp();

    SharedPtr<Plugin> plugin(new Plugin(context_));
    plugin->type_ = GetPluginType(path);

    if (plugin->type_ == PLUGIN_NATIVE)
    {
        if (cr_plugin_load(plugin->nativeContext_, qPrintable(path)))
        {
            plugin->nativeContext_.userdata = context_;
            plugin->fileName_ = path;
            plugins_.push_back(plugin);
            return plugin.Get();
        }
        else
            URHO3D_LOGWARNINGF("Failed loading native plugin \"%s\".", qPrintable(GetFileNameAndExtension(path)));
    }
    else if (plugin->type_ == PLUGIN_MANAGED)
    {
        // TODO
    }

    return nullptr;
}

bool PluginManager::Unload(Plugin* plugin)
{
    if (plugin == nullptr)
        return false;

    auto it = std::find(plugins_.begin(),plugins_.end(),SharedPtr<Plugin>(plugin));
    if (it == plugins_.end())
    {
        URHO3D_LOGERRORF("Plugin %s was never loaded.", qPrintable(plugin->fileName_));
        return false;
    }
    emit getEditorInstance()->EditorUserCodeReloadStart();
    cr_plugin_close(plugin->nativeContext_);
    emit getEditorInstance()->EditorUserCodeReloadEnd();
    URHO3D_LOGINFOF("Plugin %s was unloaded.", qPrintable(plugin->fileName_));
    plugins_.erase(it);

    CleanUp();

    return true;
}

void PluginManager::OnEndFrame()
{
#if LUTEFISK3D_PLUGINS
    for (auto it = plugins_.begin(); it != plugins_.end(); it++)
    {
        Plugin* plugin = it->Get();
        if (plugin->type_ == PLUGIN_NATIVE && plugin->nativeContext_.userdata)
        {
            bool reloading = cr_plugin_changed(plugin->nativeContext_);
            if (reloading)
                emit getEditorInstance()->EditorUserCodeReloadStart();

            if (cr_plugin_update(plugin->nativeContext_) != 0)
            {
                URHO3D_LOGERRORF("Processing plugin \"%s\" failed and it was unloaded.",
                    qPrintable(GetFileNameAndExtension(plugin->fileName_)));
                cr_plugin_close(plugin->nativeContext_);
                plugin->nativeContext_.userdata = nullptr;
                continue;
            }

            if (reloading)
            {
                emit getEditorInstance()->EditorUserCodeReloadEnd();
                if (plugin->nativeContext_.userdata != nullptr)
                {
                    URHO3D_LOGINFOF("Loaded plugin \"%s\" version %d.",
                        qPrintable(GetFileNameAndExtension(plugin->fileName_)), plugin->nativeContext_.version);
                }
            }
        }
    }
#endif
}

void PluginManager::CleanUp(QString directory)
{
    if (directory.isEmpty())
        directory = GetFileSystem()->GetProgramDir();

    if (!GetFileSystem()->DirExists(directory))
        return;

    QStringList files;
    GetFileSystem()->ScanDir(files, directory, "*.*", SCAN_FILES, false);

    for (const QString& file : files)
    {
        bool possiblyPlugin = false;
#if __linux__
        possiblyPlugin |= file.endsWith(".so");
#endif
#if __APPLE__
        possiblyPlugin |= file.endsWith(".dylib");
#endif
        possiblyPlugin |= file.endsWith(".dll");

        if (possiblyPlugin)
        {
            QString name = GetFileName(file);
            if (name.back().isDigit())
                GetFileSystem()->Delete(directory + "/"+file);
        }
    }
}

Plugin* PluginManager::GetPlugin(const QString& fileName)
{
    for (auto it = plugins_.begin(); it != plugins_.end(); it++)
    {
        if (it->Get()->fileName_ == fileName)
            return it->Get();
    }
    return nullptr;
}

}

#endif
