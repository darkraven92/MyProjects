#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/wasm
# A first real game routine; no libc or emsdk required for this pure C module.
clang --target=wasm32 -std=c11 -O2 -nostdlib \
  -Wl,--no-entry -Wl,--export=sr_trade_price_step \
  src/trade.c -o build/wasm/trade.wasm
python3 tools/export_assets.py
python3 tools/prepare_strategy.py
python3 tools/prepare_fields.py
python3 tools/prepare_crossbow.py
python3 tools/prepare_linne.py
python3 tools/prepare_battle_rules.py
python3 tools/prepare_battle.py
python3 tools/prepare_battle_animation.py
python3 tools/prepare_battle_sounds.py
python3 tools/prepare_events.py
python3 tools/pack_menu.py
clang --target=wasm32 -std=c11 -O2 -nostdlib -ffreestanding -fno-builtin \
  -Wl,--no-entry -Wl,--export-memory -Wl,--initial-memory=150994944 \
  -Wl,--export=sr_menu_input -Wl,--export=sr_menu_capacity \
  -Wl,--export=sr_menu_load -Wl,--export=sr_menu_render \
  -Wl,--export=sr_menu_down -Wl,--export=sr_menu_up -Wl,--export=sr_menu_cancel \
  -Wl,--export=sr_menu_screen -Wl,--export=sr_menu_family -Wl,--export=sr_menu_action \
  -Wl,--export=sr_menu_result -Wl,--export=sr_menu_error \
  -Wl,--export=sr_menu_move -Wl,--export=sr_menu_hover_area \
  -Wl,--export=sr_menu_crossbow_preview -Wl,--export=sr_menu_tick -Wl,--export=sr_menu_crossbow_value \
  -Wl,--export=sr_menu_sound \
  -Wl,--export=sr_menu_sound2 -Wl,--export=sr_menu_linne_preview -Wl,--export=sr_menu_linne_value \
  -Wl,--export=sr_menu_dismiss_value \
  -Wl,--export=sr_menu_commander_value -Wl,--export=sr_menu_commander_name \
  -Wl,--export=sr_menu_person_value -Wl,--export=sr_menu_person_name \
  -Wl,--export=sr_menu_field_text -Wl,--export=sr_menu_field_font -Wl,--export=sr_menu_field_metric \
  -Wl,--export=sr_menu_diplomacy_value -Wl,--export=sr_menu_country_name \
  -Wl,--export=sr_menu_trade_value \
  -Wl,--export=sr_menu_war_value -Wl,--export=sr_menu_war_area_name \
  -Wl,--export=sr_menu_battle_input -Wl,--export=sr_menu_battle_capacity \
  -Wl,--export=sr_menu_battle_request -Wl,--export=sr_menu_battle_load \
  -Wl,--export=sr_menu_battle_value -Wl,--export=sr_menu_battle_unit \
  -Wl,--export=sr_menu_event_input -Wl,--export=sr_menu_event_capacity \
  -Wl,--export=sr_menu_event_request -Wl,--export=sr_menu_event_load \
  -Wl,--export=sr_menu_turn_value -Wl,--export=sr_menu_turn_name -Wl,--export=sr_menu_turn_minigame \
  -Wl,--export=sr_game_value -Wl,--export=sr_game_area_name \
  -Wl,--export=sr_game_seed -Wl,--export=sr_game_head_name \
  -Wl,--export=sr_catalog_count -Wl,--export=sr_catalog_period_record \
  -Wl,--export=sr_catalog_dated_count \
  src/menu.c src/game.c src/catalog.c src/random.c src/economy.c src/trade.c src/events.c src/family.c src/turn.c src/crossbow.c src/linne.c src/dismiss.c src/people.c src/fields.c src/diplomacy.c src/trade_orders.c src/war.c src/battle_rules.c src/battle_setup.c src/battle_ai.c src/battle_scene.c src/battle_attack.c src/battle_artillery.c src/wasm_memory.c -o build/wasm/menu.wasm
