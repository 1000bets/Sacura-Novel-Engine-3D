import React, { useEffect, useRef } from 'react';
import { basicSetup, EditorView } from 'codemirror';
import { keymap } from '@codemirror/view';
import { indentWithTab } from '@codemirror/commands';
import { EditorState, Compartment } from '@codemirror/state';
import { StreamLanguage, HighlightStyle, syntaxHighlighting } from '@codemirror/language';
import {PythonLanguage} from './PythonLanguage.js';
import {indentUnit} from '@codemirror/language';
import { tags } from '@lezer/highlight';
const ShaderLanguage = StreamLanguage.define({
  startState() {
    return {
      BlockComment: false
    };
  },
  token(Stream, State) {
    if (State.BlockComment) {
      if (Stream.skipTo('*/')) {
        Stream.match('*/');
        State.BlockComment = false;
      } else {
        Stream.skipToEnd();
      }
      return 'comment';
    }
    if (Stream.eatSpace()) {
      return null;
    }
    if (Stream.match('//')) {
      Stream.skipToEnd();
      return 'comment';
    }
    if (Stream.match('/*')) {
      State.BlockComment = true;
      return 'comment';
    }
    if (Stream.match(/"(?:[^"\\]|\\.)*"/)) {
      return 'string';
    }
    if (Stream.match(/#[A-Za-z_]+/)) {
      return 'meta';
    }
    if (Stream.match(/\b(?:float|half|double|int|uint|bool)(?:[1-4](?:x[1-4])?)?\b/)) {
      return 'typeName';
    }
    if (Stream.match(/\b(?:struct|return|if|else|for|while|break|continue|const|static|void|in|out|inout|cbuffer|discard)\b/)) {
      return 'keyword';
    }
    if (Stream.match(/\b(?:true|false)\b/)) {
      return 'bool';
    }
    if (Stream.match(/\b(?:0x[\da-fA-F]+|\d*\.?\d+(?:[eE][+-]?\d+)?[fFuU]?)\b/)) {
      return 'number';
    }
    if (Stream.match(/[A-Za-z_][A-Za-z_0-9]*/)) {
      if (Stream.match(/^\s*\(/, false)) {
        return 'variableName.function';
      }
      return 'variableName';
    }
    Stream.next();
    return 'operator';
  }
});
const ShaderHighlight = HighlightStyle.define([{
  tag: tags.keyword,
  color: 'var(--accent)'
}, {
  tag: tags.typeName,
  color: 'var(--info)'
}, {
  tag: tags.comment,
  color: 'var(--success)'
}, {
  tag: [tags.number, tags.bool, tags.string],
  color: 'var(--warning)'
}, {
  tag: tags.function(tags.variableName),
  color: 'var(--info)'
}, {
  tag: tags.meta,
  color: 'var(--accent)'
}]);
export default function NativeShaderEditor({
  language: Language = 'hlsl',
  source: Source,
  disabled: Disabled,
  pending: Pending,
  onChange: Change,
  onPosition: Position
}) {
  const Host = useRef(null);
  const View = useRef(null);
  const Callbacks = useRef({
    Change,
    Position
  });
  Callbacks.current = {
    Change,
    Position
  };
  const Synchronizing = useRef(false);
  const Editable = useRef(new Compartment());
  useEffect(() => {
    let Syntax = ShaderLanguage;
    let Label = 'Исходник шейдера';
    if (Language === 'python') {
      Syntax = PythonLanguage;
      Label = 'Python source';
    }
    const Editor = new EditorView({
      parent: Host.current,
      doc: Source,
      extensions: [basicSetup, keymap.of([indentWithTab]), indentUnit.of('    '), Syntax, syntaxHighlighting(ShaderHighlight), Editable.current.of([EditorState.readOnly.of(Disabled), EditorView.editable.of(!Disabled)]), EditorView.contentAttributes.of({
        'aria-label': Label,
        tabindex: '0',
        spellcheck: 'false'
      }), EditorView.theme({
        '&': {
          height: '100%',
          backgroundColor: 'var(--bg-app)',
          color: 'var(--text-primary)'
        },
        '.cm-scroller': {
          fontFamily: 'Consolas, monospace',
          fontSize: '13px',
          lineHeight: '1.65',
          overflow: 'auto'
        },
        '.cm-content': {
          padding: '12px 0'
        },
        '.cm-line': {
          padding: '0 16px'
        },
        '.cm-gutters': {
          backgroundColor: 'var(--bg-app)',
          color: 'var(--text-muted)',
          borderRight: '1px solid var(--border-subtle)'
        },
        '.cm-activeLine,.cm-activeLineGutter': {
          backgroundColor: 'var(--bg-panel-elevated)'
        },
        '.cm-cursor': {
          borderLeftColor: 'var(--accent)'
        },
        '&.cm-focused .cm-selectionBackground,.cm-selectionBackground': {
          backgroundColor: 'var(--accent-muted)'
        },
        '.cm-panels,.cm-tooltip': {
          backgroundColor: 'var(--bg-panel-elevated)',
          color: 'var(--text-primary)',
          borderColor: 'var(--border-subtle)'
        }
      }, {
        dark: true
      }), EditorView.updateListener.of(Update => {
        if (Update.docChanged && !Synchronizing.current) {
          Callbacks.current.Change(Update.state.doc.toString());
        }
        if (Update.selectionSet || Update.docChanged) {
          const Cursor = Update.state.selection.main.head;
          const Line = Update.state.doc.lineAt(Cursor);
          Callbacks.current.Position({
            Line: Line.number,
            Column: Cursor - Line.from + 1
          });
        }
      })]
    });
    View.current = Editor;
    let OwnerDocument = Editor.dom.ownerDocument;
    const OwnerTimer = setInterval(() => {
      if (OwnerDocument !== Editor.dom.ownerDocument) {
        OwnerDocument = Editor.dom.ownerDocument;
        Editor.setRoot(OwnerDocument);
        Editor.requestMeasure();
      }
    }, 100);
    return () => {
      clearInterval(OwnerTimer);
      View.current = null;
      Editor.destroy();
    };
  }, []);
  useEffect(() => {
    View.current.dispatch({
      effects: Editable.current.reconfigure([EditorState.readOnly.of(Disabled), EditorView.editable.of(!Disabled)])
    });
  }, [Disabled]);
  useEffect(() => {
    const Editor = View.current;
    if (!Pending && Editor.state.doc.toString() !== Source) {
      Synchronizing.current = true;
      Editor.dispatch({
        changes: {
          from: 0,
          to: Editor.state.doc.length,
          insert: Source
        }
      });
      Synchronizing.current = false;
    }
  }, [Source, Pending]);
  return <div className="native-shader-editor" ref={Host} />;
}
