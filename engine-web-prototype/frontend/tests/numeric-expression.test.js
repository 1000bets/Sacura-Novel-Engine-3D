import Test from 'node:test';
import Assert from 'node:assert/strict';
import {EvaluateNumericExpression} from '../src/numericExpression.js';

Test('numeric expressions respect precedence, signs, powers and localized decimals', () => {
  for (const [Expression, Expected] of [['2+3*4',14],['(2+3)*4',20],['2^3^2',512],['-2^2',-4],['2**-2',0.25],['1,5 / 3',0.5],['1e-3*100',0.1],['10%3',1],['2 − -3',5]]) {
    Assert.equal(EvaluateNumericExpression(Expression), Expected, Expression);
  }
  Assert.equal(EvaluateNumericExpression('*=2',3),6);
  Assert.equal(EvaluateNumericExpression('+=1/2',3),3.5);
});
Test('invalid arithmetic cannot mutate a value or execute JavaScript', () => {
  for (const Expression of ['', '1/0', '2%0', '1e999', '(2+3', '2 +', '2foo', 'Math.random()', 'globalThis.process.exit()', '1;2', '2(3)', '1'.repeat(513)]) {
    Assert.throws(() => EvaluateNumericExpression(Expression), undefined, Expression);
  }
});
