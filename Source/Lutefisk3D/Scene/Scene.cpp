//
// Copyright (c) 2008-2015 the Urho3D project.
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

#include "../Scene/Component.h"
#include "../Core/Context.h"
#include "../Core/CoreEvents.h"
#include "../IO/File.h"
#include "../IO/Log.h"
#include "../Scene/ObjectAnimation.h"
#include "../IO/PackageFile.h"
#include "../Core/Profiler.h"
#include "../Scene/ReplicationState.h"
#include "../Resource/ResourceCache.h"
#include "../Resource/ResourceEvents.h"
#include "../Scene/Scene.h"
#include "../Scene/SceneEvents.h"
#include "../Scene/SmoothedTransform.h"
#include "../Scene/SplinePath.h"
#include "../Scene/UnknownComponent.h"
#include "../Scene/ValueAnimation.h"
#include "../Core/WorkQueue.h"
#include "../Resource/XMLFile.h"

namespace Urho3D
{

const char* SCENE_CATEGORY = "Scene";
const char* LOGIC_CATEGORY = "Logic";
const char* SUBSYSTEM_CATEGORY = "Subsystem";

static const float DEFAULT_SMOOTHING_CONSTANT = 50.0f;
static const float DEFAULT_SNAP_THRESHOLD = 5.0f;

Scene::Scene(Context* context) :
    Node(context),
    replicatedNodeID_(FIRST_REPLICATED_ID),
    replicatedComponentID_(FIRST_REPLICATED_ID),
    localNodeID_(FIRST_LOCAL_ID),
    localComponentID_(FIRST_LOCAL_ID),
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

    SubscribeToEvent(E_UPDATE, HANDLER(Scene, HandleUpdate));
    SubscribeToEvent(E_RESOURCEBACKGROUNDLOADED, HANDLER(Scene, HandleResourceBackgroundLoaded));
}

Scene::~Scene()
{
    // Remove root-level components first, so that scene subsystems such as the octree destroy themselves. This will speed up
    // the removal of child nodes' components
    RemoveAllComponents();
    RemoveAllChildren();

    // Remove scene reference and owner from all nodes that still exist
    for (auto & elem : replicatedNodes_)
        ELEMENT_VALUE(elem)->ResetScene();
    for (auto & elem : localNodes_)
        ELEMENT_VALUE(elem)->ResetScene();
}

void Scene::RegisterObject(Context* context)
{
    context->RegisterFactory<Scene>();

    ACCESSOR_ATTRIBUTE("Name", GetName, SetName, QString, QString(), AM_DEFAULT);
    ACCESSOR_ATTRIBUTE("Time Scale", GetTimeScale, SetTimeScale, float, 1.0f, AM_DEFAULT);
    ACCESSOR_ATTRIBUTE("Smoothing Constant", GetSmoothingConstant, SetSmoothingConstant, float, DEFAULT_SMOOTHING_CONSTANT, AM_DEFAULT);
    ACCESSOR_ATTRIBUTE("Snap Threshold", GetSnapThreshold, SetSnapThreshold, float, DEFAULT_SNAP_THRESHOLD, AM_DEFAULT);
    ACCESSOR_ATTRIBUTE("Elapsed Time", GetElapsedTime, SetElapsedTime, float, 0.0f, AM_FILE);
    ATTRIBUTE("Next Replicated Node ID", int, replicatedNodeID_, FIRST_REPLICATED_ID, AM_FILE | AM_NOEDIT);
    ATTRIBUTE("Next Replicated Component ID", int, replicatedComponentID_, FIRST_REPLICATED_ID, AM_FILE | AM_NOEDIT);
    ATTRIBUTE("Next Local Node ID", int, localNodeID_, FIRST_LOCAL_ID, AM_FILE | AM_NOEDIT);
    ATTRIBUTE("Next Local Component ID", int, localComponentID_, FIRST_LOCAL_ID, AM_FILE | AM_NOEDIT);
    ATTRIBUTE("Variables", VariantMap, vars_, Variant::emptyVariantMap, AM_FILE); // Network replication of vars uses custom data
    MIXED_ACCESSOR_ATTRIBUTE("Variable Names", GetVarNamesAttr, SetVarNamesAttr, QString, QString(), AM_FILE | AM_NOEDIT);
}

bool Scene::Load(Deserializer& source, bool setInstanceDefault)
{
    PROFILE(LoadScene);

    StopAsyncLoading();

    // Check ID
    if (source.ReadFileID() != "USCN")
    {
        LOGERROR(source.GetName() + " is not a valid scene file");
        return false;
    }

    LOGINFO("Loading scene from " + source.GetName());

    Clear();

    // Load the whole scene, then perform post-load if successfully loaded
    if (Node::Load(source, setInstanceDefault))
    {
        FinishLoading(&source);
        return true;
    }
    else
        return false;
}

bool Scene::Save(Serializer& dest) const
{
    PROFILE(SaveScene);

    // Write ID first
    if (!dest.WriteFileID("USCN"))
    {
        LOGERROR("Could not save scene, writing to stream failed");
        return false;
    }

    Deserializer* ptr = dynamic_cast<Deserializer*>(&dest);
    if (ptr)
        LOGINFO("Saving scene to " + ptr->GetName());

    if (Node::Save(dest))
    {
        FinishSaving(&dest);
        return true;
    }
    else
        return false;
}

bool Scene::LoadXML(const XMLElement& source, bool setInstanceDefault)
{
    PROFILE(LoadSceneXML);

    StopAsyncLoading();

    // Load the whole scene, then perform post-load if successfully loaded
    // Note: the scene filename and checksum can not be set, as we only used an XML element
    if (Node::LoadXML(source, setInstanceDefault))
    {
        FinishLoading(nullptr);
        return true;
    }
    else
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
    for (HashMap<unsigned, Node*>::const_iterator i = replicatedNodes_.begin(); i != replicatedNodes_.end(); ++i)
        state->sceneState_->dirtyNodes_.insert(MAP_KEY(i));
}

bool Scene::LoadXML(Deserializer& source)
{
    PROFILE(LoadSceneXML);

    StopAsyncLoading();

    SharedPtr<XMLFile> xml(new XMLFile(context_));
    if (!xml->Load(source))
        return false;

    LOGINFO("Loading scene from " + source.GetName());

    Clear();

    if (Node::LoadXML(xml->GetRoot()))
    {
        FinishLoading(&source);
        return true;
    }
    else
        return false;
}

bool Scene::SaveXML(Serializer& dest, const QString& indentation) const
{
    PROFILE(SaveSceneXML);

    SharedPtr<XMLFile> xml(new XMLFile(context_));
    XMLElement rootElem = xml->CreateRoot("scene");
    if (!SaveXML(rootElem))
        return false;

    Deserializer* ptr = dynamic_cast<Deserializer*>(&dest);
    if (ptr)
        LOGINFO("Saving scene to " + ptr->GetName());

    if (xml->Save(dest, indentation))
    {
        FinishSaving(&dest);
        return true;
    }
    else
        return false;
}

bool Scene::LoadAsync(File* file, LoadMode mode)
{
    if (!file)
    {
        LOGERROR("Null file for async loading");
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
            LOGERROR(file->GetName() + " is not a valid scene file");
            return false;
        }
        else
            file->Seek(0);
    }

    if (mode > LOAD_RESOURCES_ONLY)
    {
        LOGINFO("Loading scene from " + file->GetName());
        Clear();
    }

    asyncLoading_ = true;
    asyncProgress_.file_ = file;
    asyncProgress_.mode_ = mode;
    asyncProgress_.loadedNodes_ = asyncProgress_.totalNodes_ = asyncProgress_.loadedResources_ = asyncProgress_.totalResources_ = 0;
    asyncProgress_.resources_.clear();

    if (mode > LOAD_RESOURCES_ONLY)
    {
        // Preload resources if appropriate, then return to the original position for loading the scene content
        if (mode != LOAD_SCENE)
        {
            PROFILE(FindResourcesToPreload);

            unsigned currentPos = file->GetPosition();
            PreloadResources(file, isSceneFile);
            file->Seek(currentPos);
        }

        // Store own old ID for resolving possible root node references
        unsigned nodeID = file->ReadUInt();
        resolver_.AddNode(nodeID, this);

        // Load root level components first
        if (!Node::Load(*file, resolver_, false))
        {
            StopAsyncLoading();
            return false;
        }

        // Then prepare to load child nodes in the async updates
        asyncProgress_.totalNodes_ = file->ReadVLE();
    }
    else
    {
        PROFILE(FindResourcesToPreload);

        LOGINFO("Preloading resources from " + file->GetName());
        PreloadResources(file, isSceneFile);
    }

    return true;
}

bool Scene::LoadAsyncXML(File* file, LoadMode mode)
{
    if (!file)
    {
        LOGERROR("Null file for async loading");
        return false;
    }

    StopAsyncLoading();

    SharedPtr<XMLFile> xml(new XMLFile(context_));
    if (!xml->Load(*file))
        return false;

    if (mode > LOAD_RESOURCES_ONLY)
    {
        LOGINFO("Loading scene from " + file->GetName());
        Clear();
    }

    asyncLoading_ = true;
    asyncProgress_.xmlFile_ = xml;
    asyncProgress_.file_ = file;
    asyncProgress_.mode_ = mode;
    asyncProgress_.loadedNodes_ = asyncProgress_.totalNodes_ = asyncProgress_.loadedResources_ = asyncProgress_.totalResources_ = 0;
    asyncProgress_.resources_.clear();

    if (mode > LOAD_RESOURCES_ONLY)
    {
        XMLElement rootElement = xml->GetRoot();

        // Preload resources if appropriate
        if (mode != LOAD_SCENE)
        {
            PROFILE(FindResourcesToPreload);

            PreloadResourcesXML(rootElement);
        }

        // Store own old ID for resolving possible root node references
        unsigned nodeID = rootElement.GetInt("id");
        resolver_.AddNode(nodeID, this);

        // Load the root level components first
        if (!Node::LoadXML(rootElement, resolver_, false))
            return false;

        // Then prepare for loading all root level child nodes in the async update
        XMLElement childNodeElement = rootElement.GetChild("node");
        asyncProgress_.xmlElement_ = childNodeElement;

        // Count the amount of child nodes
        while (childNodeElement)
        {
            ++asyncProgress_.totalNodes_;
            childNodeElement = childNodeElement.GetNext("node");
        }
    }
    else
    {
        PROFILE(FindResourcesToPreload);

        LOGINFO("Preloading resources from " + file->GetName());
        PreloadResourcesXML(xml->GetRoot());
    }

    return true;
}

void Scene::StopAsyncLoading()
{
    asyncLoading_ = false;
    asyncProgress_.file_.Reset();
    asyncProgress_.xmlFile_.Reset();
    asyncProgress_.xmlElement_ = XMLElement::EMPTY;
    asyncProgress_.resources_.clear();
    resolver_.Reset();
}

Node* Scene::Instantiate(Deserializer& source, const Vector3& position, const Quaternion& rotation, CreateMode mode)
{
    PROFILE(Instantiate);

    SceneResolver resolver;
    unsigned nodeID = source.ReadInt();
    // Rewrite IDs when instantiating
    Node* node = CreateChild(0, mode);
    resolver.AddNode(nodeID, node);
    if (node->Load(source, resolver, true, true, mode))
    {
        resolver.Resolve();
        node->ApplyAttributes();
        node->SetTransform(position, rotation);
        return node;
    }
    else
    {
        node->Remove();
        return nullptr;
    }
}

Node* Scene::InstantiateXML(const XMLElement& source, const Vector3& position, const Quaternion& rotation, CreateMode mode)
{
    PROFILE(InstantiateXML);

    SceneResolver resolver;
    unsigned nodeID = source.GetInt("id");
    // Rewrite IDs when instantiating
    Node* node = CreateChild(0, mode);
    resolver.AddNode(nodeID, node);
    if (node->LoadXML(source, resolver, true, true, mode))
    {
        resolver.Resolve();
        node->ApplyAttributes();
        node->SetTransform(position, rotation);
        return node;
    }
    else
    {
        node->Remove();
        return nullptr;
    }
}

Node* Scene::InstantiateXML(Deserializer& source, const Vector3& position, const Quaternion& rotation, CreateMode mode)
{
    SharedPtr<XMLFile> xml(new XMLFile(context_));
    if (!xml->Load(source))
        return nullptr;

    return InstantiateXML(xml->GetRoot(), position, rotation, mode);
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
        replicatedNodeID_ = FIRST_REPLICATED_ID;
        replicatedComponentID_ = FIRST_REPLICATED_ID;
    }
    if (clearLocal)
    {
        localNodeID_ = FIRST_LOCAL_ID;
        localComponentID_ = FIRST_LOCAL_ID;
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
    if (!package || !package->GetNumFiles())
        return;

    requiredPackageFiles_.push_back(SharedPtr<PackageFile>(package));
}

void Scene::ClearRequiredPackageFiles()
{
    requiredPackageFiles_.clear();
}

void Scene::RegisterVar(const QString& name)
{
    varNames_[name] = name;
}

void Scene::UnregisterVar(const QString& name)
{
    varNames_.remove(name);
}

void Scene::UnregisterAllVars()
{
    varNames_.clear();
}

Node* Scene::GetNode(unsigned id) const
{
    if (id < FIRST_LOCAL_ID)
    {
        HashMap<unsigned, Node*>::const_iterator i = replicatedNodes_.find(id);
        if (i != replicatedNodes_.end())
            return MAP_VALUE(i);
        return nullptr;
    }
    else
    {
        HashMap<unsigned, Node*>::const_iterator i = localNodes_.find(id);
        if (i != localNodes_.end())
            return MAP_VALUE(i);
        return nullptr;
    }
}

Component* Scene::GetComponent(unsigned id) const
{
    if (id < FIRST_LOCAL_ID)
    {
        HashMap<unsigned, Component*>::const_iterator i = replicatedComponents_.find(id);
        if (i != replicatedComponents_.end())
            return MAP_VALUE(i);
    }
    else
    {
        HashMap<unsigned, Component*>::const_iterator i = localComponents_.find(id);
        if (i != localComponents_.end())
            return MAP_VALUE(i);
    }
    return nullptr;
}

float Scene::GetAsyncProgress() const
{
    if (!asyncLoading_ || asyncProgress_.totalNodes_ + asyncProgress_.totalResources_ == 0)
        return 1.0f;
    else
    {
        return (float)(asyncProgress_.loadedNodes_ + asyncProgress_.loadedResources_) / (float)(asyncProgress_.totalNodes_ +
            asyncProgress_.totalResources_);
    }
}

QString Scene::GetVarName(StringHash hash) const
{
    HashMap<StringHash, QString>::const_iterator i = varNames_.find(hash);
    return i != varNames_.end() ? MAP_VALUE(i) : QString::null;
}

void Scene::Update(float timeStep)
{
    if (asyncLoading_)
    {
        UpdateAsyncLoading();
        // If only preloading resources, scene update can continue
        if (asyncProgress_.mode_ > LOAD_RESOURCES_ONLY)
            return;
    }

    PROFILE(UpdateScene);

    timeStep *= timeScale_;

    using namespace SceneUpdate;

    VariantMap& eventData = GetEventDataMap();
    eventData[P_SCENE] = this;
    eventData[P_TIMESTEP] = timeStep;

    // Update variable timestep logic
    SendEvent(E_SCENEUPDATE, eventData);

    // Update scene attribute animation.
    SendEvent(E_ATTRIBUTEANIMATIONUPDATE, eventData);

    // Update scene subsystems. If a physics world is present, it will be updated, triggering fixed timestep logic updates
    SendEvent(E_SCENESUBSYSTEMUPDATE, eventData);

    // Update transform smoothing
    {
        PROFILE(UpdateSmoothing);

        float constant = 1.0f - Clamp(powf(2.0f, -timeStep * smoothingConstant_), 0.0f, 1.0f);
        float squaredSnapThreshold = snapThreshold_ * snapThreshold_;

        using namespace UpdateSmoothing;

        smoothingData_[P_CONSTANT] = constant;
        smoothingData_[P_SQUAREDSNAPTHRESHOLD] = squaredSnapThreshold;
        SendEvent(E_UPDATESMOOTHING, smoothingData_);
    }

    // Post-update variable timestep logic
    SendEvent(E_SCENEPOSTUPDATE, eventData);

    // Note: using a float for elapsed time accumulation is inherently inaccurate. The purpose of this value is
    // primarily to update material animation effects, as it is available to shaders. It can be reset by calling
    // SetElapsedTime()
    elapsedTime_ += timeStep;
}

void Scene::BeginThreadedUpdate()
{
    // Check the work queue subsystem whether it actually has created worker threads. If not, do not enter threaded mode.
    if (GetSubsystem<WorkQueue>()->GetNumThreads())
        threadedUpdate_ = true;
}

void Scene::EndThreadedUpdate()
{
    if (!threadedUpdate_)
        return;

    threadedUpdate_ = false;

    if (!delayedDirtyComponents_.empty())
    {
        PROFILE(EndThreadedUpdate);

        for (Component* i : delayedDirtyComponents_)
            i->OnMarkedDirty(i->GetNode());
        delayedDirtyComponents_.clear();
    }
}

void Scene::DelayedMarkedDirty(Component* component)
{
    MutexLock lock(sceneMutex_);
    delayedDirtyComponents_.push_back(component);
}

unsigned Scene::GetFreeNodeID(CreateMode mode)
{
    if (mode == REPLICATED)
    {
        for (;;)
        {
            unsigned ret = replicatedNodeID_;
            if (replicatedNodeID_ < LAST_REPLICATED_ID)
                ++replicatedNodeID_;
            else
                replicatedNodeID_ = FIRST_REPLICATED_ID;

            if (!replicatedNodes_.contains(ret))
                return ret;
        }
    }
    else
    {
        for (;;)
        {
            unsigned ret =  localNodeID_;
            if (localNodeID_ < LAST_LOCAL_ID)
                ++localNodeID_;
            else
                localNodeID_ = FIRST_LOCAL_ID;

            if (!localNodes_.contains(ret))
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
            unsigned ret = replicatedComponentID_;
            if (replicatedComponentID_ < LAST_REPLICATED_ID)
                ++replicatedComponentID_;
            else
                replicatedComponentID_ = FIRST_REPLICATED_ID;

            if (!replicatedComponents_.contains(ret))
                return ret;
        }
    }
    else
    {
        for (;;)
        {
            unsigned ret =  localComponentID_;
            if (localComponentID_ < LAST_LOCAL_ID)
                ++localComponentID_;
            else
                localComponentID_ = FIRST_LOCAL_ID;

            if (!localComponents_.contains(ret))
                return ret;
        }
    }
}

void Scene::NodeAdded(Node* node)
{
    if (!node || node->GetScene() == this)
        return;

    // If node already exists in another scene, remove. This is unsupported, as components will not reinitialize themselves
    // to use the new scene
    Scene* oldScene = node->GetScene();
    if (oldScene)
    {
        LOGERROR("Moving a node from one scene to another is unsupported");
        oldScene->NodeRemoved(node);
    }

    node->SetScene(this);

    // If the new node has an ID of zero (default), assign a replicated ID now
    unsigned id = node->GetID();
    if (!id)
    {
        id = GetFreeNodeID(REPLICATED);
        node->SetID(id);
    }

    // If node with same ID exists, remove the scene reference from it and overwrite with the new node
    if (id < FIRST_LOCAL_ID)
    {
        HashMap<unsigned, Node*>::iterator i = replicatedNodes_.find(id);
        if (i != replicatedNodes_.end() && MAP_VALUE(i) != node)
        {
            LOGWARNING("Overwriting node with ID " + QString::number(id));
            MAP_VALUE(i)->ResetScene();
        }

        replicatedNodes_[id] = node;

        MarkNetworkUpdate(node);
        MarkReplicationDirty(node);
    }
    else
    {
        HashMap<unsigned, Node*>::iterator i = localNodes_.find(id);
        if (i != localNodes_.end() && MAP_VALUE(i) != node)
        {
            LOGWARNING("Overwriting node with ID " + QString::number(id));
            MAP_VALUE(i)->ResetScene();
        }

        localNodes_[id] = node;
    }
}

void Scene::NodeRemoved(Node* node)
{
    if (!node || node->GetScene() != this)
        return;

    unsigned id = node->GetID();
    if (id < FIRST_LOCAL_ID)
    {
        replicatedNodes_.remove(id);
        MarkReplicationDirty(node);
    }
    else
        localNodes_.remove(id);

    node->SetID(0);
    node->SetScene(nullptr);
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
    if (!component)
        return;

    unsigned id = component->GetID();
    if (id < FIRST_LOCAL_ID)
    {
        HashMap<unsigned, Component*>::iterator i = replicatedComponents_.find(id);
        if (i != replicatedComponents_.end() && MAP_VALUE(i) != component)
        {
            LOGWARNING("Overwriting component with ID " + QString::number(id));
            MAP_VALUE(i)->SetID(0);
        }

        replicatedComponents_[id] = component;
    }
    else
    {
        HashMap<unsigned, Component*>::iterator i = localComponents_.find(id);
        if (i != localComponents_.end() && MAP_VALUE(i) != component)
        {
            LOGWARNING("Overwriting component with ID " + QString::number(id));
            MAP_VALUE(i)->SetID(0);
        }

        localComponents_[id] = component;
    }
}

void Scene::ComponentRemoved(Component* component)
{
    if (!component)
        return;

    unsigned id = component->GetID();
    if (id < FIRST_LOCAL_ID)
        replicatedComponents_.remove(id);
    else
        localComponents_.remove(id);

    component->SetID(0);
}

void Scene::SetVarNamesAttr(const QString& value)
{
    QStringList varNames = value.split(';');

    varNames_.clear();
    for (QStringList::const_iterator i = varNames.begin(); i != varNames.end(); ++i)
        varNames_[*i] = *i;
}

QString Scene::GetVarNamesAttr() const
{
    QString ret;

    if (!varNames_.empty())
    {
        for (const auto & elem : varNames_)
            ret += ELEMENT_VALUE(elem) + ';';

        ret.resize(ret.length() - 1);
    }

    return ret;
}

void Scene::PrepareNetworkUpdate()
{
    for (unsigned node_id : networkUpdateNodes_)
    {
        Node* node = GetNode(node_id);
        if (node)
            node->PrepareNetworkUpdate();
    }

    for (unsigned component_id : networkUpdateComponents_)
    {
        Component* component = GetComponent(component_id);
        if (component)
            component->PrepareNetworkUpdate();
    }

    networkUpdateNodes_.clear();
    networkUpdateComponents_.clear();
}

void Scene::CleanupConnection(Connection* connection)
{
    Node::CleanupConnection(connection);

    for (auto & elem : replicatedNodes_)
        ELEMENT_VALUE(elem)->CleanupConnection(connection);

    for (auto & elem : replicatedComponents_)
        ELEMENT_VALUE(elem)->CleanupConnection(connection);
}

void Scene::MarkNetworkUpdate(Node* node)
{
    if (node)
    {
        if (!threadedUpdate_)
            networkUpdateNodes_.insert(node->GetID());
        else
        {
            MutexLock lock(sceneMutex_);
            networkUpdateNodes_.insert(node->GetID());
        }
    }
}

void Scene::MarkNetworkUpdate(Component* component)
{
    if (component)
    {
        if (!threadedUpdate_)
            networkUpdateComponents_.insert(component->GetID());
        else
        {
            MutexLock lock(sceneMutex_);
            networkUpdateComponents_.insert(component->GetID());
        }
    }
}

void Scene::MarkReplicationDirty(Node* node)
{
    unsigned id = node->GetID();

    if (id < FIRST_LOCAL_ID && networkState_)
    {
        for (ReplicationState* elem : networkState_->replicationStates_)
        {
            NodeReplicationState* nodeState = static_cast<NodeReplicationState*>(elem);
            nodeState->sceneState_->dirtyNodes_.insert(id);
        }
    }
}

void Scene::HandleUpdate(StringHash eventType, VariantMap& eventData)
{
    using namespace Update;

    if (updateEnabled_)
        Update(eventData[P_TIMESTEP].GetFloat());
}

void Scene::HandleResourceBackgroundLoaded(StringHash eventType, VariantMap& eventData)
{
    using namespace ResourceBackgroundLoaded;

    if (asyncLoading_)
    {
        Resource* resource = static_cast<Resource*>(eventData[P_RESOURCE].GetPtr());
        if (asyncProgress_.resources_.contains(resource->GetNameHash()))
        {
            asyncProgress_.resources_.remove(resource->GetNameHash());
            ++asyncProgress_.loadedResources_;
        }
    }
}

void Scene::UpdateAsyncLoading()
{
    PROFILE(UpdateAsyncLoading);

    // If resources left to load, do not load nodes yet
    if (asyncProgress_.loadedResources_ < asyncProgress_.totalResources_)
        return;

    HiresTimer asyncLoadTimer;

    for (;;)
    {
        if (asyncProgress_.loadedNodes_ >= asyncProgress_.totalNodes_)
        {
            FinishAsyncLoading();
            return;
        }

        // Read one child node with its full sub-hierarchy either from binary or XML
        /// \todo Works poorly in scenes where one root-level child node contains all content
        if (!asyncProgress_.xmlFile_)
        {
            unsigned nodeID = asyncProgress_.file_->ReadUInt();
            Node* newNode = CreateChild(nodeID, nodeID < FIRST_LOCAL_ID ? REPLICATED : LOCAL);
            resolver_.AddNode(nodeID, newNode);
            newNode->Load(*asyncProgress_.file_, resolver_);
        }
        else
        {
            unsigned nodeID = asyncProgress_.xmlElement_.GetInt("id");
            Node* newNode = CreateChild(nodeID, nodeID < FIRST_LOCAL_ID ? REPLICATED : LOCAL);
            resolver_.AddNode(nodeID, newNode);
            newNode->LoadXML(asyncProgress_.xmlElement_, resolver_);
            asyncProgress_.xmlElement_ = asyncProgress_.xmlElement_.GetNext("node");
        }

        ++asyncProgress_.loadedNodes_;

        // Break if time limit exceeded, so that we keep sufficient FPS
        if (asyncLoadTimer.GetUSecS() >= asyncLoadingMs_ * 1000)
            break;
    }

    using namespace AsyncLoadProgress;

    VariantMap& eventData = GetEventDataMap();
    eventData[P_SCENE] = this;
    eventData[P_PROGRESS] = GetAsyncProgress();
    eventData[P_LOADEDNODES] = asyncProgress_.loadedNodes_;
    eventData[P_TOTALNODES] = asyncProgress_.totalNodes_;
    eventData[P_LOADEDRESOURCES]  = asyncProgress_.loadedResources_;
    eventData[P_TOTALRESOURCES] = asyncProgress_.totalResources_;
    SendEvent(E_ASYNCLOADPROGRESS, eventData);
}

void Scene::FinishAsyncLoading()
{
    if (asyncProgress_.mode_ > LOAD_RESOURCES_ONLY)
    {
        resolver_.Resolve();
        ApplyAttributes();
        FinishLoading(asyncProgress_.file_);
    }

    StopAsyncLoading();

    using namespace AsyncLoadFinished;

    VariantMap& eventData = GetEventDataMap();
    eventData[P_SCENE] = this;
    SendEvent(E_ASYNCLOADFINISHED, eventData);
}

void Scene::FinishLoading(Deserializer* source)
{
    if (source)
    {
        fileName_ = source->GetName();
        checksum_ = source->GetChecksum();
    }
}

void Scene::FinishSaving(Serializer* dest) const
{
    Deserializer* ptr = dynamic_cast<Deserializer*>(dest);
    if (ptr)
    {
        fileName_ = ptr->GetName();
        checksum_ = ptr->GetChecksum();
    }
}

void Scene::PreloadResources(File* file, bool isSceneFile)
{
    ResourceCache* cache = GetSubsystem<ResourceCache>();

    // Read node ID (not needed)
    /*unsigned nodeID = */file->ReadUInt();

    // Read Node or Scene attributes; these do not include any resources
    const std::vector<AttributeInfo>* attributes = context_->GetAttributes(isSceneFile ? Scene::GetTypeStatic() : Node::GetTypeStatic());
    assert(attributes);

    for (unsigned i = 0; i < attributes->size(); ++i)
    {
        const AttributeInfo& attr = attributes->at(i);
        if (!(attr.mode_ & AM_FILE))
            continue;
        Variant varValue = file->ReadVariant(attr.type_);
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
        if (attributes)
        {
            for (unsigned j = 0; j < attributes->size(); ++j)
            {
                const AttributeInfo& attr = attributes->at(j);
                if (!(attr.mode_ & AM_FILE))
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
                        ++asyncProgress_.totalResources_;
                        asyncProgress_.resources_.insert(StringHash(name));
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
                            ++asyncProgress_.totalResources_;
                            asyncProgress_.resources_.insert(StringHash(name));
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
    ResourceCache* cache = GetSubsystem<ResourceCache>();

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

                while (attempts)
                {
                    const AttributeInfo& attr = attributes->at(i);
                    if ((attr.mode_ & AM_FILE) && !attr.name_.compare(name))
                    {
                        if (attr.type_ == VAR_RESOURCEREF)
                        {
                            ResourceRef ref = attrElem.GetVariantValue(attr.type_).GetResourceRef();
                            QString name = cache->SanitateResourceName(ref.name_);
                            bool success = cache->BackgroundLoadResource(ref.type_, name);
                            if (success)
                            {
                                ++asyncProgress_.totalResources_;
                                asyncProgress_.resources_.insert(StringHash(name));
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
                                    ++asyncProgress_.totalResources_;
                                    asyncProgress_.resources_.insert(StringHash(name));
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
