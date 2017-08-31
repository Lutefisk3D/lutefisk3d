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

#include "ParticleEmitter.h"
#include "ParticleEffect.h"
#include "DrawableEvents.h"
#include "Lutefisk3D/Core/Context.h"
#include "Lutefisk3D/Core/Profiler.h"
#include "Lutefisk3D/Resource/ResourceCache.h"
#include "Lutefisk3D/Resource/ResourceEvents.h"
#include "Lutefisk3D/Scene/Scene.h"
#include "Lutefisk3D/Scene/SceneEvents.h"



namespace Urho3D
{

extern const char* GEOMETRY_CATEGORY;
extern const char* faceCameraModeNames[];
static const unsigned MAX_PARTICLES_IN_FRAME = 100;

extern const char* autoRemoveModeNames[];
ParticleEmitter::ParticleEmitter(Context* context) :
    BillboardSet(context),
    periodTimer_(0.0f),
    emissionTimer_(0.0f),
    lastTimeStep_(0.0f),
    lastUpdateFrameNumber_(M_MAX_UNSIGNED),
    emitting_(true),
    needUpdate_(false),
    serializeParticles_(true),
    sendFinishedEvent_(true),
    autoRemove_(REMOVE_DISABLED)
{
    SetNumParticles(DEFAULT_NUM_PARTICLES);
}

ParticleEmitter::~ParticleEmitter()
{
}

void ParticleEmitter::RegisterObject(Context* context)
{
    context->RegisterFactory<ParticleEmitter>(GEOMETRY_CATEGORY);

    URHO3D_ACCESSOR_ATTRIBUTE("Is Enabled", IsEnabled, SetEnabled, bool, true, AM_DEFAULT);
    URHO3D_MIXED_ACCESSOR_ATTRIBUTE("Effect", GetEffectAttr, SetEffectAttr, ResourceRef, ResourceRef(ParticleEffect::GetTypeStatic()), AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("Can Be Occluded", IsOccludee, SetOccludee, bool, true, AM_DEFAULT);
    URHO3D_ATTRIBUTE("Cast Shadows", bool, castShadows_, false, AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("Draw Distance", GetDrawDistance, SetDrawDistance, float, 0.0f, AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("Shadow Distance", GetShadowDistance, SetShadowDistance, float, 0.0f, AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("Animation LOD Bias", GetAnimationLodBias, SetAnimationLodBias, float, 1.0f, AM_DEFAULT);
    URHO3D_ATTRIBUTE("Is Emitting", bool, emitting_, true, AM_FILE);
    URHO3D_ATTRIBUTE("Period Timer", float, periodTimer_, 0.0f, AM_FILE | AM_NOEDIT);
    URHO3D_ATTRIBUTE("Emission Timer", float, emissionTimer_, 0.0f, AM_FILE | AM_NOEDIT);
    URHO3D_ENUM_ATTRIBUTE("Autoremove Mode", autoRemove_, autoRemoveModeNames, REMOVE_DISABLED, AM_DEFAULT);
    URHO3D_COPY_BASE_ATTRIBUTES(Drawable);
    URHO3D_MIXED_ACCESSOR_ATTRIBUTE("Particles", GetParticlesAttr, SetParticlesAttr, VariantVector, Variant::emptyVariantVector, AM_FILE | AM_NOEDIT);
    URHO3D_MIXED_ACCESSOR_ATTRIBUTE("Billboards", GetParticleBillboardsAttr, SetBillboardsAttr, VariantVector, Variant::emptyVariantVector, AM_FILE | AM_NOEDIT);
    URHO3D_ATTRIBUTE("Serialize Particles", bool, serializeParticles_, true, AM_FILE);
}

void ParticleEmitter::OnSetEnabled()
{
    BillboardSet::OnSetEnabled();

    Scene* scene = GetScene();
    if (scene)
    {
        if (IsEnabledEffective())
            scene->scenePostUpdate.Connect(this,&ParticleEmitter::HandleScenePostUpdate);
        else
            scene->scenePostUpdate.Disconnect(this,&ParticleEmitter::HandleScenePostUpdate);
    }
}

void ParticleEmitter::Update(const FrameInfo& frame)
{
    if (!effect_)
        return;

    // Cancel update if has only moved but does not actually need to animate the particles
    if (!needUpdate_)
        return;

    // If there is an amount mismatch between particles and billboards, correct it
    if (particles_.size() != billboards_.size())
        SetNumBillboards(particles_.size());

    bool needCommit = false;

    // Check active/inactive period switching
    periodTimer_ += lastTimeStep_;
    if (emitting_)
    {
        float activeTime = effect_->GetActiveTime();
        if (activeTime && periodTimer_ >= activeTime)
        {
            emitting_ = false;
            periodTimer_ -= activeTime;
        }
    }
    else
    {
        float inactiveTime = effect_->GetInactiveTime();
        if (inactiveTime && periodTimer_ >= inactiveTime)
        {
            emitting_ = true;
            sendFinishedEvent_ = true;
            periodTimer_ -= inactiveTime;
        }
        // If emitter has an indefinite stop interval, keep period timer reset to allow restarting emission in the editor
        if (inactiveTime == 0.0f)
            periodTimer_ = 0.0f;
    }

    // Check for emitting new particles
    if (emitting_)
    {
        emissionTimer_ += lastTimeStep_;

        float intervalMin = 1.0f / effect_->GetMaxEmissionRate();
        float intervalMax = 1.0f / effect_->GetMinEmissionRate();

        // If emission timer has a longer delay than max. interval, clamp it
        if (emissionTimer_ < -intervalMax)
            emissionTimer_ = -intervalMax;

        unsigned counter = MAX_PARTICLES_IN_FRAME;

        while (emissionTimer_ > 0.0f && counter)
        {
            emissionTimer_ -= Lerp(intervalMin, intervalMax, Random(1.0f));
            if (EmitNewParticle())
            {
                --counter;
                needCommit = true;
            }
            else
                break;
        }
    }

    // Update existing particles
    Vector3 relativeConstantForce = node_->GetWorldRotation().Inverse() * effect_->GetConstantForce();
    // If billboards are not relative, apply scaling to the position update
    Vector3 scaleVector = Vector3::ONE;
    if (scaled_ && !relative_)
        scaleVector = node_->GetWorldScale();

    for (unsigned i = 0; i < particles_.size(); ++i)
    {
        Particle& particle = particles_[i];
        Billboard& billboard = billboards_[i];

        if (billboard.enabled_)
        {
            needCommit = true;

            // Time to live
            if (particle.timer_ >= particle.timeToLive_)
            {
                billboard.enabled_ = false;
                continue;
            }
            particle.timer_ += lastTimeStep_;

            // Velocity & position
            const Vector3& constantForce = effect_->GetConstantForce();
            if (constantForce != Vector3::ZERO)
            {
                if (relative_)
                    particle.velocity_ += lastTimeStep_ * relativeConstantForce;
                else
                    particle.velocity_ += lastTimeStep_ * constantForce;
            }

            float dampingForce = effect_->GetDampingForce();
            if (dampingForce != 0.0f)
            {
                Vector3 force = -dampingForce * particle.velocity_;
                particle.velocity_ += lastTimeStep_ * force;
            }
            billboard.position_ += lastTimeStep_ * particle.velocity_ * scaleVector;
            billboard.direction_ = particle.velocity_.Normalized();

            // Rotation
            billboard.rotation_ += lastTimeStep_ * particle.rotationSpeed_;

            // Scaling
            float sizeAdd = effect_->GetSizeAdd();
            float sizeMul = effect_->GetSizeMul();
            if (sizeAdd != 0.0f || sizeMul != 1.0f)
            {
                particle.scale_ += lastTimeStep_ * sizeAdd;
                if (particle.scale_ < 0.0f)
                    particle.scale_ = 0.0f;
                if (sizeMul != 1.0f)
                    particle.scale_ *= (lastTimeStep_ * (sizeMul - 1.0f)) + 1.0f;
                billboard.size_ = particle.size_ * particle.scale_;
            }

            // Color interpolation
            unsigned& index = particle.colorIndex_;
            const std::vector<ColorFrame>& colorFrames_ = effect_->GetColorFrames();
            if (index < colorFrames_.size())
            {
                if (index < colorFrames_.size() - 1)
                {
                    if (particle.timer_ >= colorFrames_[index + 1].time_)
                        ++index;
                }
                if (index < colorFrames_.size() - 1)
                    billboard.color_ = colorFrames_[index].Interpolate(colorFrames_[index + 1], particle.timer_);
                else
                    billboard.color_ = colorFrames_[index].color_;
            }

            // Texture animation
            unsigned& texIndex = particle.texIndex_;
            const std::vector<TextureFrame>& textureFrames_ = effect_->GetTextureFrames();
            if (textureFrames_.size() && texIndex < textureFrames_.size() - 1)
            {
                if (particle.timer_ >= textureFrames_[texIndex + 1].time_)
                {
                    billboard.uv_ = textureFrames_[texIndex + 1].uv_;
                    ++texIndex;
                }
            }
        }
    }

    if (needCommit)
        Commit();

    needUpdate_ = false;
}

void ParticleEmitter::SetEffect(ParticleEffect* effect)
{
    if (effect == effect_)
        return;

    Reset();

    // Unsubscribe from the reload event of previous effect (if any), then subscribe to the new
    if (effect_)
        effect_->reloadFinished.Disconnect(this,&ParticleEmitter::HandleEffectReloadFinished);

    effect_ = effect;

    if (effect_)
        effect_->reloadFinished.Connect(this,&ParticleEmitter::HandleEffectReloadFinished);

    ApplyEffect();
    MarkNetworkUpdate();
}

void ParticleEmitter::SetNumParticles(unsigned num)
{
    // Prevent negative value being assigned from the editor
    if (num > M_MAX_INT)
        num = 0;

    particles_.resize(num);
    SetNumBillboards(num);
}

void ParticleEmitter::SetEmitting(bool enable)
{
    if (enable != emitting_)
    {
        emitting_ = enable;
        // If stopping emission now, and there are active particles, send finish event once they are gone
        sendFinishedEvent_ = enable || CheckActiveParticles();
        periodTimer_ = 0.0f;
        // Note: network update does not need to be marked as this is a file only attribute
    }
}

void ParticleEmitter::SetSerializeParticles(bool enable)
{
    serializeParticles_ = enable;
    // Note: network update does not need to be marked as this is a file only attribute
}

void ParticleEmitter::SetAutoRemoveMode(AutoRemoveMode mode)
{
    autoRemove_ = mode;
    MarkNetworkUpdate();
}
void ParticleEmitter::ResetEmissionTimer()
{
    emissionTimer_ = 0.0f;
}

void ParticleEmitter::RemoveAllParticles()
{
    for (Billboard & elem : billboards_)
        elem.enabled_ = false;

    Commit();
}

void ParticleEmitter::Reset()
{
    RemoveAllParticles();
    ResetEmissionTimer();
    SetEmitting(true);
}

void ParticleEmitter::ApplyEffect()
{
    if (!effect_)
        return;

    SetMaterial(effect_->GetMaterial());
    SetNumParticles(effect_->GetNumParticles());
    SetRelative(effect_->IsRelative());
    SetScaled(effect_->IsScaled());
    SetSorted(effect_->IsSorted());
    SetFixedScreenSize(effect_->IsFixedScreenSize());
    SetAnimationLodBias(effect_->GetAnimationLodBias());
    SetFaceCameraMode(effect_->GetFaceCameraMode());
}

void ParticleEmitter::SetEffectAttr(const ResourceRef& value)
{
    ResourceCache* cache =context_->m_ResourceCache.get();
    SetEffect(cache->GetResource<ParticleEffect>(value.name_));
}

ResourceRef ParticleEmitter::GetEffectAttr() const
{
    return GetResourceRef(effect_, ParticleEffect::GetTypeStatic());
}

void ParticleEmitter::SetParticlesAttr(const VariantVector& value)
{
    unsigned index = 0;
    SetNumParticles(index < value.size() ? value[index++].GetUInt() : 0);

    for (auto i = particles_.begin(); i != particles_.end() && index < value.size(); ++i)
    {
        i->velocity_ = value[index++].GetVector3();
        i->size_ = value[index++].GetVector2();
        i->timer_ = value[index++].GetFloat();
        i->timeToLive_ = value[index++].GetFloat();
        i->scale_ = value[index++].GetFloat();
        i->rotationSpeed_ = value[index++].GetFloat();
        i->colorIndex_ = (unsigned)value[index++].GetInt();
        i->texIndex_ = (unsigned)value[index++].GetInt();
    }
}

VariantVector ParticleEmitter::GetParticlesAttr() const
{
    VariantVector ret;
    if (!serializeParticles_)
    {
        ret.push_back(uint32_t(particles_.size()));
        return ret;
    }

    ret.reserve(particles_.size() * 8 + 1);
    ret.emplace_back(uint32_t(particles_.size()));
    for (const Particle & elem : particles_)
    {
        ret.push_back(elem.velocity_);
        ret.push_back(elem.size_);
        ret.push_back(elem.timer_);
        ret.push_back(elem.timeToLive_);
        ret.push_back(elem.scale_);
        ret.push_back(elem.rotationSpeed_);
        ret.push_back(elem.colorIndex_);
        ret.push_back(elem.texIndex_);
    }
    return ret;
}

VariantVector ParticleEmitter::GetParticleBillboardsAttr() const
{
    VariantVector ret;
    if (!serializeParticles_)
    {
        ret.emplace_back(uint32_t(billboards_.size()));
        return ret;
    }

    ret.reserve(billboards_.size() * 7 + 1);
    ret.emplace_back(uint32_t(billboards_.size()));

    for (const Billboard & elem : billboards_)
    {
        ret.push_back(elem.position_);
        ret.push_back(elem.size_);
        ret.push_back(Vector4(elem.uv_.min_.x_, elem.uv_.min_.y_, elem.uv_.max_.x_, elem.uv_.max_.y_));
        ret.push_back(elem.color_);
        ret.push_back(elem.rotation_);
        ret.push_back(elem.direction_);
        ret.push_back(elem.enabled_);
    }

    return ret;
}

void ParticleEmitter::OnSceneSet(Scene* scene)
{
    BillboardSet::OnSceneSet(scene);
    if (scene && IsEnabledEffective())
        scene->scenePostUpdate.Connect(this,&ParticleEmitter::HandleScenePostUpdate);
    else if (!scene)
        scene->scenePostUpdate.Disconnect(this,&ParticleEmitter::HandleScenePostUpdate);
}

bool ParticleEmitter::EmitNewParticle()
{
    unsigned index = GetFreeParticle();
    if (index == M_MAX_UNSIGNED)
        return false;
    assert(index < particles_.size());
    Particle& particle = particles_[index];
    Billboard& billboard = billboards_[index];

    Vector3 startDir;
    Vector3 startPos;

    startDir = effect_->GetRandomDirection();
    startDir.Normalize();

    switch (effect_->GetEmitterType())
    {
    case EMITTER_SPHERE:
    {
        Vector3 dir(
                    Random(2.0f) - 1.0f,
                    Random(2.0f) - 1.0f,
                    Random(2.0f) - 1.0f
                    );
        dir.Normalize();
        startPos = effect_->GetEmitterSize() * dir * 0.5f;
    }
        break;

    case EMITTER_BOX:
    {
        const Vector3& emitterSize = effect_->GetEmitterSize();
        startPos = Vector3(
                    Random(emitterSize.x_) - emitterSize.x_ * 0.5f,
                    Random(emitterSize.y_) - emitterSize.y_ * 0.5f,
                    Random(emitterSize.z_) - emitterSize.z_ * 0.5f
                    );
    }
        break;
    }

    particle.size_ = effect_->GetRandomSize();
    particle.timer_ = 0.0f;
    particle.timeToLive_ = effect_->GetRandomTimeToLive();
    particle.scale_ = 1.0f;
    particle.rotationSpeed_ = effect_->GetRandomRotationSpeed();
    particle.colorIndex_ = 0;
    particle.texIndex_ = 0;

    if(faceCameraMode_ == FC_DIRECTION)
    {
        startPos += startDir * particle.size_.y_;
    }

    if (!relative_)
    {
        startPos = node_->GetWorldTransform() * startPos;
        startDir = node_->GetWorldRotation() * startDir;
    };

    particle.velocity_ = effect_->GetRandomVelocity() * startDir;

    billboard.position_ = startPos;
    billboard.size_ = particles_[index].size_;
    const std::vector<TextureFrame>& textureFrames_ = effect_->GetTextureFrames();
    billboard.uv_ = textureFrames_.size() ? textureFrames_[0].uv_ : Rect::POSITIVE;
    billboard.rotation_ = effect_->GetRandomRotation();
    const std::vector<ColorFrame>& colorFrames_ = effect_->GetColorFrames();
    billboard.color_ = colorFrames_.size() ? colorFrames_[0].color_ : Color();
    billboard.enabled_ = true;
    billboard.direction_ = startDir;

    return true;
}

unsigned ParticleEmitter::GetFreeParticle() const
{
    for (unsigned i = 0; i < billboards_.size(); ++i)
    {
        if (!billboards_[i].enabled_)
            return i;
    }

    return M_MAX_UNSIGNED;
}

bool ParticleEmitter::CheckActiveParticles() const
{
    for (unsigned i = 0; i < billboards_.size(); ++i)
    {
        if (billboards_[i].enabled_)
        {
            return true;
            break;
        }
    }

    return false;
}
void ParticleEmitter::HandleScenePostUpdate(Scene *,float ts)
{
    // Store scene's timestep and use it instead of global timestep, as time scale may be other than 1
    lastTimeStep_ = ts;

    // If no invisible update, check that the billboardset is in view (framenumber has changed)
    if ((effect_ && effect_->GetUpdateInvisible()) || viewFrameNumber_ != lastUpdateFrameNumber_)
    {
        lastUpdateFrameNumber_ = viewFrameNumber_;
        needUpdate_ = true;
        MarkForUpdate();
    }

    // Send finished event only once all particles are gone
    if (node_ && !emitting_ && sendFinishedEvent_ && !CheckActiveParticles())
    {
        sendFinishedEvent_ = false;

        // Make a weak pointer to self to check for destruction during event handling
        WeakPtr<ParticleEmitter> self(this);

        using namespace ParticleEffectFinished;

        VariantMap& eventData = GetEventDataMap();
        eventData[P_NODE] = node_;
        eventData[P_EFFECT] = effect_;

        node_->SendEvent(E_PARTICLEEFFECTFINISHED, eventData);
        if (self.Expired())
            return;

        DoAutoRemove(autoRemove_);
    }
}

void ParticleEmitter::HandleEffectReloadFinished()
{
    // When particle effect file is live-edited, remove existing particles and reapply the effect parameters
    Reset();
    ApplyEffect();
}

}
