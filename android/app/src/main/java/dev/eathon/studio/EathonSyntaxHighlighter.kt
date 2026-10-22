package dev.eathon.studio

import android.text.Editable
import android.text.Spannable
import android.text.TextWatcher
import android.text.style.ForegroundColorSpan
import android.widget.EditText
import java.util.regex.Pattern

/**
 * Live syntax highlighter, punctuation validator, and block auto-indenter
 * for plain-text Eathon (.eat) source files on Android.
 */
class EathonSyntaxHighlighter(private val editor: EditText) : TextWatcher {

    private var isFormatting = false

    private val commentPattern = Pattern.compile("\\bNote\\s+that\\b[^.]*\\.", Pattern.CASE_INSENSITIVE)
    private val textLiteralPattern = Pattern.compile("\\btext\\s+of\\b.*?\\bend\\s+text\\b", Pattern.CASE_INSENSITIVE)
    private val keywordPattern = Pattern.compile(
        "\\b(Let|be|Set|to|Display|Exit|with|If|then|Otherwise|End|if|While|repeat|Repeat|times|To|compute|taking|as|returning|function|Return|Call|into|Include|module|Define|structure|having|Create|field|of|Allocate|array|memory|Store|Load|element|Write|Read|at|offset|tensor|Fill|Multiply|Apply|relu|Sum|Open|database|Insert|key|value|Query|from)\\b"
    )
    private val operatorPattern = Pattern.compile(
        "\\b(plus|minus|divided|by|modulo|bitwise|and|or|xor|not|shifted|left|right|is|equal|greater|less|than|at|least|most|neg|result)\\b"
    )
    private val typePattern = Pattern.compile(
        "\\b(integer|integers|byte|bytes|boolean|text|address|true|false|[0-9]+)\\b"
    )
    private val forbiddenPunctuationPattern = Pattern.compile("[\"'=;:{}()\\[\\]<>!?@#\$%^&*+/\\\\|~`]")

    override fun beforeTextChanged(s: CharSequence?, start: Int, count: Int, after: Int) {}

    override fun onTextChanged(s: CharSequence?, start: Int, before: Int, count: Int) {
        if (isFormatting || s == null) return
        // Auto-indent after "then,\n", "Otherwise,\n", "repeat,\n", "times,\n"
        if (count == 1 && start < s.length && s[start] == '\n') {
            val beforeCursor = s.subSequence(0, start).toString()
            val lastLine = beforeCursor.substringAfterLast('\n').trimEnd()
            if (lastLine.endsWith("then,") || lastLine.endsWith("Otherwise,") ||
                lastLine.endsWith("repeat,") || lastLine.endsWith("times,")
            ) {
                isFormatting = true
                editor.editableText.insert(start + 1, "    ")
                isFormatting = false
            }
        }
    }

    override fun afterTextChanged(s: Editable?) {
        if (isFormatting || s == null) return
        isFormatting = true
        applyHighlighting(s)
        isFormatting = false
    }

    private fun applyHighlighting(s: Spannable) {
        val existingSpans = s.getSpans(0, s.length, ForegroundColorSpan::class.java)
        for (span in existingSpans) {
            s.removeSpan(span)
        }

        colorMatches(s, operatorPattern, 0xFF38BDF8.toInt())          // Sky blue operators
        colorMatches(s, typePattern, 0xFFF59E0B.toInt())              // Amber types & numbers
        colorMatches(s, keywordPattern, 0xFF10B981.toInt())           // Emerald keywords
        colorMatches(s, textLiteralPattern, 0xFFA78BFA.toInt())       // Violet text literals
        colorMatches(s, commentPattern, 0xFF64748B.toInt())           // Slate comments
        colorMatches(s, forbiddenPunctuationPattern, 0xFFEF4444.toInt()) // Red forbidden symbols
    }

    private fun colorMatches(s: Spannable, pattern: Pattern, color: Int) {
        val matcher = pattern.matcher(s)
        while (matcher.find()) {
            s.setSpan(
                ForegroundColorSpan(color),
                matcher.start(),
                matcher.end(),
                Spannable.SPAN_EXCLUSIVE_EXCLUSIVE
            )
        }
    }

    companion object {
        val AUTOCOMPLETE_SNIPPETS = listOf(
            "Let " to "Let count be 10.",
            "Display " to "Display Hello, world.",
            "If / Otherwise" to "If count is greater than 5 then,\n    Display High.\nOtherwise,\n    Display Low.\nEnd if.",
            "While loop" to "While count is greater than 0 repeat,\n    Set count to count minus 1.\nEnd while.",
            "Function" to "To compute square taking n as integer, returning integer,\n    Return n times n.\nEnd function.",
            "Note" to "Note that this section computes the result."
        )
    }
}
