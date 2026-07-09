[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![C++](https://img.shields.io/badge/c++-%2300599C.svg?logo=c%2B%2B&logoColor=white)](https://cppreference.com/)
[![Static Badge](https://img.shields.io/badge/C%2B%2BStandard-C%2B%2B23-green?style=flat&label=C%2B%2BStandard)](https://en.cppreference.com/w/cpp/23.html)
[![CMake](https://img.shields.io/badge/CMake-%23008FBA.svg?logo=cmake&logoColor=white)](https://cmake.org/)
[![Qt](https://img.shields.io/badge/Qt-%23217346.svg?logo=Qt&logoColor=white)](https://www.qt.io/)
[![SQLite](https://img.shields.io/badge/sqlite-%2307405e.svg?logo=sqlite&logoColor=white)](https://sqlite.org/)
[![Visual Studio](https://img.shields.io/badge/Visual%20Studio-5C2D91.svg?logo=visual-studio&logoColor=white)](https://visualstudio.microsoft.com/)
[![Windows](https://img.shields.io/badge/-Windows-6E46A2.svg?logo=data:image/svg+xml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHZpZXdCb3g9IjAgMCA0ODc1IDQ4NzUiPjxwYXRoIGZpbGw9IiNmZmYiIGQ9Ik0wIDBoMjMxMXYyMzEwSDB6bTI1NjQgMGgyMzExdjIzMTBIMjU2NHpNMCAyNTY0aDIzMTF2MjMxMUgwem0yNTY0IDBoMjMxMXYyMzExSDI1NjQiLz48L3N2Zz4=)](https://www.microsoft.com/en-us/windows/windows-11)
[![Linux](https://img.shields.io/badge/-Linux-ad3a90.svg?style=flat&logo=linux&logoColor=white)](https://ubuntu.com/)
[![macOS](https://img.shields.io/badge/-macOS-000000.svg?logo=apple&logoColor=white)](https://www.apple.com/macos/)

# FLibrary - каталогизатор электронной библитотеки

<details>
<summary>Скриншоты</summary>
 <img width="1871" alt="image" src="https://github.com/user-attachments/assets/7b5ef1fe-b3e5-4df7-9852-b1ea67241257" />
 <img width="1871" alt="image" src="https://github.com/user-attachments/assets/fc0501c8-726b-4117-85c9-a5b358ca06d9" />
</details>

[Сравнение с MyHomeLib](doc/compare/mhl.md)

## Сборка

#### Клонируем исходники с сабмодулями
```
git clone https://github.com/heimdallr/books.git --recursive
```

#### Устанавливаем и настраиваем conan
[Инструкция](https://docs.conan.io/2/installation.html)  

#### Устанавливаем модули, которых нет в conan
* Qt6 (6.10.0 минимум, но лучше 6.11) [^4] [^5]  
* 7zip  

### Windows  
Проверялось на Windows 10 и 11, компилятор от MS в средах MSVS2022 и QtCreator

#### Добавляем в PATH пути к: 
* conan.exe  
* cmake.exe, версия cmake должна поддерживать вашу версию MSVS, conan,... короче, берите cmake посвежее  
* git.exe, необязательно, но полезно, позволит в логах видеть хэш текущего коммита  
* Inno Setup, если нужен инсталлятор  

#### Конфигурируем:
В батнике configure.bat поменять пути к зависимостям на ваши, запустить его. Возможно, сработают и другие способы, типа cmake-gui, или открыть в MSVS папку с исходниками.  

#### Собираем:
В результате конфигурирования в папке build будет создан солюшн FLibrary.sln. В нём надо собрать проект FLibrary.  

#### Ещё варианты:
* Можно запустить батник build.bat. Если окружение настроено правильно, то в папке build/installer будут собраны инсталляторы и архив портабельной версии программы.  
* Можно открыть CMakeLists.txt в QtCreator  

### macOS
Скрипт `build-mac.sh` собирает `FLibrary.app`, добавляет HD-иконку, раскладывает Qt/framework зависимости внутрь bundle, подписывает приложение ad-hoc подписью и собирает DMG с `FLibrary.app` и ссылкой `Applications` для установки drag-and-drop.

#### Устанавливаем зависимости
Нужны Xcode Command Line Tools или Xcode, Homebrew, CMake, Ninja, Conan 2, Qt 6, p7zip и librsvg:
```
brew install cmake ninja conan qt qtwebengine qtvirtualkeyboard p7zip librsvg
```

`qtwebengine` и `qtvirtualkeyboard` нужны потому, что часть Qt image/input plugins может ссылаться на `QtPdf` и `QtVirtualKeyboard` frameworks. Скрипт подтягивает эти frameworks в `.app`, если соответствующие плагины попали в bundle.

#### Собираем DMG
Обычная сборка:
```
./build-mac.sh
```

По умолчанию скрипт пробует собрать два DMG:
```
arm64
x86_64
```

Если зависимостей для одной архитектуры нет, она будет пропущена. Это удобно на Apple Silicon: `arm64` собирается из `/opt/homebrew`, а `x86_64` будет пропущен, если Intel Homebrew в `/usr/local` не установлен.

Собрать только Apple Silicon:
```
ARCHS=arm64 ./build-mac.sh
```

Собрать только Intel:
```
ARCHS=x86_64 ./build-mac.sh
```

Требовать, чтобы все запрошенные архитектуры обязательно собрались:
```
ALLOW_MISSING_ARCHS=0 ./build-mac.sh
```

#### Результат
Готовые образы появляются внутри build-каталога. По умолчанию:
```
build/FLibrary-<version>-macOS-arm64.dmg
build/FLibrary-<version>-macOS-x86_64.dmg
```

Если задан `BUILD_DIR`, DMG будет записан в этот build root:
```
BUILD_DIR=out ./build-mac.sh
out/FLibrary-<version>-macOS-<arch>.dmg
```

Отдельный каталог для готовых артефактов можно задать через `ARTIFACT_DIR`.

Внутри DMG лежит `FLibrary.app` и ссылка `Applications`. Пользователь может открыть DMG и перетащить приложение в `Applications`.

#### Сборка x86_64 на Apple Silicon
Для `x86_64` нужны именно `x86_64`-срезы Qt и p7zip. Обычно это Intel Homebrew, установленный под Rosetta в `/usr/local`:
```
arch -x86_64 /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
arch -x86_64 /usr/local/bin/brew install cmake ninja conan qt qtwebengine qtvirtualkeyboard p7zip librsvg
```

После этого можно запускать:
```
./build-mac.sh
```

Если Qt или p7zip лежат нестандартно, пути можно указать явно:
```
QT_PREFIX_X86_64=/usr/local/opt/qt \
P7ZIP_DIR_X86_64=/usr/local/opt/p7zip/lib/p7zip \
./build-mac.sh
```

Для `arm64` аналогичные переменные:
```
QT_PREFIX_ARM64=/opt/homebrew/opt/qt
P7ZIP_DIR_ARM64=/opt/homebrew/opt/p7zip/lib/p7zip
```

#### Проверки, которые делает скрипт
Скрипт сам выполняет:
```
codesign --verify --deep --strict
FLibrary.app/Contents/MacOS/FLibrary --version
hdiutil verify build/FLibrary-<version>-macOS-<arch>.dmg
```

Также bundle проверяется на локальные absolute paths вроде `/opt/homebrew`, `/usr/local` и `/Users`. Такие зависимости не должны оставаться внутри готового DMG.

#### Подпись и notarization
Сейчас используется ad-hoc подпись:
```
codesign --sign -
```

Этого достаточно для локальной проверки bundle, но для публичного релиза через Gatekeeper нужен Developer ID certificate и notarization. Это отдельный release step, не выполняемый `build-mac.sh`.

#### Частые проблемы
* `missing command: rsvg-convert` - установите `librsvg`: `brew install librsvg`.
* `skipping x86_64: Qt with x86_64 slice was not found` - установите Intel Homebrew в `/usr/local` и поставьте `x86_64` Qt/p7zip, либо укажите `QT_PREFIX_X86_64` и `P7ZIP_DIR_X86_64`.
* `Finder layout was skipped` - на headless/CI окружениях Finder может не сохранить фон и позиции иконок. Это warning: DMG все равно содержит `FLibrary.app` и `Applications` symlink.

### Linux
Проверялось на Ubuntu 24.04, компилировалось gcc 15.2, 16.1

##### Убеждаемся в наличии gcc с поддержкой c++23
##### Выполняем команды  
```
cd your/path/to/cloned/repo/books
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DQt6_DIR=your/path/to/Qt6/lib/cmake/Qt6 -G Ninja
cmake --build .
cmake --install .
```
##### Ещё варианты
* Запустить скрипт `build.sh`. В результате в папке build будет создан архив FLibrary-x.y.z-portable-Linux.tar.xz  
* Запустить скрипт с параметром `build.sh DEB`. В папке build будет собран пакет FLibrary-x.y.z-setup-Linux.deb  

#### Проблемы и решения
* При использовании Qt, не собранного статически с libjpeg, возможна runtime-ошибка `qt.gui.imageio.jpeg: Wrong JPEG library version: library is 90, caller expects 62`. Некоторым помогает добавление в начало `start.sh` указания `LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libjpeg.so.62.x.y`, где x.y - ваша версия системной libjpeg.so. Спасибо уважаемому Simply234 за этот workaround.  
* Ошибка munmap_chunk(): invalid pointer при вызове окна выбора каталога. В start.sh добавил строку export QT_QPA_PLATFORMTHEME=generic - тема qt стала по умолчанию и окно выбора каталога перестало падать. К сожалению тему теперь не изменить. Linux Mint Mate 22.3 Zena.  

[^4]: Ну ладно, Qt 6.11 уже есть в conan'е. Но всё равно лучше собрать самостоятельно, с патчами src/home/script/conan/patch/qt. И слинковать с icu из conan.
[^5]: Если есть необходимость запуска на Windows7, можно и с Qt5. Я собирал с 5.15.16
