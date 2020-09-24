/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\ingroup    kymera
\brief      Function to set UCID

*/

#include "kymera_ucid.h"
#include "kymera_private.h"


bool Kymera_SetOperatorUcid(kymera_chain_handle_t chain, chain_operator_role_t role, kymera_operator_ucid_t ucid)
{
    Operator op;
    if (GET_OP_FROM_CHAIN(op, chain, role))
    {
        OperatorsStandardSetUCID(op, ucid);
        return TRUE;
    }
    return FALSE;
}
