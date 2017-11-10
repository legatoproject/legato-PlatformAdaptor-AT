/** @file pa_lpt_local.h
 *
 * Copyright (C) Sierra Wireless Inc.
 */

#ifndef LEGATO_PALPTLOCAL_INCLUDE_GUARD
#define LEGATO_PALPTLOCAL_INCLUDE_GUARD


#include "legato.h"

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to initialize the LPT module.
 *
 * @return
 *  - LE_OK            The function succeeded.
 *  - LE_FAULT         The function failed to initialize the module.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_lpt_Init
(
    void
);

#endif // LEGATO_PALPTLOCAL_INCLUDE_GUARD
