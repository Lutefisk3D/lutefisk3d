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

#include "AnimationController.h"

#include "AnimatedModel.h"
#include "Animation.h"
#include "AnimationState.h"
#include "../Core/Context.h"
#include "../IO/Log.h"
#include "../IO/MemoryBuffer.h"
#include "../Core/Profiler.h"
#include "../Resource/ResourceCache.h"
#include "../Scene/Scene.h"
#include "../Scene/SceneEvents.h"

namespace Urho3D
{

static const unsigned char CTRL_LOOPED = 0x1;
static const unsigned char CTRL_STARTBONE = 0x2;
static const unsigned char CTRL_AUTOFADE = 0x4;
static const unsigned char CTRL_SETTIME = 0x08;
static const unsigned char CTRL_SETWEIGHT = 0x10;
static const float EXTRA_ANIM_FADEOUT_TIME = 0.1f;
static const float COMMAND_STAY_TIME = 0.25f;
static const unsigned MAX_NODE_ANIMATION_STATES = 256;

extern const char* LOGIC_CATEGORY;

AnimationController::AnimationController(Context* context) :
    Component(context)
{
}

AnimationController::~AnimationController()
{
}

void AnimationController::RegisterObject(Context* context)
{
    context->RegisterFactory<AnimationController>(LOGIC_CATEGORY);

    ACCESSOR_ATTRIBUTE("Is Enabled", IsEnabled, SetEnabled, bool, true, AM_DEFAULT);
    MIXED_ACCESSOR_ATTRIBUTE("Animations", GetAnimationsAttr, SetAnimationsAttr, VariantVector, Variant::emptyVariantVector, AM_FILE | AM_NOEDIT);
    ACCESSOR_ATTRIBUTE("Network Animations", GetNetAnimationsAttr, SetNetAnimationsAttr, std::vector<unsigned char>, Variant::emptyBuffer, AM_NET | AM_LATESTDATA | AM_NOEDIT);
    MIXED_ACCESSOR_ATTRIBUTE("Node Animation States", GetNodeAnimationStatesAttr, SetNodeAnimationStatesAttr, VariantVector, Variant::emptyVariantVector, AM_FILE | AM_NOEDIT);
}

void AnimationController::OnSetEnabled()
{
    Scene* scene = GetScene();
    if (scene)
    {
        if (IsEnabledEffective())
            SubscribeToEvent(scene, E_SCENEPOSTUPDATE, HANDLER(AnimationController, HandleScenePostUpdate));
        else
            UnsubscribeFromEvent(scene, E_SCENEPOSTUPDATE);
    }
}

void AnimationController::Update(float timeStep)
{
    // Loop through animations
    for (std::vector<AnimationControl>::iterator i = animations_.begin(); i != animations_.end();)
    {
        bool remove = false;
        AnimationState* state = GetAnimationState(i->hash_);
        if (!state)
            remove = true;
        else
        {
            // Advance the animation
            if (i->speed_ != 0.0f)
                state->AddTime(i->speed_ * timeStep);

            float targetWeight = i->targetWeight_;
            float fadeTime = i->fadeTime_;

            // If non-looped animation at the end, activate autofade as applicable
            if (!state->IsLooped() && state->GetTime() >= state->GetLength() && i->autoFadeTime_ > 0.0f)
            {
                targetWeight = 0.0f;
                fadeTime = i->autoFadeTime_;
            }

            // Process weight fade
            float currentWeight = state->GetWeight();
            if (currentWeight != targetWeight)
            {
                if (fadeTime > 0.0f)
                {
                    float weightDelta = 1.0f / fadeTime * timeStep;
                    if (currentWeight < targetWeight)
                        currentWeight = Min(currentWeight + weightDelta, targetWeight);
                    else if (currentWeight > targetWeight)
                        currentWeight = Max(currentWeight - weightDelta, targetWeight);
                    state->SetWeight(currentWeight);
                }
                else
                    state->SetWeight(targetWeight);
            }

            // Remove if weight zero and target weight zero
            if (state->GetWeight() == 0.0f && (targetWeight == 0.0f || fadeTime == 0.0f))
                remove = true;
        }

        // Decrement the command time-to-live values
        if (i->setTimeTtl_ > 0.0f)
            i->setTimeTtl_ = Max(i->setTimeTtl_ - timeStep, 0.0f);
        if (i->setWeightTtl_ > 0.0f)
            i->setWeightTtl_ = Max(i->setWeightTtl_ - timeStep, 0.0f);

        if (remove)
        {
            if (state)
                RemoveAnimationState(state);
            i = animations_.erase(i);
            MarkNetworkUpdate();
        }
        else
            ++i;
    }

    // Node hierarchy animations need to be applied manually
    for (SharedPtr<AnimationState> & state : nodeAnimationStates_)
        state->Apply();
}

bool AnimationController::Play(const QString& name, unsigned char layer, bool looped, float fadeInTime)
{
    // Check if already exists
    unsigned index;
    AnimationState* state;
    FindAnimation(name, index, state);

    if (!state)
    {
        Animation* newAnimation = GetSubsystem<ResourceCache>()->GetResource<Animation>(name);
        state = AddAnimationState(newAnimation);
        if (!state)
            return false;
    }

    if (index == M_MAX_UNSIGNED)
    {
        AnimationControl newControl;
        Animation* animation = state->GetAnimation();
        newControl.name_ = animation->GetName();
        newControl.hash_ = animation->GetNameHash();
        animations_.push_back(newControl);
        index = animations_.size() - 1;
    }

    state->SetLayer(layer);
    state->SetLooped(looped);
    animations_[index].targetWeight_ = 1.0f;
    animations_[index].fadeTime_ = fadeInTime;

    MarkNetworkUpdate();
    return true;
}

bool AnimationController::PlayExclusive(const QString& name, unsigned char layer, bool looped, float fadeTime)
{
    FadeOthers(name, 0.0f, fadeTime);
    return Play(name, layer, looped, fadeTime);
}

bool AnimationController::Stop(const QString& name, float fadeOutTime)
{
    unsigned index;
    AnimationState* state;
    FindAnimation(name, index, state);
    if (index != M_MAX_UNSIGNED)
    {
        animations_[index].targetWeight_ = 0.0f;
        animations_[index].fadeTime_ = fadeOutTime;
        MarkNetworkUpdate();
    }

    return index != M_MAX_UNSIGNED || state != nullptr;
}

void AnimationController::StopLayer(unsigned char layer, float fadeOutTime)
{
    bool needUpdate = false;
    for (AnimationControl & elem : animations_)
    {
        AnimationState* state = GetAnimationState(elem.hash_);
        if (state && state->GetLayer() == layer)
        {
            elem.targetWeight_ = 0.0f;
            elem.fadeTime_ = fadeOutTime;
            needUpdate = true;
        }
    }

    if (needUpdate)
        MarkNetworkUpdate();
}

void AnimationController::StopAll(float fadeOutTime)
{
    if (animations_.size())
    {
        for (AnimationControl & elem : animations_)
        {
            elem.targetWeight_ = 0.0f;
            elem.fadeTime_ = fadeOutTime;
        }

        MarkNetworkUpdate();
    }
}

bool AnimationController::Fade(const QString& name, float targetWeight, float fadeTime)
{
    unsigned index;
    AnimationState* state;
    FindAnimation(name, index, state);
    if (index == M_MAX_UNSIGNED)
        return false;

    animations_[index].targetWeight_ = Clamp(targetWeight, 0.0f, 1.0f);
    animations_[index].fadeTime_ = fadeTime;
    MarkNetworkUpdate();
    return true;
}

bool AnimationController::FadeOthers(const QString& name, float targetWeight, float fadeTime)
{
    unsigned index;
    AnimationState* state;
    FindAnimation(name, index, state);
    if (index == M_MAX_UNSIGNED || !state)
        return false;

    unsigned char layer = state->GetLayer();

    bool needUpdate = false;
    for (unsigned i = 0; i < animations_.size(); ++i)
    {
        if (i != index)
        {
            AnimationControl& control = animations_[i];
            AnimationState* otherState = GetAnimationState(control.hash_);
            if (otherState && otherState->GetLayer() == layer)
            {
                control.targetWeight_ = Clamp(targetWeight, 0.0f, 1.0f);
                control.fadeTime_ = fadeTime;
                needUpdate = true;
            }
        }
    }

    if (needUpdate)
        MarkNetworkUpdate();
    return true;
}

bool AnimationController::SetLayer(const QString& name, unsigned char layer)
{
    AnimationState* state = GetAnimationState(name);
    if (!state)
        return false;

    state->SetLayer(layer);
    MarkNetworkUpdate();
    return true;
}

bool AnimationController::SetStartBone(const QString& name, const QString& startBoneName)
{
    // Start bone can only be set in model mode
    AnimatedModel* model = GetComponent<AnimatedModel>();
    if (!model)
        return false;

    AnimationState* state = model->GetAnimationState(name);
    if (!state)
        return false;

    Bone* bone = model->GetSkeleton().GetBone(startBoneName);
    state->SetStartBone(bone);
    MarkNetworkUpdate();
    return true;
}

bool AnimationController::SetTime(const QString& name, float time)
{
    unsigned index;
    AnimationState* state;
    FindAnimation(name, index, state);
    if (index == M_MAX_UNSIGNED || !state)
        return false;

    time = Clamp(time, 0.0f, state->GetLength());
    state->SetTime(time);
    // Prepare "set time" command for network replication
    animations_[index].setTime_ = (unsigned short)(time / state->GetLength() * 65535.0f);
    animations_[index].setTimeTtl_ = COMMAND_STAY_TIME;
    ++animations_[index].setTimeRev_;
    MarkNetworkUpdate();
    return true;
}

bool AnimationController::SetSpeed(const QString& name, float speed)
{
    unsigned index;
    AnimationState* state;
    FindAnimation(name, index, state);
    if (index == M_MAX_UNSIGNED)
        return false;

    animations_[index].speed_ = speed;
    MarkNetworkUpdate();
    return true;
}

bool AnimationController::SetWeight(const QString& name, float weight)
{
    unsigned index;
    AnimationState* state;
    FindAnimation(name, index, state);
    if (index == M_MAX_UNSIGNED || !state)
        return false;

    weight = Clamp(weight, 0.0f, 1.0f);
    state->SetWeight(weight);
    // Prepare "set weight" command for network replication
    animations_[index].setWeight_ = (unsigned char)(weight * 255.0f);
    animations_[index].setWeightTtl_ = COMMAND_STAY_TIME;
    ++animations_[index].setWeightRev_;
    MarkNetworkUpdate();
    return true;
}

bool AnimationController::SetLooped(const QString& name, bool enable)
{
    AnimationState* state = GetAnimationState(name);
    if (!state)
        return false;

    state->SetLooped(enable);
    MarkNetworkUpdate();
    return true;
}

bool AnimationController::SetAutoFade(const QString& name, float fadeOutTime)
{
    unsigned index;
    AnimationState* state;
    FindAnimation(name, index, state);
    if (index == M_MAX_UNSIGNED)
        return false;

    animations_[index].autoFadeTime_ = Max(fadeOutTime, 0.0f);
    MarkNetworkUpdate();
    return true;
}

bool AnimationController::IsPlaying(const QString& name) const
{
    unsigned index;
    AnimationState* state;
    FindAnimation(name, index, state);
    return index != M_MAX_UNSIGNED;
}

bool AnimationController::IsFadingIn(const QString& name) const
{
    unsigned index;
    AnimationState* state;
    FindAnimation(name, index, state);
    if (index == M_MAX_UNSIGNED || !state)
        return false;

    return animations_[index].fadeTime_ && animations_[index].targetWeight_ > state->GetWeight();
}

bool AnimationController::IsFadingOut(const QString& name) const
{
    unsigned index;
    AnimationState* state;
    FindAnimation(name, index, state);
    if (index == M_MAX_UNSIGNED || !state)
        return false;

    return (animations_[index].fadeTime_ && animations_[index].targetWeight_ < state->GetWeight())
        || (!state->IsLooped() && state->GetTime() >= state->GetLength() && animations_[index].autoFadeTime_);
}

bool AnimationController::IsAtEnd(const QString& name) const
{
    unsigned index;
    AnimationState* state;
    FindAnimation(name, index, state);
    if (index == M_MAX_UNSIGNED || !state)
        return false;
    else
        return state->GetTime() >= state->GetLength();
}

unsigned char AnimationController::GetLayer(const QString& name) const
{
    AnimationState* state = GetAnimationState(name);
    return state ? state->GetLayer() : 0;
}

Bone* AnimationController::GetStartBone(const QString& name) const
{
    AnimationState* state = GetAnimationState(name);
    return state ? state->GetStartBone() : nullptr;
}

const QString& AnimationController::GetStartBoneName(const QString& name) const
{
    Bone* bone = GetStartBone(name);
    return bone ? bone->name_ : s_dummy;
}

float AnimationController::GetTime(const QString& name) const
{
    AnimationState* state = GetAnimationState(name);
    return state ? state->GetTime() : 0.0f;
}

float AnimationController::GetWeight(const QString& name) const
{
    AnimationState* state = GetAnimationState(name);
    return state ? state->GetWeight() : 0.0f;
}

bool AnimationController::IsLooped(const QString& name) const
{
    AnimationState* state = GetAnimationState(name);
    return state ? state->IsLooped() : false;
}

float AnimationController::GetLength(const QString& name) const
{
    AnimationState* state = GetAnimationState(name);
    return state ? state->GetLength() : 0.0f;
}

float AnimationController::GetSpeed(const QString& name) const
{
    unsigned index;
    AnimationState* state;
    FindAnimation(name, index, state);
    return index != M_MAX_UNSIGNED ? animations_[index].speed_ : 0.0f;
}

float AnimationController::GetFadeTarget(const QString& name) const
{
    unsigned index;
    AnimationState* state;
    FindAnimation(name, index, state);
    return index != M_MAX_UNSIGNED ? animations_[index].targetWeight_ : 0.0f;
}

float AnimationController::GetFadeTime(const QString& name) const
{
    unsigned index;
    AnimationState* state;
    FindAnimation(name, index, state);
    return index != M_MAX_UNSIGNED ? animations_[index].targetWeight_ : 0.0f;
}

float AnimationController::GetAutoFade(const QString& name) const
{
    unsigned index;
    AnimationState* state;
    FindAnimation(name, index, state);
    return index != M_MAX_UNSIGNED ? animations_[index].autoFadeTime_ : 0.0f;
}

AnimationState* AnimationController::GetAnimationState(const QString& name) const
{
    return GetAnimationState(StringHash(name));
}

AnimationState* AnimationController::GetAnimationState(StringHash nameHash) const
{
    // Model mode
    AnimatedModel* model = GetComponent<AnimatedModel>();
    if (model)
        return model->GetAnimationState(nameHash);

    // Node hierarchy mode
    for (const SharedPtr<AnimationState> & elem : nodeAnimationStates_)
    {
        Animation* animation = (elem)->GetAnimation();
        if (animation->GetNameHash() == nameHash || animation->GetAnimationNameHash() == nameHash)
            return elem;
    }

    return nullptr;
}

void AnimationController::SetAnimationsAttr(const VariantVector& value)
{
    animations_.clear();
    animations_.reserve(value.size() / 5);  // Incomplete data is discarded
    unsigned index = 0;
    while (index + 4 < value.size())    // Prevent out-of-bound index access
    {
        AnimationControl newControl;
        newControl.name_ = value[index++].GetString();
        newControl.hash_ = StringHash(newControl.name_);
        newControl.speed_ = value[index++].GetFloat();
        newControl.targetWeight_ = value[index++].GetFloat();
        newControl.fadeTime_ = value[index++].GetFloat();
        newControl.autoFadeTime_ = value[index++].GetFloat();
        animations_.push_back(newControl);
    }
}

void AnimationController::SetNetAnimationsAttr(const std::vector<unsigned char>& value)
{
    MemoryBuffer buf(value);

    AnimatedModel* model = GetComponent<AnimatedModel>();

    // Check which animations we need to remove
    QSet<StringHash> processedAnimations;

    unsigned numAnimations = buf.ReadVLE();
    while (numAnimations--)
    {
        QString animName = buf.ReadString();
        StringHash animHash(animName);
        processedAnimations.insert(animHash);

        // Check if the animation state exists. If not, add new
        AnimationState* state = GetAnimationState(animHash);
        if (!state)
        {
            Animation* newAnimation = GetSubsystem<ResourceCache>()->GetResource<Animation>(animName);
            state = AddAnimationState(newAnimation);
            if (!state)
            {
                LOGERROR("Animation update applying aborted due to unknown animation");
                return;
            }
        }
        // Check if the internal control structure exists. If not, add new
        unsigned index;
        for (index = 0; index < animations_.size(); ++index)
        {
            if (animations_[index].hash_ == animHash)
                break;
        }
        if (index == animations_.size())
        {
            AnimationControl newControl;
            newControl.name_ = animName;
            newControl.hash_ = animHash;
            animations_.push_back(newControl);
        }

        unsigned char ctrl = buf.ReadUByte();
        state->SetLayer(buf.ReadUByte());
        state->SetLooped((ctrl & CTRL_LOOPED) != 0);
        animations_[index].speed_ = (float)buf.ReadShort() / 2048.0f; // 11 bits of decimal precision, max. 16x playback speed
        animations_[index].targetWeight_ = (float)buf.ReadUByte() / 255.0f; // 8 bits of decimal precision
        animations_[index].fadeTime_ = (float)buf.ReadUByte() / 64.0f; // 6 bits of decimal precision, max. 4 seconds fade
        if (ctrl & CTRL_STARTBONE)
        {
            StringHash boneHash = buf.ReadStringHash();
            if (model)
                state->SetStartBone(model->GetSkeleton().GetBone(boneHash));
        }
        else
            state->SetStartBone(nullptr);
        if (ctrl & CTRL_AUTOFADE)
            animations_[index].autoFadeTime_ = (float)buf.ReadUByte() / 64.0f; // 6 bits of decimal precision, max. 4 seconds fade
        else
            animations_[index].autoFadeTime_ = 0.0f;
        if (ctrl & CTRL_SETTIME)
        {
            unsigned char setTimeRev = buf.ReadUByte();
            unsigned short setTime = buf.ReadUShort();
            // Apply set time command only if revision differs
            if (setTimeRev != animations_[index].setTimeRev_)
            {
                state->SetTime(((float)setTime / 65535.0f) * state->GetLength());
                animations_[index].setTimeRev_ = setTimeRev;
            }
        }
        if (ctrl & CTRL_SETWEIGHT)
        {
            unsigned char setWeightRev = buf.ReadUByte();
            unsigned char setWeight = buf.ReadUByte();
            // Apply set weight command only if revision differs
            if (setWeightRev != animations_[index].setWeightRev_)
            {
                state->SetWeight((float)setWeight / 255.0f);
                animations_[index].setWeightRev_ = setWeightRev;
            }
        }
    }

    // Set any extra animations to fade out
    for (AnimationControl & elem : animations_)
    {
        if (!processedAnimations.contains(elem.hash_))
        {
            elem.targetWeight_ = 0.0f;
            elem.fadeTime_ = EXTRA_ANIM_FADEOUT_TIME;
        }
    }
}

void AnimationController::SetNodeAnimationStatesAttr(const VariantVector& value)
{
    ResourceCache* cache = GetSubsystem<ResourceCache>();
    nodeAnimationStates_.clear();
    unsigned index = 0;
    unsigned numStates = index < value.size() ? value[index++].GetUInt() : 0;
    // Prevent negative or overly large value being assigned from the editor
    if (numStates > M_MAX_INT)
        numStates = 0;
    if (numStates > MAX_NODE_ANIMATION_STATES)
        numStates = MAX_NODE_ANIMATION_STATES;

    nodeAnimationStates_.reserve(numStates);
    while (numStates--)
    {
        if (index + 2 < value.size())
        {
            // Note: null animation is allowed here for editing
            const ResourceRef& animRef = value[index++].GetResourceRef();
            SharedPtr<AnimationState> newState(new AnimationState(GetNode(), cache->GetResource<Animation>(animRef.name_)));
            nodeAnimationStates_.push_back(newState);

            newState->SetLooped(value[index++].GetBool());
            newState->SetTime(value[index++].GetFloat());
        }
        else
        {
            // If not enough data, just add an empty animation state
            SharedPtr<AnimationState> newState(new AnimationState(GetNode(), nullptr));
            nodeAnimationStates_.push_back(newState);
        }
    }
}

VariantVector AnimationController::GetAnimationsAttr() const
{
    VariantVector ret;
    ret.reserve(animations_.size() * 5);
    for (const AnimationControl & elem : animations_)
    {
        ret.push_back(elem.name_);
        ret.push_back(elem.speed_);
        ret.push_back(elem.targetWeight_);
        ret.push_back(elem.fadeTime_);
        ret.push_back(elem.autoFadeTime_);
    }
    return ret;
}

const std::vector<unsigned char>& AnimationController::GetNetAnimationsAttr() const
{
    attrBuffer_.clear();

    AnimatedModel* model = GetComponent<AnimatedModel>();

    unsigned validAnimations = 0;
    for (const AnimationControl & elem : animations_)
    {
        if (GetAnimationState(elem.hash_))
            ++validAnimations;
    }

    attrBuffer_.WriteVLE(validAnimations);
    for (const AnimationControl & elem : animations_)
    {
        AnimationState* state = GetAnimationState(elem.hash_);
        if (!state)
            continue;

        unsigned char ctrl = 0;
        Bone* startBone = state->GetStartBone();
        if (state->IsLooped())
            ctrl |= CTRL_LOOPED;
        if (startBone && model && startBone != model->GetSkeleton().GetRootBone())
            ctrl |= CTRL_STARTBONE;
        if (elem.autoFadeTime_ > 0.0f)
            ctrl |= CTRL_AUTOFADE;
        if (elem.setTimeTtl_ > 0.0f)
            ctrl |= CTRL_SETTIME;
        if (elem.setWeightTtl_ > 0.0f)
            ctrl |= CTRL_SETWEIGHT;

        attrBuffer_.WriteString(elem.name_);
        attrBuffer_.WriteUByte(ctrl);
        attrBuffer_.WriteUByte(state->GetLayer());
        attrBuffer_.WriteShort((short)Clamp(elem.speed_ * 2048.0f, -32767.0f, 32767.0f));
        attrBuffer_.WriteUByte((unsigned char)(elem.targetWeight_ * 255.0f));
        attrBuffer_.WriteUByte((unsigned char)Clamp(elem.fadeTime_ * 64.0f, 0.0f, 255.0f));
        if (ctrl & CTRL_STARTBONE)
            attrBuffer_.WriteStringHash(startBone->nameHash_);
        if (ctrl & CTRL_AUTOFADE)
            attrBuffer_.WriteUByte((unsigned char)Clamp(elem.autoFadeTime_ * 64.0f, 0.0f, 255.0f));
        if (ctrl & CTRL_SETTIME)
        {
            attrBuffer_.WriteUByte(elem.setTimeRev_);
            attrBuffer_.WriteUShort(elem.setTime_);
        }
        if (ctrl & CTRL_SETWEIGHT)
        {
            attrBuffer_.WriteUByte(elem.setWeightRev_);
            attrBuffer_.WriteUByte(elem.setWeight_);
        }
    }

    return attrBuffer_.GetBuffer();
}

VariantVector AnimationController::GetNodeAnimationStatesAttr() const
{
    VariantVector ret;
    ret.reserve(nodeAnimationStates_.size() * 3 + 1);
    ret.push_back(nodeAnimationStates_.size());
    for (const auto & elem : nodeAnimationStates_)
    {
        AnimationState* state = elem;
        Animation* animation = state->GetAnimation();
        ret.push_back(GetResourceRef(animation, Animation::GetTypeStatic()));
        ret.push_back(state->IsLooped());
        ret.push_back(state->GetTime());
    }
    return ret;
}

void AnimationController::OnNodeSet(Node* node)
{
    if (node)
    {
        Scene* scene = GetScene();
        if (scene && IsEnabledEffective())
            SubscribeToEvent(scene, E_SCENEPOSTUPDATE, HANDLER(AnimationController, HandleScenePostUpdate));
    }
}

AnimationState* AnimationController::AddAnimationState(Animation* animation)
{
    if (!animation)
        return nullptr;

    // Model mode
    AnimatedModel* model = GetComponent<AnimatedModel>();
    if (model)
        return model->AddAnimationState(animation);

    // Node hierarchy mode
    SharedPtr<AnimationState> newState(new AnimationState(node_, animation));
    nodeAnimationStates_.push_back(newState);
    return newState;
}

void AnimationController::RemoveAnimationState(AnimationState* state)
{
    if (!state)
        return;

    // Model mode
    AnimatedModel* model = GetComponent<AnimatedModel>();
    if (model)
    {
        model->RemoveAnimationState(state);
        return;
    }

    // Node hierarchy mode
    for (std::vector<SharedPtr<AnimationState> >::iterator i = nodeAnimationStates_.begin(); i != nodeAnimationStates_.end(); ++i)
    {
        if ((*i) == state)
        {
            nodeAnimationStates_.erase(i);
            return;
        }
    }
}

void AnimationController::FindAnimation(const QString& name, unsigned& index, AnimationState*& state) const
{
    StringHash nameHash(name);

    // Find the AnimationState
    state = GetAnimationState(nameHash);
    if (state)
    {
        // Either a resource name or animation name may be specified. We store resource names, so correct the hash if necessary
        nameHash = state->GetAnimation()->GetNameHash();
    }

    // Find the internal control structure
    index = M_MAX_UNSIGNED;
    for (unsigned i = 0; i < animations_.size(); ++i)
    {
        if (animations_[i].hash_ == nameHash)
        {
            index = i;
            break;
        }
    }
}

void AnimationController::HandleScenePostUpdate(StringHash eventType, VariantMap& eventData)
{
    using namespace ScenePostUpdate;

    Update(eventData[P_TIMESTEP].GetFloat());
}

}
