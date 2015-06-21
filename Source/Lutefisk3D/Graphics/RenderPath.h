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

#pragma once

#include "../Math/Color.h"
#include "../Graphics/GraphicsDefs.h"
#include "../Container/Ptr.h"
#include "../Container/RefCounted.h"
#include "../Math/Vector4.h"

#include <QString>
#include <vector>
#include <utility> // pair
#include <stdint.h>

#include "../Core/Variant.h"

namespace gl {
enum class GLenum : uint32_t;
}
namespace Urho3D
{

class XMLElement;
class XMLFile;

/// Rendering path command types.
enum RenderCommandType
{
    CMD_NONE = 0,
    CMD_CLEAR,
    CMD_SCENEPASS,
    CMD_QUAD,
    CMD_FORWARDLIGHTS,
    CMD_LIGHTVOLUMES,
    CMD_RENDERUI
};

/// Rendering path sorting modes.
enum RenderCommandSortMode
{
    SORT_FRONTTOBACK = 0,
    SORT_BACKTOFRONT
};

/// Rendertarget size mode.
enum RenderTargetSizeMode
{
    SIZE_ABSOLUTE = 0,
    SIZE_VIEWPORTDIVISOR,
    SIZE_VIEWPORTMULTIPLIER
};

/// Rendertarget definition.
struct RenderTargetInfo
{
    /// Construct.
    RenderTargetInfo() :
        size_(Vector2::ZERO),
        sizeMode_(SIZE_ABSOLUTE),
        enabled_(true),
        cubemap_(false),
        filtered_(false),
        sRGB_(false),
        persistent_(false)
    {
    }
    /// Read from an XML element.
    void Load(const XMLElement& element);

    /// Name.
    QString name_;
    /// Tag name.
    QString tag_;
    /// Texture format.
    gl::GLenum format_;
    /// Absolute size or multiplier.
    Vector2 size_;
    /// Size mode.
    RenderTargetSizeMode sizeMode_;
    /// Enabled flag.
    bool enabled_;
    /// Cube map flag.
    bool cubemap_;
    /// Filtering flag.
    bool filtered_;
    /// sRGB sampling/writing mode flag.
    bool sRGB_;
    /// Should be persistent and not shared/reused between other buffers of same size.
    bool persistent_;
};

/// Rendering path command.
struct RenderPathCommand
{
    /// Construct.
    RenderPathCommand() :
        clearFlags_(0),
        blendMode_(BLEND_REPLACE),
        enabled_(true),
        useFogColor_(false),
        markToStencil_(false),
        useLitBase_(true),
        vertexLights_(false)
    {
    }

    /// Read from an XML element.
    void Load(const XMLElement& element);
    /// Set a texture resource name. Can also refer to a rendertarget defined in the rendering path.
    void SetTextureName(TextureUnit unit, const QString& name);
    /// Set a shader parameter.
    void SetShaderParameter(const QString& name, const Variant& value);
    /// Remove a shader parameter.
    void RemoveShaderParameter(const QString& name);
    /// Set number of output rendertargets.
    void SetNumOutputs(unsigned num);
    /// Set output rendertarget name and face index for cube maps.
    void SetOutput(unsigned index, const QString& name, CubeMapFace face = FACE_POSITIVE_X);
    /// Set output rendertarget name.
    void SetOutputName(unsigned index, const QString& name);
    /// Set output rendertarget face index for cube maps.
    void SetOutputFace(unsigned index, CubeMapFace face);
    /// Set depth-stencil output name. When empty, will assign a depth-stencil buffer automatically.
    void SetDepthStencilName(const QString& name);

    /// Return texture resource name.
    const QString& GetTextureName(TextureUnit unit) const;
    /// Return shader parameter.
    const Variant& GetShaderParameter(const QString& name) const;
    /// Return number of output rendertargets.
    unsigned GetNumOutputs() const { return outputs_.size(); }
    /// Return output rendertarget name.
    const QString& GetOutputName(unsigned index) const;
    /// Return output rendertarget face index.
    CubeMapFace GetOutputFace(unsigned index) const;
    /// Return depth-stencil output name.
    const QString& GetDepthStencilName() const { return depthStencilName_; }


    /// Tag name.
    QString tag_;
    /// Command type.
    RenderCommandType type_;
    /// Sorting mode.
    RenderCommandSortMode sortMode_;
    /// Scene pass name.
    QString pass_;
    /// Scene pass index. Filled by View.
    unsigned passIndex_;
    /// Command/pass metadata.
    QString metadata_;
    /// Vertex shader name.
    QString vertexShaderName_;
    /// Pixel shader name.
    QString pixelShaderName_;
    /// Vertex shader defines.
    QString vertexShaderDefines_;
    /// Pixel shader defines.
    QString pixelShaderDefines_;
    /// Textures.
    QString textureNames_[MAX_TEXTURE_UNITS];
    /// %Shader parameters.
    HashMap<StringHash, Variant> shaderParameters_;
    /// Output rendertarget names and faces.
    std::vector<std::pair<QString, CubeMapFace> > outputs_;
    /// Depth-stencil output name.
    QString depthStencilName_;
    /// Clear flags. Affects clear command only.
    unsigned clearFlags_;
    /// Clear color. Affects clear command only.
    Color clearColor_;
    /// Clear depth. Affects clear command only.
    float clearDepth_;
    /// Clear stencil value. Affects clear command only.
    unsigned clearStencil_;
    /// Blend mode. Affects quad command only.
    BlendMode blendMode_;
    /// Enabled flag.
    bool enabled_;
    /// Use fog color for clearing.
    bool useFogColor_;
    /// Mark to stencil flag.
    bool markToStencil_;
    /// Use lit base pass optimization for forward per-pixel lights.
    bool useLitBase_;
    /// Vertex lights flag.
    bool vertexLights_;
};

/// Rendering path definition.
class RenderPath : public RefCounted
{
public:
    /// Construct.
    RenderPath();
    /// Destruct.
    ~RenderPath();

    /// Clone the rendering path.
    SharedPtr<RenderPath> Clone();
    /// Clear existing data and load from an XML file. Return true if successful.
    bool Load(XMLFile* file);
    /// Append data from an XML file. Return true if successful.
    bool Append(XMLFile* file);
    /// Enable/disable commands and rendertargets by tag.
    void SetEnabled(const QString& tag, bool active);
    /// Toggle enabled state of commands and rendertargets by tag.
    void ToggleEnabled(const QString& tag);
    /// Assign rendertarget at index.
    void SetRenderTarget(unsigned index, const RenderTargetInfo& info);
    /// Add a rendertarget.
    void AddRenderTarget(const RenderTargetInfo& info);
    /// Remove a rendertarget by index.
    void RemoveRenderTarget(unsigned index);
    /// Remove a rendertarget by name.
    void RemoveRenderTarget(const QString& name);
    /// Remove rendertargets by tag name.
    void RemoveRenderTargets(const QString& tag);
    /// Assign command at index.
    void SetCommand(unsigned index, const RenderPathCommand& command);
    /// Add a command to the end of the list.
    void AddCommand(const RenderPathCommand& command);
    /// Insert a command at a position.
    void InsertCommand(unsigned index, const RenderPathCommand& command);
    /// Remove a command by index.
    void RemoveCommand(unsigned index);
    /// Remove commands by tag name.
    void RemoveCommands(const QString& tag);
    /// Set a shader parameter in all commands that define it.
    void SetShaderParameter(const QString& name, const Variant& value);

    /// Return number of rendertargets.
    unsigned GetNumRenderTargets() const { return renderTargets_.size(); }
    /// Return number of commands.
    unsigned GetNumCommands() const { return commands_.size(); }
    /// Return command at index, or null if does not exist.
    RenderPathCommand* GetCommand(unsigned index) { return index < commands_.size() ? &commands_[index] : (RenderPathCommand*)nullptr; }
    /// Return a shader parameter (first appearance in any command.)
    const Variant& GetShaderParameter(const QString& name) const;

    /// Rendertargets.
    std::vector<RenderTargetInfo> renderTargets_;
    /// Rendering commands.
    std::vector<RenderPathCommand> commands_;
};

}
