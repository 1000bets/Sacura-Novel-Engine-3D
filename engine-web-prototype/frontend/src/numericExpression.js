export function EvaluateNumericExpression(Source, Previous = 0) {
  let Text = Source.trim().replaceAll(',', '.').replaceAll('−', '-').replaceAll('×', '*').replaceAll('÷', '/');
  if (!Text || Text.length > 512) {
    throw Error('Введите число или арифметическое выражение.');
  }
  const Relative = Text.match(/^([+\-*/])=/);
  if (Relative) {
    Text = '(' + Previous + ')' + Relative[1] + '(' + Text.slice(2) + ')';
  }
  let Position = 0;
  function Space() {
    while (/\s/.test(Text[Position] || '') && Position < Text.length) {
      Position++;
    }
  }
  function Take(Token) {
    Space();
    if (Text.startsWith(Token, Position)) {
      Position += Token.length;
      return true;
    }
    return false;
  }
  function Atom() {
    if (Take('(')) {
      const Value = Sum();
      if (!Take(')')) {
        throw Error('Не хватает закрывающей скобки.');
      }
      return Value;
    }
    Space();
    const Match = Text.slice(Position).match(/^(?:\d+(?:\.\d*)?|\.\d+)(?:e[+-]?\d+)?/i);
    if (!Match) {
      throw Error('Ожидается число или скобка.');
    }
    Position += Match[0].length;
    return Number(Match[0]);
  }
  function Power() {
    const Value = Atom();
    if (Take('**') || Take('^')) {
      return Value ** Unary();
    }
    return Value;
  }
  function Unary() {
    if (Take('+')) {
      return Unary();
    }
    if (Take('-')) {
      return -Unary();
    }
    return Power();
  }
  function Product() {
    let Value = Unary();
    while (true) {
      if (Take('*')) {
        Value *= Unary();
      } else if (Take('/')) {
        const Divisor = Unary();
        if (Divisor === 0) {
          throw Error('Деление на ноль недопустимо.');
        }
        Value /= Divisor;
      } else if (Take('%')) {
        const Divisor = Unary();
        if (Divisor === 0) {
          throw Error('Остаток от деления на ноль недопустим.');
        }
        Value %= Divisor;
      } else {
        return Value;
      }
    }
  }
  function Sum() {
    let Value = Product();
    while (true) {
      if (Take('+')) {
        Value += Product();
      } else if (Take('-')) {
        Value -= Product();
      } else {
        return Value;
      }
    }
  }
  const Result = Sum();
  Space();
  if (Position !== Text.length || !Number.isFinite(Result)) {
    throw Error('Некорректное выражение или слишком большое число.');
  }
  return Result;
}
