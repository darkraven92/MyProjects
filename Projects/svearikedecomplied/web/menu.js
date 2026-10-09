import { createSoundPlayer } from './audio.js';
const canvas = document.querySelector('#game');
const status = document.querySelector('#status');
const context = canvas.getContext('2d', { alpha: false });
const preview = 'Preview: manage your province, then click the hourglass at bottom right to end the turn. Events, crossbow, Linné and quick battles work. Manual battles support movement, attacks, artillery, retreat and campaign results; the remaining minigames and original dynamic text are pending.';
try {
  const [wasm, pack] = await Promise.all([
    fetch('../build/wasm/menu.wasm'), fetch('../build/wasm/menu.pack')
  ]);
  if (!wasm.ok || !pack.ok) throw new Error('Build the project and run tools/build_wasm.sh first.');
  const { instance } = await WebAssembly.instantiate(await wasm.arrayBuffer());
  const core = instance.exports;
  // The Windows projector seeds from its clock/uptime. The browser supplies
  // its own seed; ?seed=<uint32> provides a reproducible debugging session.
  const seedParameter = new URLSearchParams(location.search).get('seed');
  const seed = seedParameter !== null && /^\d+$/.test(seedParameter) && Number(seedParameter)<=0xffffffff
    ? Number(seedParameter) : crypto.getRandomValues(new Uint32Array(1))[0];
  core.sr_game_seed(seed);
  const textAt = (address,encoding='utf-8') => {
    const bytes = new Uint8Array(core.memory.buffer);
    let end = address;
    while (bytes[end]) ++end;
    return new TextDecoder(encoding).decode(bytes.subarray(address,end));
  };
  const fieldText = member => textAt(core.sr_menu_field_text(member),
    core.sr_menu_field_metric(member,10)===1?'windows-1252':'utf-8');
  document.documentElement.dataset.seed = seed;
  document.documentElement.dataset.catalogCounts = [0,1,2,3].map(kind => core.sr_catalog_count(kind)).join(',');
  document.documentElement.dataset.sciencePeriod6 = core.sr_catalog_period_record(1,6,1);
  const data = new Uint8Array(await pack.arrayBuffer());
  if (data.length > core.sr_menu_capacity()) throw new Error('Artwork exceeds the C asset buffer.');
  new Uint8Array(core.memory.buffer, core.sr_menu_input(), data.length).set(data);
  if (!core.sr_menu_load(data.length)) throw new Error('C runtime rejected the artwork pack.');
  const minigame = new URLSearchParams(location.search).get('minigame');
  const crossbowPreview = minigame === 'armborst';
  if (crossbowPreview && !core.sr_menu_crossbow_preview(seed)) throw new Error('Crossbow artwork is unavailable.');
  const linnePreview = minigame === 'linne';
  if (linnePreview && !core.sr_menu_linne_preview()) throw new Error('Linne artwork is unavailable.');
  let audioContext;
  const sounds=new Map();
  {
    audioContext=new AudioContext({sampleRate:22050});
    await Promise.all([['crossbow',0],['linne',1000],['battle',2000]].map(async ([name,offset])=>{
      const response=await fetch(`../build/wasm/sounds/${name}.json`);
      if(!response.ok) throw new Error(name+' sound manifest unavailable.');
      const entries=await response.json();
      await Promise.all(entries.map(async entry=>{
        const response=await fetch('../build/wasm/sounds/'+entry.file);
        if(!response.ok) throw new Error('Original sound unavailable: '+entry.name);
        const buffer=await audioContext.decodeAudioData(await response.arrayBuffer());
        if(buffer.length!==entry.frames || buffer.sampleRate!==entry.rate || buffer.numberOfChannels!==entry.channels)
          throw new Error('Original sound format mismatch: '+entry.name);
        sounds.set(offset+entry.member,{buffer,entry});
      }));
      canvas.dataset[name+'Samples']=String(entries.length);
    }));
    canvas.dataset.audioSamples=String(sounds.size);
    canvas.dataset.soundEvents='';
    canvas.dataset.sound2Events='';
    window.addEventListener('pagehide',()=>audioContext.close());
  }
  const soundPlayer=createSoundPlayer(audioContext,sounds,(channel,sound,record)=>{
    const key=channel?'sound2Events':'soundEvents';
    canvas.dataset[key]+=(canvas.dataset[key]?',':'')+sound;
    canvas.dataset['sound'+(channel+1)+'Loop']=String(!!record?.source.loop);
    canvas.dataset['sound'+(channel+1)+'FadeSeconds']=String(sound===-3?15/60:0);
  });
  const playSound=(channel,sound)=>soundPlayer.play(channel,sound);
  let eventLoading=0, eventError='';
  const loadEvent = async member => {
    eventLoading=member; eventError='';
    try {
      const response=await fetch(`../build/wasm/events/${member}.rgba`);
      if(!response.ok) throw new Error('The original event image could not be loaded.');
      const bytes=new Uint8Array(await response.arrayBuffer());
      if(core.sr_menu_event_request()!==member) return;
      if(bytes.length>core.sr_menu_event_capacity()) throw new Error('Event image exceeds the C buffer.');
      new Uint8Array(core.memory.buffer,core.sr_menu_event_input(),bytes.length).set(bytes);
      if(!core.sr_menu_event_load(member,bytes.length)) throw new Error('C rejected the event image.');
      render();
    } catch(error) { eventError=error.message; status.textContent=eventError; console.error(error); }
  };
  const battleLoading=[0,0,0];let battleError='';
  const loadBattle = async (bank,id) => {
    battleLoading[bank]=id;
    try {
      const response=await fetch(`../build/wasm/battle/${id}.pack`);
      if(!response.ok) throw new Error('Original battle artwork could not be loaded.');
      const bytes=new Uint8Array(await response.arrayBuffer());
      if(core.sr_menu_battle_request(bank)!==id) return;
      if(bytes.length>core.sr_menu_battle_capacity()) throw new Error('Battle artwork exceeds the C buffer.');
      new Uint8Array(core.memory.buffer,core.sr_menu_battle_input(bank),bytes.length).set(bytes);
      if(!core.sr_menu_battle_load(bank,id,bytes.length)) throw new Error('C rejected the battle artwork.');
      render();
    } catch(error) {battleError=error.message;status.textContent=battleError;console.error(error);}
  };
  const render = () => {
    const address = core.sr_menu_render();
    context.putImageData(new ImageData(new Uint8ClampedArray(core.memory.buffer, address, 640 * 480 * 4), 640, 480), 0, 0);
    canvas.dataset.screen = core.sr_menu_screen();
    canvas.dataset.family = core.sr_menu_family();
    canvas.dataset.area = core.sr_game_value(2);
    canvas.dataset.year = core.sr_game_value(0);
    canvas.dataset.turn = core.sr_game_value(1);
    canvas.dataset.silver = core.sr_game_value(3);
    canvas.dataset.crops = core.sr_game_value(4);
    canvas.dataset.metal = core.sr_game_value(5);
    canvas.dataset.farmingLevel = core.sr_game_value(6);
    canvas.dataset.farmingIndex = core.sr_game_value(7);
    canvas.dataset.miningLevel = core.sr_game_value(31);
    canvas.dataset.scienceLevel = core.sr_game_value(33);
    canvas.dataset.hqLevel = core.sr_game_value(12);
    canvas.dataset.farmBanner = core.sr_game_value(34);
    canvas.dataset.miningBanner = core.sr_game_value(36);
    canvas.dataset.tax = core.sr_game_value(14);
    canvas.dataset.unrest = core.sr_game_value(15);
    canvas.dataset.incomeBanner = core.sr_game_value(16);
    canvas.dataset.unrestBanner = core.sr_game_value(17);
    canvas.dataset.militaryLevel = core.sr_game_value(8);
    canvas.dataset.infantry = core.sr_game_value(9);
    canvas.dataset.cavalry = core.sr_game_value(10);
    canvas.dataset.artillery = core.sr_game_value(11);
    canvas.dataset.armyError = core.sr_menu_error();
    canvas.dataset.armyResult = core.sr_menu_result();
    canvas.dataset.randomCalls = core.sr_game_value(28);
    canvas.dataset.points = core.sr_game_value(29);
    canvas.dataset.familyHead = textAt(core.sr_game_head_name());
    canvas.dataset.headType = core.sr_game_value(30);
    // Text content is now produced by C from original field assignments.
    // Keep it inspectable while original glyph rasterization is unresolved.
    const fieldIds=[130,131,132,133,140,141,142,143,144,145,146,147,148,149,
      180,181,183,184,185,207,208,222,223,224,225,226,227,228,229,332,333,334,
      ...Array.from({length:8},(_,i)=>304+i),...Array.from({length:12},(_,i)=>319+i),
      159,161,163,165,...Array.from({length:8},(_,i)=>170+i),...Array.from({length:8},(_,i)=>255+i)];
    canvas.dataset.fieldText=JSON.stringify(Object.fromEntries(fieldIds
      .filter(member=>core.sr_menu_field_metric(member,0)>=0).map(member=>[member,fieldText(member)])));
    canvas.dataset.yearFont=textAt(core.sr_menu_field_font(130));
    canvas.dataset.yearGeometry=[0,1,2,3,5,6,7,8].map(field=>core.sr_menu_field_metric(130,field)).join(',');
    canvas.dataset.mapFont=textAt(core.sr_menu_field_font(207));
    canvas.dataset.eventYears = [40,41,42,43,44,45].map(field=>core.sr_game_value(field)).join(',');
    const turnFields=['request','kind','record','country','area','score','error','active','image','crossbow','linne'];
    turnFields.forEach((name,i)=>canvas.dataset['turn'+name[0].toUpperCase()+name.slice(1)]=core.sr_menu_turn_value(i));
    const requested=core.sr_menu_event_request();
    canvas.dataset.eventRequested=requested;
    if(requested && requested!==core.sr_menu_turn_value(8) && requested!==eventLoading) loadEvent(requested);
    if(!requested) { eventLoading=0; eventError=''; }
    if(core.sr_menu_screen()!==10) canvas.style.cursor='default';
    if(core.sr_menu_screen()===11 || core.sr_menu_action()===8) {
      const type=core.sr_menu_turn_value(0), score=core.sr_menu_turn_value(5);
      const kind=core.sr_menu_turn_value(1);
      const name=textAt(core.sr_menu_turn_name(),kind>=0 && kind<=2?'windows-1252':'macintosh');
      const area=textAt(core.sr_game_area_name(core.sr_menu_turn_value(4)));
      const messages={
        1:`${core.sr_game_value(0)} — the years are passing…`,
        2:requested!==core.sr_menu_turn_value(8)?'Loading original event…':`${core.sr_game_value(0)}: ${name}. Click the original OK button to continue.`,
        3:`This event requires ${textAt(core.sr_menu_turn_minigame())}. That minigame is not ported yet; the turn is waiting.`,
        4:score?`You earned ${100*score} silver and ${Math.trunc(score/2)} prestige points. Click OK to continue the turn.`:'No reward this time. Click OK to continue the turn.',
        5:`War with ${textAt(core.sr_menu_country_name(core.sr_menu_turn_value(3)))}. Waiting for the war controls.`,
        6:`Rebellion in ${area}. Choose suppression, negotiation or concession using the original buttons.`,
        7:`Unrest in ${area} reduces this turn’s production. Click OK to continue.`,
        8:`The crown has taken ${area} because of unrest. Click OK to continue.`,
        9:'Trade was lost during the war. Click OK to continue.',
        10:'The army is short of food. Dismiss troops to reduce their upkeep.',
        11:`${core.sr_game_value(0)} — turn ${core.sr_game_value(1)}. Silver ${core.sr_game_value(3)}, crops ${core.sr_game_value(4)}, metal ${core.sr_game_value(5)}.`,
        12:'The campaign has ended. The original ending screen still needs porting.',
        13:'Turn processing stopped at an unresolved game state.'
      };
      const error=core.sr_menu_turn_value(6);
      status.textContent=eventError || (error===1?'There are no troops in this province. Click OK.':
        error===2?'Not enough silver to pay the extra troop upkeep. Click OK.':messages[type] || preview);
    }
    if(core.sr_menu_screen()===12) {
      const fields=['area','infantry','cavalry','artillery','homeInfantry','homeCavalry','homeArtillery','warning','inTurn','areas','minimum'];
      fields.forEach((name,i)=>canvas.dataset['dismiss'+name[0].toUpperCase()+name.slice(1)]=core.sr_menu_dismiss_value(i));
      const area=textAt(core.sr_game_area_name(core.sr_menu_dismiss_value(0)));
      status.textContent=core.sr_menu_dismiss_value(7)?
        `Your army is starving. Dismiss at least ${core.sr_menu_dismiss_value(10)} troops. Click OK to select troops.`:
        `${area}: remaining ${core.sr_menu_dismiss_value(1)} infantry, ${core.sr_menu_dismiss_value(2)} cavalry, ${core.sr_menu_dismiss_value(3)} artillery. `+
        `Selected for dismissal: ${core.sr_menu_dismiss_value(4)}, ${core.sr_menu_dismiss_value(5)}, ${core.sr_menu_dismiss_value(6)}. `+
        'Right arrows select 1000; left arrows undo. OK commits. Original text rendering is pending.';
    }
    if(core.sr_menu_screen()===13) {
      const fields=['selected','level','price','available','owned','result'];
      fields.forEach((name,i)=>canvas.dataset['commander'+name[0].toUpperCase()+name.slice(1)]=core.sr_menu_commander_value(i));
      const names=owned=>Array.from({length:core.sr_menu_commander_value(owned?4:3)},
        (_,i)=>textAt(core.sr_menu_commander_name(i+1,owned),'windows-1252'));
      const selected=textAt(core.sr_menu_commander_name(0,0),'windows-1252');
      canvas.dataset.commanderName=selected;
      const error=core.sr_menu_commander_value(5);
      status.textContent=core.sr_menu_error()?
        (error===1?'Select a commander from the right-hand list first.':
         error===3?'Not enough silver to hire this commander.':'This commander is unavailable.')+' Click OK.':
        `Employed: ${names(1).join(', ')||'none'}. Available: ${names(0).join(', ')||'none'}. `+
        (selected?`${selected}. ${fieldText(145)}. ${fieldText(146)} silver. Click ANSTÄLL to hire.`:
          'Select a row in the right-hand list, then click ANSTÄLL. Original list text rendering is pending.');
    }
    if(core.sr_menu_screen()===14) {
      status.textContent=core.sr_menu_action()===5?'This estate control is not implemented yet.':
        `Herresäte level ${core.sr_game_value(12)}. The right-hand icon opens culture and science hiring. OK returns to the province.`;
    }
    if(core.sr_menu_screen()===15) {
      const lists=[];
      for(let kind=0;kind<2;++kind) {
        const prefix=kind?'science':'culture',fields=['Selected','Rank','Price','Available','Owned','Result'];
        fields.forEach((field,i)=>canvas.dataset[prefix+field]=core.sr_menu_person_value(kind,i));
        canvas.dataset[prefix+'Name']=textAt(core.sr_menu_person_name(kind,0,0),'windows-1252');
        const names=owned=>Array.from({length:core.sr_menu_person_value(kind,owned?4:3)},
          (_,i)=>textAt(core.sr_menu_person_name(kind,i+1,owned),'windows-1252')).join(', ')||'none';
        lists.push(`${kind?'Science':'Culture'} — employed: ${names(1)}; available: ${names(0)}.`);
      }
      const kind=core.sr_menu_person_value(0,0)?0:1;
      const selected=textAt(core.sr_menu_person_name(kind,0,0),'windows-1252');
      const errors={1:'Select a person from an available list first.',3:'Not enough silver.',
        4:'A rank-five culture person requires an owned triumphal arch.',
        5:'A rank-five scientist requires an owned university.'};
      status.textContent=core.sr_menu_error()?
        (errors[core.sr_menu_person_value(kind,5)]||'This person is unavailable.')+' Click OK.':
        lists.join(' ')+' '+(selected?`${selected}. ${fieldText(kind?184:145)}. ${fieldText(kind?185:146)} silver. Click ANSTÄLL to hire.`:
          'Select a row under LEDIGA, then click ANSTÄLL. Original list text rendering is pending.');
    }
    if (core.sr_menu_screen() === 10) {
      const fields=['phase','round','bolts','score','wind','result','startX','startY','bowX','bowY','hitX','hitY','randomCalls','dragging'];
      fields.forEach((name,i) => canvas.dataset['crossbow'+name[0].toUpperCase()+name.slice(1)]=core.sr_menu_crossbow_value(i));
      canvas.style.cursor = core.sr_menu_crossbow_value(13) ? 'none' : 'default';
      const phase=core.sr_menu_crossbow_value(0);
      status.textContent = phase===0 ? 'Crossbow contest preview: click SPELA. Hold the left mouse button and drag to aim; release to shoot. Original score-field rendering is pending.' :
        phase===10 ? `Contest finished: ${core.sr_menu_crossbow_value(3)}/75 points; original result tier ${core.sr_menu_crossbow_value(5)}/10. Standalone preview complete.` :
        `Round ${core.sr_menu_crossbow_value(1)}/3 · Bolts ${core.sr_menu_crossbow_value(2)}/5 · Score ${core.sr_menu_crossbow_value(3)}/75. Hold and drag to aim; release to shoot.`;
    }
    if(core.sr_menu_screen()===16) {
      const fields=['phase','flower','name','score','attempts','matched','success','result','remaining'];
      fields.forEach((name,i)=>canvas.dataset['linne'+name[0].toUpperCase()+name.slice(1)]=core.sr_menu_linne_value(i));
      const phase=core.sr_menu_linne_value(0);
      status.textContent=phase===0?'Linné: click SPELA. Study the six plants, then match each flower with its name.':
        phase===1?'Study the plants and names for five seconds.':
        phase===2?`${core.sr_menu_linne_value(3)}/6 pairs · ${core.sr_menu_linne_value(4)} attempts · ${Math.ceil(core.sr_menu_linne_value(8)/60)} seconds. Click a flower and its name.`:
        phase===3?`${core.sr_menu_linne_value(6)?'Completed':'Time or attempt limit exceeded'}: ${core.sr_menu_linne_value(3)}/6 pairs in ${core.sr_menu_linne_value(4)} attempts. Click OK.`:
        `Linné finished: original result tier ${core.sr_menu_linne_value(7)}/10. Standalone preview complete.`;
    }
    if(core.sr_menu_screen()===19) {
      const orders=Array.from({length:4},(_,i)=>({id:i+1,name:textAt(core.sr_menu_country_name(i+1)),
        level:core.sr_menu_trade_value(i+1,0),capacity:core.sr_menu_trade_value(i+1,1),
        crops:core.sr_menu_trade_value(i+1,2),metal:core.sr_menu_trade_value(i+1,3),
        cropX:core.sr_menu_trade_value(i+1,5),metalX:core.sr_menu_trade_value(i+1,6),
        cropPrice:fieldText(170+i),metalPrice:fieldText(174+i)}));
      canvas.dataset.trade=JSON.stringify(orders);
      canvas.dataset.tradeResult=core.sr_menu_trade_value(1,4);
      canvas.dataset.tradeDragging=core.sr_menu_trade_value(1,7);
      const errors={3:'A trading company requires a harbor in an owned province.',
        4:'The upgrade requires more silver, metal or a higher estate level.'};
      status.textContent=core.sr_menu_error()?(errors[core.sr_menu_trade_value(1,4)]||'Trade upgrade unavailable.')+' Click OK.':
        `Silver ${fieldText(131)} · Crops ${fieldText(132)} · Metal ${fieldText(133)}. `+
        orders.map(c=>`${c.name}: crops ${c.crops}, metal ${c.metal}, capacity ${c.capacity}; prices ${c.cropPrice}/${c.metalPrice}.`).join(' ')+
        ' Left buys, right sells. Crops and metal share capacity. Orders reserve resources now and settle at turn end. Drag a marker or hold an arrow.';
    }
    if(core.sr_menu_screen()===17 || core.sr_menu_screen()===18) {
      const countries=Array.from({length:4},(_,i)=>({id:i+1,name:textAt(core.sr_menu_country_name(i+1)),
        level:core.sr_menu_diplomacy_value(i+1,0),relation:core.sr_menu_diplomacy_value(i+1,1),
        queued:core.sr_menu_diplomacy_value(i+1,2),icon:core.sr_menu_diplomacy_value(i+1,3),
        troops:[6,7,8].map(field=>core.sr_menu_diplomacy_value(i+1,field))}));
      canvas.dataset.diplomacy=JSON.stringify(countries);
      canvas.dataset.diplomacyResult=core.sr_menu_diplomacy_value(1,4);
      const errors={3:'The final diplomacy level requires an embassy in an owned province.',
        4:'The diplomacy upgrade requires more silver or a higher estate level (2 for consul, 3 for ambassador).',
        5:'Not enough silver to reserve another 100.'};
      status.textContent=core.sr_menu_error()?(errors[core.sr_menu_diplomacy_value(1,4)]||'Diplomacy action unavailable.')+' Click OK.':
        core.sr_menu_screen()===17?countries.map(c=>`${c.name}: ${c.troops.join('/')} infantry/cavalry/artillery.`).join(' ')+
          ' HANDEL opens buy/sell orders; DIPLOMATI opens upgrades and relation spending.':
        countries.map(c=>`${c.name}: diplomacy ${c.level}, relation ${c.relation}, reserved ${c.queued}.`).join(' ')+
          ` Silver ${core.sr_game_value(3)}. FÖRBÄTTRA/FÖRKLARA KRIG reserve 100 per click; reversing refunds it. Relations change at end of turn. Quick and manual battle results return to the campaign.`;
    }
    if(core.sr_menu_screen()===20) {
      const warFields=['phase','country','attacking','area','regiment','won','enemyLevel','error',
        'offerType','offerArea','offerSilver','fee','applied','territory','targets','regiments',
        'infantry','cavalry','artillery','enemyInfantry','enemyCavalry','enemyArtillery',
        'lostInfantry','lostCavalry','lostArtillery','enemyLostInfantry','enemyLostCavalry','enemyLostArtillery','wins','retreated'];
      const war=Object.fromEntries(warFields.map((name,i)=>[name,core.sr_menu_war_value(i)]));
      canvas.dataset.war=JSON.stringify(war);
      const country=textAt(core.sr_menu_country_name(war.country));
      const target=textAt(core.sr_menu_war_area_name(0,0));
      const regiment=textAt(core.sr_menu_war_area_name(0,1));
      const names=owned=>Array.from({length:owned?war.regiments:war.targets},(_,i)=>textAt(core.sr_menu_war_area_name(i+1,owned))).join(', ');
      status.textContent=war.error?(war.error===2?'Negotiation requires 100 silver or an owned embassy.':'The selected regiment needs infantry or cavalry.')+' Click OK.':
        war.phase===0?`${war.attacking?'Sweden declares war on':'War declared by'} ${country}. Choose FÖRHANDLA, SNABBSTRID or STRID. STRID opens the battlefield.`:
        war.phase===1?`${country} demands ${war.offerType===1?textAt(core.sr_game_area_name(war.offerArea)):war.offerSilver+' silver'}. Accept or refuse using the original buttons.`:
        war.phase===2?`${country} rejected negotiations. Choose SNABBSTRID or STRID.`:
        war.phase===3?`No regiment has infantry or cavalry. Click OK to apply the original surrender outcome against ${country}.`:
        war.phase===4?`Choose a province: ${names(0)||'the enemy has no provinces left'}. Selected: ${target||'silver compensation'}. Click OK.`:
        war.phase===5?`Choose a regiment: ${names(1)}. Selected: ${regiment}. Click OK. Original list text rendering is pending.`:
        war.phase===6?`Swedish troops: ${war.infantry}/${war.cavalry}/${war.artillery}. ${country}: ${war.enemyInfantry}/${war.enemyCavalry}/${war.enemyArtillery} at level ${war.enemyLevel}. Click OK to show the battle result.`:
        war.phase===7?`${war.retreated?'Retreat':war.won?'Victory':'Defeat'}. ${war.territory?`${war.territory>0?'Gained':'Lost'} ${textAt(core.sr_game_area_name(Math.abs(war.territory)))}.`:`Silver change: ${war.fee}.`} Losses: ${war.lostInfantry}/${war.lostCavalry}/${war.lostArtillery}. Click OK to resume the turn.`:
        'Manual battle has been selected. The campaign waits for its battlefield result.';
    }
    if(core.sr_menu_screen()===21) {
      const fields=['phase','loaded','swedes','placed','enemies','drag','background','profile','row','selected','hover','walking','used','pending','maxActions','attacker','target','enemyTurn','winner','round','enemyArtillery','gun','gunSide','enemyReload','swedishArtillery','officerX','officerY','swedishReload','sight','swedishGun'];
      const battle=Object.fromEntries(fields.map((key,i)=>[key,core.sr_menu_battle_value(i)]));
      battle.units=Array.from({length:battle.swedes+battle.enemies},(_,n)=>
        Object.fromEntries(['type','troops','square','placed','x','y','actions'].map((key,i)=>[key,core.sr_menu_battle_unit(n,i)])));
      canvas.dataset.battle=JSON.stringify(battle);
      for(let bank=0;bank<3;++bank) {
        const id=core.sr_menu_battle_request(bank);
        if(id && battleLoading[bank]!==id) loadBattle(bank,id);
      }
      status.textContent=battleError || (!battle.loaded?'Loading original battlefield and troops…':
        battle.phase===1?`Drag your troops onto the marked starting squares. Placed ${battle.placed}/${battle.swedes}.`:
        battle.phase===3?(battle.enemyTurn?'Enemy troops are moving…':'Troops are moving…'):
        battle.phase===7?`${battle.winner===1?'Sweden':'The enemy'} wins the battle. Returning to the campaign…`:
        battle.phase===4?'Enemy turn…':
        battle.phase===5?`Choose an enemy group for cannon ${battle.swedishGun+1}. Click its target marker to fire.`:
        battle.phase===6?'Attack and counterattack in progress…':
        battle.phase===8?(battle.gunSide?'Enemy artillery is firing…':'Swedish artillery is firing…'):
        battle.phase===9?'The artillery officer is moving…':
        `Select one of your groups, then click an adjacent square to move or attack. Actions used: ${battle.used}/${battle.maxActions}. Use the bottom-left retreat control to withdraw.`);
    }
    playSound(0,core.sr_menu_sound());playSound(1,core.sr_menu_sound2());
  };
  const point = event => {
    const rect = canvas.getBoundingClientRect();
    return [Math.floor((event.clientX - rect.left) * 640 / rect.width),
            Math.floor((event.clientY - rect.top) * 480 / rect.height)];
  };
  let pointer = null;
  canvas.addEventListener('pointerdown', event => {
    if (event.button !== 0 || pointer !== null) return;
    if(audioContext?.state==='suspended') audioContext.resume().catch(console.error);
    pointer = event.pointerId;
    canvas.setPointerCapture(pointer);
    core.sr_menu_down(...point(event)); render();
  });
  canvas.addEventListener('pointerup', event => {
    if (event.pointerId !== pointer) return;
    core.sr_menu_up(...point(event));
    canvas.releasePointerCapture(pointer); pointer = null;
    render();
    if (core.sr_menu_screen()>=10 || core.sr_menu_action()===8 || core.sr_menu_action()===9) return;
    const messages = [preview, 'Save-file loading is not implemented yet.',
      'Intro playback is not implemented yet.',
      `${textAt(core.sr_game_head_name())}: new-game state initialized. Original dynamic text is still pending.`,
      'Quit requested. You can close this browser tab.',
      'This strategy control is not implemented yet.'];
    status.textContent = messages[core.sr_menu_action()];
    if (core.sr_menu_action() === 6) {
      const results = [
        `Regiment level ${core.sr_game_value(8)}: ${core.sr_game_value(9)} infantry, ${core.sr_game_value(10)} cavalry, ${core.sr_game_value(11)} artillery. Silver: ${core.sr_game_value(3)}; metal: ${core.sr_game_value(5)}.`,
        'Invalid military action.', 'Not enough silver.', 'Regiment capacity reached; upgrade it first.',
        'Cavalry requires a barracks (regiment level 2).', 'The artillery limit has been reached.',
        'Artillery requires a smithy in an owned province.', 'The regiment is already at its highest level.',
        'The next regiment level requires more resources or a higher mining level.',
        'The final regiment upgrade requires a castle or headquarters in this province.'
      ];
      status.textContent = results[core.sr_menu_result()] + (core.sr_menu_error() ? ' Dismiss the message with its OK button. Original message text rendering is pending.' : '');
    }
    if (core.sr_menu_action() === 7) {
      const results = [
        `Building upgraded. Silver: ${core.sr_game_value(3)}; metal: ${core.sr_game_value(5)}.`,
        'Invalid building action.', 'This building is already at its highest level.',
        'The final farm upgrade requires a mill or headquarters in this province.',
        'The farm upgrade requires a higher city level and enough silver and metal.',
        'The mining upgrade requires enough science ranks, silver and metal. Owned universities add five science ranks each.'
      ];
      status.textContent=results[core.sr_menu_result()] + (core.sr_menu_error() ? ' Dismiss the message with its OK button. Original message text rendering is pending.' : '');
    }
  });
  const cancel = () => { core.sr_menu_cancel(); pointer = null; render(); };
  canvas.addEventListener('pointercancel', cancel);
  canvas.addEventListener('pointermove', event => {
    if (pointer !== null && event.pointerId !== pointer) return;
    const oldArea=core.sr_menu_hover_area();
    core.sr_menu_move(...point(event));
    if (pointer !== null || oldArea!==core.sr_menu_hover_area() || core.sr_menu_screen()===21) render();
    canvas.dataset.hoverArea = core.sr_menu_hover_area();
  });
  window.addEventListener('blur', cancel);
  render(); if(!crossbowPreview && !linnePreview) status.textContent = preview;
  const animate = now => {
    const screen=core.sr_menu_screen(), phase=core.sr_menu_crossbow_value(0);
    const active=(screen===10 && phase!==0 && phase!==3 && phase!==10) ||
      (screen===11 && core.sr_menu_turn_value(0)===1) ||
      (screen===16 && (core.sr_menu_linne_value(0)===1 || core.sr_menu_linne_value(0)===2)) ||
      (screen===19 && pointer!==null) || (screen===21 && (core.sr_menu_battle_value(11)>=0 || core.sr_menu_battle_value(13) || core.sr_menu_battle_value(0)===4 || core.sr_menu_battle_value(0)===9));
    core.sr_menu_tick(Math.floor(now*60/1000)>>>0);
    if(active) render();
    requestAnimationFrame(animate);
  };
  requestAnimationFrame(animate);
  document.documentElement.dataset.ready = 'true';
} catch (error) {
  status.textContent = error.message;
  document.documentElement.dataset.ready = 'error';
  console.error(error);
}
