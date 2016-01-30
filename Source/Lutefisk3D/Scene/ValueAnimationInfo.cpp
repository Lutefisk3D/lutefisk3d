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

#include "ValueAnimationInfo.h"

#include "ValueAnimation.h"
#include "../IO/Log.h"
#include "../Math/MathDefs.h"

namespace Urho3D
{

ValueAnimationInfo::ValueAnimationInfo(ValueAnimation* animation, WrapMode wrapMode, float speed) :
    animation_(animation),
    wrapMode_(wrapMode),
    speed_(speed),
    currentTime_(0.0f),
    lastScaledTime_(0.0f)
{
    speed_ = std::max(0.0f, speed_);
}

ValueAnimationInfo::ValueAnimationInfo(Object* target, ValueAnimation* animation, WrapMode wrapMode, float speed) :
    target_(target),
    animation_(animation),
    wrapMode_(wrapMode),
    speed_(speed),
    currentTime_(0.0f),
    lastScaledTime_(0.0f)
{
    speed_ = std::max(0.0f, speed_);
}

ValueAnimationInfo::ValueAnimationInfo(const ValueAnimationInfo& other) :
    target_(other.target_),
    animation_(other.animation_),
    wrapMode_(other.wrapMode_),
    speed_(other.speed_),
    currentTime_(0.0f),
    lastScaledTime_(0.0f)
{
}

bool ValueAnimationInfo::Update(float timeStep)
{
    if ((animation_ == nullptr) || (target_ == nullptr))
        return true;

    return SetTime(currentTime_ + timeStep * speed_);
}

bool ValueAnimationInfo::SetTime(float time)
{
    if ((animation_ == nullptr) || (target_ == nullptr))
        return true;

    currentTime_ = time;

    if (!animation_->IsValid())
        return true;

    bool finished = false;

    // Calculate scale time by wrap mode
    float scaledTime = CalculateScaledTime(currentTime_, finished);

    // Apply to the target object
    ApplyValue(animation_->GetAnimationValue(scaledTime));

    // Send keyframe event if necessary
    if (animation_->HasEventFrames())
    {
        std::vector<const VAnimEventFrame*> eventFrames;
        GetEventFrames(lastScaledTime_, scaledTime, eventFrames);

        for (const VAnimEventFrame* fr : eventFrames)
            target_->SendEvent(fr->eventType_, const_cast<VariantMap&>(fr->eventData_));
    }

    lastScaledTime_ = scaledTime;

    return finished;
}

Object* ValueAnimationInfo::GetTarget() const
{
    return target_;
}

void ValueAnimationInfo::ApplyValue(const Variant & /*newValue*/)
{
}

float ValueAnimationInfo::CalculateScaledTime(float currentTime, bool& finished) const
{
    float beginTime = animation_->GetBeginTime();
    float endTime = animation_->GetEndTime();

    switch (wrapMode_)
    {
    case WM_LOOP:
        {
            float span = endTime - beginTime;
            float time = fmodf(currentTime - beginTime, span);
            if (time < 0.0f) {
                time += span;
            }
            return beginTime + time;
        }

    case WM_ONCE:
        finished = (currentTime >= endTime);
        // Fallthrough

    case WM_CLAMP:
        return Clamp(currentTime, beginTime, endTime);

    default:
        URHO3D_LOGERROR("Unsupported attribute animation wrap mode");
        return beginTime;
    }
}

void ValueAnimationInfo::GetEventFrames(float beginTime, float endTime, std::vector<const VAnimEventFrame *> & eventFrames)
{
    switch (wrapMode_)
    {
    case WM_LOOP:
        if (beginTime < endTime)
            animation_->GetEventFrames(beginTime, endTime, eventFrames);
        else
        {
            animation_->GetEventFrames(beginTime, animation_->GetEndTime(), eventFrames);
            animation_->GetEventFrames(animation_->GetBeginTime(), endTime, eventFrames);
        }
        break;

    case WM_ONCE:
    case WM_CLAMP:
        animation_->GetEventFrames(beginTime, endTime, eventFrames);
        break;
    }
}

}
