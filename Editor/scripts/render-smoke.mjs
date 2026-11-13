// Builds src/render.smoke.tsx for Node (vite, server side, React bundled in) into a temporary folder, renders every
// economy panel and the Prosperity tab, and checks each said what it shows. `npm run smoke`.
import {build} from 'vite';
import {mkdtempSync, rmSync} from 'node:fs';
import {tmpdir} from 'node:os';
import {join, dirname} from 'node:path';
import {fileURLToPath, pathToFileURL} from 'node:url';
import assert from 'node:assert/strict';

const root = join(dirname(fileURLToPath(import.meta.url)), '..');
const out = mkdtempSync(join(tmpdir(), 'ratw-s2stage-render-'));
try {
    await build({root, configFile: join(root, 'vite.config.ts'), logLevel: 'error',
        build: {ssr: 'src/render.smoke.tsx', outDir: out, emptyOutDir: true, rollupOptions: {input: 'src/render.smoke.tsx'}},
        ssr: {noExternal: true}});
    const {renderAll} = await import(pathToFileURL(join(out, 'render.smoke.js')).href);
    const html = renderAll();
    const expect = {
        'Steer': ['-45 (lean times)', 'Set the times'], 'Rules': ['wagons on the roads, 0-300% of normal', 'Harvest bias'],
        'Seasons': ['grain and flour'], 'Imports': ['Cheese', '0 never carried'], 'Status': ['The daily caps'],
        'Market': ['the House wedding'],
        'Imports (before 0061)': ['0061'], 'Status (before 0061)': ['0061'], 'Market (before 0061)': ['0061'],
        'Prosperity': ['Greyfen', '11:30', 'refused by the game', 'the Gull', 'The orchards', 'Trencher bread', "The realm's treasury"],
        'Prosperity (no town purses)': ["No town keeps a purse of its own in this world: the realm's treasury is the only one"],
        // (Doc 78, M5: My world, the owner's with the folder and the stops, a co-builder's without.)
        'My world (owner)': ['7 wolves in the world', 'Last backup 3 h ago', 'Back up now', 'Copy restore steps', 'nearly full',
            'The world folder', 'a cloud service copies', 'The world keeps stopping', '3F2A 9C10', 'Asked for a backup', 'Something went wrong?'],
        'My world (dm)': ['Your sign-ins', 'are for its owner', 'Only the world&#x27;s owner can do this.'],
    };
    const absent = {'My world (dm)': ['The world folder', 'OneDrive', 'exit 139']};
    if (process.env.RENDER_DUMP) for (const [name, text] of Object.entries(html)) console.log(`== ${name}\n${text.replace(/<[^>]+>/g, " ").replace(/\s+/g, " ").slice(0, 700)}`);
    for (const [name, text] of Object.entries(html)) {
        assert.ok(text.length > 100, `${name}: rendered`);
        const plain = text.replace(/<!--.*?-->/g, '').replace(/<[^>]+>/g, ' ').replace(/&#x27;/g, "'").replace(/\s+/g, ' ');
        for (const words of expect[name] ?? [])
            assert.ok(plain.includes(words) || text.includes(words), `${name}: shows "${words}"`);
        for (const words of absent[name] ?? [])
            assert.ok(!plain.includes(words), `${name}: doesn't show "${words}"`);
    }
    console.log(`render smoke: ${Object.keys(html).length} panels rendered: ${Object.keys(html).join(', ')}`);
} finally {
    rmSync(out, {recursive: true, force: true});
}
