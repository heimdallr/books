# FLibrary

<details>
<summary>Скриншоты</summary>
<img width="1706" alt="image" src="https://github.com/user-attachments/assets/b5b82b42-782b-406c-b96a-2c5c5a3e8035" />
<img width="1706" alt="image" src="https://github.com/user-attachments/assets/dd61bcfd-5ef6-4c04-9b38-d0923d24b8e1" />
</details>

Замена для [MyHomeLib](https://github.com/OleksiyPenkov/MyHomeLib)

#### Преимущества FLibrary перед MyHomeLib
* Скорость работы
  * создание новой коллекции
  * построение навигации
  * построение таблицы/дерева книг
  * извлечение обложки/аннотации
* Улучшеная аннотация
  * есть оглавление, ключевые слова, эпиграф
  * показан примерный размер текста в буквах/страницах
  * можно посмотреть все картинки книги
  * можно извлечь картинку книги в память/файл
  * можно открыть картинку в системном просмотрщике
* Есть навигация по архивам
* Есть навигация по ключевым словам
* Массовое (>50К) извлечение книг работает
* Автоматически определяет обновление inpx коллекции, умеет добавить новый архив в коллекцию
* Можно добавить в коллекцию неиндексированные книги или архивы с книгами
* Поддержка множества форматов архивов с книгами
* Есть собственная более экономичная форма хранения книг. [Репакер](#fb2cut) из zip прилагается
* Есть русский язык. И плохой английский :)
* Легко добавить новый язык. Приглашаю к сотрудничеству
* Локализованы жанры (русский, английский)
* Есть темы и цветовые схемы (светлая и тёмная)
* Умеет в OPDS
* Развивается
#### Недостатки
* Windows 10 x64 минимум
* Только inpx-коллекции, никакого онлайна
* Меньшая интуитивность создания/добавления коллекций
* Нет тулбара
* Меньше настроек
* Очень простой глобальный поиск
* Скорее всего, нет ещё какой-то функциональности MHL, о которой я не знаю
* Отсутствует дизайн
* Есть баги

# fb2cut

Консольное приложение для перепаковки архивов библиотеки  

#### Что делает
* Извлекает содержимое книг в формате fb2, вырезая из них картинки (содержимое узлов binary)
* Картинки складывает отдельно - папка covers для обложек, images для остальных картинок
* Ресайзит картинки, если из размеры превышают указанные максимумы
* Пережимает картинки в jpeg с указанным качеством сжатия
* Архивирует избавленные от картинок fb2 в указанный формат (7z или zip)
* Можно указать внешний архиватор (например 7z.exe) и параметры командной строки для него. ВНИМАНИЕ! Не стоит делать архивы непрерывными (solid), иначе при показе аннотации и извлечении книг гарантированы тормоза.
* Справку по ключам командной строки можно получить запуском утилиты с ключом -h
* При запуске без ключей будет предоставлен GUI

#### А зачем всё это?
Для уменьшения размера библиотеки в байтах, конечно. Особенно, если не нужны картинки. Или если нужны только обложки. Или если установить разумные ограничения размеров изображений и качество сжатиия. Но даже если оставить картинки в исходном размере, то всё равно будет ощутимый выигрыш от пережатия текстов в 7z и отдельного хранения изображений без кодирования в base64. Например, на момент разработки утилиты библиотека флибусты весит 448Г. После обработки утилитой без изменения размера картинок и умолчальном качестве сжалия в jpeg имеем: fb2 в 7z - 70Г, обложки - 41Г, остальные картинки - 176Г, итого - 287Г. Уже неплохо. С ограничением для обложек 1024px, а для остальных картинок - 480px с качеством сжатия 25: обложки - 25Г, остальные картинки - 46Г, итого - 141Г. Ещё лучше. И для совсем крохоборов, можно сделать картинки чёрно-белыми: обложки - 22Г, остальные картинки - 44Г, итого 136Г. PROFIT!

#### И что потом делать с перепакованными архивами?
Используем их с FLibrary вместо исходных. Или с любым другим каталогизатором, который поддерживает используемый формат архивации fb2, но книги будут без картинок.

#### Что за файлы в папках error?
Там fb2, которые не удалось распарсить. Кривой xml, левая кодировка и т.п. Можно попробовать как-то починить и подложить в соответствующий архив. Или подложить битые. Там же извлечённые из fb2 битые картинки, т.е. содержимое узлов binary, которое не удалось декодировать. Действительно, в файлах библиотеки много мусора, не являющегося валидными файлами изображений. Или валидными, но не являющиеся изображениями (html, xml, etc). Есть ещё вполне годные файлы в неподдерживаемом Qt формате, например riff. Их можно конвертнуть в jpeg каким-нибудь графическим редактором и подложить в соответствующий архив вручную. Или можно указать программе путь к ffmpeg.exe, который попытается декодировать или даже починить эти файлы. FFmpeg можно взять [тут](https://www.ffmpeg.org/). Но и FFmpeg не всесилен, бесповоротно битые файлы таковыми и останутся. Что с ними делать - решать вам. Если найдёте полезное им применения, сообщите мне, пожалуйста.

#### А что так долго-то?
Действительно, на моём компе обработка всей библиотеки продолжается около 11 часов. Львиную долю времени выполнения утилита тратит на архивацию текстов fb2. Увы, алгоритмы сжатия вычислительно затратны. К тому же, в сжатии алгоритмом PPMD участвуют не более 4-х потоков. Возможно, когда-нибудь я распараллелю запуск N/4 процессов внешнего архиватора (что вряд ли). Но переархивация всей библиотеки требуется лишь однократно. Если надо переработать изображения (изменить предельные размеры, подобрать качество кодирования в jpeg, etc), то можно отключить запись fb2 на диск, будет значительно быстрее.

#### Примеры запуска с параметрами командной строки
```
fb2cut.exe D:\fb2.Flibusta.Net\*.zip -o D:\repacked --max-size 480 --max-cover-size 1024 --image-quality 25 --image-grayscale -a "C:\Program Files\7-Zip\7z.exe" --ffmpeg C:\programs\ffmpeg\6.0\bin\ffmpeg.exe
```
```
fb2cut.exe --help
```

# Сборка  
### Windows  
Не запустится на винде ниже 10-ки, т.к. исползуется [Qt6](https://doc.qt.io/qt-6/windows.html)  
Собиралось только на MS Visual Studio 2022, прочие IDE не тестировались  
Используется C++20  

#### Прописать в переменных окружения:  
SDK_PATH: путь к папке с библиотеками-зависимостями, по умолчанию D:/sdk  
PATH: добавить путь к cmake.exe, версия cmake должна поддерживать вашу версию MSVS.  
PATH: добавить путь к git.exe, необязательно, но полезно, позволит в логах видеть хэш текущего коммита  
PATH: добавить путь к Inno Setup, если нужен инсталлятор.  
PATH: добавить путь к 7z.exe, для сборки портабельной версии.  

#### Зависимости:  
* Qt6 (6.8.0)  
* xerces-c  
* plog  
* Hypodermic  
* boost, непосредственно не используется, но нужен Hypodermic'у  
* 7z.dll
* imagequant (для fb2cut)

Версии и пути относительно SDK_PATH захардкожены в cmake-скриптах, пинки и помидоры принимаются по email  

#### Сконфигурировать:
Запустить батник solution_generate.bat. Возможно, сработают и другие способы, типа cmake-gui, или открыть в студии папку с исходниками.   

#### Собрать:
В результате конфигурирования в папке build будет создан солюшн FLibrary.sln. В нём надо собрать проект FLibrary.  

#### Ещё вариант:
Можно просто запустить батник build.bat. Если окружение настроено правильно, то в папке build/installer будет собраны инсталляторы и архив портабельной версии программы.

### Не Windows  
Увы, никак. Кое-где используется WinAPI. PR приветствуются.
