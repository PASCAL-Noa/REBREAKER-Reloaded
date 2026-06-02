#pragma once
#include "ECS/System.h"

class AudioMixer;

class AudioSystem : public System
{
public:
    AudioSystem(Registry& registry, AudioMixer& mixer);

    void OnUpdate(float dt) override;

private:
    AudioMixer& m_mixer;
};