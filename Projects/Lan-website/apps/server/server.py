import asyncio, json, math, os, random, secrets, time
from aiohttp import web, WSMsgType

ROOT = os.path.dirname(__file__)
WEB_ROOT = os.path.abspath(os.path.join(ROOT, '..', 'web'))
GAMES = [{'id':'warlock','title':'Warlock','subtitle':'Arena spell duels','players':'1–8','status':'playable','description':'A browser multiplayer proof of concept. Spells and physics are placeholders, not a 1:1 recreation of Warlock 1.02.'}, {'id':'next-game','title':'Next game','subtitle':'Your next LAN favorite','players':'TBA','status':'coming-soon','description':'Add future game modules without changing the platform.'}]
ROOMS = {}
TICK = 1/30
RADIUS = 16
ARENA_W, ARENA_H = 1000, 650


def clamp(x, a, b): return max(a, min(b, x))


def make_room(game='warlock'):
    while True:
        code = secrets.token_hex(3).upper()
        if code not in ROOMS: break
    room = {'game': game, 'code': code, 'players': {}, 'projectiles': [], 'round': 0, 'status': 'lobby', 'host': None, 'last_active': time.monotonic()}
    ROOMS[code] = room
    return room


def safe_send(ws, payload):
    if ws and not ws.closed: return ws.send_str(json.dumps(payload, separators=(',', ':')))
    return asyncio.sleep(0)


def snapshot(room):
    return {'type': 'state', 'code': room['code'], 'game': room['game'], 'status': room['status'], 'round': room['round'], 'host': room['host'],
            'players': [{k: v for k, v in p.items() if k not in ('ws', 'target', 'cast', 'last_seen')} for p in room['players'].values()],
            'projectiles': [{k:v for k,v in q.items() if k != 'owner'} for q in room['projectiles']]}


def spawn(p, i, total):
    ang = i * (2*math.pi/max(total, 1))
    p.update(x=ARENA_W/2+210*math.cos(ang),y=ARENA_H/2+185*math.sin(ang),hp=100,alive=True,shield=0,
             target=None, cooldown=0, blast_cd=0, shield_cd=0)


def start_round(room):
    room['round'] += 1
    room['status'] = 'playing'
    room['projectiles'] = []
    players = list(room['players'].values())
    for i, p in enumerate(players): spawn(p, i, len(players))


async def broadcast(room, payload):
    await asyncio.gather(*(safe_send(p['ws'], payload) for p in room['players'].values()), return_exceptions=True)


async def websocket(request):
    ws = web.WebSocketResponse(heartbeat=20, max_msg_size=4096)
    await ws.prepare(request)
    p = None; room = None
    await safe_send(ws, {'type':'welcome'})
    try:
        async for msg in ws:
            if msg.type != WSMsgType.TEXT: continue
            try: m = json.loads(msg.data)
            except (ValueError, TypeError): continue
            typ = m.get('type')
            if typ == 'join' and p is None:
                code = str(m.get('code', '')).upper().strip()
                game = str(m.get('game', 'warlock'))
                if game != 'warlock':
                    await safe_send(ws, {'type':'error','message':'Game not available'}); continue
                room = ROOMS.get(code) if code else make_room(game)
                if (room and room['game'] != game) or not room or len(room['players']) >= 8 or room['status'] == 'playing':
                    await safe_send(ws, {'type':'error','message':'Room not found, full, or game in progress'})
                    room = None; continue
                pid = secrets.token_hex(4)
                name = str(m.get('name', 'Warlock'))[:20].strip() or 'Warlock'
                p = {'id':pid,'name':name,'ws':ws,'x':500.,'y':325.,'hp':100,'alive':True,
                     'color':len(room['players'])%8,'target':None,'cast':None,'cooldown':0., 'blast_cd':0., 'shield_cd':0., 'shield':0.}
                room['players'][pid] = p
                room['host'] = room['host'] or pid
                room['last_active'] = time.monotonic()
                await safe_send(ws, {'type':'joined','id':pid,'code':room['code']})
            elif p and room:
                if typ == 'start' and room['host'] == p['id'] and room['status'] != 'playing' and len(room['players'])>=1:
                    start_round(room)
                elif typ == 'move' and room['status']=='playing' and p['alive']:
                    try:
                        x,y = float(m['x']), float(m['y'])
                        if math.isfinite(x) and math.isfinite(y):p['target']=(clamp(x,RADIUS,ARENA_W-RADIUS),clamp(y,RADIUS,ARENA_H-RADIUS))
                    except (KeyError,ValueError,TypeError): pass
                elif typ == 'cast' and room['status']=='playing' and p['alive']:
                    spell = m.get('spell')
                    if spell in ('bolt','blast','shield'):
                        try:
                            x,y = float(m.get('x',p['x']+1)),float(m.get('y',p['y']))
                            if math.isfinite(x) and math.isfinite(y):p['cast']=(spell,clamp(x,0,ARENA_W),clamp(y,0,ARENA_H))
                        except (TypeError,ValueError): pass
    finally:
        if room and p:
            room['players'].pop(p['id'], None)
            if room['host'] == p['id']: room['host'] = next(iter(room['players']), None)
            if not room['players']:ROOMS.pop(room['code'], None)
            elif room['status']=='playing' and sum(1 for q in room['players'].values() if q['alive'])<=1:room['status']='finished'
    return ws


def damage(victim, amount, dx, dy):
    if not victim['alive'] or victim['shield'] > 0: return
    victim['hp']=max(0,victim['hp']-amount)
    victim['x']=clamp(victim['x']+dx, RADIUS, ARENA_W-RADIUS)
    victim['y']=clamp(victim['y']+dy, RADIUS, ARENA_H-RADIUS)
    if victim['hp']<=0:victim['alive']=False


def step_room(room, dt):
    if room['status'] != 'playing':return
    for p in room['players'].values():
        if not p['alive']:continue
        for cd in ('cooldown','blast_cd','shield_cd','shield'):p[cd]=max(0,p[cd]-dt)
        if p['target']:
            dx=p['target'][0]-p['x'];dy=p['target'][1]-p['y']; d=math.hypot(dx,dy)
            if d<3:p['target']=None
            else:
                s=min(d,205*dt);p['x']+=dx/d*s;p['y']+=dy/d*s
        if p['cast']:
            spell,x,y=p['cast'];p['cast']=None
            dx=x-p['x'];dy=y-p['y'];d=max(0.001,math.hypot(dx,dy));ux=dx/d;uy=dy/d
            if spell=='bolt' and p['cooldown']==0:
                p['cooldown']=0.75
                room['projectiles'].append({'x':p['x']+ux*22,'y':p['y']+uy*22,'vx':ux*530,'vy':uy*530,'ttl':1.7,'owner':p['id'],'color':p['color']})
            elif spell=='blast' and p['blast_cd']==0:
                p['blast_cd']=5
                for v in room['players'].values():
                    dd=math.hypot(v['x']-p['x'],v['y']-p['y'])
                    if v is not p and dd<=125:
                        dd=max(0.01,dd);damage(v,24,(v['x']-p['x'])/dd*72,(v['y']-p['y'])/dd*72)
            elif spell=='shield' and p['shield_cd']==0:
                p['shield_cd']=7;p['shield']=1.5
    for q in room['projectiles'][:]:
        old_x,old_y=q['x'],q['y'];q['x']+=q['vx']*dt;q['y']+=q['vy']*dt;q['ttl']-=dt
        hit=False
        for p in room['players'].values():
            if p['id']==q['owner'] or not p['alive']:continue
            # segment-circle collision prevents high-speed tunneling
            vx=q['x']-old_x;vy=q['y']-old_y;t=clamp(((p['x']-old_x)*vx+(p['y']-old_y)*vy)/max(0.001,vx*vx+vy*vy),0,1)
            dx=p['x']-(old_x+t*vx);dy=p['y']-(old_y+t*vy)
            if dx*dx+dy*dy<(RADIUS+7)**2:
                mag=max(0.001,math.hypot(q['vx'],q['vy']))
                damage(p,16,q['vx']/mag*60,q['vy']/mag*60);hit=True;break
        if hit or q['ttl']<=0 or not (0<q['x']<ARENA_W and 0<q['y']<ARENA_H):room['projectiles'].remove(q)
    if len(room['players'])>1 and sum(1 for p in room['players'].values() if p['alive'])<=1:room['status']='finished'


async def game_loop(app):
    try:
        while True:
            t=time.monotonic()
            for room in list(ROOMS.values()):
                step_room(room,TICK)
                if room['players']:await broadcast(room,snapshot(room))
            await asyncio.sleep(max(0,TICK-(time.monotonic()-t)))
    except asyncio.CancelledError:pass


async def ctx(app):
    task=asyncio.create_task(game_loop(app))
    yield
    task.cancel();await asyncio.gather(task,return_exceptions=True)


app=web.Application()
app.router.add_get('/ws',websocket)
app.router.add_get('/api/games',lambda _:web.json_response(GAMES))
app.router.add_get('/api/rooms',lambda _:web.json_response([{'code':r['code'],'game':r['game'],'players':len(r['players']),'maxPlayers':8,'status':r['status']} for r in ROOMS.values()]))
app.router.add_get('/',lambda _:web.FileResponse(os.path.join(WEB_ROOT,'index.html')))
app.router.add_static('/static/',WEB_ROOT)
app.cleanup_ctx.append(ctx)
if __name__=='__main__':web.run_app(app,host='0.0.0.0',port=int(os.environ.get('PORT','8080')))
