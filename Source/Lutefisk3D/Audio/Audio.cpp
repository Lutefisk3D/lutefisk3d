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

#include <SDL2/SDL.h>

namespace Urho3D
{

const char* AUDIO_CATEGORY = "Audio";

static const int MIN_BUFFERLENGTH = 20;
static const int MIN_MIXRATE = 11025;
static const int MAX_MIXRATE = 48000;
static const StringHash SOUND_MASTER_HASH("Master");

void SDLAudioCallback(void *userdata, Uint8 *stream, int len);

Audio::Audio(Context* context) :
    Object(context),
    deviceID_(0),
    sampleSize_(0),
    playing_(false)
{
    context_->RequireSDL(SDL_INIT_AUDIO);
    // Set the master to the default value
    masterGain_[SOUND_MASTER_HASH] = 1.0f;

    // Register Audio library object factories
    RegisterAudioLibrary(context_);
    g_coreSignals.renderUpdate.Connect(this,&Audio::Update);
}

Audio::~Audio()
{
    Release();
    context_->ReleaseSDL();
}

/// Initialize sound output with specified buffer length and output mode.
bool Audio::SetMode(int bufferLengthMSec, int mixRate, bool stereo, bool interpolation)
{
    Release();

    bufferLengthMSec = std::max(bufferLengthMSec, MIN_BUFFERLENGTH);
    mixRate = Clamp(mixRate, MIN_MIXRATE, MAX_MIXRATE);

    SDL_AudioSpec desired;
    SDL_AudioSpec obtained;

    desired.freq = mixRate;
    desired.format = AUDIO_S16;
    desired.channels = stereo ? 2 : 1;
    desired.callback = SDLAudioCallback;
    desired.userdata = this;

    // SDL uses power of two audio fragments. Determine the closest match
    int bufferSamples = mixRate * bufferLengthMSec / 1000;
    desired.samples = NextPowerOfTwo(bufferSamples);
    if (Abs((int)desired.samples / 2 - bufferSamples) < Abs((int)desired.samples - bufferSamples))
        desired.samples /= 2;

    deviceID_ = SDL_OpenAudioDevice(nullptr, SDL_FALSE, &desired, &obtained, SDL_AUDIO_ALLOW_ANY_CHANGE);
    if (!deviceID_)
    {
        URHO3D_LOGERROR("Could not initialize audio output");
        return false;
    }

    if (obtained.format != AUDIO_S16SYS && obtained.format != AUDIO_S16LSB && obtained.format != AUDIO_S16MSB)
    {
        URHO3D_LOGERROR("Could not initialize audio output, 16-bit buffer format not supported");
        SDL_CloseAudioDevice(deviceID_);
        deviceID_ = 0;
        return false;
    }

    stereo_ = obtained.channels == 2;
    sampleSize_ = stereo_ ? sizeof(int) : sizeof(short);
    // Guarantee a fragment size that is low enough so that Vorbis decoding buffers do not wrap
    fragmentSize_ = std::min<unsigned>(NextPowerOfTwo(mixRate >> 6), obtained.samples);
    mixRate_ = obtained.freq;
    interpolation_ = interpolation;
    clipBuffer_.reset(new int[stereo ? fragmentSize_ << 1 : fragmentSize_]);

    URHO3D_LOGINFO("Set audio mode " + QString::number(mixRate_) + " Hz " + (stereo_ ? "stereo" : "mono") + " " +
                   (interpolation_ ? "interpolated" : ""));

    return Play();
}

/// Run update on sound sources. Not required for continued playback, but frees unused sound sources & sounds and updates 3D positions.
void Audio::Update(float timeStep)
{
    if (!playing_)
        return;
    if(playing_ && deviceID_ && soundSources_.empty())
        SDL_PauseAudioDevice(deviceID_, 1);
    UpdateInternal(timeStep);
}
/// Restart sound output.
bool Audio::Play()
{
    if (playing_)
        return true;

    if (!deviceID_)
    {
        URHO3D_LOGERROR("No audio mode set, can not start playback");
        return false;
    }

    SDL_PauseAudioDevice(deviceID_, 0);

    // Update sound sources before resuming playback to make sure 3D positions are up to date
    UpdateInternal(0.0f);

    playing_ = true;
    return true;
}

/// Suspend sound output.
void Audio::Stop()
{
    playing_ = false;
    if (deviceID_)
        SDL_PauseAudioDevice(deviceID_, 1);
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
    if(playing_ && deviceID_ )
        SDL_PauseAudioDevice(deviceID_, 0);

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

void SDLAudioCallback(void *userdata, Uint8* stream, int len)
{
    Audio* audio = static_cast<Audio*>(userdata);

    {
        MutexLock Lock(audio->GetMutex());
        audio->MixOutput(stream, len / audio->GetSampleSize() / Audio::SAMPLE_SIZE_MUL);
    }
}
/// Mix sound sources into the buffer.
void Audio::MixOutput(void *dest, unsigned samples)
{
    if (!playing_ || !clipBuffer_)
    {
        memset(dest, 0, samples * sampleSize_ * SAMPLE_SIZE_MUL);
        return;
    }

    while (samples)
    {
        // If sample count exceeds the fragment (clip buffer) size, split the work
        unsigned workSamples = std::min<unsigned>(samples, fragmentSize_);
        unsigned clipSamples = workSamples;
        if (stereo_)
            clipSamples <<= 1;

        // Clear clip buffer
        int* clipPtr = clipBuffer_.get();
        memset(clipPtr, 0, clipSamples * sizeof(int));

        // Mix samples to clip buffer
        for (SoundSource * source : soundSources_) {
            // Check for pause if necessary
            if (!pausedSoundTypes_.empty())
            {
                if (pausedSoundTypes_.contains(source->GetSoundType()))
                    continue;
            }

            source->Mix(clipPtr, workSamples, mixRate_, stereo_, interpolation_);
        }

        // Copy output from clip buffer to destination
        short* destPtr = (short*)dest;
        while (clipSamples--)
            *destPtr++ = Clamp(*clipPtr++, -32768, 32767);

        samples -= workSamples;
        ((unsigned char*&)dest) += sampleSize_ * SAMPLE_SIZE_MUL * workSamples;
    }
}

/// Stop sound output and release the sound buffer.
void Audio::Release()
{
    Stop();

    if (deviceID_)
    {
        SDL_CloseAudioDevice(deviceID_);
        deviceID_ = 0;
        clipBuffer_.reset();
    }
}
/// Actually update sound sources with the specific timestep. Called internally.
void Audio::UpdateInternal(float timeStep)
{
    URHO3D_PROFILE(UpdateAudio);

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
