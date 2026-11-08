# A running pre-79 DEV save (not yet wired into save_compat)

Made 2026-11-08 at `cab65ee` (before doc 79): `ratw_server --world-export <DEV build 70, exported read-only>
--save <a copy of ../dev/checkpoint.json.gz> --bind 127.0.0.1 --port 0 --speed 80 --for 1500`, file mode, no
database, no AI. Ran from day 0 12:06 to day 3 04:04 (past 18:00 and midnight three times). Holds `society.loop`
(prices, sales, wages, level, the Wharf's figures), `outside:wharf` 707p, the treasury 2,015p, 130 `till:` accounts
(17,343p), 4 `estate:` accounts, 559 resident purses (33,080p); minted 54,755, sunk 1,265 (conserved). No players.
No `loop.steer`, `fund:relief` or `farm:` accounts: DEV's settings never switch them on. `server.log` is the run's log.
To do (doc 79 Progress, the pause): move it beside the other fixtures and add it to `save_compat`.
