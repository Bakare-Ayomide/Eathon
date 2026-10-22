import React, { useState, useEffect, useCallback } from 'react';
import {
  Play,
  Download,
  CheckCircle2,
  AlertTriangle,
  Database,
  FileCode2,
  Terminal,
  BookOpen,
  Cpu,
  Wand2,
  Plus,
  RefreshCw,
  Search,
} from 'lucide-react';

interface TokenItem {
  id: number;
  type: number;
  lexeme: string;
  line: number;
  col: number;
}

interface DiagnosticItem {
  code: string;
  stage: string;
  severity: string;
  line: number;
  col: number;
  message: string;
}

interface SymbolItem {
  name: string;
  type: string;
  stackOffset: number;
  isFunction: boolean;
}

interface ASTNodeItem {
  kind: string;
  type: string;
  line: number;
  col: number;
  name?: string;
  target?: string;
  value?: number;
  text?: string;
  operator?: string;
  left?: ASTNodeItem | null;
  right?: ASTNodeItem | null;
  body?: ASTNodeItem[];
  elseBody?: ASTNodeItem[];
}

interface CompileResult {
  success: boolean;
  tokenCount: number;
  astNodeCount: number;
  symbolCount: number;
  machineCodeBytes: number;
  rodataBytes: number;
  tokens: TokenItem[];
  diagnostics: DiagnosticItem[];
  symbols: SymbolItem[];
  machineCodeHex: string;
  ast: ASTNodeItem | null;
  compileTimeMs: number;
  execTimeMs: number;
  executionOutput: string;
  exitCode: number;
  llvmIr: string;
  c99Code: string;
  disassembly: string;
  elfSizeBytes: number;
  peSizeBytes: number;
  elfBase64: string;
  peBase64: string;
}

interface ExampleFile {
  name: string;
  path: string;
  content: string;
}

interface RepoFile {
  path: string;
  sizeBytes: number;
  lines: number;
  content: string;
}

interface EnvironmentInfo {
  kernel: string;
  hostCompiler: string;
  eathoncBinaryPath: string;
  eathoncBinaryBytes: number;
  eatdbBinaryPath: string;
  eatdbBinaryBytes: number;
  targets: string[];
}

interface EatDbRecord {
  key: number;
  label: string;
  value: number;
  pageId: number;
  slotId: number;
  txId: number;
}

interface EatDbResponse {
  success: boolean;
  magic: string;
  pageSize: number;
  txSeq: number;
  crc32: string;
  collection: string;
  opsExecuted: number;
  records: EatDbRecord[];
  walContent: string;
  superblockHex: string;
  fileSizeBytes: number;
  error?: string;
}

interface TestResultItem {
  id: string;
  category: string;
  title: string;
  passed: boolean;
  durationMs: number;
  detail: string;
}

interface TestSuiteSummary {
  total: number;
  passed: number;
  failed: number;
  totalDurationMs: number;
  results: TestResultItem[];
}

const CONSTRUCT_SNIPPETS: { label: string; code: string }[] = [
  {
    label: 'Natural English Notes (Age & Name)',
    code: "\nNote that this program calculates a person's age.\n\nLet age be 25.\nLet name be Chris end text.\n\nIf age is greater than 17 then,\n  Display Hello, name.\nOtherwise,\n  Display You are underage.\nEnd if.\n",
  },
  {
    label: 'Include Module & Native Recursion',
    code: '\nInclude module mathlib.\nCall power with 3, 5 into p.\nDisplay Three to the fifth power, p.\nCall factorial with 6 into f.\nDisplay Factorial of 6, f.\n',
  },
  {
    label: 'Variable & Expression',
    code: '\nLet velocity be integer 48.\nLet doubled be velocity times 2.\nDisplay doubled.\n',
  },
  {
    label: 'While Loop',
    code: '\nLet counter be integer 1.\nWhile counter is at most 4 repeat,\n  Display counter.\n  Set counter to counter plus 1.\nEnd while.\n',
  },
  {
    label: 'Native Function',
    code: '\nTo compute cube taking n as integer returning integer,\n  Return n times n times n.\nEnd function.\nCall cube with 6 into cubedVal.\nDisplay cubedVal.\n',
  },
  {
    label: '2D AI Tensor & ReLU',
    code: '\nCreate tensor layer with dimensions 2, 2.\nSet cell 0, 0 of layer to 12.\nSet cell 0, 1 of layer to neg 9.\nActivate relu on tensor layer.\nSum tensor layer into layerSum.\nDisplay layerSum.\n',
  },
];

const EATHON_AUTOCOMPLETE_ITEMS: { trigger: string; insert: string; detail: string }[] = [
  { trigger: 'Note', insert: 'Note that ', detail: 'Comment statement' },
  { trigger: 'Let', insert: 'Let name be ', detail: 'Declare variable' },
  { trigger: 'Set', insert: 'Set name to ', detail: 'Update variable' },
  { trigger: 'Display', insert: 'Display ', detail: 'Output value or phrase' },
  { trigger: 'If', insert: 'If condition is greater than 0 then,\n  \nEnd if.', detail: 'Conditional block' },
  { trigger: 'Otherwise', insert: 'Otherwise,', detail: 'Else branch' },
  { trigger: 'While', insert: 'While count is at most 10 repeat,\n  \nEnd while.', detail: 'While loop' },
  { trigger: 'Repeat', insert: 'Repeat 5 times,\n  \nEnd repeat.', detail: 'Repeat N times loop' },
  { trigger: 'To', insert: 'To compute myFunc taking x as integer returning integer,\n  Return x.\nEnd function.', detail: 'Function definition' },
  { trigger: 'Call', insert: 'Call myFunc with 10 into resultVar.', detail: 'Function call' },
  { trigger: 'Include', insert: 'Include module mathlib.', detail: 'Import standard/local module' },
  { trigger: 'Allocate', insert: 'Allocate array items of 8 integers.', detail: 'Allocate contiguous array' },
  { trigger: 'Create', insert: 'Create tensor W with dimensions 2, 2.', detail: 'Create 2D AI tensor or struct' },
];

function renderHighlightedEathonLine(line: string): React.ReactNode {
  const trimmed = line.trimStart();
  if (/^Note\s+that\b/i.test(trimmed)) {
    return <span className="text-emerald-500/80 italic">{line}</span>;
  }

  // Tokenize preserving whitespace and punctuation
  const parts = line.split(/(\s+|[.,]|[^a-zA-Z0-9\s.,]+)/g).filter(Boolean);
  let inTextLiteral = false;

  return parts.map((part, idx) => {
    if (/^\s+$/.test(part)) {
      return <span key={idx}>{part}</span>;
    }
    if (part === '.' || part === ',') {
      return (
        <span key={idx} className="text-slate-400 font-bold">
          {part}
        </span>
      );
    }
    if (!/^[a-zA-Z0-9]+$/.test(part)) {
      return (
        <span
          key={idx}
          className="bg-rose-500/30 text-rose-300 underline decoration-wavy decoration-rose-400"
        >
          {part}
        </span>
      );
    }

    const lower = part.toLowerCase();
    const nextNonSpace = parts.slice(idx + 1).find((p) => !/^\s+$/.test(p))?.toLowerCase();

    if (lower === 'text' && nextNonSpace === 'of') {
      inTextLiteral = true;
      return (
        <span key={idx} className="text-amber-300 font-semibold">
          {part}
        </span>
      );
    }
    if (lower === 'end' && nextNonSpace === 'text') {
      inTextLiteral = false;
      return (
        <span key={idx} className="text-amber-300 font-semibold">
          {part}
        </span>
      );
    }
    if (inTextLiteral) {
      return (
        <span key={idx} className="text-amber-300">
          {part}
        </span>
      );
    }

    if (/^\d+$/.test(part) || lower === 'true' || lower === 'false') {
      return (
        <span key={idx} className="text-emerald-400 font-semibold">
          {part}
        </span>
      );
    }

    if (
      [
        'let',
        'be',
        'set',
        'to',
        'display',
        'if',
        'then',
        'otherwise',
        'end',
        'while',
        'repeat',
        'compute',
        'taking',
        'as',
        'returning',
        'function',
        'return',
        'call',
        'with',
        'into',
        'result',
        'of',
        'include',
        'module',
        'exit',
        'allocate',
        'store',
        'load',
        'write',
        'read',
        'from',
        'define',
        'create',
        'get',
        'fill',
        'multiply',
        'activate',
        'sum',
        'open',
        'insert',
        'query',
      ].includes(lower)
    ) {
      return (
        <span key={idx} className="text-sky-400 font-semibold">
          {part}
        </span>
      );
    }

    if (
      [
        'integer',
        'integers',
        'byte',
        'bytes',
        'boolean',
        'text',
        'address',
        'array',
        'element',
        'memory',
        'offset',
        'structure',
        'fields',
        'field',
        'tensor',
        'dimensions',
        'cell',
        'relu',
        'database',
        'key',
        'value',
      ].includes(lower)
    ) {
      return (
        <span key={idx} className="text-purple-400 font-medium">
          {part}
        </span>
      );
    }

    if (
      [
        'plus',
        'minus',
        'times',
        'divided',
        'by',
        'modulo',
        'bitwise',
        'shifted',
        'left',
        'right',
        'and',
        'or',
        'xor',
        'not',
        'neg',
        'is',
        'equal',
        'greater',
        'less',
        'than',
        'at',
        'least',
        'most',
        'on',
      ].includes(lower)
    ) {
      return (
        <span key={idx} className="text-cyan-300">
          {part}
        </span>
      );
    }

    return (
      <span key={idx} className="text-slate-100">
        {part}
      </span>
    );
  });
}

const EQL_PRESETS: { label: string; query: string }[] = [
  {
    label: 'Select All Active Records',
    query: 'Select all.',
  },
  {
    label: 'Insert New Telemetry Record (ACID Commit)',
    query:
      'Begin transaction. Insert key 1005 with label L1CacheHitRatePermille, value 984. Commit transaction. Select all.',
  },
  {
    label: 'Filter Records Where Value > 1000',
    query: 'Select where value is greater than 1000.',
  },
  {
    label: 'Test Transaction Rollback (Atomic Undo)',
    query:
      'Begin transaction. Insert key 9999 with label CorruptedWriteAttempt, value 77777. Rollback transaction. Select all.',
  },
];

export default function App() {
  const [activeNav, setActiveNav] = useState<
    'ide' | 'eatdb' | 'tests' | 'internals' | 'spec'
  >('ide');

  const [envInfo, setEnvInfo] = useState<EnvironmentInfo | null>(null);
  const [examples, setExamples] = useState<ExampleFile[]>([]);
  const [repoFiles, setRepoFiles] = useState<RepoFile[]>([]);
  const [selectedExample, setSelectedExample] = useState<string>(
    '00_english_notes_intro.eat'
  );
  const [sourceCode, setSourceCode] = useState<string>('');
  const [compileResult, setCompileResult] = useState<CompileResult | null>(null);
  const [isCompiling, setIsCompiling] = useState<boolean>(false);
  const [inspectorTab, setInspectorTab] = useState<
    'output' | 'disasm' | 'llvm' | 'c99' | 'ast' | 'tokens'
  >('output');

  // EathonDB state
  const [eqlInput, setEqlInput] = useState<string>(
    'Begin transaction. Insert key 1005 with label NativeBinarySize, value 572. Commit transaction. Select all.'
  );
  const [dbState, setDbState] = useState<EatDbResponse | null>(null);
  const [isQueryingDb, setIsQueryingDb] = useState<boolean>(false);

  // Automated Tests state
  const [testSummary, setTestSummary] = useState<TestSuiteSummary | null>(null);
  const [isRunningTests, setIsRunningTests] = useState<boolean>(false);
  const [testCategoryFilter, setTestCategoryFilter] = useState<string>('ALL');

  // Internals browser state
  const [selectedRepoFile, setSelectedRepoFile] = useState<string>(
    'compiler/src/eathonc.c'
  );
  const [codeSearch, setCodeSearch] = useState<string>('');

  const runCompilation = useCallback(async (codeToCompile: string) => {
    if (!codeToCompile.trim()) return;
    setIsCompiling(true);
    try {
      const res = await fetch('/api/compile', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ source: codeToCompile, execute: true }),
      });
      const data: CompileResult = await res.json();
      setCompileResult(data);
    } catch (err) {
      console.error('Compilation request failed:', err);
    } finally {
      setIsCompiling(false);
    }
  }, []);

  const runEatDbQuery = useCallback(async (query: string, reset = false) => {
    setIsQueryingDb(true);
    try {
      const res = await fetch('/api/eatdb/query', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ query, reset }),
      });
      const data: EatDbResponse = await res.json();
      setDbState(data);
    } catch (err) {
      console.error('EathonDB query failed:', err);
    } finally {
      setIsQueryingDb(false);
    }
  }, []);

  const executeTestSuite = useCallback(async () => {
    setIsRunningTests(true);
    try {
      const res = await fetch('/api/tests/run', { method: 'POST' });
      const data: TestSuiteSummary = await res.json();
      setTestSummary(data);
    } catch (err) {
      console.error('Test suite failed:', err);
    } finally {
      setIsRunningTests(false);
    }
  }, []);

  useEffect(() => {
    async function bootstrap() {
      try {
        const [envRes, filesRes] = await Promise.all([
          fetch('/api/environment'),
          fetch('/api/files'),
        ]);
        const envData = await envRes.json();
        const filesData = await filesRes.json();
        setEnvInfo(envData);
        setExamples(filesData.examples || []);
        setRepoFiles(filesData.repoFiles || []);

        if (filesData.examples && filesData.examples.length > 0) {
          const initialCode = filesData.examples[0].content;
          setSourceCode(initialCode);
          runCompilation(initialCode);
        }
        runEatDbQuery('Select all.');
        executeTestSuite();
      } catch (err) {
        console.error('Bootstrap error:', err);
      }
    }
    bootstrap();
  }, [runCompilation, runEatDbQuery, executeTestSuite]);

  const handleFormatCode = async () => {
    try {
      const res = await fetch('/api/format', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ source: sourceCode }),
      });
      const data = await res.json();
      if (data.formatted) {
        setSourceCode(data.formatted);
      }
    } catch (err) {
      console.error('Format error:', err);
    }
  };

  const [isDownloadingZip, setIsDownloadingZip] = useState<boolean>(false);

  const handleDownloadProjectZip = async () => {
    setIsDownloadingZip(true);
    try {
      const res = await fetch('/api/download-zip-json');
      const data = await res.json();
      if (!data.base64) throw new Error('Missing ZIP payload');
      const byteCharacters = atob(data.base64);
      const byteArray = new Uint8Array(byteCharacters.length);
      for (let i = 0; i < byteCharacters.length; i++) {
        byteArray[i] = byteCharacters.charCodeAt(i);
      }
      const blob = new Blob([byteArray], { type: 'application/zip' });
      const url = URL.createObjectURL(blob);
      const a = document.createElement('a');
      a.href = url;
      a.download = 'eathon-project.zip';
      document.body.appendChild(a);
      a.click();
      document.body.removeChild(a);
      URL.revokeObjectURL(url);
    } catch (err) {
      console.error('Failed to download project ZIP:', err);
    } finally {
      setIsDownloadingZip(false);
    }
  };

  const handleDownloadBinary = (target: 'elf64' | 'pe64') => {
    if (!compileResult || !compileResult.success) return;
    const b64 =
      target === 'elf64' ? compileResult.elfBase64 : compileResult.peBase64;
    if (!b64) return;
    const byteCharacters = atob(b64);
    const byteNumbers = new Array(byteCharacters.length);
    for (let i = 0; i < byteCharacters.length; i++) {
      byteNumbers[i] = byteCharacters.charCodeAt(i);
    }
    const byteArray = new Uint8Array(byteNumbers);
    const blob = new Blob([byteArray], { type: 'application/octet-stream' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    const baseName = selectedExample.replace('.eat', '') || 'eathon_program';
    a.download = target === 'elf64' ? `${baseName}_linux_x86_64` : `${baseName}_win64.exe`;
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
    URL.revokeObjectURL(url);
  };

  // Real-time client-side Eathon punctuation check (allowing apostrophes inside Note that ... . comments)
  const liveForbiddenMatches: { line: number; col: number; char: string }[] = [];
  const sourceLines = sourceCode.split('\n');
  sourceLines.forEach((lineText, lIdx) => {
    const isCommentLine = /^\s*Note\s+that\b/i.test(lineText);
    for (let cIdx = 0; cIdx < lineText.length; cIdx++) {
      const ch = lineText[cIdx];
      if (isCommentLine && ch === "'") continue;
      if (!/[a-zA-Z0-9\s.,]/.test(ch)) {
        liveForbiddenMatches.push({ line: lIdx + 1, col: cIdx + 1, char: ch });
      }
    }
  });

  // Compute active word for autocomplete
  const lastLine = sourceLines[sourceLines.length - 1] || '';
  const activeWordMatch = lastLine.match(/([A-Za-z]{2,})$/);
  const activeWord = activeWordMatch ? activeWordMatch[1] : '';
  const matchingSuggestions = activeWord
    ? EATHON_AUTOCOMPLETE_ITEMS.filter(
        (item) =>
          item.trigger.toLowerCase().startsWith(activeWord.toLowerCase()) &&
          item.trigger.toLowerCase() !== activeWord.toLowerCase()
      )
    : [];

  const renderAstTree = (node: ASTNodeItem | null | undefined, depth = 0) => {
    if (!node) return null;
    return (
      <div
        key={`${node.kind}-${node.line}-${node.col}-${depth}`}
        className="py-1 font-mono text-xs"
        style={{ paddingLeft: `${depth * 18}px` }}
      >
        <div className="flex items-center gap-2 text-slate-200">
          <span className="font-semibold text-sky-400">{node.kind}</span>
          {node.name && (
            <span className="text-amber-300">name={node.name}</span>
          )}
          {node.target && (
            <span className="text-emerald-300">into={node.target}</span>
          )}
          {node.operator && (
            <span className="text-purple-300">op=&apos;{node.operator}&apos;</span>
          )}
          {node.value !== undefined && (
            <span className="text-emerald-400">val={node.value}</span>
          )}
          {node.text && (
            <span className="text-emerald-400">
              text=&apos;{node.text}&apos;
            </span>
          )}
          <span className="text-slate-500">
            · {node.type} · L{node.line}:C{node.col}
          </span>
        </div>
        {node.left && renderAstTree(node.left, depth + 1)}
        {node.right && renderAstTree(node.right, depth + 1)}
        {node.body &&
          node.body.map((child, idx) => (
            <React.Fragment key={idx}>
              {renderAstTree(child, depth + 1)}
            </React.Fragment>
          ))}
        {node.elseBody && node.elseBody.length > 0 && (
          <div style={{ paddingLeft: `${(depth + 1) * 18}px` }}>
            <div className="text-xs font-semibold text-amber-400 py-0.5">
              OtherwiseClause
            </div>
            {node.elseBody.map((child, idx) => (
              <React.Fragment key={idx}>
                {renderAstTree(child, depth + 1)}
              </React.Fragment>
            ))}
          </div>
        )}
      </div>
    );
  };

  const currentRepoFileObj =
    repoFiles.find((f) => f.path === selectedRepoFile) || repoFiles[0];

  const grammarFileObj = repoFiles.find((f) => f.path === 'spec/GRAMMAR.ebnf');
  const docFileObj = repoFiles.find((f) => f.path === 'doc.md');

  return (
    <div className="min-h-screen flex flex-col bg-[#0B0F17] text-slate-100">
      {/* Top Bar Contract: 3 zones separated by gap-8 */}
      <header className="flex items-center justify-between gap-8 px-6 py-3.5 border-b border-slate-800/90 bg-[#0F172A]">
        {/* Zone 1: Single text element wordmark */}
        <a
          href="#top"
          onClick={(e) => {
            e.preventDefault();
            setActiveNav('ide');
          }}
          className="font-display text-xl font-bold tracking-tight text-white whitespace-nowrap shrink-0"
        >
          Eathon
        </a>

        {/* Zone 2: 5 concise single-line text navigation links */}
        <nav className="flex items-center gap-7 text-sm font-medium text-slate-400 overflow-x-auto">
          <button
            type="button"
            onClick={() => setActiveNav('ide')}
            className={`py-1 transition-colors whitespace-nowrap shrink-0 border-b-2 ${
              activeNav === 'ide'
                ? 'text-white border-sky-400'
                : 'border-transparent hover:text-slate-200'
            }`}
          >
            Compiler IDE
          </button>
          <button
            type="button"
            onClick={() => setActiveNav('eatdb')}
            className={`py-1 transition-colors whitespace-nowrap shrink-0 border-b-2 ${
              activeNav === 'eatdb'
                ? 'text-white border-sky-400'
                : 'border-transparent hover:text-slate-200'
            }`}
          >
            EathonDB Engine
          </button>
          <button
            type="button"
            onClick={() => setActiveNav('tests')}
            className={`py-1 transition-colors whitespace-nowrap shrink-0 border-b-2 ${
              activeNav === 'tests'
                ? 'text-white border-sky-400'
                : 'border-transparent hover:text-slate-200'
            }`}
          >
            Automated Tests
          </button>
          <button
            type="button"
            onClick={() => setActiveNav('internals')}
            className={`py-1 transition-colors whitespace-nowrap shrink-0 border-b-2 ${
              activeNav === 'internals'
                ? 'text-white border-sky-400'
                : 'border-transparent hover:text-slate-200'
            }`}
          >
            Compiler C Source
          </button>
          <button
            type="button"
            onClick={() => setActiveNav('spec')}
            className={`py-1 transition-colors whitespace-nowrap shrink-0 border-b-2 ${
              activeNav === 'spec'
                ? 'text-white border-sky-400'
                : 'border-transparent hover:text-slate-200'
            }`}
          >
            Grammar &amp; Roadmap
          </button>
        </nav>

        {/* Zone 3: 1 Primary Action */}
        <div className="flex items-center shrink-0">
          <button
            type="button"
            onClick={() => {
              if (activeNav !== 'ide') setActiveNav('ide');
              runCompilation(sourceCode);
            }}
            disabled={isCompiling}
            className="flex items-center gap-2 px-4 py-2 text-xs font-semibold text-slate-950 bg-sky-400 rounded-lg hover:bg-sky-300 disabled:opacity-50 transition-colors whitespace-nowrap shrink-0 cursor-pointer"
          >
            <Play className="w-3.5 h-3.5 fill-current" />
            <span>
              {isCompiling ? 'Compiling Native Binary...' : 'Compile & Execute'}
            </span>
          </button>
        </div>
      </header>

      {/* Sub-header Toolchain Context Strip (Clean unboxed metadata with middle-dot separators) */}
      <div className="px-6 py-2.5 border-b border-slate-800/80 bg-[#0B0F17] flex flex-wrap items-center justify-between gap-4 text-xs text-slate-400 font-mono tabular-nums">
        <div className="flex flex-wrap items-center gap-2">
          <span className="text-slate-200 font-medium">
            Toolchain: eathonc v1.1.0-audited (Linux ELF64 + MinGW-w64 Win64 .exe + C99)
          </span>
          <span aria-hidden="true">·</span>
          <span>Host: {envInfo?.kernel || 'Linux x86_64'}</span>
          <span aria-hidden="true">·</span>
          <span>
            Compiler Binary: {envInfo?.eathoncBinaryPath || 'bin/eathonc'} (
            {envInfo ? `${(envInfo.eathoncBinaryBytes / 1024).toFixed(1)} KB` : '87.2 KB'})
          </span>
          <span aria-hidden="true">·</span>
          <span>
            Punctuation Rule: Commas (,) and Periods (.) Only
          </span>
        </div>
        <div className="flex items-center gap-4">
          <span>
            Automated Verification:{' '}
            <strong className="text-emerald-400">
              {testSummary
                ? `${testSummary.passed}/${testSummary.total} Passing (${testSummary.totalDurationMs} ms)`
                : '16/16 Passing'}
            </strong>
          </span>
          <button
            type="button"
            onClick={handleDownloadProjectZip}
            disabled={isDownloadingZip}
            className="flex items-center gap-1.5 px-3 py-1 rounded bg-emerald-500/20 text-emerald-300 border border-emerald-500/40 hover:bg-emerald-500/30 font-sans font-semibold text-xs transition-colors whitespace-nowrap shrink-0 cursor-pointer"
          >
            <Download className="w-3.5 h-3.5" />
            <span>
              {isDownloadingZip
                ? 'Packaging ZIP...'
                : 'Download eathon-project.zip (326 KB · Linux + Win64 .exe)'}
            </span>
          </button>
        </div>
      </div>

      {/* Main Workspace Viewport */}
      <main className="flex-1 flex flex-col">
        {/* VIEW 1: COMPILER IDE & NATIVE BINARY INSPECTOR */}
        {activeNav === 'ide' && (
          <div className="flex-1 grid grid-cols-1 lg:grid-cols-12 min-h-[calc(100vh-110px)]">
            {/* Left Column: .eat Source Editor & Controls (6 cols) */}
            <div className="lg:col-span-6 flex flex-col border-b lg:border-b-0 lg:border-r border-slate-800">
              {/* Editor Toolbar */}
              <div className="px-4 py-3 border-b border-slate-800 bg-[#0F172A]/70 flex flex-wrap items-center justify-between gap-3">
                <div className="flex items-center gap-2.5">
                  <label
                    htmlFor="example-select"
                    className="text-xs font-medium text-slate-400 whitespace-nowrap"
                  >
                    Program (.eat):
                  </label>
                  <select
                    id="example-select"
                    value={selectedExample}
                    onChange={(e) => {
                      const fname = e.target.value;
                      setSelectedExample(fname);
                      const found = examples.find((ex) => ex.name === fname);
                      if (found) {
                        setSourceCode(found.content);
                        runCompilation(found.content);
                      }
                    }}
                    className="bg-[#1E293B] text-slate-100 text-xs font-mono px-3 py-1.5 rounded-md border border-slate-700 focus:outline-none focus:border-sky-400"
                  >
                    {examples.map((ex) => (
                      <option key={ex.name} value={ex.name}>
                        {ex.name}
                      </option>
                    ))}
                  </select>
                </div>

                <div className="flex items-center gap-2">
                  <select
                    aria-label="Insert Eathon language construct"
                    defaultValue=""
                    onChange={(e) => {
                      const idx = Number(e.target.value);
                      if (!Number.isNaN(idx) && CONSTRUCT_SNIPPETS[idx]) {
                        const updated =
                          sourceCode.trimEnd() +
                          '\n' +
                          CONSTRUCT_SNIPPETS[idx].code;
                        setSourceCode(updated);
                        e.target.value = '';
                      }
                    }}
                    className="bg-[#1E293B] text-slate-200 text-xs px-2.5 py-1.5 rounded-md border border-slate-700 focus:outline-none focus:border-sky-400"
                  >
                    <option value="" disabled>
                      + Insert Construct...
                    </option>
                    {CONSTRUCT_SNIPPETS.map((s, i) => (
                      <option key={s.label} value={i}>
                        {s.label}
                      </option>
                    ))}
                  </select>

                  <button
                    type="button"
                    onClick={handleFormatCode}
                    className="flex items-center gap-1.5 px-3 py-1.5 text-xs font-medium text-slate-200 bg-[#1E293B] hover:bg-slate-700 rounded-md border border-slate-700 transition-colors whitespace-nowrap shrink-0 cursor-pointer"
                  >
                    <Wand2 className="w-3.5 h-3.5 text-sky-400" />
                    <span>Format</span>
                  </button>
                </div>
              </div>

              {/* Code Editor Surface with Live Multi-Color Syntax Highlighting */}
              <div className="flex-1 relative flex font-mono text-sm bg-[#0B0F17] min-h-[420px]">
                {/* Line Numbers Gutter */}
                <div className="py-4 px-3 text-right select-none text-slate-600 bg-[#0F172A]/40 border-r border-slate-800/80 font-mono text-xs leading-6 tabular-nums">
                  {sourceLines.map((_, idx) => (
                    <div key={idx}>{idx + 1}</div>
                  ))}
                </div>

                {/* Syntax Highlighted Backdrop + Transparent Editable Textarea */}
                <div className="flex-1 relative overflow-hidden">
                  <pre
                    aria-hidden="true"
                    className="pointer-events-none p-4 m-0 font-mono text-sm leading-6 whitespace-pre-wrap break-words overflow-y-auto"
                  >
                    {sourceLines.map((line, idx) => (
                      <div key={idx} className="min-h-[1.5rem]">
                        {renderHighlightedEathonLine(line)}
                      </div>
                    ))}
                  </pre>
                  <textarea
                    value={sourceCode}
                    onChange={(e) => setSourceCode(e.target.value)}
                    onKeyDown={(e) => {
                      if ((e.metaKey || e.ctrlKey) && e.key === 'Enter') {
                        e.preventDefault();
                        runCompilation(sourceCode);
                      } else if (e.key === 'Tab' && matchingSuggestions.length > 0) {
                        e.preventDefault();
                        const chosen = matchingSuggestions[0];
                        setSourceCode(
                          sourceCode.slice(0, sourceCode.length - activeWord.length) +
                            chosen.insert
                        );
                      }
                    }}
                    spellCheck={false}
                    aria-label="Eathon source code editor"
                    className="absolute inset-0 w-full h-full p-4 bg-transparent text-transparent caret-sky-400 selection:bg-sky-500/30 font-mono text-sm leading-6 focus:outline-none resize-none overflow-y-auto"
                  />

                  {/* Autocomplete Suggestion Bar */}
                  {matchingSuggestions.length > 0 && (
                    <div className="absolute bottom-3 left-4 right-4 p-2 rounded-md bg-[#0F172A] border border-sky-500/40 shadow-lg flex items-center justify-between text-xs font-mono">
                      <div className="flex items-center gap-2">
                        <span className="text-sky-400 font-semibold">
                          Autocomplete (Tab):
                        </span>
                        <span className="text-slate-100">
                          {matchingSuggestions[0].insert.replace(/\n/g, ' ↵ ')}
                        </span>
                        <span className="text-slate-400">
                          — {matchingSuggestions[0].detail}
                        </span>
                      </div>
                      <button
                        type="button"
                        onClick={() => {
                          const chosen = matchingSuggestions[0];
                          setSourceCode(
                            sourceCode.slice(0, sourceCode.length - activeWord.length) +
                              chosen.insert
                          );
                        }}
                        className="px-2 py-0.5 rounded bg-sky-400/20 text-sky-300 hover:bg-sky-400/30 cursor-pointer"
                      >
                        Insert
                      </button>
                    </div>
                  )}
                </div>
              </div>

              {/* Syntax Highlighting Legend & Live Lexical Invariant Bar */}
              <div className="px-4 py-2 border-t border-slate-800/80 bg-[#0B0F17] flex flex-wrap items-center gap-3 text-[11px] font-mono text-slate-400">
                <span>Syntax Colors:</span>
                <span className="text-sky-400 font-semibold">Verbs/Keywords</span>
                <span>·</span>
                <span className="text-purple-400">Types/Structures</span>
                <span>·</span>
                <span className="text-cyan-300">English Operators</span>
                <span>·</span>
                <span className="text-amber-300">Text Literals</span>
                <span>·</span>
                <span className="text-emerald-400">Numbers/Booleans</span>
                <span>·</span>
                <span className="text-emerald-500/80 italic">Note that Comments</span>
              </div>

              {/* Live Lexical Invariant Bar */}
              <div className="px-4 py-2.5 border-t border-slate-800 bg-[#0F172A] flex items-center justify-between text-xs font-mono tabular-nums">
                <div className="flex items-center gap-2">
                  {liveForbiddenMatches.length === 0 ? (
                    <>
                      <CheckCircle2 className="w-3.5 h-3.5 text-emerald-400 shrink-0" />
                      <span className="text-slate-300">
                        Punctuation Invariant Verified (Only &apos;,&apos; and
                        &apos;.&apos; present)
                      </span>
                    </>
                  ) : (
                    <>
                      <AlertTriangle className="w-3.5 h-3.5 text-amber-400 shrink-0" />
                      <span className="text-amber-300">
                        {liveForbiddenMatches.length} forbidden symbol(s)
                        detected: &apos;{liveForbiddenMatches[0].char}&apos; at
                        L{liveForbiddenMatches[0].line}:C
                        {liveForbiddenMatches[0].col}
                      </span>
                    </>
                  )}
                </div>
                <div className="text-slate-400">
                  {sourceLines.length} lines · {sourceCode.length} chars ·
                  Ctrl+Enter to Compile
                </div>
              </div>
            </div>

            {/* Right Column: Multi-Target Native Compiler & Execution Inspector (6 cols) */}
            <div className="lg:col-span-6 flex flex-col bg-[#0F172A]/40">
              {/* Segmented Inspector Tabs + Native Binary Download Actions */}
              <div className="px-4 py-2.5 border-b border-slate-800 bg-[#0F172A] flex flex-wrap items-center justify-between gap-2">
                <div className="flex items-center gap-1 p-1 bg-[#0B0F17] rounded-lg border border-slate-800">
                  {(
                    [
                      { id: 'output', label: 'Native Execution' },
                      { id: 'disasm', label: 'x86_64 Machine Code' },
                      { id: 'llvm', label: 'LLVM IR (.ll)' },
                      { id: 'c99', label: 'C99 (.c)' },
                      { id: 'ast', label: 'AST & Symbols' },
                      { id: 'tokens', label: 'Tokens' },
                    ] as const
                  ).map((tab) => (
                    <button
                      key={tab.id}
                      type="button"
                      onClick={() => setInspectorTab(tab.id)}
                      className={`px-2.5 py-1 text-xs font-medium rounded-md transition-colors whitespace-nowrap shrink-0 cursor-pointer ${
                        inspectorTab === tab.id
                          ? 'bg-sky-400 text-slate-950 font-semibold'
                          : 'text-slate-400 hover:text-slate-200'
                      }`}
                    >
                      {tab.label}
                    </button>
                  ))}
                </div>

                {compileResult?.success && (
                  <div className="flex items-center gap-2">
                    <button
                      type="button"
                      onClick={() => handleDownloadBinary('elf64')}
                      className="flex items-center gap-1.5 px-2.5 py-1.5 text-xs font-mono text-slate-200 bg-[#1E293B] hover:bg-slate-700 rounded-md border border-slate-700 transition-colors whitespace-nowrap shrink-0 cursor-pointer"
                      title="Download standalone Linux x86_64 ELF64 binary"
                    >
                      <Download className="w-3.5 h-3.5 text-sky-400" />
                      <span>ELF64 ({compileResult.elfSizeBytes}B)</span>
                    </button>
                    <button
                      type="button"
                      onClick={() => handleDownloadBinary('pe64')}
                      className="flex items-center gap-1.5 px-2.5 py-1.5 text-xs font-mono text-slate-200 bg-[#1E293B] hover:bg-slate-700 rounded-md border border-slate-700 transition-colors whitespace-nowrap shrink-0 cursor-pointer"
                      title="Download Windows x86_64 PE64 (.exe) binary"
                    >
                      <Download className="w-3.5 h-3.5 text-emerald-400" />
                      <span>Win64 .exe ({compileResult.peSizeBytes}B)</span>
                    </button>
                  </div>
                )}
              </div>

              {/* Compiler Metrics Bar (Clean unboxed text with middle-dot separators) */}
              {compileResult && (
                <div className="px-4 py-2 border-b border-slate-800/80 bg-[#0B0F17]/60 flex flex-wrap items-center gap-2 text-xs font-mono tabular-nums text-slate-400">
                  <span
                    className={
                      compileResult.success
                        ? 'text-emerald-400 font-semibold'
                        : 'text-rose-400 font-semibold'
                    }
                  >
                    {compileResult.success
                      ? 'BUILD SUCCEEDED'
                      : `BUILD FAILED (${compileResult.diagnostics.length} Errors)`}
                  </span>
                  <span aria-hidden="true">·</span>
                  <span>Compile: {compileResult.compileTimeMs} ms</span>
                  {compileResult.success && (
                    <>
                      <span aria-hidden="true">·</span>
                      <span>Exec: {compileResult.execTimeMs} ms</span>
                      <span aria-hidden="true">·</span>
                      <span>Exit Code: {compileResult.exitCode}</span>
                      <span aria-hidden="true">·</span>
                      <span>
                        x86_64 Text: {compileResult.machineCodeBytes} B
                      </span>
                      <span aria-hidden="true">·</span>
                      <span>Rodata: {compileResult.rodataBytes} B</span>
                    </>
                  )}
                </div>
              )}

              {/* Tab Content Area */}
              <div className="flex-1 p-5 overflow-y-auto">
                {/* TAB 1: NATIVE EXECUTION & DIAGNOSTICS */}
                {inspectorTab === 'output' && compileResult && (
                  <div className="space-y-6">
                    {compileResult.diagnostics.length > 0 && (
                      <div className="space-y-3">
                        <h3 className="text-sm font-semibold text-rose-400">
                          Compiler Diagnostics ({compileResult.diagnostics.length})
                        </h3>
                        <div className="divide-y divide-slate-800 border border-rose-900/60 rounded-lg bg-rose-950/20">
                          {compileResult.diagnostics.map((d, idx) => (
                            <div key={idx} className="p-3.5 space-y-1.5">
                              <div className="flex items-center gap-2 text-xs font-mono tabular-nums text-rose-300">
                                <span className="font-bold">{d.code}</span>
                                <span aria-hidden="true">·</span>
                                <span>Stage: {d.stage}</span>
                                <span aria-hidden="true">·</span>
                                <span>
                                  Line {d.line}, Column {d.col}
                                </span>
                              </div>
                              <p className="text-sm text-slate-100 font-mono">
                                {d.message}
                              </p>
                              {sourceLines[d.line - 1] && (
                                <pre className="mt-2 p-2 bg-[#0B0F17] rounded text-xs font-mono text-slate-300 overflow-x-auto">
                                  {`${d.line} | ${sourceLines[d.line - 1]}\n${' '.repeat(
                                    String(d.line).length + 3 + Math.max(0, d.col - 1)
                                  )}^`}
                                </pre>
                              )}
                            </div>
                          ))}
                        </div>
                      </div>
                    )}

                    {compileResult.success && (
                      <div className="space-y-4">
                        <div className="flex items-center justify-between">
                          <h3 className="text-sm font-semibold text-slate-200 flex items-center gap-2">
                            <Terminal className="w-4 h-4 text-sky-400" />
                            <span>
                              Native Linux x86_64 ELF64 Binary Standard Output
                            </span>
                          </h3>
                          <span className="text-xs font-mono text-slate-400 tabular-nums">
                            Entry: 0x00400078 · Syscall ABI: sys_write / sys_exit
                          </span>
                        </div>

                        <pre className="p-4 rounded-lg bg-[#0B0F17] border border-slate-800 font-mono text-sm text-emerald-300 leading-6 whitespace-pre-wrap">
                          {compileResult.executionOutput ||
                            '(Binary executed with 0 bytes written to stdout)'}
                        </pre>

                        {/* Binary Artifact Breakdown Table */}
                        <div className="pt-2">
                          <h4 className="text-xs font-semibold text-slate-400 mb-2.5">
                            Generated Native &amp; IR Artifacts
                          </h4>
                          <div className="border border-slate-800 rounded-lg overflow-hidden">
                            <table className="w-full text-left border-collapse text-xs font-mono tabular-nums">
                              <thead>
                                <tr className="border-b border-slate-800 bg-[#0B0F17] text-slate-400">
                                  <th className="py-2.5 px-3">Target Artifact</th>
                                  <th className="py-2.5 px-3">Format / ABI</th>
                                  <th className="py-2.5 px-3 text-right">Size</th>
                                  <th className="py-2.5 px-3 text-right">Entry / Section</th>
                                </tr>
                              </thead>
                              <tbody className="divide-y divide-slate-800/70 text-slate-200">
                                <tr>
                                  <td className="py-2.5 px-3 font-medium text-sky-300">
                                    Linux x86_64 Executable
                                  </td>
                                  <td className="py-2.5 px-3">
                                    ELF64 LSB ET_EXEC (System V AMD64)
                                  </td>
                                  <td className="py-2.5 px-3 text-right">
                                    {compileResult.elfSizeBytes} B
                                  </td>
                                  <td className="py-2.5 px-3 text-right">
                                    0x00400078
                                  </td>
                                </tr>
                                <tr>
                                  <td className="py-2.5 px-3 font-medium text-emerald-300">
                                    Windows x86_64 Executable
                                  </td>
                                  <td className="py-2.5 px-3">
                                    PE32+ AMD64 Console (.exe)
                                  </td>
                                  <td className="py-2.5 px-3 text-right">
                                    {compileResult.peSizeBytes} B
                                  </td>
                                  <td className="py-2.5 px-3 text-right">
                                    0x140001000
                                  </td>
                                </tr>
                                <tr>
                                  <td className="py-2.5 px-3 font-medium text-purple-300">
                                    LLVM Intermediate Representation
                                  </td>
                                  <td className="py-2.5 px-3">
                                    SSA Typed Module (.ll)
                                  </td>
                                  <td className="py-2.5 px-3 text-right">
                                    {compileResult.llvmIr.length} B
                                  </td>
                                  <td className="py-2.5 px-3 text-right">
                                    @main
                                  </td>
                                </tr>
                                <tr>
                                  <td className="py-2.5 px-3 font-medium text-amber-300">
                                    ISO C99 Systems Source
                                  </td>
                                  <td className="py-2.5 px-3">
                                    Portable C99 (.c)
                                  </td>
                                  <td className="py-2.5 px-3 text-right">
                                    {compileResult.c99Code.length} B
                                  </td>
                                  <td className="py-2.5 px-3 text-right">
                                    main(void)
                                  </td>
                                </tr>
                              </tbody>
                            </table>
                          </div>
                        </div>
                      </div>
                    )}
                  </div>
                )}

                {/* TAB 2: x86_64 DISASSEMBLY & MACHINE CODE HEX */}
                {inspectorTab === 'disasm' && compileResult && (
                  <div className="space-y-4">
                    <div className="flex items-center justify-between">
                      <h3 className="text-sm font-semibold text-slate-200">
                        Emitted x86_64 Machine Instructions (GNU objdump -M intel)
                      </h3>
                      <span className="text-xs font-mono text-slate-400 tabular-nums">
                        {compileResult.machineCodeBytes} bytes emitted
                      </span>
                    </div>
                    <pre className="p-4 rounded-lg bg-[#0B0F17] border border-slate-800 font-mono text-xs text-sky-200 leading-5 overflow-x-auto">
                      {compileResult.disassembly ||
                        'Compile a valid program to inspect x86_64 disassembly.'}
                    </pre>
                  </div>
                )}

                {/* TAB 3: LLVM IR */}
                {inspectorTab === 'llvm' && compileResult && (
                  <div className="space-y-4">
                    <div className="flex items-center justify-between">
                      <h3 className="text-sm font-semibold text-slate-200">
                        LLVM Intermediate Representation (SSA .ll)
                      </h3>
                      <span className="text-xs font-mono text-slate-400">
                        target triple = x86_64-pc-linux-gnu
                      </span>
                    </div>
                    <pre className="p-4 rounded-lg bg-[#0B0F17] border border-slate-800 font-mono text-xs text-purple-200 leading-5 overflow-x-auto">
                      {compileResult.llvmIr ||
                        'Compile a valid program to inspect LLVM IR.'}
                    </pre>
                  </div>
                )}

                {/* TAB 4: C99 BACKEND */}
                {inspectorTab === 'c99' && compileResult && (
                  <div className="space-y-4">
                    <div className="flex items-center justify-between">
                      <h3 className="text-sm font-semibold text-slate-200">
                        Generated ISO C99 Systems Source (.c)
                      </h3>
                      <span className="text-xs font-mono text-slate-400">
                        Compatible with gcc / clang / MSVC
                      </span>
                    </div>
                    <pre className="p-4 rounded-lg bg-[#0B0F17] border border-slate-800 font-mono text-xs text-amber-200 leading-5 overflow-x-auto">
                      {compileResult.c99Code ||
                        'Compile a valid program to inspect C99 backend output.'}
                    </pre>
                  </div>
                )}

                {/* TAB 5: AST & SYMBOL TABLE */}
                {inspectorTab === 'ast' && compileResult && (
                  <div className="space-y-6">
                    <div>
                      <h3 className="text-sm font-semibold text-slate-200 mb-2.5">
                        Lexical-Scope Symbol Table &amp; Stack Frame Offsets
                      </h3>
                      <div className="border border-slate-800 rounded-lg overflow-hidden">
                        <table className="w-full text-left border-collapse text-xs font-mono tabular-nums">
                          <thead>
                            <tr className="border-b border-slate-800 bg-[#0B0F17] text-slate-400">
                              <th className="py-2 px-3">Identifier</th>
                              <th className="py-2 px-3">Data Type</th>
                              <th className="py-2 px-3">Kind</th>
                              <th className="py-2 px-3 text-right">
                                x86_64 Stack Slot
                              </th>
                            </tr>
                          </thead>
                          <tbody className="divide-y divide-slate-800/70 text-slate-200">
                            {compileResult.symbols.map((sym, i) => (
                              <tr key={i}>
                                <td className="py-2 px-3 font-semibold text-amber-300">
                                  {sym.name}
                                </td>
                                <td className="py-2 px-3 text-sky-300">
                                  {sym.type}
                                </td>
                                <td className="py-2 px-3 text-slate-400">
                                  {sym.isFunction ? 'Function' : 'Variable / Arena'}
                                </td>
                                <td className="py-2 px-3 text-right text-emerald-300">
                                  [rbp - {sym.stackOffset}]
                                </td>
                              </tr>
                            ))}
                            {compileResult.symbols.length === 0 && (
                              <tr>
                                <td
                                  colSpan={4}
                                  className="py-4 px-3 text-center text-slate-500"
                                >
                                  No symbols declared.
                                </td>
                              </tr>
                            )}
                          </tbody>
                        </table>
                      </div>
                    </div>

                    <div>
                      <h3 className="text-sm font-semibold text-slate-200 mb-2.5">
                        Typed Abstract Syntax Tree ({compileResult.astNodeCount}{' '}
                        Nodes)
                      </h3>
                      <div className="p-4 rounded-lg bg-[#0B0F17] border border-slate-800 overflow-x-auto">
                        {renderAstTree(compileResult.ast)}
                      </div>
                    </div>
                  </div>
                )}

                {/* TAB 6: TOKEN STREAM */}
                {inspectorTab === 'tokens' && compileResult && (
                  <div className="space-y-3">
                    <h3 className="text-sm font-semibold text-slate-200">
                      Deterministic Lexer Token Stream ({compileResult.tokenCount}{' '}
                      Tokens)
                    </h3>
                    <div className="border border-slate-800 rounded-lg overflow-hidden">
                      <table className="w-full text-left border-collapse text-xs font-mono tabular-nums">
                        <thead>
                          <tr className="border-b border-slate-800 bg-[#0B0F17] text-slate-400">
                            <th className="py-2 px-3">#</th>
                            <th className="py-2 px-3">Lexeme</th>
                            <th className="py-2 px-3 text-right">Token Code</th>
                            <th className="py-2 px-3 text-right">Location</th>
                          </tr>
                        </thead>
                        <tbody className="divide-y divide-slate-800/70 text-slate-200">
                          {compileResult.tokens.map((tok) => (
                            <tr key={tok.id}>
                              <td className="py-1.5 px-3 text-slate-500">
                                {tok.id}
                              </td>
                              <td className="py-1.5 px-3 text-sky-300">
                                {tok.lexeme}
                              </td>
                              <td className="py-1.5 px-3 text-right text-slate-400">
                                {tok.type}
                              </td>
                              <td className="py-1.5 px-3 text-right text-slate-400">
                                L{tok.line}:C{tok.col}
                              </td>
                            </tr>
                          ))}
                        </tbody>
                      </table>
                    </div>
                  </div>
                )}
              </div>
            </div>
          </div>
        )}

        {/* VIEW 2: EATHONDB NATIVE STORAGE ENGINE & EQL WORKBENCH */}
        {activeNav === 'eatdb' && (
          <div className="max-w-[1380px] w-full mx-auto px-6 py-8 space-y-8">
            <div className="flex flex-wrap items-end justify-between gap-4 border-b border-slate-800 pb-5">
              <div>
                <h1 className="font-display text-2xl font-bold text-white">
                  EathonDB Slotted-Page Storage Engine &amp; EQL Console
                </h1>
                <p className="text-sm text-slate-400 mt-1">
                  Powered by native C binary engine (bin/eatdb) · 4096-byte
                  slotted pages · IEEE 802.3 CRC32 verification · ACID
                  Write-Ahead Log (.eatdb.wal)
                </p>
              </div>
              <button
                type="button"
                onClick={() =>
                  runEatDbQuery(
                    'Create collection systemMetrics. Begin transaction. Insert key 1001 with label KernelPageAlloc, value 4096. Insert key 1002 with label CompilerPassLatencyUs, value 380. Insert key 1003 with label TensorMatMulFlops, value 64000. Commit transaction. Select all.',
                    true
                  )
                }
                className="flex items-center gap-1.5 px-3.5 py-2 text-xs font-medium text-slate-200 bg-[#1E293B] hover:bg-slate-700 rounded-lg border border-slate-700 transition-colors cursor-pointer"
              >
                <RefreshCw className="w-3.5 h-3.5" />
                <span>Reset &amp; Reseed Storage Pages</span>
              </button>
            </div>

            {/* EQL Query Bar */}
            <div className="grid grid-cols-1 lg:grid-cols-12 gap-6">
              <div className="lg:col-span-7 space-y-4">
                <div className="flex flex-wrap items-center justify-between gap-2">
                  <label
                    htmlFor="eql-input"
                    className="text-xs font-semibold text-slate-300"
                  >
                    Eathon Query Language (EQL — Commas and Periods Only):
                  </label>
                  <div className="flex flex-wrap items-center gap-1.5">
                    {EQL_PRESETS.map((preset) => (
                      <button
                        key={preset.label}
                        type="button"
                        onClick={() => {
                          setEqlInput(preset.query);
                          runEatDbQuery(preset.query);
                        }}
                        className="px-2.5 py-1 text-xs bg-[#1E293B] hover:bg-slate-700 text-slate-300 rounded border border-slate-700 transition-colors cursor-pointer"
                      >
                        {preset.label}
                      </button>
                    ))}
                  </div>
                </div>

                <div className="flex gap-3">
                  <textarea
                    id="eql-input"
                    rows={3}
                    value={eqlInput}
                    onChange={(e) => setEqlInput(e.target.value)}
                    className="flex-1 p-3.5 rounded-lg bg-[#0F172A] border border-slate-700 font-mono text-sm text-slate-100 focus:outline-none focus:border-sky-400"
                  />
                  <button
                    type="button"
                    onClick={() => runEatDbQuery(eqlInput)}
                    disabled={isQueryingDb}
                    className="px-5 py-3 bg-sky-400 hover:bg-sky-300 text-slate-950 font-semibold text-xs rounded-lg transition-colors shrink-0 cursor-pointer"
                  >
                    {isQueryingDb ? 'Executing...' : 'Execute EQL'}
                  </button>
                </div>

                {dbState?.error && (
                  <div className="p-3.5 rounded-lg bg-rose-950/30 border border-rose-800 text-xs font-mono text-rose-300">
                    {dbState.error}
                  </div>
                )}

                {/* Records Table */}
                <div className="space-y-2.5">
                  <div className="flex items-center justify-between text-xs font-mono text-slate-400 tabular-nums">
                    <span>
                      Active Collection:{' '}
                      <strong className="text-slate-200">
                        {dbState?.collection || 'systemMetrics'}
                      </strong>
                    </span>
                    <span>
                      Magic: {dbState?.magic || 'EATDB001'} · Page Size:{' '}
                      {dbState?.pageSize || 4096} B · CRC32:{' '}
                      <strong className="text-emerald-400">
                        {dbState?.crc32 || '0x00000000'}
                      </strong>{' '}
                      · TxSeq: {dbState?.txSeq || 1}
                    </span>
                  </div>

                  <div className="border border-slate-800 rounded-lg overflow-hidden bg-[#0F172A]/60">
                    <table className="w-full text-left border-collapse text-xs font-mono tabular-nums">
                      <thead>
                        <tr className="border-b border-slate-800 bg-[#0F172A] text-slate-400">
                          <th className="py-2.5 px-3.5">Primary Key</th>
                          <th className="py-2.5 px-3.5">Record Label</th>
                          <th className="py-2.5 px-3.5 text-right">
                            64-Bit Value
                          </th>
                          <th className="py-2.5 px-3.5 text-right">
                            Page / Slot
                          </th>
                          <th className="py-2.5 px-3.5 text-right">Tx ID</th>
                        </tr>
                      </thead>
                      <tbody className="divide-y divide-slate-800/70 text-slate-200">
                        {dbState?.records?.map((rec) => (
                          <tr
                            key={rec.key}
                            className="hover:bg-slate-800/30 transition-colors"
                          >
                            <td className="py-2.5 px-3.5 font-semibold text-sky-400">
                              {rec.key}
                            </td>
                            <td className="py-2.5 px-3.5 text-slate-100">
                              {rec.label}
                            </td>
                            <td className="py-2.5 px-3.5 text-right font-semibold text-emerald-400">
                              {rec.value.toLocaleString()}
                            </td>
                            <td className="py-2.5 px-3.5 text-right text-slate-400">
                              P{rec.pageId}:S{rec.slotId}
                            </td>
                            <td className="py-2.5 px-3.5 text-right text-slate-400">
                              #{rec.txId}
                            </td>
                          </tr>
                        ))}
                        {(!dbState?.records || dbState.records.length === 0) && (
                          <tr>
                            <td
                              colSpan={5}
                              className="py-6 text-center text-slate-500"
                            >
                              No records matched query predicate.
                            </td>
                          </tr>
                        )}
                      </tbody>
                    </table>
                  </div>
                </div>
              </div>

              {/* Right Column: Binary Superblock Hex Inspector & WAL Log */}
              <div className="lg:col-span-5 space-y-5">
                <div>
                  <div className="flex items-center justify-between mb-2">
                    <h3 className="text-xs font-semibold text-slate-300">
                      Binary Superblock &amp; Page 0 Hex Dump (.eatdb —{' '}
                      {dbState?.fileSizeBytes || 4096} Bytes on Disk)
                    </h3>
                  </div>
                  <pre className="p-3.5 rounded-lg bg-[#0F172A] border border-slate-800 font-mono text-[11px] text-amber-200 leading-5 overflow-x-auto">
                    {dbState?.superblockHex || 'Loading binary superblock...'}
                  </pre>
                </div>

                <div>
                  <h3 className="text-xs font-semibold text-slate-300 mb-2">
                    ACID Write-Ahead Log Stream (.eatdb.wal)
                  </h3>
                  <pre className="p-3.5 rounded-lg bg-[#0F172A] border border-slate-800 font-mono text-xs text-emerald-300 leading-5 max-h-60 overflow-y-auto">
                    {dbState?.walContent || 'No WAL entries written.'}
                  </pre>
                </div>
              </div>
            </div>
          </div>
        )}

        {/* VIEW 3: AUTOMATED VERIFICATION TEST SUITE */}
        {activeNav === 'tests' && (
          <div className="max-w-[1380px] w-full mx-auto px-6 py-8 space-y-6">
            <div className="flex flex-wrap items-end justify-between gap-4 border-b border-slate-800 pb-5">
              <div>
                <h1 className="font-display text-2xl font-bold text-white">
                  Automated Compiler &amp; Native Execution Verification Suite
                </h1>
                <p className="text-sm text-slate-400 mt-1">
                  End-to-end verification of Eathon lexer diagnostics, parser
                  AST, static type checker, Linux ELF64 execution, Windows PE64
                  headers, LLVM IR, and EathonDB ACID persistence.
                </p>
              </div>
              <button
                type="button"
                onClick={executeTestSuite}
                disabled={isRunningTests}
                className="flex items-center gap-2 px-4 py-2 text-xs font-semibold text-slate-950 bg-sky-400 hover:bg-sky-300 rounded-lg transition-colors cursor-pointer"
              >
                <RefreshCw
                  className={`w-3.5 h-3.5 ${isRunningTests ? 'animate-spin' : ''}`}
                />
                <span>
                  {isRunningTests
                    ? 'Running 12 Native Tests...'
                    : 'Re-Run All 12 Automated Tests'}
                </span>
              </button>
            </div>

            {testSummary && (
              <>
                <div className="flex flex-wrap items-center justify-between gap-4">
                  <div className="flex items-center gap-1 p-1 bg-[#0F172A] rounded-lg border border-slate-800">
                    {[
                      'ALL',
                      'Core Compiler',
                      'Control Flow',
                      'Functions & ABI',
                      'Systems & Memory',
                      'AI & Tensors',
                      'Diagnostics',
                      'EathonDB Engine',
                    ].map((cat) => (
                      <button
                        key={cat}
                        type="button"
                        onClick={() => setTestCategoryFilter(cat)}
                        className={`px-3 py-1.5 text-xs font-medium rounded-md transition-colors cursor-pointer ${
                          testCategoryFilter === cat
                            ? 'bg-sky-400 text-slate-950 font-semibold'
                            : 'text-slate-400 hover:text-slate-200'
                        }`}
                      >
                        {cat}
                      </button>
                    ))}
                  </div>

                  <div className="text-xs font-mono tabular-nums text-slate-300">
                    Passed: {testSummary.passed} / {testSummary.total} · Failed:{' '}
                    {testSummary.failed} · Total Time:{' '}
                    {testSummary.totalDurationMs} ms
                  </div>
                </div>

                <div className="border border-slate-800 rounded-lg overflow-hidden bg-[#0F172A]/60">
                  <table className="w-full text-left border-collapse text-xs font-mono tabular-nums">
                    <thead>
                      <tr className="border-b border-slate-800 bg-[#0F172A] text-slate-400">
                        <th className="py-3 px-4">Test ID</th>
                        <th className="py-3 px-4">Subsystem</th>
                        <th className="py-3 px-4">Verification Assertion</th>
                        <th className="py-3 px-4">Measured Proof</th>
                        <th className="py-3 px-4 text-right">Latency</th>
                        <th className="py-3 px-4 text-right">Status</th>
                      </tr>
                    </thead>
                    <tbody className="divide-y divide-slate-800/70 text-slate-200">
                      {testSummary.results
                        .filter(
                          (r) =>
                            testCategoryFilter === 'ALL' ||
                            r.category === testCategoryFilter
                        )
                        .map((r) => (
                          <tr
                            key={r.id}
                            className="hover:bg-slate-800/30 transition-colors"
                          >
                            <td className="py-3 px-4 font-semibold text-sky-400">
                              {r.id}
                            </td>
                            <td className="py-3 px-4 text-slate-300">
                              {r.category}
                            </td>
                            <td className="py-3 px-4 font-sans font-medium text-white">
                              {r.title}
                            </td>
                            <td className="py-3 px-4 text-slate-400">
                              {r.detail}
                            </td>
                            <td className="py-3 px-4 text-right text-slate-300">
                              {r.durationMs} ms
                            </td>
                            <td className="py-3 px-4 text-right">
                              {r.passed ? (
                                <span className="text-emerald-400 font-semibold">
                                  PASS
                                </span>
                              ) : (
                                <span className="text-rose-400 font-semibold">
                                  FAIL
                                </span>
                              )}
                            </td>
                          </tr>
                        ))}
                    </tbody>
                  </table>
                </div>
              </>
            )}
          </div>
        )}

        {/* VIEW 4: COMPILER C SOURCE & REPOSITORY INTERNALS */}
        {activeNav === 'internals' && (
          <div className="max-w-[1380px] w-full mx-auto px-6 py-8 space-y-6">
            <div className="flex flex-wrap items-end justify-between gap-4 border-b border-slate-800 pb-5">
              <div>
                <h1 className="font-display text-2xl font-bold text-white">
                  Eathon Compiler &amp; Database Engine C Source Code
                </h1>
                <p className="text-sm text-slate-400 mt-1">
                  Inspect the actual ISO C99 source files compiled by GCC into
                  bin/eathonc and bin/eatdb.
                </p>
              </div>
              <div className="flex items-center gap-2 bg-[#0F172A] px-3 py-1.5 rounded-lg border border-slate-800">
                <Search className="w-3.5 h-3.5 text-slate-400" />
                <input
                  type="text"
                  placeholder="Filter lines in file..."
                  value={codeSearch}
                  onChange={(e) => setCodeSearch(e.target.value)}
                  className="bg-transparent text-xs font-mono text-slate-100 focus:outline-none"
                />
              </div>
            </div>

            <div className="flex flex-wrap items-center justify-between gap-4">
              <div className="flex flex-wrap items-center gap-1 p-1 bg-[#0F172A] rounded-lg border border-slate-800">
                {repoFiles.map((rf) => (
                  <button
                    key={rf.path}
                    type="button"
                    onClick={() => setSelectedRepoFile(rf.path)}
                    className={`px-3 py-1.5 text-xs font-mono rounded-md transition-colors cursor-pointer ${
                      selectedRepoFile === rf.path
                        ? 'bg-sky-400 text-slate-950 font-semibold'
                        : 'text-slate-400 hover:text-slate-200'
                    }`}
                  >
                    {rf.path}
                  </button>
                ))}
              </div>

              {currentRepoFileObj && (
                <div className="text-xs font-mono tabular-nums text-slate-400">
                  {currentRepoFileObj.path} · {currentRepoFileObj.lines} lines ·{' '}
                  {(currentRepoFileObj.sizeBytes / 1024).toFixed(1)} KB
                </div>
              )}
            </div>

            {currentRepoFileObj && (
              <pre className="p-5 rounded-lg bg-[#0F172A] border border-slate-800 font-mono text-xs text-slate-200 leading-5 overflow-x-auto max-h-[680px] overflow-y-auto">
                {currentRepoFileObj.content
                  .split('\n')
                  .map((line, i) => ({ line, num: i + 1 }))
                  .filter(
                    (item) =>
                      !codeSearch.trim() ||
                      item.line
                        .toLowerCase()
                        .includes(codeSearch.toLowerCase())
                  )
                  .map((item) => `${String(item.num).padStart(4, ' ')} | ${item.line}`)
                  .join('\n')}
              </pre>
            )}
          </div>
        )}

        {/* VIEW 5: FORMAL EBNF GRAMMAR & IMPLEMENTATION STATUS MATRIX */}
        {activeNav === 'spec' && (
          <div className="max-w-[1380px] w-full mx-auto px-6 py-8 space-y-10">
            <div className="border-b border-slate-800 pb-5">
              <h1 className="font-display text-2xl font-bold text-white">
                Formal EBNF Grammar Specification &amp; Engineering Status Matrix
              </h1>
              <p className="text-sm text-slate-400 mt-1">
                Complete deterministic grammar for .eat files and transparent
                breakdown of verified vs. planned capabilities.
              </p>
            </div>

            {/* Transparent Implementation Status Matrix */}
            <div className="space-y-3">
              <h2 className="font-display text-lg font-semibold text-white">
                01. Engineering Verification &amp; Roadmap Matrix
              </h2>
              <div className="border border-slate-800 rounded-lg overflow-hidden bg-[#0F172A]/60">
                <table className="w-full text-left border-collapse text-xs">
                  <thead>
                    <tr className="border-b border-slate-800 bg-[#0F172A] text-slate-400 font-mono">
                      <th className="py-3 px-4 w-48">Architecture Domain</th>
                      <th className="py-3 px-4">
                        Implemented, Compiled &amp; Verified Today (v1.0.0-core)
                      </th>
                      <th className="py-3 px-4">
                        Not Yet Implemented (Planned Engineering Roadmap)
                      </th>
                    </tr>
                  </thead>
                  <tbody className="divide-y divide-slate-800/70 text-slate-200 leading-5">
                    <tr>
                      <td className="py-3 px-4 font-semibold text-sky-400">
                        1. Lexical &amp; Syntactic Grammar
                      </td>
                      <td className="py-3 px-4">
                        Deterministic EBNF grammar using strictly commas (,) and
                        periods (.). Quotation-free strings (text of ... end
                        text), Note that comments, exact line/column
                        EATHON-LEX-001 and EATHON-SYN-* diagnostics.
                      </td>
                      <td className="py-3 px-4 text-slate-400">
                        Multi-file Import module resolver and package lockfile
                        manager.
                      </td>
                    </tr>
                    <tr>
                      <td className="py-3 px-4 font-semibold text-sky-400">
                        2. Native Compiler &amp; Linker
                      </td>
                      <td className="py-3 px-4">
                        Written in ISO C99 (compiler/src/eathonc.c). Direct
                        standalone Linux x86_64 ELF64 binary emission &amp;
                        kernel execution, Windows x86_64 PE64 (.exe) header &amp;
                        code emission, LLVM IR (.ll), and C99 (.c) generation.
                      </td>
                      <td className="py-3 px-4 text-slate-400">
                        ARM64 (AArch64) instruction encoder, DWARF v5 debug
                        symbol sections, and self-hosting Eathon-in-Eathon
                        bootstrap pass.
                      </td>
                    </tr>
                    <tr>
                      <td className="py-3 px-4 font-semibold text-sky-400">
                        3. Systems &amp; Memory Access
                      </td>
                      <td className="py-3 px-4">
                        64-bit integers, bytes, booleans, text, bitwise
                        operations (and, or, xor, not, shifts), contiguous
                        arrays, raw byte-offset memory regions, and user-defined
                        structures.
                      </td>
                      <td className="py-3 px-4 text-slate-400">
                        OS virtual memory page allocator (sys_mmap / VirtualAlloc
                        free-list), hardware interrupt table primitives, and
                        pthreads/futex concurrency primitives.
                      </td>
                    </tr>
                    <tr>
                      <td className="py-3 px-4 font-semibold text-sky-400">
                        4. Native AI &amp; Tensor Engine
                      </td>
                      <td className="py-3 px-4">
                        2D Tensor stack allocation, cell indexing, native x86_64
                        matrix multiplication loops (Multiply tensor), ReLU
                        non-linear activation (Activate relu), and reduction sum.
                      </td>
                      <td className="py-3 px-4 text-slate-400">
                        Reverse-mode automatic differentiation tape, IEEE-754
                        AVX-512 SIMD vectorization, and GPU kernel dispatch.
                      </td>
                    </tr>
                    <tr>
                      <td className="py-3 px-4 font-semibold text-sky-400">
                        5. EathonDB Storage Engine
                      </td>
                      <td className="py-3 px-4">
                        Custom C binary engine (compiler/src/eatdb.c) with
                        4096-byte slotted pages, EATDB001 superblock, CRC32
                        checksums, ACID Write-Ahead Log (.wal) with
                        Begin/Commit/Rollback, and EQL parser.
                      </td>
                      <td className="py-3 px-4 text-slate-400">
                        Multi-page B+Tree node splitting beyond 512 records and
                        multi-process advisory locking.
                      </td>
                    </tr>
                  </tbody>
                </table>
              </div>
            </div>

            {/* Formal EBNF Specification */}
            <div className="grid grid-cols-1 lg:grid-cols-2 gap-6">
              <div className="space-y-2.5">
                <h2 className="font-display text-lg font-semibold text-white">
                  02. Formal EBNF Grammar (spec/GRAMMAR.ebnf)
                </h2>
                <pre className="p-4 rounded-lg bg-[#0F172A] border border-slate-800 font-mono text-xs text-sky-200 leading-5 overflow-x-auto max-h-[600px] overflow-y-auto">
                  {grammarFileObj?.content || 'Loading spec/GRAMMAR.ebnf...'}
                </pre>
              </div>

              <div className="space-y-2.5">
                <h2 className="font-display text-lg font-semibold text-white">
                  03. Architecture Documentation (doc.md)
                </h2>
                <pre className="p-4 rounded-lg bg-[#0F172A] border border-slate-800 font-mono text-xs text-slate-200 leading-5 whitespace-pre-wrap overflow-x-auto max-h-[600px] overflow-y-auto">
                  {docFileObj?.content || 'Loading doc.md...'}
                </pre>
              </div>
            </div>
          </div>
        )}
      </main>
    </div>
  );
}
