/*!
\copyright  Copyright (c) 2008 - 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file
\brief      Link policy manager for TWM builds.

This file controls the link policy for the handset in TWM products.
The link policy with the peer earbud is controlled by the mirror profile.

The handset must be master of the link to the primary earbud - mirroring is only
supported when the primary earbud is ACL slave. Therefore this code requests a
role switch to become slave and will prohibit further role switches once slave.

The following functions that are called from BT profiles to allow, prohibit or
change roles that are relevant for non-TWM products are now ignored:
    appLinkPolicyAllowRoleSwitchForSink
    appLinkPolicyPreventRoleSwitchForSink
    appLinkPolicyPreventRoleSwitchForSink
    appLinkPolicyUpdateRoleFromSink.
*/

#include "link_policy.h"
#include "logging.h"
#include "bdaddr.h"
#include "bt_device.h"
#include "app/bluestack/dm_prim.h"
#include "panic.h"

#ifdef INCLUDE_MIRRORING

/*! Make and populate a bluestack DM primitive based on the type.

    \note that this is a multiline macro so should not be used after a
    control statement (if, while) without the use of braces
 */
#define MAKE_PRIM_C(TYPE) MESSAGE_MAKE(prim,TYPE##_T); prim->common.op_code = TYPE; prim->common.length = sizeof(TYPE##_T);

/*! Make and populate a bluestack DM primitive based on the type.

    \note that this is a multiline macro so should not be used after a
    control statement (if, while) without the use of braces
 */
#define MAKE_PRIM_T(TYPE) MESSAGE_MAKE(prim,TYPE##_T); prim->type = TYPE;

void appLinkPolicyAllowRoleSwitch(const bdaddr *bd_addr)
{
    UNUSED(bd_addr);
}

void appLinkPolicyAllowRoleSwitchForSink(Sink sink)
{
    UNUSED(sink);
}

void appLinkPolicyPreventRoleSwitch(const bdaddr *bd_addr)
{
    UNUSED(bd_addr);
}

void appLinkPolicyPreventRoleSwitchForSink(Sink sink)
{
    UNUSED(sink);
}

void appLinkPolicyUpdateRoleFromSink(Sink sink)
{
    UNUSED(sink);
}

static void appLinkPolicyBredrSecureConnectionHostSupportOverrideSet(const bdaddr *bd_addr, uint8 override_value)
{
    MAKE_PRIM_T(DM_WRITE_SC_HOST_SUPPORT_OVERRIDE_REQ);
    BdaddrConvertVmToBluestack(&prim->bd_addr, bd_addr);
    prim->host_support_override = override_value;
    VmSendDmPrim(prim);
    DEBUG_LOG("appLinkPolicyBredrSecureConnectionHostSupportOverrideSet 0x%x:%d", bd_addr->lap, override_value);
}

void appLinkPolicyBredrSecureConnectionHostSupportOverrideEnable(const bdaddr *bd_addr)
{
    appLinkPolicyBredrSecureConnectionHostSupportOverrideSet(bd_addr, 0x01);
}

void appLinkPolicyBredrSecureConnectionHostSupportOverrideRemove(const bdaddr *bd_addr)
{
    appLinkPolicyBredrSecureConnectionHostSupportOverrideSet(bd_addr, 0xFF);
}

/*! \brief Send a prim directly to bluestack to role switch to slave.
    \param The address of remote device link.
    \param The role to request.
*/
static void role_switch(const bdaddr *bd_addr, hci_role new_role)
{
    ConnectionSetRoleBdaddr(&LinkPolicyGetTaskData()->task, bd_addr, new_role);
}

/*! \brief Prevent role switching

    Update the policy for the connection (if any) to the specified
    Bluetooth address, so as to prevent any future role switching.

    \param  bd_addr The Bluetooth address of the device
*/
static void prevent_role_switch(const bdaddr *bd_addr)
{
    MAKE_PRIM_C(DM_HCI_WRITE_LINK_POLICY_SETTINGS_REQ);
    BdaddrConvertVmToBluestack(&prim->bd_addr, bd_addr);
    prim->link_policy_settings = ENABLE_SNIFF;
    VmSendDmPrim(prim);
}

/*! \brief Discover the current role

    \param  bd_addr The Bluetooth address of the remote device
*/
static void discover_role(const bdaddr *bd_addr)
{
    ConnectionGetRoleBdaddr(&LinkPolicyGetTaskData()->task, bd_addr);
}

void appLinkPolicyHandleClDmAclOpendedIndication(const CL_DM_ACL_OPENED_IND_T *ind)
{
    uint16 flags = ind->flags;
    bool is_ble = !!(flags & DM_ACL_FLAG_ULP);
    bool is_local = !!(~flags & DM_ACL_FLAG_INCOMING);
    const bdaddr *bd_addr = &ind->bd_addr.addr;

    if (!is_ble)
    {
        if (appDeviceIsHandset(bd_addr))
        {
            discover_role(bd_addr);
        }

        /* Set default link supervision timeout if locally inititated (i.e. we're master) */
        if (is_local)
        {
            appLinkPolicyUpdateLinkSupervisionTimeout(bd_addr);
        }
    }
}

/*! Handle a change in role or discovery of role, triggering any further role
 * switches required */
static void appLinkPolicyHandleRole(const bdaddr *bd_addr, hci_status status, hci_role role)
{
    hci_role new_role = hci_role_dont_care;

    if(appDeviceIsPeer(bd_addr))
    {
        DEBUG_LOG("appLinkPolicyHandleRole, peer enum:hci_status:%d enum:hci_role:%d",
                        status, role);

        if(status == hci_success && role == hci_role_master)
        {
            appLinkPolicyUpdateLinkSupervisionTimeout(bd_addr);
        }
    }

    if (appDeviceIsHandset(bd_addr))
    {
        DEBUG_LOG("appLinkPolicyHandleRole, handset enum:hci_status:%d enum:hci_role:%d",
                        status, role);

        switch (status)
        {
            case hci_success:
                if (role == hci_role_master)
                {
                    new_role = hci_role_slave;
                }
                else
                {
                    /* This is the desired topology, disallow role switches */
                    prevent_role_switch(bd_addr);
                }
            break;

            case hci_error_role_change_not_allowed:
            case hci_error_role_switch_failed:
            case hci_error_lmp_response_timeout:
            case hci_error_unrecognised:
            case hci_error_controller_busy:
            case hci_error_instant_passed:
                discover_role(bd_addr);
            break;

            default:
            break;
        }
        if (new_role != hci_role_dont_care)
        {
            role_switch(bd_addr, new_role);
        }
    }
}

static void appLinkPolicyHandleScHostSupportOverrideCfm(const DM_WRITE_SC_HOST_SUPPORT_OVERRIDE_CFM_T *cfm)
{
    if (cfm->status == hci_success)
    {
        DEBUG_LOG("appLinkPolicyHandleScHostSupportOverrideCfm, 0x%x:%d", cfm->bd_addr.lap, cfm->host_support_override);
    }
    else
    {
        Panic();
    }
}

bool appLinkPolicyHandleConnectionLibraryMessages(MessageId id, Message message, bool already_handled)
{
    switch (id)
    {
        case CL_DM_ROLE_CFM:
        {
            const CL_DM_ROLE_CFM_T *cfm = message;
            appLinkPolicyHandleRole(&cfm->bd_addr, cfm->status, cfm->role);
        }
        return TRUE;

        case CL_DM_ROLE_IND:
        {
            const CL_DM_ROLE_IND_T *ind = message;
            appLinkPolicyHandleRole(&ind->bd_addr, ind->status, ind->role);
        }
        return TRUE;

        case DM_WRITE_SC_HOST_SUPPORT_OVERRIDE_CFM:
            appLinkPolicyHandleScHostSupportOverrideCfm(message);
            return TRUE;

        case CL_DM_ULP_SET_PRIVACY_MODE_CFM:
        {
            CL_DM_ULP_SET_PRIVACY_MODE_CFM_T *cfm = (CL_DM_ULP_SET_PRIVACY_MODE_CFM_T *)message;
            PanicFalse(cfm->status == hci_success);
            return TRUE;
        }
    }
    return already_handled;
}

void appLinkPolicyRegisterTestTask(Task task)
{
    UNUSED(task);
}


void appLinkPolicyHandoverForceUpdateHandsetLinkPolicy(const bdaddr *bd_addr)
{
    /* Note that this prevents role switch, but allows sniff */
    prevent_role_switch(bd_addr);
    appLinkPolicyForceUpdatePowerTable(bd_addr);
}

#endif
