/* =============================================================================
 * prog04.c  –  Memory Management Simulator (Paging + Page Replacement)
 *
 * USAGE:
 *   ./prog04 <VirtualMemory> <PageSize> <PhysicalMemory> <input-file> <d|n>
 *
 *   VirtualMemory  – exponent;  actual size = 2^VirtualMemory  (≤ 2^20)
 *   PageSize       – exponent;  actual size = 2^PageSize        (≥ 2^10)
 *   PhysicalMemory – number of frames (not a power-of-2 exponent)
 *   input-file     – trace file; each non-blank line: <ADDR> <r|w>
 *   d|n            – 'd' prints debug output, 'n' suppresses it
 *
 * REPLACEMENT POLICY (from spec):
 *   • On load        → ref_count = 3
 *   • Every 4 accesses (globally) → decrement every page's ref_count by 1
 *                                    (floor at 0)
 *   • On re-access   → ref_count += 1  (cap at 10)
 *                       (the very first access that caused the load does NOT
 *                        increment; only subsequent re-accesses do)
 *   • Victim         → FIFO among frames whose ref_count == 0
 *                       If none, decrement all by 1 and retry.
 * ============================================================================= */

#include "header.h"

/* ---------------------------------------------------------------------------
 * find_page_in_memory
 *   Linear scan of the physical frames to see if 'page' is already loaded.
 *   Returns the frame index (0..num_frames-1) if found, -1 if not.
 * --------------------------------------------------------------------------- */
int find_page_in_memory(Frame *frames, int num_frames, int page) {
    int i;
    for (i = 0; i < num_frames; i++) {
        if (frames[i].page_id == page)
            return i;          /* page hit → return frame index */
    }
    return -1;                 /* page miss (page fault)        */
}

/* ---------------------------------------------------------------------------
 * find_victim_frame
 *   Implements the custom replacement policy:
 *     1. Look for the oldest-loaded frame (smallest load_order) whose
 *        ref_count == 0  →  that is our FIFO victim.
 *     2. If no such frame exists, decrement every frame's ref_count by 1
 *        (floor 0) and repeat step 1.
 *   Returns the frame index of the chosen victim.
 * --------------------------------------------------------------------------- */
int find_victim_frame(Frame *frames, int num_frames) {
    int i, victim;

    while (1) {                /* keep trying until we find a victim */

        victim = -1;
        /* --- FIFO among ref_count == 0 frames ----------------------------- */
        for (i = 0; i < num_frames; i++) {
            if (frames[i].ref_count == 0) {
                /* pick the one loaded earliest (smallest load_order) */
                if (victim == -1 ||
                    frames[i].load_order < frames[victim].load_order) {
                    victim = i;
                }
            }
        }

        if (victim != -1)
            return victim;     /* found a suitable victim                    */

        /* --- No ref_count-0 frame found: decrement all by 1 --------------- */
        for (i = 0; i < num_frames; i++) {
            if (frames[i].ref_count > 0)
                frames[i].ref_count--;
        }
        /* loop again and retry */
    }
}

/* ---------------------------------------------------------------------------
 * periodic_decrement
 *   Called after every 4th memory access (access_count % 4 == 0).
 *   Reduces every loaded page's ref_count by 1, floored at 0.
 * --------------------------------------------------------------------------- */
void periodic_decrement(Frame *frames, int num_frames) {
    int i;
    for (i = 0; i < num_frames; i++) {
        if (frames[i].page_id != -1 && frames[i].ref_count > 0)
            frames[i].ref_count--;
    }
}

/* ===========================================================================
 * main
 * =========================================================================== */
int main(int c, char **v) {

    /* ------------------------------------------------------------------
     * 1. ARGUMENT VALIDATION  (your original check, unchanged)
     * ------------------------------------------------------------------ */
    if (c != 6) {
        printf("USAGE: ./prog04 virtual_mem page_size physical_mem"
               " trace_file debug_op\n");
        return 0;
    }

    /* ------------------------------------------------------------------
     * 2. PARSE COMMAND-LINE ARGS INTO M STRUCT  (your original code)
     *    v[1] = VirtualMemory exponent   → 2^v[1]
     *    v[2] = PageSize exponent         → 2^v[2]
     *    v[3] = number of physical frames (integer, NOT exponent)
     *    v[4] = trace file path
     *    v[5] = "d" or "n"
     * ------------------------------------------------------------------ */
    M m;
    m.v_size  = (int)pow(2, atof(v[1]));   /* e.g. 2^20 = 1 048 576 bytes  */
    m.p_size  = (int)pow(2, atof(v[2]));   /* e.g. 2^10 = 1 024 bytes/page */
    m.phy_mem = atoi(v[3]);                 /* e.g. 17 frames               */
    strcpy(m.mode, v[5]);                   /* "d" → debug, "n" → no debug  */

    /* ------------------------------------------------------------------
     * 3. READ TRACE FILE  (your original two-pass approach, kept intact)
     *    Pass 1: count non-blank lines so we know how much to allocate.
     *    Pass 2: read (address, r/w) pairs into arrays.
     * ------------------------------------------------------------------ */
    FILE *fp = fopen(v[4], "r");
    if (!fp) {
        printf("ERROR: Cannot open file %s\n", v[4]);
        return 1;
    }

    char str[50];
    int  line = 0;

    /* --- Pass 1: count lines (your original loop) ---------------------- */
    while (fgets(str, 50, fp)) {
        /* Skip blank lines (spec says they appear only at end of file)  */
        if (str[0] != '\n' && str[0] != '\r' && str[0] != '\0')
            line++;
    }
    rewind(fp);

    /* --- Allocate parallel arrays for addresses and r/w flags ---------- */
    long int *addr    = calloc(line, sizeof(long int)); /* virtual addresses  */
    char     *r_w     = calloc(line + 1, sizeof(char)); /* 'r' or 'w' per access */
    int      *page_num = calloc(line, sizeof(int));     /* virtual page number   */

    /* --- Pass 2: parse each line (your original fscanf loop) ----------- */
    int i = 0, j = 0;
    while (i < line && fscanf(fp, "%ld %c", &addr[i], &r_w[i]) == 2)
        i++;
    fclose(fp);

    /* --- Compute virtual page numbers from addresses ------------------- */
    /*     page_number = address / page_size                               */
    /*     This is equivalent to dropping the low (log2 page_size) bits.  */
    for (i = 0; i < line; i++)
        page_num[i] = addr[i] / m.p_size;

    /* ------------------------------------------------------------------
     * 4. INITIALISE PHYSICAL MEMORY (Frame array)
     *    Replaces your old page_in_mem[], mode[], ref_count[] arrays with
     *    a single Frame struct array — much cleaner, same idea.
     * ------------------------------------------------------------------ */
    Frame *frames = calloc(m.phy_mem, sizeof(Frame));
    for (i = 0; i < m.phy_mem; i++) {
        frames[i].page_id    = -1;  /* -1 means the frame is empty          */
        frames[i].dirty      =  0;
        frames[i].ref_count  =  0;
        frames[i].load_order =  0;
    }

    /* A monotonically increasing "clock" assigned when a page is loaded.
     * The frame with the smallest load_order among ref_count-0 frames is
     * evicted first (FIFO within that subset).                             */
    int load_clock = 0;

    /* ------------------------------------------------------------------
     * 5. STATISTICS COUNTERS
     * ------------------------------------------------------------------ */
    int total_accesses  = 0;  /* total memory accesses processed           */
    int page_faults     = 0;  /* faults where an existing page was replaced*/
    int pages_to_disk   = 0;  /* dirty pages actually written to disk      */

    /* ------------------------------------------------------------------
     * 6. FIND FIRST EMPTY FRAME (simple linear scan helper used below)
     *    Returns index of the first frame with page_id == -1, else -1.
     * ------------------------------------------------------------------ */

    /* ------------------------------------------------------------------
     * 7. MAIN SIMULATION LOOP
     *    For each memory access in the trace:
     *      a) Check if page is already in memory (page hit / miss)
     *      b) On miss: find a frame to use (empty first, then evict)
     *      c) Update ref counts (periodic every-4 decrement + re-access)
     *      d) Print debug info if mode == "d"
     * ------------------------------------------------------------------ */
    for (i = 0; i < line; i++) {

        total_accesses++;

        int pg   = page_num[i];   /* virtual page number for this access    */
        char rw  = r_w[i];        /* 'r' = read, 'w' = write                */

        /* ---- (a) Search physical memory for the page -------------------- */
        int frame_idx = find_page_in_memory(frames, m.phy_mem, pg);

        if (frame_idx != -1) {
            /* ============================================================
             * PAGE HIT: page is already in a frame.
             * Increment ref_count (cap at 10).
             * Note: the spec says the INITIAL load doesn't increment;
             * only re-accesses after the first one do.
             * ============================================================ */
            if (frames[frame_idx].ref_count < 10)
                frames[frame_idx].ref_count++;

            /* If this access is a write, mark the frame dirty */
            if (rw == 'w')
                frames[frame_idx].dirty = 1;

        } else {
            /* ============================================================
             * PAGE FAULT: page is not in memory.
             * Step 1 – find a frame to use.
             * Step 2 – if frame held a page, that is a "real" page fault
             *          (count it) and handle dirty write-back.
             * Step 3 – load the new page into the chosen frame.
             * ============================================================ */

            /* --- Step 1: prefer an empty frame first ------------------- */
            int victim_frame = -1;
            for (j = 0; j < m.phy_mem; j++) {
                if (frames[j].page_id == -1) {
                    victim_frame = j;   /* found an empty (NULL) frame      */
                    break;
                }
            }

            if (victim_frame == -1) {
                /* No empty frame → must evict using replacement policy     */
                victim_frame = find_victim_frame(frames, m.phy_mem);

                /* This is a "real" page fault (spec: count only those where
                 * a page is replaced by another, not empty-frame loads)    */
                page_faults++;

                /* --- Debug output for eviction ------------------------- */
                if (strcmp(m.mode, "d") == 0) {
                    printf("Page %d replaced by Page %d\n",
                           frames[victim_frame].page_id, pg);
                    if (frames[victim_frame].dirty)
                        printf("Page %d was dirty\n",
                               frames[victim_frame].page_id);
                    else
                        printf("Page %d was not dirty\n",
                               frames[victim_frame].page_id);
                }

                /* Dirty page must be written to disk before eviction       */
                if (frames[victim_frame].dirty)
                    pages_to_disk++;

            } else {
                /* Empty frame case – debug prints "Page NULL replaced by Y" */
                if (strcmp(m.mode, "d") == 0)
                    printf("Page NULL replaced by Page %d\n", pg);
            }

            /* --- Step 3: load new page into the chosen frame ----------- */
            frames[victim_frame].page_id    = pg;
            frames[victim_frame].dirty      = (rw == 'w') ? 1 : 0;
            frames[victim_frame].ref_count  = 3;   /* spec: load → rc = 3  */
            frames[victim_frame].load_order = load_clock++;
        }

        /* ---- (c) Every 4th access: periodic ref_count decrement --------- */
        /*  We decrement AFTER processing the access (post-access bookkeeping)*/
        if (total_accesses % 4 == 0)
            periodic_decrement(frames, m.phy_mem);
    }

    /* ------------------------------------------------------------------
     * 8. PRINT FINAL STATISTICS  (required by spec)
     * ------------------------------------------------------------------ */
    printf("\nNumber of Memory Accesses                        : %d\n",
           total_accesses);
    printf("Number of Page Faults (with replacement)         : %d\n",
           page_faults);
    printf("Number of Pages Written to Disk                  : %d\n",
           pages_to_disk);

    /* ------------------------------------------------------------------
     * 9. FREE ALLOCATED MEMORY  (good practice)
     * ------------------------------------------------------------------ */
    free(addr);
    free(r_w);
    free(page_num);
    free(frames);

    return 0;
}
