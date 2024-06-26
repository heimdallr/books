# FLibrary

Замена для [MyHomeLib](https://github.com/OleksiyPenkov/MyHomeLib)

#### Преимущества FLibrary перед MyHomeLib
* Скорость работы
  * создание новой коллекции
  * построение навигации
  * построение таблицы/дерева книг
  * извлечение обложки/аннотации
* Улучшеная аннотация
  * есть оглавление, ключевые слова, эпиграф
  * можно посмотреть все картинки книги
  * можно извлечь картинку книги в память/файл
  * можно открыть картинку в системном просмотрщике
* Есть навигация по архивам
* Массовое (>50К) извлечение книг работает
* Автоматически определяет обновление inpx коллекции, умеет добавить новый архив в коллекцию
* Можно добавить в коллекцию неиндексированные книги или архивы с книгами
* Поддержка 7z (а вдруг?)
* Есть русский язык. И плохой английский :)
* Локализованы жанры
* Развивается
#### Недостатки
* Windows 10 x64 минимум. На Win 3, 95, 98, Millenium, Nt3.5, Nt4, 2000, XP, 2003, Vista, 2008, 7, 8, 8.1 (ничего не забыл?:)) не запустится. На Win11 не проверял.
* Только inpx-коллекции, никакого онлайна
* Меньшая интуитивность создания/добавления коллекций
* Нет тулбара
* Меньше настроек
* Очень простой глобальный поиск
* Скорее всего, нет ещё какой-то функциональности MHL, о которой я не знаю
* Отсутствует дизайн
* Есть баги

## Сборка  
### Windows  
Не запустится на винде ниже 10-ки, т.к. исползуется Qt6  
Собиралось только на MS Visual Studio 2022, прочие IDE не тестировались  
Используется C++20  

#### Прописать в переменных окружения:  
SDK_PATH: путь к папке с библиотеками-зависимостями, по умолчанию D:/sdk  
PATH: добавить путь к cmake.exe, версия cmake должна поддерживать вашу версию MSVS.  
PATH: добавить путь к git.exe, необязательно, но полезно, позволит в логах видеть хэш текущего коммита  
PATH: добавить путь к Inno Setup, если нужен инсталлятор.  
PATH: добавить путь к 7z.exe, для сборки портабельной версии.  

#### Зависимости:  
* Qt6  
* xerces-c  
* plog  
* Hypodermic  
* boost, непосредственно не используется, но нужен Hypodermic'у  
* quazip, у него могут быть свои зависимости, например zlib  
* 7za.dll  

Версии и пути относительно SDK_PATH захардкожены в cmake-скриптах, пинки и помидоры принимаются по email  

#### Сконфигурировать:
Запустить батник solution_generate.bat. Возможно, сработают и другие способы, типа cmake-gui, или открыть в студии папку с исходниками.   

#### Собрать:
В результате конфигурирования в папке build будет создан солюшн FLibrary.sln. В нём надо собрать проект FLibrary.  

#### Ещё вариант:
Можно просто запустить батник build.bat. Если окружение настроено правильно, то в папке build/bin/installer будет собран инсталлятор и архив портабельной версии программы.

### Не Windows  
Увы, никак. Кое-где используется WinAPI. И зависимость от 7za.dll  
