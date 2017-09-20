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


#include "Lutefisk3D/Scene/Node.h"
#include "Lutefisk3D/Scene/SceneEvents.h"

#include <jlsignal/SignalBase.h>
#include <QtCore/QSet>

namespace Urho3D
{
class JSONFile;
class XMLFile;
class File;
class PackageFile;
class Resource;
class XMLElement;
extern template class SharedPtr<XMLFile>;
extern template class SharedPtr<JSONFile>;
extern template class SharedPtr<PackageFile>;
static const unsigned FIRST_REPLICATED_ID = 0x1;
static const unsigned LAST_REPLICATED_ID = 0xffffff;
static const unsigned FIRST_LOCAL_ID = 0x01000000;
static const unsigned LAST_LOCAL_ID = 0xffffffff;
extern const char* SCENE_CATEGORY;
extern const char* LOGIC_CATEGORY;
extern const char* SUBSYSTEM_CATEGORY;

/// Asynchronous scene loading mode.
enum LoadMode
{
    /// Preload resources used by a scene or object prefab file, but do not load any scene content.
    LOAD_RESOURCES_ONLY = 0,
    /// Load scene content without preloading. Resources will be requested synchronously when encountered.
    LOAD_SCENE,
    /// Default mode: preload resources used by the scene first, then load the scene content.
    LOAD_SCENE_AND_RESOURCES
};


class ScenePrivate;
/// Root scene node, represents the whole scene.
class LUTEFISK3D_EXPORT Scene : public Node, public SingularSceneSignals
{
    URHO3D_OBJECT(Scene,Node)

    using Node::GetComponent;
    using Node::SaveXML;
    using Node::SaveJSON;

public:
    Scene(Context* context);
    virtual ~Scene();
    /// Register object factory. Node must be registered first.
    static void RegisterObject(Context* context);

    /// Load from binary data. Removes all existing child nodes and components first. Return true if successful.
    virtual bool Load(Deserializer& source, bool setInstanceDefault = false) override;
    /// Save to binary data. Return true if successful.
    virtual bool Save(Serializer& dest) const override;
    /// Load from XML data. Removes all existing child nodes and components first. Return true if successful.
    virtual bool LoadXML(const XMLElement& source, bool setInstanceDefault = false) override;
    /// Load from JSON data. Removes all existing child nodes and components first. Return true if successful.
    virtual bool LoadJSON(const JSONValue& source, bool setInstanceDefault = false) override;
    /// Mark for attribute check on the next network update.
    virtual void MarkNetworkUpdate() override;
    /// Add a replication state that is tracking this scene.
    virtual void AddReplicationState(NodeReplicationState* state) override;

    /// Load from an XML file. Return true if successful.
    bool LoadXML(Deserializer& source);
    /// Load from a JSON file. Return true if successful.
    bool LoadJSON(Deserializer& source);
    /// Save to an XML file. Return true if successful.
    bool SaveXML(Serializer& dest, const QString& indentation = "\t") const;
    /// Save to a JSON file. Return true if successful.
    bool SaveJSON(Serializer& dest, const QString& indentation = "\t") const;
    /// Load from a binary file asynchronously. Return true if started successfully. The LOAD_RESOURCES_ONLY mode can also be used to preload resources from object prefab files.
    bool LoadAsync(File* file, LoadMode mode = LOAD_SCENE_AND_RESOURCES);
    /// Load from an XML file asynchronously. Return true if started successfully. The LOAD_RESOURCES_ONLY mode can also be used to preload resources from object prefab files.
    bool LoadAsyncXML(File* file, LoadMode mode = LOAD_SCENE_AND_RESOURCES);
    /// Load from a JSON file asynchronously. Return true if started successfully. The LOAD_RESOURCES_ONLY mode can also be used to preload resources from object prefab files.
    bool LoadAsyncJSON(File* file, LoadMode mode = LOAD_SCENE_AND_RESOURCES);
    /// Stop asynchronous loading.
    void StopAsyncLoading();
    /// Instantiate scene content from binary data. Return root node if successful.
    Node* Instantiate(Deserializer& source, const Vector3& position, const Quaternion& rotation, CreateMode mode = REPLICATED);
    /// Instantiate scene content from XML data. Return root node if successful.
    Node* InstantiateXML(const XMLElement& source, const Vector3& position, const Quaternion& rotation, CreateMode mode = REPLICATED);
    /// Instantiate scene content from XML data. Return root node if successful.
    Node* InstantiateXML(Deserializer& source, const Vector3& position, const Quaternion& rotation, CreateMode mode = REPLICATED);
    /// Instantiate scene content from JSON data. Return root node if successful.
    Node* InstantiateJSON(const JSONValue& source, const Vector3& position, const Quaternion& rotation, CreateMode mode = REPLICATED);
    /// Instantiate scene content from JSON data. Return root node if successful.
    Node* InstantiateJSON(Deserializer& source, const Vector3& position, const Quaternion& rotation, CreateMode mode = REPLICATED);
    /// Clear scene completely of either replicated, local or all nodes and components.
    void Clear(bool clearReplicated = true, bool clearLocal = true);
    /// Enable or disable scene update.
    void SetUpdateEnabled(bool enable);
    /// Set update time scale. 1.0 = real time (default.)
    void SetTimeScale(float scale);
    /// Set elapsed time in seconds. This can be used to prevent inaccuracy in the timer if the scene runs for a long time.
    void SetElapsedTime(float time);
    /// Set network client motion smoothing constant.
    void SetSmoothingConstant(float constant);
    /// Set network client motion smoothing snap threshold.
    void SetSnapThreshold(float threshold);
    /// Set maximum milliseconds per frame to spend on async scene loading.
    void SetAsyncLoadingMs(int ms);
    /// Add a required package file for networking. To be called on the server.
    void AddRequiredPackageFile(PackageFile* package);
    /// Clear required package files.
    void ClearRequiredPackageFiles();
    /// Register a node user variable hash reverse mapping (for editing.)
    void RegisterVar(const QString& name);
    /// Unregister a node user variable hash reverse mapping.
    void UnregisterVar(const QString& name);
    /// Clear all registered node user variable hash reverse mappings.
    void UnregisterAllVars();

    /// Return node from the whole scene by ID, or null if not found.
    Node* GetNode(unsigned id) const;
    /// Return component from the whole scene by ID, or null if not found.
    Component* GetComponent(unsigned id) const;
    /// Get nodes with specific tag from the whole scene, return false if empty.
    bool GetNodesWithTag(std::vector<Node*>& dest, const QString &tag)  const;
    /// Return whether updates are enabled.
    bool IsUpdateEnabled() const { return updateEnabled_; }
    /// Return whether an asynchronous loading operation is in progress.
    bool IsAsyncLoading() const { return asyncLoading_; }
    /// Return asynchronous loading progress between 0.0 and 1.0, or 1.0 if not in progress.
    float GetAsyncProgress() const;
    /// Return the load mode of the current asynchronous loading operation.
    LoadMode GetAsyncLoadMode() const;
    /// Return source file name.
    const QString& GetFileName() const { return fileName_; }
    /// Return source file checksum.
    unsigned GetChecksum() const { return checksum_; }
    /// Return update time scale.
    float GetTimeScale() const { return timeScale_; }
    /// Return elapsed time in seconds.
    float GetElapsedTime() const { return elapsedTime_; }
    /// Return motion smoothing constant.
    float GetSmoothingConstant() const { return smoothingConstant_; }
    /// Return motion smoothing snap threshold.
    float GetSnapThreshold() const { return snapThreshold_; }
    /// Return maximum milliseconds per frame to spend on async loading.
    int GetAsyncLoadingMs() const { return asyncLoadingMs_; }
    /// Return required package files.
    const std::vector<SharedPtr<PackageFile> >& GetRequiredPackageFiles() const { return requiredPackageFiles_; }
    /// Return a node user variable name, or empty if not registered.
    const QString &GetVarName(StringHash hash) const;

    /// Update scene. Called by HandleUpdate.
    void Update(float timeStep);
    /// Begin a threaded update. During threaded update components can choose to delay dirty processing.
    void BeginThreadedUpdate();
    /// End a threaded update. Notify components that marked themselves for delayed dirty processing.
    void EndThreadedUpdate();
    /// Add a component to the delayed dirty notify queue. Is thread-safe.
    void DelayedMarkedDirty(Component* component);
    /// Return threaded update flag.
    bool IsThreadedUpdate() const { return threadedUpdate_; }
    /// Get free node ID, either non-local or local.
    unsigned GetFreeNodeID(CreateMode mode);
    /// Get free component ID, either non-local or local.
    unsigned GetFreeComponentID(CreateMode mode);
    /// Cache node by tag if tag not zero, no checking if already added. Used internaly in Node::AddTag.
    void NodeTagAdded(Node* node, const QString &tag);
    /// Cache node by tag if tag not zero.
    void NodeTagRemoved(Node* node, const QString &tag);
    /// Node added. Assign scene pointer and add to ID map.
    void NodeAdded(Node* node);
    /// Node removed. Remove from ID map.
    void NodeRemoved(Node* node);
    /// Component added. Add to ID map.
    void ComponentAdded(Component* component);
    /// Component removed. Remove from ID map.
    void ComponentRemoved(Component* component);
    /// Set node user variable reverse mappings.
    void SetVarNamesAttr(const QString& value);
    /// Return node user variable reverse mappings.
    QString GetVarNamesAttr() const;
    /// Prepare network update by comparing attributes and marking replication states dirty as necessary.
    void PrepareNetworkUpdate();
    /// Clean up all references to a network connection that is about to be removed.
    void CleanupConnection(Connection* connection);
    /// Mark a node for attribute check on the next network update.
    void MarkNetworkUpdate(Node* node);
    /// Mark a component for attribute check on the next network update.
    void MarkNetworkUpdate(Component* component);
    /// Mark a node dirty in scene replication states. The node does not need to have own replication state yet.
    void MarkReplicationDirty(Node* node);

private:
    std::unique_ptr<ScenePrivate> d;
    /// Handle the logic update event to update the scene, if active.
    void HandleUpdate(float ts);
    /// Handle a background loaded resource completing.
    void HandleResourceBackgroundLoaded(const QString &, bool, Resource *resource);
    /// Update asynchronous loading.
    void UpdateAsyncLoading();
    /// Finish asynchronous loading.
    void FinishAsyncLoading();
    /// Finish loading. Sets the scene filename and checksum.
    void FinishLoading(Deserializer* source);
    /// Finish saving. Sets the scene filename and checksum.
    void FinishSaving(Serializer* dest) const;
    /// Preload resources from a binary scene or object prefab file.
    void PreloadResources(File* file, bool isSceneFile);
    /// Preload resources from an XML scene or object prefab file.
    void PreloadResourcesXML(const XMLElement& element);
    /// Preload resources from a JSON scene or object prefab file.
    void PreloadResourcesJSON(const JSONValue& value);

    /// Source file name.
    mutable QString fileName_;
    /// Required package files for networking.
    std::vector<SharedPtr<PackageFile> > requiredPackageFiles_;
    /// Scene source file checksum.
    mutable unsigned checksum_;
    /// Maximum milliseconds per frame to spend on async scene loading.
    int asyncLoadingMs_;
    /// Scene update time scale.
    float timeScale_;
    /// Elapsed time accumulator.
    float elapsedTime_;
    /// Motion smoothing constant.
    float smoothingConstant_;
    /// Motion smoothing snap threshold.
    float snapThreshold_;
    /// Update enabled flag.
    bool updateEnabled_;
    /// Asynchronous loading flag.
    bool asyncLoading_;
    /// Threaded update flag.
    bool threadedUpdate_;
};


/// Register Scene library objects.
void LUTEFISK3D_EXPORT RegisterSceneLibrary(Context* context);

}
