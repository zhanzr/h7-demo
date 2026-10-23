#include <stdio.h>

#include "board.h"
#include "uart_printf.h"
#include "stm32h7xx_ll_adc.h"

/* On-board LED: LD3 on PA8 (see cubemx_file/Core/Inc/main.h). */
#define LED_PORT GPIOA
#define LED_PIN  GPIO_PIN_8

/* ADC3 (the only instance wired to the internal channels on the H750) samples
 * the die temperature sensor (ADC3_IN18): 16 bit, single polled conversions,
 * once per blink loop (~1 Hz). 16 bit because the factory calibration words
 * and the LL temperature maths are 16-bit based on this ADC IP. */
static ADC_HandleTypeDef hadc3;

/* H750 temperature calibration words live in system flash (TS_CAL1 30 degC
 * at 0x1FF1E820, TS_CAL2 110/130 degC at 0x1FF1E840; 16-bit words at
 * Vref+ = 3.3 V). Some parts leave them unprogrammed so they read garbage,
 * which poisons the temperature maths. Validate them and fall back to the
 * datasheet typical values when they look wrong:
 *   sane range 8000..18000 counts (~0.4..0.9 V), CAL2 > CAL1
 */
#define TS_V30_MV         762    /* temp sensor 0.762 V @ 30 degC typ */
#define TS_AVG_SLOPE_UV_C 2500   /* 2.5 mV/degC typ                   */
#define VREFBUF_MV        2500   /* internal VREFBUF, scale 0 output   */
#define VREF_NOM_MV       3300   /* board Vref+ if the buffer is off  */

static uint16_t g_raw_temp, g_raw_vref, g_raw_vbat;
static int32_t  g_temp_c;
static uint32_t g_vbat_mv;
static int      g_cal_factory   = -1;    /* 1: factory words, 0: typicals */
static int      g_vref_cal_ok   = -1;    /* VREFINT_CAL word validated    */
static uint32_t g_vref_mv;               /* Vref+ the conversions ran at  */
static uint32_t g_vref_meas_mv;          /* Vref+ measured via VREFINT    */

/* The ADC reference must be present for any conversion, so try the internal
 * VREFBUF (VRS=000, ~2.5 V) first. Per RM0433 table "VREF buffer modes" the
 * clean sequence is a single CSR write straight into internal-reference mode
 * (ENVR=1, HIZ=0): passing through HIZ=0 with ENVR=0 would pull VREF+ to
 * VSSA, and ENVR=1 with HIZ=1 is hold mode where VRR detection is off.
 *
 * VREFBUF is clocked by its own gate, RCC_APB4ENR.VREFEN (bit 15) - it is
 * *not* covered by RCC_APB4ENR.SYSCFGEN (bit 1), which only gates the SYSCFG
 * registers at 0x58000400. With VREFEN cleared the VREFBUF registers read
 * back all-zero (not even the reset HIZ=1) and writes are silently
 * discarded, which looks exactly like "VREFBUF not implemented". Neither
 * HAL_Init() nor this project's Board_Init() enables the gate, and the HAL
 * HAL_SYSCFG_EnableVREFBUF()/..._VoltageScalingConfig() helpers assume it is
 * already on, so it must be enabled here before the first CSR access. */
static void VREFBUF_Start(void)
{
    uint32_t start, csr;

    __HAL_RCC_VREF_CLK_ENABLE();

    csr = VREFBUF->CSR;   /* reset: HIZ=1, ENVR=0, VRR=0, VRS=000 */
    printf("VREFBUF CSR before: %08lX\r\n", (unsigned long)csr);

    /* VRS=000 is 0, so SCALE0 needs no explicit bit; it is kept in the
     * expression to document the selected range (VREFBUF_VOLTAGE_SCALE0). */
    MODIFY_REG(VREFBUF->CSR,
               VREFBUF_CSR_ENVR | VREFBUF_CSR_HIZ | VREFBUF_CSR_VRS,
               VREFBUF_CSR_ENVR | SYSCFG_VREFBUF_VOLTAGE_SCALE0);

    csr = VREFBUF->CSR;
    printf("VREFBUF CSR after ENVR=1,HIZ=0 write: %08lX\r\n", (unsigned long)csr);

    start = HAL_GetTick();
    while (READ_BIT(VREFBUF->CSR, VREFBUF_CSR_VRR) == 0UL)
    {
        if ((HAL_GetTick() - start) > 500UL)
        {
            g_vref_mv = VREF_NOM_MV;
            printf("VREFBUF not ready after %lu ms (CSR %08lX) - check VDDA "
                   "and the VREF+ decoupling (1uF + 100nF). ADC reads will "
                   "rail while VREF+ is open.\r\n",
                   (unsigned long)(HAL_GetTick() - start),
                   (unsigned long)VREFBUF->CSR);
            return;
        }
    }

    g_vref_mv = VREFBUF_MV;
    printf("Vref+ = internal VREFBUF %lu mV (ready in %lu ms)\r\n",
           (unsigned long)g_vref_mv, (unsigned long)(HAL_GetTick() - start));
}

static void ADC3_Init(void)
{
    RCC_PeriphCLKInitTypeDef periph = {0};

    /* ADC3 kernel clock: PLL2 "P" @ 12.5 MHz from the 25 MHz HSE
     * (M=5 -> 5 MHz input, N=60 -> VCO 300 MHz, P=12 -> 25 MHz, /2 in the
     * ADC prescaler). A slow, asynchronous ADC clock is the accuracy-friendly
     * choice for the temperature sensor; the 480 MHz PLL1 tree stays
     * untouched. When the ADC selects PLL2, HAL_RCCEx_PeriphCLKConfig()
     * programs and turns on PLL2 from the .PLL2 fields below. */
    periph.PeriphClockSelection = RCC_PERIPHCLK_ADC;
    periph.AdcClockSelection    = RCC_ADCCLKSOURCE_PLL2;
    periph.PLL2.PLL2M           = 5;
    periph.PLL2.PLL2N           = 60;
    periph.PLL2.PLL2P           = 12;
    periph.PLL2.PLL2Q           = 2;
    periph.PLL2.PLL2R           = 2;
    periph.PLL2.PLL2RGE         = RCC_PLL2VCIRANGE_2;    /* 4..8 MHz PLL input */
    periph.PLL2.PLL2VCOSEL      = RCC_PLL2VCOMEDIUM;     /* VCO 150..420 MHz   */
    periph.PLL2.PLL2FRACN       = 0;
    if (HAL_RCCEx_PeriphCLKConfig(&periph) != HAL_OK)
    {
        Error_Handler();
    }

    __HAL_RCC_ADC3_CLK_ENABLE();

    hadc3.Instance                      = ADC3;
    hadc3.Init.ClockPrescaler           = ADC_CLOCK_ASYNC_DIV2;
    hadc3.Init.Resolution               = ADC_RESOLUTION_16B;
    hadc3.Init.ScanConvMode             = ADC_SCAN_DISABLE;
    hadc3.Init.EOCSelection             = ADC_EOC_SINGLE_CONV;
    hadc3.Init.LowPowerAutoWait         = DISABLE;
    hadc3.Init.ContinuousConvMode       = DISABLE;
    hadc3.Init.NbrOfConversion          = 1;
    hadc3.Init.DiscontinuousConvMode    = DISABLE;
    hadc3.Init.NbrOfDiscConversion      = 1;
    hadc3.Init.ExternalTrigConv         = ADC_SOFTWARE_START;
    hadc3.Init.ExternalTrigConvEdge     = ADC_EXTERNALTRIGCONVEDGE_NONE;
    hadc3.Init.ConversionDataManagement = ADC_CONVERSIONDATA_DR;
    hadc3.Init.Overrun                  = ADC_OVR_DATA_OVERWRITTEN;
    hadc3.Init.LeftBitShift             = ADC_LEFTBITSHIFT_NONE;
    hadc3.Init.OversamplingMode         = DISABLE;
    if (HAL_ADC_Init(&hadc3) != HAL_OK)
    {
        Error_Handler();
    }

    /* Preselect the three internal channels (PCSEL): mandatory on the 16-bit
     * H7 ADC IP - a channel that is not preselected is not sampled and its
     * conversions return garbage (RM0433 "ADC channel preselection"). The
     * HAL only does this for differential negative inputs, so set it here.
     * VREFINT (IN19) doubles as a reference-stability probe: its 1.21 V
     * bandgap is independent of the sensors, so a jittering raw_VREFINT
     * means the VREF+ node is moving. */
    LL_ADC_SetChannelPreselection(ADC3, ADC_CHANNEL_TEMPSENSOR);
    LL_ADC_SetChannelPreselection(ADC3, ADC_CHANNEL_VREFINT);
    LL_ADC_SetChannelPreselection(ADC3, ADC_CHANNEL_VBAT);

    /* Turn the three internal analog paths on (TSEN/VREFEN/VBATEN in
     * ADC3_COMMON.CCR; only writable while the ADC is disabled). The
     * sensors need a short start-up, covered by the delay in ADC3_Read(). */
    LL_ADC_SetCommonPathInternalCh(ADC3_COMMON,
                                   LL_ADC_PATH_INTERNAL_TEMPSENSOR |
                                   LL_ADC_PATH_INTERNAL_VREFINT |
                                   LL_ADC_PATH_INTERNAL_VBAT);

    /* Offset + linearity calibration: required on this ADC version, needs an
     * idle ADC (the HAL enables it for the duration of the calibration). */
    if (HAL_ADCEx_Calibration_Start(&hadc3, ADC_CALIB_OFFSET_LINEARITY,
                                    ADC_SINGLE_ENDED) != HAL_OK)
    {
        Error_Handler();
    }
}

static uint16_t ADC3_Read(uint32_t channel)
{
    ADC_ChannelConfTypeDef cfg = {0};

    cfg.Channel      = channel;
    cfg.Rank         = ADC_REGULAR_RANK_1;
    /* SMP code 0b111 = 810.5 cycles: 64.8 us @ 12.5 MHz, far above the
     * >= 5.3 us the temperature sensor needs for full accuracy. */
    cfg.SamplingTime = ADC_SAMPLETIME_810CYCLES_5;
    cfg.SingleDiff   = ADC_SINGLE_ENDED;
    cfg.OffsetNumber = ADC_OFFSET_NONE;
    cfg.Offset       = 0;
    if (HAL_ADC_ConfigChannel(&hadc3, &cfg) != HAL_OK)
    {
        Error_Handler();
    }

    /* Give the freshly enabled sensor / mux a moment to settle; at the 1 Hz
     * cadence this delay is free. */
    HAL_Delay(2);

    if (HAL_ADC_Start(&hadc3) != HAL_OK)
    {
        Error_Handler();
    }
    if (HAL_ADC_PollForConversion(&hadc3, 10) != HAL_OK)
    {
        Error_Handler();
    }
    uint16_t raw = (uint16_t)HAL_ADC_GetValue(&hadc3);
    HAL_ADC_Stop(&hadc3);

    return raw;
}

/* n-sample trimmed mean: drop the single min and max, average the rest.
 * At the 1 Hz cadence the ~50 ms spent here is free, and it rides out the
 * residual VREF+ wobble (see the burst diagnostics below) far better than a
 * single conversion. */
static uint16_t ADC3_ReadAvg(uint32_t channel, int n)
{
    uint16_t minv = 0xFFFF, maxv = 0;
    uint32_t sum = 0;

    for (int i = 0; i < n; i++)
    {
        uint16_t v = ADC3_Read(channel);
        if (v < minv)
        {
            minv = v;
        }
        if (v > maxv)
        {
            maxv = v;
        }
        sum += v;
    }
    return (uint16_t)((sum - minv - maxv) / (n - 2));
}

/* Sample the three internal channels and convert them to physical values.
 * The VREFINT reading gives the actual Vref+ (factory-calibrated bandgap vs
 * the 3.3 V-calibrated VREFINT_CAL word), which then compensates the
 * temperature maths; VBAT is measured through the internal /4 divider and
 * is noise unless a battery sits on the pin. Uses the factory calibration
 * words when they are sane, the datasheet typical values otherwise. */
static void ADC3_ReadInternals(void)
{
    uint32_t ts_cal1     = *TEMPSENSOR_CAL1_ADDR;
    uint32_t ts_cal2     = *TEMPSENSOR_CAL2_ADDR;
    uint32_t vrefint_cal = *VREFINT_CAL_ADDR;

    /* VREFINT first: the measured Vref+ refines the other two conversions. */
    g_raw_vref = ADC3_ReadAvg(ADC_CHANNEL_VREFINT, 16);
    if (g_vref_cal_ok < 0)
    {
        g_vref_cal_ok = (vrefint_cal >= 20000 && vrefint_cal <= 28000);
    }
    if (g_vref_cal_ok)
    {
        g_vref_meas_mv = (uint32_t)(((uint64_t)vrefint_cal * 3300UL) /
                                    g_raw_vref);
    }
    else
    {
        g_vref_meas_mv = g_vref_mv;   /* unvalidated cal word: assume nominal */
    }

    g_raw_temp = ADC3_ReadAvg(ADC_CHANNEL_TEMPSENSOR, 16);
    if (g_cal_factory < 0)
    {
        g_cal_factory = (ts_cal1 >= 8000 && ts_cal1 <= 18000 &&
                         ts_cal2 >= 8000 && ts_cal2 <= 18000 &&
                         ts_cal2 > ts_cal1);
    }

    if (g_cal_factory)
    {
        g_temp_c = __LL_ADC_CALC_TEMPERATURE(g_vref_meas_mv, g_raw_temp,
                                             LL_ADC_RESOLUTION_16B);
    }
    else
    {
        g_temp_c = __LL_ADC_CALC_TEMPERATURE_TYP_PARAMS(TS_AVG_SLOPE_UV_C,
                                                        TS_V30_MV, 30,
                                                        g_vref_meas_mv,
                                                        g_raw_temp,
                                                        LL_ADC_RESOLUTION_16B);
    }

    g_raw_vbat = ADC3_ReadAvg(ADC_CHANNEL_VBAT, 16);
    g_vbat_mv  = (uint32_t)(((uint64_t)g_raw_vbat * g_vref_meas_mv * 4) /
                            65535);
}

static void LED_Init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();

    GPIO_InitTypeDef gpio = {0};
    gpio.Pin   = LED_PIN;
    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Pull  = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LED_PORT, &gpio);

    HAL_GPIO_WritePin(LED_PORT, LED_PIN, GPIO_PIN_RESET);
}

int main(void)
{
    HAL_Init();
    Board_Init();
    LED_Init();
    VREFBUF_Start();
    ADC3_Init();

    printf("\r\n=== blink_hello on STM32H750VBTx @ %lu Hz ===\r\n",
           (unsigned long)SystemCoreClock);

    /* One-time diagnostics: factory calibration words (0x1FF1E820/840/860)
     * and the first raw conversions, so scaling problems are visible in the
     * log. Expected if the words are programmed (16-bit data): TS_CAL1
     * ~15000, VREFINT_CAL ~24000. A raw reading pinned at 65535 means the
     * ADC reference (VREF+ pin) is not usable - check it before suspecting
     * the code. */
    printf("ADC3 cal words: TS_CAL1 %lu  TS_CAL2 %lu  VREFINT_CAL %lu "
           "(chip rev 0x%04lX)\r\n",
           (unsigned long)*TEMPSENSOR_CAL1_ADDR,
           (unsigned long)*TEMPSENSOR_CAL2_ADDR,
           (unsigned long)*VREFINT_CAL_ADDR,
           (unsigned long)((DBGMCU->IDCODE >> 16) & 0xFFFFUL));

    ADC3_ReadInternals();
    printf("ADC3 raw: temp %lu  vref %lu  vbat %lu | using %s calibration\r\n",
           (unsigned long)g_raw_temp, (unsigned long)g_raw_vref,
           (unsigned long)g_raw_vbat,
           g_cal_factory ? "factory" : "nominal");

    /* Back-to-back burst: a stable reference / sensor gives counts within a
     * few LSB here, while tens-of-thousands spread means the VREF+ node the
     * VREFBUF drives is oscillating or floating (missing decoupling). The
     * VREFINT burst isolates the two: raw_VREFINT = 1.21 V / Vref * 65535
     * (~31700 counts at 2.5 V), and it must be rock steady. */
    printf("ADC3 burst temp:");
    for (int i = 0; i < 8; i++)
    {
        printf(" %lu", (unsigned long)ADC3_Read(ADC_CHANNEL_TEMPSENSOR));
    }
    printf("\r\nADC3 burst vrefint:");
    for (int i = 0; i < 8; i++)
    {
        printf(" %lu", (unsigned long)ADC3_Read(ADC_CHANNEL_VREFINT));
    }
    printf("\r\n");

    while (1)
    {
        HAL_GPIO_TogglePin(LED_PORT, LED_PIN);
        ADC3_ReadInternals();
        printf("Tdie %ld C | Vref+ %lu mV | VBAT %lu mV (raw t/v/b "
               "%lu/%lu/%lu)\r\n",
               (long)g_temp_c,
               (unsigned long)g_vref_meas_mv, (unsigned long)g_vbat_mv,
               (unsigned long)g_raw_temp, (unsigned long)g_raw_vref,
               (unsigned long)g_raw_vbat);
        HAL_Delay(1000);
    }

    return 0;
}
