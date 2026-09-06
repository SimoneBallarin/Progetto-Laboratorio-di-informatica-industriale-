/**
 * @file idlist.c
 * @brief Implementazione della lista generica di ID.
 */

#include <stdlib.h>
#include <string.h>

#include "idlist.h"

short int idlist_add( idNode_t **head, const char *ID )
{
    idNode_t *node;
    idNode_t *cur;

    if ( head == NULL || ID == NULL ) {
        return ERR_NULL_PTR;
    }
    if ( strlen( ID ) == 0 || strlen( ID ) >= IDLENGTH ) {
        return ERR_ID_INVALID;
    }
    if ( idlist_contains( *head, ID ) ) {
        return ERR_DUPLICATE;
    }

    node = malloc( sizeof( idNode_t ) );
    if ( node == NULL ) {
        return ERR_ALLOC;
    }

    strncpy( node->ID, ID, IDLENGTH - 1 );
    node->ID[IDLENGTH - 1] = '\0';
    node->next = NULL;

    if ( *head == NULL ) {
        *head = node;
    } else {
        cur = *head;
        while ( cur->next != NULL ) {
            cur = cur->next;
        }
        cur->next = node;
    }

    return OP_SUCCESS;
}

bool idlist_contains( const idNode_t *head, const char *ID )
{
    const idNode_t *cur;

    if ( ID == NULL ) {
        return false;
    }

    cur = head;
    while ( cur != NULL ) {
        if ( strcmp( cur->ID, ID ) == 0 ) {
            return true;
        }
        cur = cur->next;
    }

    return false;
}

int idlist_count( const idNode_t *head )
{
    const idNode_t *cur;
    int count;

    count = 0;
    cur = head;
    while ( cur != NULL ) {
        count++;
        cur = cur->next;
    }

    return count;
}

short int idlist_getAt( const idNode_t *head, int index, char outID[IDLENGTH] )
{
    const idNode_t *cur;
    int i;

    if ( outID == NULL ) {
        return ERR_NULL_PTR;
    }
    if ( index < 0 ) {
        return ERR_OUT_OF_RANGE;
    }

    cur = head;
    i = 0;
    while ( cur != NULL && i < index ) {
        cur = cur->next;
        i++;
    }

    if ( cur == NULL ) {
        return ERR_NOT_FOUND;
    }

    strncpy( outID, cur->ID, IDLENGTH - 1 );
    outID[IDLENGTH - 1] = '\0';

    return OP_SUCCESS;
}

short int idlist_remove( idNode_t **head, const char *ID )
{
    idNode_t *cur;
    idNode_t *prev;

    if ( head == NULL || ID == NULL ) {
        return ERR_NULL_PTR;
    }

    prev = NULL;
    cur = *head;
    while ( cur != NULL && strcmp( cur->ID, ID ) != 0 ) {
        prev = cur;
        cur = cur->next;
    }

    if ( cur == NULL ) {
        return ERR_NOT_FOUND;
    }

    if ( prev == NULL ) {
        *head = cur->next;
    } else {
        prev->next = cur->next;
    }
    free( cur );

    return OP_SUCCESS;
}

void idlist_free( idNode_t *head )
{
    idNode_t *cur;
    idNode_t *next;

    cur = head;
    while ( cur != NULL ) {
        next = cur->next;
        free( cur );
        cur = next;
    }
}
