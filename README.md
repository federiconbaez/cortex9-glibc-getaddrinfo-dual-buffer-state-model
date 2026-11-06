# cortex9-glibc-getaddrinfo-dual-buffer-state-model

Abstract state model of **CVE-2015-7547**: the buffer handling of glibc's
`send_dg` and `__libc_res_nsend` when `getaddrinfo(AF_UNSPEC)` sends A and
AAAA queries in parallel. It covers the code before and after commit
e9db92d. The model is in C, educational, offline and deterministic. It
**opens no socket**, sends and parses no DNS, links no glibc resolver and
writes no buffer. Memory is a list of labelled regions with a capacity
(`USER` 2048, `HEAPn` 65536), and a "write" is a length comparison.

*Versión en español: [README.es.md](README.es.md).*

## What the source shows

Line numbers are for `resolv/res_send.c` at parent 2c8f75f (pre) and at
e9db92d (post).

- `dns-host.c:307-317` passes a 2048-byte `alloca` buffer as `ans` and
  `&host_buffer.ptr` as `anscp`, with `ans2p = NULL` and `nans2p = 0`.
- `send_dg` snapshots `orig_anssizp = *anssizp` on entry (961). When the
  first reply is in, it **derives the second buffer** from `*ansp`
  (1162-1182): the rest of the user buffer, or the whole of it when
  `*anssizp == MAXPACKET`.
- When a datagram does not fit, the pre code mallocs `MAXPACKET`. It then
  sets `*anssizp = MAXPACKET` (**1200**), the *first* buffer's size, even
  when the buffer being replaced is the second one.
- After that, the USER region is paired with the size 65536. A
  `goto retry` (1039, after a timeout with one answer) resets
  `recvresp1`. The next first-buffer `recvfrom` then passes length 65536
  into the 2048-byte buffer.
- **The fix** (post 1290-1323) no longer derives `ansp2` from `*ansp`. It
  updates `*thisanssizp` (1318), reallocates only when
  `thisansp != ansp` (1307), and removes `orig_anssizp`. `res_query.c`
  also resets `*nanswerp2 = 0` (399, 451). That reset is not modelled.

## What the model finds

Every event sequence of the **finite model** up to 7 events was
enumerated, over 11 events: datagrams of 100, 1500 or 3000 bytes
answering query 1, query 2 or neither; an 8-byte undersized datagram;
and a timeout. Two `send_dg` calls are allowed. This covers the model's
alphabet only, **not the real resolver, network or kernel**.

| variant | overflow sequences | claim > room | shortest overflow |
|---|---|---|---|
| pre | 119,172 | 544,920 | `100:1,3000:1,T,3000:1` (952 past USER) |
| fix | **0** | **0** | none |
| size-only counterfactual (pre + `*thisanssizp`) | 52,776 | 184,560 | `3000:x,8:1,100:1,3000:1` |
| control: one query (`buf2 == NULL`), all versions | 0 | 0 | none |

The table is from `results/explore.csv`. Sequences still running at the
bound are counted in `cut_at_depth` (MODEL_LIMIT). No run reached the
region bound. The run with `options single-request` set from the start
also overflows. This matches the advisory's "Mitigations that don't work".

**Model-derived observation (not stated in the advisory):** changing only
line 1200 to `*thisanssizp` still overflows in the model. After a
first-buffer reallocation, res_nsend's `ans` still points at USER while
`anssiz` is 65536. The *next* `send_dg` call snapshots 65536 and sets
`ansp2 = *ansp`. The upstream fix also removed that derivation.

`scripts/derive_expected.py` is a separate Python implementation. It
reproduces every fixture result and the exact depth-5 enumeration
counts, and CI checks both.

## Usage

```sh
sh scripts/build.sh test        # -Wall -Wextra -Werror, ASan/UBSan
sh scripts/build.sh results /tmp/r
./build/resmodel trace pre 100:1,3000:x,T,3000:1
./build/resmodel explore fix 7
```

See [LIMITATIONS.md](LIMITATIONS.md), [SOURCES.md](SOURCES.md) and
[PRACTICE.md](PRACTICE.md) (Spanish). License: Apache-2.0.
