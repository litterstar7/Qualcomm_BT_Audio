/*!
\copyright  Copyright (c) 2017 - 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       vncl3020.c
\brief      Support for the vncl3020 proximity sensor
*/

#ifdef INCLUDE_PROXIMITY
#ifdef HAVE_VNCL3020
#include <bitserial_api.h>
#include <panic.h>
#include <pio.h>
#include <pio_monitor.h>
#include <pio_common.h>
#include <stdlib.h>

#include "earbud_led.h"

#include "adk_log.h"
#include "../../domains/peripheral/led_manager/led_manager.h"
#include "proximity.h"
#include "proximity_config.h"
#include "vncl3020.h"

/* Make the type used for message IDs available in debug tools */
LOGGING_PRESERVE_MESSAGE_ENUM(proximity_messages)

/*! \brief IR LED current = Value (dec.) x 10 mA. In this case -30mA
*/
#define PROXIMITY_IR_LED_CURRENT_VALUE    (3)

/*! \brief The threshold low and high should be changed based on the enclosure for the earbud.
*/
const struct __proximity_config proximity_config = {
    .threshold_low = 9500,
    .threshold_high = 11000,
    .threshold_counts = vncl3020_threshold_count_4,
    .rate = vncl3020_proximity_rate_7p8125_per_second,
    .i2c_clock_khz = 100,
    .pios = {
        /* The PROXIMITY_PIO definitions are defined in the platform x2p file */
        .on = PROXIMITY_PIO_ON,
        .i2c_scl = PROXIMITY_PIO_I2C_SCL,
        .i2c_sda = PROXIMITY_PIO_I2C_SDA,
        .interrupt = PROXIMITY_PIO_INT,
    },
};

/*!< Task information for proximity sensor */
proximityTaskData app_proximity;

/*! \brief Read a register from the proximity sensor */
static bool vncl3020ReadRegister(bitserial_handle handle, uint8 reg,  uint8 *value)
{
    bitserial_result result;
    /* First write the register address to be read */
    result = BitserialWrite(handle,
                            BITSERIAL_NO_MSG,
                            &reg, 1,
                            BITSERIAL_FLAG_BLOCK);
    if (result == BITSERIAL_RESULT_SUCCESS)
    {
        /* Now read the actual data in the register */
        result = BitserialRead(handle,
                                BITSERIAL_NO_MSG,
                                value, 1,
                                BITSERIAL_FLAG_BLOCK);
    }
    return (result == BITSERIAL_RESULT_SUCCESS);
}

/*! \brief Write to a proximity sensor register */
static bool vncl3020WriteRegister(bitserial_handle handle, uint8 reg, uint8 value)
{
    bitserial_result result;
    uint8 command[2] = {reg, value};

    /* Write the write command and register */
    result = BitserialWrite(handle,
                            BITSERIAL_NO_MSG,
                            command, 2,
                            BITSERIAL_FLAG_BLOCK);
    return (result == BITSERIAL_RESULT_SUCCESS);
}

/*! \brief Read the proximity sensor version */
static bool vncl3020ReadVersion(bitserial_handle handle, vncl3020_revision_register_t *reg)
{
    reg->reg = 0;
    return vncl3020ReadRegister(handle, VNCL3020_PRODCUT_ID_REVISION_REG, &reg->reg);
}

static bool vncl3020ReadInterruptStatus(bitserial_handle handle, vncl3020_interrupt_status_register_t *reg)
{
    reg->reg = 0;
    return vncl3020ReadRegister(handle, VNCL3020_INTERRUPT_STATUS_REG, &reg->reg);
}

/*! \brief Set the proximity sensor measurement rate */
static bool vncl3020SetRate(bitserial_handle handle, enum vncl3020_proximity_rates rate)
{
    vncl3020_proximity_rate_register_t reg = {0};
    reg.bits.rate = rate;
    return vncl3020WriteRegister(handle, VNCL3020_PROXIMITY_RATE_REG, reg.reg);
}

/*! \brief Stop any proximity readings */
static bool vncl3020StopReading(bitserial_handle handle)
{
    vncl3020_command_register_t reg = {0};
    return vncl3020WriteRegister(handle, VNCL3020_COMMAND_REG, reg.reg);
}

/*! \brief Set the proximity sensor to perform measurements periodically  */
static bool vncl3020SetPeriodic(bitserial_handle handle)
{
    vncl3020_command_register_t reg = {0};
    reg.bits.selftimed_en = 1;
    reg.bits.prox_en = 1;
    return vncl3020WriteRegister(handle, VNCL3020_COMMAND_REG, reg.reg);
}

/*! \brief Enable interrupt on high/low threshold */
static bool vncl3020EnableInterruptOnThreshold(bitserial_handle handle,
                                               enum vncl_threshold_counts counts)
{
    vncl3020_interrupt_control_register_t reg = {0};
    reg.bits.threshold_en = 1;
    reg.bits.count_exceeded = counts;
    return vncl3020WriteRegister(handle, VNCL3020_INTERRUPT_CONTORL_REG, reg.reg);
}

/*! \brief Disable all interrupts */
static bool vncl3020DisableInterrupts(bitserial_handle handle)
{
    vncl3020_interrupt_status_register_t reg = {0};
    return vncl3020WriteRegister(handle, VNCL3020_INTERRUPT_CONTORL_REG, reg.reg);
}

/*! \brief Clear all interrupt flags */
static bool vncl3020ClearInterruptFlags(bitserial_handle handle)
{
    vncl3020_interrupt_status_register_t reg = {0};
    reg.bits.prox_ready = 1;
    reg.bits.threshold_high = 1;
    reg.bits.threshold_low = 1;
    return vncl3020WriteRegister(handle, VNCL3020_INTERRUPT_STATUS_REG, reg.reg);
}

static bool vncl3020SetHighThreshold(bitserial_handle handle, uint16 threshold)
{
    return vncl3020WriteRegister(handle, VNCL3020_HIGH_THRESHOLD_HI_REG, threshold >> 8) &&
           vncl3020WriteRegister(handle, VNCL3020_HIGH_THRESHOLD_LO_REG, threshold & 0xff);
}

static bool vncl3020SetLowThreshold(bitserial_handle handle, uint16 threshold)
{
    return vncl3020WriteRegister(handle, VNCL3020_LOW_THRESHOLD_HI_REG, threshold >> 8) &&
           vncl3020WriteRegister(handle, VNCL3020_LOW_THRESHOLD_LO_REG, threshold & 0xff);
}

/*! \brief Enable the interrupts for thresholds of proximity sensor */
static void appProximityInterruptsEnable(void)
{
    DEBUG_LOG_VERBOSE("vncl3020:appProximityRegisterInterrupts");
    proximityTaskData *prox = ProximityGetTaskData();
    const proximityConfig *config = appConfigProximity();
    PanicFalse(vncl3020EnableInterruptOnThreshold(prox->handle, config->threshold_counts));
}

/*! \brief Read a proximity result.
   FIXME doesn't handle the result changing mid-read, but if the proximity
   is read after an interrupt informs a new result is ready it should be ok */
static bool vncl3020ReadProximityResult(bitserial_handle handle, uint16 *proximity)
{
    uint8 value;
    bool result = FALSE;
    result = vncl3020ReadRegister(handle, VNCL3020_PROXIMITY_RESULT_HI_REG, &value);
    if (result)
    {
        *proximity = value << 8;
        result = vncl3020ReadRegister(handle, VNCL3020_PROXIMITY_RESULT_LO_REG, &value);
        if (result)
        {
            *proximity |= value;
        }
    }
    return result;
}

/*! \brief Handle the proximity interrupt */
static void vncl3020InterruptHandler(Task task, MessageId id, Message msg)
{
    proximityTaskData *proximity = (proximityTaskData *) task;

    switch(id)
    {
        case MESSAGE_PIO_CHANGED:
        {
            const MessagePioChanged *mpc = (const MessagePioChanged *)msg;
            const proximityConfig *config = proximity->config;
            bool pio_set;

            if (PioMonitorIsPioInMessage(mpc, config->pios.interrupt, &pio_set))
            {
                if (!pio_set)
                {
                    vncl3020_interrupt_status_register_t isr;

                    PanicFalse(vncl3020ReadInterruptStatus(proximity->handle, &isr));
                    if (isr.bits.threshold_high)
                    {
                        DEBUG_LOG_VERBOSE("vncl3020InterruptHandler in proximity");

                        proximity->state->proximity = proximity_state_in_proximity;
                        /* Set high threshold to max to avoid further interrupts */
                        PanicFalse(vncl3020SetHighThreshold(proximity->handle, 0xffff));
                        /* Reinstate low threshold */
                        PanicFalse(vncl3020SetLowThreshold(proximity->handle, config->threshold_low));
                        /* Inform clients */
                        TaskList_MessageSendId(proximity->clients, PROXIMITY_MESSAGE_IN_PROXIMITY);
                    }
                    else if (isr.bits.threshold_low)
                    {
                        DEBUG_LOG_VERBOSE("vncl3020InterruptHandler not in proximity");

                        proximity->state->proximity = proximity_state_not_in_proximity;
                        /* Set low threshold to min to avoid further interrupts */
                        PanicFalse(vncl3020SetLowThreshold(proximity->handle, 0));
                        /* Reinstate high threshold */
                        PanicFalse(vncl3020SetHighThreshold(proximity->handle, config->threshold_high));
                        /* Inform clients */
                        TaskList_MessageSendId(proximity->clients, PROXIMITY_MESSAGE_NOT_IN_PROXIMITY);
                    }
                    PanicFalse(vncl3020ClearInterruptFlags(proximity->handle));
                }
            }
        }
        break; 

        case PIO_MONITOR_ENABLE_CFM:
        {
            DEBUG_LOG_VERBOSE("Received event: PIO_MONITOR_ENABLE_CFM");
            appProximityInterruptsEnable();
        }
        break;

        default:
        break;
    }
}

/*! \brief Enable the proximity sensor */
static bitserial_handle vncl3020Enable(const proximityConfig *config)
{
    bitserial_config bsconfig;
    uint32 i;
    uint16 bank;
    uint32 mask;
    struct
    {
        uint16 pio;
        pin_function_id func;
    } i2c_pios[] = {{config->pios.i2c_scl, BITSERIAL_1_CLOCK_OUT},
                    {config->pios.i2c_scl, BITSERIAL_1_CLOCK_IN},
                    {config->pios.i2c_sda, BITSERIAL_1_DATA_OUT},
                    {config->pios.i2c_sda, BITSERIAL_1_DATA_IN}};

    DEBUG_LOG_INFO("vncl3020Enable");

    if (config->pios.on != VNCL3020_ON_PIO_UNUSED)
    {
        /* Setup power PIO then power-on the sensor */
        bank = PioCommonPioBank(config->pios.on);
        mask = PioCommonPioMask(config->pios.on);
        PanicNotZero(PioSetMapPins32Bank(bank, mask, mask));
        PanicNotZero(PioSetDir32Bank(bank, mask, mask));
        PanicNotZero(PioSet32Bank(bank, mask, mask));
    }

    /* Setup Interrupt as input with weak pull up */
    bank = PioCommonPioBank(config->pios.interrupt);
    mask = PioCommonPioMask(config->pios.interrupt);
    PanicNotZero(PioSetMapPins32Bank(bank, mask, mask));
    PanicNotZero(PioSetDir32Bank(bank, mask, 0));
    PanicNotZero(PioSet32Bank(bank, mask, mask));

    for (i = 0; i < ARRAY_DIM(i2c_pios); i++)
    {
        uint16 pio = i2c_pios[i].pio;
        bank = PioCommonPioBank(pio);
        mask = PioCommonPioMask(pio);

        /* Setup I2C PIOs with strong pull-up */
        PanicNotZero(PioSetMapPins32Bank(bank, mask, 0));
        PanicFalse(PioSetFunction(pio, i2c_pios[i].func));
        PanicNotZero(PioSetDir32Bank(bank, mask, 0));
        PanicNotZero(PioSet32Bank(bank, mask, mask));
        PanicNotZero(PioSetStrongBias32Bank(bank, mask, mask));
    }

    /* Configure Bitserial to work with vncl3020 proximity sensor */
    memset(&bsconfig, 0, sizeof(bsconfig));
    bsconfig.mode = BITSERIAL_MODE_I2C_MASTER;
    bsconfig.clock_frequency_khz = config->i2c_clock_khz;
    bsconfig.u.i2c_cfg.i2c_address = I2C_ADDRESS;
    return BitserialOpen((bitserial_block_index)BITSERIAL_BLOCK_1, &bsconfig);
}

/*! \brief Disable the proximity sensor */
static void vncl3020Disable(bitserial_handle handle, const proximityConfig *config)
{
    uint16 bank;
    uint32 mask;
    DEBUG_LOG_INFO("vncl3020Disable");

    /* Disable interrupt and set weak pull down */
    bank = PioCommonPioBank(config->pios.interrupt);
    mask = PioCommonPioMask(config->pios.interrupt);
    PanicNotZero(PioSet32Bank(bank, mask, 0));

    /* Release bitserial instance */
    BitserialClose(handle);
    handle = BITSERIAL_HANDLE_ERROR;

    if (config->pios.on != VNCL3020_ON_PIO_UNUSED)
    {
        /* Power off the proximity sensor */
        PanicNotZero(PioSet32Bank(PioCommonPioBank(config->pios.on),
                                  PioCommonPioMask(config->pios.on),
                                  0));
    }
}

/*! \brief Reset the proximity sensor */
static void vncl3020Reset(bitserial_handle handle)
{
    uint16 result;
    PanicFalse(vncl3020StopReading(handle));
    PanicFalse(vncl3020DisableInterrupts(handle));
    PanicFalse(vncl3020ReadProximityResult(handle, &result));
    PanicFalse(vncl3020ClearInterruptFlags(handle));
}

/*! \brief Set the proximity sensor LED level */
static bool vncl3020SetLED(bitserial_handle handle, unsigned current)
{

    DEBUG_LOG_VERBOSE("vncl3020SetLED VNCL3020 LED=%d", current);
    vncl3020_ir_led_current_register_t reg = {0};
    reg.bits.current = current;

    return vncl3020WriteRegister(handle, VNCL3020_IR_LED_CURRENT_REG, reg.reg);
}

bool appProximityClientRegister(Task task)
{
    vncl3020_revision_register_t rev;
    proximityTaskData *prox = ProximityGetTaskData();
    uint16 result;

    const proximityConfig *config = appConfigProximity();

    if (NULL == prox->clients)
    {
        prox->config = config;
        prox->state = PanicUnlessNew(proximityState);
        prox->state->proximity = proximity_state_unknown;
        prox->clients = TaskList_Create();

        prox->handle = vncl3020Enable(config);
        PanicFalse(prox->handle != BITSERIAL_HANDLE_ERROR);

        PanicFalse(vncl3020ReadVersion(prox->handle, &rev));
        DEBUG_LOG_INFO("appProximityInit VNCL3020 prod_id=%d ver_id=%d",
                        rev.bits.product_id, rev.bits.revision_id);

        vncl3020Reset(prox->handle);
        /* Set the fastest measurement rate to get an initial reading as fast as possible */
        PanicFalse(vncl3020SetRate(prox->handle, vncl3020_proximity_rate_250_per_second));
        PanicFalse(vncl3020SetPeriodic(prox->handle));

        /* Set the IR LED's current level to 30mA */
        PanicFalse(vncl3020SetLED(prox->handle, PROXIMITY_IR_LED_CURRENT_VALUE));
        PanicFalse(vncl3020SetHighThreshold(prox->handle, config->threshold_high));
        PanicFalse(vncl3020SetLowThreshold(prox->handle, config->threshold_low));

        /* Set the handler and PIO for sensor interrupt events. */
        /* Sensor interrupts are not configured until #PIO_MONITOR_ENABLE_CFM */
        prox->task.handler = vncl3020InterruptHandler;
        PioMonitorRegisterTask(&prox->task, config->pios.interrupt);
    }

    /* Send initial message to client by reading from sensor. Post app init, the state is updated via interrupts */
    if(prox->state->proximity == proximity_state_unknown)
    {
        PanicFalse(vncl3020ReadProximityResult(prox->handle, &result));
        if(result >= config->threshold_high)
        {
            DEBUG_LOG_VERBOSE("vncl3020 Initial State: IN PROXIMITY");
            prox->state->proximity = proximity_state_in_proximity;
        }
        else if(result <= config->threshold_low)
        {
            DEBUG_LOG_VERBOSE("vncl3020 Initial State: NOT IN PROXIMITY");
            prox->state->proximity = proximity_state_not_in_proximity;
        }
        PanicFalse(vncl3020SetRate(prox->handle, config->rate));
    }
    switch (prox->state->proximity)
    {
        case proximity_state_in_proximity:
            MessageSend(task, PROXIMITY_MESSAGE_IN_PROXIMITY, NULL);
            break;
        case proximity_state_not_in_proximity:
            MessageSend(task, PROXIMITY_MESSAGE_NOT_IN_PROXIMITY, NULL);
            break;
        case proximity_state_unknown:
        default:
            /* The client will be informed after the first interrupt */
            break;
    }

    return TaskList_AddTask(prox->clients, task);
}

void appProximityClientUnregister(Task task)
{
    proximityTaskData *prox = ProximityGetTaskData();
    TaskList_RemoveTask(prox->clients, task);
    if (0 == TaskList_Size(prox->clients))
    {
        TaskList_Destroy(prox->clients);
        prox->clients = NULL;
        free(prox->state);
        prox->state = NULL;

        PanicFalse(prox->handle != BITSERIAL_HANDLE_ERROR);

        /* Unregister for interrupt events */
        PioMonitorUnregisterTask(&prox->task, prox->config->pios.interrupt);

        /* Reset into lowest power mode in case the sensor is not powered off. */
        vncl3020Reset(prox->handle);
        vncl3020Disable(prox->handle, prox->config);
        prox->handle = BITSERIAL_HANDLE_ERROR;
    }
}

/* This function is to switch on/off sensor for power saving*/
void appProximityEnableSensor(Task task, bool enable)
{
    UNUSED(task);
    UNUSED(enable);
}
#endif /* HAVE_VNCL3020 */
#endif /* INCLUDE_PROXIMITY */
