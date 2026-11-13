import assert from 'node:assert/strict';
import {test} from 'node:test';
import {ago, backupProblem, historyWords, lastBackupLine, restoreSteps, size, syncedFolder, windowsFolder} from './myWorld.ts';

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
