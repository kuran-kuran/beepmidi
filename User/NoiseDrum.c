#include <string.h>
#include "NoiseDrum.h"

static const uint8_t volumeTable[] =
{
    0, 10, 12, 16, 20, 25, 32, 40, 51, 64, 80, 101, 128, 161, 203, 255, 255
};

// Bass Drum
static const Effect effect000[] =
{
    1 * INTERVAL60, 1500 * FREQUENCY_SCALE, 31 * FREQUENCY_SCALE, 54, 15, 0, 0, 127 * FREQUENCY_SCALE, 0, 0,
    8 * INTERVAL60, 1700 * FREQUENCY_SCALE, 1, 62, 16, 1200 * INTERVAL, 0, 127 * FREQUENCY_SCALE, 0, 0,
};

// Snare Drum
static const Effect effect001[] =
{
    14 * INTERVAL60, 400 * FREQUENCY_SCALE, 7 * FREQUENCY_SCALE, 54, 16, 3000 * INTERVAL, 0, 93 * FREQUENCY_SCALE, 15 * INTERVAL60, 2 * FREQUENCY_SCALE,
};

// Low Tom
static const Effect effect002[] =
{
    2 * INTERVAL60, 700 * FREQUENCY_SCALE, 1, 54, 15, 0, 0, 100 * FREQUENCY_SCALE, 0, 0,
    14 * INTERVAL60, 900 * FREQUENCY_SCALE, 1, 54, 16, 2500 * INTERVAL, 0, 100 * FREQUENCY_SCALE, 0, 0,
};

// Middle Tom
static const Effect effect003[] =
{
    2 * INTERVAL60, 500 * FREQUENCY_SCALE, 5 * FREQUENCY_SCALE, 54, 15, 0, 0, 60 * FREQUENCY_SCALE, 0, 0,
    14 * INTERVAL60, 620 * FREQUENCY_SCALE, 1, 54, 16, 2500 * INTERVAL, 0, 60 * FREQUENCY_SCALE, 0, 0,
};

// High Tom
static const Effect effect004[] =
{
    2 * INTERVAL60, 300 * FREQUENCY_SCALE, 1, 54, 15, 0, 0, 50 * FREQUENCY_SCALE, 0, 0,
    14 * INTERVAL60, 400 * FREQUENCY_SCALE, 1, 54, 16, 2500 * INTERVAL, 0, 50 * FREQUENCY_SCALE, 0, 0,
};

// Rim Shot
static const Effect effect005[] =
{
    2 * INTERVAL60, 55 * FREQUENCY_SCALE, 1, 62, 16, 300 * INTERVAL, 0, 100 * FREQUENCY_SCALE, 0, 0,
};

// Snare Drum 2
static const Effect effect006[] =
{
    16 * INTERVAL60, 0, 15 * FREQUENCY_SCALE, 55, 16, 3000 * INTERVAL, 0, 0, 15 * INTERVAL60, 1 * FREQUENCY_SCALE,
};

// Hi-Hat Close
static const Effect effect007[] =
{
    6 * INTERVAL60, 39 * FREQUENCY_SCALE, 1, 54, 16, 500 * INTERVAL, 0, 0, 0, 0,
};

// Hi-Hat Open
static const Effect effect008[] =
{
    32 * INTERVAL60, 39 * FREQUENCY_SCALE, 1, 54, 16, 5000 * INTERVAL, 0, 0, 0, 0,
};

// Crush Cymbal
static const Effect effect009[] =
{
    31 * INTERVAL60, 40 * FREQUENCY_SCALE, 31 * FREQUENCY_SCALE, 54, 16, 5000 * INTERVAL, 0, 0, 15 * INTERVAL60, 1 * FREQUENCY_SCALE,
};

// Ride Cymbal
static const Effect effect010[] =
{
    31 * INTERVAL60, 30 * FREQUENCY_SCALE, 1, 54, 16, 5000 * INTERVAL, 0, 0, 0, 0,
};

const EffectData effectDatas[] =
{
    {sizeof(effect000) / sizeof(Effect), effect000},
    {sizeof(effect001) / sizeof(Effect), effect001},
    {sizeof(effect002) / sizeof(Effect), effect002},
    {sizeof(effect003) / sizeof(Effect), effect003},
    {sizeof(effect004) / sizeof(Effect), effect004},
    {sizeof(effect005) / sizeof(Effect), effect005},
    {sizeof(effect006) / sizeof(Effect), effect006},
    {sizeof(effect007) / sizeof(Effect), effect007},
    {sizeof(effect008) / sizeof(Effect), effect008},
    {sizeof(effect009) / sizeof(Effect), effect009},
    {sizeof(effect010) / sizeof(Effect), effect010}
};

// 変数
static unsigned short rndSeed;

unsigned char Rnd(void)
{
    unsigned short hl = rndSeed;
    unsigned short de = hl;
    hl += hl;
    hl += hl;
    hl += de;
    hl += 0x3711;
    rndSeed = hl;
    return hl >> 8;
}

void NoiseDrumInitialize(Drum* drum)
{
    memset(drum, 0, sizeof(Drum));
}

void NoiseDrumSetPlay(Drum* drum, uint8_t index)
{
    drum->effectData = &effectDatas[index];
    drum->playIndex = 0;
    drum->phase = 0;
}

void NoiseDrumSetVolume(Drum* drum, uint8_t volume)
{
    drum->volume = volume;
}

void NoiseDrumInitializePhase(Drum* drum)
{
    drum->toneInterval = drum->effectData->data[drum->playIndex].toneFrequency;
    drum->toneIntervalHalf = drum->toneInterval >> 1;
    drum->noiseInterval = drum->effectData->data[drum->playIndex].noiseFrequency;
    drum->noiseReleaseCounter = 0;
    drum->toneSweepCounter = 0;
    drum->noiseSweepCounter = 0;
    drum->phase = 1;
}

void NoiseDrumNextData(Drum* drum)
{
    if(drum->playIndex < drum->effectData[drum->playIndex].dataCount)
    {
        ++ drum->playIndex;
        drum->phase = 0;
        return;
    }
    drum->phase = 2;
}

uint8_t NoiseDrumGetData(Drum* drum)
{
    if(drum->phase == 2)
    {
        return 0;
    }
    if(drum->phase == 0)
    {
        NoiseDrumInitializePhase(drum);
    }
    uint8_t data = 0;
    drum->noiseReleaseCounter += INTERVAL;
    // Tone
    if((drum->effectData->data[drum->playIndex].mixControl & 1) == 0)
    {
        // Tone sweep
        drum->toneSweepCounter += INTERVAL;
        if((drum->effectData->data[drum->playIndex].toneSweep > 0) && (drum->toneSweepCounter > FPS60_INTERVAL))
        {
            drum->toneInterval += drum->effectData->data[drum->playIndex].toneSweep;
            drum->toneIntervalHalf = drum->toneInterval >> 1;
            drum->toneSweepCounter -= FPS60_INTERVAL;
        }
        drum->toneCounter += INTERVAL;
        if(drum->toneCounter < drum->toneIntervalHalf)
        {
            data = 1;
        }
        else if(drum->toneCounter > drum->toneInterval)
        {
            drum->toneCounter -= drum->toneInterval;
            data = 1;
        }
        else
        {
            return 0;
        }
    }
    // Noise
    if((drum->effectData->data[drum->playIndex].mixControl & 8) == 0)
    {
        // Noise sweep
        drum->noiseSweepCounter += INTERVAL;
        if((drum->effectData->data[drum->playIndex].noiseSweepCount > 0) && (drum->noiseSweepCounter > drum->effectData->data[drum->playIndex].noiseSweepCount))
        {
            drum->noiseInterval += drum->effectData->data[drum->playIndex].noiseSweepData;
            drum->noiseSweepCounter -= drum->effectData->data[drum->playIndex].noiseSweepCount;
        }
        drum->counter += INTERVAL;
        if(drum->counter >= drum->noiseInterval)
        {
            // 音量計算
            int noise = 0;
            if(drum->effectData->data[drum->playIndex].envelopeFrequency > 0)
            {
                if(drum->noiseReleaseCounter < drum->effectData->data[drum->playIndex].envelopeFrequency)
                {
                    noise = 1;
                }
            }
            else
            {
                noise = 1;
            }
            data = Rnd() < 128 ? 0 : noise;
            drum->noiseBeforeData = data;
            drum->counter -= drum->noiseInterval;
        }
        else
        {
            data = drum->noiseBeforeData;
        }
    }
    if(data == 1)
    {
        if(drum->effectData->data[drum->playIndex].envelopeFrequency == 0)
        {
            if(drum->noiseReleaseCounter > drum->effectData->data[drum->playIndex].time)
            {
                NoiseDrumNextData(drum);
            }
            return volumeTable[drum->effectData->data[drum->playIndex].volume];
        }
        if(drum->noiseReleaseCounter > drum->effectData->data[drum->playIndex].envelopeFrequency)
        {
            // エンベロープ終了だったら音量0
            NoiseDrumNextData(drum);
            return 0;
        }
        // 線形補完
        uint8_t volume = drum->effectData->data[drum->playIndex].volume - drum->effectData->data[drum->playIndex].volume * drum->noiseReleaseCounter / drum->effectData->data[drum->playIndex].envelopeFrequency;
        volume = volume * drum->volume / 16;
        volume = volumeTable[volume];
        return volume;
    }
    return 0;
}
