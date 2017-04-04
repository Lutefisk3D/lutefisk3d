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

#include "Batch.h"

#include "Camera.h"
#include "Geometry.h"
#include "Graphics.h"
#include "GraphicsDefs.h"
#include "GraphicsImpl.h"
#include "Material.h"
#include "Renderer.h"
#include "ShaderVariation.h"
#include "Technique.h"
#include "Texture2D.h"
#include "VertexBuffer.h"
#include "View.h"
#include "Zone.h"
#include "../Core/Profiler.h"
#include "../Scene/Node.h"
#include "../Scene/Scene.h"


namespace Urho3D
{

inline bool CompareBatchesState(Batch* lhs, Batch* rhs)
{
    if (lhs->renderOrder_ != rhs->renderOrder_)
        return lhs->renderOrder_ < rhs->renderOrder_;
    if (lhs->sortKey_ != rhs->sortKey_)
        return lhs->sortKey_ < rhs->sortKey_;
    return lhs->distance_ < rhs->distance_;
}

inline bool CompareBatchesFrontToBack(Batch* lhs, Batch* rhs)
{
    if (lhs->renderOrder_ != rhs->renderOrder_)
        return lhs->renderOrder_ < rhs->renderOrder_;
    if (lhs->distance_ != rhs->distance_)
        return lhs->distance_ < rhs->distance_;
    return lhs->sortKey_ < rhs->sortKey_;
}

inline bool CompareBatchesBackToFront(Batch* lhs, Batch* rhs)
{
    if (lhs->renderOrder_ != rhs->renderOrder_)
        return lhs->renderOrder_ < rhs->renderOrder_;
    if (lhs->distance_ != rhs->distance_)
        return lhs->distance_ > rhs->distance_;
    return lhs->sortKey_ < rhs->sortKey_;
}

inline bool CompareInstancesFrontToBack(const InstanceData& lhs, const InstanceData& rhs)
{
    return lhs.distance_ < rhs.distance_;
}

inline bool CompareBatchGroupOrder(BatchGroup* lhs, BatchGroup* rhs)
{
    return lhs->renderOrder_ < rhs->renderOrder_;
}

void CalculateShadowMatrix(Matrix4& dest, LightBatchQueue* queue, unsigned split, Renderer* renderer)
{
    Camera* shadowCamera = queue->shadowSplits_[split].shadowCamera_;
    const IntRect& viewport = queue->shadowSplits_[split].shadowViewport_;

    const Matrix3x4 &shadowView(shadowCamera->GetView());
    Matrix4 shadowProj(shadowCamera->GetGPUProjection());
    Matrix4 texAdjust(Matrix4::IDENTITY);

    Texture2D* shadowMap = queue->shadowMap_;
    if (nullptr==shadowMap)
        return;

    float width = (float)shadowMap->GetWidth();
    float height = (float)shadowMap->GetHeight();

    Vector3 offset(
                (float)viewport.left_ / width,
                (float)viewport.top_ / height,
                0.0f
                );

    Vector3 scale(
                0.5f * (float)viewport.Width() / width,
                0.5f * (float)viewport.Height() / height,
                1.0f
                );
    // Add pixel-perfect offset if needed by the graphics API
    const Vector2& pixelUVOffset = Graphics::GetPixelUVOffset();
    offset.x_ += scale.x_ + pixelUVOffset.x_ / width;
    offset.y_ += scale.y_ + pixelUVOffset.y_ / height;

    offset.z_ = 0.5f;
    scale.z_ = 0.5f;
    offset.y_ = 1.0f - offset.y_;

    // If using 4 shadow samples, offset the position diagonally by half pixel
    if (renderer->GetShadowQuality() == SHADOWQUALITY_PCF_16BIT || renderer->GetShadowQuality() == SHADOWQUALITY_PCF_24BIT)
    {
        offset.x_ -= 0.5f / width;
        offset.y_ -= 0.5f / height;
    }
    texAdjust.SetTranslation(offset);
    texAdjust.SetScale(scale);

    dest = texAdjust * shadowProj * shadowView;
}

void CalculateSpotMatrix(Matrix4& dest, Light* light)
{
    Node* lightNode = light->GetNode();
    Matrix3x4 spotView = Matrix3x4(lightNode->GetWorldPosition(), lightNode->GetWorldRotation(), 1.0f).Inverse();
    Matrix4 spotProj(Matrix4::ZERO);
    Matrix4 texAdjust(Matrix4::IDENTITY);

    // Make the projected light slightly smaller than the shadow map to prevent light spill
    float h = 1.005f / tanf(light->GetFov() * M_DEGTORAD * 0.5f);
    float w = h / light->GetAspectRatio();
    spotProj.m00_ = w;
    spotProj.m11_ = h;
    spotProj.m22_ = 1.0f / Max(light->GetRange(), M_EPSILON);
    spotProj.m32_ = 1.0f;

    texAdjust.SetTranslation(Vector3(0.5f, 0.5f, 0.5f));
    texAdjust.SetScale(Vector3(0.5f, -0.5f, 0.5f));

    dest = texAdjust * spotProj * spotView;
}

void Batch::CalculateSortKey()
{
    unsigned shaderID = (unsigned)(
        ((*((unsigned*)&vertexShader_) / sizeof(ShaderVariation)) + (*((unsigned*)&pixelShader_) / sizeof(ShaderVariation))) &
        0x7fff);
    if (!isBase_)
        shaderID |= 0x8000;

    unsigned lightQueueID = (unsigned)((*((unsigned*)&lightQueue_) / sizeof(LightBatchQueue)) & 0xffff);
    unsigned materialID = (unsigned)((*((unsigned*)&material_) / sizeof(Material)) & 0xffff);
    unsigned geometryID = (unsigned)((*((unsigned*)&geometry_) / sizeof(Geometry)) & 0xffff);

    sortKey_ = (((unsigned long long)shaderID) << 48) | (((unsigned long long)lightQueueID) << 32) |
               (((unsigned long long)materialID) << 16) | geometryID;
}

void Batch::Prepare(View* view, const Camera* camera, bool setModelTransform, bool allowDepthWrite) const
{
    if (nullptr==vertexShader_ || nullptr==pixelShader_)
        return;

    Graphics* graphics = view->GetGraphics();
    Renderer* renderer = view->GetRenderer();
    Node* cameraNode = camera!=nullptr ? camera->GetNode() : nullptr;
    Light* light = lightQueue_ ? lightQueue_->light_ : nullptr;
    Texture2D* shadowMap = lightQueue_ ? lightQueue_->shadowMap_ : nullptr;
    // Set shaders first. The available shader parameters and their register/uniform positions depend on the currently set shaders
    graphics->SetShaders(vertexShader_, pixelShader_);

    // Set pass / material-specific renderstates
    if (pass_!=nullptr && nullptr!=material_)
    {
        assert(camera!=nullptr);
        BlendMode blend = pass_->GetBlendMode();
        // Turn additive blending into subtract if the light is negative
        if (nullptr!=light && light->IsNegative())
        {
            if (blend == BLEND_ADD)
                blend = BLEND_SUBTRACT;
            else if (blend == BLEND_ADDALPHA)
                blend = BLEND_SUBTRACTALPHA;
        }
        graphics->SetBlendMode(blend, pass_->GetAlphaToCoverage() || material_->GetAlphaToCoverage());
        graphics->SetLineAntiAlias(material_->GetLineAntiAlias());

        bool isShadowPass = pass_->GetIndex() == Technique::shadowPassIndex;
        CullMode effectiveCullMode = pass_->GetCullMode();
        // Get cull mode from material if pass doesn't override it
        if (effectiveCullMode == MAX_CULLMODES)
            effectiveCullMode = isShadowPass ? material_->GetShadowCullMode() : material_->GetCullMode();

        renderer->SetCullMode(effectiveCullMode, camera);
        if (!isShadowPass)
        {
            const BiasParameters& depthBias = material_->GetDepthBias();
            graphics->SetDepthBias(depthBias.constantBias_, depthBias.slopeScaledBias_);
        }
        // Use the "least filled" fill mode combined from camera & material
        graphics->SetFillMode((FillMode)(Max(camera->GetFillMode(), material_->GetFillMode())));
        graphics->SetDepthTest(pass_->GetDepthTestMode());
        graphics->SetDepthWrite(pass_->GetDepthWrite() && allowDepthWrite);
    }


    // Set global (per-frame) shader parameters
    if (graphics->NeedParameterUpdate(SP_FRAME, (void*)nullptr))
        view->SetGlobalShaderParameters();

    // Set camera & viewport shader parameters
    unsigned cameraHash = (unsigned)(size_t)camera;
    IntRect viewport = graphics->GetViewport();
    IntVector2 viewSize = IntVector2(viewport.Width(), viewport.Height());
    unsigned viewportHash = viewSize.x_ | (viewSize.y_ << 16);

    if (graphics->NeedParameterUpdate(SP_CAMERA, reinterpret_cast<const void*>(cameraHash + viewportHash)))
    {
        if(camera)
            view->SetCameraShaderParameters(*camera);
        // During renderpath commands the G-Buffer or viewport texture is assumed to always be viewport-sized
        view->SetGBufferShaderParameters(viewSize, IntRect(0, 0, viewSize.x_, viewSize.y_));
    }

    // Set model or skinning transforms
    if (setModelTransform && graphics->NeedParameterUpdate(SP_OBJECT, worldTransform_))
    {
        assert(camera);
        if (geometryType_ == GEOM_SKINNED)
        {
            graphics->SetShaderParameter(VSP_SKINMATRICES, worldTransform_->Data(), 12 * numWorldTransforms_);
        }
        else
            graphics->SetShaderParameter(VSP_MODEL, *worldTransform_);

        // Set the orientation for billboards, either from the object itself or from the camera
        if (geometryType_ == GEOM_BILLBOARD)
        {
            if (numWorldTransforms_ > 1)
                graphics->SetShaderParameter(VSP_BILLBOARDROT, worldTransform_[1].RotationMatrix());
            else
                graphics->SetShaderParameter(VSP_BILLBOARDROT, cameraNode->GetWorldRotation().RotationMatrix());
        }
    }

    // Set zone-related shader parameters
    BlendMode blend = graphics->GetBlendMode();
    // If the pass is additive, override fog color to black so that shaders do not need a separate additive path
    bool overrideFogColorToBlack = blend == BLEND_ADD || blend == BLEND_ADDALPHA;
    unsigned zoneHash = (unsigned)(size_t)zone_;
    if (overrideFogColorToBlack)
        zoneHash += 0x80000000;
    if (zone_!=nullptr && graphics->NeedParameterUpdate(SP_ZONE, reinterpret_cast<const void*>(zoneHash)))
    {
        assert(camera!=nullptr);
        graphics->SetShaderParameter(VSP_AMBIENTSTARTCOLOR, zone_->GetAmbientStartColor());
        graphics->SetShaderParameter(VSP_AMBIENTENDCOLOR, zone_->GetAmbientEndColor().ToVector4() - zone_->GetAmbientStartColor().ToVector4());

        const BoundingBox& box = zone_->GetBoundingBox();
        Vector3 boxSize = box.size();
        Matrix3x4 adjust(Matrix3x4::IDENTITY);
        adjust.SetScale(Vector3(1.0f / boxSize.x_, 1.0f / boxSize.y_, 1.0f / boxSize.z_));
        adjust.SetTranslation(Vector3(0.5f, 0.5f, 0.5f));
        Matrix3x4 zoneTransform = adjust * zone_->GetInverseWorldTransform();
        graphics->SetShaderParameter(VSP_ZONE, zoneTransform);

        graphics->SetShaderParameter(PSP_AMBIENTCOLOR, zone_->GetAmbientColor());
        graphics->SetShaderParameter(PSP_FOGCOLOR, overrideFogColorToBlack ? Color::BLACK : zone_->GetFogColor());
        graphics->SetShaderParameter(PSP_ZONEMIN, zone_->GetBoundingBox().min_);
        graphics->SetShaderParameter(PSP_ZONEMAX, zone_->GetBoundingBox().max_);

        float farClip = camera->GetFarClip();
        float fogStart = std::min(zone_->GetFogStart(), farClip);
        float fogEnd = std::min(zone_->GetFogEnd(), farClip);
        if (fogStart >= fogEnd * (1.0f - M_LARGE_EPSILON))
            fogStart = fogEnd * (1.0f - M_LARGE_EPSILON);
        float fogRange = std::max(fogEnd - fogStart, M_EPSILON);
        Vector4 fogParams(fogEnd / farClip, farClip / fogRange, 0.0f, 0.0f);

        Node* zoneNode = zone_->GetNode();
        if (zone_->GetHeightFog() && zoneNode)
        {
            Vector3 worldFogHeightVec = zoneNode->GetWorldTransform() * Vector3(0.0f, zone_->GetFogHeight(), 0.0f);
            fogParams.z_ = worldFogHeightVec.y_;
            fogParams.w_ = zone_->GetFogHeightScale() / Max(zoneNode->GetWorldScale().y_, M_EPSILON);
        }

        graphics->SetShaderParameter(PSP_FOGPARAMS, fogParams);
    }

    // Set light-related shader parameters
    if (nullptr!=lightQueue_)
    {
        if (light && graphics->NeedParameterUpdate(SP_LIGHT, lightQueue_))
        {
            assert(camera!=nullptr);

            Node* lightNode = light->GetNode();
            float atten = 1.0f / Max(light->GetRange(), M_EPSILON);
            Vector3 lightDir(lightNode->GetWorldRotation() * Vector3::BACK);
            Vector4 lightPos(lightNode->GetWorldPosition(), atten);

            graphics->SetShaderParameter(VSP_LIGHTDIR, lightDir);
            graphics->SetShaderParameter(VSP_LIGHTPOS, lightPos);

            if (graphics->HasShaderParameter(VSP_LIGHTMATRICES))
            {
                switch (light->GetLightType())
                {
                case LIGHT_DIRECTIONAL:
                {
                    Matrix4 shadowMatrices[MAX_CASCADE_SPLITS];
                    unsigned numSplits = std::min<unsigned>(MAX_CASCADE_SPLITS, lightQueue_->shadowSplits_.size());
                    for (unsigned i = 0; i < numSplits; ++i)
                        CalculateShadowMatrix(shadowMatrices[i], lightQueue_, i, renderer);

                    graphics->SetShaderParameter(VSP_LIGHTMATRICES, shadowMatrices[0].Data(), 16 * numSplits);
                    break;
                }

                case LIGHT_SPOT:
                {
                    Matrix4 shadowMatrices[2];

                    CalculateSpotMatrix(shadowMatrices[0], light);
                    bool isShadowed = shadowMap!=nullptr && graphics->HasTextureUnit(TU_SHADOWMAP);
                    if (isShadowed)
                        CalculateShadowMatrix(shadowMatrices[1], lightQueue_, 0, renderer);

                    graphics->SetShaderParameter(VSP_LIGHTMATRICES, shadowMatrices[0].Data(), isShadowed ? 32 : 16);
                    break;
                }

                case LIGHT_POINT:
                {
                    Matrix4 lightVecRot(lightNode->GetWorldRotation().RotationMatrix());
                    // HLSL compiler will pack the parameters as if the matrix is only 3x4, so must be careful to not overwrite
                    // the next parameter
                    graphics->SetShaderParameter(VSP_LIGHTMATRICES, lightVecRot.Data(), 16);
                }
                    break;
                }
            }

            float fade = 1.0f;
            float fadeEnd = light->GetDrawDistance();
            float fadeStart = light->GetFadeDistance();

            // Do fade calculation for light if both fade & draw distance defined
            if (light->GetLightType() != LIGHT_DIRECTIONAL && fadeEnd > 0.0f && fadeStart > 0.0f && fadeStart < fadeEnd)
                fade = std::min(1.0f - (light->GetDistance() - fadeStart) / (fadeEnd - fadeStart), 1.0f);

            // Negative lights will use subtract blending, so write absolute RGB values to the shader parameter
            graphics->SetShaderParameter(PSP_LIGHTCOLOR, Color(light->GetEffectiveColor().Abs(),
                                                               light->GetEffectiveSpecularIntensity()) * fade);
            graphics->SetShaderParameter(PSP_LIGHTDIR, lightDir);
            graphics->SetShaderParameter(PSP_LIGHTPOS, lightPos);
            graphics->SetShaderParameter(PSP_LIGHTRAD, light->GetRadius());
            graphics->SetShaderParameter(PSP_LIGHTLENGTH, light->GetLength());

            if (graphics->HasShaderParameter(PSP_LIGHTMATRICES))
            {
                switch (light->GetLightType())
                {
                case LIGHT_DIRECTIONAL:
                {
                    Matrix4 shadowMatrices[MAX_CASCADE_SPLITS];
                    unsigned numSplits = std::min<unsigned>(MAX_CASCADE_SPLITS, lightQueue_->shadowSplits_.size());
                    for (unsigned i = 0; i < numSplits; ++i)
                    {
                        CalculateShadowMatrix(shadowMatrices[i], lightQueue_, i, renderer);
                    }
                    graphics->SetShaderParameter(PSP_LIGHTMATRICES, shadowMatrices[0].Data(), 16 * numSplits);
                }
                    break;

                case LIGHT_SPOT:
                {
                    Matrix4 shadowMatrices[2];

                    CalculateSpotMatrix(shadowMatrices[0], light);
                    bool isShadowed = lightQueue_->shadowMap_ != nullptr;
                    if (isShadowed)
                    {
                        CalculateShadowMatrix(shadowMatrices[1], lightQueue_, 0, renderer);
                    }

                    graphics->SetShaderParameter(PSP_LIGHTMATRICES, shadowMatrices[0].Data(), isShadowed ? 32 : 16);
                }
                    break;

                case LIGHT_POINT:
                {
                    Matrix4 lightVecRot(lightNode->GetWorldRotation().RotationMatrix());
                        // HLSL compiler will pack the parameters as if the matrix is only 3x4, so must be careful to not overwrite
                        // the next parameter
                    graphics->SetShaderParameter(PSP_LIGHTMATRICES, lightVecRot.Data(), 16);
                }
                    break;
                }
            }

            // Set shadow mapping shader parameters
            if (nullptr!=shadowMap)
            {
                {
                    // Calculate point light shadow sampling offsets (unrolled cube map)
                    unsigned faceWidth = shadowMap->GetWidth() / 2;
                    unsigned faceHeight = shadowMap->GetHeight() / 3;
                    float width = (float)shadowMap->GetWidth();
                    float height = (float)shadowMap->GetHeight();
                    float mulX = (float)(faceWidth - 3) / width;
                    float mulY = (float)(faceHeight - 3) / height;
                    float addX = 1.5f / width;
                    float addY = 1.5f / height;
                    // If using 4 shadow samples, offset the position diagonally by half pixel
                    if (renderer->GetShadowQuality() == SHADOWQUALITY_PCF_16BIT || renderer->GetShadowQuality() == SHADOWQUALITY_PCF_24BIT)
                    {
                        addX -= 0.5f / width;
                        addY -= 0.5f / height;
                    }
                    graphics->SetShaderParameter(PSP_SHADOWCUBEADJUST, Vector4(mulX, mulY, addX, addY));
                }

                {
                    // Calculate shadow camera depth parameters for point light shadows and shadow fade parameters for
                    //  directional light shadows, stored in the same uniform
                    Camera* shadowCamera = lightQueue_->shadowSplits_[0].shadowCamera_;
                    float nearClip = shadowCamera->GetNearClip();
                    float farClip = shadowCamera->GetFarClip();
                    float q = farClip / (farClip - nearClip);
                    float r = -q * nearClip;

                    const CascadeParameters& parameters = light->GetShadowCascade();
                    float viewFarClip = camera->GetFarClip();
                    float shadowRange = parameters.GetShadowRange();
                    float fadeStart = parameters.fadeStart_ * shadowRange / viewFarClip;
                    float fadeEnd = shadowRange / viewFarClip;
                    float fadeRange = fadeEnd - fadeStart;

                    graphics->SetShaderParameter(PSP_SHADOWDEPTHFADE, Vector4(q, r, fadeStart, 1.0f / fadeRange));
                }

                {
                    float intensity = light->GetShadowIntensity();
                    float fadeStart = light->GetShadowFadeDistance();
                    float fadeEnd = light->GetShadowDistance();
                    if (fadeStart > 0.0f && fadeEnd > 0.0f && fadeEnd > fadeStart)
                        intensity = Lerp(intensity, 1.0f, Clamp((light->GetDistance() - fadeStart) / (fadeEnd - fadeStart), 0.0f, 1.0f));
                    float pcfValues = (1.0f - intensity);
                    float samples = 1.0f;
                    if (renderer->GetShadowQuality() == SHADOWQUALITY_PCF_16BIT || renderer->GetShadowQuality() == SHADOWQUALITY_PCF_24BIT)
                        samples = 4.0f;

                    graphics->SetShaderParameter(PSP_SHADOWINTENSITY, Vector4(pcfValues / samples, intensity, 0.0f, 0.0f));
                }

                float sizeX = 1.0f / (float)shadowMap->GetWidth();
                float sizeY = 1.0f / (float)shadowMap->GetHeight();
                graphics->SetShaderParameter(PSP_SHADOWMAPINVSIZE, Vector2(sizeX, sizeY));

                Vector4 lightSplits(M_LARGE_VALUE, M_LARGE_VALUE, M_LARGE_VALUE, M_LARGE_VALUE);
                if (lightQueue_->shadowSplits_.size() > 1)
                    lightSplits.x_ = lightQueue_->shadowSplits_[0].farSplit_ / camera->GetFarClip();
                if (lightQueue_->shadowSplits_.size() > 2)
                    lightSplits.y_ = lightQueue_->shadowSplits_[1].farSplit_ / camera->GetFarClip();
                if (lightQueue_->shadowSplits_.size() > 3)
                    lightSplits.z_ = lightQueue_->shadowSplits_[2].farSplit_ / camera->GetFarClip();

                graphics->SetShaderParameter(PSP_SHADOWSPLITS, lightSplits);

                if (graphics->HasShaderParameter(PSP_VSMSHADOWPARAMS))
                    graphics->SetShaderParameter(PSP_VSMSHADOWPARAMS, renderer->GetVSMShadowParameters());

                if (light->GetShadowBias().normalOffset_ > 0.0f)
                {
                    Vector4 normalOffsetScale(Vector4::ZERO);

                    // Scale normal offset strength with the width of the shadow camera view
                    if (light->GetLightType() != LIGHT_DIRECTIONAL)
                    {
                        Camera* shadowCamera = lightQueue_->shadowSplits_[0].shadowCamera_;
                        normalOffsetScale.x_ = 2.0f * tanf(shadowCamera->GetFov() * M_DEGTORAD * 0.5f) * shadowCamera->GetFarClip();
                    }
                    else
                    {
                        normalOffsetScale.x_ = lightQueue_->shadowSplits_[0].shadowCamera_->GetOrthoSize();
                        if (lightQueue_->shadowSplits_.size() > 1)
                            normalOffsetScale.y_ = lightQueue_->shadowSplits_[1].shadowCamera_->GetOrthoSize();
                        if (lightQueue_->shadowSplits_.size() > 2)
                            normalOffsetScale.z_ = lightQueue_->shadowSplits_[2].shadowCamera_->GetOrthoSize();
                        if (lightQueue_->shadowSplits_.size() > 3)
                            normalOffsetScale.w_ = lightQueue_->shadowSplits_[3].shadowCamera_->GetOrthoSize();
                    }

                    normalOffsetScale *= light->GetShadowBias().normalOffset_;
                    graphics->SetShaderParameter(VSP_NORMALOFFSETSCALE, normalOffsetScale);
                    graphics->SetShaderParameter(PSP_NORMALOFFSETSCALE, normalOffsetScale);
                }

            }
        }
        else if (!lightQueue_->vertexLights_.empty() && graphics->HasShaderParameter(VSP_VERTEXLIGHTS) &&
                 graphics->NeedParameterUpdate(SP_LIGHT, lightQueue_))
        {
            Vector4 vertexLights[MAX_VERTEX_LIGHTS * 3];
            const std::vector<Light*>& lights = lightQueue_->vertexLights_;

            for (unsigned i = 0; i < lights.size(); ++i)
            {
                Light* vertexLight = lights[i];
                Node* vertexLightNode = vertexLight->GetNode();
                LightType type = vertexLight->GetLightType();

                // Attenuation
                float invRange, cutoff, invCutoff;
                if (type == LIGHT_DIRECTIONAL)
                    invRange = 0.0f;
                else
                    invRange = 1.0f / Max(vertexLight->GetRange(), M_EPSILON);
                if (type == LIGHT_SPOT)
                {
                    cutoff = Cos(vertexLight->GetFov() * 0.5f);
                    invCutoff = 1.0f / (1.0f - cutoff);
                }
                else
                {
                    cutoff = -1.0f;
                    invCutoff = 1.0f;
                }

                // Color
                float fade = 1.0f;
                float fadeEnd = vertexLight->GetDrawDistance();
                float fadeStart = vertexLight->GetFadeDistance();

                // Do fade calculation for light if both fade & draw distance defined
                if (vertexLight->GetLightType() != LIGHT_DIRECTIONAL && fadeEnd > 0.0f && fadeStart > 0.0f && fadeStart < fadeEnd)
                    fade = std::min(1.0f - (vertexLight->GetDistance() - fadeStart) / (fadeEnd - fadeStart), 1.0f);

                Color color = vertexLight->GetEffectiveColor() * fade;
                vertexLights[i * 3] = Vector4(color.r_, color.g_, color.b_, invRange);

                // Direction
                vertexLights[i * 3 + 1] = Vector4(-(vertexLightNode->GetWorldDirection()), cutoff);

                // Position
                vertexLights[i * 3 + 2] = Vector4(vertexLightNode->GetWorldPosition(), invCutoff);
            }

            graphics->SetShaderParameter(VSP_VERTEXLIGHTS, vertexLights[0].Data(), lights.size() * 3 * 4);
        }
    }

    // Set zone texture if necessary
    if (zone_ && graphics->HasTextureUnit(TU_ZONE))
        graphics->SetTexture(TU_ZONE, zone_->GetZoneTexture());
    // Set material-specific shader parameters and textures
    if (nullptr!=material_)
    {
        if (graphics->NeedParameterUpdate(SP_MATERIAL, reinterpret_cast<const void*>(material_->GetShaderParameterHash())))
        {
            const HashMap<StringHash, MaterialShaderParameter>& parameters(material_->GetShaderParameters());
            for (auto iter= parameters.begin(), fin=parameters.end(); iter!=fin; ++iter)
                graphics->SetShaderParameter(MAP_KEY(iter), MAP_VALUE(iter).value_);
        }

        const HashMap<TextureUnit, SharedPtr<Texture> >& textures = material_->GetTextures();
        for (auto i = textures.begin(); i != textures.end(); ++i)
        {
            if (graphics->HasTextureUnit(MAP_KEY(i)))
                graphics->SetTexture(MAP_KEY(i), MAP_VALUE(i).Get());
        }
    }

    // Set light-related textures
    if (nullptr!=light)
    {
        if (nullptr!=shadowMap && graphics->HasTextureUnit(TU_SHADOWMAP))
            graphics->SetTexture(TU_SHADOWMAP, shadowMap);
        if (graphics->HasTextureUnit(TU_LIGHTRAMP))
        {
            Texture* rampTexture = light->GetRampTexture();
            if (nullptr==rampTexture)
                rampTexture = renderer->GetDefaultLightRamp();
            graphics->SetTexture(TU_LIGHTRAMP, rampTexture);
        }
        if (graphics->HasTextureUnit(TU_LIGHTSHAPE))
        {
            Texture* shapeTexture = light->GetShapeTexture();
            if (nullptr==shapeTexture && light->GetLightType() == LIGHT_SPOT)
                shapeTexture = renderer->GetDefaultLightSpot();
            graphics->SetTexture(TU_LIGHTSHAPE, shapeTexture);
        }
    }
}

void Batch::Draw(View* view, Camera* camera, bool allowDepthWrite) const
{
    if (!geometry_->IsEmpty())
    {
        Prepare(view, camera, true, allowDepthWrite);
        geometry_->Draw(view->GetGraphics());
    }
}

void BatchGroup::SetInstancingData(void* lockedData, unsigned stride, unsigned& freeIndex)
{
    // Do not use up buffer space if not going to draw as instanced
    if (geometryType_ != GEOM_INSTANCED)
        return;

    startIndex_ = freeIndex;
    unsigned char* buffer = static_cast<unsigned char*>(lockedData) + startIndex_ * stride;

    for (const InstanceData& instance : instances_)
    {
        memcpy(buffer, instance.worldTransform_, sizeof(Matrix3x4));
        if (instance.instancingData_)
            memcpy(buffer + sizeof(Matrix3x4), instance.instancingData_, stride - sizeof(Matrix3x4));

        buffer += stride;
    }

    freeIndex += instances_.size();
}

void BatchGroup::Draw(View *view, Camera *camera, bool allowDepthWrite) const
{

    if (instances_.empty() || geometry_->IsEmpty())
        return;

    Graphics *graphics = view->GetGraphics();
    Renderer *renderer = view->GetRenderer();

    // Draw as individual objects if instancing not supported or could not fill the instancing buffer
    VertexBuffer *instanceBuffer = renderer->GetInstancingBuffer();
    if (nullptr==instanceBuffer || geometryType_ != GEOM_INSTANCED || startIndex_ == M_MAX_UNSIGNED)
    {
        Batch::Prepare(view, camera, false, allowDepthWrite);

        graphics->SetIndexBuffer(geometry_->GetIndexBuffer());
        graphics->SetVertexBuffers(geometry_->GetVertexBuffers());

        for (unsigned i = 0; i < instances_.size(); ++i)
        {
            if (graphics->NeedParameterUpdate(SP_OBJECT, instances_[i].worldTransform_))
                graphics->SetShaderParameter(VSP_MODEL, *instances_[i].worldTransform_);

            graphics->Draw(geometry_->GetPrimitiveType(), geometry_->GetIndexStart(), geometry_->GetIndexCount(),
                           geometry_->GetVertexStart(), geometry_->GetVertexCount());
        }
    }
    else
    {
        Batch::Prepare(view, camera, false, allowDepthWrite);

        // Get the geometry vertex buffers, then add the instancing stream buffer
        // Hack: use a const_cast to avoid dynamic allocation of new temp vectors
        std::vector<SharedPtr<VertexBuffer>> &vertexBuffers =
                const_cast<std::vector<SharedPtr<VertexBuffer>> &>(geometry_->GetVertexBuffers());
        vertexBuffers.emplace_back(SharedPtr<VertexBuffer>(instanceBuffer));

        graphics->SetIndexBuffer(geometry_->GetIndexBuffer());
        graphics->SetVertexBuffers(vertexBuffers, startIndex_);
        graphics->DrawInstanced(geometry_->GetPrimitiveType(), geometry_->GetIndexStart(), geometry_->GetIndexCount(),
                                geometry_->GetVertexStart(), geometry_->GetVertexCount(), instances_.size());

        // Remove the instancing buffer & element mask now
        vertexBuffers.pop_back();
    }
}


void BatchQueue::Clear(int maxSortedInstances)
{
    batches_.clear();
    sortedBatches_.clear();
    batchGroupStorage_.clear();
    batchGroups_.clear();
    maxSortedInstances_ = maxSortedInstances;
}

void BatchQueue::SortBackToFront()
{
    sortedBatches_.resize(batches_.size());

    for (unsigned i = 0; i < batches_.size(); ++i)
    {
        sortedBatches_[i] = &batches_[i];
    }

    std::sort(sortedBatches_.begin(), sortedBatches_.end(), CompareBatchesBackToFront);

    sortedBatchGroups_.resize(batchGroupStorage_.size());

    unsigned index = 0;
    for (BatchGroup &elem : batchGroupStorage_)
        sortedBatchGroups_[index++] = &elem;

    std::sort(sortedBatchGroups_.begin(), sortedBatchGroups_.end(), CompareBatchGroupOrder);
}

void BatchQueue::SortFrontToBack()
{
    sortedBatches_.resize(batches_.size());

    for (unsigned i = 0; i < batches_.size(); ++i)
    {
        sortedBatches_[i] = &batches_[i];
    }

    SortFrontToBack2Pass(sortedBatches_);

    // Sort each group front to back
    for (BatchGroup &elem : batchGroupStorage_)
    {
        if (elem.instances_.size() <= maxSortedInstances_)
        {
            if (!elem.instances_.empty())
            {
                std::sort(elem.instances_.begin(), elem.instances_.end(), CompareInstancesFrontToBack);
                elem.distance_ = elem.instances_[0].distance_;
            }
        }
        else
        {
            float minDistance = M_INFINITY;
            for (const InstanceData &j : elem.instances_)
                minDistance = std::min(minDistance, j.distance_);
            elem.distance_  = minDistance;
        }
    }

    sortedBatchGroups_.resize(batchGroupStorage_.size());

    unsigned index = 0;
    for (BatchGroup &elem : batchGroupStorage_)
        sortedBatchGroups_[index++] = &elem;

    SortFrontToBack2Pass(reinterpret_cast<std::vector<Batch *> &>(sortedBatchGroups_));
}

void BatchQueue::SortFrontToBack2Pass(std::vector<Batch*>& batches)
{
    // For desktop, first sort by distance and remap shader/material/geometry IDs in the sort key
    std::sort(batches.begin(), batches.end(), CompareBatchesFrontToBack);

    unsigned freeShaderID = 0;
    unsigned short freeMaterialID = 0;
    unsigned short freeGeometryID = 0;

    for (Batch* batch : batches)
    {
        unsigned shaderID = (batch->sortKey_ >> 32);
        HashMap<unsigned, unsigned>::const_iterator j = shaderRemapping_.find(shaderID);
        if (j != shaderRemapping_.end())
            shaderID = MAP_VALUE(j);
        else
        {
            shaderID = shaderRemapping_[shaderID] = freeShaderID | (shaderID & 0x80000000);
            ++freeShaderID;
        }

        unsigned short materialID = (unsigned short)(batch->sortKey_ & 0xffff0000);
        HashMap<unsigned short, unsigned short>::const_iterator k = materialRemapping_.find(materialID);
        if (k != materialRemapping_.end())
            materialID = MAP_VALUE(k);
        else
        {
            materialID = materialRemapping_[materialID] = freeMaterialID;
            ++freeMaterialID;
        }

        unsigned short geometryID = (unsigned short)(batch->sortKey_ & 0xffff);
        HashMap<unsigned short, unsigned short>::const_iterator l = geometryRemapping_.find(geometryID);
        if (l != geometryRemapping_.end())
            geometryID = MAP_VALUE(l);
        else
        {
            geometryID = geometryRemapping_[geometryID] = freeGeometryID;
            ++freeGeometryID;
        }

        batch->sortKey_ = (((unsigned long long)shaderID) << 32) | (((unsigned long long)materialID) << 16) | geometryID;
    }

    shaderRemapping_.clear();
    materialRemapping_.clear();
    geometryRemapping_.clear();

    // Finally sort again with the rewritten ID's
    std::sort(batches.begin(), batches.end(), CompareBatchesState);
}

void BatchQueue::SetInstancingData(void* lockedData, unsigned stride, unsigned& freeIndex)
{
    for (BatchGroup & elem : batchGroupStorage_)
        elem.SetInstancingData(lockedData, stride, freeIndex);
}
void BatchQueue::Draw(View* view, Camera* camera, bool markToStencil, bool usingLightOptimization, bool allowDepthWrite) const
{
    Graphics* graphics = view->GetGraphics();
    Renderer* renderer = view->GetRenderer();

    // If View has set up its own light optimizations, do not disturb the stencil/scissor test settings
    if (!usingLightOptimization)
    {
        graphics->SetScissorTest(false);

        // During G-buffer rendering, mark opaque pixels' lightmask to stencil buffer if requested
        if (!markToStencil)
            graphics->SetStencilTest(false);
    }

    // Instanced
    for (BatchGroup* group : sortedBatchGroups_)
    {

        if (markToStencil)
            graphics->SetStencilTest(true, CMP_ALWAYS, OP_REF, OP_KEEP, OP_KEEP, group->lightMask_);

        group->Draw(view, camera, allowDepthWrite);
    }
    // Non-instanced
    for (Batch* batch : sortedBatches_)
    {

        if (markToStencil)
            graphics->SetStencilTest(true, CMP_ALWAYS, OP_REF, OP_KEEP, OP_KEEP, batch->lightMask_);
        if (!usingLightOptimization)
        {
            // If drawing an alpha batch, we can optimize fillrate by scissor test
            if (!batch->isBase_ && batch->lightQueue_!=nullptr)
                renderer->OptimizeLightByScissor(batch->lightQueue_->light_, camera);
            else
                graphics->SetScissorTest(false);
        }

        batch->Draw(view, camera, allowDepthWrite);
    }
}

unsigned BatchQueue::GetNumInstances() const
{
    unsigned total = 0;
    for (const BatchGroup & elem : batchGroupStorage_)
    {
        if (elem.geometryType_ == GEOM_INSTANCED)
            total += elem.instances_.size();
    }

    return total;
}

}
