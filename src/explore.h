/* Exhaustive enumeration of every event sequence of the FINITE model up to
 * a depth bound. It covers the interleavings of this model's alphabet
 * only, not of the real resolver, network or kernel. */
#ifndef CX9_EXPLORE_H
#define CX9_EXPLORE_H
#include "resolv_model.h"

#define CX9_ALPHABET 11
#define CX9_MAX_DEPTH 10

typedef struct {
  long terminal[7];        /* count of traces per cx9_end, by first end */
  long cut_at_depth;       /* still RUNNING at the bound: MODEL_LIMIT */
  long with_claim_violation; /* traces that ended or were cut after one */
  int max_past_end;
  int shortest_overflow;   /* 0 = none */
  char shortest_trace[160];
  int shortest_claim;      /* shortest trace with a claim violation */
  char shortest_claim_trace[160];
} cx9_explore_result;

extern const char *const cx9_alphabet[CX9_ALPHABET];

/* Returns 0, or -1 (r zeroed, nothing explored) when depth is outside
 * 0..CX9_MAX_DEPTH, r is NULL or v is not a known version. Depth is never
 * clamped silently. */
int cx9_explore(cx9_version v, int dual, int single, int calls_max,
                int depth, cx9_explore_result *r);
#endif
