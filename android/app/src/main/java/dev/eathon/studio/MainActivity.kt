package dev.eathon.studio

import android.app.Activity
import android.content.Intent
import android.net.Uri
import android.os.Bundle
import android.widget.Button
import android.widget.EditText
import android.widget.LinearLayout
import android.widget.TextView
import android.widget.Toast
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatActivity
import androidx.documentfile.provider.DocumentFile
import java.io.File

/**
 * Eathon Mobile Studio (inspired by Pydroid):
 * Native Android IDE for writing, opening, saving, importing folders,
 * compiling, and executing Eathon (.eat) programs offline via JNI (libeathon_jni.so).
 */
class MainActivity : AppCompatActivity() {

    private lateinit var codeEditor: EditText
    private lateinit var consoleOutput: TextView
    private lateinit var statusHeader: TextView
    private var currentFileName: String = "main.eat"
    private var lastCompiledC99: String = ""

    private val openFileLauncher = registerForActivityResult(
        ActivityResultContracts.StartActivityForResult()
    ) { result ->
        if (result.resultCode == Activity.RESULT_OK) {
            result.data?.data?.let { uri -> readEatFileFromUri(uri) }
        }
    }

    private val saveFileLauncher = registerForActivityResult(
        ActivityResultContracts.StartActivityForResult()
    ) { result ->
        if (result.resultCode == Activity.RESULT_OK) {
            result.data?.data?.let { uri -> writeTextToUri(uri, codeEditor.text.toString()) }
        }
    }

    private val exportC99Launcher = registerForActivityResult(
        ActivityResultContracts.StartActivityForResult()
    ) { result ->
        if (result.resultCode == Activity.RESULT_OK) {
            result.data?.data?.let { uri -> writeTextToUri(uri, lastCompiledC99) }
        }
    }

    private val importFolderLauncher = registerForActivityResult(
        ActivityResultContracts.StartActivityForResult()
    ) { result ->
        if (result.resultCode == Activity.RESULT_OK) {
            result.data?.data?.let { treeUri -> importProjectFolder(treeUri) }
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        codeEditor = findViewById(R.id.codeEditor)
        consoleOutput = findViewById(R.id.consoleOutput)
        statusHeader = findViewById(R.id.statusHeader)

        // Attach live syntax highlighting & block indentation
        codeEditor.addTextChangedListener(EathonSyntaxHighlighter(codeEditor))

        // Populate quick-insert snippet bar (similar to Pydroid top symbol/keyword bar)
        val snippetContainer = findViewById<LinearLayout>(R.id.snippetBarContainer)
        for ((label, snippet) in EathonSyntaxHighlighter.AUTOCOMPLETE_SNIPPETS) {
            val btn = Button(this).apply {
                text = label
                textSize = 11f
                setOnClickListener {
                    val start = codeEditor.selectionStart.coerceAtLeast(0)
                    codeEditor.editableText.insert(start, snippet + "\n")
                }
            }
            snippetContainer.addView(btn)
        }

        // Load default sample or external .eat intent
        val defaultProgram = """
            Note that this program runs natively offline on Android via JNI.

            Let age be 25.
            Let name be Chris end text.

            If age is greater than 17 then,
                Display Hello, name.
            Otherwise,
                Display You are underage.
            End if.

            To compute factorial taking n as integer, returning integer,
                If n is at most 1 then,
                    Return 1.
                End if.
                Return n times result of factorial with n minus 1.
            End function.

            Let fact5 be result of factorial with 5.
            Display Factorial of 5 is, fact5.
            Exit with 0.
        """.trimIndent()

        codeEditor.setText(defaultProgram)
        statusHeader.text = EathonBridge.getCompilerInfoNative()

        // Handle incoming VIEW/EDIT intent if user tapped a .eat file in Android file manager
        intent?.data?.let { uri -> readEatFileFromUri(uri) }

        findViewById<Button>(R.id.btnRun).setOnClickListener { runCurrentProgram() }
        findViewById<Button>(R.id.btnOpen).setOnClickListener { promptOpenFile() }
        findViewById<Button>(R.id.btnSave).setOnClickListener { promptSaveFile() }
        findViewById<Button>(R.id.btnImportFolder).setOnClickListener { promptImportFolder() }
        findViewById<Button>(R.id.btnExportC99).setOnClickListener { promptExportCompiledC99() }
        findViewById<Button>(R.id.btnEatDb).setOnClickListener { runEmbeddedDatabaseDemo() }
    }

    private fun runCurrentProgram() {
        val source = codeEditor.text.toString()
        val workspaceFile = File(filesDir, currentFileName)
        workspaceFile.writeText(source)

        val t0 = System.nanoTime()
        val res = EathonBridge.compileAndRun(source, workspaceFile.absolutePath)
        val elapsedMs = (System.nanoTime() - t0) / 1_000_000.0
        lastCompiledC99 = res.c99Code

        val report = buildString {
            appendLine("=== EATHON NATIVE EXECUTION (${res.platform} / ${res.arch}) ===")
            appendLine("File: $currentFileName | Status: ${if (res.success) "COMPILED & EXECUTED" else "DIAGNOSTIC ERROR"}")
            appendLine("Exit Code: ${res.exitCode} | Time: ${"%.2f".format(elapsedMs)} ms")
            appendLine("------------------------------------------------------------")
            append(if (res.stdout.isNotEmpty()) res.stdout else "(no output)\n")
        }
        consoleOutput.text = report
    }

    private fun runEmbeddedDatabaseDemo() {
        val dbFile = File(filesDir, "android_studio.eatdb")
        val script = "Create collection mobile_nodes. Begin. Insert key 1, label arm64_core, value 900. Commit. Select all."
        val rawJson = EathonBridge.executeEatDbNative(dbFile.absolutePath, script)
        consoleOutput.text = "=== EATHONDB PERSISTENT SLOTTED-PAGE ENGINE ===\nDB Path: ${dbFile.absolutePath}\nQuery: $script\n\nResult JSON:\n$rawJson"
    }

    private fun promptOpenFile() {
        val intent = Intent(Intent.ACTION_OPEN_DOCUMENT).apply {
            addCategory(Intent.CATEGORY_OPENABLE)
            type = "*/*"
        }
        openFileLauncher.launch(intent)
    }

    private fun promptSaveFile() {
        val intent = Intent(Intent.ACTION_CREATE_DOCUMENT).apply {
            addCategory(Intent.CATEGORY_OPENABLE)
            type = "text/plain"
            putExtra(Intent.EXTRA_TITLE, currentFileName)
        }
        saveFileLauncher.launch(intent)
    }

    private fun promptExportCompiledC99() {
        if (lastCompiledC99.isEmpty()) {
            runCurrentProgram()
        }
        val intent = Intent(Intent.ACTION_CREATE_DOCUMENT).apply {
            addCategory(Intent.CATEGORY_OPENABLE)
            type = "text/x-csrc"
            putExtra(Intent.EXTRA_TITLE, currentFileName.removeSuffix(".eat") + ".c")
        }
        exportC99Launcher.launch(intent)
    }

    private fun promptImportFolder() {
        val intent = Intent(Intent.ACTION_OPEN_DOCUMENT_TREE)
        importFolderLauncher.launch(intent)
    }

    private fun readEatFileFromUri(uri: Uri) {
        contentResolver.openInputStream(uri)?.bufferedReader()?.use { reader ->
            codeEditor.setText(reader.readText())
            currentFileName = uri.lastPathSegment?.substringAfterLast('/') ?: "program.eat"
            statusHeader.text = "Loaded: $currentFileName"
        }
    }

    private fun writeTextToUri(uri: Uri, content: String) {
        contentResolver.openOutputStream(uri, "wt")?.bufferedWriter()?.use { writer ->
            writer.write(content)
            Toast.makeText(this, "Saved to device storage.", Toast.LENGTH_SHORT).show()
        }
    }

    private fun importProjectFolder(treeUri: Uri) {
        val dir = DocumentFile.fromTreeUri(this, treeUri) ?: return
        val eatFiles = dir.listFiles().filter { it.name?.endsWith(".eat") == true }
        if (eatFiles.isEmpty()) {
            Toast.makeText(this, "No .eat files found in selected folder.", Toast.LENGTH_LONG).show()
            return
        }
        // Copy all .eat modules in the folder into app workspace so `Include module <name>.` resolves offline
        for (doc in eatFiles) {
            val name = doc.name ?: continue
            contentResolver.openInputStream(doc.uri)?.bufferedReader()?.use { reader ->
                File(filesDir, name).writeText(reader.readText())
            }
        }
        val names = eatFiles.mapNotNull { it.name }.toTypedArray()
        AlertDialog.Builder(this)
            .setTitle("Imported ${eatFiles.size} Eathon (.eat) files")
            .setItems(names) { _, which ->
                val chosen = eatFiles[which]
                readEatFileFromUri(chosen.uri)
            }
            .show()
    }
}
