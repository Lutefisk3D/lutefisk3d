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

#include "Audio.h"

#include "Sound.h"
#include "SoundListener.h"
#include "SoundSource3D.h"
#include "Lutefisk3D/Core/Context.h"
#include "Lutefisk3D/Core/CoreEvents.h"
#include "Lutefisk3D/IO/Log.h"
#include "Lutefisk3D/Core/Mutex.h"
#include "Lutefisk3D/Core/ProcessUtils.h"
#include "Lutefisk3D/Core/Profiler.h"
#include "Lutefisk3D/Scene/Node.h"

#define AL_ALEXT_PROTOTYPES
#include <AL/al.h>
#include <AL/alc.h>
#include <AL/alext.h>
namespace Urho3D
{

const char* AUDIO_CATEGORY = "Audio";

static const int MIN_BUFFERLENGTH = 20;
static const int MIN_MIXRATE = 11025;
static const int MAX_MIXRATE = 48000;
static const StringHash SOUND_MASTER_HASH("Master");

struct Audio::AudioPrivate
{
    QStringList m_deviceNames;
    ALCdevice *device=nullptr;
    ALCcontext *context=nullptr;
    ~AudioPrivate()
    {
        if (device)
        {
            alcCloseDevice(device);
            device = nullptr;
        }
    }
    static QStringList deviceNames(const ALCchar *devices)
    {
        const ALCchar *device = devices, *next = devices + 1;
        size_t len = 0;
        QStringList available;

        while (device && *device != '\0' && next && *next != '\0') {
            available+=device;
            len = strlen(device);
            device += (len + 1);
            next += (len + 2);
        }
        return available;
    }
    void initialize() {
        ALboolean enumeration = alcIsExtensionPresent(nullptr, "ALC_ENUMERATION_EXT");
        QStringList devices;
        if (enumeration != AL_FALSE)
            m_deviceNames = deviceNames(alcGetString(nullptr, ALC_DEVICE_SPECIFIER));

    }
    void openDevice(int index)
    {
        alGetError();
        device = alcOpenDevice(m_deviceNames.isEmpty() ? nullptr : qPrintable(m_deviceNames.at(index)));
        auto errorcode=alGetError();
        if (!device)
        {
            URHO3D_LOGERROR(QString("Failed to open OpenAL device alerror = %1").arg(errorcode));
        }
    }
    bool recreateContext(int freq)
    {
        if (!device)
        {
            URHO3D_LOGERROR("Failed to create OpenAL context, device is not open");
            return false;
        }
        if (freq == 0)
        {
            ALCint dev_rate = 0;
            alcGetIntegerv(device, ALC_FREQUENCY, 1, &dev_rate);
            if (ALC_NO_ERROR == alcGetError(device))
                freq = dev_rate;
        }
        ALCint attrvalues[] = {ALC_FREQUENCY, freq, 0};
        alGetError();
        if (!context)
        {
            context = alcCreateContext(device, attrvalues);
        }
        else
        {
            assert(alcResetDeviceSOFT!=nullptr);
            alcResetDeviceSOFT(device, attrvalues);
        }
        if (!alcMakeContextCurrent(context))
        {
            auto errorcode = alGetError();
            URHO3D_LOGERROR(QString("Failed to create OpenAL context: alerror = %1").arg(errorcode));
            return false;
        }
        return true;
    }
    void Release()
    {
        alcMakeContextCurrent(nullptr);
        if(context)
        {
            alcDestroyContext(context);
            context = nullptr;
        }
    }
    void Pause()
    {
        alcDevicePauseSOFT(device);
    }
    void Unpause()
    {
        alcDeviceResumeSOFT(device);
    }
    void updateListenerPostion(Node *n)
    {
        if(n) {
            Vector3 pos = n->GetWorldPosition();
            Vector3 upv = n->GetWorldUp();
            Vector3 forwardv = n->GetWorldDirection();
            float ori[6];
            ori[0] = forwardv.x_;
            ori[1] = forwardv.y_;
            ori[2] = forwardv.z_;
            ori[3] = upv.x_;
            ori[4] = upv.y_;
            ori[5] = upv.z_;
            alListenerfv(AL_POSITION, pos.Data());
            alListenerfv(AL_ORIENTATION, ori);
        }
    }
};
Audio::Audio(Context* context) :
    Object(context),
    SignalObserver(context->m_observer_allocator),
    d(new AudioPrivate),
    sampleSize_(0),
    playing_(false)
{
    //context_->RequireSDL(SDL_INIT_AUDIO);
    // Set the master to the default value
    masterGain_[SOUND_MASTER_HASH] = 1.0f;
    d->initialize();

    // Register Audio library object factories
    RegisterAudioLibrary(context_);
    g_coreSignals.renderUpdate.Connect(this,&Audio::Update);
}

Audio::~Audio()
{
    Release();
}

/// Initialize sound output with specified buffer length and output mode.
/// \a freq is
bool Audio::SetMode(int bufferLengthMSec, int freq)
{
    Release();

    bufferLengthMSec = std::max(bufferLengthMSec, MIN_BUFFERLENGTH);
    freq = Clamp(freq, MIN_MIXRATE, MAX_MIXRATE);

    if(!d->recreateContext(freq))
        return false;

    URHO3D_LOGINFO("Set audio mode " + QString::number(mixRate_) + " Hz ");
    // SDL uses power of two audio fragments. Determine the closest match
    // Guarantee a fragment size that is low enough so that Vorbis decoding buffers do not wrap
    return Play();
}

/// Run update on sound sources. Not required for continued playback, but frees unused sound sources & sounds and
/// updates 3D positions.
void Audio::Update(float timeStep)
{
    if (!playing_)
        return;
    if(playing_ && d->context && soundSources_.empty())
    {
        d->Pause();
    }
    UpdateInternal(timeStep);
}
/// Restart sound output.
bool Audio::Play()
{
    if (playing_)
        return true;

    if (!d->context)
    {
        URHO3D_LOGERROR("No audio mode set, can not start playback");
        return false;
    }

    d->Unpause();

    // Update sound sources before resuming playback to make sure 3D positions are up to date
    UpdateInternal(0.0f);

    playing_ = true;
    return true;
}

/// Suspend sound output.
void Audio::Stop()
{
    playing_ = false;

    if (d->device)
        d->Pause();
}

/// Set master gain on a specific sound type such as sound effects, music or voice.
void Audio::SetMasterGain(const QString& type, float gain)
{
    masterGain_[type] = Clamp(gain, 0.0f, 1.0f);

    for (SoundSource* src : soundSources_)
        src->UpdateMasterGain();
}
/// Pause playback of specific sound type. This allows to suspend e.g. sound effects or voice when the game is paused. By default all sound types are unpaused.
void Audio::PauseSoundType(const QString& type)
{
    pausedSoundTypes_.insert(type);
}

/// Resume playback of specific sound type.
void Audio::ResumeSoundType(const QString& type)
{
    MutexLock lock(audioMutex_);
    pausedSoundTypes_.erase(type);
    // Update sound sources before resuming playback to make sure 3D positions are up to date
    // Done under mutex to ensure no mixing happens before we are ready
    UpdateInternal(0.0f);
}

/// Resume playback of all sound types.
void Audio::ResumeAll()
{
    MutexLock lock(audioMutex_);
    pausedSoundTypes_.clear();
    UpdateInternal(0.0f);
}

/// Set active sound listener for 3D sounds.
void Audio::SetListener(SoundListener* listener)
{
    listener_ = listener;
}

/// Stop any sound source playing a certain sound clip.
void Audio::StopSound(Sound* soundClip)
{
    for (SoundSource * elem : soundSources_)
    {
        if (elem->GetSound() == soundClip)
            elem->Stop();
    }
}

bool Audio::IsInitialized() const
{
    return d->context != nullptr;
}
/// Return master gain for a specific sound source type. Unknown sound types will return full gain (1).
float Audio::GetMasterGain(const QString& type) const
{
    // By definition previously unknown types return full volume
    HashMap<StringHash, float>::const_iterator findIt = masterGain_.find(type);
    if (findIt == masterGain_.end())
        return 1.0f;

    return MAP_VALUE(findIt);
}

/// Return whether specific sound type has been paused.
bool Audio::IsSoundTypePaused(const QString& type) const
{
    return pausedSoundTypes_.contains(type);
}
/// Return active sound listener.
SoundListener* Audio::GetListener() const
{
    return listener_;
}

/// Add a sound source to keep track of. Called by SoundSource.
void Audio::AddSoundSource(SoundSource* channel)
{
    MutexLock lock(audioMutex_);
    soundSources_.push_back(channel);
    if(playing_ && d->context )
        d->Unpause();

}

/// Remove a sound source. Called by SoundSource.
void Audio::RemoveSoundSource(SoundSource* channel)
{
    std::vector<SoundSource*>::iterator i = std::find(soundSources_.begin(),soundSources_.end(),channel);
    if (i != soundSources_.end())
    {
        MutexLock lock(audioMutex_);
        soundSources_.erase(i);
    }
}

/// Return sound type specific gain multiplied by master gain.
float Audio::GetSoundSourceMasterGain(StringHash typeHash) const
{
    HashMap<StringHash, float>::const_iterator masterIt = masterGain_.find(SOUND_MASTER_HASH);

    if (!typeHash)
        return MAP_VALUE(masterIt);

    HashMap<StringHash, float>::const_iterator typeIt = masterGain_.find(typeHash);

    if (typeIt == masterGain_.end() || typeIt == masterIt)
        return MAP_VALUE(masterIt);

    return MAP_VALUE(masterIt) * MAP_VALUE(typeIt);
}

/// Stop sound output and release the sound buffer.
void Audio::Release()
{
    Stop();
    d->Release();
}
/// Actually update sound sources with the specific timestep. Called internally.
void Audio::UpdateInternal(float timeStep)
{
    URHO3D_PROFILE(UpdateAudio);
    if(listener_) {
        d->updateListenerPostion(listener_->GetNode());
    }
    // Update in reverse order, because sound sources might remove themselves
    for (unsigned i = soundSources_.size() - 1; i < soundSources_.size(); --i)
    {
        SoundSource* source = soundSources_[i];

        // Check for pause if necessary; do not update paused sound sources
        if (!pausedSoundTypes_.empty())
        {
            if (pausedSoundTypes_.contains(source->GetSoundType()))
                continue;
        }

        source->Update(timeStep);
    }
}

void RegisterAudioLibrary(Context* context)
{
    Sound::RegisterObject(context);
    SoundSource::RegisterObject(context);
    SoundSource3D::RegisterObject(context);
    SoundListener::RegisterObject(context);
}

}
