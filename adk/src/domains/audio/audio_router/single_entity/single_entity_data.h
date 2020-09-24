/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\defgroup   audio_router single_entity
\ingroup    audio_domain
\brief      Storage and retrieval of dynamic data for the single entity module
*/

#ifndef SINGLE_ENTITY_DATA_H_
#define SINGLE_ENTITY_DATA_H_

#include "audio_router.h"


/*! \brief Add source to the list of current connected sources

    \param source The source to be added to the list

    \return TRUE if successfully added. 
    
    \note This will fail if you try to add the same source twice. 
 */
bool SingleEntityData_AddSource(audio_router_source_t source);

/*! \brief Remove source from the list of current connected sources

    \param source The source to be removed from the list

    \return TRUE if successfully removed. Note that this will fail
            if the source isn't in the list. 
 */
bool SingleEntityData_RemoveSource(audio_router_source_t source);

/*! \brief Record that 'source' is currently routed

    \param source The source that is routed
 */
void SingleEntityData_SetRoutedSource(audio_router_source_t source);

/*! \brief Record that 'source' is no longer routed

    \param source The source that is not routed

    \return TRUE if successfully updated.
 */
bool SingleEntityData_ClearRoutedSource(audio_router_source_t source);

/*! \brief Get the source currently marked as routed

    \param source Pointer to audio_router_source_t that will be populated
                  with routed source (if there is one)

    \return TRUE if a source is marked routed.
 */
bool SingleEntityData_GetRoutedSource(audio_router_source_t* source);

/*! \brief Set the stored priority of a given audio_source_type_t

    \param source_type Source type to set the priority of.

    \param priority The priority to associate with source_type.

    \return TRUE if the priority is stored.
 */
bool SingleEntityData_SetSourceTypePriority(audio_source_type_t source_type, unsigned priority);

/*! \brief Get the stored priority of a given audio_source_type_t

    \param source_type Source type to set the priority of.

    \return The priority of the source. This will return 0 (the lowest priority) if the source type is not found
 */
unsigned SingleEntityData_GetSourceTypePriority(audio_source_type_t source_type);

/*! \brief Get the source to route

    \param source Pointer to a source structure to be populated with the source to route

    \return TRUE if source contains a source to route.
 */
bool SingleEntityData_GetSourceToRoute(audio_router_source_t* source);

/*! \brief Utlity to compare two source structures

    \param source1 first source for comparison

    \param source2 second source for comparison

    \return TRUE if source1 == source2
 */
bool SingleEntityData_AreSourcesSame(audio_router_source_t source1, audio_router_source_t source2);

/*! \brief Reset all the data structures within single entity data.
 */
void SingleEntityData_Init(void);

/*! \brief Test if source is present within single entity data

    \param source source to find

    \return TRUE if source is marked as present
*/
bool SingleEntityData_IsSourcePresent(audio_router_source_t source);

#endif /* SINGLE_ENTITY_DATA_H_ */