[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![C++](https://img.shields.io/badge/c++-%2300599C.svg?logo=c%2B%2B&logoColor=white)](https://cppreference.com/)
[![Static Badge](https://img.shields.io/badge/C%2B%2BStandard-C%2B%2B23-green?style=flat&label=C%2B%2BStandard)](https://en.cppreference.com/w/cpp/23.html)
[![CMake](https://img.shields.io/badge/CMake-%23008FBA.svg?logo=cmake&logoColor=white)](https://cmake.org/)
[![Qt](https://img.shields.io/badge/Qt-%23217346.svg?logo=Qt&logoColor=white)](https://www.qt.io/)
[![SQLite](https://img.shields.io/badge/sqlite-%2307405e.svg?logo=sqlite&logoColor=white)](https://sqlite.org/)
[![Visual Studio](https://img.shields.io/badge/Visual%20Studio-5C2D91.svg?logo=visual-studio&logoColor=white)](https://visualstudio.microsoft.com/)
[![Windows](https://img.shields.io/badge/-Windows-6E46A2.svg?logo=data:image/svg+xml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHZpZXdCb3g9IjAgMCA0ODc1IDQ4NzUiPjxwYXRoIGZpbGw9IiNmZmYiIGQ9Ik0wIDBoMjMxMXYyMzEwSDB6bTI1NjQgMGgyMzExdjIzMTBIMjU2NHpNMCAyNTY0aDIzMTF2MjMxMUgwem0yNTY0IDBoMjMxMXYyMzExSDI1NjQiLz48L3N2Zz4=)](https://www.microsoft.com/en-us/windows/windows-11)
[![Linux](https://img.shields.io/badge/-Linux-ad3a90.svg?style=flat&logo=linux&logoColor=white)](https://ubuntu.com/)

# FLibrary

<details>
<summary>Скриншоты</summary>
 <img width="1871" alt="image" src="https://github.com/user-attachments/assets/7b5ef1fe-b3e5-4df7-9852-b1ea67241257" />
 <img width="1871" alt="image" src="https://github.com/user-attachments/assets/fc0501c8-726b-4117-85c9-a5b358ca06d9" />
</details>

Замена для [MyHomeLib](https://github.com/OleksiyPenkov/MyHomeLib)

# Сборка  

#### Клонируем исходники с сабмодулями
```
git clone https://github.com/heimdallr/books.git --recursive
```

#### Устанавливаем и настраиваем conan
[Инструкция](https://docs.conan.io/2/installation.html)  

#### Устанавливаем модули, которых нет в conan
* Qt6 (6.10.0 минимум, но лучше 6.11.0) [^4] [^5]  
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

### Linux
Проверялось на Ubuntu 24.04, компилировалось gcc15.2

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
* Запустить скрипт `build.sh`. В результате в папке build будет создан архив FLibrary-x.y.z-portable.tar.xz  
* Запустить скрипт с параметром `build.sh DEB`. В папке build будет собран пакет FLibrary-x.y.z-setup.deb  

[^4]: Имеет смысл собрать самостоятельно, применив патчи src/home/script/conan/patch/qt, линкуя с icu из conan.
[^5]: Если есть необходимость запуска на Windows7, можно и с Qt5. Я собирал с 5.15.16
