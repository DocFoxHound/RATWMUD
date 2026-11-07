// What this page speaks and bakes in (doc 77, 5.1), put in by the build (vite.config.ts, build-info.mjs). A world says
// its own in its lobby and GET /status; the client hides a world that differs. Outside a build (the unit tests) the
// protocol is today's and the hash unknown.
declare const __RATW_BUILD__: {protocol: number; clientData: string} | undefined;

export const ClientBuild: {protocol: number; clientData: string} =
    typeof __RATW_BUILD__ !== 'undefined' ? __RATW_BUILD__ : {protocol: 1, clientData: ''};
