// Two original sound channels. C supplies cast IDs and control commands.
export function createSoundPlayer(context, samples, onEvent = () => {}) {
  const channels = [null, null];
  function play(channel, sound) {
    if (!sound) return;
    const previous = channels[channel];
    if (sound === -2) {
      // Clearing a looping score channel finishes its current iteration.
      if (previous) previous.source.loop = false;
    } else if (sound === -3) {
      // KRIG ScoreScript 45: fadeOut channel 1 over 15 ticks (60 Hz).
      // Fade changes volume; it does not request a stop or affect channel 2.
      if (previous) {
        const now = context.currentTime;
        previous.gain.gain.cancelAndHoldAtTime(now);
        previous.gain.gain.linearRampToValueAtTime(0, now + 15 / 60);
      }
    } else {
      if (previous) previous.source.stop();
      channels[channel] = null;
      if (sound > 0) {
        const sample = samples.get(sound);
        if (!sample) throw new Error('Missing original sound ' + sound);
        const source = context.createBufferSource();
        const gain = context.createGain();
        source.buffer = sample.buffer;
        source.loop = !!sample.entry.loop;
        if (source.loop) {
          source.loopStart = sample.entry.loop_start / sample.entry.rate;
          source.loopEnd = sample.entry.loop_end / sample.entry.rate;
        }
        source.connect(gain);gain.connect(context.destination);source.start();
        const record = { source, gain };
        channels[channel] = record;
        source.onended = () => {
          source.disconnect();gain.disconnect();
          if (channels[channel] === record) channels[channel] = null;
        };
      }
    }
    onEvent(channel, sound, channels[channel]);
  }
  return { play };
}
