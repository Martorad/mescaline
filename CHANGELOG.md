# Changelog

All notable user-visible changes to Mescaline will be documented in this file.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/). The project will use
[Semantic Versioning](https://semver.org/) once its first version is assigned.

## Unreleased

### Added

- Initial project README with current build and usage instructions.
- Repository-wide contributor and agent guidance.
- Draft CLI, output, expression, GUI, and release contracts.
- Ordered implementation roadmap.
- CTest-driven CLI regression suite covering all current algorithms, parsing, output, paths,
  deterministic rendering, frame generation, and known defects.
- Optional AddressSanitizer and UndefinedBehaviorSanitizer build configuration.

### Changed

- Selected GPL-3.0 as the project license for the first release.
- Raised the declared CMake minimum to 3.21 and made C23 support mandatory.
