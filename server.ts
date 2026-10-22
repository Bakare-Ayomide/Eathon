import express from 'express';
import { createServer as createViteServer } from 'vite';
import { execFileSync } from 'child_process';
import fs from 'fs';
import path from 'path';
import { runAllAutomatedTests } from './tests/run_tests.mjs';

const ROOT_DIR = process.cwd();
const BIN_DIR = path.join(ROOT_DIR, 'bin');
const EATHONC_BIN = path.join(BIN_DIR, 'eathonc');
const EATDB_BIN = path.join(BIN_DIR, 'eatdb');

function ensureCompilerBuilt() {
  if (!fs.existsSync(BIN_DIR)) {
    fs.mkdirSync(BIN_DIR, { recursive: true });
  }
  if (!fs.existsSync(EATHONC_BIN)) {
    execFileSync('gcc', [
      '-O2',
      '-std=c99',
      '-Icompiler/include',
      '-D_POSIX_C_SOURCE=200809L',
      path.join(ROOT_DIR, 'compiler/src/eathonc.c'),
      '-o',
      EATHONC_BIN,
    ]);
  }
  if (!fs.existsSync(EATDB_BIN)) {
    execFileSync('gcc', [
      '-O2',
      '-std=c99',
      '-Icompiler/include',
      '-D_POSIX_C_SOURCE=200809L',
      path.join(ROOT_DIR, 'compiler/src/eatdb.c'),
      '-o',
      EATDB_BIN,
    ]);
  }
}

function disassembleElfCode(binPath: string, machineCodeHex: string): string {
  try {
    // Disassemble the raw x86_64 code bytes using GNU objdump if installed
    const rawBinPath = `${binPath}.raw`;
    fs.writeFileSync(rawBinPath, Buffer.from(machineCodeHex, 'hex'));
    const disasm = execFileSync(
      'objdump',
      ['-D', '-b', 'binary', '-m', 'i386:x86-64', '-M', 'intel', rawBinPath],
      { encoding: 'utf8' }
    );
    return disasm;
  } catch {
    // Fallback structured hex + instruction breakdown if objdump is unavailable
    const bytes = Buffer.from(machineCodeHex, 'hex');
    const lines: string[] = [];
    for (let i = 0; i < bytes.length; i += 8) {
      const slice = bytes.subarray(i, Math.min(i + 8, bytes.length));
      const hex = Array.from(slice)
        .map((b) => b.toString(16).padStart(2, '0'))
        .join(' ');
      lines.push(`0x${(0x400078 + i).toString(16).padStart(8, '0')}:  ${hex}`);
    }
    return lines.join('\n');
  }
}

async function startServer() {
  ensureCompilerBuilt();

  const app = express();
  app.use(express.json({ limit: '2mb' }));

  // 1. Environment & Toolchain Inspection Endpoint
  app.get('/api/environment', (_req, res) => {
    let uname = 'Linux x86_64';
    let gccVersion = 'GCC (GNU Compiler Collection)';
    try {
      uname = execFileSync('uname', ['-srm'], { encoding: 'utf8' }).trim();
      gccVersion = execFileSync('gcc', ['--version'], { encoding: 'utf8' })
        .split('\n')[0]
        .trim();
    } catch {
      // ignore
    }
    const eathoncStat = fs.existsSync(EATHONC_BIN) ? fs.statSync(EATHONC_BIN).size : 0;
    const eatdbStat = fs.existsSync(EATDB_BIN) ? fs.statSync(EATDB_BIN).size : 0;

    res.json({
      kernel: uname,
      hostCompiler: gccVersion,
      eathoncBinaryPath: 'bin/eathonc',
      eathoncBinaryBytes: eathoncStat,
      eatdbBinaryPath: 'bin/eatdb',
      eatdbBinaryBytes: eatdbStat,
      targets: [
        'x86_64-linux-elf64',
        'x86_64-windows-pe64',
        'android-ndk-jni-so',
        'portable-c-run',
        'llvm-ir-ssa',
        'iso-c99',
      ],
    });
  });

  // 2. List Example Programs & Repository Files
  app.get('/api/files', (_req, res) => {
    const examplesDir = path.join(ROOT_DIR, 'examples');
    const exampleFiles = fs
      .readdirSync(examplesDir)
      .filter((f) => f.endsWith('.eat'))
      .sort()
      .map((f) => ({
        name: f,
        path: `examples/${f}`,
        content: fs.readFileSync(path.join(examplesDir, f), 'utf8'),
      }));

    const repoFiles = [
      'doc.md',
      '.github/workflows/build-and-release.yml',
      'compiler/include/eathon_platform.h',
      'compiler/src/eathonc.c',
      'compiler/src/eatdb.c',
      'android/app/build.gradle',
      'android/app/src/main/AndroidManifest.xml',
      'android/app/src/main/cpp/CMakeLists.txt',
      'android/app/src/main/cpp/eathon_jni.c',
      'android/app/src/main/java/dev/eathon/studio/MainActivity.kt',
      'android/app/src/main/java/dev/eathon/studio/EathonBridge.kt',
      'android/app/src/main/java/dev/eathon/studio/EathonSyntaxHighlighter.kt',
      'android/app/src/main/res/layout/activity_main.xml',
      'editors/README.md',
      'editors/vscode/package.json',
      'editors/vscode/tasks.json',
      'editors/vscode/syntaxes/eathon.tmLanguage.json',
      'editors/acode/plugin.json',
      'editors/acode/main.js',
      'stdlib/mathlib.eat',
      'spec/GRAMMAR.ebnf',
      'Makefile',
      'build_windows.bat',
      'tests/run_tests.mjs',
    ].map((relPath) => {
      const full = path.join(ROOT_DIR, relPath);
      const content = fs.existsSync(full) ? fs.readFileSync(full, 'utf8') : '';
      return {
        path: relPath,
        sizeBytes: Buffer.byteLength(content, 'utf8'),
        lines: content.split('\n').length,
        content,
      };
    });

    res.json({ examples: exampleFiles, repoFiles });
  });

  // 2b. Download Complete Project ZIP Archive (Binary & Base64 JSON for iframe-safe Blob download)
  const ensureZipExists = () => {
    const zipPath = path.join(ROOT_DIR, 'public', 'eathon-project.zip');
    if (!fs.existsSync(zipPath)) {
      try {
        execFileSync('python3', [path.join(ROOT_DIR, 'scripts', 'package_zip.py')]);
      } catch {
        // ignore
      }
    }
    return zipPath;
  };

  app.get('/api/download-zip', (_req, res) => {
    const zipPath = ensureZipExists();
    if (fs.existsSync(zipPath)) {
      const buf = fs.readFileSync(zipPath);
      res.setHeader('Content-Type', 'application/zip');
      res.setHeader('Content-Disposition', 'attachment; filename="eathon-project.zip"');
      res.setHeader('Content-Length', String(buf.length));
      res.send(buf);
    } else {
      res.status(404).send('ZIP archive not found.');
    }
  });

  app.get('/api/download-zip-json', (_req, res) => {
    const zipPath = ensureZipExists();
    if (fs.existsSync(zipPath)) {
      const buf = fs.readFileSync(zipPath);
      res.json({
        filename: 'eathon-project.zip',
        sizeBytes: buf.length,
        base64: buf.toString('base64'),
      });
    } else {
      res.status(404).json({ error: 'ZIP archive not found.' });
    }
  });

  // 3. Compile & Execute .eat Source Code via Native C Compiler (bin/eathonc)
  app.post('/api/compile', (req, res) => {
    try {
      const source = typeof req.body?.source === 'string' ? req.body.source : '';
      const runBinary = req.body?.execute !== false;

      const jobId = `job_${Date.now()}_${Math.random().toString(36).slice(2, 7)}`;
      const srcPath = `/tmp/${jobId}.eat`;
      const elfPath = `/tmp/${jobId}_elf64`;
      const pePath = `/tmp/${jobId}.exe`;
      const llPath = `/tmp/${jobId}.ll`;
      const cPath = `/tmp/${jobId}.c`;

      fs.writeFileSync(srcPath, source, 'utf8');

      const compileStart = performance.now();
      let compilerRaw = '';
      try {
        compilerRaw = execFileSync(
          EATHONC_BIN,
          [srcPath, '-o', elfPath, '--pe', pePath, '--llvm', llPath, '--c', cPath, '--json'],
          { encoding: 'utf8', timeout: 5000 }
        );
      } catch (err: any) {
        compilerRaw = err.stdout ? err.stdout.toString() : '';
        if (!compilerRaw) {
          throw err;
        }
      }
      const compileTimeMs = Number((performance.now() - compileStart).toFixed(2));
      const report = JSON.parse(compilerRaw);

      let llvmIr = '';
      let c99Code = '';
      let disassembly = '';
      let elfSizeBytes = 0;
      let peSizeBytes = 0;
      let elfBase64 = '';
      let peBase64 = '';
      let executionOutput = '';
      let exitCode = 0;
      let execTimeMs = 0;

      if (report.success) {
        if (fs.existsSync(llPath)) llvmIr = fs.readFileSync(llPath, 'utf8');
        if (fs.existsSync(cPath)) c99Code = fs.readFileSync(cPath, 'utf8');
        if (fs.existsSync(elfPath)) {
          const elfBuf = fs.readFileSync(elfPath);
          elfSizeBytes = elfBuf.length;
          elfBase64 = elfBuf.toString('base64');
        }
        if (fs.existsSync(pePath)) {
          const peBuf = fs.readFileSync(pePath);
          peSizeBytes = peBuf.length;
          peBase64 = peBuf.toString('base64');
        }
        disassembly = disassembleElfCode(elfPath, report.machineCodeHex || '');

        if (runBinary && fs.existsSync(elfPath)) {
          const execStart = performance.now();
          try {
            executionOutput = execFileSync(elfPath, [], {
              encoding: 'utf8',
              timeout: 4000,
            });
            exitCode = 0;
          } catch (execErr: any) {
            executionOutput =
              (execErr.stdout ? execErr.stdout.toString() : '') +
              (execErr.stderr ? execErr.stderr.toString() : '');
            exitCode = typeof execErr.status === 'number' ? execErr.status : 1;
          }
          execTimeMs = Number((performance.now() - execStart).toFixed(2));
        }
      }

      // Clean up temporary files
      for (const f of [srcPath, elfPath, `${elfPath}.raw`, pePath, llPath, cPath]) {
        if (fs.existsSync(f)) {
          try {
            fs.unlinkSync(f);
          } catch {
            // ignore
          }
        }
      }

      res.json({
        ...report,
        compileTimeMs,
        execTimeMs,
        executionOutput,
        exitCode,
        llvmIr,
        c99Code,
        disassembly,
        elfSizeBytes,
        peSizeBytes,
        elfBase64,
        peBase64,
      });
    } catch (err: any) {
      res.status(500).json({
        success: false,
        error: err instanceof Error ? err.message : String(err),
      });
    }
  });

  // 4. Eathon Formatter & Static Linter
  app.post('/api/format', (req, res) => {
    const source = typeof req.body?.source === 'string' ? req.body.source : '';
    const rawLines = source.split('\n');
    let indentLevel = 0;
    const formattedLines: string[] = [];

    for (const rawLine of rawLines) {
      const trimmed = rawLine.trim().replace(/\s+/g, ' ');
      if (!trimmed) {
        formattedLines.push('');
        continue;
      }
      const lower = trimmed.toLowerCase();
      if (
        lower.startsWith('end if.') ||
        lower.startsWith('end while.') ||
        lower.startsWith('end repeat.') ||
        lower.startsWith('end function.') ||
        lower.startsWith('otherwise,')
      ) {
        indentLevel = Math.max(0, indentLevel - 1);
      }

      const pad = '  '.repeat(indentLevel);
      // Capitalize first letter of statement keyword
      const cap = trimmed.charAt(0).toUpperCase() + trimmed.slice(1);
      formattedLines.push(`${pad}${cap}`);

      if (
        lower.endsWith('then,') ||
        lower.endsWith('repeat,') ||
        lower.endsWith('times,') ||
        lower.endsWith('otherwise,') ||
        (lower.startsWith('to compute ') && lower.endsWith(','))
      ) {
        indentLevel++;
      }
    }

    res.json({ formatted: formattedLines.join('\n') });
  });

  // 5. EathonDB Native Storage Engine Endpoint (bin/eatdb)
  const PERSISTENT_DB_PATH = '/tmp/eathon_workbench.eatdb';

  // Seed initial persistent database if not yet created
  if (!fs.existsSync(PERSISTENT_DB_PATH)) {
    try {
      execFileSync(EATDB_BIN, [
        PERSISTENT_DB_PATH,
        [
          'Create collection systemMetrics.',
          'Begin transaction.',
          'Insert key 1001 with label KernelPageAlloc, value 4096.',
          'Insert key 1002 with label CompilerPassLatencyUs, value 380.',
          'Insert key 1003 with label TensorMatMulFlops, value 64000.',
          'Insert key 1004 with label ActiveWorkerThreads, value 16.',
          'Commit transaction.',
        ].join(' '),
      ]);
    } catch {
      // ignore
    }
  }

  app.post('/api/eatdb/query', (req, res) => {
    try {
      const query = typeof req.body?.query === 'string' ? req.body.query : 'Select all.';
      const reset = req.body?.reset === true;

      if (reset) {
        if (fs.existsSync(PERSISTENT_DB_PATH)) fs.unlinkSync(PERSISTENT_DB_PATH);
        if (fs.existsSync(`${PERSISTENT_DB_PATH}.wal`)) fs.unlinkSync(`${PERSISTENT_DB_PATH}.wal`);
      }

      const outRaw = execFileSync(EATDB_BIN, [PERSISTENT_DB_PATH, query], {
        encoding: 'utf8',
        timeout: 4000,
      });
      const result = JSON.parse(outRaw);

      let walContent = '';
      if (fs.existsSync(`${PERSISTENT_DB_PATH}.wal`)) {
        walContent = fs.readFileSync(`${PERSISTENT_DB_PATH}.wal`, 'utf8');
      }

      let superblockHex = '';
      let fileSizeBytes = 0;
      if (fs.existsSync(PERSISTENT_DB_PATH)) {
        const dbBytes = fs.readFileSync(PERSISTENT_DB_PATH);
        fileSizeBytes = dbBytes.length;
        const sbSlice = dbBytes.subarray(0, Math.min(128, dbBytes.length));
        const rows: string[] = [];
        for (let i = 0; i < sbSlice.length; i += 16) {
          const chunk = sbSlice.subarray(i, i + 16);
          const hex = Array.from(chunk)
            .map((b) => b.toString(16).padStart(2, '0'))
            .join(' ');
          const ascii = Array.from(chunk)
            .map((b) => (b >= 32 && b <= 126 ? String.fromCharCode(b) : '.'))
            .join('');
          rows.push(`0x${i.toString(16).padStart(4, '0')}:  ${hex.padEnd(47, ' ')}  |${ascii}|`);
        }
        superblockHex = rows.join('\n');
      }

      res.json({
        ...result,
        walContent,
        superblockHex,
        fileSizeBytes,
      });
    } catch (err: any) {
      res.status(400).json({
        success: false,
        error: err instanceof Error ? err.message : String(err),
      });
    }
  });

  // 6. Automated Test Suite Runner Endpoint
  app.post('/api/tests/run', (_req, res) => {
    try {
      const summary = runAllAutomatedTests();
      res.json(summary);
    } catch (err: any) {
      res.status(500).json({
        total: 0,
        passed: 0,
        failed: 1,
        error: err instanceof Error ? err.message : String(err),
      });
    }
  });

  if (process.env.NODE_ENV !== 'production') {
    const vite = await createViteServer({
      server: { middlewareMode: true },
      appType: 'spa',
    });
    app.use(vite.middlewares);
  } else {
    const distPath = path.join(ROOT_DIR, 'dist');
    app.use(express.static(distPath));
    app.get('*', (_req, res) => {
      res.sendFile(path.join(distPath, 'index.html'));
    });
  }

  const PORT = 3000;
  app.listen(PORT, '0.0.0.0', () => {
    console.log(`Eathon Native Compiler & Toolchain Server listening on http://0.0.0.0:${PORT}`);
  });
}

startServer();
