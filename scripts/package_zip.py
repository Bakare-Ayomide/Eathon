#!/usr/bin/env python3
import os
import zipfile

ROOT = os.getcwd()
PUBLIC_DIR = os.path.join(ROOT, "public")
os.makedirs(PUBLIC_DIR, exist_ok=True)
ZIP_PATH = os.path.join(PUBLIC_DIR, "eathon-project.zip")

INCLUDE_FILES = [
    "doc.md",
    "Makefile",
    "build_windows.bat",
    "spec/GRAMMAR.ebnf",
    ".github/workflows/build-and-release.yml",
    "compiler/include/eathon_platform.h",
    "compiler/src/eathonc.c",
    "compiler/src/eatdb.c",
    "tests/run_tests.mjs",
    "stdlib/mathlib.eat",
    "editors/README.md",
    "editors/vscode/package.json",
    "editors/vscode/language-configuration.json",
    "editors/vscode/syntaxes/eathon.tmLanguage.json",
    "editors/vscode/tasks.json",
    "editors/acode/plugin.json",
    "editors/acode/main.js",
    "android/settings.gradle",
    "android/build.gradle",
    "android/gradle.properties",
    "android/app/build.gradle",
    "android/app/src/main/AndroidManifest.xml",
    "android/app/src/main/cpp/CMakeLists.txt",
    "android/app/src/main/cpp/eathon_jni.c",
    "android/app/src/main/java/dev/eathon/studio/EathonBridge.kt",
    "android/app/src/main/java/dev/eathon/studio/EathonSyntaxHighlighter.kt",
    "android/app/src/main/java/dev/eathon/studio/MainActivity.kt",
    "android/app/src/main/res/layout/activity_main.xml",
    "android/app/src/main/res/values/strings.xml",
    "android/app/src/main/res/values/colors.xml",
    "bin/eathonc",
    "bin/eatdb",
    "bin/libeathon_jni.so",
    "bin/eathonc.exe",
    "bin/eatdb.exe",
]

for fname in sorted(os.listdir(os.path.join(ROOT, "examples"))):
    if fname.endswith(".eat"):
        INCLUDE_FILES.append(f"examples/{fname}")

with zipfile.ZipFile(ZIP_PATH, "w", compression=zipfile.ZIP_DEFLATED) as zf:
    for rel_path in INCLUDE_FILES:
        abs_path = os.path.join(ROOT, rel_path)
        if os.path.exists(abs_path):
            zf.write(abs_path, arcname=f"eathon/{rel_path}")

print(f"Created {ZIP_PATH} ({os.path.getsize(ZIP_PATH)} bytes) with {len(INCLUDE_FILES)} files.")
with zipfile.ZipFile(ZIP_PATH, "r") as zf:
    for info in zf.infolist():
        print(f"  {info.filename:62s} {info.file_size:8d} bytes")
