//
// Copyright (c) 2008-2016 the Urho3D project.
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

#include "Font.h"

#include "Lutefisk3D/Core/Context.h"
#include "Lutefisk3D/IO/Deserializer.h"
#include "Lutefisk3D/IO/FileSystem.h"
#include "FontFaceBitmap.h"
#include "FontFaceFreeType.h"
#include "Lutefisk3D/Graphics/Graphics.h"
#include "Lutefisk3D/Core/Profiler.h"
#include "Lutefisk3D/Resource/ResourceCache.h"
#include "Lutefisk3D/Resource/XMLElement.h"
#include "Lutefisk3D/Resource/XMLFile.h"

namespace Urho3D
{

static const int MIN_POINT_SIZE = 1;
static const int MAX_POINT_SIZE = 96;

Font::Font(Context* context) :
    Resource(context),
    fontDataSize_(0),
    absoluteOffset_(IntVector2::ZERO),
    scaledOffset_(Vector2::ZERO),
    fontType_(FONT_NONE),
    sdfFont_(false)
{
}

Font::~Font()
{
    // To ensure FreeType deallocates properly, first clear all faces, then release the raw font data
    ReleaseFaces();
    fontData_.Reset();
}

void Font::RegisterObject(Context* context)
{
    context->RegisterFactory<Font>();
}

bool Font::BeginLoad(Deserializer& source)
{
    // In headless mode, do not actually load, just return success
    Graphics* graphics = GetSubsystem<Graphics>();
    if (!graphics)
        return true;

    fontType_ = FONT_NONE;
    faces_.clear();

    fontDataSize_ = source.GetSize();
    if (fontDataSize_)
    {
        fontData_ = new unsigned char[fontDataSize_];
        if (source.Read(&fontData_[0], fontDataSize_) != fontDataSize_)
            return false;
    }
    else
    {
        fontData_.Reset();
        return false;
    }

    QString ext = GetExtension(GetName());
    if (ext == ".ttf" || ext == ".otf" || ext == ".woff")
    {
        fontType_ = FONT_FREETYPE;
        LoadParameters();
    }
    else if (ext == ".xml" || ext == ".fnt" || ext == ".sdf")
        fontType_ = FONT_BITMAP;

    sdfFont_ = ext == ".sdf";

    SetMemoryUse(fontDataSize_);
    return true;
}

bool Font::SaveXML(Serializer& dest, int pointSize, bool usedGlyphs, const QString& indentation)
{
    FontFace* fontFace = GetFace(pointSize);
    if (!fontFace)
        return false;

    URHO3D_PROFILE(FontSaveXML);

    SharedPtr<FontFaceBitmap> packedFontFace(new FontFaceBitmap(this));
    if (!packedFontFace->Load(fontFace, usedGlyphs))
        return false;

    return packedFontFace->Save(dest, pointSize, indentation);
}

void Font::SetAbsoluteGlyphOffset(const IntVector2& offset)
{
    absoluteOffset_ = offset;
}

void Font::SetScaledGlyphOffset(const Vector2& offset)
{
    scaledOffset_ = offset;
}

FontFace* Font::GetFace(int pointSize)
{
    // In headless mode, always return null
    Graphics* graphics = GetSubsystem<Graphics>();
    if (!graphics)
        return nullptr;

    // For bitmap font type, always return the same font face provided by the font's bitmap file regardless of the actual requested point size
    if (fontType_ == FONT_BITMAP)
        pointSize = 0;
    else
        pointSize = Clamp(pointSize, MIN_POINT_SIZE, MAX_POINT_SIZE);

    HashMap<int, SharedPtr<FontFace> >::iterator i = faces_.find(pointSize);
    if (i != faces_.end())
    {
        if (!MAP_VALUE(i)->IsDataLost())
            return MAP_VALUE(i);
        else
        {
            // Erase and reload face if texture data lost (OpenGL mode only)
            faces_.erase(i);
        }
    }

    URHO3D_PROFILE(GetFontFace);

    switch (fontType_)
    {
    case FONT_FREETYPE:
        return GetFaceFreeType(pointSize);

    case FONT_BITMAP:
        return GetFaceBitmap(pointSize);

    default:
        return nullptr;
    }
}

IntVector2 Font::GetTotalGlyphOffset(int pointSize) const
{
    Vector2 multipliedOffset = (float)pointSize * scaledOffset_;
    return absoluteOffset_ + IntVector2((int)multipliedOffset.x_, (int)multipliedOffset.y_);
}

void Font::ReleaseFaces()
{
    faces_.clear();
}

void Font::LoadParameters()
{
    ResourceCache* cache = GetSubsystem<ResourceCache>();
    QString xmlName = ReplaceExtension(GetName(), ".xml");
    SharedPtr<XMLFile> xml = cache->GetTempResource<XMLFile>(xmlName, false);
    if (!xml)
        return;

    XMLElement rootElem = xml->GetRoot();

    XMLElement absoluteElem = rootElem.GetChild("absoluteoffset");
    if (!absoluteElem)
        absoluteElem = rootElem.GetChild("absolute");

    if (absoluteElem)
    {
        absoluteOffset_.x_ = absoluteElem.GetInt("x");
        absoluteOffset_.y_ = absoluteElem.GetInt("y");
    }

    XMLElement scaledElem = rootElem.GetChild("scaledoffset");
    if (!scaledElem)
        scaledElem = rootElem.GetChild("scaled");

    if (scaledElem)
    {
        scaledOffset_.x_ = scaledElem.GetFloat("x");
        scaledOffset_.y_ = scaledElem.GetFloat("y");
    }
}

FontFace* Font::GetFaceFreeType(int pointSize)
{
    SharedPtr<FontFace> newFace(new FontFaceFreeType(this));
    if (!newFace->Load(&fontData_[0], fontDataSize_, pointSize))
        return nullptr;

    faces_[pointSize] = newFace;
    return newFace;
}

FontFace* Font::GetFaceBitmap(int pointSize)
{
    SharedPtr<FontFace> newFace(new FontFaceBitmap(this));
    if (!newFace->Load(&fontData_[0], fontDataSize_, pointSize))
        return nullptr;

    faces_[pointSize] = newFace;
    return newFace;
}

}
