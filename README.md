<h1 align="center">SANDWAVE</h1>
<p align="center"><b>Airy sand-top VST3 for vocals</b> — air, sparkle and falling grains of sand in the highs.</p>

---

## Что это

SANDWAVE поднимает верха и добавляет вокалу «песчаный воздух»: яркие, блестящие верха, в которых как будто сыплются мелкие зёрна.
Плагин сделан на **JUCE 8 (C++17)**, форматы **VST3** (+ Standalone для тестов). Весь интерфейс нарисован кодом — картинок и ассетов нет.

## Звуковая цепочка

```
in ─┬─► Mid shelf 4.2k ─► High shelf 11.5k ─► (минус in) ───────────┐  AIR
    ├─► HP 5.2k ─► 2x oversampled soft-sat (чёт+нечёт) ─► HP ──────┤  EXCITE
    ├─► HP 6k ─► envelope ─► гейт «зёрен» полосового шума ─────────┤  SAND
    └─► BP 7.2k ─► envelope ─► gain reduction на всё, что добавили ┘  DE-HARSH

out = (in + extras · mix · guard) · output
```

| Блок | Что делает |
|---|---|
| **MID AIR / HIGH AIR** | Два мягких high-shelf (до +9 / +12 dB) — основная яркость |
| **EXCITE** | Гармонический эксайтер на верхней полосе (оверсемплинг ×2), добавляет настоящие верхние гармоники |
| **SAND** | Гранулярный «песок»: узкополосный шум, гейтится огибающей верхов вокала, поэтому сыплется в такт голосу |
| **GRAIN** | Плотность зёрен (300…7300 в секунду) |
| **TONE** | Частота/цвет песка (3–16 кГц) |
| **WIDTH** | Стерео-разброс добавленных верхов (шум независим по каналам + M/S) |
| **DE-HARSH** | Динамическая защита от шипящих — не даёт верхам превратиться в «ножи» |
| **MIX / OUTPUT / POWER** | Параллельный микс, выход, плавный байпас |

7 фабричных пресетов: Default, Silk Air, Desert Vocal, Neon Dust, Sandstorm, Fine Grain, Master Sparkle.

## Дизайн

Белое матовое стекло, ледяной синий, фиолетовый и неоновые акценты. В центре — окно с песчаными дюнами, северным сиянием и мерцающим «воздухом»: песок падает тем гуще, чем больше SAND и чем ярче верха в сигнале. Ручки — «галька» с зернистой фактурой и неоновым кольцом, AIR — вертикальные слайдеры-дюны, питание — кристалл-гексагон. Окно масштабируется.

## Сборка

Нужны: **CMake ≥ 3.22**, компилятор C++17 (VS 2022 / Xcode / gcc), Git. JUCE подтянется сам.

```bash
git clone <your-repo-url> SANDWAVE
cd SANDWAVE
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
```

Готовый плагин: `build/SANDWAVE_artefacts/Release/VST3/SANDWAVE.vst3`

Автокопирование в системную папку VST3 (на Windows нужны права администратора):

```bash
cmake -B build -DSANDWAVE_COPY_AFTER_BUILD=ON
```

Вручную: Windows — `C:\Program Files\Common Files\VST3\`, macOS — `/Library/Audio/Plug-Ins/VST3/`, Linux — `~/.vst3/`.
В FL Studio: *Options → Manage plugins → Find plugins*.

## GitHub Actions

`.github/workflows/build.yml` собирает VST3 под Windows, macOS (arm64 + x86_64) и Linux на каждый push.
Чтобы выпустить релиз: `git tag v1.0.0 && git push --tags` — на странице Releases появятся zip-архивы.

## Настройка звука

Все «крутилки характера» — константы в начале `Source/SandEngine.h`:
`kMidShelfMaxDb`, `kHighShelfMaxDb`, `kExciteMaxGain`, `kSandGain`, `kGrainMinPerSec`, `kGrainMaxPerSec`.
Частоты шельфов и фильтров — в `updateShelves()` и `prepare()`.

## Структура

```
Source/
  SandEngine.h        DSP целиком
  PluginProcessor.*   параметры, пресеты, состояние
  PluginEditor.*      раскладка и отрисовка интерфейса
  SandLookAndFeel.h   ручки, слайдеры, кнопка, комбобокс
  SandVisualizer.h    анимация песка и дюн
  Style.h             палитра, шрифты, зернистая текстура
```

## Лицензия

Код SANDWAVE — MIT (см. `LICENSE`). Плагин использует **JUCE** (AGPLv3 / коммерческая лицензия): при распространении собранных бинарников соблюдайте лицензию JUCE.

## Roadmap

- Спектр-анализатор в окне дюн
- Собственные пресеты пользователя
- AU / AAX, Windows-инсталлятор
- Линейно-фазовый режим для мастеринга
