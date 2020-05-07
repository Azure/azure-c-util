// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#ifndef SM_H
#define SM_H

#include "azure_macro_utils/macro_utils.h"

#ifdef __cplusplus
extern "C"
{
#endif

#include "umock_c/umock_c_prod.h"

typedef struct SM_HANDLE_DATA_TAG* SM_HANDLE;

#define SM_RESULT_VALUES    \
    SM_EXEC_GRANTED,        \
    SM_EXEC_REFUSED,        \
    SM_ERROR                \

MU_DEFINE_ENUM(SM_RESULT, SM_RESULT_VALUES);

MOCKABLE_FUNCTION(, SM_HANDLE, sm_create, const char*, name);
MOCKABLE_FUNCTION(, void, sm_destroy, SM_HANDLE, sm);

MOCKABLE_FUNCTION(, SM_RESULT, sm_open_begin, SM_HANDLE, sm);
MOCKABLE_FUNCTION(, void, sm_open_end, SM_HANDLE, sm);

MOCKABLE_FUNCTION(, SM_RESULT, sm_close_begin, SM_HANDLE, sm);
MOCKABLE_FUNCTION(, void, sm_close_end, SM_HANDLE, sm);

MOCKABLE_FUNCTION(, SM_RESULT, sm_begin, SM_HANDLE, sm);
MOCKABLE_FUNCTION(, void, sm_end, SM_HANDLE, sm);

MOCKABLE_FUNCTION(, SM_RESULT, sm_barrier_begin, SM_HANDLE, sm);
MOCKABLE_FUNCTION(, void, sm_barrier_end, SM_HANDLE, sm);

#ifdef __cplusplus
}
#endif

#endif /*SM_H*/
