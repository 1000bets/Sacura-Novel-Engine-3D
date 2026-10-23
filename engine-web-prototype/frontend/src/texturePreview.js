export function ApplyTextureChannel(Pixels, Channel) {
  for (let Offset = 0; Offset < Pixels.length; Offset += 4) {
    if (Channel === 'RGB') {
      Pixels[Offset + 3] = 255;
    } else if (Channel !== 'RGBA') {
      const ChannelOffset = {R: 0, G: 1, B: 2, A: 3}[Channel];
      const Value = Pixels[Offset + ChannelOffset];
      Pixels[Offset] = Value;
      Pixels[Offset + 1] = Value;
      Pixels[Offset + 2] = Value;
      Pixels[Offset + 3] = 255;
    }
  }
  return Pixels;
}
