# Sources

Local copies under `security-research-45/sources/next3/glibc/` (not in
this repo). The copies of `res_send.c` at both commits and of
`dns-host.c` at e9db92d match the GitHub mirror `bminor/glibc` byte for
byte (checked 2026-11-06 by read-only GET).

| Source | Used for | SHA-256 of copy |
|---|---|---|
| glibc commit e9db92d3acfe1822d56d11abcea5bfc4c41cf6ca patch ("CVE-2015-7547: getaddrinfo() stack-based buffer overflow (Bug 18665)", Carlos O'Donell) | causal diff | 632e807415ef39b2395b8494c5e005bf32f4dd49dba794afc2f5b41114423932 |
| `resolv/res_send.c` at parent 2c8f75f79bd6f3f4b3400a9f1a01a75e3086006b | pre: 452-530, 961-1055, 1150-1215, 1231-1233, 1357-1374 | a4972a3de9d6b6b2f88854eab6f3f0199b954a191d12ad7ae8bc477e6572dad3 |
| `resolv/res_send.c` at e9db92d | fix: 1290-1323 | a11b8c7e4b761bcda6c688f458046a4fac22123a36ad2a8b22a4057f906279da |
| `resolv/res_query.c` pre / post | `*nanswerp2 = 0` (post 399, 451), not modelled | 0d1866bf6a8429a5c1d53fca424da837509d0fd8c910c38c4de8fc04eb92b389 / 0aeb8d065a538da0c791f9a733f0f49a18e1f42e9f90c90d1bbed98f58f6f763 |
| `resolv/nss_dns/dns-host.c` pre / post | caller buffer 2048 and `ans2p`/`nans2p` init (pre 307-317) | 5027d6d687b9cc085b0a273019333c7be9584c5e6a128e0768623465651c2a0a / 6e40c32ba2a3e0494f8561d87d0aef0609195dfafad5f02b41f029441c51df69 |
| libc-alpha 2016-02 msg00416, "[PATCH] CVE-2015-7547 --- glibc getaddrinfo() stack-based buffer overflow" | 2048-byte alloca, at most 65535 bytes, single-request not a mitigation | 5917d639f3d541747329618442fe8ce9c23f05702108993ea82addb4f6559674 |
