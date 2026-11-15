# syntax=docker/dockerfile:1

# FLibrary (https://github.com/heimdallr/books) по README: gcc с C++23, Conan 2, Qt 6 и ./build.sh.
# Только x86_64.

############################ сборка ############################
FROM ubuntu:24.04 AS build

ARG BOOKS_REF=master
ARG QT_VERSION=6.12.0
ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
 && apt-get install --yes --no-install-recommends \
        ca-certificates curl git \
        gcc-14 g++-14 make cmake ninja-build patch perl xz-utils file 7zip \
        python3 python3-venv \
        libdbus-1-dev libegl1-mesa-dev libgl1-mesa-dev libwayland-dev libxkbcommon-dev \
        libfontconfig1 libglib2.0-0t64 libgssapi-krb5-2 \
 && rm -rf /var/lib/apt/lists/* \
 && test -f /usr/lib/7zip/7z.so \
 && update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-14 100 \
 && update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-14 100 \
 && update-alternatives --install /usr/bin/cc  cc  /usr/bin/gcc-14 100 \
 && update-alternatives --install /usr/bin/c++ c++ /usr/bin/g++-14 100

# Conan 2 и aqtinstall (ставит официальные сборки Qt)
RUN python3 -m venv /opt/venv \
 && /opt/venv/bin/pip install --no-cache-dir conan aqtinstall
ENV PATH=/opt/venv/bin:$PATH

# После установки QT обязательно делаем ссылку на /opt/Qt/6.12.0/gcc14_Release потому что build.sh хочет видеть ее именно там
RUN aqt install-qt linux desktop "${QT_VERSION}" linux_gcc_64 -O /opt/Qt \
        -m qthttpserver qtimageformats qtwaylandcompositor qtwebsockets \
 && mkdir -p /opt/Qt/6.12.0 \
 && ln -s "/opt/Qt/${QT_VERSION}/gcc_64" /opt/Qt/6.12.0/gcc14_Release

RUN git clone --recursive --shallow-submodules --depth 1 --branch "${BOOKS_REF}" \
        https://github.com/heimdallr/books.git /src
WORKDIR /src

RUN --mount=type=cache,target=/root/.conan2 ./build.sh

RUN mkdir /opt/FLibrary \
 && tar --extract --xz --file build/FLibrary-*.tar.xz --directory /opt/FLibrary --strip-components=1 \
 && test -x /opt/FLibrary/opds

############################ образ с сервером ############################
FROM ubuntu:24.04

ARG DEBIAN_FRONTEND=noninteractive

# Системные библиотеки, от которых зависит Qt, и curl для healthcheck
RUN apt-get update \
 && apt-get install --yes --no-install-recommends \
        ca-certificates curl \
        libatomic1 libdbus-1-3 libegl1 libfontconfig1 libfreetype6 libgl1 libglib2.0-0t64 \
        libgssapi-krb5-2 libopengl0 libssl3t64 libx11-6 libxcb1 libxkbcommon0 zlib1g \
 && rm -rf /var/lib/apt/lists/* \
 && install --directory --owner ubuntu --group ubuntu /data

COPY --from=build /opt/FLibrary /opt/FLibrary

# Запуск opds с тем же окружением, что задаёт штатный start.sh
COPY --chmod=755 <<'EOF' /usr/local/bin/flibrary-opds
#!/bin/sh
set -eu

app_dir=/opt/FLibrary
export LD_LIBRARY_PATH="${app_dir}/lib${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}"
export ICU_DATA="${app_dir}/lib"

# Явные аргументы уходят в opds как есть: --help, --version, --stop
if [ "$#" -gt 0 ]; then
    exec "${app_dir}/opds" "$@"
fi

name="${FLIBRARY_COLLECTION:-Library}"
database="${FLIBRARY_DATABASE:-/data/db/${name}.db}"

# Каталог базы должен существовать заранее, иначе создание коллекции зависает
mkdir -p "$(dirname "${database}")"

# Коллекция с таким именем уже есть - opds её откроет, нет - создаст из .inpx
set -- --log console --name "${name}" --database "${database}" --archives "${FLIBRARY_ARCHIVES:-/library}"
if [ -n "${FLIBRARY_INPX:-}" ]; then
    set -- "$@" --inpx "${FLIBRARY_INPX}"
fi
if [ -n "${FLIBRARY_ADDITIONAL:-}" ]; then
    set -- "$@" --additional "${FLIBRARY_ADDITIONAL}"
fi

exec "${app_dir}/opds" "$@"
EOF

# Настройки (~/.config/HomeCompa/FLibrary.conf) и база живут в /data
ENV LANG=C.UTF-8 \
    HOME=/data

# Проверка, что бинарник запускается и все библиотеки на месте
RUN HOME=/tmp flibrary-opds --version

USER ubuntu
WORKDIR /data
VOLUME ["/data"]
EXPOSE 12791

ENTRYPOINT ["flibrary-opds"]
