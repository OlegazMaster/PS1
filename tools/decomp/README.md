# Инструменты для доводки функций до совпадения

Небольшие скрипты, которыми сделана большая часть работы над совпадениями.
Запускать из корня репозитория, после `make setup`, `make extract` и обычного
`make` (нужны `asm/`, `build/us/main.elf` и `disc/SCUS_942.44`).

Черновик функции — файл с её C-кодом (комментарий, прототипы и определение),
например `/tmp/f.c`.

| Скрипт | Что делает |
|--------|-----------|
| `odiff.sh FUNC [n]` | Дизассемблирует функцию из оригинала (`/tmp/od_a.txt`) и из `build/us/main.elf` (`/tmp/od_b.txt`) и показывает первые `n` строк разницы. Адреса переходов заменены на `<tgt>`. |
| `python3 putfn.py FILE FUNC < draft` | Ставит функцию из черновика в файл вместо её `INCLUDE_ASM` (или вместо старого определения). Прототипы из начала черновика добавляет один раз. |
| `tryfn.sh FILE FUNC < draft` | `putfn.py` + сборка + `odiff.sh`: печатает `MATCH` или число отличающихся строк. Черновик остаётся в файле. |
| `tryn.sh FILE FUNC < draft` | То же на чистой копии файла: из `git HEAD` или из файла `$BASE`. Сдвинутые `%lo()`-смещения не считаются разницей. Основной инструмент перебора вариантов. |
| `python3 park.py FILE FUNC "заметка"` | Оборачивает определение в `#ifdef NON_MATCHING … #else INCLUDE_ASM … #endif` и дописывает заметку в комментарий. |
| `python3 unpark.py FILE FUNC OUT` | Обратное: заменяет блок `NON_MATCHING` на `INCLUDE_ASM`, а C-код кладёт в `OUT`. |
| `inst.sh FILE FUNC draft` | Ставит совпавший черновик вместо блока `NON_MATCHING` (убирает строку «Equivalent…» из комментария) и проверяет полной сборкой; при несовпадении откатывает файл. |
| `python3 scoreall.py` | Оценивает все функции `NON_MATCHING` (около 20 минут); результат сведён в `docs/nm_status.md`. |
| `python3 declperm.py FILE draft FUNC OUT` | Перебирает порядок объявлений локальных переменных. В GCC 2.8.1 он почти никогда не влияет. |

Нормализованная разница (имена регистров заменены одним):

```sh
N='s/\b(s[0-8]|fp|t[0-9]|a[0-3]|v[01])\b/R/g'
sed -E "$N" /tmp/od_a.txt > /tmp/na.txt; sed -E "$N" /tmp/od_b.txt > /tmp/nb.txt
diff /tmp/na.txt /tmp/nb.txt
```

Типичный цикл:

```sh
python3 tools/decomp/unpark.py src/x.c func_X /tmp/f.c   # взять текущий C
cp src/x.c /tmp/base.c; git checkout src/x.c
BASE=/tmp/base.c tools/decomp/tryn.sh src/x.c func_X < /tmp/f.c
# … правим /tmp/f.c, повторяем …
git checkout src/x.c
tools/decomp/inst.sh src/x.c func_X /tmp/f.c      # когда разница 0
```

Совпадение подтверждает только полная сборка (`make`, «OK, совпадает с
оригиналом»): сравнение по тексту не видит, куда ведут переходы. Перед
коммитом проверьте и `make NON_MATCHING=1` (должна собраться; если ошибка
`.org backwards`, функцию нужно добавить в `config/us/nm_too_long.txt`).
