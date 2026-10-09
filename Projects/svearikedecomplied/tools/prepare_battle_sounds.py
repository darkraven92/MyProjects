"""Export KRIG's script-selected PCM through the original resource decoder."""
from pathlib import Path
import hashlib
import json
from export_assets import load_library
from export_sounds import export_sounds


def main():
    source = Path('SveaRike/KRIG.DIR')
    scene = json.loads(Path('analysis/assets/KRIG/scene.json').read_text())
    assert scene['sha256'] == hashlib.sha256(source.read_bytes()).hexdigest()
    members = {m['number']: m for m in scene['members']}
    by_name = {m['name'].lower(): m['number'] for m in scene['members'] if m['type'] == 6}
    names = [('FOREST', 'skog.aif'), ('FOREST2', 'skog2'), ('WINTER', 'vinter'),
             ('WALK', 'walk'), ('HORSE_WALK', 'hwalk'), ('HALBERD', 'hileb'),
             ('GUNSHOT', 'pang'), ('CROSSBOW', 'crosshit'), ('SABRE', 'huggsab'),
             ('HORSE_KICK', 'hästspark'), ('CANNON12', 'art1_2'), ('CANNON3', 'kanon3')]
    numbers = [by_name[name] for _, name in names]
    export_sounds(load_library(Path('build/libsvea_assets.so')), source, members, numbers, 'battle')
    lines = ['/* Generated KRIG sound cast IDs; script playback uses channels 1/2. */', 'enum {']
    lines += [f'    SR_KRIG_{symbol}={by_name[name]},' for symbol, name in names]
    lines += ['};']
    Path('src/generated/battle_sounds.h').write_text('\n'.join(lines)+'\n')
    scripts = Path('analysis/decompiled/KRIG/casts/Internal')
    sources = ['ScoreScript 6.ls', 'ScoreScript 47.ls', 'ScoreScript 45.ls',
               'MovieScript 66 - Troop.ls', 'MovieScript 67 - ArtTroop.ls',
               'MovieScript 74 - Cavalery.ls', 'MovieScript 78 - EnemyCav.ls',
               'MovieScript 76 - Infantry.ls', 'MovieScript 73 - EnemyInfantry.ls',
               'MovieScript 75 - Artilery.ls', 'MovieScript 77 - EnemyArtilery.ls']
    Path('analysis/battle-sound-provenance.json').write_text(json.dumps({
        'source_sha256': scene['sha256'], 'members': numbers,
        'scripts': [{'file': str(scripts / name), 'sha256': hashlib.sha256((scripts / name).read_bytes()).hexdigest()}
                    for name in sources],
        'note': 'Script-selected sounds only. Unused death and bird sounds are not added.'
    }, indent=2)+'\n')
    print(f'Exported {len(numbers)} original battle PCM sounds.')


if __name__ == '__main__':
    main()
