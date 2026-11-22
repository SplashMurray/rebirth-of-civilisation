# Code signing policy

Free code signing provided by [SignPath.io](https://about.signpath.io), certificate by [SignPath Foundation](https://signpath.org).

> **Status:** Rebirth of Civilisation has applied to the SignPath Foundation open-source programme. Until the application is approved, release downloads are unsigned. This page will be updated when signed releases begin.

## What is signed

Only the Windows game executable built from this repository:

| File | Contents |
|---|---|
| `RebirthOfCivilisation.exe` | The game, built from the source code in this repository |

It is shipped inside `RebirthOfCivilisation-windows-x64.zip` on the [Releases page](https://github.com/SplashMurray/rebirth-of-civilisation/releases). The other files in the zip (`README.md`, `LICENSE`) are plain text and are not signed.

The game contains no third-party code and no proprietary components. It uses only Windows system libraries (user32, gdi32, GDI+, ole32) and the C++ standard library supplied by the compiler.

## How releases are built and signed

1. All source code and build scripts are in this public repository, under the [MIT licence](LICENSE).
2. Release builds are produced only by the GitHub Actions workflow [`.github/workflows/build.yml`](.github/workflows/build.yml), running on GitHub-hosted runners. No build is made on, or uploaded from, a personal computer.
3. The workflow uploads the unsigned build to SignPath, which verifies that it came from this repository's workflow.
4. **Every signing request is approved manually** by an approver listed below before the certificate is applied.
5. The signed executable is then published on the Releases page.

The product name (`Rebirth of Civilisation`) and version embedded in the executable come from the build system (`project(... VERSION ...)` in `CMakeLists.txt`), so they are identical for every file built for a given release.

## Team roles

This is a single-maintainer project.

| Role | Members | Responsibility |
|---|---|---|
| Authors | [SplashMurray](https://github.com/SplashMurray) | Trusted to commit changes to the source code without further review. |
| Reviewers | [SplashMurray](https://github.com/SplashMurray) | Review every contribution from anyone who is not an author (pull requests) before it is merged. |
| Approvers | [SplashMurray](https://github.com/SplashMurray) | Approve each signing request in SignPath. |

All team members use multi-factor authentication for GitHub and SignPath.

## Privacy policy

This program will not transfer any information to other networked systems unless specifically requested by the user or the person installing or operating it.

Rebirth of Civilisation never connects to the internet. It has no accounts, telemetry, analytics, advertising or automatic updates, and it collects no personal data. The only files it writes are saved games, stored on your own computer in `%APPDATA%\RebirthOfCivilisation`.

## Reporting a problem

If you believe a signed file was not built from this repository, or that a release misbehaves, please [open an issue](https://github.com/SplashMurray/rebirth-of-civilisation/issues). Problems with the certificate itself can also be reported to [SignPath Foundation](https://signpath.org).
