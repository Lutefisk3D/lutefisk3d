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
#include "Scene.h"

#include "SceneResolver.h"
#include "SceneEvents.h"
#include "SmoothedTransform.h"
#include "SplinePath.h"
#include "UnknownComponent.h"
#include "ValueAnimation.h"
#include "Component.h"
#include "ObjectAnimation.h"
#include "ReplicationState.h"
#include "Lutefisk3D/Core/Mutex.h"
#include "Lutefisk3D/Core/Context.h"
#include "Lutefisk3D/Core/CoreEvents.h"
#include "Lutefisk3D/Core/Profiler.h"
#include "Lutefisk3D/Core/WorkQueue.h"
#include "Lutefisk3D/IO/File.h"
#include "Lutefisk3D/IO/Log.h"
#include "Lutefisk3D/IO/PackageFile.h"
#include "Lutefisk3D/IO/VectorBuffer.h"
#include "Lutefisk3D/Resource/ResourceCache.h"
#include "Lutefisk3D/Resource/ResourceEvents.h"
#include "Lutefisk3D/Resource/XMLElement.h"
#include "Lutefisk3D/Resource/XMLFile.h"
#include "Lutefisk3D/Resource/JSONFile.h"

#if 1
namespace Urho3D
{
namespace  {
/// Asynchronous loading progress of a scene.
struct AsyncProgress
{
    /// File for binary mode.
    SharedPtr<File> file_;
    /// XML file for XML mode.
    SharedPtr<XMLFile> xmlFile_;
    /// JSON file for JSON mode
    SharedPtr<JSONFile> jsonFile_;
    /// Current XML element for XML mode.
    XMLElement xmlElement_;
    /// Current JSON child array and for JSON mode
    unsigned jsonIndex_;
    /// Current load mode.
    LoadMode mode_;
    /// Resource name hashes left to load.
    QSet<StringHash> resources_;
    /// Loaded resources.
    unsigned loadedResources_;
    /// Total resources.
    unsigned totalResources_;
    /// Loaded root-level nodes.
    unsigned loadedNodes_;
    /// Total root-level nodes.
    unsigned totalNodes_;
};
}
class ScenePrivate
{
public:
    /// Replicated scene nodes by ID.
    HashMap<unsigned, Node*> replicatedNodes_;
    /// Local scene nodes by ID.
    HashMap<unsigned, Node*> localNodes_;
    /// Replicated components by ID.
    HashMap<unsigned, Component*> replicatedComponents_;
    /// Local components by ID.
    HashMap<unsigned, Component*> localComponents_;
    /// Cached tagged nodes by tag.
    HashMap<StringHash, std::vector<Node*> > taggedNodes_;
    /// Node and component ID resolver for asynchronous loading.
    SceneResolver resolver_;
    /// Registered node user variable reverse mappings.
    HashMap<StringHash, QString> varNames_;
    /// Nodes to check for attribute changes on the next network update.
    QSet<unsigned> networkUpdateNodes_;
    /// Components to check for attribute changes on the next network update.
    QSet<unsigned> networkUpdateComponents_;
    /// Delayed dirty notification queue for components.
    std::vector<Component*> delayedDirtyComponents_;
    /// Mutex for the delayed dirty notification queue.
    Mutex sceneMutex_;
    /// Next free non-local node ID.
    unsigned replicatedNodeID_=FIRST_REPLICATED_ID;
    /// Next free non-local component ID.
    unsigned replicatedComponentID_=FIRST_REPLICATED_ID;
    /// Next free local node ID.
    unsigned localNodeID_=FIRST_LOCAL_ID;
    /// Next free local component ID.
    unsigned localComponentID_=FIRST_LOCAL_ID;
    /// Asynchronous loading progress.
    AsyncProgress asyncProgress_;
    void stopAsyncLoad()
    {
        asyncProgress_.file_.Reset();
        asyncProgress_.xmlFile_.Reset();
        asyncProgress_.jsonFile_.Reset();
        asyncProgress_.xmlElement_ = XMLElement::EMPTY;
        asyncProgress_.jsonIndex_ = 0;
        asyncProgress_.resources_.clear();
        resolver_.Reset();
    }

};
const char* SCENE_CATEGORY = "Scene";
const char* LOGIC_CATEGORY = "Logic";
const char* SUBSYSTEM_CATEGORY = "Subsystem";

static const float DEFAULT_SMOOTHING_CONSTANT = 50.0f;
static const float DEFAULT_SNAP_THRESHOLD = 5.0f;

Scene::Scene(Context* context) :
    Node(context),
    d(new ScenePrivate),
    checksum_(0),
    asyncLoadingMs_(5),
    timeScale_(1.0f),
    elapsedTime_(0),
    smoothingConstant_(DEFAULT_SMOOTHING_CONSTANT),
    snapThreshold_(DEFAULT_SNAP_THRESHOLD),
    updateEnabled_(true),
    asyncLoading_(false),
    threadedUpdate_(false)
{
    // Assign an ID to self so that nodes can refer to this node as a parent
    SetID(GetFreeNodeID(REPLICATED));
    NodeAdded(this);
    g_coreSignals.update.Connect(this,&Scene::HandleUpdate);
    g_resourceSignals.resourceBackgroundLoaded.Connect(this,&Scene::HandleResourceBackgroundLoaded);
}

Scene::~Scene()
{
    // Remove root-level components first, so that scene subsystems such as the octree destroy themselves. This will speed up
    // the removal of child nodes' components
    RemoveAllComponents();
    RemoveAllChildren();

    // Remove scene reference and owner from all nodes that still exist
    for (auto & elem : d->replicatedNodes_)
        ELEMENT_VALUE(elem)->ResetScene();
    for (auto & elem : d->localNodes_)
        ELEMENT_VALUE(elem)->ResetScene();
}

void Scene::RegisterObject(Context* context)
{
    context->RegisterFactory<Scene>();

    URHO3D_ACCESSOR_ATTRIBUTE("Name", GetName, SetName, QString, QString(), AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("Time Scale", GetTimeScale, SetTimeScale, float, 1.0f, AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("Smoothing Constant", GetSmoothingConstant, SetSmoothingConstant, float, DEFAULT_SMOOTHING_CONSTANT, AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("Snap Threshold", GetSnapThreshold, SetSnapThreshold, float, DEFAULT_SNAP_THRESHOLD, AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("Elapsed Time", GetElapsedTime, SetElapsedTime, float, 0.0f, AM_FILE);
//    URHO3D_ATTRIBUTE("Next Replicated Node ID", unsigned, replicatedNodeID_, FIRST_REPLICATED_ID, AM_FILE | AM_NOEDIT);
//    URHO3D_ATTRIBUTE("Next Replicated Component ID", unsigned, replicatedComponentID_, FIRST_REPLICATED_ID, AM_FILE | AM_NOEDIT);
//    URHO3D_ATTRIBUTE("Next Local Node ID", unsigned, localNodeID_, FIRST_LOCAL_ID, AM_FILE | AM_NOEDIT);
//    URHO3D_ATTRIBUTE("Next Local Component ID", unsigned, localComponentID_, FIRST_LOCAL_ID, AM_FILE | AM_NOEDIT);
    URHO3D_ATTRIBUTE("Variables", VariantMap, vars_, Variant::emptyVariantMap, AM_FILE); // Network replication of vars uses custom data
    URHO3D_MIXED_ACCESSOR_ATTRIBUTE("Variable Names", GetVarNamesAttr, SetVarNamesAttr, QString, QString(), AM_FILE | AM_NOEDIT);
}

bool Scene::Load(Deserializer& source)
{
    URHO3D_PROFILE(LoadScene);

    StopAsyncLoading();

    // Check ID
    if (source.ReadFileID() != "USCN")
    {
        URHO3D_LOGERROR(source.GetName() + " is not a valid scene file");
        return false;
    }

    URHO3D_LOGINFO("Loading scene from " + source.GetName());

    Clear();

    // Load the whole scene, then perform post-load if successfully loaded
    if (Node::Load(source))
    {
        FinishLoading(&source);
        return true;
    }

    return false;
}

bool Scene::Save(Serializer& dest) const
{
    URHO3D_PROFILE(SaveScene);

    // Write ID first
    if (!dest.WriteFileID("USCN"))
    {
        URHO3D_LOGERROR("Could not save scene, writing to stream failed");
        return false;
    }

    Deserializer* ptr = dynamic_cast<Deserializer*>(&dest);
    if (ptr != nullptr)
        URHO3D_LOGINFO("Saving scene to " + ptr->GetName());

    if (Node::Save(dest))
    {
        FinishSaving(&dest);
        return true;
    }

    return false;
}

bool Scene::LoadXML(const XMLElement& source)
{
    URHO3D_PROFILE(LoadSceneXML);

    StopAsyncLoading();

    // Load the whole scene, then perform post-load if successfully loaded
    // Note: the scene filename and checksum can not be set, as we only used an XML element
    if (Node::LoadXML(source))
    {
        FinishLoading(nullptr);
        return true;
    }

    return false;
}

bool Scene::LoadJSON(const JSONValue& source)
{
    URHO3D_PROFILE(LoadSceneJSON);

    StopAsyncLoading();

    // Load the whole scene, then perform post-load if successfully loaded
    // Note: the scene filename and checksum can not be set, as we only used an XML element
    if (Node::LoadJSON(source))
    {
        FinishLoading(nullptr);
        return true;
    }

    return false;
}

void Scene::MarkNetworkUpdate()
{
    if (!networkUpdate_)
    {
        MarkNetworkUpdate(this);
        networkUpdate_ = true;
    }
}

void Scene::AddReplicationState(NodeReplicationState* state)
{
    Node::AddReplicationState(state);

    // This is the first update for a new connection. Mark all replicated nodes dirty
    for (auto i = d->replicatedNodes_.cbegin(); i != d->replicatedNodes_.end(); ++i)
        state->sceneState_->dirtyNodes_.insert(MAP_KEY(i));
}

bool Scene::LoadXML(Deserializer& source)
{
    URHO3D_PROFILE(LoadSceneXML);

    StopAsyncLoading();

    SharedPtr<XMLFile> xml(new XMLFile(context_));
    if (!xml->Load(source))
        return false;

    URHO3D_LOGINFO("Loading scene from " + source.GetName());

    Clear();

    if (Node::LoadXML(xml->GetRoot()))
    {
        FinishLoading(&source);
        return true;
    }

    return false;
}

bool Scene::LoadJSON(Deserializer& source)
{
    URHO3D_PROFILE(LoadSceneJSON);

    StopAsyncLoading();

    SharedPtr<JSONFile> json(new JSONFile(context_));
    if (!json->Load(source))
        return false;

    URHO3D_LOGINFO("Loading scene from " + source.GetName());

    Clear();

    if (Node::LoadJSON(json->GetRoot()))
    {
        FinishLoading(&source);
        return true;
    }

    return false;
}

bool Scene::SaveXML(Serializer& dest, const QString& indentation) const
{
    URHO3D_PROFILE(SaveSceneXML);

    SharedPtr<XMLFile> xml(new XMLFile(context_));
    XMLElement rootElem = xml->CreateRoot("scene");
    if (!SaveXML(rootElem))
        return false;

    Deserializer* ptr = dynamic_cast<Deserializer*>(&dest);
    if (ptr != nullptr)
        URHO3D_LOGINFO("Saving scene to " + ptr->GetName());

    if (xml->Save(dest, indentation))
    {
        FinishSaving(&dest);
        return true;
    }

    return false;
}

bool Scene::SaveJSON(Serializer& dest, const QString& indentation) const
{
    URHO3D_PROFILE(SaveSceneJSON);

    SharedPtr<JSONFile> json(new JSONFile(context_));
    JSONValue rootVal;
    if (!SaveJSON(rootVal))
        return false;

    Deserializer* ptr = dynamic_cast<Deserializer*>(&dest);
    if (ptr != nullptr)
        URHO3D_LOGINFO("Saving scene to " + ptr->GetName());

    json->GetRoot() = rootVal;

    if (json->Save(dest, indentation))
    {
        FinishSaving(&dest);
        return true;
    }

    return false;
}

bool Scene::LoadAsync(File* file, LoadMode mode)
{
    if (file == nullptr)
    {
        URHO3D_LOGERROR("Null file for async loading");
        return false;
    }

    StopAsyncLoading();

    // Check ID
    bool isSceneFile = file->ReadFileID() == "USCN";
    if (!isSceneFile)
    {
        // In resource load mode can load also object prefabs, which have no identifier
        if (mode > LOAD_RESOURCES_ONLY)
        {
            URHO3D_LOGERROR(file->GetName() + " is not a valid scene file");
            return false;
        }

        file->Seek(0);
    }

    if (mode > LOAD_RESOURCES_ONLY)
    {
        URHO3D_LOGINFO("Loading scene from " + file->GetName());
        Clear();
    }

    asyncLoading_ = true;
    d->asyncProgress_.file_ = file;
    d->asyncProgress_.mode_ = mode;
    d->asyncProgress_.loadedNodes_ = d->asyncProgress_.totalNodes_ = d->asyncProgress_.loadedResources_ = d->asyncProgress_.totalResources_ = 0;
    d->asyncProgress_.resources_.clear();

    if (mode > LOAD_RESOURCES_ONLY)
    {
        // Preload resources if appropriate, then return to the original position for loading the scene content
        if (mode != LOAD_SCENE)
        {
            URHO3D_PROFILE(FindResourcesToPreload);

            unsigned currentPos = file->GetPosition();
            PreloadResources(file, isSceneFile);
            file->Seek(currentPos);
        }

        // Store own old ID for resolving possible root node references
        unsigned nodeID = file->ReadUInt();
        d->resolver_.AddNode(nodeID, this);

        // Load root level components first
        if (!Node::Load(*file, d->resolver_, false))
        {
            StopAsyncLoading();
            return false;
        }

        // Then prepare to load child nodes in the async updates
        d->asyncProgress_.totalNodes_ = file->ReadVLE();
    }
    else
    {
        URHO3D_PROFILE(FindResourcesToPreload);

        URHO3D_LOGINFO("Preloading resources from " + file->GetName());
        PreloadResources(file, isSceneFile);
    }

    return true;
}

bool Scene::LoadAsyncXML(File* file, LoadMode mode)
{
    if (file == nullptr)
    {
        URHO3D_LOGERROR("Null file for async loading");
        return false;
    }

    StopAsyncLoading();

    SharedPtr<XMLFile> xml(new XMLFile(context_));
    if (!xml->Load(*file))
        return false;

    if (mode > LOAD_RESOURCES_ONLY)
    {
        URHO3D_LOGINFO("Loading scene from " + file->GetName());
        Clear();
    }

    asyncLoading_ = true;
    d->asyncProgress_.xmlFile_ = xml;
    d->asyncProgress_.file_ = file;
    d->asyncProgress_.mode_ = mode;
    d->asyncProgress_.loadedNodes_ = d->asyncProgress_.totalNodes_ = d->asyncProgress_.loadedResources_ = d->asyncProgress_.totalResources_ = 0;
    d->asyncProgress_.resources_.clear();

    if (mode > LOAD_RESOURCES_ONLY)
    {
        XMLElement rootElement = xml->GetRoot();

        // Preload resources if appropriate
        if (mode != LOAD_SCENE)
        {
            URHO3D_PROFILE(FindResourcesToPreload);

            PreloadResourcesXML(rootElement);
        }

        // Store own old ID for resolving possible root node references
        unsigned nodeID = rootElement.GetUInt("id");
        d->resolver_.AddNode(nodeID, this);

        // Load the root level components first
        if (!Node::LoadXML(rootElement, d->resolver_, false))
            return false;

        // Then prepare for loading all root level child nodes in the async update
        XMLElement childNodeElement = rootElement.GetChild("node");
        d->asyncProgress_.xmlElement_ = childNodeElement;

        // Count the amount of child nodes
        while (childNodeElement)
        {
            ++d->asyncProgress_.totalNodes_;
            childNodeElement = childNodeElement.GetNext("node");
        }
    }
    else
    {
        URHO3D_PROFILE(FindResourcesToPreload);

        URHO3D_LOGINFO("Preloading resources from " + file->GetName());
        PreloadResourcesXML(xml->GetRoot());
    }

    return true;
}

bool Scene::LoadAsyncJSON(File* file, LoadMode mode)
{
    if (file == nullptr)
    {
        URHO3D_LOGERROR("Null file for async loading");
        return false;
    }

    StopAsyncLoading();

    SharedPtr<JSONFile> json(new JSONFile(context_));
    if (!json->Load(*file))
        return false;

    if (mode > LOAD_RESOURCES_ONLY)
    {
        URHO3D_LOGINFO("Loading scene from " + file->GetName());
        Clear();
    }

    asyncLoading_ = true;
    d->asyncProgress_.jsonFile_ = json;
    d->asyncProgress_.file_ = file;
    d->asyncProgress_.mode_ = mode;
    d->asyncProgress_.loadedNodes_ = d->asyncProgress_.totalNodes_ = d->asyncProgress_.loadedResources_ = d->asyncProgress_.totalResources_ = 0;
    d->asyncProgress_.resources_.clear();

    if (mode > LOAD_RESOURCES_ONLY)
    {
        JSONValue rootVal = json->GetRoot();

        // Preload resources if appropriate
        if (mode != LOAD_SCENE)
        {
            URHO3D_PROFILE(FindResourcesToPreload);

            PreloadResourcesJSON(rootVal);
        }

        // Store own old ID for resolving possible root node references
        unsigned nodeID = rootVal.Get("id").GetUInt();
        d->resolver_.AddNode(nodeID, this);

        // Load the root level components first
        if (!Node::LoadJSON(rootVal, d->resolver_, false))
            return false;

        // Then prepare for loading all root level child nodes in the async update
        JSONArray childrenArray = rootVal.Get("children").GetArray();
        d->asyncProgress_.jsonIndex_ = 0;

        // Count the amount of child nodes
        d->asyncProgress_.totalNodes_ = childrenArray.size();
    }
    else
    {
        URHO3D_PROFILE(FindResourcesToPreload);

        URHO3D_LOGINFO("Preloading resources from " + file->GetName());
        PreloadResourcesJSON(json->GetRoot());
    }

    return true;
}

void Scene::StopAsyncLoading()
{
    asyncLoading_ = false;
    d->stopAsyncLoad();
}

Node* Scene::Instantiate(Deserializer& source, const Vector3& position, const Quaternion& rotation, CreateMode mode)
{
    URHO3D_PROFILE(Instantiate);

    SceneResolver resolver;
    unsigned nodeID = source.ReadUInt();
    // Rewrite IDs when instantiating
    Node* node = CreateChild(0, mode);
    resolver.AddNode(nodeID, node);
    if (node->Load(source, resolver, true, true, mode))
    {
        resolver.Resolve();
        node->SetTransform(position, rotation);
        node->ApplyAttributes();
        return node;
    }


    node->Remove();
    return nullptr;

}

Node* Scene::InstantiateXML(const XMLElement& source, const Vector3& position, const Quaternion& rotation, CreateMode mode)
{
    URHO3D_PROFILE(InstantiateXML);

    SceneResolver resolver;
    unsigned nodeID = source.GetUInt("id");
    // Rewrite IDs when instantiating
    Node* node = CreateChild(0, mode);
    resolver.AddNode(nodeID, node);
    if (node->LoadXML(source, resolver, true, true, mode))
    {
        resolver.Resolve();
        node->SetTransform(position, rotation);
        node->ApplyAttributes();
        return node;
    }


    node->Remove();
    return nullptr;

}

Node* Scene::InstantiateJSON(const JSONValue& source, const Vector3& position, const Quaternion& rotation, CreateMode mode)
{
    URHO3D_PROFILE(InstantiateJSON);

    SceneResolver resolver;
    unsigned nodeID = source.Get("id").GetUInt();
    // Rewrite IDs when instantiating
    Node* node = CreateChild(0, mode);
    resolver.AddNode(nodeID, node);
    if (node->LoadJSON(source, resolver, true, true, mode))
    {
        resolver.Resolve();
        node->SetTransform(position, rotation);
        node->ApplyAttributes();
        return node;
    }


    node->Remove();
    return nullptr;

}

Node* Scene::InstantiateXML(Deserializer& source, const Vector3& position, const Quaternion& rotation, CreateMode mode)
{
    SharedPtr<XMLFile> xml(new XMLFile(context_));
    if (!xml->Load(source))
        return nullptr;

    return InstantiateXML(xml->GetRoot(), position, rotation, mode);
}

Node* Scene::InstantiateJSON(Deserializer& source, const Vector3& position, const Quaternion& rotation, CreateMode mode)
{
    SharedPtr<JSONFile> json(new JSONFile(context_));
    if (!json->Load(source))
        return nullptr;

    return InstantiateJSON(json->GetRoot(), position, rotation, mode);
}
void Scene::Clear(bool clearReplicated, bool clearLocal)
{
    StopAsyncLoading();

    RemoveChildren(clearReplicated, clearLocal, true);
    RemoveComponents(clearReplicated, clearLocal);

    // Only clear name etc. if clearing completely
    if (clearReplicated && clearLocal)
    {
        UnregisterAllVars();
        SetName(QString::null);
        fileName_.clear();
        checksum_ = 0;
    }

    // Reset ID generators
    if (clearReplicated)
    {
        d->replicatedNodeID_ = FIRST_REPLICATED_ID;
        d->replicatedComponentID_ = FIRST_REPLICATED_ID;
    }
    if (clearLocal)
    {
        d->localNodeID_ = FIRST_LOCAL_ID;
        d->localComponentID_ = FIRST_LOCAL_ID;
    }
}

void Scene::SetUpdateEnabled(bool enable)
{
    updateEnabled_ = enable;
}

void Scene::SetTimeScale(float scale)
{
    timeScale_ = Max(scale, M_EPSILON);
    Node::MarkNetworkUpdate();
}

void Scene::SetSmoothingConstant(float constant)
{
    smoothingConstant_ = Max(constant, M_EPSILON);
    Node::MarkNetworkUpdate();
}

void Scene::SetSnapThreshold(float threshold)
{
    snapThreshold_ = Max(threshold, 0.0f);
    Node::MarkNetworkUpdate();
}

void Scene::SetAsyncLoadingMs(int ms)
{
    asyncLoadingMs_ = Max(ms, 1);
}

void Scene::SetElapsedTime(float time)
{
    elapsedTime_ = time;
}

void Scene::AddRequiredPackageFile(PackageFile* package)
{
    // Do not add packages that failed to load
    if ((package == nullptr) || (package->GetNumFiles() == 0u))
        return;

    requiredPackageFiles_.push_back(SharedPtr<PackageFile>(package));
}

void Scene::ClearRequiredPackageFiles()
{
    requiredPackageFiles_.clear();
}

void Scene::RegisterVar(const QString& name)
{
    d->varNames_[name] = name;
}

void Scene::UnregisterVar(const QString& name)
{
    d->varNames_.erase(name);
}

void Scene::UnregisterAllVars()
{
    d->varNames_.clear();
}

Node* Scene::GetNode(unsigned id) const
{
    if (id < FIRST_LOCAL_ID)
    {
        HashMap<unsigned, Node*>::const_iterator i = d->replicatedNodes_.find(id);
        return i != d->replicatedNodes_.end() ? MAP_VALUE(i) : nullptr;
    }


    HashMap<unsigned, Node*>::const_iterator i = d->localNodes_.find(id);
    return i != d->localNodes_.end() ? MAP_VALUE(i) : nullptr;

}

bool Scene::GetNodesWithTag(std::vector<Node*>& dest, const QString & tag) const
{
    dest.clear();
    HashMap<StringHash, std::vector<Node*> >::const_iterator it = d->taggedNodes_.find(tag);
    if (it != d->taggedNodes_.end())
    {
        dest = it->second;
        return true;
    }

    return false;
}

Component* Scene::GetComponent(unsigned id) const
{
    if (id < FIRST_LOCAL_ID)
    {
        HashMap<unsigned, Component*>::const_iterator i = d->replicatedComponents_.find(id);
        return i != d->replicatedComponents_.end() ? MAP_VALUE(i) :  nullptr;
    }


    HashMap<unsigned, Component*>::const_iterator i = d->localComponents_.find(id);
    return i != d->localComponents_.end() ? MAP_VALUE(i) :  nullptr;

}

float Scene::GetAsyncProgress() const
{
    return !asyncLoading_ || d->asyncProgress_.totalNodes_ + d->asyncProgress_.totalResources_ == 0
               ? 1.0f
               : (float)(d->asyncProgress_.loadedNodes_ + d->asyncProgress_.loadedResources_) /
                     (float)(d->asyncProgress_.totalNodes_ + d->asyncProgress_.totalResources_);
}

LoadMode Scene::GetAsyncLoadMode() const { return d->asyncProgress_.mode_; }

const QString &Scene::GetVarName(StringHash hash) const
{
    HashMap<StringHash, QString>::const_iterator i = d->varNames_.find(hash);
    return i != d->varNames_.end() ? MAP_VALUE(i) : s_dummy;
}

void Scene::Update(float timeStep)
{
    if (asyncLoading_)
    {
        UpdateAsyncLoading();
        // If only preloading resources, scene update can continue
        if (d->asyncProgress_.mode_ > LOAD_RESOURCES_ONLY)
            return;
    }

    URHO3D_PROFILE(UpdateScene);

    timeStep *= timeScale_;

    // Update variable timestep logic
    g_sceneSignals.sceneUpdate(this,timeStep);

    // Update scene attribute animation.
    attributeAnimationUpdate(this,timeStep);

    // Update scene subsystems. If a physics world is present, it will be updated, triggering fixed timestep logic updates
    sceneSubsystemUpdate(this,timeStep);

    // Update transform smoothing
    {
        URHO3D_PROFILE(UpdateSmoothing);

        float constant = 1.0f - Clamp(powf(2.0f, -timeStep * smoothingConstant_), 0.0f, 1.0f);
        float squaredSnapThreshold = snapThreshold_ * snapThreshold_;

        updateSmoothing(constant,squaredSnapThreshold);
    }

    // Post-update variable timestep logic
    scenePostUpdate(this,timeStep);

    // Note: using a float for elapsed time accumulation is inherently inaccurate. The purpose of this value is
    // primarily to update material animation effects, as it is available to shaders. It can be reset by calling
    // SetElapsedTime()
    elapsedTime_ += timeStep;
}

void Scene::BeginThreadedUpdate()
{
    // Check the work queue subsystem whether it actually has created worker threads. If not, do not enter threaded mode.
    if (context_->m_WorkQueueSystem->GetNumThreads() != 0u)
        threadedUpdate_ = true;
}

void Scene::EndThreadedUpdate()
{
    if (!threadedUpdate_)
        return;

    threadedUpdate_ = false;

    if (!d->delayedDirtyComponents_.empty())
    {
        URHO3D_PROFILE(EndThreadedUpdate);

        for (Component* i : d->delayedDirtyComponents_)
            i->OnMarkedDirty(i->GetNode());
        d->delayedDirtyComponents_.clear();
    }
}

void Scene::DelayedMarkedDirty(Component* component)
{
    MutexLock lock(d->sceneMutex_);
    d->delayedDirtyComponents_.push_back(component);
}

unsigned Scene::GetFreeNodeID(CreateMode mode)
{
    if (mode == REPLICATED)
    {
        for (;;)
        {
            unsigned ret = d->replicatedNodeID_;
            if (d->replicatedNodeID_ < LAST_REPLICATED_ID)
                ++d->replicatedNodeID_;
            else
                d->replicatedNodeID_ = FIRST_REPLICATED_ID;

            if (!hashContains(d->replicatedNodes_,ret))
                return ret;
        }
    }
    else
    {
        for (;;)
        {
            unsigned ret =  d->localNodeID_;
            if (d->localNodeID_ < LAST_LOCAL_ID)
                ++d->localNodeID_;
            else
                d->localNodeID_ = FIRST_LOCAL_ID;

            if (!hashContains(d->localNodes_,ret))
                return ret;
        }
    }
}

unsigned Scene::GetFreeComponentID(CreateMode mode)
{
    if (mode == REPLICATED)
    {
        for (;;)
        {
            unsigned ret = d->replicatedComponentID_;
            if (d->replicatedComponentID_ < LAST_REPLICATED_ID)
                ++d->replicatedComponentID_;
            else
                d->replicatedComponentID_ = FIRST_REPLICATED_ID;

            if (!hashContains(d->replicatedComponents_,ret))
                return ret;
        }
    }
    else
    {
        for (;;)
        {
            unsigned ret =  d->localComponentID_;
            if (d->localComponentID_ < LAST_LOCAL_ID)
                ++d->localComponentID_;
            else
                d->localComponentID_ = FIRST_LOCAL_ID;

            if (!hashContains(d->localComponents_,ret))
                return ret;
        }
    }
}

void Scene::NodeAdded(Node* node)
{
    if ((node == nullptr) || node->GetScene() == this)
        return;

    // Remove from old scene first

    Scene* oldScene = node->GetScene();

    if (oldScene != nullptr)
        oldScene->NodeRemoved(node);

    node->SetScene(this);

    // If the new node has an ID of zero (default), assign a replicated ID now
    unsigned id = node->GetID();
    if (id == 0u)
    {
        id = GetFreeNodeID(REPLICATED);
        node->SetID(id);
    }

    // If node with same ID exists, remove the scene reference from it and overwrite with the new node
    if (id < FIRST_LOCAL_ID)
    {
        HashMap<unsigned, Node*>::iterator i = d->replicatedNodes_.find(id);
        if (i != d->replicatedNodes_.end() && MAP_VALUE(i) != node)
        {
            URHO3D_LOGWARNING("Overwriting node with ID " + QString::number(id));
            NodeRemoved(MAP_VALUE(i));
        }

        d->replicatedNodes_[id] = node;

        MarkNetworkUpdate(node);
        MarkReplicationDirty(node);
    }
    else
    {
        HashMap<unsigned, Node*>::iterator i = d->localNodes_.find(id);
        if (i != d->localNodes_.end() && MAP_VALUE(i) != node)
        {
            URHO3D_LOGWARNING("Overwriting node with ID " + QString::number(id));
            NodeRemoved(MAP_VALUE(i));
        }

        d->localNodes_[id] = node;
    }

    // Cache tag if already tagged.
    if (!node->GetTags().empty())
    {
        const QStringList& tags = node->GetTags();
        for (unsigned i = 0; i < tags.size(); ++i)
            d->taggedNodes_[tags[i]].push_back(node);
    }
    // Add already created components and child nodes now
    const std::vector<SharedPtr<Component> > &components = node->GetComponents();
    for(const SharedPtr<Component> & p : components)
        ComponentAdded(p.Get());
    const std::vector<SharedPtr<Node> > &children = node->GetChildren();
    for(const SharedPtr<Node> & n : children)
        NodeAdded(n.Get());
}

void Scene::NodeTagAdded(Node* node, const QString & tag)
{
    d->taggedNodes_[tag].push_back(node);
}

void Scene::NodeTagRemoved(Node* node, const QString & tag)
{
    std::vector<Node*> &nodes_with_tag(d->taggedNodes_[tag]);
    auto it = std::find(nodes_with_tag.begin(),nodes_with_tag.end(),node);
    assert(it!=nodes_with_tag.end());
    nodes_with_tag.erase(it);
}

void Scene::NodeRemoved(Node* node)
{
    if ((node == nullptr) || node->GetScene() != this)
        return;

    unsigned id = node->GetID();
    if (id < FIRST_LOCAL_ID)
    {
        d->replicatedNodes_.erase(id);
        MarkReplicationDirty(node);
    }
    else
        d->localNodes_.erase(id);

    node->ResetScene();

    // Remove node from tag cache
    if (!node->GetTags().empty())
    {
        for (const QString &tag : node->GetTags()) {
            NodeTagRemoved(node,tag);
        }
    }
    // Remove components and child nodes as well
    const std::vector<SharedPtr<Component> >& components = node->GetComponents();
    for (const SharedPtr<Component> &i : components)
        ComponentRemoved(i.Get());
    const std::vector<SharedPtr<Node> >& children = node->GetChildren();
    for (const SharedPtr<Node> &i : children)
        NodeRemoved(i.Get());
}

void Scene::ComponentAdded(Component* component)
{
    if (component == nullptr)
        return;

    unsigned id = component->GetID();

    // If the new component has an ID of zero (default), assign a replicated ID now
    if(id == 0u)
    {
        id = GetFreeComponentID(REPLICATED);
        component->SetID(id);
    }

    if (id < FIRST_LOCAL_ID)
    {
        HashMap<unsigned, Component*>::iterator i = d->replicatedComponents_.find(id);
        if (i != d->replicatedComponents_.end() && MAP_VALUE(i) != component)
        {
            URHO3D_LOGWARNING("Overwriting component with ID " + QString::number(id));
            ComponentRemoved(MAP_VALUE(i));
        }

        d->replicatedComponents_[id] = component;
    }
    else
    {
        HashMap<unsigned, Component*>::iterator i = d->localComponents_.find(id);
        if (i != d->localComponents_.end() && MAP_VALUE(i) != component)
        {
            URHO3D_LOGWARNING("Overwriting component with ID " + QString::number(id));
            ComponentRemoved(MAP_VALUE(i));
        }

        d->localComponents_[id] = component;
    }

    component->OnSceneSet(this);
}

void Scene::ComponentRemoved(Component* component)
{
    if (component == nullptr)
        return;

    unsigned id = component->GetID();
    if (id < FIRST_LOCAL_ID)
        d->replicatedComponents_.erase(id);
    else
        d->localComponents_.erase(id);

    component->SetID(0);
    component->OnSceneSet(nullptr);
}

void Scene::SetVarNamesAttr(const QString& value)
{
    QStringList varNames = value.split(';');

    d->varNames_.clear();
    for (QStringList::const_iterator i = varNames.begin(); i != varNames.end(); ++i)
        d->varNames_[*i] = *i;
}

QString Scene::GetVarNamesAttr() const
{
    QString ret;

    if (!d->varNames_.empty())
    {
        for (const auto & elem : d->varNames_)
            ret += ELEMENT_VALUE(elem) + ';';

        ret.resize(ret.length() - 1);
    }

    return ret;
}

void Scene::PrepareNetworkUpdate()
{
    for (unsigned node_id : d->networkUpdateNodes_)
    {
        Node* node = GetNode(node_id);
        if (node != nullptr)
            node->PrepareNetworkUpdate();
    }

    for (unsigned component_id : d->networkUpdateComponents_)
    {
        Component* component = GetComponent(component_id);
        if (component != nullptr)
            component->PrepareNetworkUpdate();
    }

    d->networkUpdateNodes_.clear();
    d->networkUpdateComponents_.clear();
}

void Scene::CleanupConnection(Connection* connection)
{
    Node::CleanupConnection(connection);

    for (auto & elem : d->replicatedNodes_)
        ELEMENT_VALUE(elem)->CleanupConnection(connection);

    for (auto & elem : d->replicatedComponents_)
        ELEMENT_VALUE(elem)->CleanupConnection(connection);
}

void Scene::MarkNetworkUpdate(Node* node)
{
    if (node != nullptr)
    {
        if (!threadedUpdate_)
            d->networkUpdateNodes_.insert(node->GetID());
        else
        {
            MutexLock lock(d->sceneMutex_);
            d->networkUpdateNodes_.insert(node->GetID());
        }
    }
}

void Scene::MarkNetworkUpdate(Component* component)
{
    if (component != nullptr)
    {
        if (!threadedUpdate_)
            d->networkUpdateComponents_.insert(component->GetID());
        else
        {
            MutexLock lock(d->sceneMutex_);
            d->networkUpdateComponents_.insert(component->GetID());
        }
    }
}

void Scene::MarkReplicationDirty(Node* node)
{
    unsigned id = node->GetID();

    if (id < FIRST_LOCAL_ID && (networkState_ != nullptr))
    {
        for (ReplicationState* elem : networkState_->replicationStates_)
        {
            NodeReplicationState* nodeState = static_cast<NodeReplicationState*>(elem);
            nodeState->sceneState_->dirtyNodes_.insert(id);
        }
    }
}

void Scene::HandleUpdate(float ts)
{
    if (!updateEnabled_)
        return;
    Update(ts);
}

void Scene::HandleResourceBackgroundLoaded(const QString &,bool,Resource *resource)
{
    if (asyncLoading_)
    {
        if (d->asyncProgress_.resources_.contains(resource->GetNameHash()))
        {
            d->asyncProgress_.resources_.remove(resource->GetNameHash());
            ++d->asyncProgress_.loadedResources_;
        }
    }
}

void Scene::UpdateAsyncLoading()
{
    URHO3D_PROFILE(UpdateAsyncLoading);

    // If resources left to load, do not load nodes yet
    if (d->asyncProgress_.loadedResources_ < d->asyncProgress_.totalResources_)
        return;

    HiresTimer asyncLoadTimer;

    for (;;)
    {
        if (d->asyncProgress_.loadedNodes_ >= d->asyncProgress_.totalNodes_)
        {
            FinishAsyncLoading();
            return;
        }

        // Read one child node with its full sub-hierarchy either from binary, JSON, or XML
        /// \todo Works poorly in scenes where one root-level child node contains all content
        if (d->asyncProgress_.xmlFile_ != nullptr)
        {
            unsigned nodeID = d->asyncProgress_.xmlElement_.GetUInt("id");
            Node* newNode = CreateChild(nodeID, nodeID < FIRST_LOCAL_ID ? REPLICATED : LOCAL);
            d->resolver_.AddNode(nodeID, newNode);
            newNode->LoadXML(d->asyncProgress_.xmlElement_, d->resolver_);
            d->asyncProgress_.xmlElement_ = d->asyncProgress_.xmlElement_.GetNext("node");
        }
        else if (d->asyncProgress_.jsonFile_ != nullptr) // Load from JSON
        {
            const JSONValue& childValue = d->asyncProgress_.jsonFile_->GetRoot().Get("children").GetArray().at(d->asyncProgress_.jsonIndex_);

            unsigned nodeID =childValue.Get("id").GetUInt();
            Node* newNode = CreateChild(nodeID, nodeID < FIRST_LOCAL_ID ? REPLICATED : LOCAL);
            d->resolver_.AddNode(nodeID, newNode);
            newNode->LoadJSON(childValue, d->resolver_);
            ++d->asyncProgress_.jsonIndex_;
        }
        else // Load from binary
        {
            unsigned nodeID = d->asyncProgress_.file_->ReadUInt();
            Node* newNode = CreateChild(nodeID, nodeID < FIRST_LOCAL_ID ? REPLICATED : LOCAL);
            d->resolver_.AddNode(nodeID, newNode);
            newNode->Load(*d->asyncProgress_.file_, d->resolver_);
        }

        ++d->asyncProgress_.loadedNodes_;

        // Break if time limit exceeded, so that we keep sufficient FPS
        if (asyncLoadTimer.GetUSecS() >= asyncLoadingMs_ * 1000)
            break;
    }
    asyncLoadProgress(this, GetAsyncProgress(), d->asyncProgress_.loadedNodes_, d->asyncProgress_.totalNodes_,
                           d->asyncProgress_.loadedResources_, d->asyncProgress_.totalResources_);
}

void Scene::FinishAsyncLoading()
{
    if (d->asyncProgress_.mode_ > LOAD_RESOURCES_ONLY)
    {
        d->resolver_.Resolve();
        ApplyAttributes();
        FinishLoading(d->asyncProgress_.file_);
    }

    StopAsyncLoading();
    asyncLoadFinished(this);
}

void Scene::FinishLoading(Deserializer* source)
{
    if (source != nullptr)
    {
        fileName_ = source->GetName();
        checksum_ = source->GetChecksum();
    }
}

void Scene::FinishSaving(Serializer* dest) const
{
    Deserializer* ptr = dynamic_cast<Deserializer*>(dest);
    if (ptr != nullptr)
    {
        fileName_ = ptr->GetName();
        checksum_ = ptr->GetChecksum();
    }
}

void Scene::PreloadResources(File* file, bool isSceneFile)
{
    ResourceCache* cache = context_->resourceCache();

    // Read node ID (not needed)
    /*unsigned nodeID = */file->ReadUInt();

    // Read Node or Scene attributes; these do not include any resources
    const std::vector<AttributeInfo>* attributes = context_->GetAttributes(isSceneFile ? Scene::GetTypeStatic() : Node::GetTypeStatic());
    assert(attributes);

    for (const AttributeInfo& attr : *attributes)
    {
        if ((attr.mode_ & AM_FILE) == 0u)
            continue;
        /*Variant varValue = */file->ReadVariant(attr.type_);
    }

    // Read component attributes
    unsigned numComponents = file->ReadVLE();
    for (unsigned i = 0; i < numComponents; ++i)
    {
        VectorBuffer compBuffer(*file, file->ReadVLE());
        StringHash compType = compBuffer.ReadStringHash();
        // Read component ID (not needed)
        /*unsigned compID = */compBuffer.ReadUInt();

        attributes = context_->GetAttributes(compType);
        if (attributes != nullptr)
        {
            for (unsigned j = 0; j < attributes->size(); ++j)
            {
                const AttributeInfo& attr = attributes->at(j);
                if ((attr.mode_ & AM_FILE) == 0u)
                    continue;
                Variant varValue = compBuffer.ReadVariant(attr.type_);
                if (attr.type_ == VAR_RESOURCEREF)
                {
                    const ResourceRef& ref = varValue.GetResourceRef();
                    // Sanitate resource name beforehand so that when we get the background load event, the name matches exactly
                    QString name = cache->SanitateResourceName(ref.name_);
                    bool success = cache->BackgroundLoadResource(ref.type_, name);
                    if (success)
                    {
                        ++d->asyncProgress_.totalResources_;
                        d->asyncProgress_.resources_.insert(StringHash(name));
                    }
                }
                else if (attr.type_ == VAR_RESOURCEREFLIST)
                {
                    const ResourceRefList& refList = varValue.GetResourceRefList();
                    for (unsigned k = 0; k < refList.names_.size(); ++k)
                    {
                        QString name = cache->SanitateResourceName(refList.names_[k]);
                        bool success = cache->BackgroundLoadResource(refList.type_, name);
                        if (success)
                        {
                            ++d->asyncProgress_.totalResources_;
                            d->asyncProgress_.resources_.insert(StringHash(name));
                        }
                    }
                }
            }
        }
    }

    // Read child nodes
    unsigned numChildren = file->ReadVLE();
    for (unsigned i = 0; i < numChildren; ++i)
        PreloadResources(file, false);
}

void Scene::PreloadResourcesXML(const XMLElement& element)
{
    ResourceCache* cache = context_->resourceCache();

    // Node or Scene attributes do not include any resources; therefore skip to the components
    XMLElement compElem = element.GetChild("component");
    while (compElem)
    {
        QString typeName = compElem.GetAttribute("type");
        const std::vector<AttributeInfo>* attributes = context_->GetAttributes(StringHash(typeName));
        if (attributes)
        {
            XMLElement attrElem = compElem.GetChild("attribute");
            unsigned startIndex = 0;

            while (attrElem)
            {
                QString name = attrElem.GetAttribute("name");
                unsigned i = startIndex;
                unsigned attempts = attributes->size();

                while (attempts != 0u)
                {
                    const AttributeInfo& attr = attributes->at(i);
                    if (((attr.mode_ & AM_FILE) != 0u) && (attr.name_.compare(name) == 0))
                    {
                        if (attr.type_ == VAR_RESOURCEREF)
                        {
                            ResourceRef ref = attrElem.GetVariantValue(attr.type_).GetResourceRef();
                            QString name = cache->SanitateResourceName(ref.name_);
                            bool success = cache->BackgroundLoadResource(ref.type_, name);
                            if (success)
                            {
                                ++d->asyncProgress_.totalResources_;
                                d->asyncProgress_.resources_.insert(StringHash(name));
                            }
                        }
                        else if (attr.type_ == VAR_RESOURCEREFLIST)
                        {
                            ResourceRefList refList = attrElem.GetVariantValue(attr.type_).GetResourceRefList();
                            for (unsigned k = 0; k < refList.names_.size(); ++k)
                            {
                                QString name = cache->SanitateResourceName(refList.names_[k]);
                                bool success = cache->BackgroundLoadResource(refList.type_, name);
                                if (success)
                                {
                                    ++d->asyncProgress_.totalResources_;
                                    d->asyncProgress_.resources_.insert(StringHash(name));
                                }
                            }
                        }

                        startIndex = (i + 1) % attributes->size();
                        break;
                    }
                    else
                    {
                        i = (i + 1) % attributes->size();
                        --attempts;
                    }
                }

                attrElem = attrElem.GetNext("attribute");
            }
        }

        compElem = compElem.GetNext("component");
    }

    XMLElement childElem = element.GetChild("node");
    while (childElem)
    {
        PreloadResourcesXML(childElem);
        childElem = childElem.GetNext("node");
    }
}

void Scene::PreloadResourcesJSON(const JSONValue& value)
{
    // If not threaded, can not background load resources, so rather load synchronously later when needed
    ResourceCache* cache = context_->resourceCache();

    // Node or Scene attributes do not include any resources; therefore skip to the components
    JSONArray componentArray = value.Get("components").GetArray();

    for (unsigned i = 0; i < componentArray.size(); i++)
    {
        const JSONValue& compValue = componentArray.at(i);
        QString typeName = compValue.Get("type").GetString();

        const std::vector<AttributeInfo>* attributes = context_->GetAttributes(StringHash(typeName));
        if (attributes != nullptr)
        {
            JSONArray attributesArray = compValue.Get("attributes").GetArray();

            unsigned startIndex = 0;

            for (unsigned j = 0; j < attributesArray.size(); j++)
            {
                const JSONValue& attrVal = attributesArray.at(j);
                QString name = attrVal.Get("name").GetString();
                unsigned i = startIndex;
                unsigned attempts = attributes->size();

                while (attempts != 0u)
                {
                    const AttributeInfo& attr = attributes->at(i);
                    if (((attr.mode_ & AM_FILE) != 0u) && (attr.name_.compare(name) == 0))
                    {
                        if (attr.type_ == VAR_RESOURCEREF)
                        {
                            ResourceRef ref = attrVal.Get("value").GetVariantValue(attr.type_).GetResourceRef();
                            QString name = cache->SanitateResourceName(ref.name_);
                            bool success = cache->BackgroundLoadResource(ref.type_, name);
                            if (success)
                            {
                                ++d->asyncProgress_.totalResources_;
                                d->asyncProgress_.resources_.insert(StringHash(name));
                            }
                        }
                        else if (attr.type_ == VAR_RESOURCEREFLIST)
                        {
                            ResourceRefList refList = attrVal.Get("value").GetVariantValue(attr.type_).GetResourceRefList();
                            for (unsigned k = 0; k < refList.names_.size(); ++k)
                            {
                                QString name = cache->SanitateResourceName(refList.names_[k]);
                                bool success = cache->BackgroundLoadResource(refList.type_, name);
                                if (success)
                                {
                                    ++d->asyncProgress_.totalResources_;
                                    d->asyncProgress_.resources_.insert(StringHash(name));
                                }
                            }
                        }

                        startIndex = (i + 1) % attributes->size();
                        break;
                    }
                    else
                    {
                        i = (i + 1) % attributes->size();
                        --attempts;
                    }
                }

            }
        }

    }

    JSONArray childrenArray = value.Get("children").GetArray();
    for (unsigned i = 0; i < childrenArray.size(); i++)
    {
        const JSONValue& childVal = childrenArray.at(i);
        PreloadResourcesJSON(childVal);
    }
}

void RegisterSceneLibrary(Context* context)
{
    ValueAnimation::RegisterObject(context);
    ObjectAnimation::RegisterObject(context);
    Node::RegisterObject(context);
    Scene::RegisterObject(context);
    SmoothedTransform::RegisterObject(context);
    UnknownComponent::RegisterObject(context);
    SplinePath::RegisterObject(context);
}

}
#endif
