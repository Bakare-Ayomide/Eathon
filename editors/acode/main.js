/*
 * Eathon Syntax Highlighting Plugin for Acode (Android Code Editor)
 * Registers Ace Editor Mode "ace/mode/eathon" for .eat and .eatql files.
 */
(function () {
  const plugin = {
    id: 'org.eathon.syntax.acode',
  };

  function registerAceEathonMode() {
    if (typeof ace === 'undefined' || !ace.define) return;

    ace.define(
      'ace/mode/eathon_highlight_rules',
      ['require', 'exports', 'module', 'ace/lib/oop', 'ace/mode/text_highlight_rules'],
      function (require, exports) {
        const oop = require('ace/lib/oop');
        const TextHighlightRules = require('ace/mode/text_highlight_rules').TextHighlightRules;

        const EathonHighlightRules = function () {
          this.$rules = {
            start: [
              {
                token: 'comment.block.eathon',
                regex: '\\b[Nn]ote\\s+that\\b[^.]*\\.',
              },
              {
                token: 'string.unquoted.eathon',
                regex: '\\btext\\s+of\\b.*?\\bend\\s+text\\b',
              },
              {
                token: 'invalid.illegal.punctuation.eathon',
                regex: '["\';:=+\\-*/<>\\[\\]{}()]',
              },
              {
                token: 'keyword.control.eathon',
                regex:
                  '\\b(?:If|then|Otherwise|End\\s+if|While|repeat|End\\s+while|Repeat|End\\s+repeat|To\\s+compute|End\\s+function|Return|Display|Exit\\s+with|Include\\s+module)\\b',
              },
              {
                token: 'keyword.declaration.eathon',
                regex:
                  '\\b(?:Let|be|Set|to|Call|with|into|taking|as|returning|Allocate|array|memory|Store|Load|Write|Read|Define|structure|fields|Create|Get|Fill|Multiply|Activate|relu|Sum|Open|database|Insert|Query)\\b',
              },
              {
                token: 'storage.type.eathon',
                regex:
                  '\\b(?:integer|integers|byte|bytes|boolean|text|address|tensor|dimensions|collection)\\b',
              },
              {
                token: 'keyword.operator.eathon',
                regex:
                  '\\b(?:plus|minus|times|divided\\s+by|modulo|bitwise\\s+and|bitwise\\s+or|bitwise\\s+xor|bitwise\\s+not|shifted\\s+left\\s+by|shifted\\s+right\\s+by|is\\s+not\\s+equal\\s+to|is\\s+equal\\s+to|is\\s+greater\\s+than|is\\s+less\\s+than|is\\s+at\\s+least|is\\s+at\\s+most|and|or|not|neg|result\\s+of|element|field|cell|of|from|offset|key|value)\\b',
              },
              {
                token: 'constant.numeric.eathon',
                regex: '\\b[0-9]+\\b',
              },
              {
                token: 'constant.language.boolean.eathon',
                regex: '\\b(?:true|false)\\b',
              },
              {
                token: 'punctuation.operator.eathon',
                regex: '[.,]',
              },
              {
                token: 'identifier.eathon',
                regex: '\\b[A-Za-z][A-Za-z0-9]*\\b',
              },
            ],
          };
        };

        oop.inherits(EathonHighlightRules, TextHighlightRules);
        exports.EathonHighlightRules = EathonHighlightRules;
      }
    );

    ace.define(
      'ace/mode/eathon',
      ['require', 'exports', 'module', 'ace/lib/oop', 'ace/mode/text', 'ace/mode/eathon_highlight_rules'],
      function (require, exports) {
        const oop = require('ace/lib/oop');
        const TextMode = require('ace/mode/text').Mode;
        const EathonHighlightRules = require('ace/mode/eathon_highlight_rules').EathonHighlightRules;

        const Mode = function () {
          this.HighlightRules = EathonHighlightRules;
        };
        oop.inherits(Mode, TextMode);
        (function () {
          this.$id = 'ace/mode/eathon';
        }).call(Mode.prototype);

        exports.Mode = Mode;
      }
    );

    if (typeof editorManager !== 'undefined' && editorManager.addMode) {
      editorManager.addMode('eathon', ['eat', 'eatql'], 'Eathon (.eat)');
    }
  }

  if (typeof acode !== 'undefined') {
    acode.setPluginInit(plugin.id, () => {
      registerAceEathonMode();
    });
    acode.setPluginUnmount(plugin.id, () => {});
  }
})();
