// JSON-lines driver used by the native scene integration test. Each viewer is a real game connection.
import {createInterface} from 'node:readline';
import {Connection} from '../../Client/src/net/connection.ts';
import type {Pose} from '../../Client/src/net/motion.ts';
import type {Json} from '../../Client/src/net/session.ts';

const port = process.argv[2];
const positions = new Map<string, Pose>();
const viewers = new Map<string, Connection>();
for await (const line of createInterface({input: process.stdin})) {
    try {
        const {op, identity, command} = JSON.parse(line);
        let connection = viewers.get(identity);
        let result: unknown = true;
        if (op === 'connect') {
            if (connection) throw Error('Already connected');
            connection = new Connection('ws://127.0.0.1:' + port + '/ws', {
                lobby() {}, enter() {}, artwork() {}, snapshot() {}, event() {},
                motion(frame) {
                    const self = frame.entities.find(pose => pose.id === frame.observer);
                    if (self) positions.set(identity, self);
                },
            }, open => {
                if (open) connection!.submit({type:'hello', id:identity, name:identity});
            });
            viewers.set(identity, connection);
        } else if (connection && op === 'read') {
            const snapshot = connection.session.latestSnapshot;
            const self = snapshot?.self as Json|undefined;
            result = {id:self?.id, scene:self?.scene ?? null, party:self?.party, journal:self?.journal ?? snapshot?.journal, position:positions.get(identity)};
        } else if (connection && op === 'command') {
            connection.submit(command);
        } else if (connection && op === 'close') {
            connection.close();
            viewers.delete(identity);
        } else throw Error('Unrecognised operation or viewer');
        process.stdout.write(JSON.stringify({result})+'\n');
    } catch (e) {
        process.stdout.write(JSON.stringify({error:String(e)})+'\n');
    }
}
for (const connection of viewers.values()) connection.close();
