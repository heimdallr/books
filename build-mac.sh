#!/usr/bin/env bash
set -euo pipefail

APP_NAME="FLibrary"
BUNDLE_ID="org.homecompa.flibrary"
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

CONFIG="${CONFIG:-Release}"
ARCHS="${ARCHS:-${ARCH:-arm64 x86_64}}"
ALLOW_MISSING_ARCHS="${ALLOW_MISSING_ARCHS:-1}"
BASE_BUILD_DIR="${BUILD_DIR:-${ROOT_DIR}/build}"
if [[ "${BASE_BUILD_DIR}" != /* ]]; then
  BASE_BUILD_DIR="${ROOT_DIR}/${BASE_BUILD_DIR}"
fi
ARTIFACT_DIR="${ARTIFACT_DIR:-${BASE_BUILD_DIR}}"
if [[ "${ARTIFACT_DIR}" != /* ]]; then
  ARTIFACT_DIR="${ROOT_DIR}/${ARTIFACT_DIR}"
fi

export PATH="/opt/homebrew/bin:/usr/local/bin:${PATH}"

log() {
  printf '\033[1;34m==>\033[0m %s\n' "$*"
}

warn() {
  printf '\033[1;33mwarning:\033[0m %s\n' "$*" >&2
}

die() {
  printf 'error: %s\n' "$*" >&2
  exit 1
}

need_cmd() {
  command -v "$1" >/dev/null 2>&1 || die "missing command: $1"
}

cmake_value() {
  LC_ALL=C sed -nE "s/^set\\($1[[:space:]]+([^\\)]+)\\).*/\\1/p" "${ROOT_DIR}/CMakeLists.txt" | head -1
}

version() {
  printf '%s.%s.%s' \
    "$(cmake_value MAJOR_PRODUCT_VERSION)" \
    "$(cmake_value MINOR_PRODUCT_VERSION)" \
    "$(cmake_value PATCH_PRODUCT_VERSION)"
}

has_arch() {
  local file="$1"
  local arch="$2"
  local archs

  [[ -e "${file}" ]] || return 1
  archs="$(lipo -archs "${file}" 2>/dev/null || true)"
  [[ " ${archs} " == *" ${arch} "* ]]
}

arch_suffix() {
  case "$1" in
    arm64) printf 'ARM64' ;;
    x86_64) printf 'X86_64' ;;
    *) return 1 ;;
  esac
}

conan_profile_for_arch() {
  case "$1" in
    arm64) printf 'apple_clang_armv8' ;;
    x86_64) printf 'apple_clang_x86_64' ;;
    *) return 1 ;;
  esac
}

qt_core_path() {
  printf '%s/lib/QtCore.framework/Versions/A/QtCore' "$1"
}

qt_macdeployqt_path() {
  if [[ -x "$1/bin/macdeployqt" ]]; then
    printf '%s/bin/macdeployqt' "$1"
  else
    command -v macdeployqt
  fi
}

candidate_qt_prefixes() {
  local arch="$1"
  local suffix
  suffix="$(arch_suffix "${arch}")"

  local arch_var="QT_PREFIX_${suffix}"
  [[ -n "${!arch_var:-}" ]] && printf '%s\n' "${!arch_var}"
  [[ -n "${QT_PREFIX:-}" ]] && printf '%s\n' "${QT_PREFIX}"
  [[ -n "${QT_DIR:-}" ]] && printf '%s\n' "${QT_DIR}"

  case "${arch}" in
    arm64)
      printf '%s\n' \
        /opt/homebrew/opt/qt \
        /opt/homebrew/opt/qt@6 \
        /opt/homebrew/opt/qt6 \
        /usr/local/opt/qt \
        /usr/local/opt/qt@6 \
        /usr/local/opt/qt6
      ;;
    x86_64)
      printf '%s\n' \
        /usr/local/opt/qt \
        /usr/local/opt/qt@6 \
        /usr/local/opt/qt6 \
        /opt/homebrew/opt/qt \
        /opt/homebrew/opt/qt@6 \
        /opt/homebrew/opt/qt6
      ;;
  esac
}

resolve_qt_prefix() {
  local arch="$1"
  local prefix

  while IFS= read -r prefix; do
    [[ -n "${prefix}" ]] || continue
    [[ -d "${prefix}" ]] || continue
    if has_arch "$(qt_core_path "${prefix}")" "${arch}"; then
      printf '%s\n' "${prefix}"
      return 0
    fi
  done < <(candidate_qt_prefixes "${arch}" | awk '!seen[$0]++')

  return 1
}

candidate_p7zip_dirs() {
  local arch="$1"
  local suffix
  suffix="$(arch_suffix "${arch}")"

  local arch_var="P7ZIP_DIR_${suffix}"
  [[ -n "${!arch_var:-}" ]] && printf '%s\n' "${!arch_var}"
  [[ -n "${P7ZIP_DIR:-}" ]] && printf '%s\n' "${P7ZIP_DIR}"

  local prefix
  case "${arch}" in
    arm64)
      for prefix in /opt/homebrew/opt/p7zip /usr/local/opt/p7zip; do
        [[ -d "${prefix}" ]] && find -L "${prefix}" -name 7z.so -print 2>/dev/null | while IFS= read -r so; do dirname "${so}"; done
      done
      ;;
    x86_64)
      for prefix in /usr/local/opt/p7zip /opt/homebrew/opt/p7zip; do
        [[ -d "${prefix}" ]] && find -L "${prefix}" -name 7z.so -print 2>/dev/null | while IFS= read -r so; do dirname "${so}"; done
      done
      ;;
  esac
}

resolve_p7zip_dir() {
  local arch="$1"
  local dir

  while IFS= read -r dir; do
    [[ -n "${dir}" ]] || continue
    if has_arch "${dir}/7z.so" "${arch}"; then
      printf '%s\n' "${dir}"
      return 0
    fi
  done < <(candidate_p7zip_dirs "${arch}" | awk '!seen[$0]++')

  return 1
}

collect_qt_libpaths() {
  local prefix="$1"
  local opt_root
  opt_root="$(cd "$(dirname "${prefix}")" 2>/dev/null && pwd || true)"

  printf '%s/lib\n' "${prefix}"
  if [[ -n "${opt_root}" && -d "${opt_root}" ]]; then
    find -L "${opt_root}" -maxdepth 2 -type d -path '*/lib' -print 2>/dev/null \
      | while IFS= read -r libdir; do
          case "$(basename "$(dirname "${libdir}")")" in
            qt*) printf '%s\n' "${libdir}" ;;
          esac
        done
  fi | awk '!seen[$0]++'
}

if [[ "$(uname -s)" != "Darwin" ]]; then
  die "build-mac.sh must be run on macOS"
fi

need_cmd cmake
need_cmd ninja
need_cmd conan
need_cmd hdiutil
need_cmd iconutil
need_cmd install_name_tool
need_cmd lipo
need_cmd otool
need_cmd osascript
need_cmd rsvg-convert
need_cmd codesign
need_cmd xcrun

SDKROOT="$(xcrun --sdk macosx --show-sdk-path)"
APP_VERSION="$(version)"
[[ "${APP_VERSION}" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || die "could not read product version from CMakeLists.txt"

ICON_SVG="${ROOT_DIR}/src/home/resources/icons/${APP_NAME}.svg"

render_icon() {
  log "Rendering HD app icon for ${CURRENT_ARCH}"
  [[ -f "${ICON_SVG}" ]] || die "icon source is missing: ${ICON_SVG}"

  rm -rf "${ICONSET_DIR}"
  mkdir -p "${PACKAGE_DIR}" "${ICONSET_DIR}"

  rsvg-convert -w 1024 -h 1024 "${ICON_SVG}" -o "${ICON_PNG}"
  rsvg-convert -w 16 -h 16 "${ICON_SVG}" -o "${ICONSET_DIR}/icon_16x16.png"
  rsvg-convert -w 32 -h 32 "${ICON_SVG}" -o "${ICONSET_DIR}/icon_16x16@2x.png"
  rsvg-convert -w 32 -h 32 "${ICON_SVG}" -o "${ICONSET_DIR}/icon_32x32.png"
  rsvg-convert -w 64 -h 64 "${ICON_SVG}" -o "${ICONSET_DIR}/icon_32x32@2x.png"
  rsvg-convert -w 128 -h 128 "${ICON_SVG}" -o "${ICONSET_DIR}/icon_128x128.png"
  rsvg-convert -w 256 -h 256 "${ICON_SVG}" -o "${ICONSET_DIR}/icon_128x128@2x.png"
  rsvg-convert -w 256 -h 256 "${ICON_SVG}" -o "${ICONSET_DIR}/icon_256x256.png"
  rsvg-convert -w 512 -h 512 "${ICON_SVG}" -o "${ICONSET_DIR}/icon_256x256@2x.png"
  rsvg-convert -w 512 -h 512 "${ICON_SVG}" -o "${ICONSET_DIR}/icon_512x512.png"
  rsvg-convert -w 1024 -h 1024 "${ICON_SVG}" -o "${ICONSET_DIR}/icon_512x512@2x.png"
  iconutil -c icns "${ICONSET_DIR}" -o "${ICON_ICNS}"
}

render_dmg_background() {
  log "Rendering DMG background for ${CURRENT_ARCH}"
  mkdir -p "${PACKAGE_DIR}"

  cat > "${DMG_BACKGROUND_SVG}" <<'EOF'
<?xml version="1.0" encoding="UTF-8"?>
<svg xmlns="http://www.w3.org/2000/svg" width="600" height="400" viewBox="0 0 600 400">
  <defs>
    <linearGradient id="bg" x1="0" y1="0" x2="0" y2="1">
      <stop offset="0" stop-color="#f8fbfb"/>
      <stop offset="1" stop-color="#e6eeee"/>
    </linearGradient>
    <filter id="softShadow" x="-20%" y="-20%" width="140%" height="140%">
      <feDropShadow dx="0" dy="8" stdDeviation="10" flood-color="#54615f" flood-opacity="0.22"/>
    </filter>
  </defs>
  <rect width="600" height="400" fill="url(#bg)"/>
  <text x="300" y="54" text-anchor="middle" font-family="-apple-system, BlinkMacSystemFont, Helvetica, Arial, sans-serif" font-size="28" font-weight="700" fill="#24302f">Install FLibrary</text>
  <text x="300" y="86" text-anchor="middle" font-family="-apple-system, BlinkMacSystemFont, Helvetica, Arial, sans-serif" font-size="15" fill="#53615f">Drag FLibrary.app to Applications</text>
  <path d="M255 205 H345" stroke="#6e7f7d" stroke-width="12" stroke-linecap="round" filter="url(#softShadow)"/>
  <path d="M342 164 L412 205 L342 246 Z" fill="#6e7f7d" filter="url(#softShadow)"/>
  <circle cx="170" cy="205" r="72" fill="#ffffff" opacity="0.55" filter="url(#softShadow)"/>
  <circle cx="430" cy="205" r="72" fill="#ffffff" opacity="0.55" filter="url(#softShadow)"/>
</svg>
EOF

  rsvg-convert -w 600 -h 400 "${DMG_BACKGROUND_SVG}" -o "${DMG_BACKGROUND_PNG}"
}

configure_and_build() {
  log "Configuring ${APP_NAME} ${APP_VERSION} (${CURRENT_ARCH})"
  cmake -S "${ROOT_DIR}" -B "${BUILD_DIR}" -G Ninja \
    -DCMAKE_BUILD_TYPE="${CONFIG}" \
    -DCMAKE_INSTALL_PREFIX="${BUILD_DIR}/install" \
    -DCMAKE_MAKE_PROGRAM="$(command -v ninja)" \
    -DCMAKE_OSX_SYSROOT="${SDKROOT}" \
    -DCMAKE_OSX_ARCHITECTURES="${CURRENT_ARCH}" \
    -DCMAKE_EXE_LINKER_FLAGS="-Wl,-headerpad_max_install_names" \
    -DCMAKE_SHARED_LINKER_FLAGS="-Wl,-headerpad_max_install_names" \
    -DCMAKE_MODULE_LINKER_FLAGS="-Wl,-headerpad_max_install_names" \
    -DCONAN_PROFILE="${CONAN_PROFILE_NAME}" \
    -DQt6_DIR="${QT_PREFIX}/lib/cmake/Qt6" \
    -D7zip_BIN_DIR="${P7ZIP_DIR}" \
    -DCPACK_GENERATOR=ZIP

  log "Building ${CURRENT_ARCH}"
  cmake --build "${BUILD_DIR}" --config "${CONFIG}" --parallel

  log "Installing ${CURRENT_ARCH}"
  cmake --install "${BUILD_DIR}" --config "${CONFIG}"
}

write_plist() {
  local plist="$1"
  cat > "${plist}" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>CFBundleDevelopmentRegion</key>
  <string>en</string>
  <key>CFBundleDisplayName</key>
  <string>${APP_NAME}</string>
  <key>CFBundleExecutable</key>
  <string>${APP_NAME}</string>
  <key>CFBundleIconFile</key>
  <string>${APP_NAME}.icns</string>
  <key>CFBundleIdentifier</key>
  <string>${BUNDLE_ID}</string>
  <key>CFBundleName</key>
  <string>${APP_NAME}</string>
  <key>CFBundlePackageType</key>
  <string>APPL</string>
  <key>CFBundleShortVersionString</key>
  <string>${APP_VERSION}</string>
  <key>CFBundleVersion</key>
  <string>${APP_VERSION}</string>
  <key>LSApplicationCategoryType</key>
  <string>public.app-category.productivity</string>
  <key>LSMinimumSystemVersion</key>
  <string>11.0</string>
  <key>NSHighResolutionCapable</key>
  <true/>
  <key>NSPrincipalClass</key>
  <string>NSApplication</string>
</dict>
</plist>
EOF
}

is_macho() {
  file "$1" | grep -q 'Mach-O'
}

read_rpaths() {
  otool -l "$1" 2>/dev/null | awk '
    $1 == "cmd" && $2 == "LC_RPATH" { in_rpath = 1; next }
    in_rpath && $1 == "path" { print $2; in_rpath = 0 }
  '
}

has_rpath() {
  read_rpaths "$1" | grep -Fxq "$2"
}

delete_build_rpaths() {
  local file="$1"
  local rpath

  while IFS= read -r rpath; do
    case "${rpath}" in
      "${BUILD_DIR}"*|"${ROOT_DIR}"*|/opt/homebrew/*|/usr/local/*|/Users/*)
        install_name_tool -delete_rpath "${rpath}" "${file}" 2>/dev/null || true
        ;;
    esac
  done < <(read_rpaths "${file}")
}

add_rpath_once() {
  local file="$1"
  local rpath="$2"

  has_rpath "${file}" "${rpath}" || install_name_tool -add_rpath "${rpath}" "${file}" 2>/dev/null || true
}

fix_bundle_rpaths() {
  log "Fixing bundle rpaths (${CURRENT_ARCH})"

  local file
  while IFS= read -r -d '' file; do
    is_macho "${file}" || continue
    delete_build_rpaths "${file}"

    case "${file}" in
      "${APP_PATH}/Contents/MacOS/"*)
        add_rpath_once "${file}" "@executable_path/../Frameworks"
        ;;
      "${APP_PATH}/Contents/Frameworks/"*.dylib|"${APP_PATH}/Contents/Frameworks/7z.so")
        add_rpath_once "${file}" "@loader_path"
        ;;
      "${APP_PATH}/Contents/Frameworks/"*.framework/Versions/*/*)
        add_rpath_once "${file}" "@loader_path/../../../"
        ;;
      "${APP_PATH}/Contents/PlugIns/"*)
        add_rpath_once "${file}" "@loader_path/../../Frameworks"
        ;;
    esac
  done < <(find "${APP_PATH}/Contents" -type f -print0)
}

bundle_framework_target() {
  local dep="$1"
  local framework version binary local_binary

  if [[ "${dep}" =~ /([^/]+\.framework)/Versions/([^/]+)/([^/]+)$ ]]; then
    framework="${BASH_REMATCH[1]}"
    version="${BASH_REMATCH[2]}"
    binary="${BASH_REMATCH[3]}"
    local_binary="${APP_PATH}/Contents/Frameworks/${framework}/Versions/${version}/${binary}"

    if [[ -e "${local_binary}" ]]; then
      printf '@rpath/%s/Versions/%s/%s\n' "${framework}" "${version}" "${binary}"
      return 0
    fi
  fi

  return 1
}

bundle_dylib_target() {
  local dep="$1"
  local base

  base="$(basename "${dep}")"
  if [[ -e "${APP_PATH}/Contents/Frameworks/${base}" ]]; then
    printf '@rpath/%s\n' "${base}"
    return 0
  fi

  return 1
}

bundle_dependency_target() {
  local dep="$1"

  bundle_framework_target "${dep}" || bundle_dylib_target "${dep}"
}

list_macho_dependencies() {
  local file

  while IFS= read -r -d '' file; do
    is_macho "${file}" || continue
    otool -L "${file}" 2>/dev/null | tail -n +2 | awk '{ print $1 }'
  done < <(find "${APP_PATH}/Contents" -type f -print0)
}

qt_framework_source() {
  local framework="$1"
  local libdir

  while IFS= read -r libdir; do
    [[ -d "${libdir}/${framework}" ]] || continue
    printf '%s/%s\n' "${libdir}" "${framework}"
    return 0
  done < <(collect_qt_libpaths "${QT_PREFIX}")

  return 1
}

copy_framework_if_missing() {
  local framework="$1"
  local src="$2"
  local dst="${APP_PATH}/Contents/Frameworks/${framework}"

  [[ -d "${dst}" ]] && return 1
  [[ -d "${src}" ]] || return 1

  log "Copying ${framework} (${CURRENT_ARCH})"
  ditto "${src}" "${dst}"
  return 0
}

copy_dylib_if_missing() {
  local src="$1"
  local base dst

  [[ -f "${src}" ]] || return 1
  base="$(basename "${src}")"
  dst="${APP_PATH}/Contents/Frameworks/${base}"
  [[ -e "${dst}" ]] && return 1

  log "Copying ${base} (${CURRENT_ARCH})"
  ditto "${src}" "${dst}"
  return 0
}

copy_missing_bundle_dependencies() {
  log "Copying missing bundle dependencies (${CURRENT_ARCH})"

  local pass dep framework src copied
  for pass in 1 2 3 4 5; do
    copied=0

    while IFS= read -r dep; do
      case "${dep}" in
        @rpath/*.framework/Versions/*/*)
          framework="${dep#@rpath/}"
          framework="${framework%%/Versions/*}"
          if src="$(qt_framework_source "${framework}")"; then
            copy_framework_if_missing "${framework}" "${src}" && copied=1
          fi
          ;;
        /opt/homebrew/*.framework/Versions/*/*|/usr/local/*.framework/Versions/*/*|/Users/*.framework/Versions/*/*)
          framework="${dep##*/lib/}"
          framework="${framework%%/Versions/*}"
          src="${dep%/Versions/*}"
          copy_framework_if_missing "${framework}" "${src}" && copied=1
          ;;
        /opt/homebrew/*.dylib|/usr/local/*.dylib|/Users/*.dylib)
          copy_dylib_if_missing "${dep}" && copied=1
          ;;
      esac
    done < <(list_macho_dependencies | sort -u)

    (( copied == 0 )) && break
  done
}

set_bundle_install_id() {
  local file="$1"
  local rel target

  case "${file}" in
    "${APP_PATH}/Contents/Frameworks/"*.framework/Versions/*/*)
      rel="${file#"${APP_PATH}/Contents/Frameworks/"}"
      target="@rpath/${rel}"
      install_name_tool -id "${target}" "${file}" 2>/dev/null || true
      ;;
    "${APP_PATH}/Contents/Frameworks/"*.dylib|"${APP_PATH}/Contents/Frameworks/7z.so")
      target="@rpath/$(basename "${file}")"
      install_name_tool -id "${target}" "${file}" 2>/dev/null || true
      ;;
  esac
}

fix_bundle_install_names() {
  log "Fixing bundle install names (${CURRENT_ARCH})"

  local file dep target
  while IFS= read -r -d '' file; do
    is_macho "${file}" || continue
    set_bundle_install_id "${file}"

    while IFS= read -r dep; do
      case "${dep}" in
        /opt/homebrew/*|/usr/local/*|/Users/*)
          if target="$(bundle_dependency_target "${dep}")"; then
            install_name_tool -change "${dep}" "${target}" "${file}" 2>/dev/null || true
          fi
          ;;
      esac
    done < <(otool -L "${file}" 2>/dev/null | tail -n +2 | awk '{ print $1 }')
  done < <(find "${APP_PATH}/Contents" -type f -print0)
}

prune_stale_project_libraries() {
  local frameworks="$1"
  local major_minor="${APP_VERSION%.*}"

  find "${frameworks}" -maxdepth 1 -type f \
    -name "lib*.${major_minor}.*.dylib" \
    ! -name "lib*.${APP_VERSION}.dylib" \
    -delete
}

copy_if_exists() {
  local src="$1"
  local dst="$2"
  [[ -e "${src}" ]] || return 0
  ditto "${src}" "${dst}"
}

qt_plugin_roots() {
  local brew_root

  [[ -d "${QT_PREFIX}/share/qt/plugins" ]] && printf '%s\n' "${QT_PREFIX}/share/qt/plugins"
  [[ -d "${QT_PREFIX}/plugins" ]] && printf '%s\n' "${QT_PREFIX}/plugins"

  brew_root="$(cd "${QT_PREFIX}/../.." 2>/dev/null && pwd || true)"
  if [[ -n "${brew_root}" && -d "${brew_root}/share/qt/plugins" ]]; then
    printf '%s\n' "${brew_root}/share/qt/plugins"
  fi
}

write_qt_conf() {
  local resources="$1"

  cat > "${resources}/qt.conf" <<'EOF'
[Paths]
Plugins = PlugIns
Translations = Resources/locales
EOF
}

copy_qt_plugins() {
  local plugins="$1"
  local plugin_dirs=(iconengines imageformats networkinformation platforminputcontexts platforms styles tls)
  local dir root link subdir name src tmp

  mkdir -p "${plugins}"

  for dir in "${plugin_dirs[@]}"; do
    [[ -d "${plugins}/${dir}" ]] && continue
    while IFS= read -r root; do
      [[ -d "${root}/${dir}" ]] || continue
      mkdir -p "${plugins}/${dir}"
      cp -R -L "${root}/${dir}/." "${plugins}/${dir}/"
      break
    done < <(qt_plugin_roots | awk '!seen[$0]++')
  done

  while IFS= read -r -d '' link; do
    subdir="$(basename "$(dirname "${link}")")"
    name="$(basename "${link}")"
    while IFS= read -r root; do
      src="${root}/${subdir}/${name}"
      [[ -e "${src}" || -L "${src}" ]] || continue
      tmp="${link}.tmp"
      rm -f "${tmp}"
      cp -p -L "${src}" "${tmp}"
      mv "${tmp}" "${link}"
      break
    done < <(qt_plugin_roots | awk '!seen[$0]++')
  done < <(find "${plugins}" -type l -print0)
}

bundle_app() {
  log "Creating app bundle (${CURRENT_ARCH})"

  local contents="${APP_PATH}/Contents"
  local macos="${contents}/MacOS"
  local frameworks="${contents}/Frameworks"
  local plugins="${contents}/PlugIns"
  local resources="${contents}/Resources"

  rm -rf "${APP_PATH}"
  mkdir -p "${macos}" "${frameworks}" "${plugins}" "${resources}"

  install -m 755 "${BUILD_DIR}/bin/${APP_NAME}" "${macos}/${APP_NAME}"
  if [[ -f "${BUILD_DIR}/bin/opds" ]]; then
    install -m 755 "${BUILD_DIR}/bin/opds" "${macos}/opds"
  fi

  ditto "${BUILD_DIR}/lib" "${frameworks}"
  rm -f "${frameworks}"/*.a
  prune_stale_project_libraries "${frameworks}"

  copy_if_exists "${BUILD_DIR}/bin/locales" "${macos}/locales"
  copy_if_exists "${BUILD_DIR}/bin/locales" "${resources}/locales"
  copy_if_exists "${BUILD_DIR}/bin/genres.json" "${macos}/genres.json"
  copy_if_exists "${BUILD_DIR}/bin/genres.json" "${resources}/genres.json"
  copy_if_exists "${BUILD_DIR}/bin/LICENSE.txt" "${resources}/LICENSE.txt"
  copy_if_exists "${BUILD_DIR}/bin/LICENSE.txt" "${macos}/LICENSE.txt"
  printf 'dmg\n' > "${macos}/installer_mode"

  install -m 644 "${ICON_ICNS}" "${resources}/${APP_NAME}.icns"
  write_qt_conf "${resources}"
  write_plist "${contents}/Info.plist"

  fix_bundle_rpaths

  log "Deploying Qt frameworks (${CURRENT_ARCH})"
  local deploy_args=("${APP_PATH}")
  if [[ -f "${macos}/opds" ]]; then
    deploy_args+=("-executable=${macos}/opds")
  fi
  deploy_args+=("-libpath=${frameworks}")
  local libpath
  while IFS= read -r libpath; do
    [[ -n "${libpath}" && -d "${libpath}" ]] && deploy_args+=("-libpath=${libpath}")
  done < <(collect_qt_libpaths "${QT_PREFIX}")
  deploy_args+=(-always-overwrite -no-codesign -verbose=1)
  "${MACDEPLOYQT}" "${deploy_args[@]}"

  write_qt_conf "${resources}"
  copy_qt_plugins "${plugins}"

  copy_missing_bundle_dependencies
  fix_bundle_rpaths
  fix_bundle_install_names
}

sign_and_verify_app() {
  log "Signing app bundle (${CURRENT_ARCH})"
  codesign --force --deep --sign - "${APP_PATH}"
  codesign --verify --deep --strict --verbose=2 "${APP_PATH}"

  log "Checking local absolute paths (${CURRENT_ARCH})"
  local file
  while IFS= read -r -d '' file; do
    is_macho "${file}" || continue
    if otool -L "${file}" 2>/dev/null | tail -n +2 | grep -Eq '(/opt/homebrew|/usr/local|/Users/)'; then
      printf 'local dependency found in %s\n' "${file}" >&2
      die "bundle still contains local absolute dependency paths"
    fi
    if read_rpaths "${file}" | grep -Eq '(/opt/homebrew|/usr/local|/Users/)'; then
      printf 'local rpath found in %s\n' "${file}" >&2
      die "bundle still contains local absolute rpaths"
    fi
  done < <(find "${APP_PATH}/Contents" -type f -print0)

  "${APP_PATH}/Contents/MacOS/${APP_NAME}" --version
}

create_dmg() {
  log "Creating drag-to-Applications DMG (${CURRENT_ARCH})"

  local volname="${APP_NAME} ${APP_VERSION}"
  local background_dir="${STAGE_DIR}/.background"

  rm -rf "${STAGE_DIR}" "${DMG_PATH}" "${RW_DMG_PATH}"
  mkdir -p "${background_dir}" "$(dirname "${DMG_PATH}")"

  ditto "${APP_PATH}" "${STAGE_DIR}/${APP_NAME}.app"
  ln -s /Applications "${STAGE_DIR}/Applications"
  install -m 644 "${DMG_BACKGROUND_PNG}" "${background_dir}/background.png"

  hdiutil create \
    -volname "${volname}" \
    -srcfolder "${STAGE_DIR}" \
    -ov \
    -fs HFS+ \
    -format UDRW \
    "${RW_DMG_PATH}"

  local attach_output
  attach_output="$(hdiutil attach -readwrite -noverify -noautoopen "${RW_DMG_PATH}")"

  local mount_point
  mount_point="$(printf '%s\n' "${attach_output}" | sed -n 's#.*\(/Volumes/.*\)$#\1#p' | head -1)"
  [[ -n "${mount_point}" && -d "${mount_point}" ]] || die "could not mount temporary DMG"

  osascript <<EOF || warn "Finder layout was skipped for ${CURRENT_ARCH}"
with timeout of 45 seconds
  tell application "Finder"
    tell disk "${volname}"
      open
      delay 1
      set current view of container window to icon view
      set toolbar visible of container window to false
      set statusbar visible of container window to false
      set bounds of container window to {180, 120, 780, 520}
      set viewOptions to the icon view options of container window
      set arrangement of viewOptions to not arranged
      set icon size of viewOptions to 96
      set background picture of viewOptions to file ".background:background.png"
      set position of item "${APP_NAME}.app" of container window to {170, 220}
      set position of item "Applications" of container window to {430, 220}
      delay 1
      close container window
    end tell
  end tell
end timeout
EOF

  sync
  hdiutil detach "${mount_point}" -quiet || hdiutil detach "${mount_point}" -force -quiet

  hdiutil convert "${RW_DMG_PATH}" \
    -format UDZO \
    -imagekey zlib-level=9 \
    -o "${DMG_PATH}"
  rm -f "${RW_DMG_PATH}"

  hdiutil verify "${DMG_PATH}"
}

setup_arch() {
  CURRENT_ARCH="$1"
  CONAN_PROFILE_NAME="$(conan_profile_for_arch "${CURRENT_ARCH}")"

  if ! QT_PREFIX="$(resolve_qt_prefix "${CURRENT_ARCH}")"; then
    SKIP_REASON="Qt with ${CURRENT_ARCH} slice was not found"
    return 1
  fi
  if ! P7ZIP_DIR="$(resolve_p7zip_dir "${CURRENT_ARCH}")"; then
    SKIP_REASON="p7zip 7z.so with ${CURRENT_ARCH} slice was not found"
    return 1
  fi

  MACDEPLOYQT="$(qt_macdeployqt_path "${QT_PREFIX}")"
  [[ -x "${MACDEPLOYQT}" ]] || {
    SKIP_REASON="macdeployqt was not found for ${CURRENT_ARCH}"
    return 1
  }

  if (( ARCH_COUNT > 1 )); then
    BUILD_DIR="${BASE_BUILD_DIR}-${CURRENT_ARCH}"
  else
    BUILD_DIR="${BASE_BUILD_DIR}"
  fi

  PACKAGE_DIR="${BUILD_DIR}/package"
  APP_PATH="${BUILD_DIR}/${APP_NAME}.app"
  STAGE_DIR="${BUILD_DIR}/dmg-root"
  DMG_PATH="${ARTIFACT_DIR}/${APP_NAME}-${APP_VERSION}-macOS-${CURRENT_ARCH}.dmg"
  RW_DMG_PATH="${BUILD_DIR}/${APP_NAME}-${APP_VERSION}-macOS-${CURRENT_ARCH}-rw.dmg"
  ICON_PNG="${PACKAGE_DIR}/${APP_NAME}-1024.png"
  ICONSET_DIR="${PACKAGE_DIR}/${APP_NAME}.iconset"
  ICON_ICNS="${PACKAGE_DIR}/${APP_NAME}.icns"
  DMG_BACKGROUND_SVG="${PACKAGE_DIR}/dmg-background.svg"
  DMG_BACKGROUND_PNG="${PACKAGE_DIR}/dmg-background.png"

  log "Using Qt: ${QT_PREFIX} (${CURRENT_ARCH})"
  log "Using p7zip: ${P7ZIP_DIR} (${CURRENT_ARCH})"
}

run_arch() {
  render_icon
  render_dmg_background
  configure_and_build
  bundle_app
  sign_and_verify_app
  create_dmg
}

read -r -a REQUESTED_ARCHS <<< "${ARCHS}"
ARCH_COUNT="${#REQUESTED_ARCHS[@]}"
BUILT_DMGS=()
SKIPPED_ARCHS=()

for arch in "${REQUESTED_ARCHS[@]}"; do
  case "${arch}" in
    arm64|x86_64) ;;
    *) die "unsupported arch '${arch}'. Supported: arm64 x86_64" ;;
  esac

  SKIP_REASON=""
  if ! setup_arch "${arch}"; then
    if [[ "${ALLOW_MISSING_ARCHS}" == "1" ]]; then
      warn "skipping ${arch}: ${SKIP_REASON}"
      SKIPPED_ARCHS+=("${arch}: ${SKIP_REASON}")
      continue
    fi
    die "cannot build ${arch}: ${SKIP_REASON}"
  fi

  run_arch
  BUILT_DMGS+=("${DMG_PATH}")
done

(( ${#BUILT_DMGS[@]} > 0 )) || die "no DMGs were built"

log "Done"
for dmg in "${BUILT_DMGS[@]}"; do
  printf 'DMG: %s\n' "${dmg}"
done
if (( ${#SKIPPED_ARCHS[@]} > 0 )); then
  for skipped in "${SKIPPED_ARCHS[@]}"; do
    printf 'Skipped: %s\n' "${skipped}"
  done
fi
