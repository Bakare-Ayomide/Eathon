# Eathon (`.eat`) Editor Compatibility & Toolchain Integration Guide

Eathon source files (`.eat`) are **100% standard UTF-8 plain-text files** governed by the Eathon Punctuation Invariant (only commas `,` and periods `.` are valid punctuation). Because `.eat` files require no special binary container or proprietary encoding, they can be opened and edited in any desktop or mobile text editor.

> **Important Distinction Between Editing and Execution:**
> - **Editing & Syntax Highlighting:** Any plain-text editor (VS Code, Acode, Vim, Nano, Notepad++, Sublime Text, Android text editors) can open, edit, and save `.eat` files directly.
> - **Compilation & Execution:** Standard text editors do **not** execute `.eat` programs by themselves without invoking the Eathon compiler (`eathon` / `eathon.exe`) or opening the file inside the **Eathon Studio Android App** (`dev.eathon.studio`). Below are exact integration configurations for each environment.

---

## 1. Visual Studio Code (`editors/vscode/`)

### Included Files
- `editors/vscode/package.json` — Extension manifest registering the `eathon` language and `.eat` extension.
- `editors/vscode/language-configuration.json` — Auto-indentation and block folding rules for `If ... then,` / `Otherwise,` / `End if.`, `While ... repeat,` / `End while.`, `Repeat ... times,` / `End repeat.`, and `To compute ...` / `End function.`.
- `editors/vscode/syntaxes/eathon.tmLanguage.json` — TextMate grammar highlighting English-like keywords, operators, types, `Note that ... .` comments, `text of ... end text` literals, and flagging forbidden punctuation (`"`, `'`, `;`, `=`, `{`, `}`, `(`, `)`).
- `editors/vscode/tasks.json` — VS Code build/run tasks invoking `bin/eathon` (Linux/macOS) or `bin\eathon.exe` (Windows).

### Installation & External Compiler Setup
1. Copy the `editors/vscode` folder into your VS Code extensions directory:
   - **Windows:** `%USERPROFILE%\.vscode\extensions\eathon-language-support`
   - **Linux / macOS:** `~/.vscode/extensions/eathon-language-support`
2. To enable `Ctrl+Shift+B` (Build & Run) inside a workspace, copy `editors/vscode/tasks.json` into your project's `.vscode/tasks.json`.
3. Press `Ctrl+Shift+B` while viewing any `.eat` file to run `eathon <file.eat> --run` or compile a standalone native binary.

---

## 2. Acode Editor on Android (`editors/acode/`)

### Included Files
- `editors/acode/plugin.json` — Acode plugin manifest (`dev.eathon.acode`).
- `editors/acode/main.js` — Ace Editor mode (`ace/mode/eathon`) registering syntax highlighting and auto-indentation for `.eat` files in Acode.

### Editing vs. Running on Android
- **Editing in Acode:** Install the `editors/acode` plugin folder in Acode to edit `.eat` files with full syntax highlighting on Android.
- **Running `.eat` Programs on Android:** Acode is a code editor and does not bundle a native compiler runtime by default. To compile and run `.eat` files on Android:
  1. **Option A (Recommended — Eathon Studio Android APK):** Open or share the `.eat` file directly into the **Eathon Studio** Android app (`eathon-studio-android.apk`, built from `/android/`), which embeds `libeathon_jni.so` (`eathon` + `eatdb`) and executes `.eat` files natively offline.
  2. **Option B (Termux Terminal):** Inside Acode's Termux terminal on Android, compile `eathon.c` with Clang (`pkg install clang && clang -O2 -std=c99 -Icompiler/include compiler/src/eathon.c -o eathon`) and run `./eathon program.eat --run`.
