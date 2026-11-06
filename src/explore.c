#include "explore.h"
#include <stdio.h>
#include <string.h>

/* Datagram sizes: fits anywhere (100), fits the 2048 buffer alone (1500),
 * larger than 2048 (3000), undersized (8). */
const char *const cx9_alphabet[CX9_ALPHABET] = {
    "100:1",  "100:2",  "100:x",  "1500:1", "1500:2", "1500:x",
    "3000:1", "3000:2", "3000:x", "8:1",    "T"};

static cx9_event events[CX9_ALPHABET];

static void trace_str(const int *path, int len, char *out, size_t cap) {
  size_t used = 0;
  int i;
  out[0] = '\0';
  for (i = 0; i < len; ++i) {
    int w = snprintf(out + used, cap - used, "%s%s", i ? "," : "",
                     cx9_alphabet[path[i]]);
    if (w < 0 || (size_t)w >= cap - used) return;
    used += (size_t)w;
  }
}

static void dfs(const cx9_state *s, int *path, int len, int depth,
                cx9_explore_result *r) {
  int k;
  if (s->end != CX9_RUNNING || len == depth) {
    if (s->end == CX9_RUNNING)
      r->cut_at_depth++;
    else
      r->terminal[s->end]++;
    if (s->claim_violations) {
      r->with_claim_violation++;
      if (!r->shortest_claim || len < r->shortest_claim) {
        r->shortest_claim = len;
        trace_str(path, len, r->shortest_claim_trace,
                  sizeof r->shortest_claim_trace);
      }
    }
    if (s->end == CX9_OVERFLOW) {
      if (s->past_end > r->max_past_end) r->max_past_end = s->past_end;
      if (!r->shortest_overflow || len < r->shortest_overflow) {
        r->shortest_overflow = len;
        trace_str(path, len, r->shortest_trace, sizeof r->shortest_trace);
      }
    }
    return;
  }
  for (k = 0; k < CX9_ALPHABET; ++k) {
    cx9_state next = *s;
    path[len] = k;
    cx9_step(&next, events[k]);
    dfs(&next, path, len + 1, depth, r);
  }
}

void cx9_explore(cx9_version v, int dual, int single, int calls_max,
                 int depth, cx9_explore_result *r) {
  cx9_state s;
  int path[CX9_MAX_DEPTH];
  int k;
  memset(r, 0, sizeof *r);
  if (depth > CX9_MAX_DEPTH) depth = CX9_MAX_DEPTH;
  for (k = 0; k < CX9_ALPHABET; ++k) cx9_parse_event(cx9_alphabet[k], &events[k]);
  cx9_init(&s, v, dual, single, calls_max);
  dfs(&s, path, 0, depth, r);
}
