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

#include "Lutefisk3D/Math/Color.h"
#include "Lutefisk3D/Graphics/GraphicsDefs.h"
#include "Lutefisk3D/Container/Ptr.h"
#include "Lutefisk3D/Container/RefCounted.h"
#include "Lutefisk3D/Math/Vector4.h"
#include "Lutefisk3D/Core/Variant.h"

#include <QString>
#include <vector>
#include <stdint.h>

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
    CMD_RENDERUI,
    CMD_SENDEVENT
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
struct LUTEFISK3D_EXPORT RenderTargetInfo
{
    /// Read from an XML element.
    void Load(const XMLElement& element);
    QString              name_;                        //!< Name.
    QString              tag_;                         //!< Tag name.
    uint32_t             format_;                      //!< Texture format.
    Vector2              size_        = Vector2::ZERO; //!< Absolute size or multiplier.
    RenderTargetSizeMode sizeMode_    = SIZE_ABSOLUTE; //!< Size mode.
    int                  multiSample_ = 1;             //!< Multisampling level (1 = no multisampling).
    bool                 autoResolve_ = true;          //!< Multisampling autoresolve flag.
    bool                 enabled_     = true;          //!< Enabled flag.
    bool                 cubemap_     = false;         //!< Cube map flag.
    bool                 filtered_    = false;         //!< Filtering flag.
    bool                 sRGB_        = false;         //!< sRGB sampling/writing mode flag.
    bool persistent_ = false; //!< Should be persistent and not shared/reused between other buffers of same size.
};

/// Rendering path command.
struct LUTEFISK3D_EXPORT RenderPathCommand
{
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
    size_t GetNumOutputs() const { return outputs_.size(); }
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
    unsigned clearFlags_=0;
    /// Clear color. Affects clear command only.
    Color clearColor_;
    /// Clear depth. Affects clear command only.
    float clearDepth_;
    /// Clear stencil value. Affects clear command only.
    unsigned clearStencil_;
    /// Blend mode. Affects quad command only.
    BlendMode blendMode_=BLEND_REPLACE;
    /// Enabled flag.
    bool enabled_=true;
    /// Use fog color for clearing.
    bool useFogColor_=false;
    /// Mark to stencil flag.
    bool markToStencil_=false;
    /// Use lit base pass optimization for forward per-pixel lights.
    bool useLitBase_=true;
    /// Vertex lights flag.
    bool vertexLights_=false;
    /// Event name.
    QString eventName_;
};

/// Rendering path definition. A sequence of commands (e.g. clear screen, draw objects with specific pass) that yields the scene rendering result.
class LUTEFISK3D_EXPORT RenderPath : public RefCounted
{
public:
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
    size_t GetNumRenderTargets() const { return renderTargets_.size(); }
    /// Return number of commands.
    size_t GetNumCommands() const { return commands_.size(); }
    /// Return command at index, or null if does not exist.
    RenderPathCommand* GetCommand(unsigned index) { return index < commands_.size() ? &commands_[index] : nullptr; }
    /// Return a shader parameter (first appearance in any command.)
    const Variant& GetShaderParameter(const QString& name) const;

    /// Rendertargets.
    std::vector<RenderTargetInfo> renderTargets_;
    /// Rendering commands.
    std::vector<RenderPathCommand> commands_;
};

}
