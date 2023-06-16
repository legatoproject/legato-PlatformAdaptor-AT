/**
 * @file pa_info.c
 *
 * AT implementation of @ref c_pa_info API.
 *
 * Copyright (C) Sierra Wireless Inc.
 */

#include "legato.h"
#include "interfaces.h"
#include "pa_info.h"
#include "pa_utils.h"


//--------------------------------------------------------------------------------------------------
/**
 * Get the CDMA Mobile Directory Number (MDN) of the device.
 *
 * @return
 *      - LE_OK            The function succeeded.
 *      - LE_FAULT         The function failed to get the value.
 *      - LE_OVERFLOW      The Mobile Directory Number length exceed the maximum length.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_info_GetMdn
(
    char*  mdnStr,            ///< [OUT] The Mobile Directory Number (MDN) string (null-terminated).
    size_t mdnStrNumElements  ///< [IN] Size of Mobile Directory Number string.
)
{
    return LE_FAULT;
}

//--------------------------------------------------------------------------------------------------
/**
 * Get the Product Requirement Information Part Number and Revision Number strings in ASCII text.
 *
 * @return
 *      - LE_OK            The function succeeded.
 *      - LE_FAULT         The function failed to get the value.
 *      - LE_OVERFLOW      The Part or the Revision Number strings length exceed the maximum length.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_info_GetPriId
(
    char*  priIdPnStr,              ///< [OUT] The Product Requirement Information Identifier
                                    ///<       (PRI ID) Part Number string (null-terminated).
    size_t priIdPnStrNumElements,   ///< [IN] Size of Product Requirement Information Identifier
                                    ///<      string.
    char*  priIdRevStr,             ///< [OUT] The Product Requirement Information Identifier
                                    ///<       (PRI ID) Revision Number string (null-terminated).
    size_t priIdRevStrNumElements   ///< [IN] Size of Product Requirement Information Identifier
                                    ///<      string.
)
{
    le_result_t res = LE_FAULT;

    if ((priIdPnStr == NULL) || (priIdRevStr == NULL))
    {
        LE_ERROR("priIdPnStr or priIdRevStr is NULL.");
        return LE_FAULT;
    }

    if (priIdPnStrNumElements < LE_INFO_MAX_PRIID_PN_BYTES)
    {
        LE_ERROR("priIdPnStrNumElements lentgh (%d) too small < %d",
                 (int) priIdPnStrNumElements,
                 LE_INFO_MAX_PRIID_PN_BYTES);
        res = LE_OVERFLOW;
    }

    if (priIdRevStrNumElements < LE_INFO_MAX_PRIID_REV_BYTES)
    {
        LE_ERROR("priIdRevStrNumElements lentgh (%d) too small < %d",
                 (int) priIdRevStrNumElements,
                 LE_INFO_MAX_PRIID_REV_BYTES);
        res = LE_OVERFLOW;
    }

    return res;
}

//--------------------------------------------------------------------------------------------------
/**
 * Get the Platform Serial Number (PSN) string.
 *
 * @return
 *      - LE_OK on success
 *      - LE_OVERFLOW if Platform Serial Number to big to fit in provided buffer
 *      - LE_FAULT for any other errors
 */
//--------------------------------------------------------------------------------------------------
le_result_t  pa_info_GetPlatformSerialNumber
(
    char*  platformSerialNumberStr,             ///< [OUT] Platform Serial Number string.
    size_t platformSerialNumberStrNumElements   ///< [IN] Size of Platform Serial Number string.
)
{
    return LE_FAULT;
}

//--------------------------------------------------------------------------------------------------
/**
 * Get Build time string in ASCII text.
 *
 * @return
 *      - LE_OK            The function succeeded.
 *      - LE_FAULT         The function failed to get the value.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_info_GetBuildTime
(
    char*  buildTime,    ///< [OUT] Build time buffer string.
    size_t size          ///< [IN]  size of buildTime buffer.
)
{
    LE_ERROR("Not implemented function called");
    return LE_NOT_IMPLEMENTED;
}

//--------------------------------------------------------------------------------------------------
/**
 * Get Product Name string in ASCII text.
 *
 * @return
 *      - LE_OK            The function succeeded.
 *      - LE_FAULT         The function failed to get the value.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_info_GetProductName
(
    char*  prodName,    ///< [OUT] Buffer to retrieve/store product name.
    size_t size         ///< [IN]  Buffer size.
)
{
    LE_ERROR("Not implemented function called");
    return LE_NOT_IMPLEMENTED;
}
