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
* Есть навигация по архивам
* Массовое (>50К) извлечение книг работает
* Автоматически определяет обновление inpx коллекции, умеет добавить новый архив в коллекцию
* Поддержка 7z (а вдруг?)
* Есть русский язык. И плохой английский :)
* Развивается
#### Недостатки
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
Собиралось только на MS Visual Studio 2022, прочие IDE не тестировались  
Используется C++20  

#### Прописать в переменных окружения:  
SDK_PATH: путь к папке с библиотеками-зависимостями, по умолчанию D:/sdk  
PATH: добавить путь к cmake.exe, версия cmake должна поддерживать вашу версию MSVS.  
PATH: добавить путь к git.exe, необязательно, но полезно, позволит в логах видеть хэш текущего коммита  
PATH: добавить путь к Inno Setup, если нужен инсталлятор.  

#### Зависимости:  
* Qt6, сборка с icu, иначе не будут извлекаться аннотации из книг в windows-1251  
* xerces-c  
* plog  
* Hypodermic  
* boost, непосредственно не используется, но нужен Hypodermic'у  
* quazip, у него могут быть свои зависимости, например zlib  
* 7za.dll  

Версии и пути относительно SDK_PATH захардкожены в cmake-скриптах, пинки и помидоры принимаются по email  

#### Сконфигурировать:
Простейший путь - запустить батник solution_generate.bat. Может, сработают и другие способы, типа cmake-gui, или открыть в студии папку с исходниками, кто знает?   

#### Собрать:
В результате конфигурирования в папке build будет создан солюшн FLibrary.sln. В нём надо собрать проект FLibrary.  

#### Ещё вариант:
Можно просто запустить батник build.bat. Если окружение настроено правильно, то в папке build/bin/installer будет собран инсталлятор.

### Не Windows  
Увы, никак. Кое-где используется WinAPI. И зависимость от 7za.dll  

