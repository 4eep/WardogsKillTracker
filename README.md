> **Этот проект полностью создан с использованием нейросети OpenAI Codex.**

# Wardogs Kill Tracker

Windows-приложение для автоматической фиксации убийств в **WARDOGS** во время записи NVIDIA. Программа захватывает изображение монитора через DXGI и подтверждает убийство только при согласованном появлении:

1. новой строки с именем игрока в килфиде;
2. значка подтверждения убийства в центральной части HUD.

Для каждого подтверждённого убийства приложение записывает таймкод и сохраняет PNG-фрагмент соответствующей строки килфида. После окончания записи видео, TXT и изображения перемещаются в отдельную папку с именем видеоролика.

## Возможности

- захват экрана через DXGI без чтения памяти игры;
- распознавание элементов интерфейса с помощью OpenCV;
- совместная проверка килфида и центрального значка;
- защита от повторного учёта строки при сдвиге килфида;
- автоматическое определение начала и конца записи NVIDIA по `Alt+F9` и каталогу записей;
- сохранение таймкодов и снимков килфида;
- инструменты калибровки и проверки на отдельных кадрах.

## Требования

- Windows 10 или Windows 11 x64;
- Visual Studio 2022 или новее с компонентом **Desktop development with C++**;
- CMake 3.24 или новее;
- [vcpkg](https://github.com/microsoft/vcpkg);
- OpenCV 4 и nlohmann-json.

## Сборка

Установите зависимости через vcpkg:

```powershell
C:\vcpkg\vcpkg.exe install opencv4:x64-windows nlohmann-json:x64-windows
```

Настройте и соберите проект из корневого каталога:

```powershell
cmake -S . -B build -A x64 `
  -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Готовая программа находится в `build/Release/WardogsKillTracker.exe`. CMake автоматически копирует рядом `config.json` и каталог `assets`.

## Первоначальная настройка

1. Откройте `config.json` рядом с EXE.
2. Укажите в `nvidia.recordingDirectory` каталог, куда NVIDIA App сохраняет видео.
3. Узнайте индекс нужного монитора:

   ```powershell
   .\WardogsKillTracker.exe --list-monitors
   ```

4. Запишите индекс в `capture.monitorIndex`.
5. Создайте или замените `assets/player_template.png` фрагментом имени игрока из килфида. Для подготовки диагностического кадра можно использовать:

   ```powershell
   .\WardogsKillTracker.exe --create-template
   ```

6. Проверьте области захвата и распознавание командой:

   ```powershell
   .\WardogsKillTracker.exe --calibrate
   ```

Координаты областей задаются долями изображения монитора от `0.0` до `1.0`, поэтому не зависят напрямую от разрешения.

## Использование

Запустите `WardogsKillTracker.exe` до начала записи NVIDIA. Первое нажатие `Alt+F9` начинает сессию, второе завершает её. Приложение продолжает следить за видеофайлом, пока NVIDIA не освободит его, после чего формирует каталог результата:

```text
NVIDIA/Wardogs/recording.mp4
→ NVIDIA/Wardogs/recording/
  ├─ recording.mp4
  ├─ recording.txt
  ├─ 00-13-24-28.png
  └─ 00-18-07-03.png
```

Файл `recording.txt` содержит строки вида:

```text
Kill 00:13:24:28
Kill 00:18:07:03
```

Технический журнал создаётся в `logs/latest.log`. До успешного переноса видео временные результаты остаются в каталоге `output` рядом с программой.

## Диагностика

Проверить один полноэкранный кадр:

```powershell
.\WardogsKillTracker.exe --test-image C:\frames\frame.png
```

Проверить последовательность кадров:

```powershell
.\WardogsKillTracker.exe --test-folder C:\frames\sequence
```

При включённом `detection.debugPreview` приложение показывает отладочные окна OpenCV. Диагностические изображения сохраняются в каталоге `debug`.

## Основные параметры

- `capture.fps` — частота анализа экрана;
- `capture.killFeedRegion` — область килфида;
- `detection.templateThreshold` — порог совпадения имени игрока;
- `detection.hashDistanceThreshold` — допустимое различие строк при подавлении дублей;
- `detection.centerKillConfirmation.templateThreshold` — порог центрального значка;
- `detection.centerKillConfirmation.fusionWindowMs` — максимальный интервал согласования двух источников;
- `timestampOffsetMs` — поправка таймкода в миллисекундах.

Слишком низкие пороги увеличивают число ложных совпадений, а слишком высокие могут пропускать реальные события. Для другого разрешения или масштаба интерфейса сначала выполните калибровку.

## Структура репозитория

```text
assets/       шаблоны OpenCV
src/          исходный код приложения
tests/        автоматические тесты
CMakeLists.txt
config.json   пример конфигурации
```

## Ограничения

Приложение рассчитано на Windows, захват одного монитора и текущий вид HUD WARDOGS. После обновлений игры шаблоны или координаты областей могут потребовать повторной настройки.
