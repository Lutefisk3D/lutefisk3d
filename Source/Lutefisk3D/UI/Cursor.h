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

#pragma once

#include "../UI/BorderImage.h"
#include "../Resource/Image.h"
#include "../Graphics/Texture.h"
#include "../Container/HashMap.h"

#include <SDL2/SDL_mouse.h>

namespace Urho3D
{

/// %Cursor shapes recognized by the UI subsystem.
enum CursorShape
{
    CS_NORMAL = 0,
    CS_IBEAM,
    CS_CROSS,
    CS_RESIZEVERTICAL,
    CS_RESIZEDIAGONAL_TOPRIGHT,
    CS_RESIZEHORIZONTAL,
    CS_RESIZEDIAGONAL_TOPLEFT,
    CS_RESIZE_ALL,
    CS_ACCEPTDROP,
    CS_REJECTDROP,
    CS_BUSY,
    CS_BUSY_ARROW,
    CS_MAX_SHAPES
};

/// %Cursor image and hotspot information.
struct CursorShapeInfo
{
    /// Construct with defaults.
    CursorShapeInfo() :
        imageRect_(IntRect::ZERO),
        hotSpot_(IntVector2::ZERO),
        osCursor_(nullptr),
        systemDefined_(false),
        systemCursor_(-1)
    {
    }

    /// Construct with system cursor.
    CursorShapeInfo(int systemCursor) :
        imageRect_(IntRect::ZERO),
        hotSpot_(IntVector2::ZERO),
        osCursor_(nullptr),
        systemDefined_(false),
        systemCursor_(systemCursor)
    {
    }

    /// Image.
    SharedPtr<Image> image_;
    /// Texture.
    SharedPtr<Texture> texture_;
    /// Image rectangle.
    IntRect imageRect_;
    /// Hotspot coordinates.
    IntVector2 hotSpot_;
    /// OS cursor.
    SDL_Cursor* osCursor_;
    /// Whether the OS cursor is system defined.
    bool systemDefined_;
    /// System cursor index.
    int systemCursor_;
};

/// Mouse cursor %UI element.
class Cursor : public BorderImage
{
    URHO3D_OBJECT(Cursor,BorderImage);

public:
    /// Construct.
    Cursor(Context* context);
    /// Destruct.
    virtual ~Cursor();
    /// Register object factory.
    static void RegisterObject(Context* context);

    /// Return UI rendering batches.
    virtual void GetBatches(std::vector<UIBatch>& batches, std::vector<float>& vertexData, const IntRect& currentScissor) override;

    /// Define a shape.
    void DefineShape(const QString& shape, Image* image, const IntRect& imageRect, const IntVector2& hotSpot);
    /// Define a shape.
    void DefineShape(CursorShape shape, Image* image, const IntRect& imageRect, const IntVector2& hotSpot);
    /// Set current shape.
    void SetShape(const QString& shape);
    /// Set current shape.
    void SetShape(CursorShape shape);
    /// Set whether to use system default shapes. Is only possible when the OS mouse cursor has been set visible from the Input subsystem.
    void SetUseSystemShapes(bool enable);
    /// Get current shape.
    const QString& GetShape() const { return shape_; }
    /// Return whether is using system default shapes.
    bool GetUseSystemShapes() const { return useSystemShapes_; }

    /// Set shapes attribute.
    void SetShapesAttr(const VariantVector& value);
    /// Return shapes attribute.
    VariantVector GetShapesAttr() const;
    /// Apply pending OS cursor shape. Called by UI. No-op when the OS mouse pointer is not used.
    void ApplyOSCursorShape();

protected:
    /// Handle operating system mouse cursor visibility change event.
    void HandleMouseVisibleChanged(StringHash eventType, VariantMap& eventData);

    /// Current shape definition.
    QString shape_;
    /// Shape definitions.
    HashMap<QString, CursorShapeInfo> shapeInfos_;
    /// Use system default shapes flag.
    bool useSystemShapes_;
    /// OS cursor shape needs update flag.
    bool osShapeDirty_;
};

}
