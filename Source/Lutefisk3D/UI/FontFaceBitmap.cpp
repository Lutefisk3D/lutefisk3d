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

#include "FontFaceBitmap.h"

#include "Font.h"
#include "UI.h"
#include "Lutefisk3D/Core/Context.h"
#include "Lutefisk3D/IO/File.h"
#include "Lutefisk3D/IO/FileSystem.h"
#include "Lutefisk3D/Graphics/Graphics.h"
#include "Lutefisk3D/Resource/Image.h"
#include "Lutefisk3D/IO/Log.h"
#include "Lutefisk3D/IO/MemoryBuffer.h"
#include "Lutefisk3D/Resource/ResourceCache.h"
#include "Lutefisk3D/Graphics/Texture2D.h"
#include "Lutefisk3D/Resource/XMLFile.h"

namespace Urho3D
{

FontFaceBitmap::FontFaceBitmap(Font* font) :
    FontFace(font)
{
}

FontFaceBitmap::~FontFaceBitmap()
{
}

bool FontFaceBitmap::Load(const unsigned char* fontData, unsigned fontDataSize, float pointSize)
{
    Context* context = font_->GetContext();

    SharedPtr<XMLFile> xmlReader(new XMLFile(context));
    MemoryBuffer memoryBuffer(fontData, fontDataSize);
    if (!xmlReader->Load(memoryBuffer))
    {
        URHO3D_LOGERROR("Could not load XML file");
        return false;
    }

    XMLElement root = xmlReader->GetRoot("font");
    if (root.IsNull())
    {
        URHO3D_LOGERROR("Could not find Font element");
        return false;
    }

    XMLElement pagesElem = root.GetChild("pages");
    if (pagesElem.IsNull())
    {
        URHO3D_LOGERROR("Could not find Pages element");
        return false;
    }

    XMLElement infoElem = root.GetChild("info");
    if (!infoElem.IsNull())
        pointSize_ = infoElem.GetInt("size");

    XMLElement commonElem = root.GetChild("common");
    rowHeight_ = commonElem.GetInt("lineHeight");
    unsigned pages = commonElem.GetUInt("pages");
    textures_.reserve(pages);

    ResourceCache* resourceCache = font_->GetContext()->m_ResourceCache.get();
    QString fontPath = GetPath(font_->GetName());
    unsigned totalTextureSize = 0;

    XMLElement pageElem = pagesElem.GetChild("page");
    for (unsigned i = 0; i < pages; ++i)
    {
        if (pageElem.IsNull())
        {
            URHO3D_LOGERROR("Could not find Page element for page: " + QString::number(i));
            return 0;
        }

        // Assume the font image is in the same directory as the font description file
        QString textureFile = fontPath + pageElem.GetAttribute("file");

        // Load texture manually to allow controlling the alpha channel mode
        std::unique_ptr<File> fontFile = resourceCache->GetFile(textureFile);
        SharedPtr<Image> fontImage(new Image(context));
        if (!fontFile || !fontImage->Load(*fontFile))
        {
            URHO3D_LOGERROR("Failed to load font image file");
            return 0;
        }
        SharedPtr<Texture2D> texture = LoadFaceTexture(fontImage);
        if (!texture)
            return 0;

        textures_.push_back(texture);

        // Add texture to resource cache
        texture->SetName(fontFile->GetName());
        resourceCache->AddManualResource(texture);

        totalTextureSize += fontImage->GetWidth() * fontImage->GetHeight() * fontImage->GetComponents();

        pageElem = pageElem.GetNext("page");
    }

    XMLElement charsElem = root.GetChild("chars");
    int count = charsElem.GetInt("count");

    XMLElement charElem = charsElem.GetChild("char");
    while (!charElem.IsNull())
    {
        int id = charElem.GetInt("id");

        FontGlyph glyph;
        glyph.x_ = (short)charElem.GetInt("x");
        glyph.y_ = (short)charElem.GetInt("y");
        glyph.width_ = glyph.texWidth_ = (short)charElem.GetInt("width");
        glyph.height_ = glyph.texHeight_ = (short)charElem.GetInt("height");
        glyph.offsetX_ = (short)charElem.GetInt("xoffset");
        glyph.offsetY_ = (short)charElem.GetInt("yoffset");
        glyph.advanceX_ = (short)charElem.GetInt("xadvance");
        glyph.page_ = charElem.GetUInt("page");

        glyphMapping_[id] = glyph;

        charElem = charElem.GetNext("char");
    }

    XMLElement kerningsElem = root.GetChild("kernings");
    if (kerningsElem.NotNull())
    {
        XMLElement kerningElem = kerningsElem.GetChild("kerning");
        while (!kerningElem.IsNull())
        {
            int first = kerningElem.GetInt("first");
            int second = kerningElem.GetInt("second");
            unsigned value = (first << 16) + second;
            kerningMapping_[value] = (short)kerningElem.GetInt("amount");

            kerningElem = kerningElem.GetNext("kerning");
        }
    }

    URHO3D_LOGDEBUG(QString("Bitmap font face %1 has %2 glyphs").arg(GetFileName(font_->GetName())).arg(count));

    font_->SetMemoryUse(font_->GetMemoryUse() + totalTextureSize);
    return true;
}

bool FontFaceBitmap::Load(FontFace* fontFace, bool usedGlyphs)
{
    if (this == fontFace)
        return true;

    if (!usedGlyphs)
    {
        glyphMapping_ = fontFace->glyphMapping_;
        kerningMapping_ = fontFace->kerningMapping_;
        textures_ = fontFace->textures_;
        pointSize_ = fontFace->pointSize_;
        rowHeight_ = fontFace->rowHeight_;

        return true;
    }

    pointSize_ = fontFace->pointSize_;
    rowHeight_ = fontFace->rowHeight_;

    int numPages = 1;
    int maxTextureSize = font_->GetContext()->m_UISystem->GetMaxFontTextureSize();
    AreaAllocator allocator(FONT_TEXTURE_MIN_SIZE, FONT_TEXTURE_MIN_SIZE, maxTextureSize, maxTextureSize);

    for (auto i = fontFace->glyphMapping_.begin(), fin=fontFace->glyphMapping_.end(); i!=fin; ++i)
    {
        FontGlyph fontGlyph = MAP_VALUE(i);
        if (!fontGlyph.used_)
            continue;

        int x, y;
        if (!allocator.Allocate(fontGlyph.width_ + 1, fontGlyph.height_ + 1, x, y))
        {
            ++numPages;

            allocator = AreaAllocator(FONT_TEXTURE_MIN_SIZE, FONT_TEXTURE_MIN_SIZE, maxTextureSize, maxTextureSize);
            if (!allocator.Allocate(fontGlyph.width_ + 1, fontGlyph.height_ + 1, x, y))
                return false;
        }

        fontGlyph.x_ = x;
        fontGlyph.y_ = y;
        fontGlyph.page_ = numPages - 1;

        glyphMapping_[MAP_KEY(i)] = fontGlyph;
    }

    // Assume that format is the same for all textures and that bitmap font type may have more than one component
    unsigned components = ConvertFormatToNumComponents(fontFace->textures_[0]->GetFormat());

    // Save the existing textures as image resources
    std::vector<SharedPtr<Image> > oldImages;
    for (unsigned i = 0; i < fontFace->textures_.size(); ++i)
        oldImages.push_back(SaveFaceTexture(fontFace->textures_[i]));

    std::vector<SharedPtr<Image> > newImages(numPages);
    for (int i = 0; i < numPages; ++i)
    {
        SharedPtr<Image> image(new Image(font_->GetContext()));

        int width = maxTextureSize;
        int height = maxTextureSize;
        if (i == numPages - 1)
        {
            width = allocator.GetWidth();
            height = allocator.GetHeight();
        }

        image->SetSize(width, height, components);
        memset(image->GetData(), 0, width * height * components);

        newImages.push_back(image);
    }

    for (auto elem=glyphMapping_.begin(),fin=glyphMapping_.end(); elem!=fin; ++elem)
    {
        FontGlyph& newGlyph = MAP_VALUE(elem);
        const FontGlyph& oldGlyph = fontFace->glyphMapping_[MAP_KEY(elem)];
        Blit(newImages[newGlyph.page_], newGlyph.x_, newGlyph.y_, newGlyph.width_, newGlyph.height_,
             oldImages[oldGlyph.page_], oldGlyph.x_, oldGlyph.y_, components);
    }

    textures_.resize(newImages.size());
    for (unsigned i = 0; i < newImages.size(); ++i)
        textures_[i] = LoadFaceTexture(newImages[i]);

    for (auto i = fontFace->kerningMapping_.cbegin(); i != fontFace->kerningMapping_.cend(); ++i)
    {
        unsigned first = (MAP_KEY(i)) >> 16;
        unsigned second = (MAP_KEY(i)) & 0xffff;
        if (glyphMapping_.find(first) != glyphMapping_.end() && glyphMapping_.find(second) != glyphMapping_.end())
            kerningMapping_[MAP_KEY(i)] = MAP_VALUE(i);
    }

    return true;
}

bool FontFaceBitmap::Save(Serializer& dest, int pointSize, const QString& indentation)
{
    Context* context = font_->GetContext();

    SharedPtr<XMLFile> xml(new XMLFile(context));
    XMLElement rootElem = xml->CreateRoot("font");

    // Information
    XMLElement childElem = rootElem.CreateChild("info");
    QString fileName = GetFileName(font_->GetName());
    childElem.SetAttribute("face", fileName);
    childElem.SetAttribute("size", QString::number(pointSize));

    // Common
    childElem = rootElem.CreateChild("common");
    childElem.SetInt("lineHeight", rowHeight_);
    unsigned pages = textures_.size();
    childElem.SetUInt("pages", pages);

    // Construct the path to store the texture
    QString pathName;
    File* file = dynamic_cast<File*>(&dest);
    if (file)
        // If serialize to file, use the file's path
        pathName = GetPath(file->GetName());
    else
        // Otherwise, use the font resource's path
        pathName = "Data/" + GetPath(font_->GetName());

    // Pages
    childElem = rootElem.CreateChild("pages");
    for (unsigned i = 0; i < pages; ++i)
    {
        XMLElement pageElem = childElem.CreateChild("page");
        pageElem.SetInt("id", i);
        QString texFileName = fileName + "_" + QString::number(i) + ".png";
        pageElem.SetAttribute("file", texFileName);

        // Save the font face texture to image file
        SaveFaceTexture(textures_[i], pathName + texFileName);
    }

    // Chars and kernings
    XMLElement charsElem = rootElem.CreateChild("chars");
    unsigned numGlyphs = glyphMapping_.size();
    charsElem.SetInt("count", numGlyphs);

    for (auto i = glyphMapping_.begin(), fin=glyphMapping_.end(); i!=fin; ++i)
    {
        // Char
        XMLElement charElem = charsElem.CreateChild("char");
        charElem.SetInt("id", MAP_KEY(i));

        const FontGlyph& glyph = MAP_VALUE(i);
        charElem.SetInt("x", glyph.x_);
        charElem.SetInt("y", glyph.y_);
        charElem.SetInt("width", glyph.width_);
        charElem.SetInt("height", glyph.height_);
        charElem.SetInt("xoffset", glyph.offsetX_);
        charElem.SetInt("yoffset", glyph.offsetY_);
        charElem.SetInt("xadvance", glyph.advanceX_);
        charElem.SetUInt("page", glyph.page_);
    }

    if (!kerningMapping_.empty())
    {
        XMLElement kerningsElem = rootElem.CreateChild("kernings");
        for (auto i = kerningMapping_.begin(), fin=kerningMapping_.end(); i!=fin; ++i)
        {
            XMLElement kerningElem = kerningsElem.CreateChild("kerning");
            kerningElem.SetInt("first", MAP_KEY(i)>> 16);
            kerningElem.SetInt("second", MAP_KEY(i) & 0xffff);
            kerningElem.SetInt("amount", MAP_VALUE(i));
        }
    }

    return xml->Save(dest, indentation);
}

unsigned FontFaceBitmap::ConvertFormatToNumComponents(uint32_t format)
{
    if (format == Graphics::GetRGBAFormat())
        return 4;
    else if (format == Graphics::GetRGBFormat())
        return 3;
    else if (format == Graphics::GetLuminanceAlphaFormat())
        return 2;
    else
        return 1;
}

SharedPtr<Image> FontFaceBitmap::SaveFaceTexture(Texture2D* texture)
{
    SharedPtr<Image> image(new Image(font_->GetContext()));
    image->SetSize(texture->GetWidth(), texture->GetHeight(), ConvertFormatToNumComponents(texture->GetFormat()));
    if (!texture->GetData(0, image->GetData()))
    {
        URHO3D_LOGERROR("Could not save texture to image resource");
        return SharedPtr<Image>();
    }
    return image;
}

bool FontFaceBitmap::SaveFaceTexture(Texture2D* texture, const QString& fileName)
{
    SharedPtr<Image> image = SaveFaceTexture(texture);
    return image ? image->SavePNG(fileName) : false;
}

void FontFaceBitmap::Blit(Image* dest, int x, int y, int width, int height, Image* source, int sourceX, int sourceY, int components)
{
    unsigned char* destData = dest->GetData() + (y * dest->GetWidth() + x) * components;
    unsigned char* sourceData = source->GetData() + (sourceY * source->GetWidth() + sourceX) * components;
    for (int i = 0; i < height; ++i)
    {
        memcpy(destData, sourceData, width * components);
        destData += dest->GetWidth() * components;
        sourceData += source->GetWidth() * components;
    }
}

}
