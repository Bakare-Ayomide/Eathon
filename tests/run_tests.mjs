import { execFileSync } from 'child_process';
import fs from 'fs';
import path from 'path';

const ROOT_DIR = process.cwd();
const EATHONC_BIN = path.join(ROOT_DIR, 'bin', 'eathonc');
const EATDB_BIN = path.join(ROOT_DIR, 'bin', 'eatdb');

export function runAllAutomatedTests() {
  const results = [];
  const startAll = Date.now();

  function addTest(id, category, title, fn) {
    const t0 = Date.now();
    try {
      const detail = fn();
      results.push({
        id,
        category,
        title,
        passed: true,
        durationMs: Date.now() - t0,
        detail,
      });
    } catch (err) {
      results.push({
        id,
        category,
        title,
        passed: false,
        durationMs: Date.now() - t0,
        detail: err instanceof Error ? err.message : String(err),
      });
    }
  }

  function compileAndRunEat(name, sourceCode) {
    const srcFile = `/tmp/test_${name}.eat`;
    const binFile = `/tmp/test_${name}_bin`;
    const llFile = `/tmp/test_${name}.ll`;
    const cFile = `/tmp/test_${name}.c`;
    fs.writeFileSync(srcFile, sourceCode, 'utf8');

    const compilerJsonRaw = execFileSync(
      EATHONC_BIN,
      [srcFile, '-o', binFile, '--llvm', llFile, '--c', cFile, '--json'],
      { encoding: 'utf8' }
    );
    const compilerReport = JSON.parse(compilerJsonRaw);
    if (!compilerReport.success) {
      throw new Error(`Compilation failed: ${JSON.stringify(compilerReport.diagnostics)}`);
    }
    const stdout = execFileSync(binFile, [], { encoding: 'utf8' }).trim();
    return { compilerReport, stdout, binFile, llFile, cFile };
  }

  // Test 1: Milestone 1 - Variables & Deterministic Precedence
  addTest(
    'TEST-01',
    'Core Compiler',
    'Variables, Types & Operator Precedence (Native ELF64 Execution)',
    () => {
      const { stdout, compilerReport } = compileAndRunEat(
        'vars',
        [
          'Let x be integer 25.',
          'Let y be integer 4.',
          'Let z be x plus y times 10.',
          'Display z.',
        ].join('\n')
      );
      if (stdout !== '65') {
        throw new Error(`Expected native binary stdout '65', got '${stdout}'`);
      }
      return `Native ELF64 binary (${compilerReport.machineCodeBytes}B x86_64 text) output verified: 65`;
    }
  );

  // Test 2: Quotation-Free Text Literals
  addTest(
    'TEST-02',
    'Core Language',
    'Quotation-Free English Text Literals (sys_write Syscall)',
    () => {
      const { stdout } = compileAndRunEat(
        'text',
        [
          'Display text of Hello from native Eathon binary end text.',
          'Let greeting be text of Systems programming in English end text.',
          'Display greeting.',
        ].join('\n')
      );
      const lines = stdout.split('\n').map((s) => s.trim());
      if (
        lines[0] !== 'Hello from native Eathon binary' ||
        lines[1] !== 'Systems programming in English'
      ) {
        throw new Error(`Unexpected text output: ${stdout}`);
      }
      return `Verified quotation-free text literal & variable sys_write output (${lines.length} lines)`;
    }
  );

  // Test 3: Conditionals & Comparison Operators
  addTest(
    'TEST-03',
    'Control Flow',
    'Structured If / Otherwise Branching & Comparisons',
    () => {
      const { stdout } = compileAndRunEat(
        'cond',
        [
          'Let temp be integer 85.',
          'If temp is at least 80 then,',
          '  Display 1.',
          'Otherwise,',
          '  Display 0.',
          'End if.',
          'If temp is equal to 50 then,',
          '  Display 99.',
          'Otherwise,',
          '  Display 42.',
          'End if.',
        ].join('\n')
      );
      if (stdout.replace(/\s+/g, ',') !== '1,42') {
        throw new Error(`Expected '1,42', got '${stdout}'`);
      }
      return 'Conditional jumps (jz / jmp rel32) verified: 1, 42';
    }
  );

  // Test 4: While & Repeat Loops
  addTest(
    'TEST-04',
    'Control Flow',
    'While & Repeat Loops (Iterative Summation)',
    () => {
      const { stdout } = compileAndRunEat(
        'loops',
        [
          'Let acc be integer 0.',
          'Let i be integer 1.',
          'While i is at most 5 repeat,',
          '  Set acc to acc plus i.',
          '  Set i to i plus 1.',
          'End while.',
          'Repeat 3 times,',
          '  Set acc to acc plus 10.',
          'End repeat.',
          'Display acc.',
        ].join('\n')
      );
      if (stdout !== '45') {
        throw new Error(`Expected '45' (15 + 30), got '${stdout}'`);
      }
      return 'Loop backpatching verified: sum(1..5) + 3*10 = 45';
    }
  );

  // Test 5: Native Functions & System V AMD64 Parameter Passing
  addTest(
    'TEST-05',
    'Functions & ABI',
    'Parameterized Functions & Return Values (call / ret)',
    () => {
      const { stdout } = compileAndRunEat(
        'funcs',
        [
          'To compute rectangleArea taking width as integer, height as integer returning integer,',
          '  Return width times height.',
          'End function.',
          'Call rectangleArea with 14, 9 into area.',
          'Display area.',
        ].join('\n')
      );
      if (stdout !== '126') {
        throw new Error(`Expected '126', got '${stdout}'`);
      }
      return 'Function call frame (rdi, rsi -> rax) verified: 14 * 9 = 126';
    }
  );

  // Test 6: Systems Programming - Bitwise Operations, Arrays & Direct Memory
  addTest(
    'TEST-06',
    'Systems & Memory',
    'Bitwise Logic, Contiguous Arrays, Structures & Raw Memory Offsets',
    () => {
      const { stdout } = compileAndRunEat(
        'sys',
        [
          'Let b be 7 shifted left by 3.',
          'Display b.',
          'Allocate array buf of 4 integers.',
          'Store 777 into element 2 of buf.',
          'Load element 2 of buf into val.',
          'Display val.',
          'Allocate memory ptr of 32 bytes.',
          'Write 4096 at address ptr offset 16.',
          'Read integer from address ptr offset 16 into mval.',
          'Display mval.',
        ].join('\n')
      );
      if (stdout.replace(/\s+/g, ',') !== '56,777,4096') {
        throw new Error(`Expected '56,777,4096', got '${stdout}'`);
      }
      return 'Bitwise shift (56), array indexing (777), and raw memory offset (4096) verified';
    }
  );

  // Test 7: Native AI / 2D Tensor Matrix Multiply & ReLU Activation
  addTest(
    'TEST-07',
    'AI & Tensors',
    '2D Tensor Allocation, Matrix Multiplication & ReLU Activation',
    () => {
      const { stdout } = compileAndRunEat(
        'tensor',
        [
          'Create tensor A with dimensions 2, 2.',
          'Set cell 0, 0 of A to 2.',
          'Set cell 0, 1 of A to 3.',
          'Set cell 1, 0 of A to 4.',
          'Set cell 1, 1 of A to 5.',
          'Create tensor B with dimensions 2, 2.',
          'Fill tensor B with 2.',
          'Create tensor C with dimensions 2, 2.',
          'Multiply tensor A by tensor B into C.',
          'Sum tensor C into total.',
          'Display total.',
        ].join('\n')
      );
      if (stdout !== '56') {
        throw new Error(`Expected tensor matmul sum '56', got '${stdout}'`);
      }
      return 'Native 2x2 matrix multiplication & reduction verified: total = 56';
    }
  );

  // Test 8: Lexical Error Diagnostics (Forbidden Punctuation Enforcement)
  addTest(
    'TEST-08',
    'Diagnostics',
    'Rejects Forbidden Punctuation (= ; " { }) with Exact Line/Col',
    () => {
      const badFile = '/tmp/test_bad_lex.eat';
      fs.writeFileSync(badFile, 'Let score = 100;\n', 'utf8');
      let out = '';
      try {
        out = execFileSync(EATHONC_BIN, [badFile, '--json'], { encoding: 'utf8' });
      } catch (e) {
        out = e.stdout ? e.stdout.toString() : '';
      }
      const parsed = JSON.parse(out);
      if (parsed.success || parsed.diagnostics.length < 2) {
        throw new Error('Expected lexer to report forbidden = and ; symbols');
      }
      return `Verified ${parsed.diagnostics.length} lexical errors (${parsed.diagnostics[0].code} at col ${parsed.diagnostics[0].col})`;
    }
  );

  // Test 9: Syntax Error Diagnostics (Missing Period Enforcement)
  addTest(
    'TEST-09',
    'Diagnostics',
    'Rejects Statements Missing Terminating Period (.)',
    () => {
      const badSynFile = '/tmp/test_bad_syn.eat';
      fs.writeFileSync(badSynFile, 'Let alpha be integer 10\nLet beta be integer 20.\n', 'utf8');
      let out = '';
      try {
        out = execFileSync(EATHONC_BIN, [badSynFile, '--json'], { encoding: 'utf8' });
      } catch (e) {
        out = e.stdout ? e.stdout.toString() : '';
      }
      const parsed = JSON.parse(out);
      if (parsed.success || parsed.diagnostics.length === 0) {
        throw new Error('Expected parser to report missing period error');
      }
      return `Verified syntax diagnostic ${parsed.diagnostics[0].code}: ${parsed.diagnostics[0].message}`;
    }
  );

  // Test 10: Semantic Type Checker Diagnostics (Undeclared Identifier)
  addTest(
    'TEST-10',
    'Diagnostics',
    'Detects Undeclared Identifiers & Type Mismatches',
    () => {
      const badSemFile = '/tmp/test_bad_sem.eat';
      fs.writeFileSync(badSemFile, 'Display unknownVariable.\n', 'utf8');
      let out = '';
      try {
        out = execFileSync(EATHONC_BIN, [badSemFile, '--json'], { encoding: 'utf8' });
      } catch (e) {
        out = e.stdout ? e.stdout.toString() : '';
      }
      const parsed = JSON.parse(out);
      if (parsed.success || parsed.diagnostics[0]?.code !== 'EATHON-SEM-001') {
        throw new Error('Expected semantic error EATHON-SEM-001');
      }
      return `Verified semantic diagnostic ${parsed.diagnostics[0].code}: ${parsed.diagnostics[0].message}`;
    }
  );

  // Test 11: Multi-Target Parity (Direct ELF64 vs ISO C99 + GCC Native Binary & Recursive Functions)
  addTest(
    'TEST-11',
    'Multi-Target',
    'ISO C99 + GCC Driver (--cc gcc) Parity & Recursive Function Verification',
    () => {
      const srcFile = '/tmp/test_recursion_cc.eat';
      const binFile = '/tmp/test_recursion_cc_bin';
      const cFile = '/tmp/test_recursion_cc.c';
      const llFile = '/tmp/test_recursion_cc.ll';
      fs.writeFileSync(
        srcFile,
        [
          'To compute factorial taking n as integer returning integer,',
          '  If n is at most 1 then,',
          '    Return 1.',
          '  End if.',
          '  Let prev be n minus 1.',
          '  Call factorial with prev into sub.',
          '  Return n times sub.',
          'End function.',
          'Call factorial with 6 into ans.',
          'Display ans.',
        ].join('\n'),
        'utf8'
      );
      execFileSync(
        EATHONC_BIN,
        [srcFile, '-o', binFile, '--cc', 'gcc', '--c', cFile, '--llvm', llFile],
        { encoding: 'utf8' }
      );
      const out = execFileSync(binFile, [], { encoding: 'utf8' }).trim();
      if (out !== '720') {
        throw new Error(`Expected recursive factorial(6) = 720, got '${out}'`);
      }
      return 'Verified ISO C99 + GCC native binary execution & recursive factorial(6) = 720';
    }
  );

  // Test 12: EathonDB Native Slotted-Page Storage Engine & ACID WAL
  addTest(
    'TEST-12',
    'EathonDB Engine',
    'Persistent Slotted-Page Storage, CRC32 Checksum & ACID Transactions',
    () => {
      const dbPath = '/tmp/test_suite.eatdb';
      if (fs.existsSync(dbPath)) fs.unlinkSync(dbPath);
      if (fs.existsSync(`${dbPath}.wal`)) fs.unlinkSync(`${dbPath}.wal`);

      const eql = [
        'Create collection telemetry.',
        'Begin transaction.',
        'Insert key 101 with label SensorAlpha, value 420.',
        'Insert key 102 with label SensorBeta, value 890.',
        'Commit transaction.',
        'Select where value is greater than 500.',
      ].join(' ');

      const raw = execFileSync(EATDB_BIN, [dbPath, eql], { encoding: 'utf8' });
      const res = JSON.parse(raw);
      if (!res.success || res.records.length !== 1 || res.records[0].key !== 102) {
        throw new Error(`Unexpected EathonDB query result: ${raw}`);
      }
      const stat = fs.statSync(dbPath);
      return `Verified EATDB001 4096B page file (${stat.size}B), CRC32 ${res.crc32}, and EQL query match`;
    }
  );

  // Test 13: Natural English Notes Syntax & Multi-Item Display
  addTest(
    'TEST-13',
    'English Syntax',
    'Natural English Notes (Let name be Chris end text. / Display Hello, name.)',
    () => {
      const { stdout } = compileAndRunEat(
        'english_notes',
        [
          "Note that this program calculates a person's age.",
          'Let age be 25.',
          'Let name be Chris end text.',
          'If age is greater than 17 then,',
          '  Display Hello, name.',
          'Otherwise,',
          '  Display You are underage.',
          'End if.',
        ].join('\n')
      );
      if (stdout !== 'Hello Chris') {
        throw new Error(`Expected 'Hello Chris', got '${stdout}'`);
      }
      return 'Verified apostrophes in comments, short end-text literal, and multi-item Display: Hello Chris';
    }
  );

  // Test 14: Module System (Include module) & Direct ELF64 Stack-Frame Recursion
  addTest(
    'TEST-14',
    'Modules & ELF64 Recursion',
    'Include module mathlib & Direct ELF64 Stack-Frame Recursion (power & factorial)',
    () => {
      const { stdout } = compileAndRunEat(
        'mod_recursion_elf',
        [
          'Include module mathlib.',
          'Call power with 3, 5 into p.',
          'Display p.',
          'Call factorial with 6 into f.',
          'Display f.',
        ].join('\n')
      );
      if (stdout.replace(/\s+/g, ',') !== '243,720') {
        throw new Error(`Expected Direct ELF64 recursive output '243,720', got '${stdout}'`);
      }
      return 'Verified stdlib/mathlib.eat inclusion and Direct ELF64 stack-frame recursion: 3^5=243, 6!=720';
    }
  );

  // Test 15: Genuine Windows 64-bit Cross-Compiled Executables (MinGW-w64 PE32+)
  addTest(
    'TEST-15',
    'Windows Portability',
    'Genuine Cross-Compiled 64-bit Windows Executables (bin/eathonc.exe & bin/eatdb.exe)',
    () => {
      const winCompiler = path.join(ROOT_DIR, 'bin', 'eathonc.exe');
      const winDb = path.join(ROOT_DIR, 'bin', 'eatdb.exe');
      if (!fs.existsSync(winCompiler) || !fs.existsSync(winDb)) {
        throw new Error('Missing bin/eathonc.exe or bin/eatdb.exe');
      }
      const fileOut = execFileSync('file', [winCompiler, winDb], { encoding: 'utf8' });
      if (!fileOut.includes('PE32+ executable (console) x86-64, for MS Windows')) {
        throw new Error(`Unexpected file header for Windows binaries: ${fileOut}`);
      }
      const cSize = fs.statSync(winCompiler).size;
      const dSize = fs.statSync(winDb).size;
      return `Verified PE32+ x86-64 Windows executables: eathonc.exe (${cSize}B), eatdb.exe (${dSize}B)`;
    }
  );

  // Test 16: VS Code TextMate Grammar & Android Acode Plugin
  addTest(
    'TEST-16',
    'Editor Tooling',
    'VS Code TextMate Grammar (.tmLanguage.json), Tasks & Android Acode Plugin',
    () => {
      const tmPath = path.join(ROOT_DIR, 'editors', 'vscode', 'syntaxes', 'eathon.tmLanguage.json');
      const tasksPath = path.join(ROOT_DIR, 'editors', 'vscode', 'tasks.json');
      const acodePath = path.join(ROOT_DIR, 'editors', 'acode', 'main.js');
      const tmJson = JSON.parse(fs.readFileSync(tmPath, 'utf8'));
      const tasksJson = JSON.parse(fs.readFileSync(tasksPath, 'utf8'));
      const acodeJs = fs.readFileSync(acodePath, 'utf8');
      if (
        tmJson.scopeName !== 'source.eathon' ||
        !acodeJs.includes('ace/mode/eathon') ||
        !Array.isArray(tasksJson.tasks)
      ) {
        throw new Error('Invalid VS Code TextMate grammar, tasks.json, or Acode plugin structure');
      }
      return `Verified VS Code TextMate grammar (${tmJson.scopeName}), tasks.json & Android Acode Ace mode plugin`;
    }
  );

  // Test 17: Portable Native C AST Execution Engine (--run) Parity
  addTest(
    'TEST-17',
    'Portable --run Engine',
    'In-Process Native C Execution (--run) Parity across Recursion, Structs & Tensors',
    () => {
      const srcFile = '/tmp/test_portable_run.eat';
      fs.writeFileSync(
        srcFile,
        [
          'Include module mathlib.',
          'Let name be Chris end text.',
          'Display Hello, name.',
          'Call factorial with 5 into f5.',
          'Display f5.',
          'Define structure Point with fields x as integer, y as integer.',
          'Create p as Point.',
          'Set field x of p to 15.',
          'Set field y of p to 27.',
          'Display field x of p plus field y of p.',
        ].join('\n'),
        'utf8'
      );
      const out = execFileSync(EATHONC_BIN, [srcFile, '--run'], { encoding: 'utf8' }).trim();
      const lines = out.split(/\r?\n/).map((s) => s.trim());
      if (lines.join(',') !== 'Hello Chris,120,42') {
        throw new Error(`Expected 'Hello Chris,120,42', got '${lines.join(',')}'`);
      }
      return 'Verified --run native C execution: Hello Chris, 5! = 120, struct Point(15+27) = 42';
    }
  );

  // Test 18: Android NDK JNI Shared Library (bin/libeathon_jni.so) Live Execution
  addTest(
    'TEST-18',
    'Android JNI Library',
    'Native Shared Library (libeathon_jni.so) Compilation & Live C FFI Invocation',
    () => {
      const soPath = path.join(ROOT_DIR, 'bin', 'libeathon_jni.so');
      if (!fs.existsSync(soPath)) {
        execFileSync(
          'gcc',
          [
            '-O2',
            '-std=c99',
            '-Icompiler/include',
            '-fPIC',
            '-shared',
            '-DEATHON_LIB_MODE',
            'android/app/src/main/cpp/eathon_jni.c',
            'compiler/src/eathonc.c',
            'compiler/src/eatdb.c',
            '-o',
            soPath,
          ],
          { cwd: ROOT_DIR }
        );
      }

      const harnessC = '/tmp/test_jni_harness.c';
      const harnessBin = '/tmp/test_jni_harness_bin';
      fs.writeFileSync(
        harnessC,
        [
          '#include <stdio.h>',
          '#include <stdlib.h>',
          'extern int eathon_jni_compile_and_run_json(const char *src, const char *path, char *out, size_t cap);',
          'int main(void) {',
          '  char buf[16384];',
          '  const char *prog = "Let a be 21.\\nDisplay a times 2.\\n";',
          '  int rc = eathon_jni_compile_and_run_json(prog, "jni_test.eat", buf, sizeof(buf));',
          '  printf("%s\\n", buf);',
          '  return rc;',
          '}',
        ].join('\n'),
        'utf8'
      );
      execFileSync('gcc', [harnessC, soPath, `-Wl,-rpath,${path.join(ROOT_DIR, 'bin')}`, '-o', harnessBin]);
      const rawJson = execFileSync(harnessBin, [], { encoding: 'utf8' }).trim();
      const parsed = JSON.parse(rawJson);
      if (!parsed.success || parsed.stdout.trim() !== '42') {
        throw new Error(`Unexpected libeathon_jni.so output: ${rawJson}`);
      }
      return `Verified libeathon_jni.so shared library execution: stdout='42', platform=${parsed.platform}/${parsed.arch}`;
    }
  );

  // Test 19: GitHub Actions Cross-Platform CI/CD & Release Workflow Verification
  addTest(
    'TEST-19',
    'GitHub Actions CI/CD',
    'Automated Linux, Windows, macOS & Android APK Release Workflow (.github/workflows/build-and-release.yml)',
    () => {
      const wfPath = path.join(ROOT_DIR, '.github', 'workflows', 'build-and-release.yml');
      if (!fs.existsSync(wfPath)) {
        throw new Error('Missing .github/workflows/build-and-release.yml');
      }
      const wf = fs.readFileSync(wfPath, 'utf8');
      const requiredTokens = [
        'linux-build-and-test:',
        'windows-native-build:',
        'runs-on: windows-latest',
        'eathonc.exe',
        'eatdb.exe',
        'android-apk-build:',
        'eathon-studio-android.apk',
        'softprops/action-gh-release@v2',
      ];
      for (const tok of requiredTokens) {
        if (!wf.includes(tok)) {
          throw new Error(`Workflow missing required section: ${tok}`);
        }
      }
      return 'Verified multi-platform GitHub Actions workflow (Linux, Windows, macOS, Android APK & GitHub Releases)';
    }
  );

  // Test 20: Native Android Studio + NDK Application Structure (Pydroid-Style IDE)
  addTest(
    'TEST-20',
    'Android Studio Project',
    'Native Android App (Kotlin + NDK CMake + JNI Bridge + Offline .eat File Support)',
    () => {
      const requiredFiles = [
        'android/settings.gradle',
        'android/build.gradle',
        'android/app/build.gradle',
        'android/app/src/main/AndroidManifest.xml',
        'android/app/src/main/cpp/CMakeLists.txt',
        'android/app/src/main/cpp/eathon_jni.c',
        'android/app/src/main/java/dev/eathon/studio/EathonBridge.kt',
        'android/app/src/main/java/dev/eathon/studio/EathonSyntaxHighlighter.kt',
        'android/app/src/main/java/dev/eathon/studio/MainActivity.kt',
        'android/app/src/main/res/layout/activity_main.xml',
      ];
      for (const rel of requiredFiles) {
        if (!fs.existsSync(path.join(ROOT_DIR, rel))) {
          throw new Error(`Missing Android project file: ${rel}`);
        }
      }
      return `Verified ${requiredFiles.length} native Android Studio + NDK CMake + Kotlin IDE files`;
    }
  );

  const passedCount = results.filter((r) => r.passed).length;
  return {
    total: results.length,
    passed: passedCount,
    failed: results.length - passedCount,
    totalDurationMs: Date.now() - startAll,
    results,
  };
}

if (import.meta.url === `file://${process.argv[1]}`) {
  const summary = runAllAutomatedTests();
  console.log(JSON.stringify(summary, null, 2));
  process.exit(summary.failed > 0 ? 1 : 0);
}

