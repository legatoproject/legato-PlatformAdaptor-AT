/**
 * @file pa_riPin_simu.c
 *
 * AT implementation of PA Ring Indicator signal.
 *
 * Copyright (C) Sierra Wireless Inc.
 */

#include "pa_riPin.h"



//--------------------------------------------------------------------------------------------------
//                                       Public declarations
//--------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to initialize the PA Ring Indicator signal module.
 *
 * @return
 *   - LE_FAULT         The function failed.
 *   - LE_OK            The function succeeded.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_riPin_Init
(
    void
)
{
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Check whether the application core is the current owner of the Ring Indicator signal.
 *
 * @return
 *      - LE_OK           The function succeeded.
 *      - LE_FAULT        The function failed.
 *      - LE_UNSUPPORTED  The platform does not support this operation.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_riPin_AmIOwnerOfRingSignal
(
    bool* amIOwnerPtr ///< true when application core is the owner of the Ring Indicator signal,
                      ///  false when modem core is the owner of the Ring Indicator signal.
)
{
    return LE_FAULT;
}

//--------------------------------------------------------------------------------------------------
/**
 * Take control of the Ring Indicator signal.
 *
 * @return
 *      - LE_OK           The function succeeded.
 *      - LE_FAULT        The function failed.
 *      - LE_UNSUPPORTED  The platform does not support this operation.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_riPin_TakeRingSignal
(
    void
)
{
    return LE_FAULT;
}

//--------------------------------------------------------------------------------------------------
/**
 * Release control of the Ring Indicator signal.
 *
 * @return
 *      - LE_OK           The function succeeded.
 *      - LE_FAULT        The function failed.
 *      - LE_UNSUPPORTED  The platform does not support this operation.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_riPin_ReleaseRingSignal
(
    void
)
{
    return LE_FAULT;
}

//--------------------------------------------------------------------------------------------------
/**
 * Set RI GPIO value
 */
//--------------------------------------------------------------------------------------------------
void pa_riPin_Set
(
    uint8_t     set ///< [IN] 1 to Pull up GPIO RI or 0 to lower it down
)
{
    return ;
}
