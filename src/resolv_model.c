#include "resolv_model.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *cx9_end_name(cx9_end e) {
  static const char *n[] = {"RUNNING", "OVERFLOW", "DONE", "RETURNED_ONE",
                            "RETURNED_ZERO", "EXHAUSTED", "MODEL_LIMIT"};
  if ((int)e < 0 || (int)e >= (int)(sizeof n / sizeof n[0])) return "UNKNOWN";
  return n[e];
}
const char *cx9_version_name(cx9_version v) {
  static const char *n[] = {"pre", "fix", "size_only"};
  if ((int)v < 0 || (int)v >= (int)(sizeof n / sizeof n[0])) return "unknown";
  return n[v];
}

int cx9_room(const cx9_state *s, cx9_ptr p) {
  /* validate before indexing cap[]: never read outside the live regions */
  if (!s || s->nregions < 1 || s->nregions > CX9_MAX_REGIONS)
    return CX9_ROOM_INVALID;
  if (p.region == -1) return CX9_ROOM_NULL;
  if (p.region < 0 || p.region >= s->nregions) return CX9_ROOM_INVALID;
  if (p.off < 0 || p.off > s->cap[p.region]) return CX9_ROOM_INVALID;
  return s->cap[p.region] - p.off;
}

/* send_dg entry (res_send.c:961, 983-997) and the retry label. */
static void retry_reset(cx9_state *s) {
  s->recvresp1 = 0;
  s->recvresp2 = !s->dual; /* recvresp2 = buf2 == NULL */
  s->resplen2 = 0;
}
static void send_dg_entry(cx9_state *s) {
  s->calls++;
  s->orig_anssizp = s->anssiz; /* int orig_anssizp = *anssizp; */
  s->resplen = 0;
  retry_reset(s);
}

void cx9_init(cx9_state *s, cx9_version v, int dual, int single,
              int calls_max) {
  memset(s, 0, sizeof *s);
  s->version = v;
  s->dual = dual;
  s->cap[0] = CX9_USER_CAP;
  s->nregions = 1;
  s->anscp.region = 0;          /* &host_buffer.ptr -> 2048 buffer */
  s->ans.region = 0;            /* res_nsend ans = host_buffer.buf */
  s->anssiz = CX9_USER_CAP;     /* anssiz = 2048 (dns-host.c:316) */
  s->ansp2.region = -1;         /* u_char *ans2p = NULL (dns-host.c:308) */
  s->anssiz2 = 0;               /* int nans2p = 0 (dns-host.c:309) */
  s->single_request = single;
  s->single_request_reopen = 0;
  s->calls_max = calls_max;
  send_dg_entry(s);
}

/* send_dg returned 0: res_nsend line 525 decides whether to go to next_ns. */
static void returned_zero(cx9_state *s) {
  if (s->dual && s->resplen2 != 0) { /* (buf2 == NULL || *resplen2 == 0) */
    s->end = CX9_RETURNED_ZERO;
    return;
  }
  if (s->calls >= s->calls_max) {
    s->end = CX9_EXHAUSTED;
    return;
  }
  send_dg_entry(s);
}

static void timeout(cx9_state *s) {
  /* res_send.c:1020-1055 */
  if (s->resplen > 1 && (s->recvresp1 || (s->dual && s->recvresp2))) {
    if (!s->single_request) {
      s->single_request = 1;
      retry_reset(s);          /* goto retry */
      return;
    }
    if (!s->single_request_reopen) {
      s->single_request_reopen = 1;
      retry_reset(s);          /* goto retry_reopen */
      return;
    }
    s->resplen2 = 1;
    s->end = CX9_RETURNED_ONE;
    return;
  }
  returned_zero(s);            /* *gotsomewhere = 1; return (0); */
}

static void datagram(cx9_state *s, int size, int id) {
  int *sizp, *rlp;
  cx9_ptr *ptrp;
  int realloc_ok, claim, room, n;

  if ((s->recvresp1 | s->recvresp2) == 0 || !s->dual) {
    sizp = &s->anssiz;         /* thisanssizp = anssizp */
    ptrp = &s->anscp;          /* thisansp = anscp ?: ansp */
    rlp = &s->resplen;
  } else {
    /* The fix (post 1296-1300) uses *ansp2 / *anssizp2 as given. */
    if (s->version != CX9_FIX) {
      /* pre lines 1162-1182 (_STRING_ARCH_unaligned path) */
      if (s->anssiz != CX9_MAXPACKET) {
        s->anssiz2 = s->orig_anssizp - s->resplen;
        s->ansp2 = s->ans;
        s->ansp2.off += s->resplen;   /* *ansp + resplen */
      } else {
        s->anssiz2 = s->orig_anssizp;
        s->ansp2 = s->ans;            /* *ansp2 = *ansp */
      }
    }
    sizp = &s->anssiz2;
    ptrp = &s->ansp2;
    rlp = &s->resplen2;
  }

  /* realloc test: pre 1189-1196 (anscp is non-NULL for getaddrinfo);
   * fix 1305-1312 adds thisansp != ansp, always true here because
   * thisansp is anscp or ansp2. FIONREAD reports the datagram size. */
  realloc_ok = *sizp < CX9_MAXPACKET && *sizp < size;
  if (realloc_ok) {
    if (s->nregions == CX9_MAX_REGIONS) {
      s->end = CX9_MODEL_LIMIT;
      return;
    }
    s->cap[s->nregions] = CX9_MAXPACKET;
    if (s->version == CX9_PRE)
      s->anssiz = CX9_MAXPACKET;      /* pre 1200: *anssizp = MAXPACKET */
    else
      *sizp = CX9_MAXPACKET;          /* fix 1318: *thisanssizp */
    ptrp->region = s->nregions++;
    ptrp->off = 0;                    /* *thisansp = newp */
  }

  /* recvfrom(fd, *thisansp, *thisanssizp, ...) */
  claim = *sizp;
  room = cx9_room(s, *ptrp);
  if (room == CX9_ROOM_INVALID) {     /* not reachable from cx9_init/step */
    s->end = CX9_MODEL_LIMIT;
    return;
  }
  s->last_claim = claim;
  s->last_room = room;
  n = size < claim ? size : claim;
  if (claim > 0 && claim > room) {
    s->claim_violations++;
    if (!s->first_claim_step) s->first_claim_step = s->steps;
  }
  if (claim > 0 && n > room) {
    s->past_end = n - room;
    s->end = CX9_OVERFLOW;
    return;
  }
  if (claim <= 0) n = 0;
  *rlp = n;
  if (n <= 0) {                       /* *thisresplenp <= 0: err_out */
    returned_zero(s);
    return;
  }
  if (n < CX9_HFIXEDSZ) {             /* undersized: err_out */
    returned_zero(s);
    return;
  }
  /* res_send.c:1231-1233: response from old query, ignore it */
  if ((s->recvresp1 || id != 1) && (!s->dual || s->recvresp2 || id != 2))
    return;
  /* 1358-1361 */
  if (s->recvresp1 == 0 && id == 1)
    s->recvresp1 = 1;
  else
    s->recvresp2 = 1;
  if (s->recvresp1 & s->recvresp2)
    s->end = CX9_DONE;
}

void cx9_step(cx9_state *s, cx9_event e) {
  if (s->end != CX9_RUNNING) return;
  s->steps++;
  if (e.timeout)
    timeout(s);
  else
    datagram(s, e.size, e.id);
}

int cx9_parse_event(const char *tok, cx9_event *e) {
  char *end;
  long v;
  memset(e, 0, sizeof *e);
  if (strcmp(tok, "T") == 0) {
    e->timeout = 1;
    return 0;
  }
  v = strtol(tok, &end, 10);
  if (end == tok || *end != ':' || v < 1 || v > CX9_MAXPACKET) return -1;
  e->size = (int)v;
  if (strcmp(end + 1, "1") == 0) e->id = 1;
  else if (strcmp(end + 1, "2") == 0) e->id = 2;
  else if (strcmp(end + 1, "x") == 0) e->id = 0;
  else return -1;
  return 0;
}
