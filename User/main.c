// Serial MIDI player for CH32V003
//
//  Input:  PD6 RX (MIDI-In)
//  Output: PC4 Speaker
//  Output: PC1 LED

#include "debug.h"
#include "NoiseDrum.h"

#define TIME_UNIT                 2000000
#define OUTPUT_SAMPLING_FREQUENCY 16000
#define PSG_DEVIDE_FACTOR         9
#define CHANNEL_COUNT             20
#define SAMPLING_INTERVAL         (TIME_UNIT/OUTPUT_SAMPLING_FREQUENCY)
#define RX_BUFFER_LEN             256
#define SERIAL_BPS                38400
//#define SERIAL_BPS                31250

// Beep structure
typedef struct Beep_
{
    uint32_t psg_osc_intervalHalf;
    uint32_t psg_osc_interval;
    uint32_t psg_osc_counter;
    uint8_t psg_tone_on;
    uint8_t psg_midi_inuse;
    uint8_t psg_midi_inuse_ch;
    uint8_t psg_midi_note;
} Beep;

// Volume table
static const uint16_t psg_volume[] = { 0x00, 0x00, 0x17, 0x20, 0x27, 0x30, 0x37, 0x40,
        0x47, 0x50, 0x57, 0x60, 0x67, 0x70, 0x77, 0x80, 0x87, 0x90, 0x97, 0xa0,
        0xa7, 0xb0, 0xb7, 0xc0, 0xc7, 0xd0, 0xd7, 0xe0, 0xe7, 0xf0, 0xf7, 0xff };

// Note毎の周波数の半分の値
static const uint32_t toneIntervalHalf[] =
{

    122312, 115447, 108967, 102851, 97079, 91630, 86487, 81633,
    77051, 72727, 68645, 64792, 61156, 57723, 54483, 51425,
    48539, 45815, 43243, 40816, 38525, 36363, 34322, 32396,
    30578, 28861, 27241, 25712, 24269, 22907, 21621, 20408,
    19262, 18181, 17161, 16198, 15289, 14430, 13620, 12856,
    12134, 11453, 10810, 10204, 9631, 9090, 8580, 8099,
    7644, 7215, 6810, 6428, 6067, 5726, 5405, 5102,
    4815, 4545, 4290, 4049, 3822, 3607, 3405, 3214,
    3033, 2863, 2702, 2551, 2407, 2272, 2145, 2024,
    1911, 1803, 1702, 1607, 1516, 1431, 1351, 1275,
    1203, 1136, 1072, 1012, 955, 901, 851, 803,
    758, 715, 675, 637, 601, 568, 536, 506,
    477, 450, 425, 401, 379, 357, 337, 318,
    300, 284, 268, 253, 238, 225, 212, 200,
    189, 178, 168, 159, 150, 142, 134, 126,
    119, 112, 106, 100, 94, 89, 84, 79
};

// リズムノート変換テーブル
static const uint8_t Note35_57ChangeTable[] =
{
	0 , 0, 5, 1, 255, 6, 2, 7,
	4, 255, 2, 8, 3, 3, 9, 4,
	10, 255, 255, 255, 255, 255, 9
};

// 受信リングバッファ
#define RX_BUFFER_LENGTH 256
volatile uint8_t rxBuffer[RX_BUFFER_LENGTH];
uint8_t rxIndex = 0;
uint32_t lastRxIndex = RX_BUFFER_LENGTH;

// Beep
Beep beep[CHANNEL_COUNT];
uint16_t psg_master_volume;
uint8_t midi_ch_volume[16];

// NoiseDrum
Drum drum;

// LED
static int ledCount = 0;
uint8_t led;

// PWM設定
void SetupPWMOut(void)
{
    TIM_OCInitTypeDef TIM_OCInitStructure = {0};
    TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure = {0};
    TIM_TimeBaseInitStructure.TIM_Period = 255;
    TIM_TimeBaseInitStructure.TIM_Prescaler = 0;
    TIM_TimeBaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM1, &TIM_TimeBaseInitStructure);
    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_Pulse = 255;
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
    TIM_OC4Init(TIM1, &TIM_OCInitStructure);
    TIM_CtrlPWMOutputs(TIM1, ENABLE);
    TIM_OC4PreloadConfig(TIM1, TIM_OCPreload_Disable);
    TIM_ARRPreloadConfig(TIM1, ENABLE);
    TIM_Cmd(TIM1, ENABLE);
}

// 音声出力ピンをPC4に設定
void SetupOutput(void)
{
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC | RCC_APB2Periph_TIM1, ENABLE);
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOC, &GPIO_InitStructure);
}

// LEDピンをPC1に設定
void SetupLed(void)
{
    led = 0;
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1 | GPIO_Pin_2;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOC, &GPIO_InitStructure);
}

void BlinkLED(void)
{
	++ ledCount;
	if(ledCount > 10)
	{
		ledCount = 0;
	}
	else
	{
		return;
	}
    led = 1 - led;
    if(led == 1)
    {
        GPIO_WriteBit(GPIOC, GPIO_Pin_1, Bit_SET);
    }
    else
    {
        GPIO_WriteBit(GPIOC, GPIO_Pin_1, Bit_RESET);
    }
}

void psg_reset(void)
{
    for(int i = 0; i < CHANNEL_COUNT; i ++)
    {
        beep[i].psg_osc_intervalHalf = UINT32_MAX;
        beep[i].psg_osc_interval = UINT32_MAX;
        beep[i].psg_osc_counter = 0;
        beep[i].psg_tone_on = 0;
        beep[i].psg_midi_inuse=0;
    }
}

static inline void noteon(uint8_t i, uint8_t note, uint8_t volume)
{
    beep[i].psg_osc_intervalHalf = toneIntervalHalf[note];
    beep[i].psg_osc_interval = toneIntervalHalf[note] << 1;
    beep[i].psg_osc_counter = 0;
    beep[i].psg_tone_on = 1;
}

static inline void noteoff(uint8_t i, uint8_t note)
{
    beep[i].psg_osc_intervalHalf = UINT32_MAX;
    beep[i].psg_osc_interval = UINT32_MAX;
    beep[i].psg_tone_on = 0;
}

// シリアル初期化
// PD6 RX (MIDI-In) Setting
void SetupUSART(uint32_t bps)
{
    GPIO_InitTypeDef  GPIO_InitStructure = {0};
    USART_InitTypeDef USART_InitStructure = {0};
    DMA_InitTypeDef DMA_InitStructure = {0};
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOD | RCC_APB2Periph_USART1, ENABLE);

    // GPIO Setting
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOD, &GPIO_InitStructure);

    // USART Setting
    USART_InitStructure.USART_BaudRate = bps;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;
    USART_Init(USART1, &USART_InitStructure);
    USART_DMACmd(USART1, USART_DMAReq_Rx, ENABLE);
    USART_Cmd(USART1, ENABLE);

    // DMA Setting
    RCC_AHBPeriphClockCmd( RCC_AHBPeriph_DMA1, ENABLE);
    DMA_DeInit(DMA1_Channel5);
    DMA_InitStructure.DMA_PeripheralBaseAddr = (u32)&USART1->DATAR;
    DMA_InitStructure.DMA_MemoryBaseAddr = (u32)(&rxBuffer);
    DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralSRC;
    DMA_InitStructure.DMA_BufferSize = RX_BUFFER_LENGTH;
    DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
    DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;
    DMA_InitStructure.DMA_Mode = DMA_Mode_Circular;
    DMA_InitStructure.DMA_Priority = DMA_Priority_VeryHigh;
    DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;
    DMA_Init(DMA1_Channel5, &DMA_InitStructure);
    DMA_Cmd(DMA1_Channel5, ENABLE);
}

// シリアルから1バイト読み込み
static inline uint8_t ReadByte()
{
    uint8_t byteData;
    // データが来るまで待ち続ける
    while(1)
    {
        if(DMA_GetCurrDataCounter(DMA1_Channel5) != lastRxIndex)
        {
            break;
        }
    }
    byteData = rxBuffer[rxIndex];
    -- lastRxIndex;
    if(lastRxIndex == 0)
    {
        lastRxIndex = RX_BUFFER_LENGTH;
    }
    ++ rxIndex;
    if(rxIndex >= RX_BUFFER_LENGTH)
    {
        rxIndex = 0;
    }
    return byteData;
}

// タイマ割り込み設定
void SetupSysTick(void)
{
    // 割り込み優先度設定
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
    // タイマ割り込み設定
    NVIC_EnableIRQ(SysTicK_IRQn);
    SysTick->SR &= ~(1 << 0);
    SysTick->CMP = (SystemCoreClock / OUTPUT_SAMPLING_FREQUENCY) - 1;
    SysTick->CNT = 0;
    SysTick->CTLR = 0xF;
}

void SysTick_Handler(void) __attribute__((interrupt("WCH-Interrupt-fast")));

void SysTick_Handler(void)
{
    uint16_t master_volume;
    uint8_t tone_output[CHANNEL_COUNT];
    TIM1->CH4CVR = psg_master_volume;
// Run Oscillator
    for(int i = 0; i < CHANNEL_COUNT; i ++)
    {
        uint32_t pon_count = beep[i].psg_osc_counter += SAMPLING_INTERVAL;
        if(pon_count < (beep[i].psg_osc_intervalHalf))
        {
            tone_output[i] = beep[i].psg_tone_on;
        }
        else if (pon_count > beep[i].psg_osc_interval)
        {
            beep[i].psg_osc_counter -= beep[i].psg_osc_interval;
            tone_output[i] = beep[i].psg_tone_on;
        }
        else
        {
            tone_output[i] = 0;
        }
    }
// Mixer
    master_volume = 0;
    for(int i = 0; i < CHANNEL_COUNT; i ++)
    {
        if(beep[i].psg_tone_on == 1)
        {
            if(tone_output[i] != 0)
            {
                master_volume += psg_volume[midi_ch_volume[beep[i].psg_midi_inuse_ch] * 2 + 1];
            }
        }
    }
    master_volume += NoiseDrumGetData(&drum);
    psg_master_volume = master_volume / PSG_DEVIDE_FACTOR;
    if(psg_master_volume > 255)
    {
        psg_master_volume = 255;
    }
    SysTick->SR &= 0;
}

// メイン
int main(void)
{
    uint8_t midicc1, midicc2, midinote, midivel;
    uint8_t override;

    // 割り込み初期化
    SetupSysTick();

    // シリアル初期化
    SetupUSART(SERIAL_BPS);

    // PWM設定
    SetupOutput();
    SetupPWMOut();

    // LED初期化
    SetupLed();

    // Drum初期化
    NoiseDrumInitialize(&drum);

    // PowerLED
    GPIO_WriteBit(GPIOC, GPIO_Pin_2, Bit_SET);

    psg_master_volume = 0;
    psg_reset();

    while(1)
    {
        // Listen USART
        uint8_t midicmd = ReadByte();
        uint8_t midich = midicmd&0xf;
        BlinkLED();
        switch(midicmd & 0xF0)
        {
        case 0x80: // Note off
            midinote = ReadByte();
            midivel = ReadByte();
            for(int i = 0; i < CHANNEL_COUNT; i ++)
            {
                if((beep[i].psg_midi_inuse == 1) && (beep[i].psg_midi_inuse_ch == midich) && (beep[i].psg_midi_note == midinote))
                {
                    noteoff(i, midinote);
                    beep[i].psg_midi_inuse = 0;
                }
            }
            break;
        case 0x90: // Note on
            midinote = ReadByte();
            midivel = ReadByte();
            if(midich != 9)
            {
                if(midivel != 0)
                {
                    // check note is already on
                    override = 0;
                    for(int i = 0; i < CHANNEL_COUNT; i ++)
                    {
                        if((beep[i].psg_midi_inuse == 1) && (beep[i].psg_midi_inuse_ch == midich) && (beep[i].psg_midi_note == midinote))
                        {
                            override=1;
                        }
                    }
                    if(override == 0)
                    {
                        for(int i = 0; i < CHANNEL_COUNT; i ++)
                        {
                            if(beep[i].psg_midi_inuse == 0)
                            {
                                noteon(i, midinote,midi_ch_volume[midich]);
                                beep[i].psg_midi_inuse = 1;
                                beep[i].psg_midi_inuse_ch = midich;
                                beep[i].psg_midi_note = midinote;
                                break;
                            }
                        }
                    }
                } else {
                    for(int i = 0; i < CHANNEL_COUNT; i ++)
                    {
                        if((beep[i].psg_midi_inuse == 1) && (beep[i].psg_midi_inuse_ch == midich) && (beep[i].psg_midi_note == midinote))
                        {
                            noteoff(i, midinote);
                            beep[i].psg_midi_inuse = 0;
                        }
                    }
                }
            }
            else
            {
                if((35 <= midinote) && (midinote <= 57))
                {
                    uint8_t rythmNote = Note35_57ChangeTable[midinote - 35];
                    if(rythmNote < 11)
                    {
                        NoiseDrumSetPlay(&drum, rythmNote);
                    }
                }
            }
            break;
        case 0xB0:
            // Channel control
            midicc1 = ReadByte();
            switch(midicc1)
            {
            case 7:
            case 11: // Expression
                midicc2 = ReadByte();
                midi_ch_volume[midich] = (midicc2 >> 3);
                if(midich == 9)
                {
                    NoiseDrumSetVolume(&drum, midi_ch_volume[midich]);
                }
                break;

            case 0: //Bank select
            case 120:// All note off
            case 121:// All reset
            case 123:
            case 124:
            case 125:
            case 126:
            case 127:
                for(int i = 0; i < CHANNEL_COUNT; i ++)
                {
                    if((beep[i].psg_midi_inuse == 1) && (beep[i].psg_midi_inuse_ch == midich))
                    {
                        noteoff(i, beep[i].psg_midi_note);
                        beep[i].psg_midi_inuse = 0;
                    }
                }
                break;
            default:
                break;
            }
            break;
        case 0xC0:
            // Program change
            for(int i = 0; i < CHANNEL_COUNT; i ++)
            {
                if((beep[i].psg_midi_inuse == 1) && (beep[i].psg_midi_inuse_ch == midich))
                {
                    noteoff(i, beep[i].psg_midi_note);
                    beep[i].psg_midi_inuse = 0;
                }
            }
            break;
        default: // Skip
            break;
        }
    }
}
