# LAN Games Website — prototype

Multi-game browser lobby and Warlock-inspired multiplayer arena prototype. The starter is dependency-light: Python `aiohttp` powers the WebSocket simulation, and the website uses HTML/CSS/vanilla JavaScript. No build pipeline is required.

## Start on CachyOS / Arch Linux

```bash
cd /home/ludvig/Programming/Projects/Lan-website
python -m venv .venv
source .venv/bin/activate  # fish shell: source .venv/bin/activate.fish
pip install -r apps/server/requirements.txt
python apps/server/server.py
```

Open http://localhost:8080. Others on the same LAN can open `http://YOUR_LOCAL_IP:8080`. Allow TCP port 8080 through your firewall if needed. To locate your IP: `hostname -I`.

## Test multiplayer

1. Open the site in two tabs or devices.
2. Enter distinct nicknames.
3. Host a Warlock room in the first tab; copy its room code.
4. Join that room from the second tab via **Join with code**.
5. Host clicks **Start match**. Right-click arena to move. Hover arena and press Q (bolt), W (blast), E (shield).

## Status and limitations

- Supports game library, join codes, live lobby and server-authoritative 30Hz game simulation.
- Only Warlock prototype is playable; future game card is intentionally unavailable.
- Gameplay spells, arena appearance, physics and balance are temporary tests, **not** verified Warlock 1.02 behavior.
- Rooms are in memory and disappear on server restart. No authentication, persistence or public matchmaking. LAN/prototype use only.
- Original Warcraft III map stays under `original-maps/` as a local research reference; no original artwork is served to clients. Confirm asset/distribution rights before publishing.
- Python server is presently monolithic. Extract `games/warlock/server` simulation and define a server plugin API before integrating a second game.
