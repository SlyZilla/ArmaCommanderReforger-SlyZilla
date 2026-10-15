"""Destructive cases run only in temporary fixture repositories, never real mod folders."""
from pathlib import Path
import contextlib
import io
import json
import subprocess
import tempfile
import unittest
from unittest.mock import patch

from release_console import Console, SPECS, Blocked, digest, write_json, read_json
import package_guard


def run_git(root, *args):
    return subprocess.check_output(['git', '-C', str(root), *args], text=True).strip()


class GuardTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.base = Path(self.temp.name)
        addons = self.base / 'addons'
        for mod, spec in SPECS.items():
            root = addons / spec['folder']
            root.mkdir(parents=True)
            run_git(root, 'init', '-q')
            run_git(root, 'config', 'user.email', 'fixture@example.invalid')
            run_git(root, 'config', 'user.name', 'Fixture')
            (root / '.gitignore').write_text('.release-console/\nresourceDatabase.rdb\n')
            for name in spec['files'] + ['resourceDatabase.rdb']:
                (root / name).write_text(name)
            deps = f'"{SPECS["BBB"]["guid"]}"' if mod == 'ACA' else ''
            (root / 'addon.gproj').write_text(f'GameProject {{\n GUID "{spec["guid"]}"\n TITLE "Development"\n Dependencies {{ {deps} }}\n}}')
            for directory in spec['dirs']:
                target = root / directory / 'fixture.c'
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_text('class Fixture {}\n')
            tools = root / 'tools'
            tools.mkdir()
            (tools / 'check_mission_doc.py').write_text('print("fixture docs pass")')
            run_git(root, 'add', '.')
            run_git(root, 'commit', '-qm', 'fixture')
        self.root = addons / SPECS['ACA']['folder']
        self.console = Console(self.root)
        self.bbb = self.console.source('BBB')
        write_json(self.root / 'tools/bbb-source.json', {'commit': run_git(self.bbb, 'rev-parse', 'HEAD')})
        self.commit(self.root)

    def tearDown(self):
        self.temp.cleanup()

    def commit(self, root):
        run_git(root, 'add', '.')
        run_git(root, 'commit', '-qm', 'update')

    def mark_checked(self, mod):
        parent = self.console.stage(mod)
        write_json(parent / 'STARTUP-CHECK.json', {'manifest': digest(parent / 'WORKSHOP-BUILD.json')})
        write_json(self.console.pointer(mod), {'candidate': str(parent)})
        return parent

    def test_fresh_candidate_and_unique_rebuild(self):
        first = self.mark_checked('BBB')
        self.assertEqual(first, self.console.checked('BBB'))
        second = self.console.stage('BBB')
        self.assertNotEqual(first, second)
        self.assertTrue(first.is_dir())

    def test_dirty_source_refused(self):
        (self.bbb / 'README.md').write_text('unsaved')
        with self.assertRaisesRegex(Blocked, 'unsaved source'):
            self.console.stage('BBB')

    def test_source_commit_advance_refused(self):
        self.mark_checked('BBB')
        (self.bbb / 'README.md').write_text('next')
        self.commit(self.bbb)
        with self.assertRaisesRegex(Blocked, 'moved on'):
            self.console.checked('BBB')

    def test_candidate_edit_refused(self):
        parent = self.mark_checked('BBB')
        (parent / 'BetterBattleBuddies/README.md').write_text('edited release')
        with self.assertRaisesRegex(Blocked, 'edited or damaged'):
            self.console.checked('BBB')

    def test_extra_and_missing_candidate_files_refused(self):
        parent = self.mark_checked('BBB')
        extra = parent / 'BetterBattleBuddies/stale.c'
        extra.write_text('stale')
        with self.assertRaisesRegex(Blocked, 'file set changed'):
            self.console.checked('BBB')
        extra.unlink()
        (parent / 'BetterBattleBuddies/README.md').unlink()
        with self.assertRaisesRegex(Blocked, 'edited or damaged'):
            self.console.checked('BBB')

    def test_no_startup_receipt_refused(self):
        parent = self.console.stage('BBB')
        write_json(self.console.pointer('BBB'), {'candidate': str(parent)})
        with self.assertRaises(FileNotFoundError):
            self.console.checked('BBB')

    def test_failed_startup_never_promotes_candidate(self):
        parent = self.console.stage('BBB')
        with patch('release_console.subprocess.run', side_effect=subprocess.CalledProcessError(1, 'startup')):
            with self.assertRaises(subprocess.CalledProcessError):
                self.console.smoke(parent)
        self.assertFalse(self.console.pointer('BBB').exists())

    def test_wrong_mod_pointer_refused(self):
        parent = self.mark_checked('AIO')
        write_json(self.console.pointer('BBB'), {'candidate': str(parent)})
        with self.assertRaisesRegex(Blocked, 'different mod'):
            self.console.checked('BBB')

    def test_changed_manifest_invalidates_startup(self):
        parent = self.mark_checked('BBB')
        manifest = read_json(parent / 'WORKSHOP-BUILD.json')
        manifest['builtUtc'] = 'changed'
        write_json(parent / 'WORKSHOP-BUILD.json', manifest)
        with self.assertRaisesRegex(Blocked, 'Startup check'):
            self.console.checked('BBB')

    def test_ignored_resource_database_change_refused(self):
        self.mark_checked('BBB')
        (self.bbb / 'resourceDatabase.rdb').write_text('rebuilt')
        with self.assertRaisesRegex(Blocked, 'source content changed'):
            self.console.checked('BBB')

    def test_workbench_regenerated_candidate_index_allowed_but_missing_refused(self):
        parent = self.mark_checked('BBB')
        index = parent / 'BetterBattleBuddies/resourceDatabase.rdb'
        index.write_bytes(b'Workbench regenerated resource index')
        self.assertEqual(parent, self.console.checked('BBB'))
        index.unlink()
        with self.assertRaisesRegex(Blocked, 'edited or damaged'):
            self.console.checked('BBB')

    def test_output_inside_addons_refused(self):
        with self.assertRaisesRegex(Blocked, 'OUTSIDE'):
            Console(self.root, self.root.parent / '_bad-release')

    def test_wrong_bbb_pin_refused(self):
        write_json(self.root / 'tools/bbb-source.json', {'commit': '0' * 40})
        self.commit(self.root)
        with self.assertRaisesRegex(Blocked, 'pinned dependency'):
            self.console.stage('ACA')

    def test_aca_test_scripts_excluded(self):
        test = self.root / 'Scripts/Game/ACR_Tests/test.c'
        test.parent.mkdir(parents=True)
        test.write_text('test')
        self.commit(self.root)
        parent = self.console.stage('ACA')
        self.assertFalse((parent / 'ArmaCommanderReforger/Scripts/Game/ACR_Tests').exists())

    def test_installed_missing_and_stale_blocked(self):
        self.mark_checked('BBB')
        installed = self.base / 'downloaded'
        installed.mkdir()
        with self.assertRaisesRegex(Blocked, 'downloaded Workshop copy'):
            self.console.installed('BBB', installed)
        copy = installed / 'BBB'
        copy.mkdir()
        (copy / 'addon.gproj').write_text(f'GUID "{SPECS["BBB"]["guid"]}"')
        with self.assertRaisesRegex(Blocked, 'file set differs'):
            self.console.installed('BBB', installed)

    def test_installed_content_match_crlf_and_stale_bytes(self):
        parent = self.mark_checked('BBB')
        installed = self.base / 'downloaded'
        copy = installed / 'BBB'
        copy.mkdir(parents=True)
        (copy / 'addon.gproj').write_text(f'GUID "{SPECS["BBB"]["guid"]}"')
        manifest = read_json(parent / 'WORKSHOP-BUILD.json')
        index = {}
        payload = bytearray()
        pak = copy / 'data.pak'
        for item in manifest['files']:
            if item['path'].startswith(('Scripts/', 'Configs/')) and item['path'].endswith(('.c', '.conf')):
                data = (Path(manifest['addonRoot']) / item['path']).read_bytes().replace(b'\r\n', b'\n').replace(b'\n', b'\r\n')
                index[item['path']] = [str(pak), len(payload), len(data), len(data), 0]
                payload.extend(data)
        pak.write_bytes(payload)
        def fake_index(directory, path):
            write_json(Path(path), index)
            print('index progress must not contaminate the project path')
        with patch('pakread.index', side_effect=fake_index):
            output = io.StringIO()
            with contextlib.redirect_stdout(output):
                self.console.installed('BBB', installed)
            self.assertEqual('', output.getvalue())
            pak.write_bytes(b'X' + payload[1:])
            with self.assertRaisesRegex(Blocked, 'content is stale'):
                self.console.installed('BBB', installed)

    def test_duplicate_installed_mod_refused(self):
        self.mark_checked('BBB')
        installed = self.base / 'downloaded'
        for name in ['copy1', 'copy2']:
            copy = installed / name
            copy.mkdir(parents=True)
            (copy / 'addon.gproj').write_text(f'GUID "{SPECS["BBB"]["guid"]}"')
        with self.assertRaisesRegex(Blocked, 'found 2'):
            self.console.installed('BBB', installed)

    def test_aca_receipt_binds_exact_bbb_candidate(self):
        bbb = self.mark_checked('BBB')
        aca = self.mark_checked('ACA')
        receipt = read_json(aca / 'STARTUP-CHECK.json')
        receipt['dependencyManifest'] = digest(bbb / 'WORKSHOP-BUILD.json')
        write_json(aca / 'STARTUP-CHECK.json', receipt)
        self.assertEqual(aca, self.console.checked('ACA'))
        self.mark_checked('BBB')
        with self.assertRaisesRegex(Blocked, 'different BBB candidate'):
            self.console.checked('ACA')

    def test_package_provenance_rejects_replaced_zip(self):
        dist = self.root / 'dist'
        dist.mkdir()
        # Keep fixture output ignored, as it is in the real repository.
        with (self.root / '.gitignore').open('a') as handle:
            handle.write('dist/\n')
        self.commit(self.root)
        archive = dist / 'ACActual-WithBBB.zip'
        archive.write_bytes(b'original')
        (dist / 'VERSION.txt').write_text('fixture')
        sha = run_git(self.bbb, 'rev-parse', 'HEAD')
        write_json(dist / 'PACKAGE-BUILD.json', dict(commit=run_git(self.root, 'rev-parse', 'HEAD'),
                   bbbCommit=sha, version='fixture', sha256=digest(archive)))
        with patch.object(package_guard, 'bbb_source', return_value=(self.bbb, sha)):
            package_guard.verify(self.root)
            archive.write_bytes(b'wrong archive')
            with self.assertRaisesRegex(Blocked, 'replaced or edited'):
                package_guard.verify(self.root)


if __name__ == '__main__':
    unittest.main()
