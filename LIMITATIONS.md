# Limitations

- **Finite model, finite alphabet.** The enumeration covers every sequence
  of the model's 11 events up to the depth bound (7 in `results/`).
  - It is not an enumeration of real resolver, network or kernel
    behaviour.
  - Sequences still running at the bound are reported as `cut_at_depth`
    (MODEL_LIMIT), not as safe.
  - Overflow bytes are bounded by the largest alphabet size (3000), so 952
    is a model number. The length argument is 65536, and the advisory
    states up to 65535 bytes.
- **UDP `send_dg` only.**
  - `send_vc` (TCP) is not modelled. It has its own pre-fix derivation of
    the second buffer (782-797), although its realloc already updated
    `*thisanssizp` (817).
  - EDNS0, FORMERR, SERVFAIL / next_ns, TC→TCP, the hooks and
    `res_queriesmatch` details are not modelled. A datagram either answers
    query 1, answers query 2, or is ignored.
- **Assumptions.**
  - x86-style `_STRING_ARCH_unaligned`: no alignment of `resplen`.
  - FIONREAD reports the datagram size, and `malloc` never fails.
  - Two `send_dg` calls by default, a model parameter for
    `retry × nscount`. Upstream defaults were not re-verified here.
- **Caller-level parts of the fix not modelled.** The `*nanswerp2 = 0`
  resets in `res_query.c` and the search-domain loop are out of scope.
- **Fix pair not fully coupled.** After the fix, res_nsend's own
  `ans`/`anssiz` pair can still disagree after a first-buffer
  reallocation. Post-fix `send_dg` never writes through `*ansp`, and the
  model checks only the pairs passed to `recvfrom`.
- **Same model, two implementations.** The Python and C implementations
  agree with each other, which checks the transcription. Neither runs
  glibc.
- This is not a new finding. The "size-only" result is a property of this
  model.
- **Input domain:** `cx9_explore` accepts depth 0..10 and the three known
  versions; anything else returns -1 with a zeroed result (the depth is
  never clamped). `cx9_room` returns `CX9_ROOM_INVALID` without reading
  `cap[]` for a region outside the live regions or an offset outside
  0..cap. The CLI parses numbers with checked `strtol`.
