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

#include "tclist.h"

/* Get the larger value of two integers. */
long tclmax(long a, long b){
  return (a > b) ? a : b;
}


/* Create a list object. */
TCLIST *tclistnew(void){
  TCLIST *list;
  TCMALLOC(list, sizeof(*list));
  list->anum = TCLISTUNIT;
  TCMALLOC(list->array, sizeof(list->array[0]) * list->anum);
  list->start = 0;
  list->num = 0;
  return list;
}

/* Get the number of elements of a list object. */
int tclistnum(const TCLIST *list){
  assert(list);
  return list->num;
}

/* Add a string element at the end of a list object. */
void tclistpush2(TCLIST *list, const char *str){
  assert(list && str);
  int index = list->start + list->num;
  if(index >= list->anum){
    list->anum += list->num + 1;
    TCREALLOC(list->array, list->array, list->anum * sizeof(list->array[0]));
  }
  int size = strlen(str);
  TCLISTDATUM *array = list->array;
  TCMALLOC(array[index].ptr, tclmax(size + 1, TCXSTRUNIT));
  memcpy(array[index].ptr, str, size + 1);
  array[index].size = size;
  list->num++;
}


/* Add an element at the end of a list object. */
void tclistpush(TCLIST *list, const void *ptr, int size){
  assert(list && ptr && size >= 0);
  int index = list->start + list->num;
  if(index >= list->anum){
    list->anum += list->num + 1;
    TCREALLOC(list->array, list->array, list->anum * sizeof(list->array[0]));
  }
  TCLISTDATUM *array = list->array;
  TCMALLOC(array[index].ptr, tclmax(size + 1, TCXSTRUNIT));
  memcpy(array[index].ptr, ptr, size);
  array[index].ptr[size] = '\0';
  array[index].size = size;
  list->num++;
}

/* Clear a list object. */
void tclistclear(TCLIST *list){
  int i;
  assert(list);
  TCLISTDATUM *array = list->array;
  int end = list->start + list->num;
  for(i = list->start; i < end; i++){
    TCFREE(array[i].ptr);
  }
  list->start = 0;
  list->num = 0;
}

const void *tclistval(const TCLIST *list, int index, int *sp){
  assert(list && index >= 0 && sp);
  if(index >= list->num) return NULL;
  index += list->start;
  *sp = list->array[index].size;
  return list->array[index].ptr;
}


/* Get the string of an element of a list object. */
const char *tclistval2(const TCLIST *list, int index){
  assert(list && index >= 0);
  if(index >= list->num) return NULL;
  index += list->start;
  return list->array[index].ptr;
}



void *tcmyfatal(const char *message){
  assert(message);
  fprintf(stderr, "fatal error: %s\n", message);
  exit(1);
  return NULL;
}

