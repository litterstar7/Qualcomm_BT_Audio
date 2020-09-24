/*!
\copyright  Copyright (c) 2019-2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       
\brief      Support for the Cypress Touchpad PSoc 4000S
*/
#ifdef INCLUDE_CAPSENSE
#ifdef HAVE_TOUCHPAD_PSOC4000S
#include <bitserial_api.h>
#include <panic.h>
#include <pio.h>
#include <pio_monitor.h>
#include <pio_common.h>
#include <stdlib.h>
#include <logging.h>

#include "adk_log.h"
#include "touchpad_psoc4000s.h"
#include "touch.h"
#include "touch_config.h"
#include "proximity.h"

/* Make the type used for message IDs available in debug tools */
LOGGING_PRESERVE_MESSAGE_TYPE(touch_sensor_messages)

const struct __touch_config_t touch_config = {
    .i2c_clock_khz = 100,
    .pios = {
        /* The touch PIO definitions are defined in the platform x2p file */
        .xres = RDP_PIO_XRES,
        .i2c_scl = RDP_PIO_I2C_SCL,
        .i2c_sda = RDP_PIO_I2C_SDA,
        .interrupt = RDP_PIO_INT_TOUCH,
        .ldo1v8 = RDP_PIO_LDO1V8
    },
};
/* This table converts touch specific data to generic touch events */
const touch_data_to_action_t touch_action_map[] =
{
#ifdef TOUCH_SENSOR_V2
    {
        SLIDE_UP,
        TOUCH_DATA_SLIDER_UP
    },
    {
        SLIDE_DOWN,
        TOUCH_DATA_SLIDER_DOWN
    },
    {
        HAND_COVER,
        TOUCH_DATA_PRESS
    },
    {
        HAND_COVER_RELEASE,
        TOUCH_DATA_RELEASE
    },

#else
    {
        SINGLE_PRESS,
        TOUCH_DATA_SINGLE_PRESS
    },
    {
        DOUBLE_PRESS,
        TOUCH_DATA_DOUBLE_PRESS
    },    
    {
        DOUBLE_PRESS_HOLD,
        TOUCH_DATA_DOUBLE_PRESS_HOLD
    },
    {
        TRIPLE_PRESS,
        TOUCH_DATA_TRIPLE_PRESS
    },    
    {
        TRIPLE_PRESS_HOLD,
        TOUCH_DATA_TRIPLE_PRESS_HOLD
    },
    {
        FOUR_PRESS,
        TOUCH_DATA_FOUR_PRESS
    },    
    {
        FOUR_PRESS_HOLD,
        TOUCH_DATA_FOUR_PRESS_HOLD
    },
    {
        FIVE_PRESS,
        TOUCH_DATA_FIVE_PRESS
    },
    {
        FIVE_PRESS_HOLD,
        TOUCH_DATA_FIVE_PRESS_HOLD
    },
    {
        SIX_PRESS,
        TOUCH_DATA_SIX_PRESS
    },
    {
        SEVEN_PRESS,
        TOUCH_DATA_SEVEN_PRESS
    },
    {
        EIGHT_PRESS,
        TOUCH_DATA_EIGHT_PRESS
    },    
    {
        NINE_PRESS,
        TOUCH_DATA_NINE_PRESS
    },
    {
        LONG_PRESS,
        TOUCH_DATA_LONG_PRESS
    },
    {
        VERY_LONG_PRESS,
        TOUCH_DATA_VERY_LONG_PRESS
    },
    {
        VERY_VERY_LONG_PRESS,
        TOUCH_DATA_VERY_VERY_LONG_PRESS
    },
    {
        VERY_VERY_VERY_LONG_PRESS,
        TOUCH_DATA_VERY_VERY_VERY_LONG_PRESS
    },
    {
        SLIDE_UP,
        TOUCH_DATA_SLIDER_UP
    },
    {
        SLIDE_DOWN,
        TOUCH_DATA_SLIDER_DOWN
    },
    {
        SLIDE_LEFT,
        TOUCH_DATA_SLIDER_LEFT
    },
    {
        SLIDE_RIGHT,
        TOUCH_DATA_SLIDER_RIGHT
    },
    {
        HAND_COVER,
        TOUCH_DATA_HANDCOVER
    },
    {
        HAND_COVER_RELEASE,
        TOUCH_DATA_HANDCOVER_RELEASE
    },
#endif
};

uint8 touchData [5];
/*!< Task information for touch pad */
touchTaskData app_touch;
/*! \brief Read touch event */
static bool touchPsoc4000s_ReadEvent(uint8 *value)
{
    bitserial_result result = BITSERIAL_RESULT_INVAL;
    bitserial_handle handle;

    bitserial_config bsconfig;
    touchTaskData *touch = &app_touch;

    /* Configure Bitserial to work with txcpa224 proximity sensor */
    memset(&bsconfig, 0, sizeof(bsconfig));
    bsconfig.mode = BITSERIAL_MODE_I2C_MASTER;
    bsconfig.clock_frequency_khz = touch->config->i2c_clock_khz;
    bsconfig.u.i2c_cfg.i2c_address = TOUCHPAD_I2C_ADDRESS;
    handle = BitserialOpen((bitserial_block_index)BITSERIAL_BLOCK_1, &bsconfig);

    /* First write the register address to be read */
    result = BitserialTransfer(handle,
                            NULL,
                            NULL,
                            0,
                            value,
                            5);
    BitserialClose(handle);
    return (result == BITSERIAL_RESULT_SUCCESS);
}

/*! \brief Send action to anyone registered 

    Check for registrants first to save message allocation if not needed.
*/
static void touchPsoc4000s_SendActionMessage(touch_action_t action)
{
    if (TaskList_Size(TaskList_GetFlexibleBaseTaskList(TouchSensor_GetActionClientTasks())))
    {
        MESSAGE_MAKE(message, TOUCH_SENSOR_ACTION_T);

        message->action = action;
        TaskList_MessageSend(TaskList_GetFlexibleBaseTaskList(TouchSensor_GetActionClientTasks()), 
                             TOUCH_SENSOR_ACTION, message);
    }
}


static bool touchPsoc4000s_MapAndSendEvents(touch_action_t action, bool send_raw_regardless)
{
    touchTaskData *touch = &app_touch;
    unsigned i;

    /* try to match input action with UI message to be broadcasted*/
    for (i=0; i < touch->action_table_size; i++)
    {
        if (action == touch->action_table[i].action)
        {
            MessageId id = touch->action_table[i].message;

            DEBUG_LOG_VERBOSE("touchPsoc4000s_MapAndSendEvents action enum:touch_action_t:%d message: %x", action, id);

            touchPsoc4000s_SendActionMessage(action);
            TaskList_MessageSendId(TaskList_GetFlexibleBaseTaskList(TouchSensor_GetUiClientTasks()), id);
            return TRUE;
        }
    }

    if (send_raw_regardless)
    {
        touchPsoc4000s_SendActionMessage(action);
    }
    return FALSE;
}

static void touchPsoc4000s_MapTouchToLogicalInput(uint8 data)
{
    uint8 actionTableSize = ARRAY_DIM(touch_action_map);
    uint8 i = 0;

    for (i=0; i < actionTableSize; i++)
    {
        if (touch_action_map[i].touch_data == data)
        {
            touch_action_t touch_ui_input = touch_action_map[i].action;

            if (touch_ui_input != MAX_ACTION)
            {
                touchPsoc4000s_MapAndSendEvents(touch_ui_input, TRUE);
            }
            return;
        }
    }
}

#ifdef TOUCH_SENSOR_V2
static void touchPsoc4000s_Reset(touchTaskData *touch)
{

    /* disable interrupt */
    uint16 bank = PioCommonPioBank(touch->config->pios.interrupt);
    uint32 mask = PioCommonPioMask(touch->config->pios.interrupt);
    PanicNotZero(PioSetMapPins32Bank(bank, mask, mask));
    PanicNotZero(PioSetDir32Bank(bank, mask, mask));
    PanicNotZero(PioSet32Bank(bank, mask, mask));

    /* Pull low and high to reset */
    bank = PioCommonPioBank(touch->config->pios.xres);
    mask = PioCommonPioMask(touch->config->pios.xres);

    PanicNotZero(PioSet32Bank(bank, mask, 0));
    PanicNotZero(PioSet32Bank(bank, mask, mask));

    /* re-enable interrupt */
    bank = PioCommonPioBank(touch->config->pios.interrupt);
    mask = PioCommonPioMask(touch->config->pios.interrupt);
    PanicNotZero(PioSetMapPins32Bank(bank, mask, mask));
    PanicNotZero(PioSetDir32Bank(bank, mask, 0));
    PanicNotZero(PioSet32Bank(bank, mask, mask));


}

static void touchPsoc4000s_MapTouchTimerToLogicalInput(MessageId id, uint8 press, uint8 data)
{
    touch_action_t touch_ui_input = MAX_ACTION;
    bool found = FALSE;

    /* convert timer count to touch event map*/
    if (id == TOUCH_INTERNAL_HELD_TIMER)
    {
        /* press hold handling */
        switch (press)
        {
            case SINGLE_PRESS:
                touch_ui_input = data + TOUCH_PRESS_HOLD_OFFSET;
                break;
            case DOUBLE_PRESS:
                touch_ui_input = data + TOUCH_DOUBLE_PRESS_HOLD_OFFSET;
                break;
            /* only handle up to double press hold for now */
            default:
                return;
        }

    }
    else if (id == TOUCH_INTERNAL_HELD_RELEASE)
    {
        /* hold release handling */
        switch (press)
        {
            case SINGLE_PRESS:
                touch_ui_input = (touch_action_t) data + TOUCH_PRESS_RELEASE_OFFSET;
                break;
            case DOUBLE_PRESS:
                touch_ui_input = (touch_action_t) data + TOUCH_DOUBLE_PRESS_HOLD_RELEASE_OFFSET;
                break;
            /* hold release only handle up to double press for now */
            default:
                return;
        }
    }
    else if (id == TOUCH_INTERNAL_CLICK_TIMER)
    {
        /* quick press handling */
        if (press >= MAX_PRESS_SUPPORT)
        {
            return;
        }
        /* cast up the type */
        touch_ui_input = (touch_action_t) press;
    }

    if (touch_ui_input != MAX_ACTION)
    {
        /* try to match input action with UI message to be broadcasted*/
        found = touchPsoc4000s_MapAndSendEvents(touch_ui_input, TRUE);

        /* for release event, find the closest release UI event, if not found the exact release timer UI event  */
        if (id == TOUCH_INTERNAL_HELD_RELEASE)
        {
            while (touch_ui_input && touch_ui_input > TOUCH_PRESS_RELEASE_OFFSET && !found)
            {
                touch_ui_input --;

                /* This is a generated action. Only send the action if we send a UI event */
                found = touchPsoc4000s_MapAndSendEvents(touch_ui_input, FALSE);
            }
        }
    }
}
#endif

/*! \brief Decode touch data */
static void touchPsoc4000s_HandleTouchData(uint8 *data)
{
    /* verify valid data received*/
    if (    (data[0] != TOUCH_DATA_FIRST_BYTE) || (data[1] != TOUCH_DATA_SECOND_BYTE)
         || (data[2] != TOUCH_DATA_THIRD_BYTE) || (data[3] != TOUCH_DATA_FOURTH_BYTE))
    {
        DEBUG_LOG_WARN("touchPsoc4000s_HandleTouchData Wrong Event Data received");
        return;
    }
    DEBUG_LOG_VERBOSE("Touch Event %02x", data[4]);
#ifdef TOUCH_SENSOR_V2
    touchTaskData *touch = &app_touch;
    static uint8 last_touch_data = TOUCH_DATA_UNDEFINED;
    /* 2nd version touch sensor only have following events
       PRESS
       RELEASE
       SLIDE UP
       SLIDE DOWN
       if a press < 300ms it'll be ignored
    */

    /* cancel the held timer if receiving any touch event*/
    MessageCancelAll(&touch->task, TOUCH_INTERNAL_HELD_CANCEL_TIMER);
    MessageCancelAll(&touch->task, TOUCH_INTERNAL_HELD_TIMER);
    MessageCancelAll(&touch->task, TOUCH_INTERNAL_CLICK_TIMER);

    switch (data[4])
    {
        case TOUCH_DATA_PRESS:
            touchPsoc4000s_MapTouchToLogicalInput(data[4]);
            MessageSendLater(&touch->task, TOUCH_INTERNAL_HELD_CANCEL_TIMER, NULL, touchConfigPressCancelMs());
            break;
        case TOUCH_DATA_SLIDER_UP:
        case TOUCH_DATA_SLIDER_DOWN:
            touchPsoc4000s_MapTouchToLogicalInput(data[4]);
            break;
        case TOUCH_DATA_RELEASE:
            /* only handle release event if we are waiting for one */
            if (last_touch_data == TOUCH_DATA_PRESS)
            {
                touchPsoc4000s_MapTouchToLogicalInput(data[4]);
                MessageSend(&touch->task, TOUCH_INTERNAL_HELD_RELEASE, NULL);
            }
            break;
        default:
            break;
    }
    /* cache the last touch data to avoid sending release event out after slide up/down */
    last_touch_data = data[4];
#else
    /* Map touch event to predefined ui events */
    touchPsoc4000s_MapTouchToLogicalInput(data[4]);

#endif
}

/*! \brief Handle the touch interrupt */
static void touchPsoc4000s_MessageHandler(Task task, MessageId id, Message msg)
{
    touchTaskData *touch = (touchTaskData *) task;

    switch(id)
    {
        case MESSAGE_PIO_CHANGED:
            {
                const MessagePioChanged *mpc = (const MessagePioChanged *)msg;
                bool pio_set;

                if (PioMonitorIsPioInMessage(mpc, touch->config->pios.interrupt, &pio_set))
                {
                    if (!pio_set)
                    {
                        memset(&touchData[0], 0, sizeof(touchData));
                        PanicFalse(touchPsoc4000s_ReadEvent(touchData));
                        touchPsoc4000s_HandleTouchData(touchData);
                    }
                }
            }
            break;

#ifdef TOUCH_SENSOR_V2
        case TOUCH_INTERNAL_HELD_CANCEL_TIMER:
            /* This time out is to prevent accidental very quick touch, if this timer expired then a touch is counted */
            touch->number_of_seconds_held = 0;
            MessageSendLater(&touch->task, TOUCH_INTERNAL_HELD_TIMER, NULL, D_SEC(1) - touchConfigPressCancelMs());
            break;
        case TOUCH_INTERNAL_HELD_TIMER:
            /* send notification if we have button held subscription before increasing the counter*/
            touch->number_of_seconds_held++;
            DEBUG_LOG_VERBOSE("Touch %u held %u seconds", touch->number_of_press, touch->number_of_seconds_held); 
            touchPsoc4000s_MapTouchTimerToLogicalInput(id, touch->number_of_press + 1, touch->number_of_seconds_held);
            /* to recover from a loss of release event */
            if (touch->number_of_seconds_held <= touchConfigMaximumHeldTimeSeconds())
            {
                MessageSendLater(&touch->task, TOUCH_INTERNAL_HELD_TIMER, NULL, D_SEC(1));
            }
            else
            {
                touch->number_of_seconds_held = 0;
                touchPsoc4000s_Reset(touch);
            }
            break;
        case TOUCH_INTERNAL_HELD_RELEASE:
            /* send notification if we have held release subscription then reset the counter*/
            MessageSendLater(&touch->task, TOUCH_INTERNAL_CLICK_TIMER, NULL, touchConfigClickTimeoutlMs());
            if (touch->number_of_seconds_held > 0)
            {
                /* long press release */
                DEBUG_LOG_VERBOSE("Touch %u held release %u seconds", touch->number_of_press, touch->number_of_seconds_held);
                touchPsoc4000s_MapTouchTimerToLogicalInput(id, touch->number_of_press + 1, touch->number_of_seconds_held);
                touch->number_of_press = 0;
            }
            else
            {
                /* quick press release*/
                touch->number_of_press++;
                DEBUG_LOG_VERBOSE("Quick press %u", touch->number_of_press);
            }
            touch->number_of_seconds_held = 0;
            break;
        case TOUCH_INTERNAL_CLICK_TIMER:
            /* if this is expired, meaning the quick click hasn't been cancelled, can send multi click event*/
            if (touch->number_of_press)
            {
                DEBUG_LOG_VERBOSE("Quick press release %u", touch->number_of_press);
                touchPsoc4000s_MapTouchTimerToLogicalInput(TOUCH_INTERNAL_CLICK_TIMER, touch->number_of_press, 0);
                touch->number_of_press = 0;
            }
            break;
#endif
        default:
            break;
    }
}

/*! \brief Enable the proximity sensor */
static void touchPsoc4000s_Enable(const touchConfig *config)
{
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

    DEBUG_LOG_VERBOSE("touchPsoc4000sEnable");

    /* Setup Interrupt as input with weak pull up */
    bank = PioCommonPioBank(config->pios.interrupt);
    mask = PioCommonPioMask(config->pios.interrupt);
    PanicNotZero(PioSetMapPins32Bank(bank, mask, mask));
    PanicNotZero(PioSetDir32Bank(bank, mask, 0));
    PanicNotZero(PioSet32Bank(bank, mask, mask));
    /* Power On */
    bank = PioCommonPioBank(config->pios.ldo1v8);
    mask = PioCommonPioMask(config->pios.ldo1v8);
    PanicNotZero(PioSetMapPins32Bank(bank, mask, mask));
    PanicNotZero(PioSetDir32Bank(bank, mask, mask));
    PanicNotZero(PioSet32Bank(bank, mask, mask));

    bank = PioCommonPioBank(config->pios.xres);
    mask = PioCommonPioMask(config->pios.xres);
    PanicNotZero(PioSetMapPins32Bank(bank, mask, mask));
    PanicNotZero(PioSetDir32Bank(bank, mask, mask));
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

}

/*! \brief Disable the proximity sensor */
static void touchPsoc4000s_Disable(const touchConfig *config)
{
    uint16 bank;
    uint32 mask;
    DEBUG_LOG_VERBOSE("touchPsoc4000sDisable");

    /* Disable interrupt and set weak pull down */
    bank = PioCommonPioBank(config->pios.interrupt);
    mask = PioCommonPioMask(config->pios.interrupt);
    PanicNotZero(PioSet32Bank(bank, mask, 0));
}

static void touchPsoc4000s_StartIfNeeded(void)
{
    touchTaskData *touch = &app_touch;

    if (!touch->config)
    {
        const touchConfig *config = &touch_config;

        touch->config = config;
        touchPsoc4000s_Enable(config);
        
        /* Register for interrupt events */
        touch->task.handler = touchPsoc4000s_MessageHandler;
        PioMonitorRegisterTask(&touch->task, config->pios.interrupt);
        touch->number_of_press = 0;
        touch->number_of_seconds_held = 0;
    }
}

static void touchPsoc4000s_StopIfNeeded(void)
{
    touchTaskData *touch = &app_touch;

    if (touch->config)
    {
        uint16 ui_clients = TaskList_Size(TaskList_GetFlexibleBaseTaskList(TouchSensor_GetUiClientTasks()));
        uint16 action_clients = TaskList_Size(TaskList_GetFlexibleBaseTaskList(TouchSensor_GetActionClientTasks()));

        if (!ui_clients && !action_clients)
        {
            PioMonitorUnregisterTask(&touch->task, touch->config->pios.interrupt);
            touchPsoc4000s_Disable(touch->config);
            touch->config = NULL;

            if (!ui_clients)
            {
                touch->action_table = NULL;
                touch->action_table_size = 0;
            }
        }
    }
}


bool TouchSensorClientRegister(Task task, uint32 size_action_table, const touch_event_config_t *action_table)
{
    touchTaskData *touch = &app_touch;

    touchPsoc4000s_StartIfNeeded();

    /* update action table*/
    if (size_action_table && action_table != NULL)
    {
        touch->action_table = action_table;
        touch->action_table_size = size_action_table;
    }
    return TaskList_AddTask(TaskList_GetFlexibleBaseTaskList(TouchSensor_GetUiClientTasks()), task);
}

void TouchSensorClientUnRegister(Task task)
{
    TaskList_RemoveTask(TaskList_GetFlexibleBaseTaskList(TouchSensor_GetUiClientTasks()), task);

    touchPsoc4000s_StopIfNeeded();
}

bool TouchSensorActionClientRegister(Task task)
{
    touchPsoc4000s_StartIfNeeded();

    return TaskList_AddTask(TaskList_GetFlexibleBaseTaskList(TouchSensor_GetActionClientTasks()), task);
}

void TouchSensorActionClientUnRegister(Task task)
{
    TaskList_RemoveTask(TaskList_GetFlexibleBaseTaskList(TouchSensor_GetActionClientTasks()), task);

    touchPsoc4000s_StopIfNeeded();
}

bool TouchSensor_Init(Task init_task)
{
    UNUSED(init_task);

    TaskList_InitialiseWithCapacity(TouchSensor_GetUiClientTasks(), 
                                    TOUCH_CLIENTS_INITIAL_CAPACITY);
    TaskList_InitialiseWithCapacity(TouchSensor_GetActionClientTasks(), 
                                    TOUCH_CLIENTS_INITIAL_CAPACITY);

    return TRUE;
}

bool AppTouchSensorGetDormantConfigureKeyValue(dormant_config_key *key, uint32* value)
{
    *key = PIO_WAKE_MASK;
    *value = 1 << touch_config.pios.interrupt;
    return TRUE;
}

#endif /* HAVE_TOUCHPAD_PSOC4000S */
#endif /* INCLUDE_CAPSENSE*/
