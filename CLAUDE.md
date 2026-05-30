# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build & Run

Standard cycle for adding/modifying a module on macOS:

```bash
# Configure (run after adding/removing a module dir or touching CMakeLists.txt)
PKG_CONFIG_PATH=/opt/homebrew/opt/mariadb-connector-c/lib/pkgconfig cmake -S . -B build

# Build
cmake --build build --target lele-tools

# Restart the app (the build product is a .app bundle)
pkill -f "lele-tools.app/Contents/MacOS/lele-tools" 2>/dev/null
open build/lele-tools.app
```

The `PKG_CONFIG_PATH` is needed because MariaDB Connector/C is a Homebrew keg-only formula and CMake's `pkg_check_modules` won't find it otherwise. The CMake script also has a manual `find_path/find_library` fallback if pkg-config is unavailable.

`modules/` is picked up via `file(GLOB_RECURSE SOURCES ./modules/*.cpp)` in `CMakeLists.txt:125`, so a fresh module directory is automatically compiled in **once cmake is reconfigured** (touching just the source isn't enough — you must rerun `cmake -S . -B build`).

There is no test suite. Validate changes by launching the .app and exercising the module.

## Module Architecture

Each tool is a `QWidget` subclass that also inherits `DynamicObjectBase`. The framework instantiates modules by string class name via a factory registered with the `REGISTER_DYNAMICOBJECT(ClassName)` macro (`common/dynamicobjectbase.h`). To add a new module:

1. Create `modules/<kebab-name>/<lowercase-name>.{h,cpp}`
2. In the .cpp, after includes, put `REGISTER_DYNAMICOBJECT(YourClass);` at file scope
3. Inherit `public QWidget, public DynamicObjectBase` and have a default constructor `explicit YourClass();`
4. Append an entry to `tool-list/module-meta.h`: `{"xxx", "Display Name", "YourClass"},`
5. Reconfigure CMake (see above) so the GLOB picks up the new files
6. (Optional) add Chinese translation entry to `translations/lele-tools_zh_CN.ts`

`tool-list/module-meta.h` is the source of truth for what appears in the left-side tool list. The display string is the `titleKey` and is translated via `QObject::tr()` at lookup time.

## Cross-cutting Patterns

**Persistence** — two layers exist, with clear roles:

- `QSettings` (group-scoped per module, e.g., `s.beginGroup("OssImageHost")`) for user-tweakable config: window state, API keys, preferences. Stored at `~/Library/Preferences/...` on macOS.
- `SqliteWrapper::SqliteManager` (shared singleton via `getDefaultInstance()`) for structured records: salary attendance, todo items, OSS upload history. `common/sqlite/SqliteManager.cpp` and `common/sqlite/TableManager.cpp` are the canonical reference. Modules create their tables via `m_db->execute("CREATE TABLE IF NOT EXISTS ...")` on first init.

**HiDPI / Retina** — a recurring pattern when displaying remote/computed images: scale the pixmap to **physical** pixels (`availLogical * devicePixelRatioF()`) then `setDevicePixelRatio(dpr)` on the pixmap, and resize the label to logical size. Forgetting this makes images look blurry on Retina even when source resolution is high. See `MarkdownToLongImage::refreshPreview` and `OssImageHost::onListItemActivated` for examples.

**Heavy work off the UI thread** — for QProcess-backed tools (ping, traceroute, network scanner) prefer `waitForFinished(timeout)` over polling `state()` — polling doesn't tick without an event loop in worker threads. The network-scanner module has the canonical fix.

**i18n** — every user-visible string goes through `tr("中文")`. Source language is Chinese; `lupdate` collects keys into `translations/lele-tools_zh_CN.ts` and `_en_US.ts`. Don't use `type="unfinished"` markers — strip them with `sed -E 's/ type="unfinished"//g'` after running lupdate.

**Translations regen**:
```bash
lupdate -locations none ./ -ts translations/lele-tools_en_US.ts translations/lele-tools_zh_CN.ts
```

## Optional Dependencies (compile-time)

Many features are gated on CMake `WITH_*` flags. Code paths use `#ifdef WITH_FOO` guards so the build stays green when a dep is missing. When adding a feature that needs a new dep:

| Dep | Flag | Default | Notes |
|-----|------|---------|-------|
| MySQL client (libmariadb preferred over libmysqlclient) | `WITH_MYSQL` | ON | MariaDB Connector/C resolves the `mysql_native_password` removal in MySQL 9 |
| PostgreSQL libpq | `WITH_LIBPQ` | ON | for db-compare |
| Oracle OCI | `WITH_ORACLE_OCI` | **OFF** | License-restricted; user must install Oracle Instant Client and pass `-DORACLE_OCI_INCLUDE_DIR/LIBRARY` |
| OpenCV | `WITH_OPENCV` | ON | |
| Qt Multimedia | `WITH_QT_MULTIMEDIA` | ON | camera tool |
| Qt PDF | `WITH_QT_PDF` | ON | pdf-stamp renders pages via QPdfDocument |
| libssh | `WITH_LIBSSH` | ON | ssh-client + ftp-client |
| libpcap | `WITH_PCAP` | ON | network-scanner enhancement |
| VTK 9 | `WITH_VTK` | ON | scientific viz demo. Requires `find_package(jsoncpp)` first (VTK 9.5+ transitive dep). Also needs `QSurfaceFormat::setDefaultFormat(QVTKOpenGLNativeWidget::defaultFormat())` in `main.cpp` before `QApplication`. |

## CI

`.github/workflows/build-and-release.yml` produces Windows zip, macOS dmg, Debian/Ubuntu deb, Fedora rpm artifacts on push. Version scheme is `1.<run_number / 100>.<run_number % 100>` — patch caps at 99 then rolls minor. Linux apt/dnf install steps must list `qt6-svg-dev` / `qt6-qtsvg-devel` explicitly (CMake requires Qt6::Svg, and the base Qt 6 package on most distros doesn't include Svg by default).

The Windows MySQL Connector download uses a candidate-version fallback list (9.5.0 → 9.0.0 → 8.4.x → 8.0.x with cdn.mysql.com and dev.mysql.com mirrors) because Aliyun-style "old version removed" 404s have broken CI before.

## Workflow Notes from `.cursor/rules/basic-rules.mdc`

- Database operations go through the shared `SqliteManager` — don't open a new `QSqlDatabase` connection per module.
- After writing code, **don't** run a final compile to "verify" — the user prefers to build themselves to see errors in their own loop. (Note: this guideline is from cursor rules; in practice this repo's iteration has involved frequent test builds. Use judgment per session.)
- Source-language strings inside `tr()` are Chinese.
