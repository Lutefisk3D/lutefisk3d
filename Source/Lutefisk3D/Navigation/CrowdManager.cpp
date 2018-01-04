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

#include "Lutefisk3D/Scene/Component.h"
#include "Lutefisk3D/Core/Context.h"
#include "Lutefisk3D/Core/Profiler.h"
#include "Lutefisk3D/Graphics/DebugRenderer.h"
#include "Lutefisk3D/IO/Log.h"
#include "Lutefisk3D/Navigation/CrowdAgent.h"
#include "Lutefisk3D/Navigation/CrowdManager.h"
#include "Lutefisk3D/Navigation/DynamicNavigationMesh.h"
#include "Lutefisk3D/Navigation/NavigationEvents.h"
#include "Lutefisk3D/Scene/Node.h"
#include "Lutefisk3D/Scene/Scene.h"
#include "Lutefisk3D/Scene/SceneEvents.h"
#include <vector>

#include <DetourCrowd/DetourCrowd.h>
#include <Recast/Recast.h>

namespace Urho3D
{

extern const char* NAVIGATION_CATEGORY;

static const unsigned DEFAULT_MAX_AGENTS = 512;
static const float DEFAULT_MAX_AGENT_RADIUS = 0.f;

const char* filterTypesStructureElementNames[] =
{
    "Query Filter Type Count",
    "   Include Flags",
    "   Exclude Flags",
    "   >AreaCost",
    0
};

const char* obstacleAvoidanceTypesStructureElementNames[] =
{
    "Obstacle Avoid. Type Count",
    "   Velocity Bias",
    "   Desired Velocity Weight",
    "   Current Velocity Weight",
    "   Side Bias Weight",
    "   Time of Impact Weight",
    "   Time Horizon",
    "   Grid Size",
    "   Adaptive Divs",
    "   Adaptive Rings",
    "   Adaptive Depth",
    0
};
void CrowdAgentUpdateCallback(dtCrowdAgent* ag, float dt)
{
    static_cast<CrowdAgent*>(ag->params.userData)->OnCrowdUpdate(ag, dt);
}

CrowdManager::CrowdManager(Context* context) :
    Component(context),
    crowd_(nullptr),
    navigationMeshId_(0),
    maxAgents_(DEFAULT_MAX_AGENTS),
    maxAgentRadius_(DEFAULT_MAX_AGENT_RADIUS),
    numQueryFilterTypes_(0),
    numObstacleAvoidanceTypes_(0)
{
    // The actual buffer is allocated inside dtCrowd, we only track the number of "slots" being configured explicitly
    numAreas_.reserve(DT_CROWD_MAX_QUERY_FILTER_TYPE);
    for (unsigned i = 0; i < DT_CROWD_MAX_QUERY_FILTER_TYPE; ++i)
        numAreas_.push_back(0);
}

CrowdManager::~CrowdManager()
{
    dtFreeCrowd(crowd_);
    crowd_ = 0;
}

void CrowdManager::RegisterObject(Context* context)
{
    context->RegisterFactory<CrowdManager>(NAVIGATION_CATEGORY);

    URHO3D_ATTRIBUTE("Max Agents", unsigned, maxAgents_, DEFAULT_MAX_AGENTS, AM_DEFAULT);
    URHO3D_ATTRIBUTE("Max Agent Radius", float, maxAgentRadius_, DEFAULT_MAX_AGENT_RADIUS, AM_DEFAULT);
    URHO3D_ATTRIBUTE("Navigation Mesh", unsigned, navigationMeshId_, 0, AM_DEFAULT | AM_COMPONENTID);
    URHO3D_MIXED_ACCESSOR_ATTRIBUTE("Filter Types", GetQueryFilterTypesAttr, SetQueryFilterTypesAttr,
        VariantVector, Variant::emptyVariantVector, AM_DEFAULT)
        .SetMetadata(AttributeMetadata::P_VECTOR_STRUCT_ELEMENTS, filterTypesStructureElementNames);
    URHO3D_MIXED_ACCESSOR_ATTRIBUTE("Obstacle Avoidance Types", GetObstacleAvoidanceTypesAttr, SetObstacleAvoidanceTypesAttr,
        VariantVector, Variant::emptyVariantVector, AM_DEFAULT)
        .SetMetadata(AttributeMetadata::P_VECTOR_STRUCT_ELEMENTS, obstacleAvoidanceTypesStructureElementNames);
}

void CrowdManager::ApplyAttributes()
{
    // Values from Editor, saved-file, or network must be checked before applying
    maxAgents_ = std::max<unsigned>(1, maxAgents_);
    maxAgentRadius_ = std::max<float>(0.f, maxAgentRadius_);

    bool navMeshChange = false;
    Scene* scene = GetScene();
    if (scene && navigationMeshId_)
    {
        NavigationMesh* navMesh = dynamic_cast<NavigationMesh*>(scene->GetComponent(navigationMeshId_));
        if (navMesh && navMesh != navigationMesh_)
        {
            SetNavigationMesh(navMesh); // This will also CreateCrowd(), so the rest of the function is unnecessary
            return;
        }
    }
    // In case of receiving an invalid component id, revert it back to the existing navmesh component id (if any)
    navigationMeshId_ = navigationMesh_ ? navigationMesh_->GetID() : 0;

    // If the Detour crowd initialization parameters have changed then recreate it
    if (crowd_ && (navMeshChange || crowd_->getAgentCount() != maxAgents_ || crowd_->getMaxAgentRadius() != maxAgentRadius_))
        CreateCrowd();
}

void CrowdManager::DrawDebugGeometry(DebugRenderer* debug, bool depthTest)
{
    if (debug && crowd_)
    {
        // Current position-to-target line
        for (int i = 0; i < crowd_->getAgentCount(); i++)
        {
            const dtCrowdAgent* ag = crowd_->getAgent(i);
            if (!ag->active)
                continue;

            // Draw CrowdAgent shape (from its radius & height)
            CrowdAgent* crowdAgent = static_cast<CrowdAgent*>(ag->params.userData);
            crowdAgent->DrawDebugGeometry(debug, depthTest);

            // Draw move target if any
            if (crowdAgent->GetTargetState() == CA_TARGET_NONE || crowdAgent->GetTargetState() == CA_TARGET_VELOCITY)
                continue;

            Color color(0.6f, 0.2f, 0.2f, 1.0f);

            // Draw line to target
            Vector3 pos1(ag->npos[0], ag->npos[1], ag->npos[2]);
            Vector3 pos2;
            for (int i = 0; i < ag->ncorners; ++i)
            {
                pos2.x_ = ag->cornerVerts[i * 3];
                pos2.y_ = ag->cornerVerts[i * 3 + 1];
                pos2.z_ = ag->cornerVerts[i * 3 + 2];
                debug->AddLine(pos1, pos2, color, depthTest);
                pos1 = pos2;
            }
            pos2.x_ = ag->targetPos[0];
            pos2.y_ = ag->targetPos[1];
            pos2.z_ = ag->targetPos[2];
            debug->AddLine(pos1, pos2, color, depthTest);

            // Draw target circle
            debug->AddSphere(Sphere(pos2, 0.5f), color, depthTest);
        }
    }
}

void CrowdManager::DrawDebugGeometry(bool depthTest)
{
    Scene* scene = GetScene();
    if (scene)
    {
        DebugRenderer* debug = scene->GetComponent<DebugRenderer>();
        if (debug)
            DrawDebugGeometry(debug, depthTest);
    }
}

void CrowdManager::SetCrowdTarget(const Vector3& position, Node* node)
{
    if (!crowd_)
        return;

    std::vector<CrowdAgent*> agents = GetAgents(node, false);     // Get all crowd agent components
    Vector3 moveTarget(position);
    for (unsigned i = 0; i < agents.size(); ++i)
    {
        // Give application a chance to determine the desired crowd formation when they reach the target position
        CrowdAgent* agent = agents[i];

        using namespace CrowdAgentFormation;

        VariantMap& map = GetEventDataMap();
        map[P_NODE] = agent->GetNode();
        map[P_CROWD_AGENT] = agent;
        map[P_INDEX] = i;
        map[P_SIZE] = int(agents.size());
        map[P_POSITION] = moveTarget;   // Expect the event handler will modify this position accordingly

        SendEvent(E_CROWD_AGENT_FORMATION, map);

        moveTarget = map[P_POSITION].GetVector3();
        agent->SetTargetPosition(moveTarget);
    }
}

void CrowdManager::SetCrowdVelocity(const Vector3& velocity, Node* node)
{
    if (!crowd_)
        return;

    std::vector<CrowdAgent*> agents = GetAgents(node, true);      // Get only crowd agent components already in the crowd
    for (unsigned i = 0; i < agents.size(); ++i)
        agents[i]->SetTargetVelocity(velocity);
}

void CrowdManager::ResetCrowdTarget(Node* node)
{
    if (!crowd_)
        return;

    std::vector<CrowdAgent*> agents = GetAgents(node, true);
    for (unsigned i = 0; i < agents.size(); ++i)
        agents[i]->ResetTarget();
}

void CrowdManager::SetMaxAgents(unsigned maxAgents)
{
    if (maxAgents != maxAgents_ && maxAgents > 0)
    {
        maxAgents_ = maxAgents;
        CreateCrowd();
        MarkNetworkUpdate();
    }
}

void CrowdManager::SetMaxAgentRadius(float maxAgentRadius)
{
    if (maxAgentRadius != maxAgentRadius_ && maxAgentRadius > 0.f)
    {
        maxAgentRadius_ = maxAgentRadius;
        CreateCrowd();
        MarkNetworkUpdate();
    }
}

void CrowdManager::SetNavigationMesh(NavigationMesh* navMesh)
{
    Scene* scene = GetScene();
    if(scene) {
        scene->componentAdded.Disconnect(this,&CrowdManager::HandleComponentAdded);
        scene->componentRemoved.Disconnect(this,&CrowdManager::HandleNavMeshRemoved);
    }
    if(navigationMesh_)
        navigationMesh_->navigationMeshRebuilt.Disconnect(this,&CrowdManager::HandleNavMeshRebuilt);

    if (navMesh != navigationMesh_)     // It is possible to reset navmesh pointer back to 0
    {

        navigationMesh_ = navMesh;
        navigationMeshId_ = navMesh ? navMesh->GetID() : 0;

        if (navMesh)
        {
            navMesh->navigationMeshRebuilt.Connect(this,&CrowdManager::HandleNavMeshRebuilt);
            scene->componentRemoved.Connect(this,&CrowdManager::HandleNavMeshRemoved);
        }

        CreateCrowd();
        MarkNetworkUpdate();
    }
}

void CrowdManager::SetQueryFilterTypesAttr(const VariantVector& value)
{
    if (!crowd_)
        return;

    unsigned index = 0;
    unsigned queryFilterType = 0;
    numQueryFilterTypes_ = index < value.size() ? std::min<unsigned>(value[index++].GetUInt(), DT_CROWD_MAX_QUERY_FILTER_TYPE) : 0;

    while (queryFilterType < numQueryFilterTypes_)
    {
        if (index + 3 <= value.size())
        {
            dtQueryFilter* filter = crowd_->getEditableFilter(queryFilterType);
            assert(filter);
            filter->setIncludeFlags((unsigned short)value[index++].GetUInt());
            filter->setExcludeFlags((unsigned short)value[index++].GetUInt());
            unsigned prevNumAreas = numAreas_[queryFilterType];
            numAreas_[queryFilterType] = std::min<unsigned>(value[index++].GetUInt(), DT_MAX_AREAS);

            // Must loop through based on previous number of areas, the new area cost (if any) can only be set in the next attribute get/set iteration
            if (index + prevNumAreas <= value.size())
            {
                for (unsigned i = 0; i < prevNumAreas; ++i)
                    filter->setAreaCost(i, value[index++].GetFloat());
            }
        }
        ++queryFilterType;
    }
}

void CrowdManager::SetIncludeFlags(unsigned queryFilterType, unsigned short flags)
{
    dtQueryFilter* filter = const_cast<dtQueryFilter*>(GetDetourQueryFilter(queryFilterType));
    if (filter)
    {
        filter->setIncludeFlags(flags);
        if (numQueryFilterTypes_ < queryFilterType + 1)
            numQueryFilterTypes_ = queryFilterType + 1;
        MarkNetworkUpdate();
    }
}

void CrowdManager::SetExcludeFlags(unsigned queryFilterType, unsigned short flags)
{
    dtQueryFilter* filter = const_cast<dtQueryFilter*>(GetDetourQueryFilter(queryFilterType));
    if (filter)
    {
        filter->setExcludeFlags(flags);
        if (numQueryFilterTypes_ < queryFilterType + 1)
            numQueryFilterTypes_ = queryFilterType + 1;
        MarkNetworkUpdate();
    }
}

void CrowdManager::SetAreaCost(unsigned queryFilterType, unsigned areaID, float cost)
{
    dtQueryFilter* filter = const_cast<dtQueryFilter*>(GetDetourQueryFilter(queryFilterType));
    if (filter && areaID < DT_MAX_AREAS)
    {
        filter->setAreaCost((int)areaID, cost);
        if (numQueryFilterTypes_ < queryFilterType + 1)
            numQueryFilterTypes_ = queryFilterType + 1;
        if (numAreas_[queryFilterType] < areaID + 1)
            numAreas_[queryFilterType] = areaID + 1;
        MarkNetworkUpdate();
    }
}

void CrowdManager::SetObstacleAvoidanceTypesAttr(const VariantVector& value)
{
    if (!crowd_)
        return;

    unsigned index = 0;
    unsigned obstacleAvoidanceType = 0;
    numObstacleAvoidanceTypes_ = index < value.size() ? std::min<unsigned>(value[index++].GetUInt(), DT_CROWD_MAX_OBSTAVOIDANCE_PARAMS) : 0;

    while (obstacleAvoidanceType < numObstacleAvoidanceTypes_)
    {
        if (index + 10 <= value.size())
        {
            dtObstacleAvoidanceParams params;
            params.velBias = value[index++].GetFloat();
            params.weightDesVel = value[index++].GetFloat();
            params.weightCurVel = value[index++].GetFloat();
            params.weightSide = value[index++].GetFloat();
            params.weightToi = value[index++].GetFloat();
            params.horizTime = value[index++].GetFloat();
            params.gridSize = (unsigned char)value[index++].GetUInt();
            params.adaptiveDivs = (unsigned char)value[index++].GetUInt();
            params.adaptiveRings = (unsigned char)value[index++].GetUInt();
            params.adaptiveDepth = (unsigned char)value[index++].GetUInt();
            crowd_->setObstacleAvoidanceParams(obstacleAvoidanceType, &params);
        }
        ++obstacleAvoidanceType;
    }
}

void CrowdManager::SetObstacleAvoidanceParams(unsigned obstacleAvoidanceType, const CrowdObstacleAvoidanceParams& params)
{
    if (crowd_ && obstacleAvoidanceType < DT_CROWD_MAX_OBSTAVOIDANCE_PARAMS)
    {
        crowd_->setObstacleAvoidanceParams(obstacleAvoidanceType, reinterpret_cast<const dtObstacleAvoidanceParams*>(&params));
        if (numObstacleAvoidanceTypes_ < obstacleAvoidanceType + 1)
            numObstacleAvoidanceTypes_ = obstacleAvoidanceType + 1;
        MarkNetworkUpdate();
    }
}

Vector3 CrowdManager::FindNearestPoint(const Vector3& point, int queryFilterType, dtPolyRef* nearestRef)
{
    if (nearestRef)
        *nearestRef = 0;
    return crowd_ && navigationMesh_ ?
                navigationMesh_->FindNearestPoint(point, Vector3(crowd_->getQueryExtents()), crowd_->getFilter(queryFilterType), nearestRef) : point;
}

Vector3 CrowdManager::MoveAlongSurface(const Vector3& start, const Vector3& end, int queryFilterType, int maxVisited)
{
    return crowd_ && navigationMesh_ ?
                navigationMesh_->MoveAlongSurface(start, end, Vector3(crowd_->getQueryExtents()), maxVisited, crowd_->getFilter(queryFilterType)) :
                end;
}

void CrowdManager::FindPath(std::deque<Vector3>& dest, const Vector3& start, const Vector3& end, int queryFilterType)
{
    if (crowd_ && navigationMesh_)
        navigationMesh_->FindPath(dest, start, end, Vector3(crowd_->getQueryExtents()), crowd_->getFilter(queryFilterType));
}

Vector3 CrowdManager::GetRandomPoint(int queryFilterType, dtPolyRef* randomRef)
{
    if (randomRef)
        *randomRef = 0;
    return crowd_ && navigationMesh_ ? navigationMesh_->GetRandomPoint(crowd_->getFilter(queryFilterType), randomRef) :
                                       Vector3::ZERO;
}

Vector3 CrowdManager::GetRandomPointInCircle(const Vector3& center, float radius, int queryFilterType, dtPolyRef* randomRef)
{
    if (randomRef)
        *randomRef = 0;
    return crowd_ && navigationMesh_ ?
                navigationMesh_->GetRandomPointInCircle(center, radius, Vector3(crowd_->getQueryExtents()),
                                                        crowd_->getFilter(queryFilterType), randomRef) : center;
}

float CrowdManager::GetDistanceToWall(const Vector3& point, float radius, int queryFilterType, Vector3* hitPos, Vector3* hitNormal)
{
    if (hitPos)
        *hitPos = Vector3::ZERO;
    if (hitNormal)
        *hitNormal = Vector3::DOWN;
    return crowd_ && navigationMesh_ ?
                navigationMesh_->GetDistanceToWall(point, radius, Vector3(crowd_->getQueryExtents()), crowd_->getFilter(queryFilterType),
                                                   hitPos, hitNormal) : radius;
}

Vector3 CrowdManager::Raycast(const Vector3& start, const Vector3& end, int queryFilterType, Vector3* hitNormal)
{
    if (hitNormal)
        *hitNormal = Vector3::DOWN;
    return crowd_ && navigationMesh_ ?
                navigationMesh_->Raycast(start, end, Vector3(crowd_->getQueryExtents()), crowd_->getFilter(queryFilterType), hitNormal)
              : end;
}

unsigned CrowdManager::GetNumAreas(unsigned queryFilterType) const
{
    return queryFilterType < numQueryFilterTypes_ ? numAreas_[queryFilterType] : 0;
}

VariantVector CrowdManager::GetQueryFilterTypesAttr() const
{
    VariantVector ret;
    if (crowd_)
    {
        unsigned totalNumAreas = 0;
        for (unsigned i = 0; i < numQueryFilterTypes_; ++i)
            totalNumAreas += numAreas_[i];

        ret.reserve(numQueryFilterTypes_ * 3 + totalNumAreas + 1);
        ret.push_back(numQueryFilterTypes_);

        for (unsigned i = 0; i < numQueryFilterTypes_; ++i)
        {
            const dtQueryFilter* filter = crowd_->getFilter(i);
            assert(filter);
            ret.push_back(filter->getIncludeFlags());
            ret.push_back(filter->getExcludeFlags());
            ret.push_back(numAreas_[i]);

            for (unsigned j = 0; j < numAreas_[i]; ++j)
                ret.push_back(filter->getAreaCost(j));
        }
    }
    else
        ret.push_back(0);

    return ret;
}

unsigned short CrowdManager::GetIncludeFlags(unsigned queryFilterType) const
{
    if (queryFilterType >= numQueryFilterTypes_)
        URHO3D_LOGWARNING(QString("Query filter type %1 is not configured yet, returning the default include flags initialized by dtCrowd")
                          .arg(queryFilterType));
    const dtQueryFilter* filter = GetDetourQueryFilter(queryFilterType);
    return (unsigned short)(filter ? filter->getIncludeFlags() : 0xffff);
}

unsigned short CrowdManager::GetExcludeFlags(unsigned queryFilterType) const
{
    if (queryFilterType >= numQueryFilterTypes_)
        URHO3D_LOGWARNING(QString("Query filter type %1 is not configured yet, returning the default exclude flags initialized by dtCrowd")
                          .arg(queryFilterType));
    const dtQueryFilter* filter = GetDetourQueryFilter(queryFilterType);
    return (unsigned short)(filter ? filter->getExcludeFlags() : 0);
}

float CrowdManager::GetAreaCost(unsigned queryFilterType, unsigned areaID) const
{
    if (queryFilterType >= numQueryFilterTypes_ || areaID >= numAreas_[queryFilterType])
        URHO3D_LOGWARNING(QString("Query filter type %1 and/or area id %d are not configured yet, returning the default area cost initialized by dtCrowd")
                          .arg(queryFilterType).arg(areaID));
    const dtQueryFilter* filter = GetDetourQueryFilter(queryFilterType);
    return filter ? filter->getAreaCost((int)areaID) : 1.f;
}

VariantVector CrowdManager::GetObstacleAvoidanceTypesAttr() const
{
    VariantVector ret;
    if (crowd_)
    {
        ret.reserve(numObstacleAvoidanceTypes_ * 10 + 1);
        ret.push_back(numObstacleAvoidanceTypes_);

        for (unsigned i = 0; i < numObstacleAvoidanceTypes_; ++i)
        {
            const dtObstacleAvoidanceParams* params = crowd_->getObstacleAvoidanceParams(i);
            assert(params);
            ret.push_back(params->velBias);
            ret.push_back(params->weightDesVel);
            ret.push_back(params->weightCurVel);
            ret.push_back(params->weightSide);
            ret.push_back(params->weightToi);
            ret.push_back(params->horizTime);
            ret.push_back(params->gridSize);
            ret.push_back(params->adaptiveDivs);
            ret.push_back(params->adaptiveRings);
            ret.push_back(params->adaptiveDepth);
        }
    }
    else
        ret.push_back(0);

    return ret;
}

const CrowdObstacleAvoidanceParams& CrowdManager::GetObstacleAvoidanceParams(unsigned obstacleAvoidanceType) const
{
    static const CrowdObstacleAvoidanceParams EMPTY_PARAMS = CrowdObstacleAvoidanceParams();
    const dtObstacleAvoidanceParams* params = crowd_ ? crowd_->getObstacleAvoidanceParams(obstacleAvoidanceType) : 0;
    return params ? *reinterpret_cast<const CrowdObstacleAvoidanceParams*>(params) : EMPTY_PARAMS;
}

std::vector<CrowdAgent*> CrowdManager::GetAgents(Node* node, bool inCrowdFilter) const
{
    if (!node)
        node = GetScene();
    std::vector<CrowdAgent*> agents;
    node->GetComponents<CrowdAgent>(agents, true);
    if (inCrowdFilter)
    {
        std::vector<CrowdAgent*>::iterator i = agents.begin();
        while (i != agents.end())
        {
            if ((*i)->IsInCrowd())
                ++i;
            else
                i = agents.erase(i);
        }
    }
    return agents;
}

bool CrowdManager::CreateCrowd()
{
    if (!navigationMesh_ || !navigationMesh_->InitializeQuery())
        return false;

    // Preserve the existing crowd configuration before recreating it
    VariantVector queryFilterTypeConfiguration, obstacleAvoidanceTypeConfiguration;
    bool recreate = crowd_ != 0;
    if (recreate)
    {
        queryFilterTypeConfiguration = GetQueryFilterTypesAttr();
        obstacleAvoidanceTypeConfiguration = GetObstacleAvoidanceTypesAttr();
        dtFreeCrowd(crowd_);
    }
    crowd_ = dtAllocCrowd();

    // Initialize the crowd
    if (maxAgentRadius_ == 0.f)
        maxAgentRadius_ = navigationMesh_->GetAgentRadius();
    if (!crowd_->init(maxAgents_, maxAgentRadius_, navigationMesh_->navMesh_, CrowdAgentUpdateCallback))
    {
        URHO3D_LOGERROR("Could not initialize DetourCrowd");
        return false;
    }
    if (recreate)
    {
        // Reconfigure the newly initialized crowd
        SetQueryFilterTypesAttr(queryFilterTypeConfiguration);
        SetObstacleAvoidanceTypesAttr(obstacleAvoidanceTypeConfiguration);

        // Re-add the existing crowd agents
        std::vector<CrowdAgent*> agents = GetAgents();
        for (unsigned i = 0; i < agents.size(); ++i)
        {
            // Keep adding until the crowd cannot take it anymore
            if (agents[i]->AddAgentToCrowd(true) == -1)
            {
                URHO3D_LOGWARNING(QString("CrowdManager: %1 crowd agents orphaned").arg(agents.size() - i));
                break;
            }
        }
    }

    return true;
}

int CrowdManager::AddAgent(CrowdAgent* agent, const Vector3& pos)
{
    if (!crowd_ || !navigationMesh_ || !agent)
        return -1;
    dtCrowdAgentParams params;
    params.userData = agent;
    if (agent->radius_ == 0.f)
        agent->radius_ = navigationMesh_->GetAgentRadius();
    if (agent->height_ == 0.f)
        agent->height_ = navigationMesh_->GetAgentHeight();
    // dtCrowd::addAgent() requires the query filter type to find the nearest position on navmesh as the initial agent's position
    params.queryFilterType = (unsigned char)agent->GetQueryFilterType();
    return crowd_->addAgent(pos.Data(), &params);
}

void CrowdManager::RemoveAgent(CrowdAgent* agent)
{
    if (!crowd_ || !agent)
        return;
    dtCrowdAgent* agt = crowd_->getEditableAgent(agent->GetAgentCrowdId());
    if (agt)
        agt->params.userData = 0;
    crowd_->removeAgent(agent->GetAgentCrowdId());
}

void CrowdManager::OnSceneSet(Scene* scene)
{
    // Subscribe to the scene subsystem update, which will trigger the crowd update step, and grab a reference
    // to the scene's NavigationMesh
    if (scene)
    {
        if (scene != node_)
        {
            URHO3D_LOGERROR("CrowdManager is a scene component and should only be attached to the scene node");
            return;
        }
        scene->sceneSubsystemUpdate.Connect(this,&CrowdManager::HandleSceneSubsystemUpdate);

        // Attempt to auto discover a NavigationMesh component (or its derivative) under the scene node
        if (navigationMeshId_ == 0)
        {
            NavigationMesh* navMesh = scene->GetDerivedComponent<NavigationMesh>(true);
            if (navMesh)
                SetNavigationMesh(navMesh);
            else
            {
                // If not found, attempt to find in a delayed manner
                scene->componentAdded.Connect(this,&CrowdManager::HandleComponentAdded);
            }
        }
    }
    else
    {
        scene->sceneSubsystemUpdate.Disconnect(this,&CrowdManager::HandleSceneSubsystemUpdate);
        if(navigationMesh_)
            navigationMesh_->navigationMeshRebuilt.Disconnect(this,&CrowdManager::HandleNavMeshRebuilt);
        scene->componentAdded.Disconnect(this,&CrowdManager::HandleComponentAdded);
        scene->componentRemoved.Disconnect(this,&CrowdManager::HandleNavMeshRemoved);

        navigationMesh_ = 0;
    }
}

void CrowdManager::Update(float delta)
{
    assert(crowd_ && navigationMesh_);
    URHO3D_PROFILE(UpdateCrowd);
    crowd_->update(delta, 0);
}

const dtCrowdAgent* CrowdManager::GetDetourCrowdAgent(int agent) const
{
    return crowd_ ? crowd_->getAgent(agent) : 0;
}

const dtQueryFilter* CrowdManager::GetDetourQueryFilter(unsigned queryFilterType) const
{
    return crowd_ ? crowd_->getFilter(queryFilterType) : 0;
}

void CrowdManager::HandleSceneSubsystemUpdate(Scene *,float ts)
{
    // Perform update tick as long as the crowd is initialized and the associated navmesh has not been removed
    if (crowd_ && navigationMesh_)
    {
        if (IsEnabledEffective())
            Update(ts);
    }
}
void CrowdManager::HandleNavMeshRemoved(Scene *,Node *,Component *component)
{
    NavigationMesh* navMesh = static_cast<NavigationMesh*>(component);
    // Only interested in navmesh component being used to initialized the crowd
    if (navMesh != navigationMesh_)
        return;
    // Since this is a component removed event, reset our own navmesh pointer
    SetNavigationMesh(nullptr);
}
void CrowdManager::HandleNavMeshRebuilt(Node *,NavigationMesh *navMesh)
{
    // Reset internal pointer so that the same navmesh can be reassigned and the crowd creation be reattempted
    if (navMesh == navigationMesh_)
        navigationMesh_.Reset();
    SetNavigationMesh(navMesh);
}

void CrowdManager::HandleComponentAdded(Scene *,Node *,Component *)
{
    Scene* scene = GetScene();
    if (scene)
    {
        NavigationMesh* navMesh = scene->GetDerivedComponent<NavigationMesh>(true);
        if (navMesh)
            SetNavigationMesh(navMesh);
    }
}

}
