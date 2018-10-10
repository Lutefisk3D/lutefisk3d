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

#pragma once


#if LUTEFISK3D_PLUGINS

#include <cr/cr.h>
#include <Lutefisk3D/Core/Object.h>

namespace Urho3D
{

/// Enumeration describing plugin file path status.
enum PluginType
{
    /// Not a valid plugin.
    PLUGIN_INVALID,
    /// A native plugin.
    PLUGIN_NATIVE,
    /// A managed plugin.
    PLUGIN_MANAGED,
};

class Plugin : public Object
{
    URHO3D_OBJECT(Plugin, Object);
public:
    explicit Plugin(Context* context);

    /// Returns type of the plugin.
    PluginType GetPluginType() const { return type_; }
    /// Returns file name of plugin.
    QString GetFileName() const { return fileName_; }

protected:
    /// Path to plugin dynamic library file.
    QString fileName_;
    /// Type of plugin (invalid/native/managed).
    PluginType type_ = PLUGIN_INVALID;
    /// Context of native plugin. Not initialized for managed plugins.
    cr_plugin nativeContext_{};

    friend class PluginManager;
};

class PluginManager : public Object
{
    URHO3D_OBJECT(PluginManager, Object);
public:
    /// Construct.
    explicit PluginManager(Context* context);
    /// Load a plugin and return true if succeeded.
    virtual Plugin* Load(const QString& path);
    /// Unload a plugin and return true if succeeded.
    virtual bool Unload(Plugin* plugin);
    /// Returns a loaded plugin with specified name.
    Plugin* GetPlugin(const QString& fileName);
    /// Returns a vector containing all loaded plugins.
    const std::vector<SharedPtr<Plugin>>& GetPlugins() const { return plugins_; }

protected:
    /// Tick native plugins.
    void OnEndFrame();
    /// Checks specified file and recognizes it's plugin type.
    PluginType GetPluginType(const QString& path);
    /// Delete temporary files from binary directory.
    void CleanUp(QString directory = QString());

    /// Loaded plugins.
    std::vector<SharedPtr<Plugin>> plugins_;
};

}
#endif
