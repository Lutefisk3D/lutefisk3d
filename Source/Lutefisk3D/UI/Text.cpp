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

#include "Text.h"
#include "Font.h"
#include "FontFace.h"

#include "Lutefisk3D/Core/Context.h"
#include "Lutefisk3D/IO/Log.h"
#include "Lutefisk3D/Core/Profiler.h"
#include "Lutefisk3D/Resource/ResourceCache.h"
#include "Lutefisk3D/Resource/Localization.h"
#include "Lutefisk3D/Resource/ResourceEvents.h"
#include "Lutefisk3D/Graphics/Texture2D.h"


namespace Urho3D
{

const char* textEffects[] =
{
    "None",
    "Shadow",
    "Stroke",
    "Stroke 2",
    "Stroke 3",
    "Stroke 4",
    "Stroke 5",
    nullptr
};

static const float MIN_ROW_SPACING = 0.5f;

extern const char* horizontalAlignments[];
extern const char* UI_CATEGORY;

Text::Text(Context* context) :
    UIElement(context),
    fontSize_(DEFAULT_FONT_SIZE),
    textAlignment_(HA_LEFT),
    rowSpacing_(1.0f),
    wordWrap_(false),
    autoLocalizable_(false),
    charLocationsDirty_(true),
    selectionStart_(0),
    selectionLength_(0),
    selectionColor_(Color::TRANSPARENT),
    hoverColor_(Color::TRANSPARENT),
    textEffect_(TE_NONE),
    effectColor_(Color::BLACK),
    effectDepthBias_(0.0f),
    rowHeight_(0)
{
    // By default Text does not derive opacity from parent elements
    useDerivedOpacity_ = false;
}

Text::~Text()
{
}

void Text::RegisterObject(Context* context)
{
    context->RegisterFactory<Text>(UI_CATEGORY);

    URHO3D_COPY_BASE_ATTRIBUTES(UIElement);
    URHO3D_UPDATE_ATTRIBUTE_DEFAULT_VALUE("Use Derived Opacity", false);
    URHO3D_MIXED_ACCESSOR_ATTRIBUTE("Font", GetFontAttr, SetFontAttr, ResourceRef, ResourceRef(Font::GetTypeStatic()), AM_FILE);
    URHO3D_ATTRIBUTE("Font Size", int, fontSize_, DEFAULT_FONT_SIZE, AM_FILE);
    URHO3D_ATTRIBUTE("Text", QString, text_, QString(), AM_FILE);
    URHO3D_ENUM_ATTRIBUTE("Text Alignment", textAlignment_, horizontalAlignments, HA_LEFT, AM_FILE);
    URHO3D_ATTRIBUTE("Row Spacing", float, rowSpacing_, 1.0f, AM_FILE);
    URHO3D_ATTRIBUTE("Word Wrap", bool, wordWrap_, false, AM_FILE);
    URHO3D_ACCESSOR_ATTRIBUTE("Auto Localizable", GetAutoLocalizable, SetAutoLocalizable, bool, false, AM_FILE);
    URHO3D_ACCESSOR_ATTRIBUTE("Selection Color", GetSelectionColor, SetSelectionColor, Color, Color::TRANSPARENT, AM_FILE);
    URHO3D_ACCESSOR_ATTRIBUTE("Hover Color", GetHoverColor, SetHoverColor, Color, Color::TRANSPARENT, AM_FILE);
    URHO3D_ENUM_ATTRIBUTE("Text Effect", textEffect_, textEffects, TE_NONE, AM_FILE);
    URHO3D_ACCESSOR_ATTRIBUTE("Effect Color", GetEffectColor, SetEffectColor, Color, Color::BLACK, AM_FILE);

    // Change the default value for UseDerivedOpacity
    context->GetAttribute<Text>("Use Derived Opacity")->defaultValue_ = false;
}

void Text::ApplyAttributes()
{
    UIElement::ApplyAttributes();

    DecodeToUnicode();

    fontSize_ = Max(fontSize_, 1);
    strokeThickness_ = Abs(strokeThickness_);
    ValidateSelection();
    UpdateText();
}

void Text::GetBatches(std::vector<UIBatch>& batches, std::vector<float>& vertexData, const IntRect& currentScissor)
{
    FontFace* face = font_ ? font_->GetFace(fontSize_) : (FontFace*)nullptr;
    if (!face)
    {
        hovering_ = false;
        return;
    }

    // If face has changed or char locations are not valid anymore, update before rendering
    if (charLocationsDirty_ || !fontFace_ || face != fontFace_)
        UpdateCharLocations();
    // If face uses mutable glyphs mechanism, reacquire glyphs before rendering to make sure they are in the texture
    else if (face->HasMutableGlyphs())
    {
        for (unsigned i = 0; i < printText_.size(); ++i)
            face->GetGlyph(printText_[i].unicode());
    }

    // Hovering and/or whole selection batch
    if ((hovering_ && hoverColor_.a_ > 0.0) || (selected_ && selectionColor_.a_ > 0.0f))
    {
        bool both = hovering_ && selected_ && hoverColor_.a_ > 0.0 && selectionColor_.a_ > 0.0f;
        UIBatch batch(this, BLEND_ALPHA, currentScissor, nullptr, &vertexData);
        batch.SetColor(both ? selectionColor_.Lerp(hoverColor_, 0.5f) : (selected_ && selectionColor_.a_ > 0.0f ?
                                                                             selectionColor_: hoverColor_));
        batch.AddQuad(0, 0, GetWidth(), GetHeight(), 0, 0);
        UIBatch::AddOrMerge(batch, batches);
    }

    // Partial selection batch
    if (!selected_ && selectionLength_ && charLocations_.size() >= selectionStart_ + selectionLength_ && selectionColor_.a_ > 0.0f)
    {
        UIBatch batch(this, BLEND_ALPHA, currentScissor, nullptr, &vertexData);
        batch.SetColor(selectionColor_);

        IntVector2 currentStart = charLocations_[selectionStart_].position_;
        IntVector2 currentEnd = currentStart;
        for (unsigned i = selectionStart_; i < selectionStart_ + selectionLength_; ++i)
        {
            // Check if row changes, and start a new quad in that case
            if (charLocations_[i].size_ != IntVector2::ZERO)
            {
                if (charLocations_[i].position_.y_ != currentStart.y_)
                {
                    batch.AddQuad(currentStart.x_, currentStart.y_, currentEnd.x_ - currentStart.x_, currentEnd.y_ - currentStart.y_,
                                  0, 0);
                    currentStart = charLocations_[i].position_;
                    currentEnd = currentStart + charLocations_[i].size_;
                }
                else
                {
                    currentEnd.x_ += charLocations_[i].size_.x_;
                    currentEnd.y_ = Max(currentStart.y_ + charLocations_[i].size_.y_, currentEnd.y_);
                }
            }
        }
        if (currentEnd != currentStart)
        {
            batch.AddQuad(currentStart.x_, currentStart.y_, currentEnd.x_ - currentStart.x_, currentEnd.y_ - currentStart.y_, 0, 0);
        }

        UIBatch::AddOrMerge(batch, batches);
    }

    // Text batch
    TextEffect textEffect = font_->IsSDFFont() ? TE_NONE : textEffect_;
    const std::vector<SharedPtr<Texture2D> >& textures = face->GetTextures();
    for (unsigned n = 0; n < textures.size() && n < pageGlyphLocations_.size(); ++n)
    {
        // One batch per texture/page
        UIBatch pageBatch(this, BLEND_ALPHA, currentScissor, textures[n], &vertexData);

        const std::vector<GlyphLocation>& pageGlyphLocation = pageGlyphLocations_[n];

        switch (textEffect)
        {
        case TE_NONE:
            ConstructBatch(pageBatch, pageGlyphLocation, 0, 0);
            break;

        case TE_SHADOW:
            ConstructBatch(pageBatch, pageGlyphLocation, 1, 1, &effectColor_, effectDepthBias_);
            ConstructBatch(pageBatch, pageGlyphLocation, 0, 0);
            break;

        case TE_STROKE:
            if (roundStroke_)
            {
                // Samples should be even or glyph may be redrawn in wrong x y pos making stroke corners rough
                // Adding to thickness helps with thickness of 1 not having enought samples for this formula
                // or certain fonts with reflex corners requiring more glyph samples for a smooth stroke when large
                int thickness = Min(strokeThickness_, fontSize_);
                int samples = thickness * thickness + (thickness % 2 == 0 ? 4 : 3);
                float angle = 360.f / samples;
                float floatThickness = (float)thickness;
                for (int i = 0; i < samples; ++i)
                {
                    float x = Cos(angle * i) * floatThickness;
                    float y = Sin(angle * i) * floatThickness;
                    ConstructBatch(pageBatch, pageGlyphLocation, (int)x, (int)y, &effectColor_, effectDepthBias_);
                }
            }
            else
            {
                int thickness = Min(strokeThickness_, fontSize_);
                int x, y;
                for (x = -thickness; x <= thickness; ++x)
                {
                    for (y = -thickness; y <= thickness; ++y)
                    {
                        // Don't draw glyphs that aren't on the edges
                        if (x > -thickness && x < thickness &&
                                y > -thickness && y < thickness)
                            continue;

                        ConstructBatch(pageBatch, pageGlyphLocation, x, y, &effectColor_, effectDepthBias_);
                    }
                }
            }
            ConstructBatch(pageBatch, pageGlyphLocation, 0, 0);
            break;
        }

        UIBatch::AddOrMerge(pageBatch, batches);
    }

    // Reset hovering for next frame
    hovering_ = false;
}

void Text::OnResize(const IntVector2& newSize, const IntVector2& delta)
{
    if (wordWrap_)
        UpdateText(true);
    else
        charLocationsDirty_ = true;
}

void Text::OnIndentSet()
{
    charLocationsDirty_ = true;
}

bool Text::SetFont(const QString& fontName, int size)
{
    ResourceCache* cache = GetSubsystem<ResourceCache>();
    return SetFont(cache->GetResource<Font>(fontName), size);
}

bool Text::SetFont(Font* font, int size)
{
    if (!font)
    {
        URHO3D_LOGERROR("Null font for Text");
        return false;
    }

    if (font != font_ || size != fontSize_)
    {
        font_ = font;
        fontSize_ = Max(size, 1);
        UpdateText();
    }

    return true;
}

bool Text::SetFontSize(int size)
{
    // Initial font must be set
    if (!font_)
        return false;
    else
        return SetFont(font_, size);
}

void Text::DecodeToUnicode()
{
    unicodeText_.clear();
    for (unsigned i = 0; i < text_.length(); ++i)
        unicodeText_.push_back(text_[i]);
}

void Text::SetText(const QString& text)
{
    if (autoLocalizable_)
    {
        stringId_ = text;
        Localization* l10n = GetSubsystem<Localization>();
        text_ = l10n->Get(stringId_);
    }
    else
    {
        text_ = text;
    }

    DecodeToUnicode();

    ValidateSelection();
    UpdateText();
}

void Text::SetTextAlignment(HorizontalAlignment align)
{
    if (align != textAlignment_)
    {
        textAlignment_ = align;
        charLocationsDirty_ = true;
    }
}

void Text::SetRowSpacing(float spacing)
{
    if (spacing != rowSpacing_)
    {
        rowSpacing_ = Max(spacing, MIN_ROW_SPACING);
        UpdateText();
    }
}

void Text::SetWordwrap(bool enable)
{
    if (enable != wordWrap_)
    {
        wordWrap_ = enable;
        UpdateText();
    }
}

void Text::SetAutoLocalizable(bool enable)
{
    if (enable != autoLocalizable_)
    {
        autoLocalizable_ = enable;
        if (enable)
        {
            stringId_ = text_;
            Localization* l10n = GetSubsystem<Localization>();
            text_ = l10n->Get(stringId_);
            SubscribeToEvent(E_CHANGELANGUAGE, URHO3D_HANDLER(Text, HandleChangeLanguage));
        }
        else
        {
            text_ = stringId_;
            stringId_ = "";
            UnsubscribeFromEvent(E_CHANGELANGUAGE);
        }
        DecodeToUnicode();
        ValidateSelection();
        UpdateText();
    }
}

void Text::HandleChangeLanguage(StringHash eventType, VariantMap& eventData)
{
    Localization* l10n = GetSubsystem<Localization>();
    text_ = l10n->Get(stringId_);
    DecodeToUnicode();
    ValidateSelection();
    UpdateText();
}

void Text::SetSelection(unsigned start, unsigned length)
{
    selectionStart_ = start;
    selectionLength_ = length;
    ValidateSelection();
}

void Text::ClearSelection()
{
    selectionStart_ = 0;
    selectionLength_ = 0;
}

void Text::SetSelectionColor(const Color& color)
{
    selectionColor_ = color;
}

void Text::SetHoverColor(const Color& color)
{
    hoverColor_ = color;
}

void Text::SetTextEffect(TextEffect textEffect)
{
    textEffect_ = textEffect;
}

void Text::SetEffectShadowOffset(const IntVector2& offset)
{
    shadowOffset_ = offset;
}

void Text::SetEffectStrokeThickness(int thickness)
{
    strokeThickness_ = Abs(thickness);
}

void Text::SetEffectRoundStroke(bool roundStroke)
{
    roundStroke_ = roundStroke;
}

void Text::SetEffectColor(const Color& effectColor)
{
    effectColor_ = effectColor;
}

void Text::SetEffectDepthBias(float bias)
{
    effectDepthBias_ = bias;
}

int Text::GetRowWidth(unsigned index) const
{
    return index < rowWidths_.size() ? rowWidths_[index] : 0;
}

IntVector2 Text::GetCharPosition(unsigned index)
{
    if (charLocationsDirty_)
        UpdateCharLocations();
    if (charLocations_.empty())
        return IntVector2::ZERO;
    // For convenience, return the position of the text ending if index exceeded
    if (index > charLocations_.size() - 1)
        index = charLocations_.size() - 1;
    return charLocations_[index].position_;
}

IntVector2 Text::GetCharSize(unsigned index)
{
    if (charLocationsDirty_)
        UpdateCharLocations();
    if (charLocations_.size() < 2)
        return IntVector2::ZERO;
    // For convenience, return the size of the last char if index exceeded (last size entry is zero)
    if (index > charLocations_.size() - 2)
        index = charLocations_.size() - 2;
    return charLocations_[index].size_;
}

void Text::SetFontAttr(const ResourceRef& value)
{
    ResourceCache* cache = GetSubsystem<ResourceCache>();
    font_ = cache->GetResource<Font>(value.name_);
}

ResourceRef Text::GetFontAttr() const
{
    return GetResourceRef(font_, Font::GetTypeStatic());
}

void Text::SetTextAttr(const QString &value)
{
    text_ = value;
    if (autoLocalizable_)
        stringId_ = value;
}

QString Text::GetTextAttr() const
{
    if (autoLocalizable_ && !stringId_.isEmpty())
        return stringId_;
    else
        return text_;
}

bool Text::FilterImplicitAttributes(XMLElement& dest) const
{
    if (!UIElement::FilterImplicitAttributes(dest))
        return false;

    if (!IsFixedWidth())
    {
        if (!RemoveChildXML(dest, "Size"))
            return false;
        if (!RemoveChildXML(dest, "Min Size"))
            return false;
        if (!RemoveChildXML(dest, "Max Size"))
            return false;
    }

    return true;
}

void Text::UpdateText(bool onResize)
{
    rowWidths_.clear();
    printText_.clear();

    if (font_)
    {
        FontFace* face = font_->GetFace(fontSize_);
        if (!face)
            return;

        rowHeight_ = face->GetRowHeight();

        int width = 0;
        int height = 0;
        int rowWidth = 0;
        int rowHeight = (int)(rowSpacing_ * rowHeight_);

        // First see if the text must be split up
        if (!wordWrap_)
        {
            printText_ = unicodeText_;
            printToText_.resize(printText_.size());
            for (unsigned i = 0; i < printText_.size(); ++i)
                printToText_[i] = i;
        }
        else
        {
            int maxWidth = GetWidth();
            unsigned nextBreak = 0;
            unsigned lineStart = 0;
            printToText_.clear();

            for (unsigned i = 0; i < unicodeText_.size(); ++i)
            {
                unsigned j;
                QChar c = unicodeText_[i];

                if (c != '\n')
                {
                    bool ok = true;

                    if (nextBreak <= i)
                    {
                        int futureRowWidth = rowWidth;
                        for (j = i; j < unicodeText_.size(); ++j)
                        {
                            QChar d = unicodeText_[j];
                            if (d == ' ' || d == '\n')
                            {
                                nextBreak = j;
                                break;
                            }
                            const FontGlyph* glyph = face->GetGlyph(d.unicode());
                            if (glyph)
                            {
                                futureRowWidth += glyph->advanceX_;
                                if (j < unicodeText_.size() - 1)
                                    futureRowWidth += face->GetKerning(d.unicode(), unicodeText_[j + 1].unicode());
                            }
                            if (d == '-' && futureRowWidth <= maxWidth)
                            {
                                nextBreak = j + 1;
                                break;
                            }
                            if (futureRowWidth > maxWidth)
                            {
                                ok = false;
                                break;
                            }
                        }
                    }

                    if (!ok)
                    {
                        // If did not find any breaks on the line, copy until j, or at least 1 char, to prevent infinite loop
                        if (nextBreak == lineStart)
                        {
                            while (i < j)
                            {
                                printText_.push_back(unicodeText_[i]);
                                printToText_.push_back(i);
                                ++i;
                            }
                        }
                        // Eliminate spaces that have been copied before the forced break
                        while (printText_.size() && printText_.back() == ' ')
                        {
                            printText_.pop_back();
                            printToText_.pop_back();
                        }
                        printText_.push_back('\n');
                        printToText_.push_back(Min((int)i, (int)unicodeText_.size() - 1));
                        rowWidth = 0;
                        nextBreak = lineStart = i;
                    }

                    if (i < unicodeText_.size())
                    {
                        // When copying a space, position is allowed to be over row width
                        c = unicodeText_[i];
                        const FontGlyph* glyph = face->GetGlyph(c.unicode());
                        if (glyph)
                        {
                            rowWidth += glyph->advanceX_;
                            if (i < unicodeText_.size() - 1)
                                rowWidth += face->GetKerning(c.unicode(), unicodeText_[i + 1].unicode());
                        }
                        if (rowWidth <= maxWidth)
                        {
                            printText_.push_back(c);
                            printToText_.push_back(i);
                        }
                    }
                }
                else
                {
                    printText_.push_back('\n');
                    printToText_.push_back(Min((int)i, (int)unicodeText_.size() - 1));
                    rowWidth = 0;
                    nextBreak = lineStart = i;
                }
            }
        }

        rowWidth = 0;

        for (unsigned i = 0; i < printText_.size(); ++i)
        {
            unsigned c = printText_[i].unicode();

            if (c != '\n')
            {
                const FontGlyph* glyph = face->GetGlyph(c);
                if (glyph)
                {
                    rowWidth += glyph->advanceX_;
                    if (i < printText_.size() - 1)
                        rowWidth += face->GetKerning(c, printText_[i + 1].unicode());
                }
            }
            else
            {
                width = Max(width, rowWidth);
                height += rowHeight;
                rowWidths_.push_back(rowWidth);
                rowWidth = 0;
            }
        }

        if (rowWidth)
        {
            width = Max(width, rowWidth);
            height += rowHeight;
            rowWidths_.push_back(rowWidth);
        }

        // Set at least one row height even if text is empty
        if (!height)
            height = rowHeight;

        // Set minimum and current size according to the text size, but respect fixed width if set
        if (!IsFixedWidth())
        {
            SetMinWidth(wordWrap_ ? 0 : width);
            SetWidth(width);
        }
        SetFixedHeight(height);

        charLocationsDirty_ = true;
    }
    else
    {
        // No font, nothing to render
        pageGlyphLocations_.clear();
    }

    // If wordwrap is on, parent may need layout update to correct for overshoot in size. However, do not do this when the
    // update is a response to resize, as that could cause infinite recursion
    if (wordWrap_ && !onResize)
    {
        UIElement* parent = GetParent();
        if (parent && parent->GetLayoutMode() != LM_FREE)
            parent->UpdateLayout();
    }
}

void Text::UpdateCharLocations()
{
    // Remember the font face to see if it's still valid when it's time to render
    FontFace* face = font_ ? font_->GetFace(fontSize_) : (FontFace*)nullptr;
    if (!face)
        return;
    fontFace_ = face;

    int rowHeight = (int)(rowSpacing_ * rowHeight_);

    // Store position & size of each character, and locations per texture page
    unsigned numChars = unicodeText_.size();
    charLocations_.resize(numChars + 1);
    pageGlyphLocations_.resize(face->GetTextures().size());
    for (unsigned i = 0; i < pageGlyphLocations_.size(); ++i)
        pageGlyphLocations_[i].clear();

    IntVector2 offset = font_->GetTotalGlyphOffset(fontSize_);

    unsigned rowIndex = 0;
    unsigned lastFilled = 0;
    int x = GetRowStartPosition(rowIndex) + offset.x_;
    int y = offset.y_;

    for (unsigned i = 0; i < printText_.size(); ++i)
    {
        CharLocation loc;
        loc.position_ = IntVector2(x, y);

        QChar c = printText_[i];
        if (c != '\n')
        {
            const FontGlyph* glyph = face->GetGlyph(c.unicode());
            loc.size_ = IntVector2(glyph ? glyph->advanceX_ : 0, rowHeight_);
            if (glyph)
            {
                // Store glyph's location for rendering. Verify that glyph page is valid
                if (glyph->page_ < pageGlyphLocations_.size())
                    pageGlyphLocations_[glyph->page_].push_back(GlyphLocation(x, y, glyph));
                x += glyph->advanceX_;
                if (i < printText_.size() - 1)
                    x += face->GetKerning(c.unicode(), printText_[i + 1].unicode());
            }
        }
        else
        {
            loc.size_ = IntVector2::ZERO;
            x = GetRowStartPosition(++rowIndex);
            y += rowHeight;
        }

        // Fill gaps in case characters were skipped from printing
        for (unsigned j = lastFilled; j <= printToText_[i]; ++j)
            charLocations_[j] = loc;
        lastFilled = printToText_[i] + 1;
    }
    // Store the ending position
    charLocations_[numChars].position_ = IntVector2(x, y);
    charLocations_[numChars].size_ = IntVector2::ZERO;

    charLocationsDirty_ = false;
}

void Text::ValidateSelection()
{
    unsigned textLength = unicodeText_.size();

    if (textLength)
    {
        if (selectionStart_ >= textLength)
            selectionStart_ = textLength - 1;
        if (selectionStart_ + selectionLength_ > textLength)
            selectionLength_ = textLength - selectionStart_;
    }
    else
    {
        selectionStart_ = 0;
        selectionLength_ = 0;
    }
}

int Text::GetRowStartPosition(unsigned rowIndex) const
{
    int rowWidth = 0;

    if (rowIndex < rowWidths_.size())
        rowWidth = rowWidths_[rowIndex];

    int ret = GetIndentWidth();

    switch (textAlignment_)
    {
    case HA_LEFT:
        break;
    case HA_CENTER:
        ret += (GetSize().x_ - rowWidth) / 2;
        break;
    case HA_RIGHT:
        ret += GetSize().x_ - rowWidth;
        break;
    }

    return ret;
}

void Text::ConstructBatch(UIBatch& pageBatch, const std::vector<GlyphLocation>& pageGlyphLocation, int dx, int dy, Color* color,
                          float depthBias)
{
    unsigned startDataSize = pageBatch.vertexData_->size();

    if (!color)
        pageBatch.SetDefaultColor();
    else
        pageBatch.SetColor(*color);

    pageBatch.vertexData_->reserve(startDataSize+pageGlyphLocation.size()*6);

    for (const GlyphLocation& glyphLocation : pageGlyphLocation)
    {
        const FontGlyph& glyph = *glyphLocation.glyph_;
        pageBatch.AddQuad(dx + glyphLocation.x_ + glyph.offsetX_, dy + glyphLocation.y_ + glyph.offsetY_, glyph.width_,
                          glyph.height_, glyph.x_, glyph.y_);
    }

    if (depthBias != 0.0f)
    {
        unsigned dataSize = pageBatch.vertexData_->size();
        for (unsigned i = startDataSize; i < dataSize; i += UI_VERTEX_SIZE)
            pageBatch.vertexData_->at(i + 2) += depthBias;
    }
}

}
