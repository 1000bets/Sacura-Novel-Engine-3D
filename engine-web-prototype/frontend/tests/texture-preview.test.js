import Test from 'node:test';
import Assert from 'node:assert/strict';
import {ApplyTextureChannel} from '../src/texturePreview.js';

Test('RGBA preserves transparency while RGB shows the hidden color', () => {
  const Original = [12, 34, 56, 78, 90, 110, 130, 0];
  Assert.deepEqual([...ApplyTextureChannel(new Uint8ClampedArray(Original), 'RGBA')], Original);
  Assert.deepEqual([...ApplyTextureChannel(new Uint8ClampedArray(Original), 'RGB')], [12, 34, 56, 255, 90, 110, 130, 255]);
});
Test('individual channels show opaque grayscale including source alpha', () => {
  for (const [Channel, Value] of [['R', 12], ['G', 34], ['B', 56], ['A', 78]]) {
    Assert.deepEqual([...ApplyTextureChannel(new Uint8ClampedArray([12, 34, 56, 78]), Channel)], [Value, Value, Value, 255]);
  }
});
