# ПК-порт на Windows

Порт — обычная программа для Windows: `crash3.exe` и `SDL2.dll`. Окно,
клавиатура и геймпад работают через SDL2. Нужна **64-битная** Windows: порт
32-битный и держит память PlayStation по её адресу `0x80000000`, а такие
адреса 32-битная программа получает только в 64-битной системе.

Собирается порт в WSL (Linux внутри Windows): там нужны и инструменты
PlayStation (из вашего исполняемого файла извлекается код, адреса берутся из
совпадающей сборки), и кросс-компилятор MinGW, который делает `.exe` для
Windows. Готовый `crash3.exe` запускается уже в самой Windows.

## 1. WSL

В PowerShell от администратора:

```
wsl --install -d Ubuntu
```

После перезагрузки откройте «Ubuntu» из меню «Пуск» и задайте имя и пароль.

## 2. Сборка (в Ubuntu)

```sh
sudo apt update
sudo apt install binutils-mipsel-linux-gnu python3-venv git curl make cpp gcc-mingw-w64-i686

git clone https://github.com/OlegazMaster/PS1.git
cd PS1
git checkout claude/crash-bandicoot-3-pc-port-fhrc36
make setup

# исполняемый файл с вашего диска (диски Windows видны как /mnt/c, /mnt/d, ...):
python3 tools/disc_image.py "/mnt/d/Games/Crash 3/Crash Bandicoot 3.cue"

make extract
make             # должно напечатать «OK, совпадает с оригиналом»
make pc-windows  # build/pc-win/crash3.exe и SDL2.dll (SDL2 скачивается один раз)

# в папку Windows, например C:\Games\crash3:
mkdir -p /mnt/c/Games/crash3
cp build/pc-win/crash3.exe build/pc-win/SDL2.dll /mnt/c/Games/crash3/
```

## 3. Запуск (в Windows)

Положите образ своего диска (`.cue` вместе с его `.bin`, или `.iso`) в ту же
папку, что `crash3.exe`, и запустите `crash3.exe` двойным щелчком: он сам
найдёт образ рядом. Или из командной строки, с путём к образу:

```
C:\Games\crash3> crash3.exe "D:\Games\Crash 3\Crash Bandicoot 3.cue"
```

Управление: стрелки; Z — крестик (прыжок), X — квадрат (вращение), C — круг,
S — треугольник, Enter — Start, Backspace — Select, Q/W — L1/R1, A/E — L2/R2.
Геймпад (Xbox и совместимые) подключается сам.

## Что сейчас работает

Порт в разработке. Готовы чтение диска, графика (программный GPU), геймпад,
таймеры. Звука и карты памяти пока нет. В окне консоли порт пишет, какие
функции PlayStation ещё не реализованы (`port: ... is not implemented yet`).
Если игра остановится или упадёт, пришлите этот вывод — по нему видно, что
делать дальше.
