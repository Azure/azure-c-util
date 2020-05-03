// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#ifndef SM_H
#define SM_H

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct SM_HANDLE_DATA_TAG* SM_HANDLE;

#include "umock_c/umock_c_prod.h"

    MOCKABLE_FUNCTION(, SM_HANDLE, sm_create, const char*, name);
    MOCKABLE_FUNCTION(, void, sm_destroy, SM_HANDLE, sm);

    MOCKABLE_FUNCTION(, int, sm_open_begin, SM_HANDLE, sm);
    MOCKABLE_FUNCTION(, void, sm_open_end, SM_HANDLE, sm);

    MOCKABLE_FUNCTION(, int, sm_close_begin, SM_HANDLE, sm);
    MOCKABLE_FUNCTION(, void, sm_close_end, SM_HANDLE, sm);

    MOCKABLE_FUNCTION(, int, sm_begin, SM_HANDLE, sm);
    MOCKABLE_FUNCTION(, void, sm_end, SM_HANDLE, sm);

    MOCKABLE_FUNCTION(, int, sm_barrier_begin, SM_HANDLE, sm);
    MOCKABLE_FUNCTION(, void, sm_barrier_end, SM_HANDLE, sm);

#ifdef __cplusplus
}
#endif


#endif