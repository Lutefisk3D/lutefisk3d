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

#include "DynamicNavigationMesh.h"

#include "CrowdAgent.h"
#include "DynamicNavigationMesh.h"
#include "NavArea.h"
#include "NavBuildData.h"
#include "NavigationEvents.h"
#include "Obstacle.h"
#include "OffMeshConnection.h"
#include "Lutefisk3D/Core/Context.h"
#include "Lutefisk3D/Core/Profiler.h"
#include "Lutefisk3D/Graphics/DebugRenderer.h"
#include "Lutefisk3D/IO/Log.h"
#include "Lutefisk3D/IO/VectorBuffer.h"
#include "Lutefisk3D/IO/MemoryBuffer.h"
#include "Lutefisk3D/Math/BoundingBox.h"
#include "Lutefisk3D/Scene/Node.h"
#include "Lutefisk3D/Scene/Scene.h"
#include "Lutefisk3D/Scene/SceneEvents.h"

#include <LZ4/lz4.h>
#include <cfloat>

#include <Detour/DetourNavMesh.h>
#include <Detour/DetourNavMeshBuilder.h>
#include <Detour/DetourNavMeshQuery.h>
#include <DetourTileCache/DetourTileCache.h>
#include <DetourTileCache/DetourTileCacheBuilder.h>
#include <Recast/Recast.h>
#include <Recast/RecastAlloc.h>


static constexpr const unsigned TILECACHE_MAXLAYERS = 255;

namespace Urho3D
{

extern const char* NAVIGATION_CATEGORY;
namespace {
const constexpr int DEFAULT_MAX_OBSTACLES = 1024;
const constexpr int DEFAULT_MAX_LAYERS = 16;
}
struct DynamicNavigationMesh::TileCacheData
{
    unsigned char* data;
    int dataSize;
};

struct TileCompressor : public dtTileCacheCompressor
{
    virtual int maxCompressedSize(const int bufferSize)
    {
        return (int)(bufferSize* 1.05f);
    }

    virtual dtStatus compress(const unsigned char* buffer, const int bufferSize,
                              unsigned char* compressed, const int /*maxCompressedSize*/, int* compressedSize)
    {
        *compressedSize =
                LZ4_compress_default((const char *)buffer, (char *)compressed, bufferSize, LZ4_compressBound(bufferSize));
        return DT_SUCCESS;
    }

    virtual dtStatus decompress(const unsigned char* compressed, const int compressedSize,
                                unsigned char* buffer, const int maxBufferSize, int* bufferSize)
    {
        *bufferSize = LZ4_decompress_safe((const char*)compressed, (char*)buffer, compressedSize, maxBufferSize);
        return *bufferSize < 0 ? DT_FAILURE : DT_SUCCESS;
    }
};

struct MeshProcess : public dtTileCacheMeshProcess
{
    DynamicNavigationMesh* owner_;
    std::vector<Vector3> offMeshVertices_;
    std::vector<float> offMeshRadii_;
    std::vector<unsigned short> offMeshFlags_;
    std::vector<unsigned char> offMeshAreas_;
    std::vector<unsigned char> offMeshDir_;

    inline MeshProcess(DynamicNavigationMesh* owner) :
        owner_(owner)
    {
    }

    virtual void process(struct dtNavMeshCreateParams* params, unsigned char* polyAreas, unsigned short* polyFlags)
    {
        // Update poly flags from areas.
        // \todo Assignment of flags from areas?
        for (int i = 0; i < params->polyCount; ++i)
        {
            if (polyAreas[i] != RC_NULL_AREA)
                polyFlags[i] = RC_WALKABLE_AREA;
        }

        BoundingBox bounds;
        rcVcopy(&bounds.min_.x_, params->bmin);
        rcVcopy(&bounds.max_.x_, params->bmin);

        // collect off-mesh connections
        std::vector<OffMeshConnection*> offMeshConnections = owner_->CollectOffMeshConnections(bounds);

        if (offMeshConnections.size() > 0)
        {
            if (offMeshConnections.size() != offMeshRadii_.size())
            {
                Matrix3x4 inverse = owner_->GetNode()->GetWorldTransform().Inverse();
                ClearConnectionData();
                for (unsigned i = 0; i < offMeshConnections.size(); ++i)
                {
                    OffMeshConnection* connection = offMeshConnections[i];
                    Vector3 start = inverse * connection->GetNode()->GetWorldPosition();
                    Vector3 end = inverse * connection->GetEndPoint()->GetWorldPosition();

                    offMeshVertices_.push_back(start);
                    offMeshVertices_.push_back(end);
                    offMeshRadii_.push_back(connection->GetRadius());
                    offMeshFlags_.push_back((unsigned short)connection->GetMask());
                    offMeshAreas_.push_back((unsigned char)connection->GetAreaID());
                    offMeshDir_.push_back((unsigned char)connection->IsBidirectional() ? DT_OFFMESH_CON_BIDIR : 0);
                }
            }
            params->offMeshConCount = offMeshRadii_.size();
            params->offMeshConVerts = &offMeshVertices_[0].x_;
            params->offMeshConRad = &offMeshRadii_[0];
            params->offMeshConFlags = &offMeshFlags_[0];
            params->offMeshConAreas = &offMeshAreas_[0];
            params->offMeshConDir = &offMeshDir_[0];
        }
    }

    void ClearConnectionData()
    {
        offMeshVertices_.clear();
        offMeshRadii_.clear();
        offMeshFlags_.clear();
        offMeshAreas_.clear();
        offMeshDir_.clear();
    }
};


// From the Detour/Recast Sample_TempObstacles.cpp
struct LinearAllocator : public dtTileCacheAlloc
{
    unsigned char* buffer;
    int capacity;
    int top;
    int high;

    LinearAllocator(const int cap) : buffer(0), capacity(0), top(0), high(0)
    {
        resize(cap);
    }

    ~LinearAllocator()
    {
        dtFree(buffer);
    }

    void resize(const int cap)
    {
        if (buffer)
            dtFree(buffer);
        buffer = (unsigned char*)dtAlloc(cap, DT_ALLOC_PERM);
        capacity = cap;
    }

    virtual void reset()
    {
        high = Max(high, top);
        top = 0;
    }

    virtual void* alloc(const int size)
    {
        if (!buffer)
            return 0;
        if (top + size > capacity)
            return 0;
        unsigned char* mem = &buffer[top];
        top += size;
        return mem;
    }

    virtual void free(void*)
    {
    }
};


DynamicNavigationMesh::DynamicNavigationMesh(Context* context) :
    NavigationMesh(context),
    tileCache_(0),
    allocator_(new LinearAllocator(32000)), //32kb to start
    compressor_(new TileCompressor()),
    meshProcessor_(new MeshProcess(this)),
    maxObstacles_(1024),
    maxLayers_(DEFAULT_MAX_LAYERS),
    drawObstacles_(false)
{
    //64 is the largest tile-size that DetourTileCache will tolerate without silently failing
    tileSize_ = 64;
    partitionType_ = NAVMESH_PARTITION_MONOTONE;
}

DynamicNavigationMesh::~DynamicNavigationMesh()
{
    ReleaseNavigationMesh();
}

void DynamicNavigationMesh::RegisterObject(Context* context)
{
    context->RegisterFactory<DynamicNavigationMesh>(NAVIGATION_CATEGORY);

    URHO3D_COPY_BASE_ATTRIBUTES(NavigationMesh);
    URHO3D_ACCESSOR_ATTRIBUTE("Max Obstacles", GetMaxObstacles, SetMaxObstacles, unsigned, DEFAULT_MAX_OBSTACLES, AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("Max Layers", GetMaxLayers, SetMaxLayers, unsigned, DEFAULT_MAX_LAYERS, AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("Draw Obstacles", GetDrawObstacles, SetDrawObstacles, bool, false, AM_DEFAULT);
}

bool DynamicNavigationMesh::Allocate(const BoundingBox& boundingBox, unsigned maxTiles)
{
    // Release existing navigation data and zero the bounding box
    ReleaseNavigationMesh();

    if (!node_)
        return false;

    if (!node_->GetWorldScale().Equals(Vector3::ONE))
        URHO3D_LOGWARNING("Navigation mesh root node has scaling. Agent parameters may not work as intended");

    boundingBox_ = boundingBox.Transformed(node_->GetWorldTransform().Inverse());
    maxTiles = NextPowerOfTwo(maxTiles);

    // Calculate number of tiles
    int gridW = 0, gridH = 0;
    float tileEdgeLength = (float)tileSize_ * cellSize_;
    rcCalcGridSize(&boundingBox_.min_.x_, &boundingBox_.max_.x_, cellSize_, &gridW, &gridH);
    numTilesX_ = (gridW + tileSize_ - 1) / tileSize_;
    numTilesZ_ = (gridH + tileSize_ - 1) / tileSize_;

    // Calculate max number of polygons, 22 bits available to identify both tile & polygon within tile
    unsigned tileBits = LogBaseTwo(maxTiles);
    unsigned maxPolys = (unsigned)(1 << (22 - tileBits));

    dtNavMeshParams params;
    rcVcopy(params.orig, &boundingBox_.min_.x_);
    params.tileWidth = tileEdgeLength;
    params.tileHeight = tileEdgeLength;
    params.maxTiles = maxTiles;
    params.maxPolys = maxPolys;

    navMesh_ = dtAllocNavMesh();
    if (!navMesh_)
    {
        URHO3D_LOGERROR("Could not allocate navigation mesh");
        return false;
    }

    if (dtStatusFailed(navMesh_->init(&params)))
    {
        URHO3D_LOGERROR("Could not initialize navigation mesh");
        ReleaseNavigationMesh();
        return false;
    }

    dtTileCacheParams tileCacheParams;
    memset(&tileCacheParams, 0, sizeof(tileCacheParams));
    rcVcopy(tileCacheParams.orig, &boundingBox_.min_.x_);
    tileCacheParams.ch = cellHeight_;
    tileCacheParams.cs = cellSize_;
    tileCacheParams.width = tileSize_;
    tileCacheParams.height = tileSize_;
    tileCacheParams.maxSimplificationError = edgeMaxError_;
    tileCacheParams.maxTiles = maxTiles * maxLayers_;
    tileCacheParams.maxObstacles = maxObstacles_;
    // Settings from NavigationMesh
    tileCacheParams.walkableClimb = agentMaxClimb_;
    tileCacheParams.walkableHeight = agentHeight_;
    tileCacheParams.walkableRadius = agentRadius_;

    tileCache_ = dtAllocTileCache();
    if (!tileCache_)
    {
        URHO3D_LOGERROR("Could not allocate tile cache");
        ReleaseNavigationMesh();
        return false;
    }

    if (dtStatusFailed(tileCache_->init(&tileCacheParams, allocator_.get(), compressor_.get(), meshProcessor_.get())))
    {
        URHO3D_LOGERROR("Could not initialize tile cache");
        ReleaseNavigationMesh();
        return false;
    }

    URHO3D_LOGDEBUG("Allocated empty navigation mesh with max " + QString::number(maxTiles) + " tiles");

    // Scan for obstacles to insert into us
    std::vector<Node*> obstacles;
    GetScene()->GetChildrenWithComponent<Obstacle>(obstacles, true);
    for (unsigned i = 0; i < obstacles.size(); ++i)
    {
        Obstacle* obs = obstacles[i]->GetComponent<Obstacle>();
        if (obs && obs->IsEnabledEffective())
            AddObstacle(obs);
    }

    // Send a notification event to concerned parties that we've been fully rebuilt
    navigationMeshRebuilt(node_,this);
    return true;
}

bool DynamicNavigationMesh::Build()
{
    URHO3D_PROFILE(BuildNavigationMesh);
    // Release existing navigation data and zero the bounding box
    ReleaseNavigationMesh();

    if (!node_)
        return false;

    if (!node_->GetWorldScale().Equals(Vector3::ONE))
        URHO3D_LOGWARNING("Navigation mesh root node has scaling. Agent parameters may not work as intended");

    std::vector<NavigationGeometryInfo> geometryList;
    CollectGeometries(geometryList);

    if (geometryList.empty())
        return true; // Nothing to do

    // Build the combined bounding box
    for (unsigned i = 0; i < geometryList.size(); ++i)
        boundingBox_.Merge(geometryList[i].boundingBox_);

    // Expand bounding box by padding
    boundingBox_.min_ -= padding_;
    boundingBox_.max_ += padding_;

    {
        URHO3D_PROFILE(BuildNavigationMesh);

        // Calculate number of tiles
        int gridW = 0, gridH = 0;
        float tileEdgeLength = (float)tileSize_ * cellSize_;
        rcCalcGridSize(&boundingBox_.min_.x_, &boundingBox_.max_.x_, cellSize_, &gridW, &gridH);
        numTilesX_ = (gridW + tileSize_ - 1) / tileSize_;
        numTilesZ_ = (gridH + tileSize_ - 1) / tileSize_;

        // Calculate max. number of tiles and polygons, 22 bits available to identify both tile & polygon within tile
        unsigned maxTiles = NextPowerOfTwo((unsigned)(numTilesX_ * numTilesZ_)) * maxLayers_;
        unsigned tileBits = 0;
        unsigned temp = maxTiles;
        while (temp > 1)
        {
            temp >>= 1;
            ++tileBits;
        }

        unsigned maxPolys = 1U << (22 - tileBits);

        dtNavMeshParams params;
        rcVcopy(params.orig, &boundingBox_.min_.x_);
        params.tileWidth = tileEdgeLength;
        params.tileHeight = tileEdgeLength;
        params.maxTiles = maxTiles;
        params.maxPolys = maxPolys;

        navMesh_ = dtAllocNavMesh();
        if (!navMesh_)
        {
            URHO3D_LOGERROR("Could not allocate navigation mesh");
            return false;
        }

        if (dtStatusFailed(navMesh_->init(&params)))
        {
            URHO3D_LOGERROR("Could not initialize navigation mesh");
            ReleaseNavigationMesh();
            return false;
        }

        dtTileCacheParams tileCacheParams;
        memset(&tileCacheParams, 0, sizeof(tileCacheParams));
        rcVcopy(tileCacheParams.orig, &boundingBox_.min_.x_);
        tileCacheParams.ch = cellHeight_;
        tileCacheParams.cs = cellSize_;
        tileCacheParams.width = tileSize_;
        tileCacheParams.height = tileSize_;
        tileCacheParams.maxSimplificationError = edgeMaxError_;
        tileCacheParams.maxTiles = numTilesX_ * numTilesZ_ * maxLayers_;
        tileCacheParams.maxObstacles = maxObstacles_;
        // Settings from NavigationMesh
        tileCacheParams.walkableClimb = agentMaxClimb_;
        tileCacheParams.walkableHeight = agentHeight_;
        tileCacheParams.walkableRadius = agentRadius_;

        tileCache_ = dtAllocTileCache();
        if (!tileCache_)
        {
            URHO3D_LOGERROR("Could not allocate tile cache");
            ReleaseNavigationMesh();
            return false;
        }

        if (dtStatusFailed(tileCache_->init(&tileCacheParams, allocator_.get(), compressor_.get(), meshProcessor_.get())))
        {
            URHO3D_LOGERROR("Could not initialize tile cache");
            ReleaseNavigationMesh();
            return false;
        }

        // Build each tile
        unsigned numTiles = 0;

        for (int z = 0; z < numTilesZ_; ++z)
        {
            for (int x = 0; x < numTilesX_; ++x)
            {
                TileCacheData tiles[TILECACHE_MAXLAYERS];
                int layerCt = BuildTile(geometryList, x, z, tiles);
                for (int i = 0; i < layerCt; ++i)
                {
                    dtCompressedTileRef tileRef;
                    int status = tileCache_->addTile(tiles[i].data, tiles[i].dataSize, DT_COMPRESSEDTILE_FREE_DATA, &tileRef);
                    if (dtStatusFailed((dtStatus)status))
                    {
                        dtFree(tiles[i].data);
                        tiles[i].data = 0x0;
                    }
                }
                tileCache_->buildNavMeshTilesAt(x, z, navMesh_);
                ++numTiles;
            }
        }

        // For a full build it's necessary to update the nav mesh
        // not doing so will cause dependent components to crash, like CrowdManager
        tileCache_->update(0, navMesh_);

        URHO3D_LOGDEBUG(QString("Built navigation mesh with %1 tiles").arg(numTiles));

        // Send a notification event to concerned parties that we've been fully rebuilt
        navigationMeshRebuilt(node_,this);

        // Scan for obstacles to insert into us
        std::vector<Node*> obstacles;
        GetScene()->GetChildrenWithComponent<Obstacle>(obstacles, true);
        for (unsigned i = 0; i < obstacles.size(); ++i)
        {
            Obstacle* obs = obstacles[i]->GetComponent<Obstacle>();
            if (obs && obs->IsEnabledEffective())
                AddObstacle(obs);
        }

        return true;
    }
}

bool DynamicNavigationMesh::Build(const BoundingBox& boundingBox)
{
    URHO3D_PROFILE(BuildPartialNavigationMesh);

    if (!node_)
        return false;

    if (!navMesh_)
    {
        URHO3D_LOGERROR("Navigation mesh must first be built fully before it can be partially rebuilt");
        return false;
    }

    if (!node_->GetWorldScale().Equals(Vector3::ONE))
        URHO3D_LOGWARNING("Navigation mesh root node has scaling. Agent parameters may not work as intended");

    BoundingBox localSpaceBox = boundingBox.Transformed(node_->GetWorldTransform().Inverse());

    float tileEdgeLength = (float)tileSize_ * cellSize_;

    std::vector<NavigationGeometryInfo> geometryList;
    CollectGeometries(geometryList);

    int sx = Clamp((int)((localSpaceBox.min_.x_ - boundingBox_.min_.x_) / tileEdgeLength), 0, numTilesX_ - 1);
    int sz = Clamp((int)((localSpaceBox.min_.z_ - boundingBox_.min_.z_) / tileEdgeLength), 0, numTilesZ_ - 1);
    int ex = Clamp((int)((localSpaceBox.max_.x_ - boundingBox_.min_.x_) / tileEdgeLength), 0, numTilesX_ - 1);
    int ez = Clamp((int)((localSpaceBox.max_.z_ - boundingBox_.min_.z_) / tileEdgeLength), 0, numTilesZ_ - 1);

    unsigned numTiles = BuildTiles(geometryList, IntVector2(sx, sz), IntVector2(ex, ez));

    URHO3D_LOGDEBUG("Rebuilt " + QString::number(numTiles) + " tiles of the navigation mesh");
    return true;
}

bool DynamicNavigationMesh::Build(const IntVector2& from, const IntVector2& to)
{
    URHO3D_PROFILE(BuildPartialNavigationMesh);

    if (!node_)
        return false;

    if (!navMesh_)
    {
        URHO3D_LOGERROR("Navigation mesh must first be built fully before it can be partially rebuilt");
        return false;
    }

    if (!node_->GetWorldScale().Equals(Vector3::ONE))
        URHO3D_LOGWARNING("Navigation mesh root node has scaling. Agent parameters may not work as intended");

    std::vector<NavigationGeometryInfo> geometryList;
    CollectGeometries(geometryList);

    unsigned numTiles = BuildTiles(geometryList, from, to);

    URHO3D_LOGDEBUG("Rebuilt " + QString::number(numTiles) + " tiles of the navigation mesh");
    return true;
}

std::vector<unsigned char> DynamicNavigationMesh::GetTileData(const IntVector2& tile) const
{
    VectorBuffer ret;
    WriteTiles(ret, tile.x_, tile.y_);
    return ret.GetBuffer();
}

bool DynamicNavigationMesh::IsObstacleInTile(Obstacle* obstacle, const IntVector2& tile) const
{
    const BoundingBox tileBoundingBox = GetTileBoundingBox(tile);
    const Vector3 obstaclePosition = obstacle->GetNode()->GetWorldPosition();
    return tileBoundingBox.DistanceToPoint(obstaclePosition) < obstacle->GetRadius();
}

bool DynamicNavigationMesh::AddTile(const std::vector<unsigned char>& tileData)
{
    MemoryBuffer buffer(tileData);
    return ReadTiles(buffer, false);
}

void DynamicNavigationMesh::RemoveTile(const IntVector2& tile)
{
    if (!navMesh_)
        return;
    dtCompressedTileRef existing[TILECACHE_MAXLAYERS];
    const int existingCt = tileCache_->getTilesAt(tile.x_, tile.y_, existing, maxLayers_);
    for (int i = 0; i < existingCt; ++i)
    {
        unsigned char* data = 0x0;
        if (!dtStatusFailed(tileCache_->removeTile(existing[i], &data, 0)) && data != 0x0)
            dtFree(data);
    }

    NavigationMesh::RemoveTile(tile);
}

void DynamicNavigationMesh::RemoveAllTiles()
{
    int numTiles = tileCache_->getTileCount();
    for (int i = 0; i < numTiles; ++i)
    {
        const dtCompressedTile* tile = tileCache_->getTile(i);
        assert(tile);
        if (tile->header)
            tileCache_->removeTile(tileCache_->getTileRef(tile), 0, 0);
    }

    NavigationMesh::RemoveAllTiles();
}


void DynamicNavigationMesh::DrawDebugGeometry(DebugRenderer* debug, bool depthTest)
{
    if (!debug || !navMesh_ || !node_)
        return;

    const Matrix3x4& worldTransform = node_->GetWorldTransform();

    const dtNavMesh* navMesh = navMesh_;

    for (int j = 0; j < navMesh->getMaxTiles(); ++j)
    {
        const dtMeshTile* tile = navMesh->getTile(j);
        assert(tile);
        if (!tile->header)
            continue;

        for (int i = 0; i < tile->header->polyCount; ++i)
        {
            dtPoly* poly = tile->polys + i;
            for (unsigned j = 0; j < poly->vertCount; ++j)
            {
                debug->AddLine(worldTransform * *reinterpret_cast<const Vector3*>(&tile->verts[poly->verts[j] * 3]),
                        worldTransform * *reinterpret_cast<const Vector3*>(&tile->verts[poly->verts[(j + 1) % poly->vertCount] * 3]),
                        Color::YELLOW, depthTest);
            }
        }
    }

    Scene* scene = GetScene();
    if (scene)
    {
        // Draw Obstacle components
        if (drawObstacles_)
        {
            std::vector<Node*> obstacles;
            scene->GetChildrenWithComponent<Obstacle>(obstacles, true);
            for (unsigned i = 0; i < obstacles.size(); ++i)
            {
                Obstacle* obstacle = obstacles[i]->GetComponent<Obstacle>();
                if (obstacle && obstacle->IsEnabledEffective())
                    obstacle->DrawDebugGeometry(debug, depthTest);
            }
        }

        // Draw OffMeshConnection components
        if (drawOffMeshConnections_)
        {
            std::vector<Node*> connections;
            scene->GetChildrenWithComponent<OffMeshConnection>(connections, true);
            for (unsigned i = 0; i < connections.size(); ++i)
            {
                OffMeshConnection* connection = connections[i]->GetComponent<OffMeshConnection>();
                if (connection && connection->IsEnabledEffective())
                    connection->DrawDebugGeometry(debug, depthTest);
            }
        }

        // Draw NavArea components
        if (drawNavAreas_)
        {
            std::vector<Node*> areas;
            scene->GetChildrenWithComponent<NavArea>(areas, true);
            for (unsigned i = 0; i < areas.size(); ++i)
            {
                NavArea* area = areas[i]->GetComponent<NavArea>();
                if (area && area->IsEnabledEffective())
                    area->DrawDebugGeometry(debug, depthTest);
            }
        }
    }
}

void DynamicNavigationMesh::DrawDebugGeometry(bool depthTest)
{
    Scene* scene = GetScene();
    if (scene)
    {
        DebugRenderer* debug = scene->GetComponent<DebugRenderer>();
        if (debug)
            DrawDebugGeometry(debug, depthTest);
    }
}

void DynamicNavigationMesh::SetNavigationDataAttr(const std::vector<unsigned char>& value)
{
    ReleaseNavigationMesh();

    if (value.empty())
        return;

    MemoryBuffer buffer(value);
    boundingBox_ = buffer.ReadBoundingBox();
    numTilesX_ = buffer.ReadInt();
    numTilesZ_ = buffer.ReadInt();

    dtNavMeshParams params;
    buffer.Read(&params, sizeof(dtNavMeshParams));

    navMesh_ = dtAllocNavMesh();
    if (!navMesh_)
    {
        URHO3D_LOGERROR("Could not allocate navigation mesh");
        return;
    }

    if (dtStatusFailed(navMesh_->init(&params)))
    {
        URHO3D_LOGERROR("Could not initialize navigation mesh");
        ReleaseNavigationMesh();
        return;
    }

    dtTileCacheParams tcParams;
    buffer.Read(&tcParams, sizeof(tcParams));

    tileCache_ = dtAllocTileCache();
    if (!tileCache_)
    {
        URHO3D_LOGERROR("Could not allocate tile cache");
        ReleaseNavigationMesh();
        return;
    }
    if (dtStatusFailed(tileCache_->init(&tcParams, allocator_.get(), compressor_.get(), meshProcessor_.get())))
    {
        URHO3D_LOGERROR("Could not initialize tile cache");
        ReleaseNavigationMesh();
        return;
    }

    ReadTiles(buffer, true);
    // \todo Shall we send E_NAVIGATION_MESH_REBUILT here?
}

std::vector<unsigned char> DynamicNavigationMesh::GetNavigationDataAttr() const
{
    VectorBuffer ret;
    if (navMesh_ && tileCache_)
    {
        ret.WriteBoundingBox(boundingBox_);
        ret.WriteInt(numTilesX_);
        ret.WriteInt(numTilesZ_);

        const dtNavMeshParams* params = navMesh_->getParams();
        ret.Write(params, sizeof(dtNavMeshParams));

        const dtTileCacheParams* tcParams = tileCache_->getParams();
        ret.Write(tcParams, sizeof(dtTileCacheParams));

        for (int z = 0; z < numTilesZ_; ++z)
            for (int x = 0; x < numTilesX_; ++x)
                WriteTiles(ret, x, z);
    }
    return ret.GetBuffer();
}

void DynamicNavigationMesh::SetMaxLayers(unsigned maxLayers)
{
    // Set 3 as a minimum due to the tendency of layers to be constructed inside the hollow space of stacked objects
    // That behavior is unlikely to be expected by the end user
    maxLayers_ = Max(3U, Min(maxLayers, TILECACHE_MAXLAYERS));
}

void DynamicNavigationMesh::WriteTiles(Serializer& dest, int x, int z) const
{
    dtCompressedTileRef tiles[TILECACHE_MAXLAYERS];
    const int ct = tileCache_->getTilesAt(x, z, tiles, maxLayers_);
    for (int i = 0; i < ct; ++i)
    {
        const dtCompressedTile* tile = tileCache_->getTileByRef(tiles[i]);
        if (!tile || !tile->header || !tile->dataSize)
            continue; // Don't write "void-space" tiles
        // The header conveniently has the majority of the information required
        dest.Write(tile->header, sizeof(dtTileCacheLayerHeader));
        dest.WriteInt(tile->dataSize);
        dest.Write(tile->data, (unsigned)tile->dataSize);
    }
}
bool DynamicNavigationMesh::ReadTiles(Deserializer& source, bool silent)
{
    tileQueue_.clear();
    while (!source.IsEof())
    {
        dtTileCacheLayerHeader header;
        source.Read(&header, sizeof(dtTileCacheLayerHeader));
        const int dataSize = source.ReadInt();

        unsigned char* data = (unsigned char*)dtAlloc(dataSize, DT_ALLOC_PERM);
        if (!data)
        {
            URHO3D_LOGERROR("Could not allocate data for navigation mesh tile");
            return false;
        }
        source.Read(data, (unsigned)dataSize);
        if (dtStatusFailed(tileCache_->addTile(data, dataSize, DT_TILE_FREE_DATA, 0)))
        {
            URHO3D_LOGERROR("Failed to add tile");
            dtFree(data);
            return false;
        }
        const IntVector2 tileIdx = IntVector2(header.tx, header.ty);
        if (tileQueue_.empty() || tileQueue_.back() != tileIdx)
            tileQueue_.push_back(tileIdx);
    }

    for (unsigned i = 0; i < tileQueue_.size(); ++i)
        tileCache_->buildNavMeshTilesAt(tileQueue_[i].x_, tileQueue_[i].y_, navMesh_);

    tileCache_->update(0, navMesh_);

    // Send event
    if (!silent)
    {
        for (unsigned i = 0; i < tileQueue_.size(); ++i)
        {
            navigationTileRebuilt(GetNode(),this,tileQueue_[i]);
        }
    }
    return true;
}

int DynamicNavigationMesh::BuildTile(std::vector<NavigationGeometryInfo>& geometryList, int x, int z, TileCacheData* tiles)
{
    URHO3D_PROFILE(BuildNavigationMeshTile);

    tileCache_->removeTile(navMesh_->getTileRefAt(x, z, 0), 0, 0);

    const BoundingBox tileBoundingBox = GetTileBoundingBox(IntVector2(x, z));

    DynamicNavBuildData build(allocator_.get());

    rcConfig cfg;
    memset(&cfg, 0, sizeof cfg);
    cfg.cs = cellSize_;
    cfg.ch = cellHeight_;
    cfg.walkableSlopeAngle = agentMaxSlope_;
    cfg.walkableHeight = (int)ceilf(agentHeight_ / cfg.ch);
    cfg.walkableClimb = (int)floorf(agentMaxClimb_ / cfg.ch);
    cfg.walkableRadius = (int)ceilf(agentRadius_ / cfg.cs);
    cfg.maxEdgeLen = (int)(edgeMaxLength_ / cellSize_);
    cfg.maxSimplificationError = edgeMaxError_;
    cfg.minRegionArea = (int)sqrtf(regionMinSize_);
    cfg.mergeRegionArea = (int)sqrtf(regionMergeSize_);
    cfg.maxVertsPerPoly = 6;
    cfg.tileSize = tileSize_;
    cfg.borderSize = cfg.walkableRadius + 3; // Add padding
    cfg.width = cfg.tileSize + cfg.borderSize * 2;
    cfg.height = cfg.tileSize + cfg.borderSize * 2;
    cfg.detailSampleDist = detailSampleDistance_ < 0.9f ? 0.0f : cellSize_ * detailSampleDistance_;
    cfg.detailSampleMaxError = cellHeight_ * detailSampleMaxError_;

    rcVcopy(cfg.bmin, &tileBoundingBox.min_.x_);
    rcVcopy(cfg.bmax, &tileBoundingBox.max_.x_);
    cfg.bmin[0] -= cfg.borderSize * cfg.cs;
    cfg.bmin[2] -= cfg.borderSize * cfg.cs;
    cfg.bmax[0] += cfg.borderSize * cfg.cs;
    cfg.bmax[2] += cfg.borderSize * cfg.cs;

    BoundingBox expandedBox(*reinterpret_cast<Vector3*>(cfg.bmin), *reinterpret_cast<Vector3*>(cfg.bmax));
    GetTileGeometry(&build, geometryList, expandedBox);

    if (build.vertices_.empty() || build.indices_.empty())
        return 0; // Nothing to do

    build.heightField_ = rcAllocHeightfield();
    if (!build.heightField_)
    {
        URHO3D_LOGERROR("Could not allocate heightfield");
        return 0;
    }

    if (!rcCreateHeightfield(build.ctx_, *build.heightField_, cfg.width, cfg.height, cfg.bmin, cfg.bmax, cfg.cs,
                             cfg.ch))
    {
        URHO3D_LOGERROR("Could not create heightfield");
        return 0;
    }

    size_t numTriangles = build.indices_.size() / 3;
    std::unique_ptr<uint8_t[]> triAreas(new unsigned char[numTriangles]);
    memset(triAreas.get(), 0, numTriangles);

    rcMarkWalkableTriangles(build.ctx_, cfg.walkableSlopeAngle, &build.vertices_[0].x_, build.vertices_.size(),
            &build.indices_[0], numTriangles, triAreas.get());
    rcRasterizeTriangles(build.ctx_, &build.vertices_[0].x_, build.vertices_.size(), &build.indices_[0],
            triAreas.get(), numTriangles, *build.heightField_, cfg.walkableClimb);
    rcFilterLowHangingWalkableObstacles(build.ctx_, cfg.walkableClimb, *build.heightField_);

    rcFilterLedgeSpans(build.ctx_, cfg.walkableHeight, cfg.walkableClimb, *build.heightField_);
    rcFilterWalkableLowHeightSpans(build.ctx_, cfg.walkableHeight, *build.heightField_);

    build.compactHeightField_ = rcAllocCompactHeightfield();
    if (!build.compactHeightField_)
    {
        URHO3D_LOGERROR("Could not allocate create compact heightfield");
        return 0;
    }
    if (!rcBuildCompactHeightfield(build.ctx_, cfg.walkableHeight, cfg.walkableClimb, *build.heightField_,
                                   *build.compactHeightField_))
    {
        URHO3D_LOGERROR("Could not build compact heightfield");
        return 0;
    }
    if (!rcErodeWalkableArea(build.ctx_, cfg.walkableRadius, *build.compactHeightField_))
    {
        URHO3D_LOGERROR("Could not erode compact heightfield");
        return 0;
    }

    // area volumes
    for (unsigned i = 0; i < build.navAreas_.size(); ++i)
        rcMarkBoxArea(build.ctx_, &build.navAreas_[i].bounds_.min_.x_, &build.navAreas_[i].bounds_.max_.x_, build.navAreas_[i].areaID_, *build.compactHeightField_);

    if (this->partitionType_ == NAVMESH_PARTITION_WATERSHED)
    {
        if (!rcBuildDistanceField(build.ctx_, *build.compactHeightField_))
        {
            URHO3D_LOGERROR("Could not build distance field");
            return 0;
        }
        if (!rcBuildRegions(build.ctx_, *build.compactHeightField_, cfg.borderSize, cfg.minRegionArea,
                            cfg.mergeRegionArea))
        {
            URHO3D_LOGERROR("Could not build regions");
            return 0;
        }
    }
    else
    {
        if (!rcBuildRegionsMonotone(build.ctx_, *build.compactHeightField_, cfg.borderSize, cfg.minRegionArea, cfg.mergeRegionArea))
        {
            URHO3D_LOGERROR("Could not build monotone regions");
            return 0;
        }
    }

    build.heightFieldLayers_ = rcAllocHeightfieldLayerSet();
    if (!build.heightFieldLayers_)
    {
        URHO3D_LOGERROR("Could not allocate height field layer set");
        return 0;
    }

    if (!rcBuildHeightfieldLayers(build.ctx_, *build.compactHeightField_, cfg.borderSize, cfg.walkableHeight, *build.heightFieldLayers_))
    {
        URHO3D_LOGERROR("Could not build height field layers");
        return 0;
    }

    int retCt = 0;
    for (int i = 0; i < build.heightFieldLayers_->nlayers; ++i)
    {
        dtTileCacheLayerHeader header;
        header.magic = DT_TILECACHE_MAGIC;
        header.version = DT_TILECACHE_VERSION;
        header.tx = x;
        header.ty = z;
        header.tlayer = i;

        rcHeightfieldLayer* layer = &build.heightFieldLayers_->layers[i];

        // Tile info.
        rcVcopy(header.bmin, layer->bmin);
        rcVcopy(header.bmax, layer->bmax);
        header.width = (unsigned char)layer->width;
        header.height = (unsigned char)layer->height;
        header.minx = (unsigned char)layer->minx;
        header.maxx = (unsigned char)layer->maxx;
        header.miny = (unsigned char)layer->miny;
        header.maxy = (unsigned char)layer->maxy;
        header.hmin = (unsigned short)layer->hmin;
        header.hmax = (unsigned short)layer->hmax;

        if (dtStatusFailed(dtBuildTileCacheLayer(compressor_.get(), &header, layer->heights, layer->areas, layer->cons, &(tiles[retCt].data), &tiles[retCt].dataSize)))
        {
            URHO3D_LOGERROR("Failed to build tile cache layers");
            return 0;
        }
        else
            ++retCt;
    }

    // Send a notification of the rebuild of this tile to anyone interested
    navigationAreaRebuilt(node_,this,tileBoundingBox.min_,tileBoundingBox.max_);

    return retCt;
}

unsigned DynamicNavigationMesh::BuildTiles(std::vector<NavigationGeometryInfo>& geometryList, const IntVector2& from, const IntVector2& to)
{
    unsigned numTiles = 0;

    for (int z = from.y_; z <= to.y_; ++z)
    {
        for (int x = from.x_; x <= to.x_; ++x)
        {
            dtCompressedTileRef existing[TILECACHE_MAXLAYERS];
            const int existingCt = tileCache_->getTilesAt(x, z, existing, maxLayers_);
            for (int i = 0; i < existingCt; ++i)
            {
                unsigned char* data = 0x0;
                if (!dtStatusFailed(tileCache_->removeTile(existing[i], &data, 0)) && data != 0x0)
                    dtFree(data);
            }

            TileCacheData tiles[TILECACHE_MAXLAYERS];
            int layerCt = BuildTile(geometryList, x, z, tiles);
            for (int i = 0; i < layerCt; ++i)
            {
                dtCompressedTileRef tileRef;
                int status = tileCache_->addTile(tiles[i].data, tiles[i].dataSize, DT_COMPRESSEDTILE_FREE_DATA, &tileRef);
                if (dtStatusFailed((dtStatus)status))
                {
                    dtFree(tiles[i].data);
                    tiles[i].data = 0x0;
                }
                else
                {
                    tileCache_->buildNavMeshTile(tileRef, navMesh_);
                    ++numTiles;
                }
            }
        }
    }

    return numTiles;
}

std::vector<OffMeshConnection*> DynamicNavigationMesh::CollectOffMeshConnections(const BoundingBox& bounds)
{
    std::vector<OffMeshConnection*> connections;
    node_->GetComponents<OffMeshConnection>(connections, true);
    for (auto i = connections.begin(), fin=connections.end(); i != fin; )
    {
        OffMeshConnection* connection = *i;
        if (!(connection->IsEnabledEffective() && connection->GetEndPoint()))
        {
            // discard this connection
            i = connections.erase(i);
        }
        else
            ++i;
    }

    return connections;
}

void DynamicNavigationMesh::ReleaseNavigationMesh()
{
    NavigationMesh::ReleaseNavigationMesh();
    ReleaseTileCache();
}

void DynamicNavigationMesh::ReleaseTileCache()
{
    dtFreeTileCache(tileCache_);
    tileCache_ = 0;
}

void DynamicNavigationMesh::OnSceneSet(Scene* scene)
{
    // Subscribe to the scene subsystem update, which will trigger the tile cache to update the nav mesh
    if (scene)
        scene->sceneSubsystemUpdate.Connect(this,&DynamicNavigationMesh::HandleSceneSubsystemUpdate);
    else {
        if(GetScene())
            GetScene()->sceneSubsystemUpdate.Disconnect(this);
    }
}

void DynamicNavigationMesh::AddObstacle(Obstacle* obstacle, bool silent)
{
    if (tileCache_)
    {
        float pos[3];
        Vector3 obsPos = obstacle->GetNode()->GetWorldPosition();
        rcVcopy(pos, &obsPos.x_);
        dtObstacleRef refHolder;

        // Because dtTileCache doesn't process obstacle requests while updating tiles
        // it's necessary update until sufficient request space is available
        while (tileCache_->isObstacleQueueFull())
            tileCache_->update(1, navMesh_);

        if (dtStatusFailed(tileCache_->addObstacle(pos, obstacle->GetRadius(), obstacle->GetHeight(), &refHolder)))
        {
            URHO3D_LOGERROR("Failed to add obstacle");
            return;
        }
        obstacle->obstacleId_ = refHolder;
        assert(refHolder > 0);

        if (!silent)
        {
            using namespace NavigationObstacleAdded;
            VariantMap& eventData = GetContext()->GetEventDataMap();
            eventData[P_NODE] = obstacle->GetNode();
            eventData[P_OBSTACLE] = obstacle;
            eventData[P_POSITION] = obstacle->GetNode()->GetWorldPosition();
            eventData[P_RADIUS] = obstacle->GetRadius();
            eventData[P_HEIGHT] = obstacle->GetHeight();
            SendEvent(E_NAVIGATION_OBSTACLE_ADDED, eventData);
        }
    }
}

void DynamicNavigationMesh::ObstacleChanged(Obstacle* obstacle)
{
    if (tileCache_)
    {
        RemoveObstacle(obstacle, true);
        AddObstacle(obstacle, true);
    }
}

void DynamicNavigationMesh::RemoveObstacle(Obstacle* obstacle, bool silent)
{
    if (tileCache_ && obstacle->obstacleId_ > 0)
    {
        // Because dtTileCache doesn't process obstacle requests while updating tiles
        // it's necessary update until sufficient request space is available
        while (tileCache_->isObstacleQueueFull())
            tileCache_->update(1, navMesh_);

        if (dtStatusFailed(tileCache_->removeObstacle(obstacle->obstacleId_)))
        {
            URHO3D_LOGERROR("Failed to remove obstacle");
            return;
        }
        obstacle->obstacleId_ = 0;
        // Require a node in order to send an event
        if (!silent && obstacle->GetNode())
        {
            using namespace NavigationObstacleRemoved;
            VariantMap& eventData = GetContext()->GetEventDataMap();
            eventData[P_NODE] = obstacle->GetNode();
            eventData[P_OBSTACLE] = obstacle;
            eventData[P_POSITION] = obstacle->GetNode()->GetWorldPosition();
            eventData[P_RADIUS] = obstacle->GetRadius();
            eventData[P_HEIGHT] = obstacle->GetHeight();
            SendEvent(E_NAVIGATION_OBSTACLE_REMOVED, eventData);
        }
    }
}

void DynamicNavigationMesh::HandleSceneSubsystemUpdate(Scene *,float ts)
{
    if (tileCache_ && navMesh_ && IsEnabledEffective())
        tileCache_->update(ts, navMesh_);
}

}
