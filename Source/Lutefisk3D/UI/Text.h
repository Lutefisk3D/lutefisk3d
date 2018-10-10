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

#include "Lutefisk3D/UI/UISelectable.h"

namespace Urho3D
{

static const float DEFAULT_FONT_SIZE = 12;

class Font;
class FontFace;
struct FontGlyph;

/// Text effect.
enum TextEffect
{
    TE_NONE = 0,
    TE_SHADOW,
    TE_STROKE
};

/// Cached character location and size within text. Used for queries related to text editing.
struct CharLocation
{
    /// Position.
    Vector2 position_;
    /// Size.
    Vector2 size_;
};

/// Glyph and its location within the text. Used when preparing text rendering.
struct GlyphLocation
{
    float x_;
    float y_;
    const FontGlyph* glyph_;
};

/// %Text %UI element.
class LUTEFISK3D_EXPORT Text : public UISelectable
{
    URHO3D_OBJECT(Text,UISelectable)

    friend class Text3D;

public:
    explicit Text(Context* context);
    ~Text() override;
    /// Register object factory.
    static void RegisterObject(Context* context);

    /// Apply attribute changes that can not be applied immediately.
    void ApplyAttributes() override;
    /// Return UI rendering batches.
    void GetBatches(std::vector<UIBatch>& batches, std::vector<float>& vertexData, const IntRect& currentScissor) override;
    /// React to resize.
    void OnResize(const IntVector2& newSize, const IntVector2& delta) override;
    /// React to indent change.
    void OnIndentSet() override;

    /// Set font by looking from resource cache by name and font size. Return true if successful.
    bool SetFont(const QString& fontName, float size = DEFAULT_FONT_SIZE);
    /// Set font and font size. Return true if successful.
    bool SetFont(Font* font, float size = DEFAULT_FONT_SIZE);
    /// Set font size only while retaining the existing font. Return true if successful.
    bool SetFontSize(float size);
    /// Set text. Text is assumed to be either ASCII or UTF8-encoded.
    void SetText(const QString& text);
    /// Set row alignment.
    void SetTextAlignment(HorizontalAlignment align);
    /// Set row spacing, 1.0 for original font spacing.
    void SetRowSpacing(float spacing);
    /// Set wordwrap. In wordwrap mode the text element will respect its current width. Otherwise it resizes itself freely.
    void SetWordwrap(bool enable);
    /// The text will be automatically translated. The text value used as string identifier.
    void SetAutoLocalizable(bool enable);
    /// Set selection. When length is not provided, select until the text ends.
    void SetSelection(unsigned start, unsigned length = M_MAX_UNSIGNED);
    /// Clear selection.
    void ClearSelection();
    /// Set text effect.
    void SetTextEffect(TextEffect textEffect);
    /// Set shadow offset.
    void SetEffectShadowOffset(const IntVector2& offset);
    /// Set stroke thickness.
    void SetEffectStrokeThickness(int thickness);
    /// Set stroke rounding. Corners of the font will be rounded off in the stroke so the stroke won't have corners.
    void SetEffectRoundStroke(bool roundStroke);
    /// Set effect color.
    void SetEffectColor(const Color& effectColor);

    /// Return font.
    Font* GetFont() const { return font_; }

    /// Return font size.
    float GetFontSize() const { return fontSize_; }

    /// Return text.
    const QString& GetText() const { return text_; }
    /// Return row alignment.
    HorizontalAlignment GetTextAlignment() const { return textAlignment_; }
    /// Return row spacing.
    float GetRowSpacing() const { return rowSpacing_; }
    /// Return wordwrap mode.
    bool GetWordwrap() const { return wordWrap_; }
    /// Return auto localizable mode.
    bool GetAutoLocalizable() const { return autoLocalizable_; }
    /// Return selection start.
    unsigned GetSelectionStart() const { return selectionStart_; }
    /// Return selection length.
    unsigned GetSelectionLength() const { return selectionLength_; }
    /// Return text effect.
    TextEffect GetTextEffect() const { return textEffect_; }

    /// Return effect shadow offset.
    const IntVector2& GetEffectShadowOffset() const { return shadowOffset_; }

    /// Return effect stroke thickness.
    int GetEffectStrokeThickness() const { return strokeThickness_; }

    /// Return effect round stroke.
    bool GetEffectRoundStroke() const { return roundStroke_; }

    /// Return effect color.
    const Color& GetEffectColor() const { return effectColor_; }
    /// Return row height.
    float GetRowHeight() const { return rowHeight_; }
    /// Return number of rows.
    size_t GetNumRows() const { return rowWidths_.size(); }
    /// Return number of characters.
    size_t GetNumChars() const { return unicodeText_.size(); }
    /// Return width of row by index.
    float GetRowWidth(unsigned index) const;
    /// Return position of character by index relative to the text element origin.
    Vector2 GetCharPosition(unsigned index);
    /// Return size of character by index.
    Vector2 GetCharSize(unsigned index);

    /// Set text effect Z bias. Zero by default, adjusted only in 3D mode.
    void SetEffectDepthBias(float bias);
    /// Return effect Z bias.
    float GetEffectDepthBias() const { return effectDepthBias_; }
    /// Set font attribute.
    void SetFontAttr(const ResourceRef& value);
    /// Return font attribute.
    ResourceRef GetFontAttr() const;
    /// Set text attribute.
    void SetTextAttr(const QString& value);
    /// Return text attribute.
    QString GetTextAttr() const;

protected:
    /// Filter implicit attributes in serialization process.
    virtual bool FilterImplicitAttributes(XMLElement& dest) const override;
    /// Update text when text, font or spacing changed.
    void UpdateText(bool onResize = false);
    /// Update cached character locations after text update, or when text alignment or indent has changed.
    void UpdateCharLocations();
    /// Validate text selection to be within the text.
    void ValidateSelection();
    /// Return row start X position.
    int GetRowStartPosition(unsigned rowIndex) const;
    /// Contruct batch.
    void ConstructBatch(UIBatch& pageBatch, const std::vector<GlyphLocation>& pageGlyphLocation, float dx = 0, float dy = 0, Color* color = nullptr, float depthBias = 0.0f);

    SharedPtr<Font> font_;
    /// Current face.
    WeakPtr<FontFace> fontFace_;
    float fontSize_;
    /// UTF-8 encoded text.
    QString text_;
    /// Row alignment.
    HorizontalAlignment textAlignment_;
    float rowSpacing_;
    /// Wordwrap mode.
    bool wordWrap_;
    bool charLocationsDirty_;
    unsigned selectionStart_;
    unsigned selectionLength_;
    /// Text effect.
    TextEffect textEffect_;
    /// Text effect shadow offset.
    IntVector2 shadowOffset_;
    /// Text effect stroke thickness.
    int strokeThickness_;
    /// Text effect stroke rounding flag.
    bool roundStroke_;
    Color effectColor_;
    /// Text effect Z bias.
    float effectDepthBias_;
    float rowHeight_;
    /// Text as Unicode characters.
    std::vector<QChar> unicodeText_;
    /// Text modified into printed form.
    std::vector<QChar> printText_;
    /// Mapping of printed form back to original char indices.
    std::vector<unsigned> printToText_;
    /// Row widths.
    std::vector<float> rowWidths_;
    /// Glyph locations per each texture in the font.
    std::vector<std::vector<GlyphLocation> > pageGlyphLocations_;
    /// Cached locations of each character in the text.
    std::vector<CharLocation> charLocations_;
    /// The text will be automatically translated.
    bool autoLocalizable_;
    /// Localization string id storage. Used when autoLocalizable flag is set.
    QString stringId_;
    /// Handle change Language.
    void HandleChangeLanguage();
    /// UTF8 to Unicode.
    void DecodeToUnicode();
};

}
