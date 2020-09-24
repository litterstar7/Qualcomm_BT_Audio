/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       
\brief      Single Entity Handover related interfaces

*/

/* Only compile if mirroring defined */
#ifdef INCLUDE_MIRRORING
#include "single_entity_typedef.h"
#include "domain_marshal_types.h"
#include "app_handover_if.h"
#include "adk_log.h"
#include <panic.h>
#include <stdlib.h>

#include <device_properties.h>
#include <device_list.h>

/******************************************************************************
 * Local Function Prototypes
 ******************************************************************************/
static bool se_Veto(void);

static bool se_Marshal(const bdaddr *bd_addr, 
                       marshal_type_t type,
                       void **marshal_obj);

static app_unmarshal_status_t se_Unmarshal(const bdaddr *bd_addr, 
                         marshal_type_t type,
                         void *unmarshal_obj);

static void se_Commit(bool is_primary);

/******************************************************************************
 * Global Declarations
 ******************************************************************************/
const marshal_type_t se_marshal_types[] = {
    MARSHAL_TYPE(single_entity_data_t)
};

const marshal_type_list_t se_marshal_types_list = {se_marshal_types, sizeof(se_marshal_types)/sizeof(marshal_type_t)};

REGISTER_HANDOVER_INTERFACE(AUDIO_ROUTER, &se_marshal_types_list, se_Veto, se_Marshal, se_Unmarshal, se_Commit);

extern single_entity_data_t single_entity_data;

/******************************************************************************
 * Local Function Definitions
 ******************************************************************************/

/*! 
    \brief Handle Veto check during handover
    \return TRUE to veto handover.
*/
static bool se_Veto(void)
{
    return FALSE;
}

/*!
    \brief The function shall set marshal_obj to the address of the object to 
           be marshalled.

    \param[in] bd_addr      Bluetooth address of the link to be marshalled
                            \ref bdaddr
    \param[in] type         Type of the data to be marshalled \ref marshal_type_t
    \param[out] marshal_obj Holds address of data to be marshalled.
    \return TRUE: Required data has been copied to the marshal_obj.
            FALSE: No data is required to be marshalled. marshal_obj is set to NULL.

*/
static bool se_Marshal(const bdaddr *bd_addr, 
                       marshal_type_t type, 
                       void **marshal_obj)
{
    UNUSED(bd_addr);
    *marshal_obj = NULL;

    switch (type)
    {
        case MARSHAL_TYPE(single_entity_data_t):
        {
            *marshal_obj = &single_entity_data;
            return TRUE;
        }
        
        default:
        break;
    }

    return FALSE;
}

/*! 
    \brief The function shall copy the unmarshal_obj associated to specific 
            marshal type \ref marshal_type_t

    \param[in] bd_addr      Bluetooth address of the link to be unmarshalled
                            \ref bdaddr
    \param[in] type         Type of the unmarshalled data \ref marshal_type_t
    \param[in] unmarshal_obj Address of the unmarshalled object.
    \return unmarshalling result. Based on this, caller decides whether to free
            the marshalling object or not.
*/
static app_unmarshal_status_t se_Unmarshal(const bdaddr *bd_addr, 
                         marshal_type_t type, 
                         void *unmarshal_obj)
{
    UNUSED(bd_addr);
    
    switch(type)
    {
        case MARSHAL_TYPE(single_entity_data_t):
        {
            single_entity_data = *((single_entity_data_t*)unmarshal_obj);
            return UNMARSHAL_SUCCESS_FREE_OBJECT;
        }
        default:
        break;
    }
    
    return UNMARSHAL_FAILURE;
}

/*!
    \brief Component commits to the specified role

    The component should take any actions necessary to commit to the
    new role.

    \param[in] is_primary   TRUE if device role is primary, else secondary

*/
static void se_Commit(bool is_primary)
{
    if(!is_primary)
    {
        memset(&single_entity_data, 0, sizeof(single_entity_data_t));
    }
}

#endif /* INCLUDE_MIRRORING */

