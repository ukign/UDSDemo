/*****************************************************
      heap.c - ANSI-C library: heap definition
 ----------------------------------------------------
   Copyright (c) Metrowerks, Basel, Switzerland
               All rights reserved
                  Do not modify!
 *****************************************************/

#include <stdlib.h>
#include <heap.h>
#include <libdefs.h>

#if LIBDEF_FAR_HEAP_DATA
  #pragma DATA_SEG __FAR_SEG HEAP_SEGMENT
#else
  #pragma DATA_SEG     HEAP_SEGMENT
#endif

#if defined(__HC08__) || defined(__RS08__) || defined(__HC12__) || defined(__XGATE__)
unsigned long _heap_[(((unsigned int)LIBDEF_HEAPSIZE + sizeof(long)) - 1U) / sizeof(long)]; /*lint !e960 , MISRA 10.1 REQ, the result of sizeof() has type size_t */
#else
unsigned long _heap_[((LIBDEF_HEAPSIZE + sizeof(long)) - 1) / sizeof(long)];
#endif

#pragma DATA_SEG DEFAULT

#pragma MESSAGE DISABLE C5703 /* code: parameter declared in function _heapcrash_ but not referenced */

void _heapcrash_(void *LIBDEF_HEAP_DPTRQ element, int cause) {
  /**** Implement your own heap error routine here */
  volatile int i = 0;
  i = i/i; /*lint !e414 !e564 !e931 division by zero! */
} /*lint !e818 !e715 !e438 , MISRA 16.7 ADV, symbol element not referenced */
/*****************************************************/
/* end heap.c */
