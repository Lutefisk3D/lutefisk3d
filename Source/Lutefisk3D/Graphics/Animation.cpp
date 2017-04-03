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

#include "Animation.h"

#include "../Core/Context.h"
#include "../Core/Profiler.h"
#include "../IO/Deserializer.h"
#include "../IO/FileSystem.h"
#include "../IO/Log.h"
#include "../IO/Serializer.h"
#include "../Resource/ResourceCache.h"
#include "../Resource/XMLFile.h"
#include "../Resource/JSONFile.h"

namespace Urho3D
{

inline bool CompareTriggers(AnimationTriggerPoint& lhs, AnimationTriggerPoint& rhs)
{
    return lhs.time_ < rhs.time_;
}

inline bool CompareKeyFrames(AnimationKeyFrame& lhs, AnimationKeyFrame& rhs)
{
    return lhs.time_ < rhs.time_;
}

void AnimationTrack::SetKeyFrame(unsigned index, const AnimationKeyFrame& keyFrame)
{
    if (index < keyFrames_.size())
    {
        keyFrames_[index] = keyFrame;
        std::sort(keyFrames_.begin(), keyFrames_.end(), CompareKeyFrames);
    }
    else if (index == keyFrames_.size())
        AddKeyFrame(keyFrame);
}

void AnimationTrack::AddKeyFrame(const AnimationKeyFrame& keyFrame)
{
    bool needSort = keyFrames_.size() ? keyFrames_.back().time_ > keyFrame.time_ : false;
    keyFrames_.push_back(keyFrame);
    if (needSort)
        std::sort(keyFrames_.begin(), keyFrames_.end(), CompareKeyFrames);
}

void AnimationTrack::InsertKeyFrame(unsigned index, const AnimationKeyFrame& keyFrame)
{
    keyFrames_.insert(keyFrames_.begin()+index, keyFrame);
    std::sort(keyFrames_.begin(), keyFrames_.end(), CompareKeyFrames);
}

void AnimationTrack::RemoveKeyFrame(unsigned index)
{
    keyFrames_.erase(keyFrames_.begin()+index);
}

void AnimationTrack::RemoveAllKeyFrames()
{
    keyFrames_.clear();
}

AnimationKeyFrame* AnimationTrack::GetKeyFrame(unsigned index)
{
    return index < keyFrames_.size() ? &keyFrames_[index] : (AnimationKeyFrame*)nullptr;
}
void AnimationTrack::GetKeyFrameIndex(float time, unsigned& index) const
{
    if (time < 0.0f)
        time = 0.0f;

    if (index >= keyFrames_.size())
        index = keyFrames_.size() - 1;

    // Check for being too far ahead
    while (index && time < keyFrames_[index].time_)
        --index;

    // Check for being too far behind
    while (index < keyFrames_.size() - 1 && time >= keyFrames_[index + 1].time_)
        ++index;
}

Animation::Animation(Context* context) :
    Resource(context),
    length_(0.f)
{
}


void Animation::RegisterObject(Context* context)
{
    context->RegisterFactory<Animation>();
}

bool Animation::BeginLoad(Deserializer& source)
{
    unsigned memoryUse = sizeof(Animation);

    // Check ID
    if (source.ReadFileID() != "UANI")
    {
        URHO3D_LOGERROR(source.GetName() + " is not a valid animation file");
        return false;
    }

    // Read name and length
    animationName_ = source.ReadString();
    animationNameHash_ = animationName_;
    length_ = source.ReadFloat();
    tracks_.clear();

    unsigned tracks = source.ReadUInt();
    tracks_.reserve(tracks);
    memoryUse += tracks * sizeof(AnimationTrack);

    // Read tracks
    for (unsigned i = 0; i < tracks; ++i)
    {
        AnimationTrack* newTrack = CreateTrack(source.ReadString());
        newTrack->channelMask_ = source.ReadUByte();

        unsigned keyFrames = source.ReadUInt();
        newTrack->keyFrames_.resize(keyFrames);
        memoryUse += keyFrames * sizeof(AnimationKeyFrame);

        // Read keyframes of the track
        for (unsigned j = 0; j < keyFrames; ++j)
        {
            AnimationKeyFrame& newKeyFrame = newTrack->keyFrames_[j];
            newKeyFrame.time_ = source.ReadFloat();
            if (newTrack->channelMask_ & CHANNEL_POSITION)
                newKeyFrame.position_ = source.ReadVector3();
            if (newTrack->channelMask_ & CHANNEL_ROTATION)
                newKeyFrame.rotation_ = source.ReadQuaternion();
            if (newTrack->channelMask_ & CHANNEL_SCALE)
                newKeyFrame.scale_ = source.ReadVector3();
        }
    }

    // Optionally read triggers from an XML file
    ResourceCache* cache = GetSubsystem<ResourceCache>();
    QString xmlName = ReplaceExtension(GetName(), ".xml");

    SharedPtr<XMLFile> file(cache->GetTempResource<XMLFile>(xmlName, false));
    if (file)
    {
        XMLElement rootElem = file->GetRoot();
        XMLElement triggerElem = rootElem.GetChild("trigger");
        while (triggerElem)
        {
            if (triggerElem.HasAttribute("normalizedtime"))
                AddTrigger(triggerElem.GetFloat("normalizedtime"), true, triggerElem.GetVariant());
            else if (triggerElem.HasAttribute("time"))
                AddTrigger(triggerElem.GetFloat("time"), false, triggerElem.GetVariant());

            triggerElem = triggerElem.GetNext("trigger");
        }

        memoryUse += triggers_.size() * sizeof(AnimationTriggerPoint);
        SetMemoryUse(memoryUse);
        return true;
    }

    // Optionally read triggers from a JSON file
    QString  jsonName = ReplaceExtension(GetName(), ".json");

    SharedPtr<JSONFile> jsonFile(cache->GetTempResource<JSONFile>(jsonName, false));
    if (jsonFile)
    {
        const JSONValue& rootVal = jsonFile->GetRoot();
        JSONArray triggerArray = rootVal.Get("triggers").GetArray();

        for (unsigned i = 0; i < triggerArray.size(); i++)
        {
            const JSONValue& triggerValue = triggerArray.at(i);
            JSONValue normalizedTimeValue = triggerValue.Get("normalizedTime");
            if (!normalizedTimeValue.IsNull())
                AddTrigger(normalizedTimeValue.GetFloat(), true, triggerValue.GetVariant());
            else
            {
                JSONValue timeVal = triggerValue.Get("time");
                if (!timeVal.IsNull())
                    AddTrigger(timeVal.GetFloat(), false, triggerValue.GetVariant());
            }
        }

        memoryUse += triggers_.size() * sizeof(AnimationTriggerPoint);
        SetMemoryUse(memoryUse);
        return true;
    }

    SetMemoryUse(memoryUse);
    return true;
}

bool Animation::Save(Serializer& dest) const
{
    // Write ID, name and length
    dest.WriteFileID("UANI");
    dest.WriteString(animationName_);
    dest.WriteFloat(length_);

    // Write tracks
    dest.WriteUInt(tracks_.size());
    for (HashMap<StringHash, AnimationTrack>::const_iterator i = tracks_.begin(); i != tracks_.end(); ++i)
    {
        const AnimationTrack& track(MAP_VALUE(i));
        dest.WriteString(track.name_);
        dest.WriteUByte(track.channelMask_);
        dest.WriteUInt(track.keyFrames_.size());

        // Write keyframes of the track
        for (unsigned j = 0; j < track.keyFrames_.size(); ++j)
        {
            const AnimationKeyFrame& keyFrame = track.keyFrames_[j];
            dest.WriteFloat(keyFrame.time_);
            if (track.channelMask_ & CHANNEL_POSITION)
                dest.WriteVector3(keyFrame.position_);
            if (track.channelMask_ & CHANNEL_ROTATION)
                dest.WriteQuaternion(keyFrame.rotation_);
            if (track.channelMask_ & CHANNEL_SCALE)
                dest.WriteVector3(keyFrame.scale_);
        }
    }

    // If triggers have been defined, write an XML file for them
    if (triggers_.size())
    {
        File* destFile = dynamic_cast<File*>(&dest);
        if (destFile)
        {
            QString xmlName = ReplaceExtension(destFile->GetName(), ".xml");

            SharedPtr<XMLFile> xml(new XMLFile(context_));
            XMLElement rootElem = xml->CreateRoot("animation");

            for (unsigned i = 0; i < triggers_.size(); ++i)
            {
                XMLElement triggerElem = rootElem.CreateChild("trigger");
                triggerElem.SetFloat("time", triggers_[i].time_);
                triggerElem.SetVariant(triggers_[i].data_);
            }

            File xmlFile(context_, xmlName, FILE_WRITE);
            xml->Save(xmlFile);
        }
        else
            URHO3D_LOGWARNING("Can not save animation trigger data when not saving into a file");
    }

    return true;
}

void Animation::SetAnimationName(const QString& name)
{
    animationName_ = name;
    animationNameHash_ = StringHash(name);
}

void Animation::SetLength(float length)
{
    length_ = Max(length, 0.0f);
}

AnimationTrack* Animation::CreateTrack(const QString& name)
{
    /// \todo When tracks / keyframes are created dynamically, memory use is not updated
    StringHash nameHash(name);
    AnimationTrack* oldTrack = GetTrack(nameHash);
    if (oldTrack)
        return oldTrack;

    AnimationTrack& newTrack = tracks_[nameHash];
    newTrack.name_ = name;
    newTrack.nameHash_ = nameHash;
    return &newTrack;
}

bool Animation::RemoveTrack(const QString& name)
{
    HashMap<StringHash, AnimationTrack>::iterator i = tracks_.find(StringHash(name));
    if (i != tracks_.end())
    {
        tracks_.erase(i);
        return true;
    }
    else
        return false;
}

void Animation::RemoveAllTracks()
{
    tracks_.clear();
}

void Animation::SetTrigger(unsigned index, const AnimationTriggerPoint& trigger)
{
    if (index == triggers_.size())
        AddTrigger(trigger);
    else if (index < triggers_.size())
    {
        triggers_[index] = trigger;
        std::sort(triggers_.begin(), triggers_.end(), CompareTriggers);
    }
}

void Animation::AddTrigger(const AnimationTriggerPoint& trigger)
{
    triggers_.push_back(trigger);
    std::sort(triggers_.begin(), triggers_.end(), CompareTriggers);
}

void Animation::AddTrigger(float time, bool timeIsNormalized, const Variant& data)
{
    AnimationTriggerPoint newTrigger;
    newTrigger.time_ = timeIsNormalized ? time * length_ : time;
    newTrigger.data_ = data;
    triggers_.push_back(newTrigger);

    std::sort(triggers_.begin(), triggers_.end(), CompareTriggers);
}

void Animation::RemoveTrigger(unsigned index)
{
    if (index < triggers_.size())
        triggers_.erase(triggers_.begin()+index);
}

void Animation::RemoveAllTriggers()
{
    triggers_.clear();
}

void Animation::SetNumTriggers(unsigned num)
{
    triggers_.resize(num);
}
SharedPtr<Animation> Animation::Clone(const QString& cloneName) const
{
    SharedPtr<Animation> ret(new Animation(context_));

    ret->SetName(cloneName);
    ret->SetAnimationName(animationName_);
    ret->length_ = length_;
    ret->tracks_ = tracks_;
    ret->triggers_ = triggers_;
    ret->SetMemoryUse(GetMemoryUse());

    return ret;
}
AnimationTrack* Animation::GetTrack(const QString& name)
{
    HashMap<StringHash, AnimationTrack>::iterator i = tracks_.find(StringHash(name));
    return i != tracks_.end() ? &MAP_VALUE(i): (AnimationTrack*)nullptr;
}

AnimationTrack* Animation::GetTrack(StringHash nameHash)
{
    HashMap<StringHash, AnimationTrack>::iterator i = tracks_.find(nameHash);
    return i != tracks_.end() ? &MAP_VALUE(i) : (AnimationTrack*)nullptr;
    }

AnimationTriggerPoint* Animation::GetTrigger(unsigned index)
{
    return index < triggers_.size() ? &triggers_[index] : (AnimationTriggerPoint*)nullptr;
}

}
