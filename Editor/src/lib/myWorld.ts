// My world's words (Docs/Design/78, sections 2, 7 and 12; Phase M5): the backups in the host's words, "Copy restore
// steps" in PowerShell and in bash, and the checks on the world folder. Pure functions, tested in myWorld.test.ts.

/** Why a backup was made, as the package's backup service names its kinds. */
export const BACKUP_WHY: Record<string, string> = {
    'daily': 'every day', 'by-hand': 'by hand', 'before-publish': 'before a map was published',
    'before-update': 'before an update', 'before-restore': 'before a restore',
};

/** "just now", "5 minutes ago", "3 h ago", "2 days ago" (seconds ago). */
export function ago(seconds: number): string {
    const s = Math.max(0, Math.round(seconds));
    if (s < 90) return 'just now';
    if (s < 90 * 60) return `${Math.round(s / 60)} minutes ago`;
    if (s < 36 * 3600) return `${Math.round(s / 3600)} h ago`;
    return `${Math.round(s / 86400)} days ago`;
}

/** "Last backup 3 h ago" (the menu's and My world's), from the newest backup's age in seconds (null: none yet). */
export const lastBackupLine = (secondsAgo: number | null | undefined) =>
    secondsAgo === null || secondsAgo === undefined ? 'No backup yet' : `Last backup ${ago(secondsAgo)}`;

/** A size in the host's words: "800 KB", "51 MB", "2.4 GB". */
export function size(bytes: number | null | undefined): string {
    if (bytes === null || bytes === undefined) return '?';
    if (bytes < 1024 * 1024) return `${Math.max(1, Math.round(bytes / 1024))} KB`;
    if (bytes < 1024 ** 3) return `${Math.round(bytes / 1024 ** 2)} MB`;
    return `${(bytes / 1024 ** 3).toFixed(1)} GB`;
}

/** A backup's refusal or failure, said plainly. */
export function backupProblem(p: {code: string; free: number | null} | null | undefined, minFree: number | null | undefined): string {
    if (!p) return '';
    switch (p.code) {
        case 'disk': return `The last backup was refused: the disk that holds the world folder's backups is nearly full (${size(p.free)} free;` +
            ` at least ${size(minFree ?? 2 * 1024 ** 3)} must stay free). Free some space, or delete old files from backups.`;
        case 'pins': return 'Five backups are pinned already: unpin one first.';
        case 'database': return 'The backup service couldn\'t reach the world\'s database.';
        default: return 'The last backup failed: in the world folder, docker compose logs backup says why.';
    }
}

/** Whether the world folder looks like one a cloud service copies (doc 78, 12.1: the dumps hold players' data and the
 *  Editor accounts' password hashes). */
export const syncedFolder = (folder: string) => /onedrive|dropbox|google ?drive|icloud|\bbox sync\b|mega(sync)?\b/i.test(folder);

/** Whether a folder is a Windows one (C:\...), so PowerShell's steps come first. */
export const windowsFolder = (folder: string) => /^[A-Za-z]:[\\/]/.test(folder);

/** "Copy restore steps" (doc 78, 12.2): the lines that put the world back to one backup, from the world folder. A
 *  backup's name is letters, digits and dashes only (anything else gives nothing); the folder is quoted for each shell. */
export function restoreSteps(folder: string, name: string): {powershell: string; bash: string} | null {
    if (!/^ratw-\d{8}T\d{6}Z-[a-z-]+$/.test(name)) return null;
    const ps = folder ? `Set-Location -LiteralPath '${folder.replace(/'/g, "''")}'\n` : '';
    const sh = folder ? `cd '${folder.replace(/'/g, `'\\''`)}' && ` : '';
    return {
        powershell: `${ps}.\\restore.bat ${name}`,
        bash: `${sh}./restore.sh ${name}`,
    };
}

/** An audit row's action in the host's words (My world's history). */
export function historyWords(action: string): string {
    const known: Record<string, string> = {
        'server.maintenance': 'Closed the world', 'server.running': 'Opened the world', 'server.restart': 'Asked for a restart',
        'server.backup': 'Asked for a backup', 'server.backup.pin': 'Pinned a backup', 'server.backup.unpin': 'Unpinned a backup',
        'account.add': 'Added an Editor account', 'account.reset': 'Reset an Editor account', 'account.role': 'Changed an account\'s role',
        'account.wolves': 'Changed who may act on players\' wolves', 'account.disable': 'Turned an account off',
        'account.enable': 'Turned an account on', 'account.remove': 'Removed an Editor account',
        'account.end-sessions': 'Ended every Editor session', 'account.password': 'Chose a new password',
    };
    const plain = action.replace(/[._]/g, ' ');
    return known[action] ?? plain.charAt(0).toUpperCase() + plain.slice(1);
}
