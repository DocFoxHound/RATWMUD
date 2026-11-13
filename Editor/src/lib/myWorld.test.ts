import assert from 'node:assert/strict';
import {test} from 'node:test';
import {ago, backupProblem, historyWords, lastBackupLine, restoreSteps, SHARE_HOLDS, SHARE_NEVER, SHARE_NOT_YET, sharedPath, shareProblem, size,
    syncedFolder, windowsFolder} from './myWorld.ts';

test('the backups in the host\'s words (doc 78, 2 and 12.1)', () => {
    assert.deepEqual([0, 89, 300, 3 * 3600, 2 * 86400].map(ago), ['just now', 'just now', '5 minutes ago', '3 h ago', '2 days ago']);
    assert.equal(lastBackupLine(3 * 3600), 'Last backup 3 h ago');
    assert.equal(lastBackupLine(null), 'No backup yet');
    assert.deepEqual([800 * 1024, 51 * 1024 ** 2, 2.4 * 1024 ** 3, null].map(size), ['800 KB', '51 MB', '2.4 GB', '?']);
    assert.match(backupProblem({code: 'disk', free: 1024 ** 3}, 2 * 1024 ** 3), /nearly full \(1\.0 GB free; at least 2\.0 GB must stay free\)/);
    assert.equal(backupProblem({code: 'pins', free: null}, null), 'Five backups are pinned already: unpin one first.');
    assert.equal(backupProblem(null, null), '');
    assert.equal(historyWords('server.maintenance'), 'Closed the world');
    assert.equal(historyWords('character.kill'), 'Character kill');
});

test('"Copy restore steps" in PowerShell and bash, each shell\'s quoting (doc 78, 12.2)', () => {
    const name = 'ratw-20261112T140300Z-by-hand';
    assert.deepEqual(restoreSteps('C:\\Users\\Ann\'s PC\\ratw-world', name), {
        powershell: `Set-Location -LiteralPath 'C:\\Users\\Ann''s PC\\ratw-world'\n.\\restore.bat ${name}`,
        bash: `cd 'C:\\Users\\Ann'\\''s PC\\ratw-world' && ./restore.sh ${name}`});
    assert.deepEqual(restoreSteps('', name), {powershell: `.\\restore.bat ${name}`, bash: `./restore.sh ${name}`});
    assert.equal(restoreSteps('/home/ann', 'ratw-x; rm -rf ~'), null, 'only a backup\'s own name');
    assert.ok(syncedFolder('C:\\Users\\Ann\\OneDrive\\Documents\\ratw-world'));
    assert.ok(!syncedFolder('/home/ann/ratw-world'));
    assert.ok(windowsFolder('C:\\Users\\Ann') && !windowsFolder('/home/ann'));
});

test('Share my world in the host\'s words: what the file holds and never does, where it is, why it failed (doc 78, 10.5)', () => {
    const never = SHARE_NEVER.join(' ');
    for (const words of ['player', 'said or wrote', 'internet addresses', 'Editor accounts', 'passwords', 'host token', 'AI key', 'backups'])
        assert.ok(never.includes(words), `never: ${words}`);
    assert.ok(SHARE_HOLDS.some(line => line.startsWith('the map')) && /editing source too/.test(SHARE_NOT_YET), 'M7: the map as built and its editing source');
    assert.ok(!SHARE_HOLDS.join(' ').includes('player'), 'nothing of players in what it holds');
    assert.equal(sharedPath('C:\\Users\\Ann\\ratw-world\\', 'ashfall-2026-11-12.ratwpkg'), 'C:\\Users\\Ann\\ratw-world\\manage\\shared\\ashfall-2026-11-12.ratwpkg');
    assert.equal(sharedPath('/home/ann/ratw-world/', 'a-2026-11-12.ratwpkg'), '/home/ann/ratw-world/manage/shared/a-2026-11-12.ratwpkg');
    assert.equal(sharedPath('', 'a-2026-11-12.ratwpkg'), 'manage/shared/a-2026-11-12.ratwpkg in your world folder');
    assert.match(shareProblem({code: 'disk', free: 300 * 1024 ** 2, need: 1280 * 1024 ** 2, where: 'docker'}),
        /the disk Docker keeps its storage on is nearly full \(300 MB free; making the file needs about 1\.3 GB\)/);
    assert.equal(shareProblem({code: 'disk', free: null, need: null, where: null}), 'The file wasn\'t made: the disk that holds the world folder is ' +
        'nearly full. Free some space, or delete old files from manage/shared, then try again.');
    assert.match(shareProblem({code: 'failed', free: null, need: null, where: null}), /RATW_SHARE/);
    assert.match(shareProblem({code: 'folder', free: null, need: null, where: null}), /manage folder/);
    assert.equal(shareProblem(null), '');
    assert.equal(historyWords('server.share'), 'Shared the world (a file without its players)');
});
