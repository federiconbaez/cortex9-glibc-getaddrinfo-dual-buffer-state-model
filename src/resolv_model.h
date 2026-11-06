/* Abstract state model of the dual-query UDP buffer handling in glibc's
 * send_dg / __libc_res_nsend (CVE-2015-7547), before and after e9db92d.
 * Memory is a list of labelled regions with a capacity; nothing is
 * allocated, read or written. "Writes" are length comparisons. */
#ifndef CX9_RESOLV_MODEL_H
#define CX9_RESOLV_MODEL_H

#define CX9_MAXPACKET 65536 /* res_send.c:104 */
#define CX9_USER_CAP 2048   /* dns-host.c:307 alloca (2048) */
#define CX9_HFIXEDSZ 12     /* undersized test, res_send.c:1221 */
#define CX9_MAX_REGIONS 8   /* model bound on malloc(MAXPACKET) calls */

typedef enum { CX9_PRE = 0, CX9_FIX = 1, CX9_SIZE_ONLY = 2 } cx9_version;

typedef enum {
  CX9_RUNNING = 0,
  CX9_OVERFLOW,        /* recvfrom result bytes exceed the region */
  CX9_DONE,            /* both answers: send_dg returns resplen */
  CX9_RETURNED_ONE,    /* timeout after retries: *resplen2 = 1, return */
  CX9_RETURNED_ZERO,   /* res_nsend returns 0 without trying again */
  CX9_EXHAUSTED,       /* every send_dg call returned 0 */
  CX9_MODEL_LIMIT      /* region bound reached: the model stops */
} cx9_end;

typedef struct { int region; int off; } cx9_ptr; /* region -1 = NULL */

typedef struct {
  cx9_version version;
  int dual;                   /* buf2 != NULL (AF_UNSPEC A + AAAA) */
  int cap[CX9_MAX_REGIONS];   /* region 0 = USER 2048 */
  int nregions;
  cx9_ptr anscp;              /* caller's host_buffer.ptr (anscp) */
  cx9_ptr ans;                /* res_nsend local ans (send_dg *ansp) */
  int anssiz;                 /* res_nsend local anssiz (*anssizp) */
  cx9_ptr ansp2;              /* caller's ans2p (*ansp2) */
  int anssiz2;                /* caller's nans2p (*anssizp2) */
  int orig_anssizp, resplen, resplen2, recvresp1, recvresp2;
  int single_request, single_request_reopen;
  int calls, calls_max, steps;
  cx9_end end;
  int claim_violations;       /* recvfrom length > room in region */
  int first_claim_step;       /* 1-based step, 0 = none */
  int past_end;               /* bytes past the region at CX9_OVERFLOW */
  int last_claim, last_room;
} cx9_state;

/* id: 1 answers query 1, 2 answers query 2, 0 matches neither. */
typedef struct { int timeout; int size; int id; } cx9_event;

void cx9_init(cx9_state *s, cx9_version v, int dual, int single,
              int calls_max);
void cx9_step(cx9_state *s, cx9_event e);
int cx9_parse_event(const char *tok, cx9_event *e);
const char *cx9_end_name(cx9_end e);
const char *cx9_version_name(cx9_version v);
int cx9_room(const cx9_state *s, cx9_ptr p); /* -1 for NULL */
#endif
