#include "explore.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LINE 1024

static void region_name(const cx9_state *s, cx9_ptr p, char *out, size_t n) {
  if (p.region < 0) snprintf(out, n, "NULL");
  else if (p.region == 0 && s->cap[0] == CX9_USER_CAP) snprintf(out, n, "USER2048+%d", p.off);
  else snprintf(out, n, "HEAP%d+%d", p.region, p.off);
}

static int run_trace(cx9_state *s, const char *events, int verbose) {
  char buf[LINE], a[32], b[32], c[32];
  char *tok, *save = NULL;
  snprintf(buf, sizeof buf, "%s", events);
  for (tok = strtok_r(buf, ",", &save); tok; tok = strtok_r(NULL, ",", &save)) {
    cx9_event e;
    if (cx9_parse_event(tok, &e)) {
      fprintf(stderr, "bad event: %s\n", tok);
      return -1;
    }
    cx9_step(s, e);
    if (verbose) {
      region_name(s, s->anscp, a, sizeof a);
      region_name(s, s->ans, b, sizeof b);
      region_name(s, s->ansp2, c, sizeof c);
      printf("%-7s call=%d anscp=%s anssiz=%d ans=%s ansp2=%s anssiz2=%d "
             "r1=%d r2=%d resplen=%d claim=%d room=%d %s\n",
             tok, s->calls, a, s->anssiz, b, c, s->anssiz2, s->recvresp1,
             s->recvresp2, s->resplen, s->last_claim, s->last_room,
             cx9_end_name(s->end));
    }
  }
  return 0;
}

static cx9_version parse_version(const char *v) {
  if (strcmp(v, "fix") == 0) return CX9_FIX;
  if (strcmp(v, "size_only") == 0) return CX9_SIZE_ONLY;
  return CX9_PRE;
}

static int results(const char *traces, const char *out) {
  char path[512], line[LINE];
  FILE *in = fopen(traces, "r"), *f;
  int v, dual, single;
  if (!in) return 1;
  snprintf(path, sizeof path, "%s/traces.csv", out);
  f = fopen(path, "w");
  if (!f) { fclose(in); return 1; }
  fprintf(f, "id,version,dual,single,end,past_end,claim_violations,"
             "first_claim_step,calls,anssiz,anscp_region\n");
  while (fgets(line, sizeof line, in)) {
    char id[64], ev[LINE];
    int d, sg;
    if (line[0] == '#' || line[0] == '\n') continue;
    if (sscanf(line, "%63s %d %d %1000s", id, &d, &sg, ev) != 4) continue;
    for (v = 0; v < 3; ++v) {
      cx9_state s;
      cx9_init(&s, (cx9_version)v, d, sg, 2);
      if (run_trace(&s, ev, 0)) { fclose(in); fclose(f); return 1; }
      fprintf(f, "%s,%s,%d,%d,%s,%d,%d,%d,%d,%d,%d\n", id,
              cx9_version_name((cx9_version)v), d, sg, cx9_end_name(s.end),
              s.past_end, s.claim_violations, s.first_claim_step, s.calls,
              s.anssiz, s.anscp.region);
    }
  }
  fclose(in);
  fclose(f);
  snprintf(path, sizeof path, "%s/explore.csv", out);
  f = fopen(path, "w");
  if (!f) return 1;
  fprintf(f, "version,dual,single,calls_max,depth,overflow,done,returned_one,"
             "returned_zero,exhausted,region_limit,cut_at_depth,"
             "with_claim_violation,max_past_end,shortest_overflow\n");
  for (v = 0; v < 3; ++v)
    for (dual = 1; dual >= 0; --dual)
      for (single = 0; single <= 1; ++single) {
        cx9_explore_result r;
        cx9_explore((cx9_version)v, dual, single, 2, 7, &r);
        fprintf(f, "%s,%d,%d,2,7,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%d,%s\n",
                cx9_version_name((cx9_version)v), dual, single,
                r.terminal[CX9_OVERFLOW], r.terminal[CX9_DONE],
                r.terminal[CX9_RETURNED_ONE], r.terminal[CX9_RETURNED_ZERO],
                r.terminal[CX9_EXHAUSTED], r.terminal[CX9_MODEL_LIMIT],
                r.cut_at_depth, r.with_claim_violation, r.max_past_end,
                r.shortest_overflow ? r.shortest_trace : "-");
      }
  fclose(f);
  return 0;
}

int main(int argc, char **argv) {
  if (argc >= 3 && strcmp(argv[1], "trace") == 0) {
    cx9_state s;
    int dual = argc > 4 ? atoi(argv[4]) : 1;
    int single = argc > 5 ? atoi(argv[5]) : 0;
    cx9_init(&s, parse_version(argv[2]), dual, single, 2);
    if (argc < 4 || run_trace(&s, argv[3], 1)) return 2;
    printf("end=%s past_end=%d claim_violations=%d\n", cx9_end_name(s.end),
           s.past_end, s.claim_violations);
    return 0;
  }
  if (argc == 4 && strcmp(argv[1], "results") == 0)
    return results(argv[2], argv[3]);
  if (argc >= 3 && strcmp(argv[1], "explore") == 0) {
    cx9_explore_result r;
    int depth = argc > 3 ? atoi(argv[3]) : 7;
    cx9_explore(parse_version(argv[2]), 1, 0, 2, depth, &r);
    printf("overflow=%ld done=%ld cut=%ld claim=%ld max_past=%d shortest=%s\n",
           r.terminal[CX9_OVERFLOW], r.terminal[CX9_DONE], r.cut_at_depth,
           r.with_claim_violation, r.max_past_end,
           r.shortest_overflow ? r.shortest_trace : "-");
    return 0;
  }
  fprintf(stderr,
          "usage: resmodel trace pre|fix|size_only EVENTS [dual] [single]\n"
          "       resmodel explore pre|fix|size_only [depth]\n"
          "       resmodel results fixtures/traces.tsv OUTDIR\n"
          "EVENTS: comma list of SIZE:1|2|x (datagram answering query 1, 2\n"
          "or neither) and T (poll timeout). Nothing is sent or received.\n");
  return 2;
}
