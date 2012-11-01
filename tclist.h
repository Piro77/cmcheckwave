/*************************************************************************************************
 * The utility API of Tokyo Cabinet
 *                                                               Copyright (C) 2006-2011 FAL Labs
 * This file is part of Tokyo Cabinet.
 * Tokyo Cabinet is free software; you can redistribute it and/or modify it under the terms of
 * the GNU Lesser General Public License as published by the Free Software Foundation; either
 * version 2.1 of the License or any later version.  Tokyo Cabinet is distributed in the hope
 * that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 * You should have received a copy of the GNU Lesser General Public License along with Tokyo
 * Cabinet; if not, write to the Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307 USA.
 *************************************************************************************************/


#ifndef _TCUTIL_H                        /* duplication check */
#define _TCUTIL_H

#include <stdlib.h>
#if ! defined(__cplusplus)
#include <stdbool.h>
#endif
#include <stdint.h>
#include <time.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <assert.h>


/*************************************************************************************************
 * array list
 *************************************************************************************************/

#define TCXSTRUNIT     12                // allocation unit size of an extensible string

#define TCLISTUNIT     64                // allocation unit number of a list handle

/* Show error message on the standard error output and exit.
   `message' specifies an error message.
   This function does not return. */
void *tcmyfatal(const char *message);

#define TCMALLOC(TC_res, TC_size) \
  do { \
    if(!((TC_res) = malloc(TC_size))) tcmyfatal("out of memory"); \
  } while(false)

#define TCCALLOC(TC_res, TC_nmemb, TC_size) \
  do { \
    if(!((TC_res) = calloc((TC_nmemb), (TC_size)))) tcmyfatal("out of memory"); \
  } while(false)

#define TCREALLOC(TC_res, TC_ptr, TC_size) \
  do { \
    if(!((TC_res) = realloc((TC_ptr), (TC_size)))) tcmyfatal("out of memory"); \
  } while(false)

#define TCFREE(TC_ptr) \
  do { \
    free(TC_ptr); \
  } while(false)

/* Alias of `tclistpush'. */
#define TCLISTPUSH(TC_list, TC_ptr, TC_size) \
  do { \
    int TC_mysize = (TC_size); \
    int TC_index = (TC_list)->start + (TC_list)->num; \
    if(TC_index >= (TC_list)->anum){ \
      (TC_list)->anum += (TC_list)->num + 1; \
      TCREALLOC((TC_list)->array, (TC_list)->array, \
                (TC_list)->anum * sizeof((TC_list)->array[0])); \
    } \
    TCLISTDATUM *array = (TC_list)->array; \
    TCMALLOC(array[TC_index].ptr, TC_mysize + 1);     \
    memcpy(array[TC_index].ptr, (TC_ptr), TC_mysize); \
    array[TC_index].ptr[TC_mysize] = '\0'; \
    array[TC_index].size = TC_mysize; \
    (TC_list)->num++; \
  } while(false)







typedef struct {                         /* type of structure for an element of a list */
  char *ptr;                             /* pointer to the region */
  int size;                              /* size of the effective region */
} TCLISTDATUM;

typedef struct {                         /* type of structure for an array list */
  TCLISTDATUM *array;                    /* array of data */
  int anum;                              /* number of the elements of the array */
  int start;                             /* start index of used elements */
  int num;                               /* number of used elements */
} TCLIST;


/* Create a list object.
   The return value is the new list object. */
TCLIST *tclistnew(void);

/* Get the number of elements of a list object.
   `list' specifies the list object.
   The return value is the number of elements of the list. */
int tclistnum(const TCLIST *list);

/* Add an element at the end of a list object.
   `list' specifies the list object.
   `ptr' specifies the pointer to the region of the new element.
   `size' specifies the size of the region. */
void tclistpush(TCLIST *list, const void *ptr, int size);

/* Add a string element at the end of a list object.
   `list' specifies the list object.
   `str' specifies the string of the new element. */
void tclistpush2(TCLIST *list, const char *str);

/* Get the pointer to the region of an element of a list object.
   `list' specifies the list object.
   `index' specifies the index of the element.
   `sp' specifies the pointer to the variable into which the size of the region of the return
   value is assigned.
   The return value is the pointer to the region of the value.
   Because an additional zero code is appended at the end of the region of the return value,
   the return value can be treated as a character string.  If `index' is equal to or more than
   the number of elements, the return value is `NULL'. */
const void *tclistval(const TCLIST *list, int index, int *sp);


/* Get the string of an element of a list object.
   `list' specifies the list object.
   `index' specifies the index of the element.
   The return value is the string of the value.
   If `index' is equal to or more than the number of elements, the return value is `NULL'. */
const char *tclistval2(const TCLIST *list, int index);



/* Clear a list object.
   `list' specifies the list object.
   All elements are removed. */
void tclistclear(TCLIST *list);

/* Create a list object by splitting a string.
   `str' specifies the source string.
   `delim' specifies a string containing delimiting characters.
   The return value is a list object of the split elements.
   If two delimiters are successive, it is assumed that an empty element is between the two.
   Because the object of the return value is created with the function `tclistnew', it should be
   deleted with the function `tclistdel' when it is no longer in use. */
TCLIST *tcstrsplit(const char *str, const char *delims);



#endif                                   /* duplication check */


/* END OF FILE */

