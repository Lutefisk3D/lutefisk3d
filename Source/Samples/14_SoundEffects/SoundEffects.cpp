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
#include "SoundEffects.h"

#include <Lutefisk3D/Audio/Audio.h>
#include <Lutefisk3D/UI/Button.h>
#include <Lutefisk3D/Engine/Engine.h>
#include <Lutefisk3D/UI/Font.h>
#include <Lutefisk3D/Input/Input.h>
#include <Lutefisk3D/IO/Log.h>
#include <Lutefisk3D/Resource/ResourceCache.h>
#include <Lutefisk3D/Scene/Scene.h>
#include <Lutefisk3D/UI/Slider.h>
#include <Lutefisk3D/Audio/Sound.h>
#include <Lutefisk3D/Audio/Audio.h>
#include <Lutefisk3D/Audio/AudioEvents.h>
#include <Lutefisk3D/Audio/SoundSource.h>
#include <Lutefisk3D/UI/Text.h>
#include <Lutefisk3D/UI/UI.h>
#include <Lutefisk3D/UI/UIEvents.h>

using namespace Urho3D;

// Custom variable identifier for storing sound effect name within the UI element
static const StringHash VAR_SOUNDRESOURCE("SoundResource");
static const unsigned NUM_SOUNDS = 3;

static QString soundNames[] = {
    "Fist",
    "Explosion",
    "Power-up"
};

static QString soundResourceNames[] = {
    "Sounds/PlayerFistHit.wav",
    "Sounds/BigExplosion.wav",
    "Sounds/Powerup.wav"
};

URHO3D_DEFINE_APPLICATION_MAIN(SoundEffects)

SoundEffects::SoundEffects(Context* context) :
    Sample("SoundEffects",context)
{
}

void SoundEffects::Setup()
{
    // Modify engine startup parameters
    Sample::Setup();
    engineParameters_["Sound"] = true;
}
void SoundEffects::Start()
{
    // Execute base class startup
    Sample::Start();

    // Enable OS cursor
    m_context->m_InputSystem.get()->SetMouseVisible(true);

    // Create the user interface
    CreateUI();
}

void SoundEffects::CreateUI()
{
    // Create a scene which will not be actually rendered, but is used to hold SoundSource components while they play sounds
    scene_ = new Scene(m_context);

    UIElement* root = m_context->m_UISystem.get()->GetRoot();
    ResourceCache* cache = m_context->m_ResourceCache.get();
    XMLFile* uiStyle = cache->GetResource<XMLFile>("UI/DefaultStyle.xml");
    // Set style to the UI root so that elements will inherit it
    root->SetDefaultStyle(uiStyle);

    // Create buttons for playing back sounds
    for (unsigned i = 0; i < NUM_SOUNDS; ++i)
    {
        Button* button = CreateButton(i * 140 + 20, 20, 120, 40, soundNames[i]);
        // Store the sound effect resource name as a custom variable into the button
        button->SetVar(VAR_SOUNDRESOURCE, soundResourceNames[i]);
        button->pressed.Connect(this,&SoundEffects::HandlePlaySound);
    }

    // Create buttons for playing/stopping music
    Button* button = CreateButton(20, 80, 120, 40, "Play Music");
    button->released.Connect(this,&SoundEffects::HandlePlayMusic));

    button = CreateButton(160, 80, 120, 40, "Stop Music");
    button->released.Connect(this,&HandleStopMusic);

    Audio* audio = m_context->GetSubsystemT<Audio>();

    // Create sliders for controlling sound and music master volume
    Slider* slider = CreateSlider(20, 140, 200, 20, "Sound Volume");
    slider->SetValue(audio->GetMasterGain(SOUND_EFFECT));
    SubscribeToEvent(slider, E_SLIDERCHANGED, URHO3D_HANDLER(SoundEffects, HandleSoundVolume));

    slider = CreateSlider(20, 200, 200, 20, "Music Volume");
    slider->SetValue(audio->GetMasterGain(SOUND_MUSIC));
    SubscribeToEvent(slider, E_SLIDERCHANGED, URHO3D_HANDLER(SoundEffects, HandleMusicVolume));
}

Button* SoundEffects::CreateButton(int x, int y, int xSize, int ySize, const QString & text)
{
    UIElement* root = m_context->m_UISystem.get()->GetRoot();
    ResourceCache* cache = m_context->m_ResourceCache.get();
    Font* font = cache->GetResource<Font>("Fonts/Anonymous Pro.ttf");

    // Create the button and center the text onto it
    Button* button = root->CreateChild<Button>();
    button->SetStyleAuto();
    button->SetPosition(x, y);
    button->SetSize(xSize, ySize);

    Text* buttonText = button->CreateChild<Text>();
    buttonText->SetAlignment(HA_CENTER, VA_CENTER);
    buttonText->SetFont(font, 12);
    buttonText->SetText(text);

    return button;
}

Slider* SoundEffects::CreateSlider(int x, int y, int xSize, int ySize, const QString & text)
{
    UIElement* root = m_context->m_UISystem.get()->GetRoot();
    ResourceCache* cache = m_context->m_ResourceCache.get();
    Font* font = cache->GetResource<Font>("Fonts/Anonymous Pro.ttf");

    // Create text and slider below it
    Text* sliderText = root->CreateChild<Text>();
    sliderText->SetPosition(x, y);
    sliderText->SetFont(font, 12);
    sliderText->SetText(text);

    Slider* slider = root->CreateChild<Slider>();
    slider->SetStyleAuto();
    slider->SetPosition(x, y + 20);
    slider->SetSize(xSize, ySize);
    // Use 0-1 range for controlling sound/music master volume
    slider->SetRange(1.0f);

    return slider;
}

void SoundEffects::HandlePlaySound(UIElement *btn)
{
    Button* button = static_cast<Button*>(btn);
    const QString& soundResourceName = button->GetVar(VAR_SOUNDRESOURCE).GetString();

    // Get the sound resource
    ResourceCache* cache = m_context->m_ResourceCache.get();
    Sound* sound = cache->GetResource<Sound>(soundResourceName);

    if (sound)
    {
        // Create a scene node with a SoundSource component for playing the sound. The SoundSource component plays
        // non-positional audio, so its 3D position in the scene does not matter. For positional sounds the
        // SoundSource3D component would be used instead
        Node* soundNode = scene_->CreateChild("Sound");
        SoundSource* soundSource = soundNode->CreateComponent<SoundSource>();
        soundSource->Play(sound);
        // In case we also play music, set the sound volume below maximum so that we don't clip the output
        soundSource->SetGain(0.75f);
        // Subscribe to the "sound finished" event generated by the SoundSource for removing the node once the sound has played
        // Note: the event is sent through the Node (similar to e.g. node physics collision and animation trigger events)
        // to not require subscribing to the particular component
        soundNode->soundFinished.Connect(this,&SoundEffects::HandleSoundFinished);
    }
}

void SoundEffects::HandlePlayMusic(StringHash eventType, VariantMap& eventData)
{
    // Check if the music player node/component already exist
    if (scene_->GetChild("Music"))
        return;

    ResourceCache* cache = m_context->m_ResourceCache.get();
    Sound* music = cache->GetResource<Sound>("Music/Ninja Gods.ogg");
    // Set the song to loop
    music->SetLooped(true);

    // Create a scene node and a sound source for the music
    Node* musicNode = scene_->CreateChild("Music");
    SoundSource* musicSource = musicNode->CreateComponent<SoundSource>();
    // Set the sound type to music so that master volume control works correctly
    musicSource->SetSoundType(SOUND_MUSIC);
    musicSource->Play(music);
}

void SoundEffects::HandleStopMusic(StringHash eventType, VariantMap& eventData)
{
    // Remove the music player node from the scene
    scene_->RemoveChild(scene_->GetChild("Music"));
}

void SoundEffects::HandleSoundVolume(StringHash eventType, VariantMap& eventData)
{
    using namespace SliderChanged;

    float newVolume = eventData[P_VALUE].GetFloat();
    GetSubsystem<Audio>()->SetMasterGain(SOUND_EFFECT, newVolume);
}

void SoundEffects::HandleMusicVolume(StringHash eventType, VariantMap& eventData)
{
    using namespace SliderChanged;

    float newVolume = eventData[P_VALUE].GetFloat();
    GetSubsystem<Audio>()->SetMasterGain(SOUND_MUSIC, newVolume);
}

void SoundEffects::HandleSoundFinished(Node* soundNode, SoundSource *src, Sound *)
{
    if (soundNode)
        soundNode->Remove();
}
