// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>

#include "windows.h"

#include "azure_macro_utils/macro_utils.h"

#include "azure_c_util/xlogging.h"
#include "azure_c_util/gballoc.h"
#include "azure_c_util/interlocked_hl.h"

#include "azure_c_util/sm.h"

#define SM_STATE_VALUES             \
    SM_CREATED,                     \
    SM_OPENING,                     \
    SM_OPENED,                      \
    SM_OPENED_DRAINING_TO_BARRIER,  \
    SM_OPENED_DRAINING_TO_CLOSE,    \
    SM_OPENED_BARRIER,              \
    SM_CLOSING                      \

MU_DEFINE_ENUM(SM_STATE, SM_STATE_VALUES)

#define SM_STATE_MASK       127
#define SM_STATE_INCREMENT 256 /*at every state change, the state is incremented by this much*/

#define SM_CLOSE_BIT 128

#undef LogError
#define LogError(...) {}

typedef struct SM_HANDLE_DATA_TAG
{
    volatile LONG state;
    volatile LONG n; /*number of API calls to non-barriers*/
#ifdef _MSC_VER
/*warning C4200: nonstandard extension used: zero-sized array in struct/union : looks very standard in C99 and it is called flexible array. Documentation-wise is a flexible array, but called "unsized" in Microsoft's docs*/ /*https://msdn.microsoft.com/en-us/library/b6fae073.aspx*/
#pragma warning(disable:4200)
#endif

    char name[]; /*used in printing "who this is"*/
}SM_HANDLE_DATA;

static const char NO_NAME[] = "NO_NAME";

MU_DEFINE_ENUM_STRINGS(SM_RESULT, SM_RESULT_VALUES);
MU_DEFINE_ENUM_STRINGS(SM_STATE, SM_STATE_VALUES);

SM_HANDLE sm_create(const char* name)
{
    SM_HANDLE result;
    
    /*Codes_SRS_SM_02_001: [ If name is NULL then sm_create shall behave as if name was "NO_NAME". ]*/
    if (name == NULL)
    {
        name = NO_NAME; 
    }
    
    size_t flexSize = strlen(name) + 1;

    /*Codes_SRS_SM_02_002: [ sm_create shall allocate memory for the instance. ]*/
    result = malloc(sizeof(SM_HANDLE_DATA) + flexSize);

    if (result == NULL)
    {
        /*Codes_SRS_SM_02_004: [ If there are any failures then sm_create shall fail and return NULL. ]*/
        LogError("SM name=%s, failure in malloc(sizeof(SM_HANDLE_DATA)=%zu);",
            MU_P_OR_NULL(name), sizeof(SM_HANDLE_DATA));
        /*return as is*/
    }
    else
    {
        /*Codes_SRS_SM_02_003: [ sm_create shall set b_now to -1, n to 0, and e to 0 succeed and return a non-NULL value. ]*/
        (void)InterlockedExchange(&result->state, SM_CREATED);
        (void)InterlockedExchange(&result->n, 0);
        (void)memcpy(result->name, name, flexSize);
        /*return as is*/
    }
    return result;
}


void sm_destroy(SM_HANDLE sm)
{
    /*Codes_SRS_SM_02_005: [ If sm is NULL then sm_destroy shall return. ]*/
    if (sm == NULL)
    {
        LogError("invalid argument SM_HANDLE sm=%p", sm);
    }
    else
    {
        /*Codes_SRS_SM_02_006: [ sm_destroy shall free all used resources. ]*/
        free(sm);
    }
}

int sm_open_begin(SM_HANDLE sm)
{
    SM_RESULT result;
    if (sm == NULL)
    {
        /*Codes_SRS_SM_02_007: [ If sm is NULL then sm_open_begin shall fail and return SM_ERROR. ]*/
        LogError("invalid argument SM_HANDLE sm=%p", sm);
        result = SM_ERROR;
    }
    else
    {
        LONG state = InterlockedAdd(&sm->state, 0);
        if ((state & SM_STATE_MASK) != SM_CREATED)
        {
            LogError("cannot begin to open that which is in %" PRI_MU_ENUM " state", MU_ENUM_VALUE(SM_STATE, state & SM_STATE_MASK));
            result = SM_EXEC_REFUSED;
        }
        else
        {
            if (InterlockedCompareExchange(&sm->state, state - SM_CREATED + SM_OPENING + SM_STATE_INCREMENT, state) != state)
            {
                LogError("state changed meanwhile, maybe some other thread...");
                result = SM_EXEC_REFUSED;
            }
            else
            {
                result = SM_EXEC_GRANTED;
            }
        }
    }
    return result;
}

void sm_open_end(SM_HANDLE sm)
{
    if (sm == NULL)
    {
        /*Codes_SRS_SM_02_010: [ If sm is NULL then sm_open_end shall return. ]*/
        LogError("invalid argument SM_HANDLE sm=%p", sm);
    }
    else
    {
        LONG state = InterlockedAdd(&sm->state, 0);
        if ((state & SM_STATE_MASK) != SM_OPENING)
        {
            LogError("cannot end to open that which is in %" PRI_MU_ENUM " state", MU_ENUM_VALUE(SM_STATE, state & SM_STATE_MASK));
        }
        else
        {
            if (InterlockedCompareExchange(&sm->state, state - SM_OPENING + SM_OPENED + SM_STATE_INCREMENT, state) != state)
            {
                LogError("state changed meanwhile, very straaaaange");
            }
            else
            {
            }
        }
    }
}

SM_RESULT sm_close_begin(SM_HANDLE sm)
{

    SM_RESULT result;
    /*Codes_SRS_SM_02_013: [ If sm is NULL then sm_close_begin shall fail and return SM_ERROR. ]*/
    if (sm == NULL)
    {
        LogError("invalid argument SM_HANDLE sm=%p", sm);
        result = SM_ERROR;
    }
    else
    {
        if ((InterlockedOr(&sm->state, SM_CLOSE_BIT) & SM_CLOSE_BIT) == SM_CLOSE_BIT)
        {
            LogError("another thread is performing close");
            result = SM_EXEC_REFUSED;
        }
        else
        {
            do
            {
                LONG state = InterlockedAdd(&sm->state, 0);

                if ((state & SM_STATE_MASK) == SM_OPENED)
                {
                    if (InterlockedCompareExchange(&sm->state, state - SM_OPENED + SM_OPENED_DRAINING_TO_CLOSE + SM_STATE_INCREMENT, state) != state)
                    {
                        /*go and retry*/
                    }
                    else
                    {
                        InterlockedHL_WaitForValue(&sm->n, 0, INFINITE);

                        InterlockedAdd(&sm->state, -SM_OPENED_DRAINING_TO_CLOSE + SM_CLOSING + SM_STATE_INCREMENT);
                        result = SM_EXEC_GRANTED;
                        break;
                    }
                }
                else if (
                    ((state & SM_STATE_MASK) == SM_OPENED_BARRIER) ||
                    ((state & SM_STATE_MASK) == SM_OPENED_DRAINING_TO_BARRIER)
                    )
                {
                    Sleep(1);
                }
                else
                {
                    result = SM_EXEC_REFUSED;
                    break;
                }
            } while (1);


            (void)InterlockedAnd(&sm->state, ~(ULONG)SM_CLOSE_BIT);
        }
    }
    
    return result;
}

void sm_close_end(SM_HANDLE sm)
{
    if (sm == NULL)
    {
        /*Codes_SRS_SM_02_010: [ If sm is NULL then sm_open_end shall return. ]*/
        LogError("invalid argument SM_HANDLE sm=%p", sm);
    }
    else
    {
        LONG state = InterlockedAdd(&sm->state, 0);
        if ((state & SM_STATE_MASK) != SM_CLOSING)
        {
            LogError("cannot end to close that which is in %" PRI_MU_ENUM " state", MU_ENUM_VALUE(SM_STATE, state & SM_STATE_MASK));
        }
        else
        {
            if (InterlockedCompareExchange(&sm->state, state - SM_CLOSING + SM_CREATED + SM_STATE_INCREMENT, state) != state)
            {
                LogError("state changed meanwhile, very straaaaange");
            }
            else
            {

            }
        }
    }
}

SM_RESULT sm_begin(SM_HANDLE sm)
{
    SM_RESULT result;
    LONG state1 = InterlockedAdd(&sm->state, 0);
    if (
        ((state1 & SM_STATE_MASK) != SM_OPENED) ||
        ((state1 & SM_CLOSE_BIT) == SM_CLOSE_BIT)
        )
    {
        LogError("cannot execute begin when state is %" PRI_MU_ENUM "", MU_ENUM_VALUE(SM_STATE, state1 & SM_STATE_MASK));
        result = SM_EXEC_REFUSED;
    }
    else
    {
        InterlockedIncrement(&sm->n);
        LONG state2 = InterlockedAdd(&sm->state, 0);
        if (state2 != state1)
        {
            LONG n = InterlockedDecrement(&sm->n);
            if (n == 0)
            {
                WakeByAddressSingle((void*)&sm->n);
            }
            result = SM_EXEC_REFUSED;
        }
        else
        {
            result = SM_EXEC_GRANTED;
        }
    }
    return result;
}

void sm_end(SM_HANDLE sm)
{
    if (sm == NULL)
    {
        LogError("return");
    }
    else
    {
        LONG state = InterlockedAdd(&sm->state, 0);
        if (
            ((state & SM_STATE_MASK) != SM_OPENED) &&
            ((state & SM_STATE_MASK) != SM_OPENED_DRAINING_TO_BARRIER) &&
            ((state & SM_STATE_MASK) != SM_OPENED_DRAINING_TO_CLOSE)
            )
        {
            LogError("cannot execute end when state is %" PRI_MU_ENUM "", MU_ENUM_VALUE(SM_STATE, state & SM_STATE_MASK));
        }
        else
        {

            do /*a rather convoluted loop to make _end be idempotent (too many _end will be ignored)*/
            {
                LONG n = InterlockedAdd(&sm->n, 0);
                if (n <= 0)
                {
                    break;
                }
                else
                {
                    if (InterlockedCompareExchange(&sm->n, n - 1, n) != n)
                    {
                        /*well - continue...*/
                    }
                    else
                    {
                        if (n - 1 == 0)
                        {
                            WakeByAddressSingle((void*)&sm->n);
                        }
                        break;
                    }
                }
            } while (1);
        }
    }
}

SM_RESULT sm_barrier_begin(SM_HANDLE sm)
{
    SM_RESULT result;
    if (sm == NULL)
    {
        LogError("return");
        result = SM_ERROR;
    }
    else
    {
        LONG state = InterlockedAdd(&sm->state, 0);
        if (
            ((state & SM_STATE_MASK) != SM_OPENED) ||
            ((state & SM_CLOSE_BIT) == SM_CLOSE_BIT)
            )
        {
            LogError("cannot execute barrier begin when state is %" PRI_MU_ENUM "", MU_ENUM_VALUE(SM_STATE, state & SM_STATE_MASK));
            result = SM_EXEC_REFUSED;
        }
        else
        {
            if (InterlockedCompareExchange(&sm->state, state - SM_OPENED + SM_OPENED_DRAINING_TO_BARRIER + SM_STATE_INCREMENT, state) != state)
            {
                LogError("state changed meanwhile, this thread cannot start a barrier");
                result = SM_EXEC_REFUSED;
            }
            else
            {
                InterlockedHL_WaitForValue(&sm->n, 0, INFINITE);
                
                InterlockedAdd(&sm->state, - SM_OPENED_DRAINING_TO_BARRIER + SM_OPENED_BARRIER + SM_STATE_INCREMENT);
                result = SM_EXEC_GRANTED;
            }
        }
    }
    return result;
}

void sm_barrier_end(SM_HANDLE sm)
{
    if (sm == NULL)
    {
        LogError("return");
    }
    else
    {
        LONG state = InterlockedAdd(&sm->state, 0);
        if ((state & SM_STATE_MASK) != SM_OPENED_BARRIER)
        {
            LogError("cannot execute barrier end when state is %" PRI_MU_ENUM "", MU_ENUM_VALUE(SM_STATE, state & SM_STATE_MASK));
        }
        else
        {
            if (InterlockedCompareExchange(&sm->state, state - SM_OPENED_BARRIER + SM_OPENED + SM_STATE_INCREMENT, state) != state)
            {
                LogError("state changed meanwhile, very straaaaange");
            }
            else
            {
                /*it's all fine, we're back to SM_OPENED*/ /*let a close know about that, if any*/
            }
        }
    }
}
