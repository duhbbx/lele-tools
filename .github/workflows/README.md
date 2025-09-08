# GitHub Actions Workflows

This directory contains GitHub Actions workflows for automated building, testing, and releasing of the Lele Tools project with redis-plus-plus integration.

## Workflows Overview

### 1. `build-and-release.yml` - Main Build and Release Pipeline
- **Trigger**: Push to main/master branches, tags, pull requests, manual dispatch
- **Purpose**: Build the application, create installers, and publish releases
- **Platforms**: Windows (primary), Linux (AppImage)
- **Dependencies**: Qt 6.9.1, redis-plus-plus (via vcpkg), OpenCV (optional)
- **Outputs**: 
  - Windows executable with Qt dependencies
  - Windows installer package (using Qt Installer Framework)
  - Windows portable ZIP package
  - Linux AppImage package
  - GitHub Release with artifacts

### 2. `manual-build.yml` - Manual Build Workflow
- **Trigger**: Manual dispatch only
- **Purpose**: On-demand builds with customizable parameters
- **Features**:
  - Configurable version number
  - Build type selection (Debug/Release)
  - Optional installer creation
  - Optional artifact upload
  - redis-plus-plus integration via vcpkg

### 3. `ci.yml` - Continuous Integration
- **Trigger**: Push/PR to main, master, dev, develop branches
- **Purpose**: Fast feedback on code changes
- **Features**:
  - Multi-platform testing (Windows, Linux)
  - Multiple build types (Debug, Release)
  - Code quality checks
  - Dependency validation

## Key Features

### Dependencies Management
- **Qt Framework**: Automatically installs Qt 6.9.1 with Multimedia modules
- **redis-plus-plus**: Installed via vcpkg for Redis connectivity
- **vcpkg**: Package manager for C++ dependencies
- **Qt Installer Framework**: For creating installation packages
- **OpenCV**: Optional computer vision library support
- **MSVC 2022**: Microsoft Visual C++ compiler (Windows)
- **GCC**: GNU Compiler Collection (Linux)

### Build Process
1. **Environment Setup**: Install Qt, vcpkg, CMake, and platform dependencies
2. **Dependency Installation**: Install redis-plus-plus via vcpkg
3. **Project Configuration**: Configure CMake with vcpkg toolchain
4. **Compilation**: Build the project with Redis support enabled
5. **Deployment**: Bundle Qt and Redis dependencies
6. **Packaging**: Create platform-specific packages

### Caching Strategy
- **Qt Installation**: Cached across builds for faster setup
- **vcpkg**: Separate cache for package manager and installed packages
- **OpenCV**: Cached installation to reduce build time
- **Multi-level Cache Keys**: Optimized cache invalidation strategy

### Multi-Platform Support

#### Windows
- **Build System**: Visual Studio 2022 + vcpkg
- **Package Format**: ZIP (portable) + EXE (installer)
- **Dependencies**: Bundled with windeployqt + Redis DLLs

#### Linux
- **Build System**: GCC + system packages
- **Package Format**: AppImage (portable)
- **Dependencies**: System packages for redis-plus-plus

## Configuration

### Environment Variables
```yaml
QT_VERSION: '6.9.1'          # Qt version with Multimedia support
CMAKE_VERSION: '3.28'        # CMake version
MSVC_VERSION: '2022'         # MSVC version (Windows)
OPENCV_VERSION: '4.10.0'     # OpenCV version (optional)
```

### Build Configuration
- **Redis Support**: Enabled by default via `-DENABLE_REDIS=ON`
- **OpenCV Support**: Optional via `-DWITH_OPENCV=ON/OFF`
- **Multimedia**: Qt Multimedia modules for camera functionality

### vcpkg Configuration
- **Windows**: Uses vcpkg for redis-plus-plus installation
- **Linux**: Uses system packages (libhiredis-dev, libsw-redis++-dev)
- **Caching**: Separate cache for vcpkg installation and packages

## Usage

### Automatic Builds
- **Push to main/master**: Triggers full multi-platform build and creates pre-release
- **Push tags (v*)**: Triggers build and creates stable release
- **Pull Requests**: Triggers CI build without release creation

### Manual Builds
1. Go to Actions tab in GitHub repository
2. Select "Manual Build" workflow
3. Click "Run workflow"
4. Configure parameters:
   - Version number (e.g., "1.2.0")
   - Build type (Release/Debug)
   - Create installer (true/false)
   - Upload artifacts (true/false)

### Release Process
1. **Tag Creation**: Create and push a version tag
   ```bash
   git tag v1.0.0
   git push origin v1.0.0
   ```
2. **Multi-Platform Build**: Workflows trigger automatically
3. **Release Creation**: GitHub release with all platform artifacts
4. **Artifact Types**:
   - `LeleTools_vX_X_X_Portable.zip` (Windows portable)
   - `LeleToolsInstaller_vX_X_X.exe` (Windows installer)
   - `LeleTools-X.X.X-x86_64.AppImage` (Linux portable)

## Troubleshooting

### Common Issues

#### vcpkg Installation Failures
- Check vcpkg cache corruption
- Verify redis-plus-plus package availability
- Review vcpkg installation logs

#### Redis Integration Issues
- Ensure vcpkg toolchain is properly configured
- Check Redis DLL bundling in Windows builds
- Verify redis-plus-plus system packages in Linux

#### Qt Multimedia Issues
- Verify Qt modules include qtmultimedia qtmultimediawidgets
- Check platform-specific multimedia dependencies
- Test camera functionality on target platforms

#### Build Failures
- Review CMake configuration with vcpkg toolchain
- Check dependency availability on target platform
- Verify all required modules are installed

### Debug Information
All workflows include extensive logging:
- vcpkg installation and package status
- CMake configuration with toolchain details
- Dependency bundling verification
- Multi-platform build status

## Platform-Specific Notes

### Windows
- **vcpkg Integration**: Uses vcpkg toolchain for redis-plus-plus
- **DLL Bundling**: Automatically includes Redis DLLs from vcpkg
- **Qt Deployment**: Enhanced windeployqt with Redis dependencies

### Linux
- **System Packages**: Uses distribution packages for Redis
- **AppImage Creation**: Portable Linux packages with linuxdeploy
- **Dependency Resolution**: Automatic Qt and Redis library bundling

## Security and Maintenance

### Dependency Security
- **Pinned Versions**: All major dependencies use specific versions
- **Regular Updates**: Quarterly updates for Qt and annual for build tools
- **Vulnerability Scanning**: Monitor dependency security advisories

### Cache Management
- **Multi-tier Caching**: Separate caches for Qt, vcpkg, and OpenCV
- **Cache Invalidation**: Version-based cache keys for proper invalidation
- **Storage Optimization**: Efficient cache size management

### Performance Optimization
- **Parallel Builds**: Multi-core compilation enabled
- **Smart Caching**: Conditional dependency installation
- **Artifact Optimization**: Compressed packages with optimal retention

## Extending Workflows

### Adding New Dependencies
1. Update vcpkg package list or system packages
2. Modify CMake configuration flags
3. Update dependency bundling steps
4. Test on all target platforms

### Adding macOS Support
1. Add macOS job with appropriate Qt installation
2. Install redis-plus-plus via Homebrew
3. Create macOS application bundle
4. Add code signing and notarization

### Custom Deployment Targets
1. Add deployment steps after successful builds
2. Configure target-specific credentials
3. Implement rollback mechanisms
4. Add deployment status monitoring