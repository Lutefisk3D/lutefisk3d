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

#include "Lutefisk3D/Resource/Resource.h"
#include "Lutefisk3D/2D/TileMapDefs2D.h"

namespace Urho3D
{

class Sprite2D;
class Texture2D;
class TmxFile2D;
class XMLElement;
class XMLFile;

/// Tmx layer.
class TmxLayer2D : public RefCounted
{
public:
    TmxLayer2D(TmxFile2D* tmxFile, TileMapLayerType2D type);
    virtual ~TmxLayer2D();

    /// Return tmx file.
    TmxFile2D* GetTmxFile() const;
    /// Return type.
    TileMapLayerType2D GetType() const { return type_; }
    /// Return name.
    const QString& GetName() const { return name_; }
    /// Return width.
    int GetWidth() const { return width_; }
    /// Return height.
    int GetHeight() const { return height_; }
    /// Return is visible.
    bool IsVisible() const { return visible_; }
    /// Return has property (use for script).
    bool HasProperty(const QString& name) const;
    /// Return property value (use for script).
    const QString& GetProperty(const QString& name) const;


protected:
    /// Load layer info.
    void LoadInfo(const XMLElement& element);
    /// Load property set.
    void LoadPropertySet(const XMLElement& element);

    /// Tmx file.
    WeakPtr<TmxFile2D> tmxFile_;
    /// Layer type.
    TileMapLayerType2D type_;
    /// Name.
    QString name_;
    /// Width.
    int width_;
    /// Height.
    int height_;
    /// Visible.
    bool visible_;
    /// Property set.
    SharedPtr<PropertySet2D> propertySet_;
};

/// Tmx tile layer.
class TmxTileLayer2D : public TmxLayer2D
{
public:
    TmxTileLayer2D(TmxFile2D* tmxFile);

    /// Load from XML element.
    bool Load(const XMLElement& element, const TileMapInfo2D& info);
    /// Return tile.
    Tile2D* GetTile(int x, int y) const;

protected:
    /// Tiles.
    std::vector<SharedPtr<Tile2D> > tiles_;
};

/// Tmx objects layer.
class TmxObjectGroup2D : public TmxLayer2D
{
public:
    TmxObjectGroup2D(TmxFile2D* tmxFile);

    /// Load from XML element.
    bool Load(const XMLElement& element, const TileMapInfo2D& info);
    /// Store object.
    void StoreObject(XMLElement objectElem, SharedPtr<TileMapObject2D> object, const TileMapInfo2D& info, bool isTile = false);
    /// Return number of objects.
    size_t GetNumObjects() const { return objects_.size(); }
    /// Return tile map object at index.
    TileMapObject2D* GetObject(unsigned index) const;

private:
    /// Objects.
    std::vector<SharedPtr<TileMapObject2D> > objects_;
};

/// Tmx image layer.
class TmxImageLayer2D : public TmxLayer2D
{
public:
    TmxImageLayer2D(TmxFile2D* tmxFile);

    /// Load from XML element.
    bool Load(const XMLElement& element, const TileMapInfo2D& info);
    /// Return position.
    const Vector2& GetPosition() const { return position_; }
    /// Return source.
    const QString& GetSource() const { return source_; }
    /// Return sprite.
    Sprite2D* GetSprite() const;

private:
    /// Position.
    Vector2 position_;
    /// Source.
    QString source_;
    /// Sprite.
    SharedPtr<Sprite2D> sprite_;
};

/// Tile map file.
class LUTEFISK3D_EXPORT TmxFile2D : public Resource
{
    URHO3D_OBJECT(TmxFile2D,Resource);

public:
    /// Construct.
    TmxFile2D(Context* context);
    /// Destruct.
    virtual ~TmxFile2D();
    /// Register object factory.
    static void RegisterObject(Context* context);

    /// Load resource from stream. May be called from a worker thread. Return true if successful.
    virtual bool BeginLoad(Deserializer& source) override;
    /// Finish resource loading. Always called from the main thread. Return true if successful.
    virtual bool EndLoad() override;

    /// Set Tilemap information.
    bool SetInfo(Orientation2D orientation, int width, int height, float tileWidth, float tileHeight);

    /// Add layer at index, if index > number of layers then append to end.
    void AddLayer(unsigned index, TmxLayer2D *layer);

    /// Append layer to end.
    void AddLayer(Urho3D::TmxLayer2D* layer);

    /// Return Tilemap information.
    const TileMapInfo2D& GetInfo() const { return info_; }

    /// Return tile sprite by gid, if not exist return 0.
    Sprite2D* GetTileSprite(int gid) const;

    /// Return tile collision shapes for a given gid.
    std::vector<SharedPtr<TileMapObject2D> > GetTileCollisionShapes(int gid) const;
    /// Return tile property set by gid, if not exist return 0.
    PropertySet2D* GetTilePropertySet(int gid) const;
    /// Return number of layers.
    size_t GetNumLayers() const { return layers_.size(); }
    /// Return layer at index.
    const TmxLayer2D* GetLayer(unsigned index) const;

private:
    /// Load TSX file.
    SharedPtr<XMLFile> LoadTSXFile(const QString& source);
    /// Load tile set.
    bool LoadTileSet(const XMLElement& element);

    /// XML file used during loading.
    std::unique_ptr<XMLFile> loadXMLFile_;
    /// TSX name to XML file mapping.
    HashMap<QString, SharedPtr<XMLFile> > tsxXMLFiles_;
    /// Tile map information.
    TileMapInfo2D info_;
    /// Gid to tile sprite mapping.
    HashMap<int, SharedPtr<Sprite2D> > gidToSpriteMapping_;
    /// Gid to tile property set mapping.
    HashMap<int, SharedPtr<PropertySet2D> > gidToPropertySetMapping_;
    /// Gid to tile collision shape mapping.
    HashMap<int, std::vector<SharedPtr<TileMapObject2D> > > gidToCollisionShapeMapping_;
    /// Layers.
    std::vector<TmxLayer2D*> layers_;
};

}

