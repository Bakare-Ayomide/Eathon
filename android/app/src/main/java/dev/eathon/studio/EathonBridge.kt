package dev.eathon.studio

import org.json.JSONObject

data class EathonExecutionResult(
    val success: Boolean,
    val exitCode: Int,
    val platform: String,
    val arch: String,
    val stdout: String,
    val c99Code: String
)

object EathonBridge {
    init {
        System.loadLibrary("eathon_jni")
    }

    external fun compileAndRunNative(sourceCode: String, sourcePath: String): String
    external fun executeEatDbNative(dbPath: String, eqlScript: String): String
    external fun getCompilerInfoNative(): String

    fun compileAndRun(sourceCode: String, sourcePath: String = "main.eat"): EathonExecutionResult {
        val rawJson = compileAndRunNative(sourceCode, sourcePath)
        val obj = JSONObject(rawJson)
        return EathonExecutionResult(
            success = obj.optBoolean("success", false),
            exitCode = obj.optInt("exitCode", 1),
            platform = obj.optString("platform", "Android"),
            arch = obj.optString("arch", "arm64-v8a"),
            stdout = obj.optString("stdout", ""),
            c99Code = obj.optString("c99Code", "")
        )
    }
}
