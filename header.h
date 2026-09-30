#ifndef HEADER_H
#define HEADER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* -----------------------------------------------------------------------
 * M  –  your original "memory config" struct, kept exactly as before.
 *        mode[] was char mode[2] before; bumped to [4] to be safe.
 * ----------------------------------------------------------------------- */
typedef struct {
    int  v_size;   /* total virtual memory size  (2^VirtualMemory)  */
    int  p_size;   /* page / frame size in bytes (2^PageSize)        */
    int  phy_mem;  /* number of frames in physical memory            */
    char mode[4];  /* "d" → debug on  |  "n" → debug off            */
} M;

/* -----------------------------------------------------------------------
 * Frame  –  NEW struct (one per physical frame).
 *   page_id  : which virtual page is currently loaded (-1 = empty)
 *   dirty    : 1 if the page was written to ('w'), 0 otherwise
 *   ref_count: reference count used by the replacement policy
 *   load_order: monotonically increasing counter set when the page is
 *               loaded; used to implement FIFO among ref-count-0 pages.
 * ----------------------------------------------------------------------- */
typedef struct {
    int page_id;
    int dirty;
    int ref_count;
    int load_order;
} Frame;

#endif /* HEADER_H */
