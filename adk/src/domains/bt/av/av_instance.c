/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      AV instance management

Main AV task.
*/

/* Only compile if AV defined */
#ifdef INCLUDE_AV

#include "av.h"
#include "av_instance.h"

#include <device_properties.h>
#include <device_list.h>
#include <panic.h>
#include <stdlib.h>

static void avInstance_AddDeviceAvInstanceToIterator(device_t device, void* iterator_data)
{
    avInstanceTaskData* av_instance = AvInstance_GetInstanceForDevice(device);
    
    if(av_instance)
    {
        av_instance_iterator_t* iterator = (av_instance_iterator_t*)iterator_data;
        iterator->instances[iterator->index] = av_instance;
        iterator->index++;
    }
}

avInstanceTaskData* AvInstance_GetFirst(av_instance_iterator_t* iterator)
{
    memset(iterator, 0, sizeof(av_instance_iterator_t));
    
    DeviceList_Iterate(avInstance_AddDeviceAvInstanceToIterator, iterator);
    
    iterator->index = 0;
    
    return iterator->instances[iterator->index];
}

avInstanceTaskData* AvInstance_GetNext(av_instance_iterator_t* iterator)
{
    iterator->index++;
    
    if(iterator->index >= AV_MAX_NUM_INSTANCES)
        return NULL;
    
    return iterator->instances[iterator->index];
}

avInstanceTaskData* AvInstance_GetInstanceForDevice(device_t device)
{
    avInstanceTaskData** pointer_to_av_instance;
    size_t size_pointer_to_av_instance;
    
    if(device && Device_GetProperty(device, device_property_av_instance, (void**)&pointer_to_av_instance, &size_pointer_to_av_instance))
    {
        PanicFalse(size_pointer_to_av_instance == sizeof(avInstanceTaskData*));
        return *pointer_to_av_instance;
    }
    return NULL;
}

void AvInstance_SetInstanceForDevice(device_t device, avInstanceTaskData* av_instance)
{
    PanicFalse(Device_SetProperty(device, device_property_av_instance, &av_instance, sizeof(avInstanceTaskData*)));
}

device_t Av_GetDeviceForInstance(avInstanceTaskData* av_instance)
{
    return DeviceList_GetFirstDeviceWithPropertyValue(device_property_av_instance, &av_instance, sizeof(avInstanceTaskData*));
}

avInstanceTaskData *AvInstance_FindFromFocusHandset(void)
{
    device_t* devices = NULL;
    unsigned num_devices = 0;
    unsigned index;
    deviceType type = DEVICE_TYPE_HANDSET;
    avInstanceTaskData* av_instance = NULL;
    
    DeviceList_GetAllDevicesWithPropertyValue(device_property_type, &type, sizeof(deviceType), &devices, &num_devices);
    
    for(index = 0; index < num_devices; index++)
    {
        av_instance = AvInstance_GetInstanceForDevice(devices[index]);
        if(av_instance)
        {
            break;
        }
    }
    
    free(devices);
    return av_instance;
}

audio_source_t Av_GetSourceForInstance(avInstanceTaskData* instance)
{
    device_t device = Av_FindDeviceFromInstance(instance);
    if(device)
    {
        audio_source_t* ptr_to_source = NULL;
        size_t size;
        if (Device_GetProperty(device, device_property_audio_source, (void *)&ptr_to_source, &size))
        {
            PanicFalse(size == sizeof(*ptr_to_source));
            return *ptr_to_source;
        }
    }
    return audio_source_none;
}


device_t handset_device_for_specified_audio_source = NULL;

static void AvInstance_SearchForHandsetWithAudioSource(device_t device, void * data)
{
    audio_source_t * ptr_to_source = NULL;
    size_t size;

    if (Device_GetProperty(device, device_property_audio_source, (void *)&ptr_to_source, &size))
    {
        if ((*ptr_to_source == *((audio_source_t *) data)) 
            && (BtDevice_GetDeviceType(device) == DEVICE_TYPE_HANDSET)
            && (size == sizeof(*ptr_to_source)))
        {
            handset_device_for_specified_audio_source = device;
        }
    }
}

avInstanceTaskData* Av_GetInstanceForHandsetSource(audio_source_t source)
{
    avInstanceTaskData* av_instance = NULL;
    handset_device_for_specified_audio_source = NULL;

    DeviceList_Iterate(AvInstance_SearchForHandsetWithAudioSource, &source);

    if(handset_device_for_specified_audio_source != NULL)
    {
        av_instance = Av_InstanceFindFromDevice(handset_device_for_specified_audio_source);
    }

    return av_instance;
}


avInstanceTaskData * sink_instance_for_specified_audio_source = NULL;

static void avInstance_SearchForSinkInstanceWithAudioSource(device_t device, void * data)
{
    audio_source_t * ptr_to_source = NULL;
    size_t size;

    if (Device_GetProperty(device, device_property_audio_source, (void *)&ptr_to_source, &size))
    {
        if ((*ptr_to_source == *((audio_source_t *) data))
            && (size == sizeof(*ptr_to_source)))
        {
            avInstanceTaskData * av_instance = Av_InstanceFindFromDevice(device);
            if (av_instance && appA2dpIsSinkCodec(av_instance))
            {
                sink_instance_for_specified_audio_source = av_instance;
            }
        }
    }
}

avInstanceTaskData* AvInstance_GetSinkInstanceForAudioSource(audio_source_t source)
{
    sink_instance_for_specified_audio_source = NULL;

    DeviceList_Iterate(avInstance_SearchForSinkInstanceWithAudioSource, &source);

    return sink_instance_for_specified_audio_source;
}

#endif /* INCLUDE_AV */
