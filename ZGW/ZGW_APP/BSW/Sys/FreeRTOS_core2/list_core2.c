/*
 * FreeRTOS_core2 Kernel V10.5.1
 * Copyright (C) 2021 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * https://www.FreeRTOS_core2.org
 * https://github.com/FreeRTOS_core2
 *
 */


#include <stdlib.h>
#include <stdint.h>

/* Defining MPU_WRAPPERS_INCLUDED_FROM_API_FILE_core2 prevents task_core2.h from redefining
 * all the API functions to use the MPU wrappers.  That should only be done when
 * task_core2.h is included from an application file. */
#define MPU_WRAPPERS_INCLUDED_FROM_API_FILE_core2

#include "FreeRTOS_core2.h"
#include "list_core2.h"

/* Route detected core2 list corruption to the common reset hook without
 * pulling the full McuSm header into the FreeRTOS kernel unit. */
extern void McuSm_PerformResetHook( uint32_t resetReason, uint32_t resetInformation );

/* Mirror of the dedicated reset code from McuSm.h. Keep in sync if changed. */
#define MCUSM_RESET_REASON_C2_LIST_CORRUPT             388u
#define FREERTOS_CORE2_LIST_INSERT_MAX_SCAN            1024u
#define FREERTOS_CORE2_LIST_CORRUPT_INFO_WALK_LIMIT    1u
#define FREERTOS_CORE2_LIST_CORRUPT_INFO_NULL_NEXT     2u
#define FREERTOS_CORE2_LIST_CORRUPT_INFO_LOW_NEXT      3u
#define FREERTOS_CORE2_LIST_CORRUPT_INFO_REINSERT      4u

volatile uint32_t FreeRTOS_core2_DebugListInsertFailReason;
volatile uint32_t FreeRTOS_core2_DebugListInsertFailInfo;
volatile uint32_t FreeRTOS_core2_DebugListInsertList;
volatile uint32_t FreeRTOS_core2_DebugListInsertItem;
volatile uint32_t FreeRTOS_core2_DebugListInsertItemValue;
volatile uint32_t FreeRTOS_core2_DebugListInsertItemContainer;
volatile uint32_t FreeRTOS_core2_DebugListInsertListItems;
volatile uint32_t FreeRTOS_core2_DebugListInsertWalkCount;
volatile uint32_t FreeRTOS_core2_DebugListInsertIterator;
volatile uint32_t FreeRTOS_core2_DebugListInsertNext;
volatile uint32_t FreeRTOS_core2_DebugListInsertNextValue;

static void prvListInsertCorruptionReset_core2( const List_t_core2 * const pxList_core2,
                                                const ListItem_t_core2 * const pxNewListItem_core2,
                                                const ListItem_t_core2 * const pxIterator_core2,
                                                const ListItem_t_core2 * const pxNext_core2,
                                                uint32_t failInfo_core2,
                                                uint32_t walkCount_core2 );

/* Lint e9021, e961 and e750 are suppressed as a MISRA exception justified
 * because the MPU ports require MPU_WRAPPERS_INCLUDED_FROM_API_FILE_core2 to be
 * defined for the header files above, but not in this file, in order to
 * generate the correct privileged Vs unprivileged linkage and placement. */
#undef MPU_WRAPPERS_INCLUDED_FROM_API_FILE_core2 /*lint !e961 !e750 !e9021. */

static void prvListInsertCorruptionReset_core2( const List_t_core2 * const pxList_core2,
                                                const ListItem_t_core2 * const pxNewListItem_core2,
                                                const ListItem_t_core2 * const pxIterator_core2,
                                                const ListItem_t_core2 * const pxNext_core2,
                                                uint32_t failInfo_core2,
                                                uint32_t walkCount_core2 )
{
    FreeRTOS_core2_DebugListInsertFailReason = MCUSM_RESET_REASON_C2_LIST_CORRUPT;
    FreeRTOS_core2_DebugListInsertFailInfo = failInfo_core2;
    FreeRTOS_core2_DebugListInsertList = ( uint32_t ) pxList_core2;
    FreeRTOS_core2_DebugListInsertItem = ( uint32_t ) pxNewListItem_core2;
    FreeRTOS_core2_DebugListInsertIterator = ( uint32_t ) pxIterator_core2;
    FreeRTOS_core2_DebugListInsertNext = ( uint32_t ) pxNext_core2;
    FreeRTOS_core2_DebugListInsertWalkCount = walkCount_core2;

    if( pxList_core2 != NULL )
    {
        FreeRTOS_core2_DebugListInsertListItems = ( uint32_t ) pxList_core2->uxNumberOfItems_core2;
    }
    else
    {
        FreeRTOS_core2_DebugListInsertListItems = 0u;
    }

    if( pxNewListItem_core2 != NULL )
    {
        FreeRTOS_core2_DebugListInsertItemValue = ( uint32_t ) pxNewListItem_core2->xItemValue_core2;
        FreeRTOS_core2_DebugListInsertItemContainer = ( uint32_t ) pxNewListItem_core2->pxContainer_core2;
    }
    else
    {
        FreeRTOS_core2_DebugListInsertItemValue = 0u;
        FreeRTOS_core2_DebugListInsertItemContainer = 0u;
    }

    if( ( pxNext_core2 != NULL ) && ( ( uint32_t ) pxNext_core2 >= 0x10000u ) )
    {
        FreeRTOS_core2_DebugListInsertNextValue = ( uint32_t ) pxNext_core2->xItemValue_core2;
    }
    else
    {
        FreeRTOS_core2_DebugListInsertNextValue = 0u;
    }

    McuSm_PerformResetHook( MCUSM_RESET_REASON_C2_LIST_CORRUPT, failInfo_core2 );

    for( ; ; )
    {
        /* McuSm_PerformResetHook() should not return. */
    }
}

/*-----------------------------------------------------------
* PUBLIC LIST API documented in list.h
*----------------------------------------------------------*/

void vListInitialise_core2( List_t_core2 * const pxList_core2 )
{
    /* The list structure contains a list item which is used to mark the
     * end of the list.  To initialise the list the list end is inserted
     * as the only list entry. */
    pxList_core2->pxIndex_core2 = ( ListItem_t_core2 * ) &( pxList_core2->xListEnd_core2 ); /*lint !e826 !e740 !e9087 The mini list structure is used as the list end to save RAM.  This is checked and valid. */

    listSET_FIRST_LIST_ITEM_INTEGRITY_CHECK_VALUE_core2( &( pxList_core2->xListEnd_core2 ) );

    /* The list end value is the highest possible value in the list to
     * ensure it remains at the end of the list. */
    pxList_core2->xListEnd_core2.xItemValue_core2 = portMAX_DELAY_core2;

    /* The list end next and previous pointers point to itself so we know
     * when the list is empty. */
    pxList_core2->xListEnd_core2.pxNext_core2 = ( ListItem_t_core2 * ) &( pxList_core2->xListEnd_core2 );     /*lint !e826 !e740 !e9087 The mini list structure is used as the list end to save RAM.  This is checked and valid. */
    pxList_core2->xListEnd_core2.pxPrevious_core2 = ( ListItem_t_core2 * ) &( pxList_core2->xListEnd_core2 ); /*lint !e826 !e740 !e9087 The mini list structure is used as the list end to save RAM.  This is checked and valid. */

    /* Initialize the remaining fields of xListEnd_core2 when it is a proper ListItem_t_core2 */
    #if ( configUSE_MINI_LIST_ITEM_core2 == 0 )
    {
        pxList_core2->xListEnd_core2.pvOwner_core2 = NULL;
        pxList_core2->xListEnd_core2.pxContainer_core2 = NULL;
        listSET_SECOND_LIST_ITEM_INTEGRITY_CHECK_VALUE_core2( &( pxList_core2->xListEnd_core2 ) );
    }
    #endif

    pxList_core2->uxNumberOfItems_core2 = ( UBaseType_t_core2 ) 0U;

    /* Write known values into the list if
     * configUSE_LIST_DATA_INTEGRITY_CHECK_BYTES_core2 is set to 1. */
    listSET_LIST_INTEGRITY_CHECK_1_VALUE_core2( pxList_core2 );
    listSET_LIST_INTEGRITY_CHECK_2_VALUE_core2( pxList_core2 );
}
/*-----------------------------------------------------------*/

void vListInitialiseItem_core2( ListItem_t_core2 * const pxItem_core2 )
{
    /* Make sure the list item is not recorded as being on a list. */
    pxItem_core2->pxContainer_core2 = NULL;

    /* Write known values into the list item if
     * configUSE_LIST_DATA_INTEGRITY_CHECK_BYTES_core2 is set to 1. */
    listSET_FIRST_LIST_ITEM_INTEGRITY_CHECK_VALUE_core2( pxItem_core2 );
    listSET_SECOND_LIST_ITEM_INTEGRITY_CHECK_VALUE_core2( pxItem_core2 );
}
/*-----------------------------------------------------------*/

void vListInsertEnd_core2( List_t_core2 * const pxList_core2,
                     ListItem_t_core2 * const pxNewListItem_core2 )
{
    ListItem_t_core2 * const pxIndex_core2 = pxList_core2->pxIndex_core2;

    /* Only effective when configASSERT_core2() is also defined, these tests may catch
     * the list data structures being overwritten in memory.  They will not catch
     * data errors caused by incorrect configuration or use of FreeRTOS_core2. */
    listTEST_LIST_INTEGRITY_core2( pxList_core2 );
    listTEST_LIST_ITEM_INTEGRITY_core2( pxNewListItem_core2 );

    /* Insert a new list item into pxList_core2, but rather than sort the list,
     * makes the new list item the last item to be removed by a call to
     * listGET_OWNER_OF_NEXT_ENTRY_core2(). */
    pxNewListItem_core2->pxNext_core2 = pxIndex_core2;
    pxNewListItem_core2->pxPrevious_core2 = pxIndex_core2->pxPrevious_core2;

    /* Only used during decision coverage testing. */
    mtCOVERAGE_TEST_DELAY_core2();

    pxIndex_core2->pxPrevious_core2->pxNext_core2 = pxNewListItem_core2;
    pxIndex_core2->pxPrevious_core2 = pxNewListItem_core2;

    /* Remember which list the item is in. */
    pxNewListItem_core2->pxContainer_core2 = pxList_core2;

    ( pxList_core2->uxNumberOfItems_core2 )++;
}
/*-----------------------------------------------------------*/

void vListInsert_core2( List_t_core2 * const pxList_core2,
                  ListItem_t_core2 * const pxNewListItem_core2 )
{
    ListItem_t_core2 * pxIterator_core2;
    const TickType_t_core2 xValueOfInsertion_core2 = pxNewListItem_core2->xItemValue_core2;
    UBaseType_t_core2 uxWalkCount_core2 = ( UBaseType_t_core2 ) 0U;
    UBaseType_t_core2 uxWalkLimit_core2;

    /* Only effective when configASSERT_core2() is also defined, these tests may catch
     * the list data structures being overwritten in memory.  They will not catch
     * data errors caused by incorrect configuration or use of FreeRTOS_core2. */
    listTEST_LIST_INTEGRITY_core2( pxList_core2 );
    listTEST_LIST_ITEM_INTEGRITY_core2( pxNewListItem_core2 );

    if( pxNewListItem_core2->pxContainer_core2 != NULL )
    {
        prvListInsertCorruptionReset_core2( pxList_core2,
                                            pxNewListItem_core2,
                                            NULL,
                                            NULL,
                                            FREERTOS_CORE2_LIST_CORRUPT_INFO_REINSERT,
                                            0u );
    }

    /* Insert the new list item into the list, sorted in xItemValue_core2 order.
     *
     * If the list already contains a list item with the same item value then the
     * new list item should be placed after it.  This ensures that TCBs which are
     * stored in ready lists (all of which have the same xItemValue_core2 value) get a
     * share of the CPU.  However, if the xItemValue_core2 is the same as the back marker
     * the iteration loop below will not end.  Therefore the value is checked
     * first, and the algorithm slightly modified if necessary. */
    if( xValueOfInsertion_core2 == portMAX_DELAY_core2 )
    {
        pxIterator_core2 = pxList_core2->xListEnd_core2.pxPrevious_core2;
    }
    else
    {
        /* *** NOTE ***********************************************************
        *  If you find your application is crashing here then likely causes are
        *  listed below.  In addition see https://www.FreeRTOS_core2.org/FAQHelp.html for
        *  more tips, and ensure configASSERT_core2() is defined!
        *  https://www.FreeRTOS_core2.org/a00110.html#configASSERT_core2
        *
        *   1) Stack overflow -
        *      see https://www.FreeRTOS_core2.org/Stacks-and-stack-overflow-checking.html
        *   2) Incorrect interrupt priority assignment, especially on Cortex-M
        *      parts where numerically high priority values denote low actual
        *      interrupt priorities, which can seem counter intuitive.  See
        *      https://www.FreeRTOS_core2.org/RTOS-Cortex-M3-M4.html and the definition
        *      of configMAX_SYSCALL_INTERRUPT_PRIORITY on
        *      https://www.FreeRTOS_core2.org/a00110.html
        *   3) Calling an API function from within a critical section or when
        *      the scheduler is suspended, or calling an API function that does
        *      not end in "FromISR" from an interrupt.
        *   4) Using a queue or semaphore before it has been initialised or
        *      before the scheduler has been started (are interrupts firing
        *      before vTaskStartScheduler_core2() has been called?).
        *   5) If the FreeRTOS_core2 port supports interrupt nesting then ensure that
        *      the priority of the tick interrupt is at or below
        *      configMAX_SYSCALL_INTERRUPT_PRIORITY.
        **********************************************************************/

        if( pxList_core2->uxNumberOfItems_core2 >= ( ( UBaseType_t_core2 ) FREERTOS_CORE2_LIST_INSERT_MAX_SCAN - ( UBaseType_t_core2 ) 1U ) )
        {
            uxWalkLimit_core2 = ( UBaseType_t_core2 ) FREERTOS_CORE2_LIST_INSERT_MAX_SCAN;
        }
        else
        {
            uxWalkLimit_core2 = pxList_core2->uxNumberOfItems_core2 + ( UBaseType_t_core2 ) 1U;
        }

        pxIterator_core2 = ( ListItem_t_core2 * ) &( pxList_core2->xListEnd_core2 ); /*lint !e826 !e740 !e9087 The mini list structure is used as the list end to save RAM.  This is checked and valid. */
        for( ; ; )
        {
            ListItem_t_core2 * const pxNext_core2 = pxIterator_core2->pxNext_core2;

            if( pxNext_core2 == NULL )
            {
                prvListInsertCorruptionReset_core2( pxList_core2,
                                                    pxNewListItem_core2,
                                                    pxIterator_core2,
                                                    pxNext_core2,
                                                    FREERTOS_CORE2_LIST_CORRUPT_INFO_NULL_NEXT,
                                                    ( uint32_t ) uxWalkCount_core2 );
            }

            if( ( uint32_t ) pxNext_core2 < 0x10000u )
            {
                prvListInsertCorruptionReset_core2( pxList_core2,
                                                    pxNewListItem_core2,
                                                    pxIterator_core2,
                                                    pxNext_core2,
                                                    FREERTOS_CORE2_LIST_CORRUPT_INFO_LOW_NEXT,
                                                    ( uint32_t ) uxWalkCount_core2 );
            }

            if( pxNext_core2->xItemValue_core2 > xValueOfInsertion_core2 )
            {
                break;
            }

            pxIterator_core2 = pxNext_core2;
            uxWalkCount_core2++;

            if( uxWalkCount_core2 > uxWalkLimit_core2 )
            {
                prvListInsertCorruptionReset_core2( pxList_core2,
                                                    pxNewListItem_core2,
                                                    pxIterator_core2,
                                                    pxNext_core2,
                                                    FREERTOS_CORE2_LIST_CORRUPT_INFO_WALK_LIMIT,
                                                    ( uint32_t ) uxWalkCount_core2 );
            }
        }
    }

    pxNewListItem_core2->pxNext_core2 = pxIterator_core2->pxNext_core2;
    pxNewListItem_core2->pxNext_core2->pxPrevious_core2 = pxNewListItem_core2;
    pxNewListItem_core2->pxPrevious_core2 = pxIterator_core2;
    pxIterator_core2->pxNext_core2 = pxNewListItem_core2;

    /* Remember which list the item is in.  This allows fast removal of the
     * item later. */
    pxNewListItem_core2->pxContainer_core2 = pxList_core2;

    ( pxList_core2->uxNumberOfItems_core2 )++;
}
/*-----------------------------------------------------------*/

UBaseType_t_core2 uxListRemove_core2( ListItem_t_core2 * const pxItemToRemove_core2 )
{
/* The list item knows which list it is in.  Obtain the list from the list
 * item. */
    List_t_core2 * const pxList_core2 = pxItemToRemove_core2->pxContainer_core2;

    pxItemToRemove_core2->pxNext_core2->pxPrevious_core2 = pxItemToRemove_core2->pxPrevious_core2;
    pxItemToRemove_core2->pxPrevious_core2->pxNext_core2 = pxItemToRemove_core2->pxNext_core2;

    /* Only used during decision coverage testing. */
    mtCOVERAGE_TEST_DELAY_core2();

    /* Make sure the index is left pointing to a valid item. */
    if( pxList_core2->pxIndex_core2 == pxItemToRemove_core2 )
    {
        pxList_core2->pxIndex_core2 = pxItemToRemove_core2->pxPrevious_core2;
    }
    else
    {
        mtCOVERAGE_TEST_MARKER_core2();
    }

    pxItemToRemove_core2->pxContainer_core2 = NULL;
    ( pxList_core2->uxNumberOfItems_core2 )--;

    return pxList_core2->uxNumberOfItems_core2;
}
/*-----------------------------------------------------------*/
