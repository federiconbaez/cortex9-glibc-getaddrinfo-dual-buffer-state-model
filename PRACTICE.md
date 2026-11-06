# Guía práctica (8 niveles)

Ejercicios **locales**: no se usa red ni DNS real. Las respuestas están
comprobadas en `tests/test_model.c` (`practice_level_N`, con ASan/UBSan).
Comando base: `./build/resmodel trace <pre|fix|size_only> <eventos>
[dual] [single]`, después de `sh scripts/build.sh results /tmp/r`.

1. **Control: una sola consulta.** `trace pre 3000:1 0`. ¿Qué región usa
   ahora `anscp` y con qué tamaño? (`practice_level_1`: HEAP1, 65536)
2. **Primera respuesta.** `trace pre 100:1`. ¿Dónde queda y cuánto mide?
   (`practice_level_2`: USER+0, 100)
3. **El resto del buffer.** `trace pre 100:1,1500:x`. ¿Dónde empieza el
   segundo buffer? Mira las líneas 1162-1168. (`practice_level_3`:
   USER+100, 1948)
4. **El tamaño equivocado.** `trace pre 100:1,3000:x`. ¿Qué tamaño queda
   asociado a USER? Mira la línea 1200. (`practice_level_4`: 65536 frente
   a 2048)
5. **El reintento.** `trace pre 100:1,3000:x,T,3000:1`. ¿Cuántos bytes
   quedan fuera de USER? (`practice_level_5`: 952)
6. **El fix.** Repite el nivel 5 con `fix`. (`practice_level_6`: sin
   violación, región nueva)
7. **Contrafactual.** `trace size_only 3000:x,T,100:1,3000:2`. ¿Por qué no
   basta con corregir el tamaño? (`practice_level_7`: la segunda llamada
   deriva `ansp2` del par obsoleto)
8. **"single-request" no mitiga.** `trace pre 100:1,3000:x,T,3000:1 1 1`.
   (`practice_level_8`: desborda igual, como dice el aviso)

Más allá: `explore pre 5` frente a `explore fix 5`, y compara con la
última sección de `fixtures/expected.tsv`.

*English summary: eight local levels from the single-query control to
the cross-call counterfactual; every answer is asserted by the tests.*
