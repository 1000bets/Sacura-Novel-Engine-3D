# Распространение редактора и игры

Редактор и Player используют React + Electron. Qt, старые SakuraEditor/SakuraPlayer и Qt MCP bridge удалены.

## Сборка редактора

Из каталога desktop-electron:

```powershell
npm run native:configure:release
npm run desktop:production
```

Команды собирают SakuraElectronHost, React и Electron-пакет. Установленный редактор экспортирует игры без компилятора и системного Python. Debug предназначен для разработки и проверок; распространяйте Release.

## Экспорт игры

Сохраните сцену, историю и открытые редакторы ресурсов, остановите Play. Выберите File → Export game и новую папку вне проекта и runtime. Существующая папка никогда не перезаписывается. Экспорт использует сохранённые данные, проверяет внешние изменения дескриптора и не сохраняет черновики автоматически.

```text
MyGame/
  Game.exe
  Electron runtime files
  resources/
    app/       React Player, preload and typed bridge
    native/    SakuraElectronHost.exe, Engine, Python, native DLLs
    Game/      Game.project, Content, Config
```

Передавайте всю папку. Player открывает resources/Game/Game.project; параметр `--project` задаёт другой дескриптор. Content/Scripts и метаданные остаются редактируемыми. Saved, кеши и системные Python site-packages не включаются. Неизвестные поля дескриптора сохраняются. Экспорт сначала собирается рядом с назначением и публикуется после успешного копирования.

Player отображает диалоги, выборы, паузу и завершение через React; симуляция и StoryRuntime работают в нативном движке. Команды редактирования в игровом режиме запрещены. Enhanced Input получает клавиатуру, мышь и стандартные геймпады от Electron.

## Проверки

Нативные тесты: CTest `Engine.IO.Services` и `Editor.AssetCreationFactories`. Интеграционные проверки находятся в desktop-electron/tests/native-bridge.test.cjs, включая Narrative, префабы, Player и экспорт. Для собственного каталога сборки задайте SACURA_NATIVE_HOST абсолютным путём к SakuraElectronHost.exe.

Проектный helper `Game.exe --player-smoke` с SACURA_PLAYER_TEST_OUTPUT проверяет React IPC, GPU-кадры, паузу, диалоги и размеры окна, сохраняет отчёт и изображения. Это диагностический режим; обычный Player не продвигает диалоги автоматически.
