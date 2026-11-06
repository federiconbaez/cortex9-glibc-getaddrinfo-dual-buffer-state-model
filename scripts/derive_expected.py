#!/usr/bin/env python3
"""Second, separate derivation of the expectations.

Written from the glibc sources (res_send.c send_dg and __libc_res_nsend at
2c8f75f and at e9db92d, dns-host.c), not from the C model. Buffers are
(name, offset) pairs with a capacity table; nothing is allocated.

Prints fixtures/expected.tsv (per trace and version) followed, after a
'#explore' line, by the depth-5 enumeration counts.
"""
import itertools
import sys

MAXPACKET, USER, HFIXEDSZ = 65536, 2048, 12
VERSIONS = ("pre", "fix", "size_only")


class Resolver:
    def __init__(self, version, dual, single, calls_max=2):
        self.v, self.dual, self.calls_max = version, dual, calls_max
        self.caps = {"USER": USER}
        self.mem = {"anscp": ("USER", 0), "ans": ("USER", 0),
                    "ansp2": None}
        self.size = {"anssiz": USER, "anssiz2": 0}
        self.single, self.reopen = single, False
        self.calls, self.end = 0, None
        self.claims, self.first_claim, self.past, self.steps = 0, 0, 0, 0
        self.enter()

    def enter(self):
        self.calls += 1
        self.orig = self.size["anssiz"]
        self.resplen = 0
        self.retry()

    def retry(self):
        self.r1, self.r2, self.resplen2 = False, not self.dual, 0

    def zero(self):
        if self.dual and self.resplen2 != 0:
            self.end = "RETURNED_ZERO"
        elif self.calls >= self.calls_max:
            self.end = "EXHAUSTED"
        else:
            self.enter()

    def room(self, ref):
        return None if ref is None else self.caps[ref[0]] - ref[1]

    def event(self, ev):
        if self.end:
            return
        self.steps += 1
        if ev == "T":
            if self.resplen > 1 and (self.r1 or (self.dual and self.r2)):
                if not self.single:
                    self.single = True
                    self.retry()
                elif not self.reopen:
                    self.reopen = True
                    self.retry()
                else:
                    self.resplen2, self.end = 1, "RETURNED_ONE"
            else:
                self.zero()
            return
        size, qid = ev.split(":")
        size = int(size)
        qid = {"1": 1, "2": 2, "x": 0}[qid]
        first = (not self.r1 and not self.r2) or not self.dual
        if first:
            sk, pk, rk = "anssiz", "anscp", "resplen"
        else:
            if self.v != "fix":
                base = self.mem["ans"]
                if self.size["anssiz"] != MAXPACKET:
                    self.size["anssiz2"] = self.orig - self.resplen
                    self.mem["ansp2"] = (base[0], base[1] + self.resplen)
                else:
                    self.size["anssiz2"] = self.orig
                    self.mem["ansp2"] = base
            sk, pk, rk = "anssiz2", "ansp2", "resplen2"
        if self.size[sk] < MAXPACKET and self.size[sk] < size:
            name = "HEAP%d" % len(self.caps)
            if len(self.caps) == 8:
                self.end = "MODEL_LIMIT"
                return
            self.caps[name] = MAXPACKET
            self.size["anssiz" if self.v == "pre" else sk] = MAXPACKET
            self.mem[pk] = (name, 0)
        length = self.size[sk]
        room = self.room(self.mem[pk])
        got = min(size, length) if length > 0 else 0
        if length > 0 and length > room:
            self.claims += 1
            self.first_claim = self.first_claim or self.steps
        if length > 0 and got > room:
            self.past, self.end = got - room, "OVERFLOW"
            return
        setattr(self, rk, got)
        if got < HFIXEDSZ:
            self.zero()
            return
        if (self.r1 or qid != 1) and (not self.dual or self.r2 or qid != 2):
            return
        if not self.r1 and qid == 1:
            self.r1 = True
        else:
            self.r2 = True
        if self.r1 and self.r2:
            self.end = "DONE"


ALPHABET = ["100:1", "100:2", "100:x", "1500:1", "1500:2", "1500:x",
            "3000:1", "3000:2", "3000:x", "8:1", "T"]


def explore(version, depth):
    """Count every sequence once, at the step where it ends or at the cut."""
    counts = {"OVERFLOW": 0, "DONE": 0, "cut": 0, "claim": 0}

    def walk(prefix):
        r = Resolver(version, True, False)
        for ev in prefix:
            r.event(ev)
        if r.end or len(prefix) == depth:
            if r.end in counts:
                counts[r.end] += 1
            if not r.end:
                counts["cut"] += 1
            if r.claims:
                counts["claim"] += 1
            return
        for ev in ALPHABET:
            walk(prefix + [ev])

    walk([])
    return counts


def main():
    out = sys.stdout
    for line in open("fixtures/traces.tsv"):
        if line.startswith("#") or not line.strip():
            continue
        tid, dual, single, events = line.split()
        for v in VERSIONS:
            r = Resolver(v, dual == "1", single == "1")
            for ev in events.split(","):
                r.event(ev)
            out.write("%s\t%s\t%s\t%d\t%d\t%d\n" % (
                tid, v, r.end or "RUNNING", r.past, r.claims, r.first_claim))
    out.write("#explore depth=5 dual=1 single=0 calls=2\n")
    for v in VERSIONS:
        c = explore(v, 5)
        out.write("%s\t%d\t%d\t%d\t%d\n" % (v, c["OVERFLOW"], c["DONE"],
                                            c["cut"], c["claim"]))


if __name__ == "__main__":
    main()
