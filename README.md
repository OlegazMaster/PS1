# Crash Bandicoot 3: Warped — декомпиляция

Цель проекта — нативный ПК-порт Crash Bandicoot 3 без эмуляции.

Путь к нему тот же, что у портов Super Mario 64 и The Legend of Zelda: Ocarina of Time:

1. **Декомпиляция.** Восстановить исходный код игры на C из исполняемого файла
   PlayStation (`SCUS_942.44`). Каждая функция переписывается так, чтобы
   оригинальный компилятор PSY-Q выдавал из неё **те же байты**, что на диске.
   Совпадение проверяется сборкой: `make` пересобирает исполняемый файл и сверяет
   его SHA-1 с оригиналом.
2. **Порт.** Когда весь код игры станет исходником на C, его можно собрать
   обычным компилятором под ПК. Библиотеки PlayStation (графика, звук, CD,
   геймпад) при этом заменяются реализациями на SDL2/OpenGL.

Подробный план — в [docs/roadmap.md](docs/roadmap.md).

## Состояние

| Этап | Статус |
|------|--------|
| Инфраструктура: тулчейн, нарезка, сборка, проверка, прогресс | ✅ готово |
| Первая нарезка `SCUS_942.44` и совпадающая сборка | ⏳ нужен исполняемый файл игры |
| Декомпиляция кода | 0% |
| ПК-порт | не начат |

Параметры компилятора выставлены предварительно: GCC 2.7.2 (PSY-Q), ASPSX 2.56, `-G0`.
Они будут уточнены по первым совпавшим функциям.

## Что нужно

- **Своя копия игры.** Репозиторий не содержит ни кода, ни ресурсов игры
  и никогда не будет их содержать. Всё извлекается из вашего диска локально,
  а папки `disc/`, `asm/`, `assets/` и `build/` исключены из git.
- **Linux** (Ubuntu/Debian) или **WSL** на Windows.

## Быстрый старт

```sh
sudo apt install binutils-mipsel-linux-gnu python3-venv git curl make cpp
make setup      # скачивает компилятор PSY-Q GCC, maspsx, splat, m2c, asm-differ

# скопируйте SCUS_942.44 с вашего диска в disc/ (см. disc/README.md)
make init       # создаёт config/us/splat.yaml по вашему файлу (один раз)
make extract    # нарезает исполняемый файл на asm/ и заготовки src/
make            # собирает build/us/SCUS_942.44 и сверяет с оригиналом
make progress   # сколько кода уже на C
```

Как декомпилировать функции, описано в [docs/decompiling.md](docs/decompiling.md).

## Устройство репозитория

```
config/us/        конфигурация splat, известные символы, контрольная сумма
disc/             ваш исполняемый файл игры (не коммитится)
include/          общие заголовки
src/              декомпилированный код на C
asm/, assets/     генерируются `make extract` (не коммитятся)
build/            результат сборки (не коммитится)
tools/            скрипты проекта; tools/toolchain/ скачивается `make setup`
tests/            тесты скриптов проекта (`make test`)
docs/             документация
```

Используемые инструменты: [splat](https://github.com/ethteck/splat) (нарезка),
[old-gcc](https://github.com/decompals/old-gcc) (компилятор PSY-Q),
[maspsx](https://github.com/mkst/maspsx) (ассемблер в стиле ASPSX),
[m2c](https://github.com/matt-kempster/m2c) (черновики функций на C),
[asm-differ](https://github.com/simonlindholm/asm-differ) (сравнение с оригиналом).

## Юридическое

Проект не связан с Sony Interactive Entertainment, Activision или Naughty Dog.
Crash Bandicoot — их товарный знак. Для работы нужна законно приобретённая копия
игры. Официальная ПК-версия Warped входит в Crash Bandicoot N. Sane Trilogy.
