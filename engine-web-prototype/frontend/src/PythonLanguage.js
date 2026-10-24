import {StreamLanguage} from '@codemirror/language';

export const PythonLanguage = StreamLanguage.define({
  startState() {
    return {Quote: null};
  },
  token(Stream, State) {
    if (State.Quote) {
      while (!Stream.eol()) {
        if (Stream.match(State.Quote)) {
          State.Quote = null;
          break;
        }
        if (Stream.next() === '\\') {
          Stream.next();
        }
      }
      return 'string';
    }
    if (Stream.eatSpace()) {
      return null;
    }
    if (Stream.match('#')) {
      Stream.skipToEnd();
      return 'comment';
    }
    if (Stream.match(/(?:[rubf]{0,2})(?=['"])/i)) {
      let Quote = Stream.next();
      if (Stream.match(Quote + Quote)) {
        State.Quote = Quote + Quote + Quote;
        return 'string';
      }
      while (!Stream.eol()) {
        const Character = Stream.next();
        if (Character === '\\') {
          Stream.next();
        } else if (Character === Quote) {
          break;
        }
      }
      return 'string';
    }
    if (Stream.match(/@[_A-Za-z][_A-Za-z0-9.]*/)) {
      return 'meta';
    }
    if (Stream.match(/\b(?:False|True|None)\b/)) {
      return 'bool';
    }
    if (Stream.match(/\b(?:and|as|assert|async|await|break|class|continue|def|del|elif|else|except|finally|for|from|global|if|import|in|is|lambda|nonlocal|not|or|pass|raise|return|try|while|with|yield)\b/)) {
      return 'keyword';
    }
    if (Stream.match(/(?:0[xX][0-9a-fA-F_]+|0[bB][01_]+|0[oO][0-7_]+|(?:\d[\d_]*\.?[\d_]*|\.\d[\d_]*)(?:[eE][+-]?[\d_]+)?[jJ]?)/)) {
      return 'number';
    }
    if (Stream.match(/[_A-Za-z][_A-Za-z0-9]*/)) {
      if (Stream.match(/^\s*\(/, false)) {
        return 'variableName.function';
      }
      return 'variableName';
    }
    Stream.next();
    return 'operator';
  },
  languageData: {
    commentTokens: {line: '#'},
    closeBrackets: {brackets: ['(', '[', '{', "'", '"']}
  }
});
