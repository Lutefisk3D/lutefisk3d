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

#include "../Urho2D/SpriterInstance2D.h"

#include <cmath>

namespace Urho3D
{

namespace Spriter
{

SpriterInstance::SpriterInstance(SpriterData* spriteData) : 
    spriterData_(spriteData),
    entity_(0),
    animation_(0)
{
}

SpriterInstance::~SpriterInstance()
{
    Clear();

    OnSetAnimation(0);
    OnSetEntity(0);
}

bool SpriterInstance::SetEntity(int index)
{
    if (!spriterData_)
        return false;

    if (index < (int)spriterData_->entities_.size())
    {
        OnSetEntity(spriterData_->entities_[index]);
        return true;
    }

    return false;
}

bool SpriterInstance::SetEntity(const QString & entityName)
{
    if (!spriterData_)
        return false;

    for (size_t i = 0; i < spriterData_->entities_.size(); ++i)
    {
        if (spriterData_->entities_[i]->name_ == entityName)
        {
            OnSetEntity(spriterData_->entities_[i]);
            return true;
        }
    }

    return false;
}   

bool SpriterInstance::SetAnimation(int index, LoopMode loopMode)
{
    if (!entity_)
        return false;

    if (index < (int)entity_->animations_.size())
    {
        OnSetAnimation(entity_->animations_[index], loopMode);
        return true;
    }

    return false;
}

bool SpriterInstance::SetAnimation(const QString & animationName, LoopMode loopMode)
{
    if (!entity_)
        return false;

    for (size_t i = 0; i < entity_->animations_.size(); ++i)
    {
        if (entity_->animations_[i]->name_ == animationName)
        {
            OnSetAnimation(entity_->animations_[i], loopMode);
            return true;
        }
    }

    return false;
}

void SpriterInstance::setSpatialInfo(const SpatialInfo& spatialInfo)
{
    this->spatialInfo_ = spatialInfo;
}

void SpriterInstance::setSpatialInfo(float x, float y, float angle, float scaleX, float scaleY)
{
    spatialInfo_ = SpatialInfo(x, y, angle, scaleX, scaleY);
}

void SpriterInstance::Update(float deltaTime)
{
    if (!animation_)
        return;

    Clear();

    currentTime_ += deltaTime;
    if (currentTime_ > animation_->length_)
    {
        if (looping_)
        {
            currentTime_ = fmod(currentTime_, animation_->length_);
        }
        else
        {
            currentTime_ = animation_->length_;
        }
    }

    UpdateMainlineKey();
    UpdateTimelineKeys();
}

void SpriterInstance::OnSetEntity(Entity* entity)
{
    if (entity == this->entity_)
        return;

    OnSetAnimation(0);

    this->entity_ = entity;
}

void SpriterInstance::OnSetAnimation(Animation* animation, LoopMode loopMode)
{
    if (animation == this->animation_)
        return;

    animation_ = animation;
    if (animation_)
    {
        if (loopMode == Default)
            looping_ = animation_->looping_;
        else if (loopMode == ForceLooped)
            looping_ = true;
        else
            looping_ = false;
    }
        
    currentTime_ = 0.0f;
    Clear();        
}

void SpriterInstance::UpdateTimelineKeys()
{
    for (size_t i = 0; i < mainlineKey_->boneRefs_.size(); ++i)
    {
        Ref* ref = mainlineKey_->boneRefs_[i];
        BoneTimelineKey* timelineKey = (BoneTimelineKey*)GetTimelineKey(ref);
        if (ref->parent_ >= 0)
        {
            timelineKey->info_ = timelineKey->info_.UnmapFromParent(timelineKeys_[ref->parent_]->info_);
        }
        else
        {
            timelineKey->info_ = timelineKey->info_.UnmapFromParent(spatialInfo_);
        }            
        timelineKeys_.push_back(timelineKey);
    }

    for (size_t i = 0; i < mainlineKey_->objectRefs_.size(); ++i)
    {
        Ref* ref = mainlineKey_->objectRefs_[i];
        SpriteTimelineKey* timelineKey = (SpriteTimelineKey*)GetTimelineKey(ref);
            
        if (ref->parent_ >= 0)
        {
            timelineKey->info_ = timelineKey->info_.UnmapFromParent(timelineKeys_[ref->parent_]->info_);
        }
        else
        {
            timelineKey->info_ = timelineKey->info_.UnmapFromParent(spatialInfo_);
        }
            
        timelineKey->zIndex_ = ref->zIndex_;

        timelineKeys_.push_back(timelineKey);
    }
}

void SpriterInstance::UpdateMainlineKey()
{
    const std::vector<MainlineKey*>& mainlineKeys = animation_->mainlineKeys_;
    for (size_t i = 0; i < mainlineKeys.size(); ++i)
    {
        if (mainlineKeys[i]->time_ <= currentTime_)
        {
            mainlineKey_ = mainlineKeys[i];
        }

        if (mainlineKeys[i]->time_ >= currentTime_)
        {
            break;
        }
    }

    if (!mainlineKey_)
    {
        mainlineKey_ = mainlineKeys[0];
    }
}

TimelineKey* SpriterInstance::GetTimelineKey(Ref* ref) const
{
    Timeline* timeline = animation_->timelines_[ref->timeline_];
    TimelineKey* timelineKey = timeline->keys_[ref->key_]->Clone();
    if (timeline->keys_.size() == 1 || timelineKey->curveType_ == INSTANT)
    {
        return timelineKey;
    }

    size_t nextTimelineKeyIndex = ref->key_ + 1;
    if (nextTimelineKeyIndex >= timeline->keys_.size())
    {
        if (animation_->looping_)
        {
            nextTimelineKeyIndex = 0;
        }
        else
        {
            return timelineKey;
        }
    }

    TimelineKey* nextTimelineKey = timeline->keys_[nextTimelineKeyIndex];
        
    float nextTimelineKeyTime = nextTimelineKey->time_;
    if (nextTimelineKey->time_ < timelineKey->time_)
    {
        nextTimelineKeyTime += animation_->length_;
    }

    float t = timelineKey->GetTByCurveType(currentTime_, nextTimelineKeyTime);
    timelineKey->Interpolate(*nextTimelineKey, t);

    return timelineKey;
}

void SpriterInstance::Clear()
{
    mainlineKey_ = 0;

    if (!timelineKeys_.empty())
    {
        for (size_t i = 0; i < timelineKeys_.size(); ++i)
        {
            delete timelineKeys_[i];
        }
        timelineKeys_.clear();
    }
}

}

}
