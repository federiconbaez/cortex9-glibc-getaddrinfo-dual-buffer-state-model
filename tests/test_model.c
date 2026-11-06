/* Fixtures against separately derived expectations, invariants over the
 * finite enumeration, a single-query control, and PRACTICE levels 1-8. */
#include "explore.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int checks, failures;
#define CHECK(c, what)                                                       \
  do {                                                                       \
    checks++;                                                                \
    if (!(c)) {                                                              \
      failures++;                                                            \
      printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, what);                  \
    }                                                                        \
  } while (0)

static void run(cx9_state *s, cx9_version v, int dual, int single,
                const char *events) {
  char buf[1024], *tok, *save = NULL;
  cx9_init(s, v, dual, single, 2);
  snprintf(buf, sizeof buf, "%s", events);
  for (tok = strtok_r(buf, ",", &save); tok; tok = strtok_r(NULL, ",", &save)) {
    cx9_event e;
    CHECK(cx9_parse_event(tok, &e) == 0, tok);
    cx9_step(s, e);
  }
}

static void test_fixtures(const char *dir) {
  char path[512], line[1024];
  char ids[32][64], evs[32][512];
  int duals[32], singles[32], n = 0, i, rows = 0, explore_rows = 0;
  FILE *f;
  snprintf(path, sizeof path, "%s/traces.tsv", dir);
  f = fopen(path, "r");
  CHECK(f != NULL, "open traces.tsv");
  if (!f) return;
  while (n < 32 && fgets(line, sizeof line, f))
    if (line[0] != '#' && line[0] != '\n' &&
        sscanf(line, "%63s %d %d %511s", ids[n], &duals[n], &singles[n],
               evs[n]) == 4)
      n++;
  fclose(f);
  snprintf(path, sizeof path, "%s/expected.tsv", dir);
  f = fopen(path, "r");
  CHECK(f != NULL, "open expected.tsv");
  if (!f) return;
  while (fgets(line, sizeof line, f)) {
    char id[64], ver[16], end[32];
    int past, claims, first, v = -1;
    long a, b, c, d;
    if (strncmp(line, "#explore", 8) == 0) {
      explore_rows = 1;
      continue;
    }
    if (explore_rows) {
      cx9_explore_result r;
      CHECK(sscanf(line, "%15s %ld %ld %ld %ld", ver, &a, &b, &c, &d) == 5,
            "explore row");
      for (i = 0; i < 3; ++i)
        if (strcmp(ver, cx9_version_name((cx9_version)i)) == 0) v = i;
      CHECK(v >= 0, ver);
      if (v < 0) continue;
      cx9_explore((cx9_version)v, 1, 0, 2, 5, &r);
      CHECK(r.terminal[CX9_OVERFLOW] == a, "depth-5 overflow count");
      CHECK(r.terminal[CX9_DONE] == b, "depth-5 done count");
      CHECK(r.cut_at_depth == c, "depth-5 cut count");
      CHECK(r.with_claim_violation == d, "depth-5 claim count");
      explore_rows++;
      continue;
    }
    if (sscanf(line, "%63s %15s %31s %d %d %d", id, ver, end, &past, &claims,
               &first) != 6)
      continue;
    for (i = 0; i < 3; ++i)
      if (strcmp(ver, cx9_version_name((cx9_version)i)) == 0) v = i;
    for (i = 0; i < n; ++i)
      if (strcmp(ids[i], id) == 0) {
        cx9_state s;
        run(&s, (cx9_version)v, duals[i], singles[i], evs[i]);
        CHECK(strcmp(cx9_end_name(s.end), end) == 0, id);
        CHECK(s.past_end == past, id);
        CHECK(s.claim_violations == claims, id);
        CHECK(s.first_claim_step == first, id);
        rows++;
      }
  }
  fclose(f);
  CHECK(rows == 3 * n && n == 11, "every fixture row checked");
  CHECK(explore_rows == 4, "three explore rows checked");
}

static void test_invariants(void) {
  int single, v;
  for (single = 0; single <= 1; ++single) {
    cx9_explore_result r;
    /* the fix: no recvfrom length ever exceeds the destination's room */
    cx9_explore(CX9_FIX, 1, single, 2, 7, &r);
    CHECK(r.terminal[CX9_OVERFLOW] == 0, "fix: no overflow at depth 7");
    CHECK(r.with_claim_violation == 0, "fix: no claim violation at depth 7");
    CHECK(r.terminal[CX9_MODEL_LIMIT] == 0, "fix: region bound not hit");
    /* pre: reachable, and bounded by the MAXPACKET length argument */
    cx9_explore(CX9_PRE, 1, single, 2, 7, &r);
    CHECK(r.terminal[CX9_OVERFLOW] > 0, "pre: overflow reachable");
    CHECK(r.max_past_end > 0 && r.max_past_end <= CX9_MAXPACKET - CX9_USER_CAP,
          "pre: past_end bounded");
    CHECK(r.shortest_overflow == 4, "pre: shortest overflow has 4 events");
  }
  /* control: without the second query (buf2 == NULL) no version overflows */
  for (v = 0; v < 3; ++v)
    for (single = 0; single <= 1; ++single) {
      cx9_explore_result r;
      cx9_explore((cx9_version)v, 0, single, 2, 7, &r);
      CHECK(r.terminal[CX9_OVERFLOW] == 0, "single-query control: none");
      CHECK(r.with_claim_violation == 0, "single-query control: no claim");
    }
}

static void practice(void) {
  cx9_state s;
  /* 1: control, one query, oversized answer: the first buffer is replaced */
  run(&s, CX9_PRE, 0, 0, "3000:1");
  CHECK(s.end == CX9_DONE && s.anscp.region == 1 && s.anssiz == CX9_MAXPACKET,
        "practice_level_1");
  /* 2: two queries, first answer of 100 bytes lands in USER at 0 */
  run(&s, CX9_PRE, 1, 0, "100:1");
  CHECK(s.anscp.region == 0 && s.resplen == 100 && s.recvresp1 == 1,
        "practice_level_2");
  /* 3: the second buffer is the rest of USER: offset 100, 1948 bytes */
  run(&s, CX9_PRE, 1, 0, "100:1,1500:x");
  CHECK(s.ansp2.region == 0 && s.ansp2.off == 100 && s.anssiz2 == 1948,
        "practice_level_3");
  /* 4: the pre realloc sets anssiz, not anssiz2: USER now claims 65536 */
  run(&s, CX9_PRE, 1, 0, "100:1,3000:x");
  CHECK(s.anscp.region == 0 && s.anssiz == CX9_MAXPACKET &&
            s.anssiz2 == 1948 && s.end == CX9_RUNNING,
        "practice_level_4");
  /* 5: goto retry, then a 3000-byte answer: 952 bytes past USER */
  run(&s, CX9_PRE, 1, 0, "100:1,3000:x,T,3000:1");
  CHECK(s.end == CX9_OVERFLOW && s.past_end == 952, "practice_level_5");
  /* 6: same events after the fix: no violation, a fresh region */
  run(&s, CX9_FIX, 1, 0, "100:1,3000:x,T,3000:1");
  CHECK(s.end == CX9_RUNNING && s.claim_violations == 0 &&
            s.anscp.region == 2,
        "practice_level_6");
  /* 7: fixing only the size line is not enough: the next send_dg call
   * derives ansp2 from the stale res_nsend ans/anssiz pair */
  run(&s, CX9_SIZE_ONLY, 1, 0, "3000:x,T,100:1,3000:2");
  CHECK(s.end == CX9_OVERFLOW && s.calls == 2, "practice_level_7");
  /* 8: options single-request does not help (advisory "Mitigations that
   * don't work"): the model agrees on the same events */
  run(&s, CX9_PRE, 1, 1, "100:1,3000:x,T,3000:1");
  CHECK(s.end == CX9_OVERFLOW && s.single_request_reopen == 1,
        "practice_level_8");
}

int main(int argc, char **argv) {
  test_fixtures(argc > 1 ? argv[1] : "fixtures");
  test_invariants();
  practice();
  printf("%d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}
