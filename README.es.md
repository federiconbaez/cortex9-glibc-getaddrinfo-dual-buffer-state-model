# cortex9-glibc-getaddrinfo-dual-buffer-state-model

Modelo de estado abstracto, en C, de **CVE-2015-7547**: la gestión de
buffers de `send_dg` y `__libc_res_nsend` de glibc con consultas A y AAAA
en paralelo, antes y después de e9db92d. Educativo y offline. **No abre
sockets**, no envía ni analiza DNS, no enlaza el resolver y no escribe
buffers. La memoria son regiones etiquetadas con capacidad (`USER` 2048,
`HEAPn` 65536).

## El fallo, según la fuente

- **Segundo buffer:** el código previo al fix lo deriva de `*ansp`
  (líneas 1162-1182).
- **Tamaño equivocado:** al reasignar, actualiza `*anssizp` (1200), el
  tamaño del **primer** buffer, aunque el buffer sustituido sea el segundo.
- **Desbordamiento:** tras un `goto retry` (1039), el siguiente `recvfrom`
  pasa una longitud de 65536 al buffer de 2048.
- **El fix:** usa `*thisanssizp`, solo reasigna si `thisansp != ansp` y ya
  no deriva `ansp2` de `*ansp`.

## Resultados (modelo finito, hasta 7 eventos)

| variante | secuencias con desbordamiento | longitud > espacio |
|---|---|---|
| antes del fix | 119.172 | 544.920 |
| fix | 0 | 0 |
| contrafactual (solo `*thisanssizp`) | 52.776 | 184.560 |
| control con una sola consulta | 0 | 0 |

- Solo se enumeran las secuencias del alfabeto del modelo, no del
  resolver real.
- **Observación derivada del modelo**, no del aviso: corregir solo la
  línea 1200 no basta. En la siguiente llamada, `ansp2 = *ansp` vuelve a
  apuntar a USER con 65536.
- `scripts/derive_expected.py` es una implementación independiente en
  Python. Reproduce los fixtures y los conteos exactos de la enumeración a
  profundidad 5.

Ver [LIMITATIONS.md](LIMITATIONS.md), [SOURCES.md](SOURCES.md) y
[PRACTICE.md](PRACTICE.md). Licencia: Apache-2.0.
