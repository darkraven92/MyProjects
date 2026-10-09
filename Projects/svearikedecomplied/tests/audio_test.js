import { createSoundPlayer } from '../web/audio.js';

export async function verifyAudio(assert) {
  const rate = 22050;
  const context = new OfflineAudioContext(1, rate, rate);
  const samples = new Map();
  for (const [id, amplitude] of [[1, 0.5], [2, 0.25], [3, 0.125]]) {
    const buffer = context.createBuffer(1, 1024, rate);
    buffer.getChannelData(0).fill(amplitude);
    samples.set(id, { buffer, entry: { loop: true, rate, loop_start: 0, loop_end: 1024 } });
  }
  const player = createSoundPlayer(context, samples);
  player.play(0, 1);player.play(1, 2);
  const fadeWait = context.suspend(0.1), replaceWait = context.suspend(0.6);
  const rendered = context.startRendering();
  await fadeWait;
  const fadeStart = context.currentTime;
  player.play(0, -3);
  await context.resume();
  await replaceWait;
  const replaceStart = context.currentTime;
  player.play(1, 3);
  await context.resume();
  const pcm = (await rendered).getChannelData(0);
  const at = time => pcm[Math.round(time * rate)];
  const near = (a, b) => Math.abs(a - b) < 0.001;
  assert(near(at(0.05), 0.75), 'Audio mixer renders both independent channels');
  assert(near(at(fadeStart + 0.125), 0.5), 'Battle fade reaches half volume after half of 15 ticks');
  assert(near(at(fadeStart + 0.27), 0.25), 'Battle fade silences only channel 1 after 15 ticks');
  assert(near(at(replaceStart + 0.03), 0.125), 'Replacing an effect stops its predecessor without restoring faded background');

  // The original Linné score clears its looping channel by allowing the
  // current iteration to finish, unlike an immediate stop or a fade.
  const tailContext = new OfflineAudioContext(1, rate / 2, rate);
  const buffer = tailContext.createBuffer(1, rate / 5, rate);
  buffer.getChannelData(0).fill(0.5);
  const tailPlayer = createSoundPlayer(tailContext, new Map([[1, {
    buffer, entry: { loop: true, rate, loop_start: 0, loop_end: rate / 5 }
  }]]));
  tailPlayer.play(1, 1);
  const clearWait = tailContext.suspend(0.05), tailRendered = tailContext.startRendering();
  await clearWait;tailPlayer.play(1, -2);await tailContext.resume();
  const tail = (await tailRendered).getChannelData(0);
  assert(near(tail[Math.round(0.1 * rate)], 0.5) && near(tail[Math.round(0.3 * rate)], 0),
    'Clearing a score loop preserves its current tail then becomes silent');
}
