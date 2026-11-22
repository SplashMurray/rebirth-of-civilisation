# SignPath Foundation application - ready-to-use answers

Apply at <https://signpath.org/apply>. Below are answers for the usual application questions. Adjust anything the form asks differently.

| Question | Answer |
|---|---|
| Project name | Rebirth of Civilisation |
| Repository URL | https://github.com/SplashMurray/rebirth-of-civilisation |
| Project home page | https://github.com/SplashMurray/rebirth-of-civilisation |
| Licence | MIT (OSI-approved). The `LICENSE` file is in the repository root. No dual licensing and no proprietary components. |
| Download page | https://github.com/SplashMurray/rebirth-of-civilisation/releases (free) |
| Code signing policy | https://github.com/SplashMurray/rebirth-of-civilisation/blob/main/CODE_SIGNING_POLICY.md |
| Privacy policy | The "Privacy policy" section of the code signing policy. The game never connects to the internet. |
| Build system | GitHub Actions on GitHub-hosted Windows runners: `.github/workflows/build.yml` |
| Files to sign | `RebirthOfCivilisation.exe` (64-bit Windows GUI executable), distributed in `RebirthOfCivilisation-windows-x64.zip` |
| Team | Single maintainer: SplashMurray (author, reviewer and approver), using multi-factor authentication on GitHub and SignPath |

## Project description (paste into the form)

> Rebirth of Civilisation is a free, open-source, turn-based 4X strategy game for Windows in the style of the classic Civilization series. In 2034 humanity destroys itself in a nuclear war, and the player leads a primate species (chimpanzees, bonobos, orangutans, gorillas and others) as it evolves language and rebuilds civilisation, salvaging technology from the ruins of the human world.
>
> It is written in C++17 with no third-party dependencies: the game uses only the Windows API (Win32, GDI+) and the C++ standard library. It is a single executable that needs no installation, makes no network connections and collects no data.
>
> Releases are built only by GitHub Actions from the public repository, which also runs automated tests (full AI-vs-AI simulated games) on every build. The first release (v0.1.0) is available on the GitHub Releases page. We would like to sign `RebirthOfCivilisation.exe` so that Windows 11 Smart App Control and SmartScreen do not block it for players.

## After approval: setting up SignPath

SignPath's onboarding instructions take precedence. These are the pieces the repository already expects:

1. In SignPath, add the predefined **GitHub.com** trusted build system and link it to the project. Install the **SignPath GitHub App** for this repository.
2. Create the project with slug `rebirth-of-civilisation` and a signing policy with slug `release-signing`. If you choose other slugs, set the repository variables below to match.
3. Use [`artifact-configuration.xml`](artifact-configuration.xml) from this folder as the project's artifact configuration.
4. Create an API token for a user with submitter rights on the signing policy.
5. In GitHub, open **Settings → Secrets and variables → Actions** for the repository and add:
   - Secret `SIGNPATH_API_TOKEN`: the API token
   - Variable `SIGNPATH_ORGANIZATION_ID`: your SignPath organisation ID
   - Optional variables `SIGNPATH_PROJECT_SLUG` and `SIGNPATH_POLICY_SLUG`, if your slugs differ from the defaults above
6. To make a signed release, go to **Actions → Build → Run workflow**, tick **release**, and run it. Approve the signing request in SignPath within an hour. The workflow then publishes the signed zip on the Releases page.

Once `SIGNPATH_ORGANIZATION_ID` is set, ordinary pushes still build and test the game but no longer overwrite the release, so the download always stays the signed one. To start a new version, change `VERSION` in `CMakeLists.txt`.
