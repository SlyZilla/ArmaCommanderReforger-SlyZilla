# Contributing

Bug reports, reproducible test cases, documentation, scenarios and focused code
changes are welcome. Before starting a feature, check the pinned **Roadmap** issue
and the open issues labelled `in progress`: those are already being worked on. To take
something on, comment on its issue (or open one) so it can be assigned to you, and for a
substantial feature discuss it there or in [Discord](https://discord.gg/WK8xGSztB) first.
Push work in progress early as a draft pull request so others can see it.

## Development and releases

`main` is the shared development branch. It can include unfinished or unverified
changes ahead of Workshop. The README identifies the published Workshop version;
[CHANGELOG.md](CHANGELOG.md) separates unreleased work from released changes.
A source push does not publish a Workshop update. Official Workshop publication
is handled by the maintainer.

## Current development limitation

At the 23 October 2026 source sync, native regression reported 108 passes,
four failures in the roadblock placement fixture, and one expected skip.
The fixture did not find a suitable road segment; placement, physical composition
alignment, vehicle obstruction and roadblock-spacing checks remain unresolved.
Do not treat this development snapshot as release acceptance or dismiss those
failures when testing related changes.

## Get your own working copy

Install Git, Arma Reforger and **Arma Reforger Tools** (Workbench) through Steam.
Use the regular game/tools versions together. Python 3 is used for source checks.
Fork [this repository](https://github.com/propst-design/ArmaCommanderReforger) on GitHub, then run
these commands from your Workbench `addons` directory (replace `YOUR-USERNAME`):

```powershell
git clone https://github.com/YOUR-USERNAME/ArmaCommanderReforger.git ArmaCommanderReforger
cd ArmaCommanderReforger
git remote add upstream https://github.com/propst-design/ArmaCommanderReforger.git
git switch -c fix-my-change
```

Keep the addon folder name shown above. Open Workbench Launcher, add the base-game
project if it is missing, then use **Add Existing Project** to register the cloned
`addon.gproj`. See Bohemia's
[project setup guide](https://community.bistudio.com/wiki/Arma_Reforger:Mod_Project_Setup).
Avoid having multiple copies with the same addon GUID in an active addon search
path. Confirm your test loads your source copy rather than an installed Workshop
copy. Preserve existing GUIDs and commit metadata for any new resources.

Clone BBB next to ACA before opening the ACA project:

```powershell
cd ..
git clone https://github.com/propst-design/BetterBattleBuddies.git BetterBattleBuddies
git -C BetterBattleBuddies checkout (Get-Content ArmaCommanderReforger/tools/bbb-source.json -Raw | ConvertFrom-Json).commit
cd ArmaCommanderReforger
```

Use the BBB commit recorded in `tools/bbb-source.json` for a reproducible pair.
If your change also needs BBB changes, use a BBB work branch and link both pull
requests. The maintainer updates the pin when accepting the paired changes.
BBB must remain independent of ACA; integration belongs in `ACR_BBBHooks.c`.
Register both projects in Workbench and open BBB once to build its resource database.
Then open ACA's `addon.gproj`. Start with `Worlds/ACR_TestRange.ent` in World Editor;
use `Play-Test-Range.cmd` for a game-client run with the local source. The other
Play/Host launchers may offer optional faction mods; vanilla testing needs only BBB.
AiO is optional and can be cloned alongside the other two projects.

Relevant source: `Scripts/Game/ACR`, tests: `Scripts/Game/ACR_Tests`, scenarios:
`Worlds`, resources: `Configs` and `Prefabs`. Mission settings are documented in
[MISSION-MAKING.md](MISSION-MAKING.md).

## Keep your work independent

Your branch and local files stay unchanged when the maintainer pushes to `main`.
Bring newer upstream changes in deliberately, after committing your own work:

```powershell
git fetch upstream
git merge upstream/main
```

Resolve any conflicts and retest. Do not force-push the shared `main`. For each
new task, start a fresh branch from an updated main:

```powershell
git switch main
git pull --ff-only upstream main
git switch -c another-fix
```

## Implement and test

Preserve server authority: clients send intent and the server validates ownership,
permissions and resources. Keep orders in the existing order/state machinery;
failed purchases must refund. Soldier combat behavior belongs in BBB and the
host policy belongs in ACA. Keep new mission options documented.

Preserve resource GUIDs, license notices and existing line endings. Keep changes
focused and avoid unrelated formatting. Enforce Script must compile in Workbench;
be careful with engine API signatures and avoid unsupported ternary expressions.

Run the mission-document check from the repository root:

```powershell
python tools/check_mission_doc.py
```

For native server tests, install **Arma Reforger Server** through Steam and run
`Start-Test-Launcher.cmd`. In a second PowerShell window at the repository root:

```powershell
$id = 'contrib-' + (Get-Date -Format 'yyyyMMdd-HHmmss')
powershell -NoProfile -File tools/runtest.ps1 $id srv:regress 40
```

Presets are listed in `tools/test-presets.txt`; use `srv:<preset>` for hidden
server tests. Run relevant feature tests too. For changes that scan soldiers,
vehicles or squads, run `srv:perf` with a fresh ID and 60 polls. Performance runs
can take over three minutes. Read the result: a completed performance measurement
without a strict verdict is not a gameplay PASS. If port 2001 is occupied, wait
for its owner to finish; do not stop another person's game/server to run a test.

For multiplayer changes, include a joined-client check when possible and state
what remains untested. Distinguish compilation, test completion, physical gameplay
results and multiplayer acceptance. A blocked test is not a passing test.

Before submitting, stage only the intended files and check them:

```powershell
git diff --cached --stat
python tools/check_public_source.py
git diff --cached --check
```

The source check also runs on GitHub pull requests and pushes. It catches common
private/generated files and recognizable sensitive text; it cannot replace a
manual review or compile/test the game.

## Submit a pull request

Commit your focused change, push your work branch to your fork, then open a pull
request with **base: `propst-design/ArmaCommanderReforger`, branch: `main`**. Include the problem,
resulting behavior, reproduction steps, source commits, game/mod versions, tests
and remaining limitations. Draft pull requests are welcome for work in progress.
For paired changes in another mod, link the other pull request. The maintainer
reviews and merges accepted changes; no direct write access is required.
See [GitHub's contribution guide](https://docs.github.com/en/get-started/exploring-projects-on-github/contributing-to-a-project).

## Public files and licensing

Submit source, resource metadata, tests and useful contributor/player docs.
Keep generated caches, logs, local settings, credentials, personal paths and private
project records out of commits. Review staged file names and content before pushing;
sanitize any diagnostic excerpts you share. Do not commit extracted base-game or
third-party mod assets. By submitting a contribution, you agree to distribute it
under this project's existing [license](LICENSE.md); retain required attribution.
