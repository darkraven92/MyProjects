# Static offset verification: WoW 1.12.1.5875

Target SHA-256:

`b4756d38ef207c02ed651f4952bd89a70b4857b73a33413339e1b285b28d2dc7`

PE preferred image base: `0x00400000`.

## Evidence from the client

| Client VA | Relevant operation | Conclusion |
| --- | --- | --- |
| `0x00467D00` | `mov eax, [0x00B41414]` | Direct global Object Manager pointer |
| `0x004650B3` | Initializes list at manager `+0xA4` with link offset `0x38` | Visible list uses object link at `+0x38` |
| `0x00467DA6` | Reads first link from manager `+0xAC` | First visible object |
| `0x00467DC9` | Adds current object to link offset and reads `+4` | Next object is `object + 0x38 + 4 = +0x3C` |
| `0x00468550` | Reads two dwords from manager `+0xC0/+0xC4` | Local player GUID |
| `0x00613889` | Stores sequential object type at object `+0x14` | Object type |
| `0x004683A9` | Reads two dwords from object `+0x30/+0x34` | Object GUID |
| `0x005FAD7C` | Constructs the Unit movement component at object `+0x9A8` | Movement structure |
| `0x007C486A` | Copies three floats to movement `+0x10/+0x14/+0x18` | X/Y/Z at object `+0x9B8/+0x9BC/+0x9C0` |
| `0x007C4880` | Stores a float at movement `+0x1C` | Facing at object `+0x9C4` |

The client tests bit 0 on list pointers before dereferencing them. That bit is
the tagged end-sentinel and must not be treated as an object address.

## Dynamic verification

The local player object, GUID, position, and facing were verified against a
running Wine process on CachyOS. Manual movement changed the read-back X/Y/Z
coordinates and manual turning changed facing. A bounded 500 ms W input through
`ydotool` then moved the player 3.703 game units, confirming the complete
read-input-read loop without writing game memory.

Closed-loop steering was subsequently verified for relative bearings of +30
and -60 degrees. Both runs arrived inside the configured 0.75-unit radius with
no recovery attempts, confirming positive and negative keyboard turning.

The first multi-waypoint run exposed fixed-pulse overshoot near a target: a
250 ms pulse moved about 1.9 units while only 0.973 remained, causing repeated
crossing and 180-degree turns. The navigation controller now shortens forward
pulses based on remaining distance, refines its speed estimate from successful
pulses, and detects when a measured movement segment crosses the arrival circle.

The corrected controller completed a recorded route containing 53 waypoints.
Every waypoint arrived without recovery, typically 0.23-0.45 units from its
target, while the learned movement speed remained approximately 7.3-7.5 units
per second. One sequence showed that 60 ms turn pulses can be missed under
Wine; verified turn pulses now use an 85 ms minimum and bounded escalation.

## Configured values

| Name | Hex | JSON decimal |
| --- | ---: | ---: |
| Object Manager pointer RVA | `0x741414` | 7607316 |
| First visible object | `0xAC` | 172 |
| Local GUID | `0xC0` | 192 |
| Next visible object | `0x3C` | 60 |
| Object type | `0x14` | 20 |
| Object GUID | `0x30` | 48 |
| Position X | `0x9B8` | 2488 |
| Position Y | `0x9BC` | 2492 |
| Position Z | `0x9C0` | 2496 |
| Facing | `0x9C4` | 2500 |

Static verification establishes the code structure. The `--probe` run provides
the required dynamic verification against a logged-in client.
