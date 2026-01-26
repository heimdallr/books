[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![CMake](https://img.shields.io/badge/CMake-%23008FBA.svg?logo=cmake&logoColor=white)](https://cmake.org/)
[![C++](https://img.shields.io/badge/c++-%2300599C.svg?logo=c%2B%2B&logoColor=white)](https://cppreference.com/)
[![Static Badge](https://img.shields.io/badge/C%2B%2BStandard-C%2B%2B23-green?style=flat&label=C%2B%2BStandard)](https://en.cppreference.com/w/cpp/23.html)
[![SQLite](https://img.shields.io/badge/sqlite-%2307405e.svg?logo=sqlite&logoColor=white)](https://sqlite.org/)
[![Qt](https://img.shields.io/badge/Qt-%23217346.svg?logo=Qt&logoColor=white)](https://www.qt.io/)
[![Visual Studio](https://img.shields.io/badge/Visual%20Studio-5C2D91.svg?logo=visual-studio&logoColor=white)](https://visualstudio.microsoft.com/)
[![Windows](https://img.shields.io/badge/-Windows-6E46A2.svg?logo=data:image/svg+xml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHZpZXdCb3g9IjAgMCA0ODc1IDQ4NzUiPjxwYXRoIGZpbGw9IiNmZmYiIGQ9Ik0wIDBoMjMxMXYyMzEwSDB6bTI1NjQgMGgyMzExdjIzMTBIMjU2NHpNMCAyNTY0aDIzMTF2MjMxMUgwem0yNTY0IDBoMjMxMXYyMzExSDI1NjQiLz48L3N2Zz4=)](https://www.microsoft.com/en-us/windows/windows-11)

# FLibrary

<details>
<summary>Скриншоты</summary>
 <img width="1871" alt="image" src="https://github.com/user-attachments/assets/7b5ef1fe-b3e5-4df7-9852-b1ea67241257" />
 <img width="1871" alt="image" src="https://github.com/user-attachments/assets/fc0501c8-726b-4117-85c9-a5b358ca06d9" />
</details>

Замена для [MyHomeLib](https://github.com/OleksiyPenkov/MyHomeLib)

#### Преимущества FLibrary перед MyHomeLib
* Скорость работы
  * Запуск программы
  * Создание новой коллекции
  * Построение навигации
  * Построение таблицы/дерева книг
  * Фильтрация списка книг по языку
  * Сплэшскрин не нужен :)
* Улучшеная аннотация
  * Есть оглавление, ключевые слова, эпиграф
  * Показан примерный размер текста в буквах/словах/страницах
  * Можно посмотреть все картинки книги
  * Можно извлечь картинку книги в память/файл
  * Можно открыть картинку в системном просмотрщике
  * Показываются отзывы читателей с Флибусты [^1]
* Навигация (левая панель)
  * Полный список авторов в навигации по авторам
  * Показывается информация об авторах [^1]
  * Есть навигация по архивам
  * Есть навигация по ключевым словам
  * Есть навигация языкам
  * Есть навигация по дате добавления книги в библиотеку
  * Есть навигация по отзывам читателей [^1]
  * Есть навигация по году издания [^2]
  * Навигацию можно фильтровать
  * Можно увидеть все книги коллекции в одном списке  [^3]
* Есть перманентная фильтрация книг
  * По авторам
  * По сериям
  * По жанрам
  * По ключевым словам
  * По языку
  * По рейтингу
* Есть полнотекстовый поиск по названиям книг и серий, именам авторов
* Поиск по названию находит книги в составе сборников [^1]
* Массовое (>50К) извлечение книг работает
* Автоматически определяет обновление inpx коллекции, умеет добавить новый архив в коллекцию
* Можно сливать коллекции в одну, просто храните их файлы в одной папке
* Поддерживаются разные форматы архивов, не только zip. См. [7z.dll](https://www.7-zip.org/)
* Есть собственная более экономичная форма хранения книг. [Репакер](https://github.com/heimdallr/books-tools?tab=readme-ov-file#fb2cut) из zip прилагается
* Есть инструмент для зачистки коллекции от нежелательных книг: дублей, языков, жанров, с удалением файлов из архивов
* Языки интерфейса
  * Есть русский язык. И плохой английский :)
  * Можно добавить новый язык. Приглашаю к сотрудничеству, в том числе по правкам фраз, криво мною переведённым на неродные для меня языки
  * Локализованы названия жанров
  * Локализованы названия языков 
* Есть возможность смены оформления UI
  * Есть темы: windows, vista, 11, fusion
  * Есть цветовые схемы (светлая и тёмная)
  * Поддерживается загрузка внешних qss, а также [dll](https://github.com/heimdallr/QtStyles/releases) с вкомпилированными в них таблицами стилей
* Умеет в web (2 варианта интерфейса) и OPDS
* Развивается
#### Недостатки
* Windows 10 x64 минимум
* Только inpx-коллекции, никакого онлайна
* Меньшая интуитивность создания/добавления коллекций
* Нет поиска по совокупности критериев
* Нет тулбара
* Меньше настроек
* Скорее всего, нет ещё какой-то функциональности MHL, о которой я не знаю
* Отсутствует дизайн
* Есть [баги](https://github.com/heimdallr/books/issues?q=is%3Aissue%20state%3Aopen%20label%3Abug)

[^1]: Требуются дополнительные файлы в папке с архивами
[^2]: Требуется расширенный inpx
[^3]: Для большой коллекции потребуется время и память


# Сборка  
### Windows  
Не запустится на винде ниже 10-ки, т.к. исползуется [Qt6](https://doc.qt.io/qt-6/windows.html)  
Собиралось только на MS Visual Studio 2022, прочие IDE не тестировались  
Используется C++23  

#### Установить и настроить conan  
[Инструкция](https://docs.conan.io/2/installation.html)  

#### Добавить в PATH пути к: 
* conan.exe  
* cmake.exe, версия cmake должна поддерживать вашу версию MSVS, conan,... короче, берите cmake посвежее  
* git.exe, необязательно, но полезно, позволит в логах видеть хэш текущего коммита  
* Inno Setup, если нужен инсталлятор  

#### Зависимости [^4]
* Qt6 (6.10.0)  
    * brotli (1.1.0)
    * bzip2 (1.0.8)
    * double-conversion (3.3.0)
    * freetype (2.13.2)
    * libjpeg (9e)
    * libpng (1.6.50)
    * pcre2 (10.42)
    * zlib (1.3.1)
* xerces-c  
* plog  
* boost, header only  
* 7z  
* libjxl  
    * brotli (1.1.Z)
    * highway (1.1.Z)
    * lcms (2.16.Z)
* ICU  

#### Сконфигурировать:
Запустить батник configure.bat. Возможно, сработают и другие способы, типа cmake-gui, или открыть в студии папку с исходниками.   

#### Собрать:
В результате конфигурирования в папке build будет создан солюшн FLibrary.sln. В нём надо собрать проект FLibrary.  

#### Ещё вариант:
Можно просто запустить батник build.bat. Если окружение настроено правильно, то в папке build/installer будет собраны инсталляторы и архив портабельной версии программы.

### Не Windows  
Увы, никак. Кое-где используется WinAPI. PR приветствуются.

[^4]: Подтянутся из conan, если необходимо - соберутся локально. Кроме Qt.
