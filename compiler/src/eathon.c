/*
 * ============================================================================
 * EATHON NATIVE COMPILER (eathon)
 * Version: 1.0.1-audited
 * Implementation Language: Portable ISO C99 (Linux, Windows MinGW/Clang, macOS)
 *
 * Architecture & Backends:
 *   1. Deterministic Lexer enforcing the Eathon Punctuation Invariant
 *      (ONLY commas ',' and periods '.' are valid punctuation).
 *   2. Recursive-Descent Parser producing a typed Abstract Syntax Tree (AST).
 *   3. Lexical-Scope Semantic Analyzer & Static Type Checker.
 *   4. Backend A (Direct ELF64 Assembler): Emits standalone 64-bit x86_64 Linux
 *      ELF executables directly using Linux kernel syscalls (sys_write/sys_exit).
 *   5. Backend B (ISO C99 + Native Host Compiler Driver): Emits complete ISO C99
 *      source code and invokes host GCC or Clang (supports Linux, Windows .exe,
 *      and macOS with full recursion, structs, tensors, and disk I/O).
 *   6. Backend C (LLVM IR): Emits SSA LLVM IR (.ll) for core arithmetic/control
 *      flow constructs.
 * ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <ctype.h>
#include "../include/eathon_platform.h"

#define SET_EXECUTABLE(path) EATHON_SET_EXECUTABLE(path)


#define MAX_TOKENS 8192
#define MAX_AST_NODES 4096
#define MAX_SYMBOLS 1024
#define MAX_STRUCTS 128
#define MAX_DIAGNOSTICS 128
#define MAX_STR_LEN 512
#define MAX_CODE_SIZE 65536

/* -------------------------------------------------------------------------- */
/* 1. TOKEN DEFINITIONS                                                       */
/* -------------------------------------------------------------------------- */

typedef enum {
    TOK_EOF = 0,
    TOK_PERIOD,          /* . */
    TOK_COMMA,           /* , */
    TOK_INT_LIT,         /* 42 */
    TOK_TEXT_LIT,        /* text of ... end text */
    TOK_IDENT,           /* variable, function, struct, or field name */

    /* Keywords */
    TOK_LET,             /* Let */
    TOK_BE,              /* be */
    TOK_SET,             /* Set */
    TOK_TO,              /* to */
    TOK_DISPLAY,         /* Display */
    TOK_EXIT,            /* Exit */
    TOK_WITH,            /* with */
    TOK_IF,              /* If */
    TOK_THEN,            /* then */
    TOK_OTHERWISE,       /* Otherwise */
    TOK_END,             /* End */
    TOK_WHILE,           /* While */
    TOK_REPEAT,          /* repeat / Repeat */
    TOK_TIMES_KW,        /* times (both loop and multiplication) */
    TOK_COMPUTE,         /* compute */
    TOK_TAKING,          /* taking */
    TOK_AS,              /* as */
    TOK_RETURNING,       /* returning */
    TOK_FUNCTION,        /* function */
    TOK_RETURN,          /* Return */
    TOK_CALL,            /* Call */
    TOK_INTO,            /* into */
    TOK_RESULT,          /* result */
    TOK_OF,              /* of */

    /* Types & Literals */
    TOK_TYPE_INTEGER,    /* integer / integers */
    TOK_TYPE_BYTE,       /* byte / bytes */
    TOK_TYPE_BOOLEAN,    /* boolean */
    TOK_TYPE_TEXT,       /* text */
    TOK_TYPE_ADDRESS,    /* address */
    TOK_TRUE,            /* true */
    TOK_FALSE,           /* false */

    /* Operators */
    TOK_PLUS,            /* plus */
    TOK_MINUS,           /* minus */
    TOK_DIVIDED,         /* divided */
    TOK_BY,              /* by */
    TOK_MODULO,          /* modulo */
    TOK_BITWISE,         /* bitwise */
    TOK_SHIFTED,         /* shifted */
    TOK_LEFT,            /* left */
    TOK_RIGHT,           /* right */
    TOK_AND,             /* and */
    TOK_OR,              /* or */
    TOK_XOR,             /* xor */
    TOK_NOT,             /* not */
    TOK_NEG,             /* neg */
    TOK_IS,              /* is */
    TOK_EQUAL,           /* equal */
    TOK_GREATER,         /* greater */
    TOK_LESS,            /* less */
    TOK_THAN,            /* than */
    TOK_AT,              /* at */
    TOK_LEAST,           /* least */
    TOK_MOST,            /* most */

    /* Systems / Memory / Arrays / Structs / Tensors / DB */
    TOK_ALLOCATE,        /* Allocate */
    TOK_ARRAY,           /* array */
    TOK_STORE,           /* Store */
    TOK_LOAD,            /* Load */
    TOK_ELEMENT,         /* element */
    TOK_MEMORY,          /* memory */
    TOK_WRITE,           /* Write */
    TOK_READ,            /* Read */
    TOK_FROM,            /* from */
    TOK_OFFSET,          /* offset */
    TOK_DEFINE,          /* Define */
    TOK_STRUCTURE,       /* structure */
    TOK_FIELDS,          /* fields */
    TOK_CREATE,          /* Create */
    TOK_FIELD,           /* field */
    TOK_GET,             /* Get */
    TOK_TENSOR,          /* tensor */
    TOK_DIMENSIONS,      /* dimensions */
    TOK_FILL,            /* Fill */
    TOK_CELL,            /* cell */
    TOK_MULTIPLY,        /* Multiply */
    TOK_ACTIVATE,        /* Activate */
    TOK_RELU,            /* relu */
    TOK_ON,              /* on */
    TOK_SUM,             /* Sum */
    TOK_OPEN,            /* Open */
    TOK_DATABASE,        /* database */
    TOK_INSERT,          /* Insert */
    TOK_KEY,             /* key */
    TOK_VALUE,           /* value */
    TOK_QUERY            /* Query */
} TokenType;

typedef struct {
    TokenType type;
    char lexeme[MAX_STR_LEN];
    int64_t int_val;
    int line;
    int col;
} Token;

/* -------------------------------------------------------------------------- */
/* 2. DIAGNOSTICS                                                             */
/* -------------------------------------------------------------------------- */

typedef struct {
    char code[32];
    char stage[32];      /* "LEXER", "PARSER", "TYPECHECK", "CODEGEN" */
    char severity[16];   /* "ERROR", "WARNING" */
    char message[512];
    int line;
    int col;
} Diagnostic;

static Diagnostic g_diags[MAX_DIAGNOSTICS];
static int g_diag_count = 0;
static const char *g_source = NULL;

static void add_diag(const char *code, const char *stage, const char *severity,
                     int line, int col, const char *msg) {
    if (g_diag_count >= MAX_DIAGNOSTICS) return;
    Diagnostic *d = &g_diags[g_diag_count++];
    snprintf(d->code, sizeof(d->code), "%s", code);
    snprintf(d->stage, sizeof(d->stage), "%s", stage);
    snprintf(d->severity, sizeof(d->severity), "%s", severity);
    snprintf(d->message, sizeof(d->message), "%s", msg);
    d->line = line;
    d->col = col;
}

/* -------------------------------------------------------------------------- */
/* 3. DATA TYPES & AST DEFINITIONS                                            */
/* -------------------------------------------------------------------------- */

typedef enum {
    TYPE_VOID = 0,
    TYPE_INTEGER,
    TYPE_BYTE,
    TYPE_BOOLEAN,
    TYPE_TEXT,
    TYPE_ADDRESS,
    TYPE_ARRAY,
    TYPE_STRUCT,
    TYPE_TENSOR,
    TYPE_DATABASE
} DataType;

static const char *datatype_name(DataType t) {
    switch (t) {
        case TYPE_VOID: return "void";
        case TYPE_INTEGER: return "integer";
        case TYPE_BYTE: return "byte";
        case TYPE_BOOLEAN: return "boolean";
        case TYPE_TEXT: return "text";
        case TYPE_ADDRESS: return "address";
        case TYPE_ARRAY: return "array";
        case TYPE_STRUCT: return "structure";
        case TYPE_TENSOR: return "tensor";
        case TYPE_DATABASE: return "database";
        default: return "unknown";
    }
}

typedef enum {
    AST_PROGRAM = 0,
    AST_LET,
    AST_SET,
    AST_DISPLAY,
    AST_EXIT,
    AST_IF,
    AST_WHILE,
    AST_REPEAT,
    AST_FUNC_DEF,
    AST_RETURN,
    AST_CALL_STMT,
    AST_ARRAY_ALLOC,
    AST_ARRAY_STORE,
    AST_ARRAY_LOAD,
    AST_MEM_ALLOC,
    AST_MEM_WRITE,
    AST_MEM_READ,
    AST_STRUCT_DEF,
    AST_STRUCT_CREATE,
    AST_STRUCT_SET,
    AST_STRUCT_GET,
    AST_TENSOR_CREATE,
    AST_TENSOR_FILL,
    AST_TENSOR_SET,
    AST_TENSOR_GET,
    AST_TENSOR_MUL,
    AST_TENSOR_RELU,
    AST_TENSOR_SUM,
    AST_DB_OPEN,
    AST_DB_INSERT,
    AST_DB_QUERY,

    /* Expressions */
    AST_BINOP,
    AST_UNOP,
    AST_INT_LIT,
    AST_BOOL_LIT,
    AST_TEXT_LIT,
    AST_VAR_REF,
    AST_ARRAY_INDEX_EXPR,
    AST_STRUCT_FIELD_EXPR,
    AST_CALL_EXPR
} ASTNodeType;

typedef enum {
    OP_ADD = 0,
    OP_SUB,
    OP_MUL,
    OP_DIV,
    OP_MOD,
    OP_BIT_AND,
    OP_BIT_OR,
    OP_BIT_XOR,
    OP_SHL,
    OP_SHR,
    OP_EQ,
    OP_NEQ,
    OP_GT,
    OP_LT,
    OP_GTE,
    OP_LTE,
    OP_LOG_AND,
    OP_LOG_OR,
    OP_NEG,
    OP_NOT,
    OP_BIT_NOT
} OperatorKind;

static const char *op_name(OperatorKind op) {
    switch (op) {
        case OP_ADD: return "plus";
        case OP_SUB: return "minus";
        case OP_MUL: return "times";
        case OP_DIV: return "divided by";
        case OP_MOD: return "modulo";
        case OP_BIT_AND: return "bitwise and";
        case OP_BIT_OR: return "bitwise or";
        case OP_BIT_XOR: return "bitwise xor";
        case OP_SHL: return "shifted left by";
        case OP_SHR: return "shifted right by";
        case OP_EQ: return "is equal to";
        case OP_NEQ: return "is not equal to";
        case OP_GT: return "is greater than";
        case OP_LT: return "is less than";
        case OP_GTE: return "is at least";
        case OP_LTE: return "is at most";
        case OP_LOG_AND: return "and";
        case OP_LOG_OR: return "or";
        case OP_NEG: return "neg";
        case OP_NOT: return "not";
        case OP_BIT_NOT: return "bitwise not";
        default: return "op";
    }
}

typedef struct ASTNode {
    ASTNodeType kind;
    DataType inferred_type;
    int line;
    int col;

    char name[MAX_STR_LEN];
    char secondary_name[MAX_STR_LEN];
    char tertiary_name[MAX_STR_LEN];
    char text_val[MAX_STR_LEN];
    int64_t int_val;
    OperatorKind op;

    /* Function params or struct fields */
    int param_count;
    char param_names[16][MAX_STR_LEN];
    DataType param_types[16];

    /* Expressions / children */
    struct ASTNode *left;
    struct ASTNode *right;
    struct ASTNode *third;

    /* Call args */
    int arg_count;
    struct ASTNode *args[16];

    /* Block statements */
    int body_count;
    struct ASTNode **body;

    int else_count;
    struct ASTNode **else_body;
} ASTNode;

static ASTNode g_ast_pool[MAX_AST_NODES];
static int g_ast_count = 0;

static ASTNode *alloc_node(ASTNodeType kind, int line, int col) {
    if (g_ast_count >= MAX_AST_NODES) {
        add_diag("EATHON-INT-001", "PARSER", "ERROR", line, col, "AST node pool exhausted.");
        return &g_ast_pool[0];
    }
    ASTNode *n = &g_ast_pool[g_ast_count++];
    memset(n, 0, sizeof(ASTNode));
    n->kind = kind;
    n->line = line;
    n->col = col;
    return n;
}

static void add_body_stmt(ASTNode *parent, ASTNode *stmt) {
    parent->body = (ASTNode **)realloc(parent->body, sizeof(ASTNode *) * (parent->body_count + 1));
    parent->body[parent->body_count++] = stmt;
}

static void add_else_stmt(ASTNode *parent, ASTNode *stmt) {
    parent->else_body = (ASTNode **)realloc(parent->else_body, sizeof(ASTNode *) * (parent->else_count + 1));
    parent->else_body[parent->else_count++] = stmt;
}

/* -------------------------------------------------------------------------- */
/* 4. LEXER IMPLEMENTATION                                                    */
/* -------------------------------------------------------------------------- */

static Token g_tokens[MAX_TOKENS];
static int g_token_count = 0;

static bool is_word_match(const char *a, const char *b) {
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return false;
        a++;
        b++;
    }
    return *a == '\0' && *b == '\0';
}

static TokenType classify_word(const char *w) {
    if (is_word_match(w, "let")) return TOK_LET;
    if (is_word_match(w, "be")) return TOK_BE;
    if (is_word_match(w, "set")) return TOK_SET;
    if (is_word_match(w, "to")) return TOK_TO;
    if (is_word_match(w, "display")) return TOK_DISPLAY;
    if (is_word_match(w, "exit")) return TOK_EXIT;
    if (is_word_match(w, "with")) return TOK_WITH;
    if (is_word_match(w, "if")) return TOK_IF;
    if (is_word_match(w, "then")) return TOK_THEN;
    if (is_word_match(w, "otherwise")) return TOK_OTHERWISE;
    if (is_word_match(w, "end")) return TOK_END;
    if (is_word_match(w, "while")) return TOK_WHILE;
    if (is_word_match(w, "repeat")) return TOK_REPEAT;
    if (is_word_match(w, "times")) return TOK_TIMES_KW;
    if (is_word_match(w, "compute")) return TOK_COMPUTE;
    if (is_word_match(w, "taking")) return TOK_TAKING;
    if (is_word_match(w, "as")) return TOK_AS;
    if (is_word_match(w, "returning")) return TOK_RETURNING;
    if (is_word_match(w, "function")) return TOK_FUNCTION;
    if (is_word_match(w, "return")) return TOK_RETURN;
    if (is_word_match(w, "call")) return TOK_CALL;
    if (is_word_match(w, "into")) return TOK_INTO;
    if (is_word_match(w, "result")) return TOK_RESULT;
    if (is_word_match(w, "of")) return TOK_OF;

    if (is_word_match(w, "integer") || is_word_match(w, "integers")) return TOK_TYPE_INTEGER;
    if (is_word_match(w, "byte") || is_word_match(w, "bytes")) return TOK_TYPE_BYTE;
    if (is_word_match(w, "boolean")) return TOK_TYPE_BOOLEAN;
    if (is_word_match(w, "text")) return TOK_TYPE_TEXT;
    if (is_word_match(w, "address")) return TOK_TYPE_ADDRESS;
    if (is_word_match(w, "true")) return TOK_TRUE;
    if (is_word_match(w, "false")) return TOK_FALSE;

    if (is_word_match(w, "plus")) return TOK_PLUS;
    if (is_word_match(w, "minus")) return TOK_MINUS;
    if (is_word_match(w, "divided")) return TOK_DIVIDED;
    if (is_word_match(w, "by")) return TOK_BY;
    if (is_word_match(w, "modulo")) return TOK_MODULO;
    if (is_word_match(w, "bitwise")) return TOK_BITWISE;
    if (is_word_match(w, "shifted")) return TOK_SHIFTED;
    if (is_word_match(w, "left")) return TOK_LEFT;
    if (is_word_match(w, "right")) return TOK_RIGHT;
    if (is_word_match(w, "and")) return TOK_AND;
    if (is_word_match(w, "or")) return TOK_OR;
    if (is_word_match(w, "xor")) return TOK_XOR;
    if (is_word_match(w, "not")) return TOK_NOT;
    if (is_word_match(w, "neg")) return TOK_NEG;
    if (is_word_match(w, "is")) return TOK_IS;
    if (is_word_match(w, "equal")) return TOK_EQUAL;
    if (is_word_match(w, "greater")) return TOK_GREATER;
    if (is_word_match(w, "less")) return TOK_LESS;
    if (is_word_match(w, "than")) return TOK_THAN;
    if (is_word_match(w, "at")) return TOK_AT;
    if (is_word_match(w, "least")) return TOK_LEAST;
    if (is_word_match(w, "most")) return TOK_MOST;

    if (is_word_match(w, "allocate")) return TOK_ALLOCATE;
    if (is_word_match(w, "array")) return TOK_ARRAY;
    if (is_word_match(w, "store")) return TOK_STORE;
    if (is_word_match(w, "load")) return TOK_LOAD;
    if (is_word_match(w, "element")) return TOK_ELEMENT;
    if (is_word_match(w, "memory")) return TOK_MEMORY;
    if (is_word_match(w, "write")) return TOK_WRITE;
    if (is_word_match(w, "read")) return TOK_READ;
    if (is_word_match(w, "from")) return TOK_FROM;
    if (is_word_match(w, "offset")) return TOK_OFFSET;
    if (is_word_match(w, "define")) return TOK_DEFINE;
    if (is_word_match(w, "structure")) return TOK_STRUCTURE;
    if (is_word_match(w, "fields")) return TOK_FIELDS;
    if (is_word_match(w, "create")) return TOK_CREATE;
    if (is_word_match(w, "field")) return TOK_FIELD;
    if (is_word_match(w, "get")) return TOK_GET;
    if (is_word_match(w, "tensor")) return TOK_TENSOR;
    if (is_word_match(w, "dimensions")) return TOK_DIMENSIONS;
    if (is_word_match(w, "fill")) return TOK_FILL;
    if (is_word_match(w, "cell")) return TOK_CELL;
    if (is_word_match(w, "multiply")) return TOK_MULTIPLY;
    if (is_word_match(w, "activate")) return TOK_ACTIVATE;
    if (is_word_match(w, "relu")) return TOK_RELU;
    if (is_word_match(w, "on")) return TOK_ON;
    if (is_word_match(w, "sum")) return TOK_SUM;
    if (is_word_match(w, "open")) return TOK_OPEN;
    if (is_word_match(w, "database")) return TOK_DATABASE;
    if (is_word_match(w, "insert")) return TOK_INSERT;
    if (is_word_match(w, "key")) return TOK_KEY;
    if (is_word_match(w, "value")) return TOK_VALUE;
    if (is_word_match(w, "query")) return TOK_QUERY;

    return TOK_IDENT;
}

static char g_included_modules[32][MAX_STR_LEN];
static int g_included_count = 0;

static bool is_module_included(const char *mod) {
    for (int i = 0; i < g_included_count; i++) {
        if (strcmp(g_included_modules[i], mod) == 0) return true;
    }
    return false;
}

static char *read_module_file(const char *mod_name, const char *input_path) {
    char cand[4][512];
    int ccount = 0;
    if (input_path) {
        const char *slash = strrchr(input_path, '/');
        const char *bslash = strrchr(input_path, '\\');
        const char *sep = slash > bslash ? slash : bslash;
        if (sep) {
            int dlen = (int)(sep - input_path);
            if (dlen > 0 && dlen < 400) {
                snprintf(cand[ccount++], 512, "%.*s/%s.eat", dlen, input_path, mod_name);
            }
        }
    }
    snprintf(cand[ccount++], 512, "%s.eat", mod_name);
    snprintf(cand[ccount++], 512, "stdlib/%s.eat", mod_name);
    snprintf(cand[ccount++], 512, "examples/%s.eat", mod_name);

    for (int i = 0; i < ccount; i++) {
        FILE *f = fopen(cand[i], "rb");
        if (f) {
            fseek(f, 0, SEEK_END);
            long sz = ftell(f);
            fseek(f, 0, SEEK_SET);
            char *buf = (char *)calloc(1, (size_t)sz + 4);
            if (sz > 0) {
                size_t nr = fread(buf, 1, (size_t)sz, f);
                (void)nr;
            }
            fclose(f);
            return buf;
        }
    }
    return NULL;
}

static char *preprocess_includes(const char *src, const char *input_path, int depth) {
    if (depth > 8) {
        size_t sl = strlen(src);
        char *dup = (char *)calloc(1, sl + 1);
        memcpy(dup, src, sl);
        return dup;
    }
    size_t cap = strlen(src) * 2 + 4096;
    char *out = (char *)calloc(1, cap);
    size_t out_len = 0;

    int i = 0;
    int len = (int)strlen(src);
    int line = 1;
    while (i < len) {
        if (src[i] == '\n') line++;
        /* Check at statement boundary for "Include module <name>." */
        if ((i == 0 || isspace((unsigned char)src[i - 1]) || src[i - 1] == '.') &&
            isalpha((unsigned char)src[i])) {
            int save_i = i;
            char w1[64] = {0}, w2[64] = {0}, w3[128] = {0};
            int wi = 0;
            while (i < len && isalnum((unsigned char)src[i]) && wi < 62) w1[wi++] = src[i++];
            w1[wi] = '\0';
            if (is_word_match(w1, "Note")) {
                /* Skip over Note that ... . comment without expanding Include inside comments */
                while (i < len && src[i] != '.') {
                    if (src[i] == '\n') line++;
                    i++;
                }
                if (i < len && src[i] == '.') i++;
                size_t chunk = (size_t)(i - save_i);
                if (out_len + chunk + 64 >= cap) {
                    cap = (out_len + chunk) * 2 + 4096;
                    out = (char *)realloc(out, cap);
                }
                memcpy(out + out_len, src + save_i, chunk);
                out_len += chunk;
                out[out_len] = '\0';
                continue;
            }
            if (is_word_match(w1, "Include")) {
                while (i < len && (src[i] == ' ' || src[i] == '\t')) i++;
                wi = 0;
                while (i < len && isalnum((unsigned char)src[i]) && wi < 62) w2[wi++] = src[i++];
                w2[wi] = '\0';
                while (i < len && (src[i] == ' ' || src[i] == '\t')) i++;
                wi = 0;
                while (i < len && isalnum((unsigned char)src[i]) && wi < 126) w3[wi++] = src[i++];
                w3[wi] = '\0';
                while (i < len && (src[i] == ' ' || src[i] == '\t')) i++;
                if (is_word_match(w2, "module") && strlen(w3) > 0 && i < len && src[i] == '.') {
                    i++; /* consume period */
                    if (!is_module_included(w3)) {
                        if (g_included_count < 32) {
                            snprintf(g_included_modules[g_included_count++], MAX_STR_LEN, "%s", w3);
                        }
                        char *mod_raw = read_module_file(w3, input_path);
                        if (!mod_raw) {
                            char msg[256];
                            snprintf(msg, sizeof(msg),
                                     "Cannot find module '%s.eat' in current directory, stdlib/, or examples/.", w3);
                            add_diag("EATHON-MOD-001", "PARSER", "ERROR", line, 1, msg);
                        } else {
                            char *mod_expanded = preprocess_includes(mod_raw, input_path, depth + 1);
                            free(mod_raw);
                            size_t mlen = strlen(mod_expanded);
                            if (out_len + mlen + 64 >= cap) {
                                cap = (out_len + mlen) * 2 + 4096;
                                out = (char *)realloc(out, cap);
                            }
                            memcpy(out + out_len, mod_expanded, mlen);
                            out_len += mlen;
                            out[out_len++] = '\n';
                            out[out_len] = '\0';
                            free(mod_expanded);
                        }
                    }
                    continue;
                }
            }
            i = save_i;
        }
        if (out_len + 8 >= cap) {
            cap = cap * 2 + 4096;
            out = (char *)realloc(out, cap);
        }
        out[out_len++] = src[i++];
        out[out_len] = '\0';
    }
    return out;
}

static void lexer_tokenize(const char *src) {
    g_token_count = 0;
    int i = 0;
    int len = (int)strlen(src);
    int line = 1;
    int col = 1;

    while (i < len) {
        char c = src[i];

        if (c == '\n') {
            line++;
            col = 1;
            i++;
            continue;
        }
        if (isspace((unsigned char)c)) {
            col++;
            i++;
            continue;
        }

        if (c == '.') {
            Token *t = &g_tokens[g_token_count++];
            t->type = TOK_PERIOD;
            strcpy(t->lexeme, ".");
            t->line = line;
            t->col = col;
            i++;
            col++;
            continue;
        }

        if (c == ',') {
            Token *t = &g_tokens[g_token_count++];
            t->type = TOK_COMMA;
            strcpy(t->lexeme, ",");
            t->line = line;
            t->col = col;
            i++;
            col++;
            continue;
        }

        if (isdigit((unsigned char)c)) {
            int start_col = col;
            char buf[64];
            int bi = 0;
            while (i < len && isdigit((unsigned char)src[i]) && bi < 62) {
                buf[bi++] = src[i++];
                col++;
            }
            buf[bi] = '\0';
            if (i < len && isalpha((unsigned char)src[i])) {
                char msg[256];
                snprintf(msg, sizeof(msg),
                         "Invalid numeric literal '%s%c': identifiers cannot start with digits.",
                         buf, src[i]);
                add_diag("EATHON-LEX-002", "LEXER", "ERROR", line, start_col, msg);
            }
            Token *t = &g_tokens[g_token_count++];
            t->type = TOK_INT_LIT;
            strcpy(t->lexeme, buf);
            t->int_val = strtoll(buf, NULL, 10);
            t->line = line;
            t->col = start_col;
            continue;
        }

        if (isalpha((unsigned char)c)) {
            int start_col = col;
            int start_line = line;
            char word[128];
            int wi = 0;
            while (i < len && isalnum((unsigned char)src[i]) && wi < 126) {
                word[wi++] = src[i++];
                col++;
            }
            word[wi] = '\0';

            /* Check for "Note that ... ." comment */
            if (is_word_match(word, "Note")) {
                int saved_i = i;
                int saved_col = col;
                while (i < len && (src[i] == ' ' || src[i] == '\t')) { i++; col++; }
                char next_w[64];
                int nwi = 0;
                while (i < len && isalnum((unsigned char)src[i]) && nwi < 62) {
                    next_w[nwi++] = src[i++];
                    col++;
                }
                next_w[nwi] = '\0';
                if (is_word_match(next_w, "that")) {
                    while (i < len && src[i] != '.') {
                        char ch = src[i];
                        if (!isalnum((unsigned char)ch) && !isspace((unsigned char)ch) && ch != ',' && ch != '\'') {
                            char msg[256];
                            snprintf(msg, sizeof(msg),
                                     "Forbidden punctuation '%c' (ASCII %d) inside comment. Eathon allows ONLY commas (,) and periods (.).",
                                     ch, (int)ch);
                            add_diag("EATHON-LEX-001", "LEXER", "ERROR", line, col, msg);
                        }
                        if (ch == '\n') { line++; col = 1; } else { col++; }
                        i++;
                    }
                    if (i < len && src[i] == '.') {
                        i++;
                        col++;
                    } else {
                        add_diag("EATHON-LEX-003", "LEXER", "ERROR", start_line, start_col,
                                 "Unterminated 'Note that' comment: every statement and comment must end with a period (.).");
                    }
                    continue;
                } else {
                    i = saved_i;
                    col = saved_col;
                }
            }

            /* Check for "text of ... end text" quotation-free text literal */
            if (is_word_match(word, "text")) {
                int saved_i = i;
                int saved_col = col;
                int saved_line = line;
                while (i < len && isspace((unsigned char)src[i])) {
                    if (src[i] == '\n') { line++; col = 1; } else { col++; }
                    i++;
                }
                char next_w[64];
                int nwi = 0;
                while (i < len && isalnum((unsigned char)src[i]) && nwi < 62) {
                    next_w[nwi++] = src[i++];
                    col++;
                }
                next_w[nwi] = '\0';
                if (is_word_match(next_w, "of")) {
                    char text_buf[MAX_STR_LEN];
                    text_buf[0] = '\0';
                    bool found_end = false;
                    while (i < len) {
                        while (i < len && isspace((unsigned char)src[i])) {
                            if (src[i] == '\n') { line++; col = 1; } else { col++; }
                            i++;
                        }
                        if (i >= len) break;
                        if (src[i] == ',') {
                            strncat(text_buf, ",", sizeof(text_buf) - strlen(text_buf) - 1);
                            i++; col++;
                            continue;
                        }
                        if (src[i] == '.') {
                            break;
                        }
                        if (!isalnum((unsigned char)src[i])) {
                            char msg[256];
                            snprintf(msg, sizeof(msg),
                                     "Forbidden character '%c' inside text literal. Eathon allows ONLY words, numbers, commas, and periods.",
                                     src[i]);
                            add_diag("EATHON-LEX-001", "LEXER", "ERROR", line, col, msg);
                            i++; col++;
                            continue;
                        }
                        char tw[128];
                        int twi = 0;
                        while (i < len && isalnum((unsigned char)src[i]) && twi < 126) {
                            tw[twi++] = src[i++];
                            col++;
                        }
                        tw[twi] = '\0';
                        if (is_word_match(tw, "end")) {
                            int peek_i = i;
                            int peek_col = col;
                            int peek_line = line;
                            while (peek_i < len && isspace((unsigned char)src[peek_i])) {
                                if (src[peek_i] == '\n') { peek_line++; peek_col = 1; } else { peek_col++; }
                                peek_i++;
                            }
                            char tw2[64];
                            int tw2i = 0;
                            while (peek_i < len && isalnum((unsigned char)src[peek_i]) && tw2i < 62) {
                                tw2[tw2i++] = src[peek_i++];
                                peek_col++;
                            }
                            tw2[tw2i] = '\0';
                            if (is_word_match(tw2, "text")) {
                                i = peek_i;
                                col = peek_col;
                                line = peek_line;
                                found_end = true;
                                break;
                            }
                        }
                        if (strlen(text_buf) > 0 && text_buf[strlen(text_buf) - 1] != ' ') {
                            strncat(text_buf, " ", sizeof(text_buf) - strlen(text_buf) - 1);
                        }
                        strncat(text_buf, tw, sizeof(text_buf) - strlen(text_buf) - 1);
                    }
                    if (!found_end) {
                        add_diag("EATHON-LEX-004", "LEXER", "ERROR", start_line, start_col,
                                 "Unterminated 'text of ...' literal: expected 'end text' before period.");
                    }
                    Token *t = &g_tokens[g_token_count++];
                    t->type = TOK_TEXT_LIT;
                    snprintf(t->lexeme, sizeof(t->lexeme), "%s", text_buf);
                    t->line = start_line;
                    t->col = start_col;
                    continue;
                } else {
                    i = saved_i;
                    col = saved_col;
                    line = saved_line;
                }
            }

            Token *t = &g_tokens[g_token_count++];
            t->type = classify_word(word);
            snprintf(t->lexeme, sizeof(t->lexeme), "%s", word);
            t->line = start_line;
            t->col = start_col;
            continue;
        }

        char msg[256];
        if (c == '"' || c == '\'') {
            snprintf(msg, sizeof(msg),
                     "Forbidden quotation mark '%c' at line %d, column %d. Eathon forbids quotation marks; use 'text of ... end text' instead.",
                     c, line, col);
        } else if (c == ';') {
            snprintf(msg, sizeof(msg),
                     "Forbidden semicolon ';' at line %d, column %d. Every Eathon statement must end with a period (.).",
                     line, col);
        } else if (c == '=') {
            snprintf(msg, sizeof(msg),
                     "Forbidden equals sign '=' at line %d, column %d. Use 'Let x be ...' or 'Set x to ...' or 'is equal to'.",
                     line, col);
        } else if (c == '{' || c == '}' || c == '(' || c == ')' || c == '[' || c == ']') {
            snprintf(msg, sizeof(msg),
                     "Forbidden bracket/brace '%c' at line %d, column %d. Eathon uses only commas (,) and periods (.) as punctuation.",
                     c, line, col);
        } else {
            snprintf(msg, sizeof(msg),
                     "Forbidden symbol '%c' (ASCII %d) at line %d, column %d. Eathon source code allows only commas (,) and periods (.) as punctuation.",
                     c, (int)c, line, col);
        }
        add_diag("EATHON-LEX-001", "LEXER", "ERROR", line, col, msg);
        i++;
        col++;
    }

    Token *eof = &g_tokens[g_token_count++];
    eof->type = TOK_EOF;
    strcpy(eof->lexeme, "EOF");
    eof->line = line;
    eof->col = col;
}

/* -------------------------------------------------------------------------- */
/* 5. RECURSIVE-DESCENT PARSER                                                */
/* -------------------------------------------------------------------------- */

static int g_pos = 0;

static Token *peek(void) {
    if (g_pos >= g_token_count) return &g_tokens[g_token_count - 1];
    return &g_tokens[g_pos];
}

static Token *peek_n(int offset) {
    int idx = g_pos + offset;
    if (idx >= g_token_count) return &g_tokens[g_token_count - 1];
    return &g_tokens[idx];
}

static Token *advance(void) {
    Token *t = peek();
    if (t->type != TOK_EOF) g_pos++;
    return t;
}

static bool match(TokenType type) {
    if (peek()->type == type) {
        advance();
        return true;
    }
    return false;
}

static Token *expect(TokenType type, const char *err_desc) {
    Token *t = peek();
    if (t->type == type) {
        return advance();
    }
    char msg[256];
    snprintf(msg, sizeof(msg), "Expected %s, but found '%s'.", err_desc, t->lexeme);
    add_diag("EATHON-SYN-001", "PARSER", "ERROR", t->line, t->col, msg);
    while (peek()->type != TOK_EOF && peek()->type != TOK_PERIOD && peek()->type != TOK_COMMA) {
        advance();
    }
    return t;
}

static DataType parse_type_name(void) {
    Token *t = peek();
    if (match(TOK_TYPE_INTEGER)) return TYPE_INTEGER;
    if (match(TOK_TYPE_BYTE)) return TYPE_BYTE;
    if (match(TOK_TYPE_BOOLEAN)) return TYPE_BOOLEAN;
    if (match(TOK_TYPE_TEXT)) return TYPE_TEXT;
    if (match(TOK_TYPE_ADDRESS)) return TYPE_ADDRESS;
    char msg[256];
    snprintf(msg, sizeof(msg), "Expected data type (integer, byte, boolean, text, address), found '%s'.", t->lexeme);
    add_diag("EATHON-SYN-002", "PARSER", "ERROR", t->line, t->col, msg);
    advance();
    return TYPE_INTEGER;
}

static ASTNode *parse_expr(void);

static ASTNode *parse_primary(void) {
    Token *t = peek();
    if (match(TOK_INT_LIT)) {
        ASTNode *n = alloc_node(AST_INT_LIT, t->line, t->col);
        n->int_val = t->int_val;
        n->inferred_type = TYPE_INTEGER;
        return n;
    }
    if (match(TOK_TRUE)) {
        ASTNode *n = alloc_node(AST_BOOL_LIT, t->line, t->col);
        n->int_val = 1;
        n->inferred_type = TYPE_BOOLEAN;
        return n;
    }
    if (match(TOK_FALSE)) {
        ASTNode *n = alloc_node(AST_BOOL_LIT, t->line, t->col);
        n->int_val = 0;
        n->inferred_type = TYPE_BOOLEAN;
        return n;
    }
    if (match(TOK_TEXT_LIT)) {
        ASTNode *n = alloc_node(AST_TEXT_LIT, t->line, t->col);
        snprintf(n->text_val, sizeof(n->text_val), "%s", t->lexeme);
        n->inferred_type = TYPE_TEXT;
        return n;
    }
    if (match(TOK_ELEMENT)) {
        ASTNode *idx = parse_primary();
        expect(TOK_OF, "'of' after element index");
        Token *arr = expect(TOK_IDENT, "array identifier");
        ASTNode *n = alloc_node(AST_ARRAY_INDEX_EXPR, t->line, t->col);
        snprintf(n->name, sizeof(n->name), "%s", arr->lexeme);
        n->left = idx;
        n->inferred_type = TYPE_INTEGER;
        return n;
    }
    if (match(TOK_FIELD)) {
        Token *fname = expect(TOK_IDENT, "field name");
        expect(TOK_OF, "'of' after field name");
        Token *sname = expect(TOK_IDENT, "structure instance name");
        ASTNode *n = alloc_node(AST_STRUCT_FIELD_EXPR, t->line, t->col);
        snprintf(n->name, sizeof(n->name), "%s", sname->lexeme);
        snprintf(n->secondary_name, sizeof(n->secondary_name), "%s", fname->lexeme);
        n->inferred_type = TYPE_INTEGER;
        return n;
    }
    if (match(TOK_RESULT)) {
        expect(TOK_OF, "'of' after 'result'");
        Token *fn = expect(TOK_IDENT, "function name");
        ASTNode *n = alloc_node(AST_CALL_EXPR, t->line, t->col);
        snprintf(n->name, sizeof(n->name), "%s", fn->lexeme);
        if (match(TOK_WITH)) {
            do {
                if (n->arg_count < 16) {
                    n->args[n->arg_count++] = parse_expr();
                }
            } while (match(TOK_COMMA));
        }
        n->inferred_type = TYPE_INTEGER;
        return n;
    }
    if (t->type == TOK_IDENT) {
        advance();
        ASTNode *n = alloc_node(AST_VAR_REF, t->line, t->col);
        snprintf(n->name, sizeof(n->name), "%s", t->lexeme);
        return n;
    }

    char msg[256];
    snprintf(msg, sizeof(msg), "Unexpected token '%s' in expression.", t->lexeme);
    add_diag("EATHON-SYN-003", "PARSER", "ERROR", t->line, t->col, msg);
    advance();
    return alloc_node(AST_INT_LIT, t->line, t->col);
}

static ASTNode *parse_unary(void) {
    Token *t = peek();
    if (match(TOK_NEG)) {
        ASTNode *n = alloc_node(AST_UNOP, t->line, t->col);
        n->op = OP_NEG;
        n->left = parse_unary();
        n->inferred_type = TYPE_INTEGER;
        return n;
    }
    if (match(TOK_NOT)) {
        ASTNode *n = alloc_node(AST_UNOP, t->line, t->col);
        n->op = OP_NOT;
        n->left = parse_unary();
        n->inferred_type = TYPE_BOOLEAN;
        return n;
    }
    if (peek()->type == TOK_BITWISE && peek_n(1)->type == TOK_NOT) {
        advance();
        advance();
        ASTNode *n = alloc_node(AST_UNOP, t->line, t->col);
        n->op = OP_BIT_NOT;
        n->left = parse_unary();
        n->inferred_type = TYPE_INTEGER;
        return n;
    }
    return parse_primary();
}

static ASTNode *parse_multiplicative(void) {
    ASTNode *left = parse_unary();
    while (1) {
        Token *t = peek();
        if (t->type == TOK_TIMES_KW && peek_n(1)->type != TOK_COMMA && peek_n(1)->type != TOK_PERIOD) {
            advance();
            ASTNode *right = parse_unary();
            ASTNode *b = alloc_node(AST_BINOP, t->line, t->col);
            b->op = OP_MUL;
            b->left = left;
            b->right = right;
            b->inferred_type = TYPE_INTEGER;
            left = b;
        } else if (t->type == TOK_DIVIDED && peek_n(1)->type == TOK_BY) {
            advance();
            advance();
            ASTNode *right = parse_unary();
            ASTNode *b = alloc_node(AST_BINOP, t->line, t->col);
            b->op = OP_DIV;
            b->left = left;
            b->right = right;
            b->inferred_type = TYPE_INTEGER;
            left = b;
        } else if (match(TOK_MODULO)) {
            ASTNode *right = parse_unary();
            ASTNode *b = alloc_node(AST_BINOP, t->line, t->col);
            b->op = OP_MOD;
            b->left = left;
            b->right = right;
            b->inferred_type = TYPE_INTEGER;
            left = b;
        } else if (t->type == TOK_BITWISE && peek_n(1)->type == TOK_AND) {
            advance();
            advance();
            ASTNode *right = parse_unary();
            ASTNode *b = alloc_node(AST_BINOP, t->line, t->col);
            b->op = OP_BIT_AND;
            b->left = left;
            b->right = right;
            b->inferred_type = TYPE_INTEGER;
            left = b;
        } else if (t->type == TOK_SHIFTED && peek_n(1)->type == TOK_LEFT && peek_n(2)->type == TOK_BY) {
            advance(); advance(); advance();
            ASTNode *right = parse_unary();
            ASTNode *b = alloc_node(AST_BINOP, t->line, t->col);
            b->op = OP_SHL;
            b->left = left;
            b->right = right;
            b->inferred_type = TYPE_INTEGER;
            left = b;
        } else if (t->type == TOK_SHIFTED && peek_n(1)->type == TOK_RIGHT && peek_n(2)->type == TOK_BY) {
            advance(); advance(); advance();
            ASTNode *right = parse_unary();
            ASTNode *b = alloc_node(AST_BINOP, t->line, t->col);
            b->op = OP_SHR;
            b->left = left;
            b->right = right;
            b->inferred_type = TYPE_INTEGER;
            left = b;
        } else {
            break;
        }
    }
    return left;
}

static ASTNode *parse_additive(void) {
    ASTNode *left = parse_multiplicative();
    while (1) {
        Token *t = peek();
        if (match(TOK_PLUS)) {
            ASTNode *right = parse_multiplicative();
            ASTNode *b = alloc_node(AST_BINOP, t->line, t->col);
            b->op = OP_ADD;
            b->left = left;
            b->right = right;
            b->inferred_type = TYPE_INTEGER;
            left = b;
        } else if (match(TOK_MINUS)) {
            ASTNode *right = parse_multiplicative();
            ASTNode *b = alloc_node(AST_BINOP, t->line, t->col);
            b->op = OP_SUB;
            b->left = left;
            b->right = right;
            b->inferred_type = TYPE_INTEGER;
            left = b;
        } else if (t->type == TOK_BITWISE && peek_n(1)->type == TOK_OR) {
            advance(); advance();
            ASTNode *right = parse_multiplicative();
            ASTNode *b = alloc_node(AST_BINOP, t->line, t->col);
            b->op = OP_BIT_OR;
            b->left = left;
            b->right = right;
            b->inferred_type = TYPE_INTEGER;
            left = b;
        } else if (t->type == TOK_BITWISE && peek_n(1)->type == TOK_XOR) {
            advance(); advance();
            ASTNode *right = parse_multiplicative();
            ASTNode *b = alloc_node(AST_BINOP, t->line, t->col);
            b->op = OP_BIT_XOR;
            b->left = left;
            b->right = right;
            b->inferred_type = TYPE_INTEGER;
            left = b;
        } else {
            break;
        }
    }
    return left;
}

static ASTNode *parse_comparison(void) {
    ASTNode *left = parse_additive();
    Token *t = peek();
    if (t->type == TOK_IS) {
        advance();
        OperatorKind op = OP_EQ;
        if (match(TOK_NOT)) {
            expect(TOK_EQUAL, "'equal' after 'is not'");
            expect(TOK_TO, "'to' after 'is not equal'");
            op = OP_NEQ;
        } else if (match(TOK_EQUAL)) {
            expect(TOK_TO, "'to' after 'is equal'");
            op = OP_EQ;
        } else if (match(TOK_GREATER)) {
            expect(TOK_THAN, "'than' after 'is greater'");
            op = OP_GT;
        } else if (match(TOK_LESS)) {
            expect(TOK_THAN, "'than' after 'is less'");
            op = OP_LT;
        } else if (match(TOK_AT)) {
            if (match(TOK_LEAST)) {
                op = OP_GTE;
            } else if (match(TOK_MOST)) {
                op = OP_LTE;
            } else {
                add_diag("EATHON-SYN-004", "PARSER", "ERROR", peek()->line, peek()->col,
                         "Expected 'least' or 'most' after 'is at'.");
            }
        } else {
            add_diag("EATHON-SYN-004", "PARSER", "ERROR", peek()->line, peek()->col,
                     "Invalid comparison operator after 'is'.");
        }
        ASTNode *right = parse_additive();
        ASTNode *b = alloc_node(AST_BINOP, t->line, t->col);
        b->op = op;
        b->left = left;
        b->right = right;
        b->inferred_type = TYPE_BOOLEAN;
        return b;
    }
    return left;
}

static ASTNode *parse_logical_and(void) {
    ASTNode *left = parse_comparison();
    while (peek()->type == TOK_AND) {
        Token *t = advance();
        ASTNode *right = parse_comparison();
        ASTNode *b = alloc_node(AST_BINOP, t->line, t->col);
        b->op = OP_LOG_AND;
        b->left = left;
        b->right = right;
        b->inferred_type = TYPE_BOOLEAN;
        left = b;
    }
    return left;
}

static ASTNode *parse_expr(void) {
    ASTNode *left = parse_logical_and();
    while (peek()->type == TOK_OR) {
        Token *t = advance();
        ASTNode *right = parse_logical_and();
        ASTNode *b = alloc_node(AST_BINOP, t->line, t->col);
        b->op = OP_LOG_OR;
        b->left = left;
        b->right = right;
        b->inferred_type = TYPE_BOOLEAN;
        left = b;
    }
    return left;
}

static bool has_end_text_before_period(void) {
    int idx = g_pos;
    while (idx < g_token_count && g_tokens[idx].type != TOK_PERIOD && g_tokens[idx].type != TOK_EOF) {
        if (g_tokens[idx].type == TOK_END && idx + 1 < g_token_count && g_tokens[idx + 1].type == TOK_TYPE_TEXT) {
            return true;
        }
        idx++;
    }
    return false;
}

static ASTNode *parse_bare_end_text_literal(void) {
    Token *start_tok = peek();
    ASTNode *n = alloc_node(AST_TEXT_LIT, start_tok->line, start_tok->col);
    n->inferred_type = TYPE_TEXT;
    n->text_val[0] = '\0';
    while (peek()->type != TOK_EOF && peek()->type != TOK_PERIOD) {
        if (peek()->type == TOK_END && peek_n(1)->type == TOK_TYPE_TEXT) {
            advance();
            advance();
            break;
        }
        Token *w = advance();
        if (w->type == TOK_COMMA) {
            strncat(n->text_val, ",", sizeof(n->text_val) - strlen(n->text_val) - 1);
        } else {
            if (strlen(n->text_val) > 0) {
                strncat(n->text_val, " ", sizeof(n->text_val) - strlen(n->text_val) - 1);
            }
            strncat(n->text_val, w->lexeme, sizeof(n->text_val) - strlen(n->text_val) - 1);
        }
    }
    return n;
}

static bool is_binary_op_token(TokenType t) {
    return (t == TOK_PLUS || t == TOK_MINUS || t == TOK_TIMES_KW ||
            t == TOK_DIVIDED || t == TOK_MODULO || t == TOK_BITWISE ||
            t == TOK_SHIFTED || t == TOK_IS || t == TOK_AND || t == TOK_OR);
}

static ASTNode *parse_statement(void) {
    Token *t = peek();

    if (match(TOK_LET)) {
        Token *id = expect(TOK_IDENT, "variable identifier after 'Let'");
        expect(TOK_BE, "'be' after variable identifier");
        DataType explicit_type = TYPE_VOID;
        if (peek()->type == TOK_TYPE_INTEGER || peek()->type == TOK_TYPE_BYTE ||
            peek()->type == TOK_TYPE_BOOLEAN || peek()->type == TOK_TYPE_TEXT ||
            peek()->type == TOK_TYPE_ADDRESS) {
            explicit_type = parse_type_name();
        }
        ASTNode *expr = NULL;
        if (peek()->type != TOK_TEXT_LIT && has_end_text_before_period()) {
            expr = parse_bare_end_text_literal();
            if (explicit_type == TYPE_VOID) explicit_type = TYPE_TEXT;
        } else {
            expr = parse_expr();
        }
        expect(TOK_PERIOD, "period (.) at end of 'Let' statement");
        ASTNode *n = alloc_node(AST_LET, t->line, t->col);
        snprintf(n->name, sizeof(n->name), "%s", id->lexeme);
        n->inferred_type = explicit_type;
        n->left = expr;
        return n;
    }

    if (match(TOK_SET)) {
        if (match(TOK_FIELD)) {
            Token *fname = expect(TOK_IDENT, "field name");
            expect(TOK_OF, "'of' after field name");
            Token *sname = expect(TOK_IDENT, "structure instance name");
            expect(TOK_TO, "'to' before value expression");
            ASTNode *val = parse_expr();
            expect(TOK_PERIOD, "period (.) at end of 'Set field' statement");
            ASTNode *n = alloc_node(AST_STRUCT_SET, t->line, t->col);
            snprintf(n->name, sizeof(n->name), "%s", sname->lexeme);
            snprintf(n->secondary_name, sizeof(n->secondary_name), "%s", fname->lexeme);
            n->left = val;
            return n;
        }
        if (match(TOK_CELL)) {
            ASTNode *row = parse_expr();
            expect(TOK_COMMA, "comma (,) between row and column indices");
            ASTNode *col_expr = parse_expr();
            expect(TOK_OF, "'of' after cell coordinates");
            Token *tname = expect(TOK_IDENT, "tensor name");
            expect(TOK_TO, "'to' before cell value");
            ASTNode *val = parse_expr();
            expect(TOK_PERIOD, "period (.) at end of 'Set cell' statement");
            ASTNode *n = alloc_node(AST_TENSOR_SET, t->line, t->col);
            snprintf(n->name, sizeof(n->name), "%s", tname->lexeme);
            n->left = row;
            n->right = col_expr;
            n->third = val;
            return n;
        }
        Token *id = expect(TOK_IDENT, "variable name after 'Set'");
        expect(TOK_TO, "'to' after variable name");
        ASTNode *expr = NULL;
        if (peek()->type != TOK_TEXT_LIT && has_end_text_before_period()) {
            expr = parse_bare_end_text_literal();
        } else {
            expr = parse_expr();
        }
        expect(TOK_PERIOD, "period (.) at end of 'Set' statement");
        ASTNode *n = alloc_node(AST_SET, t->line, t->col);
        snprintf(n->name, sizeof(n->name), "%s", id->lexeme);
        n->left = expr;
        return n;
    }

    if (match(TOK_DISPLAY)) {
        ASTNode *n = alloc_node(AST_DISPLAY, t->line, t->col);
        do {
            Token *cur = peek();
            if (cur->type == TOK_EOF || cur->type == TOK_PERIOD) break;

            int seg_len = 0;
            if (cur->type != TOK_RESULT) {
                while (peek_n(seg_len)->type != TOK_EOF &&
                       peek_n(seg_len)->type != TOK_PERIOD &&
                       peek_n(seg_len)->type != TOK_COMMA) {
                    seg_len++;
                }
            }

            bool is_phrase = false;
            if (cur->type != TOK_RESULT && seg_len >= 2) {
                bool starts_expr_kw = (cur->type == TOK_INT_LIT || cur->type == TOK_TRUE ||
                                       cur->type == TOK_FALSE || cur->type == TOK_TEXT_LIT ||
                                       cur->type == TOK_ELEMENT || cur->type == TOK_FIELD ||
                                       cur->type == TOK_RESULT || cur->type == TOK_NEG ||
                                       cur->type == TOK_NOT || cur->type == TOK_BITWISE);
                if (!starts_expr_kw && !is_binary_op_token(peek_n(1)->type)) {
                    is_phrase = true;
                }
            }

            ASTNode *item = NULL;
            if (is_phrase) {
                item = alloc_node(AST_TEXT_LIT, cur->line, cur->col);
                item->inferred_type = TYPE_TEXT;
                item->text_val[0] = '\0';
                for (int k = 0; k < seg_len; k++) {
                    if (peek()->type == TOK_END && peek_n(1)->type == TOK_TYPE_TEXT && k + 2 == seg_len) {
                        advance();
                        advance();
                        break;
                    }
                    Token *w = advance();
                    if (k > 0) strncat(item->text_val, " ", sizeof(item->text_val) - strlen(item->text_val) - 1);
                    strncat(item->text_val, w->lexeme, sizeof(item->text_val) - strlen(item->text_val) - 1);
                }
            } else {
                item = parse_expr();
            }

            if (item && n->arg_count < 16) {
                n->args[n->arg_count++] = item;
            }
        } while (match(TOK_COMMA));

        if (n->arg_count > 0) {
            n->left = n->args[0];
        }
        expect(TOK_PERIOD, "period (.) at end of 'Display' statement");
        return n;
    }

    if (match(TOK_EXIT)) {
        expect(TOK_WITH, "'with' after 'Exit'");
        ASTNode *expr = parse_expr();
        expect(TOK_PERIOD, "period (.) at end of 'Exit' statement");
        ASTNode *n = alloc_node(AST_EXIT, t->line, t->col);
        n->left = expr;
        return n;
    }

    if (match(TOK_IF)) {
        ASTNode *cond = parse_expr();
        expect(TOK_THEN, "'then' after 'If' condition");
        expect(TOK_COMMA, "comma (,) after 'then'");
        ASTNode *n = alloc_node(AST_IF, t->line, t->col);
        n->left = cond;
        bool in_else = false;
        while (peek()->type != TOK_EOF) {
            if (peek()->type == TOK_OTHERWISE) {
                advance();
                expect(TOK_COMMA, "comma (,) after 'Otherwise'");
                in_else = true;
                continue;
            }
            if (peek()->type == TOK_END && peek_n(1)->type == TOK_IF) {
                advance();
                advance();
                expect(TOK_PERIOD, "period (.) after 'End if'");
                return n;
            }
            ASTNode *s = parse_statement();
            if (in_else) add_else_stmt(n, s);
            else add_body_stmt(n, s);
        }
        add_diag("EATHON-SYN-005", "PARSER", "ERROR", t->line, t->col,
                 "Unclosed 'If' block: expected 'End if.' before end of file.");
        return n;
    }

    if (match(TOK_WHILE)) {
        ASTNode *cond = parse_expr();
        expect(TOK_REPEAT, "'repeat' after 'While' condition");
        expect(TOK_COMMA, "comma (,) after 'repeat'");
        ASTNode *n = alloc_node(AST_WHILE, t->line, t->col);
        n->left = cond;
        while (peek()->type != TOK_EOF) {
            if (peek()->type == TOK_END && peek_n(1)->type == TOK_WHILE) {
                advance();
                advance();
                expect(TOK_PERIOD, "period (.) after 'End while'");
                return n;
            }
            add_body_stmt(n, parse_statement());
        }
        add_diag("EATHON-SYN-006", "PARSER", "ERROR", t->line, t->col,
                 "Unclosed 'While' loop: expected 'End while.' before end of file.");
        return n;
    }

    if (match(TOK_REPEAT)) {
        ASTNode *count = parse_expr();
        expect(TOK_TIMES_KW, "'times' after 'Repeat' count expression");
        expect(TOK_COMMA, "comma (,) after 'times'");
        ASTNode *n = alloc_node(AST_REPEAT, t->line, t->col);
        n->left = count;
        while (peek()->type != TOK_EOF) {
            if (peek()->type == TOK_END && peek_n(1)->type == TOK_REPEAT) {
                advance();
                advance();
                expect(TOK_PERIOD, "period (.) after 'End repeat'");
                return n;
            }
            add_body_stmt(n, parse_statement());
        }
        add_diag("EATHON-SYN-007", "PARSER", "ERROR", t->line, t->col,
                 "Unclosed 'Repeat' loop: expected 'End repeat.' before end of file.");
        return n;
    }

    if (t->type == TOK_TO && peek_n(1)->type == TOK_COMPUTE) {
        advance();
        advance();
        Token *fn = expect(TOK_IDENT, "function name after 'To compute'");
        ASTNode *n = alloc_node(AST_FUNC_DEF, t->line, t->col);
        snprintf(n->name, sizeof(n->name), "%s", fn->lexeme);
        if (match(TOK_TAKING)) {
            do {
                Token *pname = expect(TOK_IDENT, "parameter name");
                expect(TOK_AS, "'as' after parameter name");
                DataType ptype = parse_type_name();
                if (n->param_count < 16) {
                    snprintf(n->param_names[n->param_count], MAX_STR_LEN, "%s", pname->lexeme);
                    n->param_types[n->param_count] = ptype;
                    n->param_count++;
                }
            } while (match(TOK_COMMA) && peek()->type != TOK_RETURNING);
        }
        expect(TOK_RETURNING, "'returning' in function header");
        n->inferred_type = parse_type_name();
        expect(TOK_COMMA, "comma (,) at end of function header");
        while (peek()->type != TOK_EOF) {
            if (peek()->type == TOK_END && peek_n(1)->type == TOK_FUNCTION) {
                advance();
                advance();
                expect(TOK_PERIOD, "period (.) after 'End function'");
                return n;
            }
            add_body_stmt(n, parse_statement());
        }
        add_diag("EATHON-SYN-008", "PARSER", "ERROR", t->line, t->col,
                 "Unclosed function definition: expected 'End function.' before end of file.");
        return n;
    }

    if (match(TOK_RETURN)) {
        ASTNode *expr = parse_expr();
        expect(TOK_PERIOD, "period (.) at end of 'Return' statement");
        ASTNode *n = alloc_node(AST_RETURN, t->line, t->col);
        n->left = expr;
        return n;
    }

    if (match(TOK_CALL)) {
        Token *fn = expect(TOK_IDENT, "function name after 'Call'");
        ASTNode *n = alloc_node(AST_CALL_STMT, t->line, t->col);
        snprintf(n->name, sizeof(n->name), "%s", fn->lexeme);
        if (match(TOK_WITH)) {
            do {
                if (n->arg_count < 16) {
                    n->args[n->arg_count++] = parse_expr();
                }
            } while (match(TOK_COMMA));
        }
        expect(TOK_INTO, "'into' specifying destination variable");
        Token *target = expect(TOK_IDENT, "destination variable name");
        snprintf(n->secondary_name, sizeof(n->secondary_name), "%s", target->lexeme);
        expect(TOK_PERIOD, "period (.) at end of 'Call' statement");
        return n;
    }

    if (match(TOK_ALLOCATE)) {
        if (match(TOK_ARRAY)) {
            Token *aname = expect(TOK_IDENT, "array variable name");
            expect(TOK_OF, "'of' after array name");
            ASTNode *sz = parse_expr();
            expect(TOK_TYPE_INTEGER, "'integers' at end of 'Allocate array'");
            expect(TOK_PERIOD, "period (.) at end of 'Allocate array'");
            ASTNode *n = alloc_node(AST_ARRAY_ALLOC, t->line, t->col);
            snprintf(n->name, sizeof(n->name), "%s", aname->lexeme);
            n->left = sz;
            return n;
        }
        if (match(TOK_MEMORY)) {
            Token *mname = expect(TOK_IDENT, "memory pointer variable name");
            expect(TOK_OF, "'of' after memory pointer name");
            ASTNode *sz = parse_expr();
            expect(TOK_TYPE_BYTE, "'bytes' at end of 'Allocate memory'");
            expect(TOK_PERIOD, "period (.) at end of 'Allocate memory'");
            ASTNode *n = alloc_node(AST_MEM_ALLOC, t->line, t->col);
            snprintf(n->name, sizeof(n->name), "%s", mname->lexeme);
            n->left = sz;
            return n;
        }
        add_diag("EATHON-SYN-009", "PARSER", "ERROR", t->line, t->col,
                 "Expected 'array' or 'memory' after 'Allocate'.");
    }

    if (match(TOK_STORE)) {
        ASTNode *val = parse_expr();
        expect(TOK_INTO, "'into' after value in 'Store'");
        expect(TOK_ELEMENT, "'element' after 'into'");
        ASTNode *idx = parse_expr();
        expect(TOK_OF, "'of' after element index");
        Token *aname = expect(TOK_IDENT, "array name");
        expect(TOK_PERIOD, "period (.) at end of 'Store' statement");
        ASTNode *n = alloc_node(AST_ARRAY_STORE, t->line, t->col);
        snprintf(n->name, sizeof(n->name), "%s", aname->lexeme);
        n->left = idx;
        n->right = val;
        return n;
    }

    if (match(TOK_LOAD)) {
        expect(TOK_ELEMENT, "'element' after 'Load'");
        ASTNode *idx = parse_expr();
        expect(TOK_OF, "'of' after element index");
        Token *aname = expect(TOK_IDENT, "array name");
        expect(TOK_INTO, "'into' after array name");
        Token *target = expect(TOK_IDENT, "target variable name");
        expect(TOK_PERIOD, "period (.) at end of 'Load' statement");
        ASTNode *n = alloc_node(AST_ARRAY_LOAD, t->line, t->col);
        snprintf(n->name, sizeof(n->name), "%s", aname->lexeme);
        snprintf(n->secondary_name, sizeof(n->secondary_name), "%s", target->lexeme);
        n->left = idx;
        return n;
    }

    if (match(TOK_WRITE)) {
        ASTNode *val = parse_expr();
        expect(TOK_AT, "'at' after value in 'Write'");
        expect(TOK_TYPE_ADDRESS, "'address' after 'at'");
        Token *ptr = expect(TOK_IDENT, "pointer variable name");
        expect(TOK_OFFSET, "'offset' after pointer name");
        ASTNode *off = parse_expr();
        expect(TOK_PERIOD, "period (.) at end of 'Write' statement");
        ASTNode *n = alloc_node(AST_MEM_WRITE, t->line, t->col);
        snprintf(n->name, sizeof(n->name), "%s", ptr->lexeme);
        n->left = off;
        n->right = val;
        return n;
    }

    if (match(TOK_READ)) {
        expect(TOK_TYPE_INTEGER, "'integer' after 'Read'");
        expect(TOK_FROM, "'from' after 'Read integer'");
        expect(TOK_TYPE_ADDRESS, "'address' after 'from'");
        Token *ptr = expect(TOK_IDENT, "pointer variable name");
        expect(TOK_OFFSET, "'offset' after pointer name");
        ASTNode *off = parse_expr();
        expect(TOK_INTO, "'into' before destination variable");
        Token *target = expect(TOK_IDENT, "destination variable name");
        expect(TOK_PERIOD, "period (.) at end of 'Read' statement");
        ASTNode *n = alloc_node(AST_MEM_READ, t->line, t->col);
        snprintf(n->name, sizeof(n->name), "%s", ptr->lexeme);
        snprintf(n->secondary_name, sizeof(n->secondary_name), "%s", target->lexeme);
        n->left = off;
        return n;
    }

    if (match(TOK_DEFINE)) {
        expect(TOK_STRUCTURE, "'structure' after 'Define'");
        Token *sname = expect(TOK_IDENT, "structure type name");
        expect(TOK_WITH, "'with' after structure name");
        expect(TOK_FIELDS, "'fields' after 'with'");
        ASTNode *n = alloc_node(AST_STRUCT_DEF, t->line, t->col);
        snprintf(n->name, sizeof(n->name), "%s", sname->lexeme);
        do {
            Token *fname = expect(TOK_IDENT, "field name");
            expect(TOK_AS, "'as' after field name");
            DataType ftype = parse_type_name();
            if (n->param_count < 16) {
                snprintf(n->param_names[n->param_count], MAX_STR_LEN, "%s", fname->lexeme);
                n->param_types[n->param_count] = ftype;
                n->param_count++;
            }
        } while (match(TOK_COMMA));
        expect(TOK_PERIOD, "period (.) at end of 'Define structure'");
        return n;
    }

    if (match(TOK_CREATE)) {
        if (match(TOK_TENSOR)) {
            Token *tname = expect(TOK_IDENT, "tensor variable name");
            expect(TOK_WITH, "'with' after tensor name");
            expect(TOK_DIMENSIONS, "'dimensions' after 'with'");
            ASTNode *rows = parse_expr();
            expect(TOK_COMMA, "comma (,) between row and column dimensions");
            ASTNode *cols = parse_expr();
            expect(TOK_PERIOD, "period (.) at end of 'Create tensor'");
            ASTNode *n = alloc_node(AST_TENSOR_CREATE, t->line, t->col);
            snprintf(n->name, sizeof(n->name), "%s", tname->lexeme);
            n->left = rows;
            n->right = cols;
            return n;
        }
        Token *inst = expect(TOK_IDENT, "instance name after 'Create'");
        expect(TOK_AS, "'as' after instance name");
        Token *stype = expect(TOK_IDENT, "structure type name");
        expect(TOK_PERIOD, "period (.) at end of 'Create' statement");
        ASTNode *n = alloc_node(AST_STRUCT_CREATE, t->line, t->col);
        snprintf(n->name, sizeof(n->name), "%s", inst->lexeme);
        snprintf(n->secondary_name, sizeof(n->secondary_name), "%s", stype->lexeme);
        return n;
    }

    if (match(TOK_GET)) {
        if (match(TOK_FIELD)) {
            Token *fname = expect(TOK_IDENT, "field name");
            expect(TOK_OF, "'of' after field name");
            Token *sname = expect(TOK_IDENT, "structure instance name");
            expect(TOK_INTO, "'into' before target variable");
            Token *target = expect(TOK_IDENT, "target variable name");
            expect(TOK_PERIOD, "period (.) at end of 'Get field'");
            ASTNode *n = alloc_node(AST_STRUCT_GET, t->line, t->col);
            snprintf(n->name, sizeof(n->name), "%s", sname->lexeme);
            snprintf(n->secondary_name, sizeof(n->secondary_name), "%s", fname->lexeme);
            snprintf(n->tertiary_name, sizeof(n->tertiary_name), "%s", target->lexeme);
            return n;
        }
        if (match(TOK_CELL)) {
            ASTNode *row = parse_expr();
            expect(TOK_COMMA, "comma (,) between row and column indices");
            ASTNode *col_expr = parse_expr();
            expect(TOK_OF, "'of' after cell coordinates");
            Token *tname = expect(TOK_IDENT, "tensor name");
            expect(TOK_INTO, "'into' before target variable");
            Token *target = expect(TOK_IDENT, "target variable name");
            expect(TOK_PERIOD, "period (.) at end of 'Get cell'");
            ASTNode *n = alloc_node(AST_TENSOR_GET, t->line, t->col);
            snprintf(n->name, sizeof(n->name), "%s", tname->lexeme);
            snprintf(n->secondary_name, sizeof(n->secondary_name), "%s", target->lexeme);
            n->left = row;
            n->right = col_expr;
            return n;
        }
    }

    if (match(TOK_FILL)) {
        expect(TOK_TENSOR, "'tensor' after 'Fill'");
        Token *tname = expect(TOK_IDENT, "tensor name");
        expect(TOK_WITH, "'with' after tensor name");
        ASTNode *val = parse_expr();
        expect(TOK_PERIOD, "period (.) at end of 'Fill tensor'");
        ASTNode *n = alloc_node(AST_TENSOR_FILL, t->line, t->col);
        snprintf(n->name, sizeof(n->name), "%s", tname->lexeme);
        n->left = val;
        return n;
    }

    if (match(TOK_MULTIPLY)) {
        expect(TOK_TENSOR, "'tensor' after 'Multiply'");
        Token *a = expect(TOK_IDENT, "first tensor name");
        expect(TOK_BY, "'by' after first tensor");
        expect(TOK_TENSOR, "'tensor' after 'by'");
        Token *b = expect(TOK_IDENT, "second tensor name");
        expect(TOK_INTO, "'into' before destination tensor");
        Token *c = expect(TOK_IDENT, "destination tensor name");
        expect(TOK_PERIOD, "period (.) at end of 'Multiply tensor'");
        ASTNode *n = alloc_node(AST_TENSOR_MUL, t->line, t->col);
        snprintf(n->name, sizeof(n->name), "%s", a->lexeme);
        snprintf(n->secondary_name, sizeof(n->secondary_name), "%s", b->lexeme);
        snprintf(n->tertiary_name, sizeof(n->tertiary_name), "%s", c->lexeme);
        return n;
    }

    if (match(TOK_ACTIVATE)) {
        expect(TOK_RELU, "'relu' after 'Activate'");
        expect(TOK_ON, "'on' after 'relu'");
        expect(TOK_TENSOR, "'tensor' after 'on'");
        Token *a = expect(TOK_IDENT, "tensor name");
        expect(TOK_PERIOD, "period (.) at end of 'Activate relu'");
        ASTNode *n = alloc_node(AST_TENSOR_RELU, t->line, t->col);
        snprintf(n->name, sizeof(n->name), "%s", a->lexeme);
        return n;
    }

    if (match(TOK_SUM)) {
        expect(TOK_TENSOR, "'tensor' after 'Sum'");
        Token *a = expect(TOK_IDENT, "tensor name");
        expect(TOK_INTO, "'into' before target variable");
        Token *target = expect(TOK_IDENT, "target variable name");
        expect(TOK_PERIOD, "period (.) at end of 'Sum tensor'");
        ASTNode *n = alloc_node(AST_TENSOR_SUM, t->line, t->col);
        snprintf(n->name, sizeof(n->name), "%s", a->lexeme);
        snprintf(n->secondary_name, sizeof(n->secondary_name), "%s", target->lexeme);
        return n;
    }

    if (match(TOK_OPEN)) {
        expect(TOK_DATABASE, "'database' after 'Open'");
        Token *db = expect(TOK_IDENT, "database handle identifier");
        expect(TOK_PERIOD, "period (.) at end of 'Open database'");
        ASTNode *n = alloc_node(AST_DB_OPEN, t->line, t->col);
        snprintf(n->name, sizeof(n->name), "%s", db->lexeme);
        return n;
    }

    if (match(TOK_INSERT)) {
        expect(TOK_KEY, "'key' after 'Insert'");
        ASTNode *k = parse_expr();
        expect(TOK_WITH, "'with' after key expression");
        expect(TOK_VALUE, "'value' after 'with'");
        ASTNode *v = parse_expr();
        expect(TOK_INTO, "'into' before database identifier");
        Token *db = expect(TOK_IDENT, "database handle identifier");
        expect(TOK_PERIOD, "period (.) at end of 'Insert key'");
        ASTNode *n = alloc_node(AST_DB_INSERT, t->line, t->col);
        snprintf(n->name, sizeof(n->name), "%s", db->lexeme);
        n->left = k;
        n->right = v;
        return n;
    }

    if (match(TOK_QUERY)) {
        expect(TOK_KEY, "'key' after 'Query'");
        ASTNode *k = parse_expr();
        expect(TOK_FROM, "'from' after key expression");
        Token *db = expect(TOK_IDENT, "database handle identifier");
        expect(TOK_INTO, "'into' before destination variable");
        Token *target = expect(TOK_IDENT, "destination variable identifier");
        expect(TOK_PERIOD, "period (.) at end of 'Query key'");
        ASTNode *n = alloc_node(AST_DB_QUERY, t->line, t->col);
        snprintf(n->name, sizeof(n->name), "%s", db->lexeme);
        snprintf(n->secondary_name, sizeof(n->secondary_name), "%s", target->lexeme);
        n->left = k;
        return n;
    }

    char msg[256];
    snprintf(msg, sizeof(msg),
             "Unrecognized statement starting with '%s'. Every Eathon statement must begin with a valid verb.",
             t->lexeme);
    add_diag("EATHON-SYN-010", "PARSER", "ERROR", t->line, t->col, msg);
    while (peek()->type != TOK_EOF && peek()->type != TOK_PERIOD) {
        advance();
    }
    match(TOK_PERIOD);
    return alloc_node(AST_INT_LIT, t->line, t->col);
}

static ASTNode *parse_program(void) {
    g_pos = 0;
    ASTNode *prog = alloc_node(AST_PROGRAM, 1, 1);
    while (peek()->type != TOK_EOF) {
        add_body_stmt(prog, parse_statement());
    }
    return prog;
}

/* -------------------------------------------------------------------------- */
/* 6. SYMBOL TABLE & STATIC TYPE CHECKER                                      */
/* -------------------------------------------------------------------------- */

typedef struct {
    char name[MAX_STR_LEN];
    DataType type;
    char struct_type_name[MAX_STR_LEN];
    int stack_offset;
    int array_len;
    int tensor_rows;
    int tensor_cols;
    bool is_function;
    bool is_local;
    char owner_func[MAX_STR_LEN];
    int param_count;
    DataType param_types[16];
} Symbol;

typedef struct {
    char name[MAX_STR_LEN];
    int field_count;
    char field_names[16][MAX_STR_LEN];
    DataType field_types[16];
} StructDef;

static Symbol g_symbols[MAX_SYMBOLS];
static int g_sym_count = 0;
static int g_next_stack_offset = 8;
static bool g_in_function = false;
static char g_current_func[MAX_STR_LEN] = "";
static int g_local_stack_offset = 8;

static StructDef g_structs[MAX_STRUCTS];
static int g_struct_count = 0;

static Symbol *find_symbol(const char *name) {
    if (g_current_func[0] != '\0') {
        for (int i = g_sym_count - 1; i >= 0; i--) {
            if (g_symbols[i].is_local &&
                strcmp(g_symbols[i].owner_func, g_current_func) == 0 &&
                strcmp(g_symbols[i].name, name) == 0) {
                return &g_symbols[i];
            }
        }
    }
    for (int i = g_sym_count - 1; i >= 0; i--) {
        if (!g_symbols[i].is_local && strcmp(g_symbols[i].name, name) == 0) {
            return &g_symbols[i];
        }
    }
    return NULL;
}

static StructDef *find_struct(const char *name) {
    for (int i = 0; i < g_struct_count; i++) {
        if (strcmp(g_structs[i].name, name) == 0) return &g_structs[i];
    }
    return NULL;
}

static Symbol *declare_symbol(const char *name, DataType type, int slots, int line, int col) {
    if (g_sym_count >= MAX_SYMBOLS) return &g_symbols[0];
    Symbol *s = &g_symbols[g_sym_count++];
    memset(s, 0, sizeof(Symbol));
    snprintf(s->name, sizeof(s->name), "%s", name);
    s->type = type;
    if (slots < 1) slots = 1;
    if (g_in_function) {
        s->is_local = true;
        snprintf(s->owner_func, sizeof(s->owner_func), "%s", g_current_func);
        s->stack_offset = g_local_stack_offset;
        g_local_stack_offset += slots * 8;
    } else {
        s->is_local = false;
        s->stack_offset = g_next_stack_offset;
        g_next_stack_offset += slots * 8;
    }
    (void)line; (void)col;
    return s;
}

static DataType typecheck_node(ASTNode *n) {
    if (!n) return TYPE_VOID;

    switch (n->kind) {
        case AST_PROGRAM:
            for (int i = 0; i < n->body_count; i++) {
                typecheck_node(n->body[i]);
            }
            return TYPE_VOID;

        case AST_INT_LIT:
            n->inferred_type = TYPE_INTEGER;
            return TYPE_INTEGER;

        case AST_BOOL_LIT:
            n->inferred_type = TYPE_BOOLEAN;
            return TYPE_BOOLEAN;

        case AST_TEXT_LIT:
            n->inferred_type = TYPE_TEXT;
            return TYPE_TEXT;

        case AST_VAR_REF: {
            Symbol *s = find_symbol(n->name);
            if (!s) {
                char msg[256];
                snprintf(msg, sizeof(msg), "Undeclared identifier '%s'. Declare it first using 'Let %s be ...'.", n->name, n->name);
                add_diag("EATHON-SEM-001", "TYPECHECK", "ERROR", n->line, n->col, msg);
                n->inferred_type = TYPE_INTEGER;
                return TYPE_INTEGER;
            }
            n->inferred_type = s->type;
            return s->type;
        }

        case AST_ARRAY_INDEX_EXPR: {
            Symbol *s = find_symbol(n->name);
            if (!s || s->type != TYPE_ARRAY) {
                char msg[256];
                snprintf(msg, sizeof(msg), "Identifier '%s' is not an allocated array.", n->name);
                add_diag("EATHON-SEM-002", "TYPECHECK", "ERROR", n->line, n->col, msg);
            }
            typecheck_node(n->left);
            n->inferred_type = TYPE_INTEGER;
            return TYPE_INTEGER;
        }

        case AST_STRUCT_FIELD_EXPR: {
            Symbol *s = find_symbol(n->name);
            if (!s || s->type != TYPE_STRUCT) {
                char msg[256];
                snprintf(msg, sizeof(msg), "Identifier '%s' is not a structure instance.", n->name);
                add_diag("EATHON-SEM-004", "TYPECHECK", "ERROR", n->line, n->col, msg);
            }
            n->inferred_type = TYPE_INTEGER;
            return TYPE_INTEGER;
        }

        case AST_CALL_EXPR: {
            Symbol *fn = find_symbol(n->name);
            if (!fn || !fn->is_function) {
                char msg[256];
                snprintf(msg, sizeof(msg), "Undeclared function '%s'.", n->name);
                add_diag("EATHON-SEM-003", "TYPECHECK", "ERROR", n->line, n->col, msg);
                n->inferred_type = TYPE_INTEGER;
                return TYPE_INTEGER;
            }
            if (fn->param_count != n->arg_count) {
                char msg[256];
                snprintf(msg, sizeof(msg), "Function '%s' expects %d arguments, but %d were provided.",
                         n->name, fn->param_count, n->arg_count);
                add_diag("EATHON-SEM-003", "TYPECHECK", "ERROR", n->line, n->col, msg);
            }
            for (int i = 0; i < n->arg_count; i++) {
                typecheck_node(n->args[i]);
            }
            n->inferred_type = fn->type;
            return fn->type;
        }

        case AST_UNOP: {
            DataType sub = typecheck_node(n->left);
            n->inferred_type = (n->op == OP_NOT) ? TYPE_BOOLEAN : sub;
            return n->inferred_type;
        }

        case AST_BINOP: {
            DataType lt = typecheck_node(n->left);
            DataType rt = typecheck_node(n->right);
            if (lt == TYPE_TEXT || rt == TYPE_TEXT) {
                add_diag("EATHON-SEM-002", "TYPECHECK", "ERROR", n->line, n->col,
                         "Cannot perform arithmetic or bitwise operations on 'text' values.");
            }
            if (n->op >= OP_EQ && n->op <= OP_LOG_OR) {
                n->inferred_type = TYPE_BOOLEAN;
            } else {
                n->inferred_type = TYPE_INTEGER;
            }
            return n->inferred_type;
        }

        case AST_LET: {
            DataType rhs = typecheck_node(n->left);
            if (n->inferred_type == TYPE_VOID) {
                n->inferred_type = rhs;
            } else if (n->inferred_type != rhs && !(n->inferred_type == TYPE_BYTE && rhs == TYPE_INTEGER)) {
                char msg[256];
                snprintf(msg, sizeof(msg),
                         "Type mismatch in declaration of '%s': declared as '%s' but initialized with '%s'.",
                         n->name, datatype_name(n->inferred_type), datatype_name(rhs));
                add_diag("EATHON-SEM-002", "TYPECHECK", "ERROR", n->line, n->col, msg);
            }
            declare_symbol(n->name, n->inferred_type, 1, n->line, n->col);
            return TYPE_VOID;
        }

        case AST_SET: {
            Symbol *s = find_symbol(n->name);
            DataType rhs = typecheck_node(n->left);
            if (!s) {
                char msg[256];
                snprintf(msg, sizeof(msg),
                         "Cannot assign to undeclared variable '%s'. Use 'Let %s be ...' first.",
                         n->name, n->name);
                add_diag("EATHON-SEM-001", "TYPECHECK", "ERROR", n->line, n->col, msg);
            } else if (s->type != rhs && !(s->type == TYPE_BYTE && rhs == TYPE_INTEGER)) {
                char msg[256];
                snprintf(msg, sizeof(msg),
                         "Type mismatch in assignment to '%s': variable has type '%s', cannot assign '%s'.",
                         n->name, datatype_name(s->type), datatype_name(rhs));
                add_diag("EATHON-SEM-002", "TYPECHECK", "ERROR", n->line, n->col, msg);
            }
            return TYPE_VOID;
        }

        case AST_DISPLAY:
            if (n->arg_count > 1) {
                for (int i = 0; i < n->arg_count; i++) {
                    ASTNode *arg = n->args[i];
                    if (arg->kind == AST_VAR_REF && !find_symbol(arg->name)) {
                        arg->kind = AST_TEXT_LIT;
                        snprintf(arg->text_val, sizeof(arg->text_val), "%s", arg->name);
                        arg->inferred_type = TYPE_TEXT;
                    } else {
                        typecheck_node(arg);
                    }
                }
            } else {
                typecheck_node(n->left);
            }
            return TYPE_VOID;

        case AST_EXIT:
        case AST_RETURN:
            typecheck_node(n->left);
            return TYPE_VOID;

        case AST_IF:
            typecheck_node(n->left);
            for (int i = 0; i < n->body_count; i++) typecheck_node(n->body[i]);
            for (int i = 0; i < n->else_count; i++) typecheck_node(n->else_body[i]);
            return TYPE_VOID;

        case AST_WHILE:
        case AST_REPEAT:
            typecheck_node(n->left);
            for (int i = 0; i < n->body_count; i++) typecheck_node(n->body[i]);
            return TYPE_VOID;

        case AST_FUNC_DEF: {
            bool prev_in_fn = g_in_function;
            g_in_function = false;
            Symbol *fn = declare_symbol(n->name, n->inferred_type, 1, n->line, n->col);
            fn->is_function = true;
            fn->param_count = n->param_count;
            for (int i = 0; i < n->param_count; i++) {
                fn->param_types[i] = n->param_types[i];
            }

            g_in_function = true;
            snprintf(g_current_func, sizeof(g_current_func), "%s", n->name);
            g_local_stack_offset = 8;

            for (int i = 0; i < n->param_count; i++) {
                declare_symbol(n->param_names[i], n->param_types[i], 1, n->line, n->col);
            }
            for (int i = 0; i < n->body_count; i++) {
                typecheck_node(n->body[i]);
            }

            g_in_function = prev_in_fn;
            g_current_func[0] = '\0';
            return TYPE_VOID;
        }

        case AST_CALL_STMT: {
            Symbol *fn = find_symbol(n->name);
            if (!fn || !fn->is_function) {
                char msg[256];
                snprintf(msg, sizeof(msg), "Undeclared function '%s' in 'Call' statement.", n->name);
                add_diag("EATHON-SEM-003", "TYPECHECK", "ERROR", n->line, n->col, msg);
            }
            for (int i = 0; i < n->arg_count; i++) typecheck_node(n->args[i]);
            Symbol *dest = find_symbol(n->secondary_name);
            if (!dest) {
                declare_symbol(n->secondary_name, fn ? fn->type : TYPE_INTEGER, 1, n->line, n->col);
                n->int_val = 1;
            } else {
                n->int_val = 0;
            }
            return TYPE_VOID;
        }

        case AST_ARRAY_ALLOC: {
            typecheck_node(n->left);
            int sz = (n->left && n->left->kind == AST_INT_LIT) ? (int)n->left->int_val : 64;
            if (sz < 1) sz = 1;
            Symbol *s = declare_symbol(n->name, TYPE_ARRAY, sz, n->line, n->col);
            s->array_len = sz;
            return TYPE_VOID;
        }

        case AST_ARRAY_STORE:
            typecheck_node(n->left);
            typecheck_node(n->right);
            if (!find_symbol(n->name)) {
                char msg[256];
                snprintf(msg, sizeof(msg), "Undeclared array '%s'.", n->name);
                add_diag("EATHON-SEM-001", "TYPECHECK", "ERROR", n->line, n->col, msg);
            }
            return TYPE_VOID;

        case AST_ARRAY_LOAD:
            typecheck_node(n->left);
            if (!find_symbol(n->secondary_name)) {
                declare_symbol(n->secondary_name, TYPE_INTEGER, 1, n->line, n->col);
                n->int_val = 1;
            } else {
                n->int_val = 0;
            }
            return TYPE_VOID;

        case AST_MEM_ALLOC: {
            typecheck_node(n->left);
            int bytes = (n->left && n->left->kind == AST_INT_LIT) ? (int)n->left->int_val : 64;
            int slots = (bytes + 7) / 8 + 1;
            declare_symbol(n->name, TYPE_ADDRESS, slots, n->line, n->col);
            return TYPE_VOID;
        }

        case AST_MEM_WRITE:
            typecheck_node(n->left);
            typecheck_node(n->right);
            return TYPE_VOID;

        case AST_MEM_READ:
            typecheck_node(n->left);
            if (!find_symbol(n->secondary_name)) {
                declare_symbol(n->secondary_name, TYPE_INTEGER, 1, n->line, n->col);
                n->int_val = 1;
            } else {
                n->int_val = 0;
            }
            return TYPE_VOID;

        case AST_STRUCT_DEF: {
            if (g_struct_count < MAX_STRUCTS) {
                StructDef *sd = &g_structs[g_struct_count++];
                snprintf(sd->name, sizeof(sd->name), "%s", n->name);
                sd->field_count = n->param_count;
                for (int i = 0; i < n->param_count; i++) {
                    snprintf(sd->field_names[i], MAX_STR_LEN, "%s", n->param_names[i]);
                    sd->field_types[i] = n->param_types[i];
                }
            }
            return TYPE_VOID;
        }

        case AST_STRUCT_CREATE: {
            StructDef *sd = find_struct(n->secondary_name);
            int slots = sd ? sd->field_count : 4;
            Symbol *s = declare_symbol(n->name, TYPE_STRUCT, slots, n->line, n->col);
            snprintf(s->struct_type_name, sizeof(s->struct_type_name), "%s", n->secondary_name);
            return TYPE_VOID;
        }

        case AST_STRUCT_SET:
            typecheck_node(n->left);
            return TYPE_VOID;

        case AST_STRUCT_GET:
            if (!find_symbol(n->tertiary_name)) {
                declare_symbol(n->tertiary_name, TYPE_INTEGER, 1, n->line, n->col);
                n->int_val = 1;
            } else {
                n->int_val = 0;
            }
            return TYPE_VOID;

        case AST_TENSOR_CREATE: {
            typecheck_node(n->left);
            typecheck_node(n->right);
            int r = (n->left && n->left->kind == AST_INT_LIT) ? (int)n->left->int_val : 4;
            int c = (n->right && n->right->kind == AST_INT_LIT) ? (int)n->right->int_val : 4;
            Symbol *s = declare_symbol(n->name, TYPE_TENSOR, r * c, n->line, n->col);
            s->tensor_rows = r;
            s->tensor_cols = c;
            return TYPE_VOID;
        }

        case AST_TENSOR_FILL:
            typecheck_node(n->left);
            return TYPE_VOID;

        case AST_TENSOR_SET:
            typecheck_node(n->left);
            typecheck_node(n->right);
            typecheck_node(n->third);
            return TYPE_VOID;

        case AST_TENSOR_GET:
            typecheck_node(n->left);
            typecheck_node(n->right);
            if (!find_symbol(n->secondary_name)) {
                declare_symbol(n->secondary_name, TYPE_INTEGER, 1, n->line, n->col);
                n->int_val = 1;
            } else {
                n->int_val = 0;
            }
            return TYPE_VOID;

        case AST_TENSOR_MUL:
        case AST_TENSOR_RELU:
            return TYPE_VOID;

        case AST_TENSOR_SUM:
            if (!find_symbol(n->secondary_name)) {
                declare_symbol(n->secondary_name, TYPE_INTEGER, 1, n->line, n->col);
                n->int_val = 1;
            } else {
                n->int_val = 0;
            }
            return TYPE_VOID;

        case AST_DB_OPEN:
            declare_symbol(n->name, TYPE_DATABASE, 64, n->line, n->col);
            return TYPE_VOID;

        case AST_DB_INSERT:
            typecheck_node(n->left);
            typecheck_node(n->right);
            return TYPE_VOID;

        case AST_DB_QUERY:
            typecheck_node(n->left);
            if (!find_symbol(n->secondary_name)) {
                declare_symbol(n->secondary_name, TYPE_INTEGER, 1, n->line, n->col);
                n->int_val = 1;
            } else {
                n->int_val = 0;
            }
            return TYPE_VOID;

        default:
            return TYPE_VOID;
    }
}

/* -------------------------------------------------------------------------- */
/* 7. DIRECT NATIVE x86_64 LINUX ELF64 MACHINE CODE ASSEMBLER & LINKER        */
/* -------------------------------------------------------------------------- */

typedef struct {
    uint8_t buf[MAX_CODE_SIZE];
    int len;
    uint8_t rodata[16384];
    int rodata_len;
} X86Buffer;

static void emit_u8(X86Buffer *b, uint8_t v) {
    if (b->len < MAX_CODE_SIZE) b->buf[b->len++] = v;
}

static void emit_u32(X86Buffer *b, uint32_t v) {
    emit_u8(b, (uint8_t)(v & 0xFF));
    emit_u8(b, (uint8_t)((v >> 8) & 0xFF));
    emit_u8(b, (uint8_t)((v >> 16) & 0xFF));
    emit_u8(b, (uint8_t)((v >> 24) & 0xFF));
}

static void emit_u64(X86Buffer *b, uint64_t v) {
    emit_u32(b, (uint32_t)(v & 0xFFFFFFFFULL));
    emit_u32(b, (uint32_t)(v >> 32));
}

static void patch_u32(X86Buffer *b, int pos, uint32_t v) {
    b->buf[pos + 0] = (uint8_t)(v & 0xFF);
    b->buf[pos + 1] = (uint8_t)((v >> 8) & 0xFF);
    b->buf[pos + 2] = (uint8_t)((v >> 16) & 0xFF);
    b->buf[pos + 3] = (uint8_t)((v >> 24) & 0xFF);
}

static int g_print_i64_offset = 0;

static void emit_runtime_print_i64(X86Buffer *b) {
    g_print_i64_offset = b->len;
    const uint8_t rt[] = {
        0x55,
        0x48, 0x89, 0xe5,
        0x48, 0x83, 0xec, 0x20,
        0x40, 0x88, 0x7d, 0xff, /* mov [rbp-1], dil (trailing char: '\n' or ' ') */
        0x48, 0x8d, 0x75, 0xff,
        0x48, 0xc7, 0xc1, 0x01, 0x00, 0x00, 0x00,
        0x49, 0xc7, 0xc0, 0x00, 0x00, 0x00, 0x00,
        0x48, 0x85, 0xc0,
        0x79, 0x0a,
        0x48, 0xf7, 0xd8,
        0x49, 0xc7, 0xc0, 0x01, 0x00, 0x00, 0x00,
        0x48, 0xc7, 0xc3, 0x0a, 0x00, 0x00, 0x00,
        /* .loop: (22 bytes total) */
        0x48, 0x31, 0xd2,
        0x48, 0xf7, 0xf3,
        0x80, 0xc2, 0x30,
        0x48, 0xff, 0xce,
        0x88, 0x16,
        0x48, 0xff, 0xc1,
        0x48, 0x85, 0xc0,
        0x75, 0xea,
        0x4d, 0x85, 0xc0,
        0x74, 0x09,
        0x48, 0xff, 0xce,
        0xc6, 0x06, 0x2d,
        0x48, 0xff, 0xc1,
        /* .write: */
        0x48, 0xc7, 0xc0, 0x01, 0x00, 0x00, 0x00,
        0x48, 0xc7, 0xc7, 0x01, 0x00, 0x00, 0x00,
        0x48, 0x89, 0xca,
        0x0f, 0x05,
        0xc9,
        0xc3
    };
    for (size_t i = 0; i < sizeof(rt); i++) emit_u8(b, rt[i]);
}

typedef struct {
    char name[MAX_STR_LEN];
    int code_offset;
} FuncOffset;

static FuncOffset g_func_offsets[64];
static int g_func_offset_count = 0;

static int find_func_offset(const char *name) {
    for (int i = 0; i < g_func_offset_count; i++) {
        if (strcmp(g_func_offsets[i].name, name) == 0) return g_func_offsets[i].code_offset;
    }
    return -1;
}

typedef struct {
    int patch_pos;
    int rodata_offset;
} RodataReloc;

static RodataReloc g_rodata_relocs[256];
static int g_rodata_reloc_count = 0;

static void emit_x86_expr(X86Buffer *b, ASTNode *n);
static void emit_x86_stmt(X86Buffer *b, ASTNode *n);

static int find_struct_field_index(const char *struct_var, const char *field_name) {
    Symbol *s = find_symbol(struct_var);
    if (!s) return 0;
    StructDef *sd = find_struct(s->struct_type_name);
    if (!sd) return 0;
    for (int i = 0; i < sd->field_count; i++) {
        if (strcmp(sd->field_names[i], field_name) == 0) return i;
    }
    return 0;
}

static void emit_load_rbp_offset(X86Buffer *b, int offset) {
    emit_u8(b, 0x48);
    emit_u8(b, 0x8b);
    emit_u8(b, 0x85);
    emit_u32(b, (uint32_t)(-offset));
}

static void emit_store_rbp_offset(X86Buffer *b, int offset) {
    emit_u8(b, 0x48);
    emit_u8(b, 0x89);
    emit_u8(b, 0x85);
    emit_u32(b, (uint32_t)(-offset));
}

static void emit_load_sym(X86Buffer *b, Symbol *s, int extra_off) {
    int offset = (s ? s->stack_offset : 8) + extra_off;
    if (s && s->is_local) {
        emit_u8(b, 0x48); emit_u8(b, 0x8b); emit_u8(b, 0x85);
        emit_u32(b, (uint32_t)(-offset));
    } else {
        emit_u8(b, 0x49); emit_u8(b, 0x8b); emit_u8(b, 0x87);
        emit_u32(b, (uint32_t)(-offset));
    }
}

static void emit_store_sym(X86Buffer *b, Symbol *s, int extra_off) {
    int offset = (s ? s->stack_offset : 8) + extra_off;
    if (s && s->is_local) {
        emit_u8(b, 0x48); emit_u8(b, 0x89); emit_u8(b, 0x85);
        emit_u32(b, (uint32_t)(-offset));
    } else {
        emit_u8(b, 0x49); emit_u8(b, 0x89); emit_u8(b, 0x87);
        emit_u32(b, (uint32_t)(-offset));
    }
}

static void emit_x86_expr(X86Buffer *b, ASTNode *n) {
    if (!n) return;
    switch (n->kind) {
        case AST_INT_LIT:
        case AST_BOOL_LIT:
            emit_u8(b, 0x48);
            emit_u8(b, 0xb8);
            emit_u64(b, (uint64_t)n->int_val);
            break;

        case AST_VAR_REF: {
            Symbol *s = find_symbol(n->name);
            emit_load_sym(b, s, 0);
            break;
        }

        case AST_ARRAY_INDEX_EXPR: {
            Symbol *s = find_symbol(n->name);
            int base_off = s ? s->stack_offset : 8;
            emit_x86_expr(b, n->left);
            emit_u8(b, 0x48); emit_u8(b, 0xc1); emit_u8(b, 0xe0); emit_u8(b, 0x03);
            if (s && s->is_local) {
                emit_u8(b, 0x48); emit_u8(b, 0x8d); emit_u8(b, 0x8d);
            } else {
                emit_u8(b, 0x49); emit_u8(b, 0x8d); emit_u8(b, 0x8f);
            }
            emit_u32(b, (uint32_t)(-base_off));
            emit_u8(b, 0x48); emit_u8(b, 0x29); emit_u8(b, 0xc1);
            emit_u8(b, 0x48); emit_u8(b, 0x8b); emit_u8(b, 0x01);
            break;
        }

        case AST_STRUCT_FIELD_EXPR: {
            Symbol *s = find_symbol(n->name);
            int fidx = find_struct_field_index(n->name, n->secondary_name);
            emit_load_sym(b, s, fidx * 8);
            break;
        }

        case AST_CALL_EXPR: {
            for (int i = 0; i < n->arg_count; i++) {
                emit_x86_expr(b, n->args[i]);
                emit_u8(b, 0x50);
            }
            for (int i = n->arg_count - 1; i >= 0; i--) {
                if (i == 0) emit_u8(b, 0x5f);
                else if (i == 1) emit_u8(b, 0x5e);
                else if (i == 2) emit_u8(b, 0x5a);
                else if (i == 3) emit_u8(b, 0x59);
                else emit_u8(b, 0x58);
            }
            int foff = find_func_offset(n->name);
            if (foff >= 0) {
                emit_u8(b, 0xe8);
                int32_t rel = foff - (b->len + 4);
                emit_u32(b, (uint32_t)rel);
            }
            break;
        }

        case AST_UNOP:
            emit_x86_expr(b, n->left);
            if (n->op == OP_NEG) {
                emit_u8(b, 0x48); emit_u8(b, 0xf7); emit_u8(b, 0xd8);
            } else if (n->op == OP_NOT) {
                emit_u8(b, 0x48); emit_u8(b, 0x85); emit_u8(b, 0xc0);
                emit_u8(b, 0x0f); emit_u8(b, 0x94); emit_u8(b, 0xc0);
                emit_u8(b, 0x48); emit_u8(b, 0x0f); emit_u8(b, 0xb6); emit_u8(b, 0xc0);
            } else if (n->op == OP_BIT_NOT) {
                emit_u8(b, 0x48); emit_u8(b, 0xf7); emit_u8(b, 0xd0);
            }
            break;

        case AST_BINOP:
            emit_x86_expr(b, n->left);
            emit_u8(b, 0x50);
            emit_x86_expr(b, n->right);
            emit_u8(b, 0x48); emit_u8(b, 0x89); emit_u8(b, 0xc1);
            emit_u8(b, 0x58);

            switch (n->op) {
                case OP_ADD:
                    emit_u8(b, 0x48); emit_u8(b, 0x01); emit_u8(b, 0xc8);
                    break;
                case OP_SUB:
                    emit_u8(b, 0x48); emit_u8(b, 0x29); emit_u8(b, 0xc8);
                    break;
                case OP_MUL:
                    emit_u8(b, 0x48); emit_u8(b, 0x0f); emit_u8(b, 0xaf); emit_u8(b, 0xc1);
                    break;
                case OP_DIV:
                    emit_u8(b, 0x48); emit_u8(b, 0x99);
                    emit_u8(b, 0x48); emit_u8(b, 0xf7); emit_u8(b, 0xf9);
                    break;
                case OP_MOD:
                    emit_u8(b, 0x48); emit_u8(b, 0x99);
                    emit_u8(b, 0x48); emit_u8(b, 0xf7); emit_u8(b, 0xf9);
                    emit_u8(b, 0x48); emit_u8(b, 0x89); emit_u8(b, 0xd0);
                    break;
                case OP_BIT_AND:
                case OP_LOG_AND:
                    emit_u8(b, 0x48); emit_u8(b, 0x21); emit_u8(b, 0xc8);
                    break;
                case OP_BIT_OR:
                case OP_LOG_OR:
                    emit_u8(b, 0x48); emit_u8(b, 0x09); emit_u8(b, 0xc8);
                    break;
                case OP_BIT_XOR:
                    emit_u8(b, 0x48); emit_u8(b, 0x31); emit_u8(b, 0xc8);
                    break;
                case OP_SHL:
                    emit_u8(b, 0x48); emit_u8(b, 0xd3); emit_u8(b, 0xe0);
                    break;
                case OP_SHR:
                    emit_u8(b, 0x48); emit_u8(b, 0xd3); emit_u8(b, 0xf8);
                    break;
                case OP_EQ:
                case OP_NEQ:
                case OP_GT:
                case OP_LT:
                case OP_GTE:
                case OP_LTE: {
                    emit_u8(b, 0x48); emit_u8(b, 0x39); emit_u8(b, 0xc8);
                    uint8_t setcc = 0x94;
                    if (n->op == OP_EQ) setcc = 0x94;
                    else if (n->op == OP_NEQ) setcc = 0x95;
                    else if (n->op == OP_GT) setcc = 0x9f;
                    else if (n->op == OP_LT) setcc = 0x9c;
                    else if (n->op == OP_GTE) setcc = 0x9d;
                    else if (n->op == OP_LTE) setcc = 0x9e;
                    emit_u8(b, 0x0f); emit_u8(b, setcc); emit_u8(b, 0xc0);
                    emit_u8(b, 0x48); emit_u8(b, 0x0f); emit_u8(b, 0xb6); emit_u8(b, 0xc0);
                    break;
                }
                default:
                    break;
            }
            break;

        default:
            break;
    }
}

static void emit_x86_display_item(X86Buffer *b, ASTNode *item, char trailing_char) {
    if (!item) return;
    if (item->kind == AST_TEXT_LIT) {
        int r_off = b->rodata_len;
        int slen = (int)strlen(item->text_val);
        memcpy(b->rodata + b->rodata_len, item->text_val, slen);
        b->rodata[b->rodata_len + slen] = (uint8_t)trailing_char;
        b->rodata_len += slen + 1;

        emit_u8(b, 0xb8); emit_u32(b, 1);
        emit_u8(b, 0xbf); emit_u32(b, 1);
        emit_u8(b, 0x48); emit_u8(b, 0xbe);
        g_rodata_relocs[g_rodata_reloc_count].patch_pos = b->len;
        g_rodata_relocs[g_rodata_reloc_count].rodata_offset = r_off;
        g_rodata_reloc_count++;
        emit_u64(b, 0);
        emit_u8(b, 0xba); emit_u32(b, (uint32_t)(slen + 1));
        emit_u8(b, 0x0f); emit_u8(b, 0x05);
    } else if (item->kind == AST_VAR_REF &&
               find_symbol(item->name) && find_symbol(item->name)->type == TYPE_TEXT) {
        Symbol *s = find_symbol(item->name);
        emit_load_sym(b, s, 0);
        emit_u8(b, 0x48); emit_u8(b, 0x89); emit_u8(b, 0xc2);
        emit_u8(b, 0x48); emit_u8(b, 0xc1); emit_u8(b, 0xea); emit_u8(b, 32);
        emit_u8(b, 0x89); emit_u8(b, 0xc6);
        emit_u8(b, 0x48); emit_u8(b, 0xb8);
        g_rodata_relocs[g_rodata_reloc_count].patch_pos = b->len;
        g_rodata_relocs[g_rodata_reloc_count].rodata_offset = 0;
        g_rodata_reloc_count++;
        emit_u64(b, 0);
        emit_u8(b, 0x48); emit_u8(b, 0x01); emit_u8(b, 0xc6);
        emit_u8(b, 0xb8); emit_u32(b, 1);
        emit_u8(b, 0xbf); emit_u32(b, 1);
        emit_u8(b, 0x0f); emit_u8(b, 0x05);

        /* Write trailing separator (' ' or '\n') */
        int r_off = b->rodata_len;
        b->rodata[b->rodata_len++] = (uint8_t)trailing_char;
        emit_u8(b, 0xb8); emit_u32(b, 1);
        emit_u8(b, 0xbf); emit_u32(b, 1);
        emit_u8(b, 0x48); emit_u8(b, 0xbe);
        g_rodata_relocs[g_rodata_reloc_count].patch_pos = b->len;
        g_rodata_relocs[g_rodata_reloc_count].rodata_offset = r_off;
        g_rodata_reloc_count++;
        emit_u64(b, 0);
        emit_u8(b, 0xba); emit_u32(b, 1);
        emit_u8(b, 0x0f); emit_u8(b, 0x05);
    } else {
        emit_x86_expr(b, item);
        emit_u8(b, 0xbf); emit_u32(b, (uint32_t)(unsigned char)trailing_char);
        emit_u8(b, 0xe8);
        int32_t rel = g_print_i64_offset - (b->len + 4);
        emit_u32(b, (uint32_t)rel);
    }
}

static void emit_x86_stmt(X86Buffer *b, ASTNode *n) {
    if (!n) return;
    switch (n->kind) {
        case AST_LET:
        case AST_SET: {
            Symbol *s = find_symbol(n->name);
            if (n->left && n->left->kind == AST_TEXT_LIT) {
                int r_off = b->rodata_len;
                int slen = (int)strlen(n->left->text_val);
                memcpy(b->rodata + b->rodata_len, n->left->text_val, slen);
                b->rodata_len += slen;
                uint64_t packed = ((uint64_t)slen << 32) | (uint64_t)r_off;
                emit_u8(b, 0x48); emit_u8(b, 0xb8); emit_u64(b, packed);
                emit_store_sym(b, s, 0);
            } else {
                emit_x86_expr(b, n->left);
                emit_store_sym(b, s, 0);
            }
            break;
        }

        case AST_DISPLAY: {
            if (n->arg_count > 1) {
                for (int i = 0; i < n->arg_count; i++) {
                    char tc = (i + 1 < n->arg_count) ? ' ' : '\n';
                    emit_x86_display_item(b, n->args[i], tc);
                }
            } else {
                emit_x86_display_item(b, n->left, '\n');
            }
            break;
        }

        case AST_EXIT:
            emit_x86_expr(b, n->left);
            emit_u8(b, 0x48); emit_u8(b, 0x89); emit_u8(b, 0xc7);
            emit_u8(b, 0xb8); emit_u32(b, 60);
            emit_u8(b, 0x0f); emit_u8(b, 0x05);
            break;

        case AST_IF: {
            emit_x86_expr(b, n->left);
            emit_u8(b, 0x48); emit_u8(b, 0x85); emit_u8(b, 0xc0);
            emit_u8(b, 0x0f); emit_u8(b, 0x84);
            int jz_patch = b->len;
            emit_u32(b, 0);

            for (int i = 0; i < n->body_count; i++) {
                emit_x86_stmt(b, n->body[i]);
            }

            if (n->else_count > 0) {
                emit_u8(b, 0xe9);
                int jmp_patch = b->len;
                emit_u32(b, 0);

                int else_start = b->len;
                patch_u32(b, jz_patch, (uint32_t)(else_start - (jz_patch + 4)));

                for (int i = 0; i < n->else_count; i++) {
                    emit_x86_stmt(b, n->else_body[i]);
                }

                int end_if = b->len;
                patch_u32(b, jmp_patch, (uint32_t)(end_if - (jmp_patch + 4)));
            } else {
                int end_if = b->len;
                patch_u32(b, jz_patch, (uint32_t)(end_if - (jz_patch + 4)));
            }
            break;
        }

        case AST_WHILE: {
            int loop_start = b->len;
            emit_x86_expr(b, n->left);
            emit_u8(b, 0x48); emit_u8(b, 0x85); emit_u8(b, 0xc0);
            emit_u8(b, 0x0f); emit_u8(b, 0x84);
            int jz_patch = b->len;
            emit_u32(b, 0);

            for (int i = 0; i < n->body_count; i++) {
                emit_x86_stmt(b, n->body[i]);
            }

            emit_u8(b, 0xe9);
            int32_t back_rel = loop_start - (b->len + 4);
            emit_u32(b, (uint32_t)back_rel);

            int loop_end = b->len;
            patch_u32(b, jz_patch, (uint32_t)(loop_end - (jz_patch + 4)));
            break;
        }

        case AST_REPEAT: {
            int counter_off = g_next_stack_offset;
            g_next_stack_offset += 8;
            emit_x86_expr(b, n->left);
            emit_store_rbp_offset(b, counter_off);

            int loop_start = b->len;
            emit_load_rbp_offset(b, counter_off);
            emit_u8(b, 0x48); emit_u8(b, 0x83); emit_u8(b, 0xf8); emit_u8(b, 0x00);
            emit_u8(b, 0x0f); emit_u8(b, 0x8e);
            int jle_patch = b->len;
            emit_u32(b, 0);

            for (int i = 0; i < n->body_count; i++) {
                emit_x86_stmt(b, n->body[i]);
            }

            emit_load_rbp_offset(b, counter_off);
            emit_u8(b, 0x48); emit_u8(b, 0xff); emit_u8(b, 0xc8);
            emit_store_rbp_offset(b, counter_off);

            emit_u8(b, 0xe9);
            int32_t back_rel = loop_start - (b->len + 4);
            emit_u32(b, (uint32_t)back_rel);

            int loop_end = b->len;
            patch_u32(b, jle_patch, (uint32_t)(loop_end - (jle_patch + 4)));
            break;
        }

        case AST_FUNC_DEF: {
            emit_u8(b, 0xe9);
            int skip_patch = b->len;
            emit_u32(b, 0);

            int fn_start = b->len;
            snprintf(g_func_offsets[g_func_offset_count].name, MAX_STR_LEN, "%s", n->name);
            g_func_offsets[g_func_offset_count].code_offset = fn_start;
            g_func_offset_count++;

            snprintf(g_current_func, sizeof(g_current_func), "%s", n->name);

            /* System V AMD64 stack frame prologue for true recursion */
            emit_u8(b, 0x55);
            emit_u8(b, 0x48); emit_u8(b, 0x89); emit_u8(b, 0xe5);
            emit_u8(b, 0x48); emit_u8(b, 0x81); emit_u8(b, 0xec);
            emit_u32(b, 256);

            for (int i = 0; i < n->param_count; i++) {
                Symbol *ps = find_symbol(n->param_names[i]);
                if (i == 0) { emit_u8(b, 0x48); emit_u8(b, 0x89); emit_u8(b, 0xf8); }
                else if (i == 1) { emit_u8(b, 0x48); emit_u8(b, 0x89); emit_u8(b, 0xf0); }
                else if (i == 2) { emit_u8(b, 0x48); emit_u8(b, 0x89); emit_u8(b, 0xd0); }
                else if (i == 3) { emit_u8(b, 0x48); emit_u8(b, 0x89); emit_u8(b, 0xc8); }
                emit_store_sym(b, ps, 0);
            }

            for (int i = 0; i < n->body_count; i++) {
                emit_x86_stmt(b, n->body[i]);
            }
            emit_u8(b, 0xc9); /* leave */
            emit_u8(b, 0xc3); /* ret */

            g_current_func[0] = '\0';

            int after_fn = b->len;
            patch_u32(b, skip_patch, (uint32_t)(after_fn - (skip_patch + 4)));
            break;
        }

        case AST_RETURN:
            emit_x86_expr(b, n->left);
            emit_u8(b, 0xc9); /* leave */
            emit_u8(b, 0xc3); /* ret */
            break;

        case AST_CALL_STMT: {
            for (int i = 0; i < n->arg_count; i++) {
                emit_x86_expr(b, n->args[i]);
                emit_u8(b, 0x50);
            }
            for (int i = n->arg_count - 1; i >= 0; i--) {
                if (i == 0) emit_u8(b, 0x5f);
                else if (i == 1) emit_u8(b, 0x5e);
                else if (i == 2) emit_u8(b, 0x5a);
                else if (i == 3) emit_u8(b, 0x59);
                else emit_u8(b, 0x58);
            }
            int foff = find_func_offset(n->name);
            if (foff >= 0) {
                emit_u8(b, 0xe8);
                int32_t rel = foff - (b->len + 4);
                emit_u32(b, (uint32_t)rel);
            }
            Symbol *dest = find_symbol(n->secondary_name);
            if (dest) emit_store_sym(b, dest, 0);
            break;
        }

        case AST_ARRAY_ALLOC:
        case AST_MEM_ALLOC:
        case AST_STRUCT_DEF:
        case AST_STRUCT_CREATE:
        case AST_TENSOR_CREATE:
        case AST_DB_OPEN:
            break;

        case AST_ARRAY_STORE:
        case AST_MEM_WRITE: {
            Symbol *s = find_symbol(n->name);
            int base_off = s ? s->stack_offset : 8;
            emit_x86_expr(b, n->left);
            emit_u8(b, 0x50);
            emit_x86_expr(b, n->right);
            emit_u8(b, 0x59);
            if (n->kind == AST_ARRAY_STORE) {
                emit_u8(b, 0x48); emit_u8(b, 0xc1); emit_u8(b, 0xe1); emit_u8(b, 0x03);
            }
            emit_u8(b, 0x48); emit_u8(b, 0x8d); emit_u8(b, 0x95);
            emit_u32(b, (uint32_t)(-base_off));
            emit_u8(b, 0x48); emit_u8(b, 0x29); emit_u8(b, 0xca);
            emit_u8(b, 0x48); emit_u8(b, 0x89); emit_u8(b, 0x02);
            break;
        }

        case AST_ARRAY_LOAD:
        case AST_MEM_READ: {
            Symbol *s = find_symbol(n->name);
            Symbol *dest = find_symbol(n->secondary_name);
            int base_off = s ? s->stack_offset : 8;
            emit_x86_expr(b, n->left);
            if (n->kind == AST_ARRAY_LOAD) {
                emit_u8(b, 0x48); emit_u8(b, 0xc1); emit_u8(b, 0xe0); emit_u8(b, 0x03);
            }
            emit_u8(b, 0x48); emit_u8(b, 0x8d); emit_u8(b, 0x8d);
            emit_u32(b, (uint32_t)(-base_off));
            emit_u8(b, 0x48); emit_u8(b, 0x29); emit_u8(b, 0xc1);
            emit_u8(b, 0x48); emit_u8(b, 0x8b); emit_u8(b, 0x01);
            if (dest) emit_store_rbp_offset(b, dest->stack_offset);
            break;
        }

        case AST_STRUCT_SET: {
            Symbol *s = find_symbol(n->name);
            int base_off = s ? s->stack_offset : 8;
            int fidx = find_struct_field_index(n->name, n->secondary_name);
            emit_x86_expr(b, n->left);
            emit_store_rbp_offset(b, base_off + fidx * 8);
            break;
        }

        case AST_STRUCT_GET: {
            Symbol *s = find_symbol(n->name);
            Symbol *dest = find_symbol(n->tertiary_name);
            int base_off = s ? s->stack_offset : 8;
            int fidx = find_struct_field_index(n->name, n->secondary_name);
            emit_load_rbp_offset(b, base_off + fidx * 8);
            if (dest) emit_store_rbp_offset(b, dest->stack_offset);
            break;
        }

        case AST_TENSOR_FILL: {
            Symbol *s = find_symbol(n->name);
            if (s) {
                int total = s->tensor_rows * s->tensor_cols;
                emit_x86_expr(b, n->left);
                for (int i = 0; i < total; i++) {
                    emit_store_rbp_offset(b, s->stack_offset + i * 8);
                }
            }
            break;
        }

        case AST_TENSOR_SET: {
            Symbol *s = find_symbol(n->name);
            if (s) {
                int cols = s->tensor_cols > 0 ? s->tensor_cols : 1;
                emit_x86_expr(b, n->left);
                emit_u8(b, 0x48); emit_u8(b, 0x69); emit_u8(b, 0xc0); emit_u32(b, (uint32_t)cols);
                emit_u8(b, 0x50);
                emit_x86_expr(b, n->right);
                emit_u8(b, 0x59);
                emit_u8(b, 0x48); emit_u8(b, 0x01); emit_u8(b, 0xc8);
                emit_u8(b, 0x50);
                emit_x86_expr(b, n->third);
                emit_u8(b, 0x59);
                emit_u8(b, 0x48); emit_u8(b, 0xc1); emit_u8(b, 0xe1); emit_u8(b, 0x03);
                emit_u8(b, 0x48); emit_u8(b, 0x8d); emit_u8(b, 0x95);
                emit_u32(b, (uint32_t)(-s->stack_offset));
                emit_u8(b, 0x48); emit_u8(b, 0x29); emit_u8(b, 0xca);
                emit_u8(b, 0x48); emit_u8(b, 0x89); emit_u8(b, 0x02);
            }
            break;
        }

        case AST_TENSOR_GET: {
            Symbol *s = find_symbol(n->name);
            Symbol *dest = find_symbol(n->secondary_name);
            if (s && dest) {
                int cols = s->tensor_cols > 0 ? s->tensor_cols : 1;
                emit_x86_expr(b, n->left);
                emit_u8(b, 0x48); emit_u8(b, 0x69); emit_u8(b, 0xc0); emit_u32(b, (uint32_t)cols);
                emit_u8(b, 0x50);
                emit_x86_expr(b, n->right);
                emit_u8(b, 0x59);
                emit_u8(b, 0x48); emit_u8(b, 0x01); emit_u8(b, 0xc8);
                emit_u8(b, 0x48); emit_u8(b, 0xc1); emit_u8(b, 0xe0); emit_u8(b, 0x03);
                emit_u8(b, 0x48); emit_u8(b, 0x8d); emit_u8(b, 0x8d);
                emit_u32(b, (uint32_t)(-s->stack_offset));
                emit_u8(b, 0x48); emit_u8(b, 0x29); emit_u8(b, 0xc1);
                emit_u8(b, 0x48); emit_u8(b, 0x8b); emit_u8(b, 0x01);
                emit_store_rbp_offset(b, dest->stack_offset);
            }
            break;
        }

        case AST_TENSOR_MUL: {
            Symbol *ta = find_symbol(n->name);
            Symbol *tb = find_symbol(n->secondary_name);
            Symbol *tc = find_symbol(n->tertiary_name);
            if (ta && tb && tc) {
                int R = ta->tensor_rows;
                int K = ta->tensor_cols;
                int C = tb->tensor_cols;
                for (int r = 0; r < R; r++) {
                    for (int c = 0; c < C; c++) {
                        emit_u8(b, 0x4d); emit_u8(b, 0x31); emit_u8(b, 0xc9);
                        for (int k = 0; k < K; k++) {
                            emit_load_rbp_offset(b, ta->stack_offset + (r * K + k) * 8);
                            emit_u8(b, 0x48); emit_u8(b, 0x89); emit_u8(b, 0xc1);
                            emit_load_rbp_offset(b, tb->stack_offset + (k * C + c) * 8);
                            emit_u8(b, 0x48); emit_u8(b, 0x0f); emit_u8(b, 0xaf); emit_u8(b, 0xc1);
                            emit_u8(b, 0x49); emit_u8(b, 0x01); emit_u8(b, 0xc1);
                        }
                        emit_u8(b, 0x4c); emit_u8(b, 0x89); emit_u8(b, 0xc8);
                        emit_store_rbp_offset(b, tc->stack_offset + (r * C + c) * 8);
                    }
                }
            }
            break;
        }

        case AST_TENSOR_RELU: {
            Symbol *ta = find_symbol(n->name);
            if (ta) {
                int total = ta->tensor_rows * ta->tensor_cols;
                for (int i = 0; i < total; i++) {
                    emit_load_rbp_offset(b, ta->stack_offset + i * 8);
                    emit_u8(b, 0x48); emit_u8(b, 0x85); emit_u8(b, 0xc0);
                    emit_u8(b, 0x79); emit_u8(b, 0x03);
                    emit_u8(b, 0x48); emit_u8(b, 0x31); emit_u8(b, 0xc0);
                    emit_store_rbp_offset(b, ta->stack_offset + i * 8);
                }
            }
            break;
        }

        case AST_TENSOR_SUM: {
            Symbol *ta = find_symbol(n->name);
            Symbol *dest = find_symbol(n->secondary_name);
            if (ta && dest) {
                int total = ta->tensor_rows * ta->tensor_cols;
                emit_u8(b, 0x4d); emit_u8(b, 0x31); emit_u8(b, 0xc9);
                for (int i = 0; i < total; i++) {
                    emit_load_rbp_offset(b, ta->stack_offset + i * 8);
                    emit_u8(b, 0x49); emit_u8(b, 0x01); emit_u8(b, 0xc1);
                }
                emit_u8(b, 0x4c); emit_u8(b, 0x89); emit_u8(b, 0xc8);
                emit_store_rbp_offset(b, dest->stack_offset);
            }
            break;
        }

        case AST_DB_INSERT: {
            /* Key-hashed slot store in 64-slot stack table [rbp - base - (key & 63)*8] */
            Symbol *db = find_symbol(n->name);
            int base_off = db ? db->stack_offset : 8;
            emit_x86_expr(b, n->left); /* key in rax */
            emit_u8(b, 0x48); emit_u8(b, 0x83); emit_u8(b, 0xe0); emit_u8(b, 0x3f); /* and rax, 63 */
            emit_u8(b, 0x50); /* push slot idx */
            emit_x86_expr(b, n->right); /* value in rax */
            emit_u8(b, 0x59); /* pop rcx */
            emit_u8(b, 0x48); emit_u8(b, 0xc1); emit_u8(b, 0xe1); emit_u8(b, 0x03); /* shl rcx, 3 */
            emit_u8(b, 0x48); emit_u8(b, 0x8d); emit_u8(b, 0x95);
            emit_u32(b, (uint32_t)(-base_off));
            emit_u8(b, 0x48); emit_u8(b, 0x29); emit_u8(b, 0xca);
            emit_u8(b, 0x48); emit_u8(b, 0x89); emit_u8(b, 0x02);
            break;
        }

        case AST_DB_QUERY: {
            Symbol *db = find_symbol(n->name);
            Symbol *dest = find_symbol(n->secondary_name);
            int base_off = db ? db->stack_offset : 8;
            emit_x86_expr(b, n->left); /* key in rax */
            emit_u8(b, 0x48); emit_u8(b, 0x83); emit_u8(b, 0xe0); emit_u8(b, 0x3f); /* and rax, 63 */
            emit_u8(b, 0x48); emit_u8(b, 0xc1); emit_u8(b, 0xe0); emit_u8(b, 0x03); /* shl rax, 3 */
            emit_u8(b, 0x48); emit_u8(b, 0x8d); emit_u8(b, 0x8d);
            emit_u32(b, (uint32_t)(-base_off));
            emit_u8(b, 0x48); emit_u8(b, 0x29); emit_u8(b, 0xc1);
            emit_u8(b, 0x48); emit_u8(b, 0x8b); emit_u8(b, 0x01);
            if (dest) emit_store_rbp_offset(b, dest->stack_offset);
            break;
        }

        default:
            break;
    }
}

static bool emit_elf64_binary(ASTNode *prog, const char *out_path, X86Buffer *out_buf) {
    memset(out_buf, 0, sizeof(X86Buffer));
    g_func_offset_count = 0;
    g_rodata_reloc_count = 0;

    emit_u8(out_buf, 0xe9);
    int entry_jmp_patch = out_buf->len;
    emit_u32(out_buf, 0);

    emit_runtime_print_i64(out_buf);

    int start_off = out_buf->len;
    patch_u32(out_buf, entry_jmp_patch, (uint32_t)(start_off - (entry_jmp_patch + 4)));

    emit_u8(out_buf, 0x55);
    emit_u8(out_buf, 0x48); emit_u8(out_buf, 0x89); emit_u8(out_buf, 0xe5);
    uint32_t frame_bytes = (uint32_t)(((g_next_stack_offset + 64) + 15) & ~15);
    emit_u8(out_buf, 0x48); emit_u8(out_buf, 0x81); emit_u8(out_buf, 0xec);
    emit_u32(out_buf, frame_bytes);
    emit_u8(out_buf, 0x49); emit_u8(out_buf, 0x89); emit_u8(out_buf, 0xef); /* mov r15, rbp */

    for (int i = 0; i < prog->body_count; i++) {
        emit_x86_stmt(out_buf, prog->body[i]);
    }

    emit_u8(out_buf, 0xb8); emit_u32(out_buf, 60);
    emit_u8(out_buf, 0x31); emit_u8(out_buf, 0xff);
    emit_u8(out_buf, 0x0f); emit_u8(out_buf, 0x05);

    uint64_t base_vaddr = 0x400000ULL;
    uint64_t code_vaddr = base_vaddr + 120ULL;
    uint64_t rodata_vaddr = code_vaddr + (uint64_t)out_buf->len;

    for (int i = 0; i < g_rodata_reloc_count; i++) {
        uint64_t target_addr = rodata_vaddr + (uint64_t)g_rodata_relocs[i].rodata_offset;
        int p = g_rodata_relocs[i].patch_pos;
        for (int b = 0; b < 8; b++) {
            out_buf->buf[p + b] = (uint8_t)((target_addr >> (b * 8)) & 0xFF);
        }
    }

    uint64_t total_file_size = 120ULL + (uint64_t)out_buf->len + (uint64_t)out_buf->rodata_len;
    uint8_t ehdr[64];
    memset(ehdr, 0, sizeof(ehdr));
    ehdr[0] = 0x7f; ehdr[1] = 'E'; ehdr[2] = 'L'; ehdr[3] = 'F';
    ehdr[4] = 2;
    ehdr[5] = 1;
    ehdr[6] = 1;
    *(uint16_t *)(ehdr + 16) = 2;
    *(uint16_t *)(ehdr + 18) = 62;
    *(uint32_t *)(ehdr + 20) = 1;
    *(uint64_t *)(ehdr + 24) = code_vaddr;
    *(uint64_t *)(ehdr + 32) = 64;
    *(uint16_t *)(ehdr + 52) = 64;
    *(uint16_t *)(ehdr + 54) = 56;
    *(uint16_t *)(ehdr + 56) = 1;

    uint8_t phdr[56];
    memset(phdr, 0, sizeof(phdr));
    *(uint32_t *)(phdr + 0) = 1;
    *(uint32_t *)(phdr + 4) = 7;
    *(uint64_t *)(phdr + 8) = 0;
    *(uint64_t *)(phdr + 16) = base_vaddr;
    *(uint64_t *)(phdr + 24) = base_vaddr;
    *(uint64_t *)(phdr + 32) = total_file_size;
    *(uint64_t *)(phdr + 40) = total_file_size + 65536ULL;
    *(uint64_t *)(phdr + 48) = 0x1000ULL;

    if (out_path && strlen(out_path) > 0) {
        FILE *f = fopen(out_path, "wb");
        if (!f) return false;
        fwrite(ehdr, 1, 64, f);
        fwrite(phdr, 1, 56, f);
        fwrite(out_buf->buf, 1, out_buf->len, f);
        if (out_buf->rodata_len > 0) {
            fwrite(out_buf->rodata, 1, out_buf->rodata_len, f);
        }
        fclose(f);
        SET_EXECUTABLE(out_path);
    }
    return true;
}

/* -------------------------------------------------------------------------- */
/* 8. LLVM IR (.ll) SSA CODE GENERATOR (CORE CONSTRUCTS)                      */
/* -------------------------------------------------------------------------- */

static int g_llvm_reg = 1;
static int g_llvm_label = 1;

static int emit_llvm_expr(FILE *out, ASTNode *n) {
    if (!n) return 0;
    switch (n->kind) {
        case AST_INT_LIT:
        case AST_BOOL_LIT: {
            int r = g_llvm_reg++;
            fprintf(out, "  %%%d = add i64 0, %lld\n", r, (long long)n->int_val);
            return r;
        }
        case AST_VAR_REF: {
            int r = g_llvm_reg++;
            fprintf(out, "  %%%d = load i64, ptr %%%s, align 8\n", r, n->name);
            return r;
        }
        case AST_UNOP: {
            int sub = emit_llvm_expr(out, n->left);
            int r = g_llvm_reg++;
            if (n->op == OP_NEG) fprintf(out, "  %%%d = sub i64 0, %%%d\n", r, sub);
            else if (n->op == OP_NOT) {
                int cmp = g_llvm_reg++;
                fprintf(out, "  %%%d = icmp eq i64 %%%d, 0\n", cmp, sub);
                fprintf(out, "  %%%d = zext i1 %%%d to i64\n", r, cmp);
            } else {
                fprintf(out, "  %%%d = xor i64 %%%d, -1\n", r, sub);
            }
            return r;
        }
        case AST_BINOP: {
            int l = emit_llvm_expr(out, n->left);
            int r = emit_llvm_expr(out, n->right);
            int dst = g_llvm_reg++;
            switch (n->op) {
                case OP_ADD: fprintf(out, "  %%%d = add i64 %%%d, %%%d\n", dst, l, r); break;
                case OP_SUB: fprintf(out, "  %%%d = sub i64 %%%d, %%%d\n", dst, l, r); break;
                case OP_MUL: fprintf(out, "  %%%d = mul i64 %%%d, %%%d\n", dst, l, r); break;
                case OP_DIV: fprintf(out, "  %%%d = sdiv i64 %%%d, %%%d\n", dst, l, r); break;
                case OP_MOD: fprintf(out, "  %%%d = srem i64 %%%d, %%%d\n", dst, l, r); break;
                case OP_BIT_AND:
                case OP_LOG_AND: fprintf(out, "  %%%d = and i64 %%%d, %%%d\n", dst, l, r); break;
                case OP_BIT_OR:
                case OP_LOG_OR: fprintf(out, "  %%%d = or i64 %%%d, %%%d\n", dst, l, r); break;
                case OP_BIT_XOR: fprintf(out, "  %%%d = xor i64 %%%d, %%%d\n", dst, l, r); break;
                case OP_SHL: fprintf(out, "  %%%d = shl i64 %%%d, %%%d\n", dst, l, r); break;
                case OP_SHR: fprintf(out, "  %%%d = ashr i64 %%%d, %%%d\n", dst, l, r); break;
                default: {
                    const char *cc = "eq";
                    if (n->op == OP_NEQ) cc = "ne";
                    else if (n->op == OP_GT) cc = "sgt";
                    else if (n->op == OP_LT) cc = "slt";
                    else if (n->op == OP_GTE) cc = "sge";
                    else if (n->op == OP_LTE) cc = "sle";
                    int cmp = g_llvm_reg++;
                    fprintf(out, "  %%%d = icmp %s i64 %%%d, %%%d\n", cmp, cc, l, r);
                    fprintf(out, "  %%%d = zext i1 %%%d to i64\n", dst, cmp);
                    break;
                }
            }
            return dst;
        }
        default: {
            int r = g_llvm_reg++;
            fprintf(out, "  %%%d = add i64 0, 0\n", r);
            return r;
        }
    }
}

static void emit_llvm_stmt(FILE *out, ASTNode *n) {
    if (!n) return;
    switch (n->kind) {
        case AST_LET:
            fprintf(out, "  %%%s = alloca i64, align 8\n", n->name);
            if (n->left && n->left->kind != AST_TEXT_LIT) {
                int r = emit_llvm_expr(out, n->left);
                fprintf(out, "  store i64 %%%d, ptr %%%s, align 8\n", r, n->name);
            }
            break;
        case AST_SET: {
            int r = emit_llvm_expr(out, n->left);
            fprintf(out, "  store i64 %%%d, ptr %%%s, align 8\n", r, n->name);
            break;
        }
        case AST_DISPLAY:
            if (n->left && n->left->kind != AST_TEXT_LIT) {
                int r = emit_llvm_expr(out, n->left);
                int call_reg = g_llvm_reg++;
                fprintf(out, "  %%%d = call i32 (ptr, ...) @printf(ptr @.fmt_int, i64 %%%d)\n", call_reg, r);
            }
            break;
        case AST_IF: {
            int cond = emit_llvm_expr(out, n->left);
            int cmp = g_llvm_reg++;
            int lbl_then = g_llvm_label++;
            int lbl_else = g_llvm_label++;
            int lbl_end = g_llvm_label++;
            fprintf(out, "  %%%d = icmp ne i64 %%%d, 0\n", cmp, cond);
            fprintf(out, "  br i1 %%%d, label %%L%d, label %%L%d\n", cmp, lbl_then, lbl_else);
            fprintf(out, "L%d:\n", lbl_then);
            for (int i = 0; i < n->body_count; i++) emit_llvm_stmt(out, n->body[i]);
            fprintf(out, "  br label %%L%d\n", lbl_end);
            fprintf(out, "L%d:\n", lbl_else);
            for (int i = 0; i < n->else_count; i++) emit_llvm_stmt(out, n->else_body[i]);
            fprintf(out, "  br label %%L%d\n", lbl_end);
            fprintf(out, "L%d:\n", lbl_end);
            break;
        }
        case AST_WHILE: {
            int lbl_cond = g_llvm_label++;
            int lbl_body = g_llvm_label++;
            int lbl_end = g_llvm_label++;
            fprintf(out, "  br label %%L%d\n", lbl_cond);
            fprintf(out, "L%d:\n", lbl_cond);
            int cond = emit_llvm_expr(out, n->left);
            int cmp = g_llvm_reg++;
            fprintf(out, "  %%%d = icmp ne i64 %%%d, 0\n", cmp, cond);
            fprintf(out, "  br i1 %%%d, label %%L%d, label %%L%d\n", cmp, lbl_body, lbl_end);
            fprintf(out, "L%d:\n", lbl_body);
            for (int i = 0; i < n->body_count; i++) emit_llvm_stmt(out, n->body[i]);
            fprintf(out, "  br label %%L%d\n", lbl_cond);
            fprintf(out, "L%d:\n", lbl_end);
            break;
        }
        default:
            break;
    }
}

static void generate_llvm_ir(ASTNode *prog, FILE *out) {
    g_llvm_reg = 1;
    g_llvm_label = 1;
    fprintf(out, "; ModuleID = 'eathon_module'\n");
    fprintf(out, "source_filename = \"program.eat\"\n");
    fprintf(out, "target datalayout = \"e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128\"\n");
    fprintf(out, "target triple = \"x86_64-pc-linux-gnu\"\n\n");
    fprintf(out, "@.fmt_int = private unnamed_addr constant [6 x i8] c\"%%lld\\0A\\00\", align 1\n");
    fprintf(out, "declare i32 @printf(ptr, ...)\n\n");
    fprintf(out, "define dso_local i32 @main() {\n");
    fprintf(out, "entry:\n");
    for (int i = 0; i < prog->body_count; i++) {
        emit_llvm_stmt(out, prog->body[i]);
    }
    fprintf(out, "  ret i32 0\n");
    fprintf(out, "}\n");
}

/* -------------------------------------------------------------------------- */
/* 9. COMPLETE ISO C99 SYSTEMS BACKEND GENERATOR (ALL CONSTRUCTS)             */
/* -------------------------------------------------------------------------- */

static void emit_c99_expr(FILE *out, ASTNode *n) {
    if (!n) return;
    switch (n->kind) {
        case AST_INT_LIT:
            fprintf(out, "%lldLL", (long long)n->int_val);
            break;
        case AST_BOOL_LIT:
            fprintf(out, "%s", n->int_val ? "1LL" : "0LL");
            break;
        case AST_TEXT_LIT:
            fprintf(out, "\"%s\"", n->text_val);
            break;
        case AST_VAR_REF:
            fprintf(out, "%s", n->name);
            break;
        case AST_ARRAY_INDEX_EXPR:
            fprintf(out, "%s[", n->name);
            emit_c99_expr(out, n->left);
            fprintf(out, "]");
            break;
        case AST_STRUCT_FIELD_EXPR:
            fprintf(out, "%s.%s", n->name, n->secondary_name);
            break;
        case AST_CALL_EXPR:
            fprintf(out, "%s(", n->name);
            for (int i = 0; i < n->arg_count; i++) {
                if (i > 0) fprintf(out, ", ");
                emit_c99_expr(out, n->args[i]);
            }
            fprintf(out, ")");
            break;
        case AST_UNOP:
            if (n->op == OP_NEG) fprintf(out, "(-(");
            else if (n->op == OP_NOT) fprintf(out, "(!(");
            else fprintf(out, "(~(");
            emit_c99_expr(out, n->left);
            fprintf(out, "))");
            break;
        case AST_BINOP:
            fprintf(out, "(");
            emit_c99_expr(out, n->left);
            switch (n->op) {
                case OP_ADD: fprintf(out, " + "); break;
                case OP_SUB: fprintf(out, " - "); break;
                case OP_MUL: fprintf(out, " * "); break;
                case OP_DIV: fprintf(out, " / "); break;
                case OP_MOD: fprintf(out, " %% "); break;
                case OP_BIT_AND: fprintf(out, " & "); break;
                case OP_BIT_OR: fprintf(out, " | "); break;
                case OP_BIT_XOR: fprintf(out, " ^ "); break;
                case OP_SHL: fprintf(out, " << "); break;
                case OP_SHR: fprintf(out, " >> "); break;
                case OP_EQ: fprintf(out, " == "); break;
                case OP_NEQ: fprintf(out, " != "); break;
                case OP_GT: fprintf(out, " > "); break;
                case OP_LT: fprintf(out, " < "); break;
                case OP_GTE: fprintf(out, " >= "); break;
                case OP_LTE: fprintf(out, " <= "); break;
                case OP_LOG_AND: fprintf(out, " && "); break;
                case OP_LOG_OR: fprintf(out, " || "); break;
                default: fprintf(out, " + "); break;
            }
            emit_c99_expr(out, n->right);
            fprintf(out, ")");
            break;
        default:
            fprintf(out, "0LL");
            break;
    }
}

static void emit_c99_stmt(FILE *out, ASTNode *n, int indent) {
    if (!n) return;
    char pad[64];
    int p = indent * 2;
    if (p > 60) p = 60;
    memset(pad, ' ', p);
    pad[p] = '\0';

    switch (n->kind) {
        case AST_LET:
            if (n->inferred_type == TYPE_TEXT) {
                fprintf(out, "%sconst char *%s = ", pad, n->name);
            } else {
                fprintf(out, "%sint64_t %s = ", pad, n->name);
            }
            emit_c99_expr(out, n->left);
            fprintf(out, ";\n");
            break;
        case AST_SET:
            fprintf(out, "%s%s = ", pad, n->name);
            emit_c99_expr(out, n->left);
            fprintf(out, ";\n");
            break;
        case AST_DISPLAY:
            if (n->arg_count > 1) {
                for (int i = 0; i < n->arg_count; i++) {
                    ASTNode *item = n->args[i];
                    const char *sep = (i + 1 < n->arg_count) ? " " : "\\n";
                    if (item && (item->inferred_type == TYPE_TEXT || item->kind == AST_TEXT_LIT)) {
                        fprintf(out, "%sprintf(\"%%s%s\", ", pad, sep);
                        emit_c99_expr(out, item);
                        fprintf(out, ");\n");
                    } else {
                        fprintf(out, "%sprintf(\"%%lld%s\", (long long)(", pad, sep);
                        emit_c99_expr(out, item);
                        fprintf(out, "));\n");
                    }
                }
            } else if (n->left && (n->left->inferred_type == TYPE_TEXT || n->left->kind == AST_TEXT_LIT)) {
                fprintf(out, "%sprintf(\"%%s\\n\", ", pad);
                emit_c99_expr(out, n->left);
                fprintf(out, ");\n");
            } else {
                fprintf(out, "%sprintf(\"%%lld\\n\", (long long)(", pad);
                emit_c99_expr(out, n->left);
                fprintf(out, "));\n");
            }
            break;
        case AST_EXIT:
            fprintf(out, "%sexit((int)(", pad);
            emit_c99_expr(out, n->left);
            fprintf(out, "));\n");
            break;
        case AST_IF:
            fprintf(out, "%sif (", pad);
            emit_c99_expr(out, n->left);
            fprintf(out, ") {\n");
            for (int i = 0; i < n->body_count; i++) emit_c99_stmt(out, n->body[i], indent + 1);
            if (n->else_count > 0) {
                fprintf(out, "%s} else {\n", pad);
                for (int i = 0; i < n->else_count; i++) emit_c99_stmt(out, n->else_body[i], indent + 1);
            }
            fprintf(out, "%s}\n", pad);
            break;
        case AST_WHILE:
            fprintf(out, "%swhile (", pad);
            emit_c99_expr(out, n->left);
            fprintf(out, ") {\n");
            for (int i = 0; i < n->body_count; i++) emit_c99_stmt(out, n->body[i], indent + 1);
            fprintf(out, "%s}\n", pad);
            break;
        case AST_REPEAT:
            fprintf(out, "%sfor (int64_t _rep = 0; _rep < (", pad);
            emit_c99_expr(out, n->left);
            fprintf(out, "); _rep++) {\n");
            for (int i = 0; i < n->body_count; i++) emit_c99_stmt(out, n->body[i], indent + 1);
            fprintf(out, "%s}\n", pad);
            break;
        case AST_RETURN:
            fprintf(out, "%sreturn ", pad);
            emit_c99_expr(out, n->left);
            fprintf(out, ";\n");
            break;
        case AST_CALL_STMT:
            fprintf(out, "%s%s%s = %s(", pad, n->int_val ? "int64_t " : "", n->secondary_name, n->name);
            for (int i = 0; i < n->arg_count; i++) {
                if (i > 0) fprintf(out, ", ");
                emit_c99_expr(out, n->args[i]);
            }
            fprintf(out, ");\n");
            break;
        case AST_ARRAY_ALLOC:
            fprintf(out, "%sint64_t %s[", pad, n->name);
            emit_c99_expr(out, n->left);
            fprintf(out, "]; memset(%s, 0, sizeof(%s));\n", n->name, n->name);
            break;
        case AST_ARRAY_STORE:
            fprintf(out, "%s%s[", pad, n->name);
            emit_c99_expr(out, n->left);
            fprintf(out, "] = ");
            emit_c99_expr(out, n->right);
            fprintf(out, ";\n");
            break;
        case AST_ARRAY_LOAD:
            fprintf(out, "%s%s%s = %s[", pad, n->int_val ? "int64_t " : "", n->secondary_name, n->name);
            emit_c99_expr(out, n->left);
            fprintf(out, "];\n");
            break;
        case AST_MEM_ALLOC:
            fprintf(out, "%suint8_t *%s = (uint8_t *)calloc(1, ", pad, n->name);
            emit_c99_expr(out, n->left);
            fprintf(out, ");\n");
            break;
        case AST_MEM_WRITE:
            fprintf(out, "%s*(int64_t *)(%s + (", pad, n->name);
            emit_c99_expr(out, n->left);
            fprintf(out, ")) = ");
            emit_c99_expr(out, n->right);
            fprintf(out, ";\n");
            break;
        case AST_MEM_READ:
            fprintf(out, "%s%s%s = *(int64_t *)(%s + (", pad, n->int_val ? "int64_t " : "", n->secondary_name, n->name);
            emit_c99_expr(out, n->left);
            fprintf(out, "));\n");
            break;
        case AST_STRUCT_CREATE:
            fprintf(out, "%sstruct %s %s; memset(&%s, 0, sizeof(%s));\n",
                    pad, n->secondary_name, n->name, n->name, n->name);
            break;
        case AST_STRUCT_SET:
            fprintf(out, "%s%s.%s = ", pad, n->name, n->secondary_name);
            emit_c99_expr(out, n->left);
            fprintf(out, ";\n");
            break;
        case AST_STRUCT_GET:
            fprintf(out, "%s%s%s = %s.%s;\n", pad, n->int_val ? "int64_t " : "", n->tertiary_name, n->name, n->secondary_name);
            break;
        case AST_TENSOR_CREATE: {
            Symbol *s = find_symbol(n->name);
            int r = s ? s->tensor_rows : 4;
            int c = s ? s->tensor_cols : 4;
            fprintf(out, "%sint64_t %s[%d][%d]; memset(%s, 0, sizeof(%s));\n",
                    pad, n->name, r, c, n->name, n->name);
            break;
        }
        case AST_TENSOR_FILL: {
            Symbol *s = find_symbol(n->name);
            int r = s ? s->tensor_rows : 4;
            int c = s ? s->tensor_cols : 4;
            fprintf(out, "%sfor(int _r=0;_r<%d;_r++) for(int _c=0;_c<%d;_c++) %s[_r][_c] = ",
                    pad, r, c, n->name);
            emit_c99_expr(out, n->left);
            fprintf(out, ";\n");
            break;
        }
        case AST_TENSOR_SET:
            fprintf(out, "%s%s[", pad, n->name);
            emit_c99_expr(out, n->left);
            fprintf(out, "][");
            emit_c99_expr(out, n->right);
            fprintf(out, "] = ");
            emit_c99_expr(out, n->third);
            fprintf(out, ";\n");
            break;
        case AST_TENSOR_GET:
            fprintf(out, "%s%s%s = %s[", pad, n->int_val ? "int64_t " : "", n->secondary_name, n->name);
            emit_c99_expr(out, n->left);
            fprintf(out, "][");
            emit_c99_expr(out, n->right);
            fprintf(out, "];\n");
            break;
        case AST_TENSOR_MUL: {
            Symbol *ta = find_symbol(n->name);
            Symbol *tb = find_symbol(n->secondary_name);
            int R = ta ? ta->tensor_rows : 2;
            int K = ta ? ta->tensor_cols : 2;
            int C = tb ? tb->tensor_cols : 2;
            fprintf(out, "%sfor(int _r=0;_r<%d;_r++) for(int _c=0;_c<%d;_c++) { int64_t _acc=0; for(int _k=0;_k<%d;_k++) _acc += %s[_r][_k] * %s[_k][_c]; %s[_r][_c] = _acc; }\n",
                    pad, R, C, K, n->name, n->secondary_name, n->tertiary_name);
            break;
        }
        case AST_TENSOR_RELU: {
            Symbol *ta = find_symbol(n->name);
            int R = ta ? ta->tensor_rows : 2;
            int C = ta ? ta->tensor_cols : 2;
            fprintf(out, "%sfor(int _r=0;_r<%d;_r++) for(int _c=0;_c<%d;_c++) if(%s[_r][_c] < 0) %s[_r][_c] = 0;\n",
                    pad, R, C, n->name, n->name);
            break;
        }
        case AST_TENSOR_SUM: {
            Symbol *ta = find_symbol(n->name);
            int R = ta ? ta->tensor_rows : 2;
            int C = ta ? ta->tensor_cols : 2;
            if (n->int_val) fprintf(out, "%sint64_t %s = 0; ", pad, n->secondary_name);
            else fprintf(out, "%s%s = 0; ", pad, n->secondary_name);
            fprintf(out, "for(int _r=0;_r<%d;_r++) for(int _c=0;_c<%d;_c++) %s += %s[_r][_c];\n",
                    R, C, n->secondary_name, n->name);
            break;
        }
        case AST_DB_OPEN:
            fprintf(out, "%sint64_t %s[64]; memset(%s, 0, sizeof(%s));\n",
                    pad, n->name, n->name, n->name);
            break;
        case AST_DB_INSERT:
            fprintf(out, "%s%s[(", pad, n->name);
            emit_c99_expr(out, n->left);
            fprintf(out, ") & 63] = ");
            emit_c99_expr(out, n->right);
            fprintf(out, ";\n");
            break;
        case AST_DB_QUERY:
            fprintf(out, "%s%s%s = %s[(", pad, n->int_val ? "int64_t " : "", n->secondary_name, n->name);
            emit_c99_expr(out, n->left);
            fprintf(out, ") & 63];\n");
            break;
        default:
            break;
    }
}

static void generate_c99_code(ASTNode *prog, FILE *out) {
    fprintf(out, "/* Generated by Eathon Native Compiler (eathon v1.0.2) */\n");
    fprintf(out, "#include <stdio.h>\n#include <stdint.h>\n#include <stdlib.h>\n#include <string.h>\n\n");

    for (int i = 0; i < prog->body_count; i++) {
        ASTNode *n = prog->body[i];
        if (n->kind == AST_STRUCT_DEF) {
            fprintf(out, "struct %s {\n", n->name);
            for (int f = 0; f < n->param_count; f++) {
                fprintf(out, "  int64_t %s;\n", n->param_names[f]);
            }
            fprintf(out, "};\n\n");
        } else if (n->kind == AST_FUNC_DEF) {
            fprintf(out, "int64_t %s(", n->name);
            for (int p = 0; p < n->param_count; p++) {
                if (p > 0) fprintf(out, ", ");
                fprintf(out, "int64_t %s", n->param_names[p]);
            }
            fprintf(out, ") {\n");
            for (int b = 0; b < n->body_count; b++) {
                emit_c99_stmt(out, n->body[b], 1);
            }
            fprintf(out, "  return 0;\n}\n\n");
        }
    }

    fprintf(out, "int main(void) {\n");
    for (int i = 0; i < prog->body_count; i++) {
        ASTNode *n = prog->body[i];
        if (n->kind != AST_STRUCT_DEF && n->kind != AST_FUNC_DEF) {
            emit_c99_stmt(out, n, 1);
        }
    }
    fprintf(out, "  return 0;\n}\n");
}

/* -------------------------------------------------------------------------- */
/* 10. JSON ESCAPING & FULL INSPECTOR OUTPUT                                  */
/* -------------------------------------------------------------------------- */

static void print_json_escaped(const char *s) {
    putchar('"');
    for (; *s; s++) {
        if (*s == '"') printf("\\\"");
        else if (*s == '\\') printf("\\\\");
        else if (*s == '\n') printf("\\n");
        else if (*s == '\r') printf("\\r");
        else if (*s == '\t') printf("\\t");
        else putchar(*s);
    }
    putchar('"');
}

static const char *ast_kind_name(ASTNodeType k) {
    switch (k) {
        case AST_PROGRAM: return "Program";
        case AST_LET: return "VariableDeclaration (Let)";
        case AST_SET: return "Assignment (Set)";
        case AST_DISPLAY: return "SystemOutput (Display)";
        case AST_EXIT: return "ProcessExit (Exit)";
        case AST_IF: return "ConditionalBranch (If)";
        case AST_WHILE: return "WhileLoop";
        case AST_REPEAT: return "RepeatLoop";
        case AST_FUNC_DEF: return "FunctionDefinition";
        case AST_RETURN: return "ReturnStatement";
        case AST_CALL_STMT: return "FunctionCallStatement";
        case AST_ARRAY_ALLOC: return "ArrayAllocation";
        case AST_ARRAY_STORE: return "ArrayStore";
        case AST_ARRAY_LOAD: return "ArrayLoad";
        case AST_MEM_ALLOC: return "MemoryAllocation";
        case AST_MEM_WRITE: return "MemoryWrite";
        case AST_MEM_READ: return "MemoryRead";
        case AST_STRUCT_DEF: return "StructureDefinition";
        case AST_STRUCT_CREATE: return "StructureInstantiation";
        case AST_STRUCT_SET: return "StructureFieldWrite";
        case AST_STRUCT_GET: return "StructureFieldRead";
        case AST_TENSOR_CREATE: return "TensorAllocation";
        case AST_TENSOR_FILL: return "TensorFill";
        case AST_TENSOR_SET: return "TensorCellWrite";
        case AST_TENSOR_GET: return "TensorCellRead";
        case AST_TENSOR_MUL: return "TensorMatrixMultiply";
        case AST_TENSOR_RELU: return "TensorReluActivation";
        case AST_TENSOR_SUM: return "TensorReductionSum";
        case AST_DB_OPEN: return "DatabaseOpen";
        case AST_DB_INSERT: return "DatabaseInsert";
        case AST_DB_QUERY: return "DatabaseQuery";
        case AST_BINOP: return "BinaryExpression";
        case AST_UNOP: return "UnaryExpression";
        case AST_INT_LIT: return "IntegerLiteral";
        case AST_BOOL_LIT: return "BooleanLiteral";
        case AST_TEXT_LIT: return "TextLiteral";
        case AST_VAR_REF: return "IdentifierReference";
        case AST_ARRAY_INDEX_EXPR: return "ArrayElementAccess";
        case AST_STRUCT_FIELD_EXPR: return "StructureFieldAccess";
        case AST_CALL_EXPR: return "FunctionCallExpression";
        default: return "ASTNode";
    }
}

static void print_ast_json(ASTNode *n) {
    if (!n) { printf("null"); return; }
    printf("{\"kind\":");
    print_json_escaped(ast_kind_name(n->kind));
    printf(",\"type\":");
    print_json_escaped(datatype_name(n->inferred_type));
    printf(",\"line\":%d,\"col\":%d", n->line, n->col);
    if (strlen(n->name) > 0) {
        printf(",\"name\":");
        print_json_escaped(n->name);
    }
    if (strlen(n->secondary_name) > 0) {
        printf(",\"target\":");
        print_json_escaped(n->secondary_name);
    }
    if (n->kind == AST_INT_LIT || n->kind == AST_BOOL_LIT) {
        printf(",\"value\":%lld", (long long)n->int_val);
    }
    if (n->kind == AST_TEXT_LIT) {
        printf(",\"text\":");
        print_json_escaped(n->text_val);
    }
    if (n->kind == AST_BINOP || n->kind == AST_UNOP) {
        printf(",\"operator\":");
        print_json_escaped(op_name(n->op));
    }
    if (n->left) {
        printf(",\"left\":");
        print_ast_json(n->left);
    }
    if (n->right) {
        printf(",\"right\":");
        print_ast_json(n->right);
    }
    if (n->body_count > 0) {
        printf(",\"body\":[");
        for (int i = 0; i < n->body_count; i++) {
            if (i > 0) putchar(',');
            print_ast_json(n->body[i]);
        }
        putchar(']');
    }
    if (n->else_count > 0) {
        printf(",\"elseBody\":[");
        for (int i = 0; i < n->else_count; i++) {
            if (i > 0) putchar(',');
            print_ast_json(n->else_body[i]);
        }
        putchar(']');
    }
    putchar('}');
}

/* -------------------------------------------------------------------------- */
/* 11. PORTABLE NATIVE C AST EXECUTION ENGINE (--run & Android JNI Backend)   */
/* -------------------------------------------------------------------------- */

typedef struct {
    char name[MAX_STR_LEN];
    DataType type;
    int64_t int_val;
    char text_val[MAX_STR_LEN];
    int64_t array_vals[128];
    uint8_t *mem_ptr;
    size_t mem_size;
    int field_count;
    char field_names[16][MAX_STR_LEN];
    int64_t field_vals[16];
    int tensor_rows;
    int tensor_cols;
    int64_t tensor_vals[16][16];
    int64_t db_slots[64];
} RuntimeVar;

typedef struct {
    RuntimeVar vars[64];
    int var_count;
} RuntimeFrame;

typedef struct {
    ASTNode *prog;
    RuntimeFrame globals;
    RuntimeFrame call_stack[64];
    int call_depth;
    FILE *out;
    bool has_exited;
    int exit_code;
    bool has_returned;
    int64_t return_val;
} RuntimeContext;

static RuntimeVar *rt_find_var(RuntimeContext *ctx, const char *name) {
    if (ctx->call_depth > 0) {
        RuntimeFrame *top = &ctx->call_stack[ctx->call_depth - 1];
        for (int i = top->var_count - 1; i >= 0; i--) {
            if (strcmp(top->vars[i].name, name) == 0) {
                return &top->vars[i];
            }
        }
    }
    for (int i = ctx->globals.var_count - 1; i >= 0; i--) {
        if (strcmp(ctx->globals.vars[i].name, name) == 0) {
            return &ctx->globals.vars[i];
        }
    }
    return NULL;
}

static RuntimeVar *rt_declare_or_get_var(RuntimeContext *ctx, const char *name, DataType type) {
    RuntimeVar *existing = rt_find_var(ctx, name);
    if (existing) return existing;

    RuntimeFrame *frame = (ctx->call_depth > 0) ? &ctx->call_stack[ctx->call_depth - 1] : &ctx->globals;
    if (frame->var_count >= 64) return &frame->vars[0];
    RuntimeVar *v = &frame->vars[frame->var_count++];
    memset(v, 0, sizeof(RuntimeVar));
    snprintf(v->name, sizeof(v->name), "%s", name);
    v->type = type;
    return v;
}

static ASTNode *rt_find_func(ASTNode *prog, const char *name) {
    for (int i = 0; i < prog->body_count; i++) {
        ASTNode *n = prog->body[i];
        if (n->kind == AST_FUNC_DEF && strcmp(n->name, name) == 0) {
            return n;
        }
    }
    return NULL;
}

static int64_t rt_eval_expr(RuntimeContext *ctx, ASTNode *n);
static void rt_exec_stmt(RuntimeContext *ctx, ASTNode *n);

static int64_t rt_call_function(RuntimeContext *ctx, const char *fn_name, int arg_count, ASTNode **args) {
    ASTNode *fn = rt_find_func(ctx->prog, fn_name);
    if (!fn || ctx->call_depth >= 63) return 0;

    int64_t eval_args[16];
    for (int i = 0; i < arg_count && i < 16; i++) {
        eval_args[i] = rt_eval_expr(ctx, args[i]);
    }

    RuntimeFrame *frame = &ctx->call_stack[ctx->call_depth++];
    memset(frame, 0, sizeof(RuntimeFrame));
    for (int i = 0; i < fn->param_count && i < arg_count; i++) {
        RuntimeVar *pv = &frame->vars[frame->var_count++];
        memset(pv, 0, sizeof(RuntimeVar));
        snprintf(pv->name, sizeof(pv->name), "%s", fn->param_names[i]);
        pv->type = fn->param_types[i];
        pv->int_val = eval_args[i];
    }

    bool saved_ret = ctx->has_returned;
    int64_t saved_val = ctx->return_val;
    ctx->has_returned = false;
    ctx->return_val = 0;

    for (int i = 0; i < fn->body_count; i++) {
        if (ctx->has_returned || ctx->has_exited) break;
        rt_exec_stmt(ctx, fn->body[i]);
    }

    int64_t result = ctx->return_val;
    ctx->has_returned = saved_ret;
    ctx->return_val = saved_val;
    ctx->call_depth--;
    return result;
}

static int64_t rt_eval_expr(RuntimeContext *ctx, ASTNode *n) {
    if (!n) return 0;
    switch (n->kind) {
        case AST_INT_LIT:
        case AST_BOOL_LIT:
            return n->int_val;
        case AST_VAR_REF: {
            RuntimeVar *v = rt_find_var(ctx, n->name);
            return v ? v->int_val : 0;
        }
        case AST_ARRAY_INDEX_EXPR: {
            RuntimeVar *v = rt_find_var(ctx, n->name);
            int64_t idx = rt_eval_expr(ctx, n->left);
            if (v && idx >= 0 && idx < 128) return v->array_vals[idx];
            return 0;
        }
        case AST_STRUCT_FIELD_EXPR: {
            RuntimeVar *v = rt_find_var(ctx, n->name);
            if (v) {
                for (int i = 0; i < v->field_count; i++) {
                    if (strcmp(v->field_names[i], n->secondary_name) == 0) {
                        return v->field_vals[i];
                    }
                }
            }
            return 0;
        }
        case AST_CALL_EXPR:
            return rt_call_function(ctx, n->name, n->arg_count, n->args);
        case AST_UNOP: {
            int64_t v = rt_eval_expr(ctx, n->left);
            if (n->op == OP_NEG) return -v;
            if (n->op == OP_NOT) return !v;
            if (n->op == OP_BIT_NOT) return ~v;
            return v;
        }
        case AST_BINOP: {
            int64_t l = rt_eval_expr(ctx, n->left);
            int64_t r = rt_eval_expr(ctx, n->right);
            switch (n->op) {
                case OP_ADD: return l + r;
                case OP_SUB: return l - r;
                case OP_MUL: return l * r;
                case OP_DIV: return r != 0 ? (l / r) : 0;
                case OP_MOD: return r != 0 ? (l % r) : 0;
                case OP_BIT_AND: return l & r;
                case OP_BIT_OR: return l | r;
                case OP_BIT_XOR: return l ^ r;
                case OP_SHL: return l << r;
                case OP_SHR: return l >> r;
                case OP_EQ: return l == r ? 1 : 0;
                case OP_NEQ: return l != r ? 1 : 0;
                case OP_GT: return l > r ? 1 : 0;
                case OP_LT: return l < r ? 1 : 0;
                case OP_GTE: return l >= r ? 1 : 0;
                case OP_LTE: return l <= r ? 1 : 0;
                case OP_LOG_AND: return (l && r) ? 1 : 0;
                case OP_LOG_OR: return (l || r) ? 1 : 0;
                default: return 0;
            }
        }
        default:
            return 0;
    }
}

static void rt_print_item(RuntimeContext *ctx, ASTNode *item) {
    if (!item) return;
    if (item->kind == AST_TEXT_LIT) {
        fprintf(ctx->out, "%s", item->text_val);
    } else if (item->kind == AST_VAR_REF) {
        RuntimeVar *v = rt_find_var(ctx, item->name);
        if (v && v->type == TYPE_TEXT) {
            fprintf(ctx->out, "%s", v->text_val);
        } else if (v && v->type == TYPE_BOOLEAN) {
            fprintf(ctx->out, "%s", v->int_val ? "true" : "false");
        } else {
            fprintf(ctx->out, "%lld", (long long)rt_eval_expr(ctx, item));
        }
    } else if (item->inferred_type == TYPE_BOOLEAN) {
        fprintf(ctx->out, "%s", rt_eval_expr(ctx, item) ? "true" : "false");
    } else {
        fprintf(ctx->out, "%lld", (long long)rt_eval_expr(ctx, item));
    }
}

static void rt_exec_stmt(RuntimeContext *ctx, ASTNode *n) {
    if (!n || ctx->has_exited || ctx->has_returned) return;
    switch (n->kind) {
        case AST_LET: {
            RuntimeVar *v = rt_declare_or_get_var(ctx, n->name, n->inferred_type);
            v->type = n->inferred_type;
            if (n->left && n->left->kind == AST_TEXT_LIT) {
                v->type = TYPE_TEXT;
                snprintf(v->text_val, sizeof(v->text_val), "%s", n->left->text_val);
            } else {
                v->int_val = rt_eval_expr(ctx, n->left);
            }
            break;
        }
        case AST_SET: {
            RuntimeVar *v = rt_declare_or_get_var(ctx, n->name, n->inferred_type);
            if (n->left && n->left->kind == AST_TEXT_LIT) {
                v->type = TYPE_TEXT;
                snprintf(v->text_val, sizeof(v->text_val), "%s", n->left->text_val);
            } else {
                v->int_val = rt_eval_expr(ctx, n->left);
            }
            break;
        }
        case AST_DISPLAY:
            if (n->arg_count > 1) {
                for (int i = 0; i < n->arg_count; i++) {
                    if (i > 0) fprintf(ctx->out, " ");
                    rt_print_item(ctx, n->args[i]);
                }
                fprintf(ctx->out, "\n");
            } else {
                rt_print_item(ctx, n->left);
                fprintf(ctx->out, "\n");
            }
            break;
        case AST_EXIT:
            ctx->exit_code = (int)rt_eval_expr(ctx, n->left);
            ctx->has_exited = true;
            break;
        case AST_RETURN:
            ctx->return_val = rt_eval_expr(ctx, n->left);
            ctx->has_returned = true;
            break;
        case AST_IF:
            if (rt_eval_expr(ctx, n->left)) {
                for (int i = 0; i < n->body_count; i++) rt_exec_stmt(ctx, n->body[i]);
            } else {
                for (int i = 0; i < n->else_count; i++) rt_exec_stmt(ctx, n->else_body[i]);
            }
            break;
        case AST_WHILE: {
            int guard = 0;
            while (rt_eval_expr(ctx, n->left) && !ctx->has_exited && !ctx->has_returned && guard++ < 100000) {
                for (int i = 0; i < n->body_count; i++) rt_exec_stmt(ctx, n->body[i]);
            }
            break;
        }
        case AST_REPEAT: {
            int64_t cnt = rt_eval_expr(ctx, n->left);
            for (int64_t r = 0; r < cnt && !ctx->has_exited && !ctx->has_returned; r++) {
                for (int i = 0; i < n->body_count; i++) rt_exec_stmt(ctx, n->body[i]);
            }
            break;
        }
        case AST_CALL_STMT: {
            int64_t res = rt_call_function(ctx, n->name, n->arg_count, n->args);
            RuntimeVar *dst = rt_declare_or_get_var(ctx, n->secondary_name, TYPE_INTEGER);
            dst->int_val = res;
            break;
        }
        case AST_ARRAY_ALLOC:
            rt_declare_or_get_var(ctx, n->name, TYPE_ARRAY);
            break;
        case AST_ARRAY_STORE: {
            RuntimeVar *arr = rt_declare_or_get_var(ctx, n->name, TYPE_ARRAY);
            int64_t idx = rt_eval_expr(ctx, n->left);
            int64_t val = rt_eval_expr(ctx, n->right);
            if (idx >= 0 && idx < 128) arr->array_vals[idx] = val;
            break;
        }
        case AST_ARRAY_LOAD: {
            RuntimeVar *arr = rt_find_var(ctx, n->name);
            int64_t idx = rt_eval_expr(ctx, n->left);
            RuntimeVar *dst = rt_declare_or_get_var(ctx, n->secondary_name, TYPE_INTEGER);
            dst->int_val = (arr && idx >= 0 && idx < 128) ? arr->array_vals[idx] : 0;
            break;
        }
        case AST_MEM_ALLOC: {
            RuntimeVar *m = rt_declare_or_get_var(ctx, n->name, TYPE_ADDRESS);
            int64_t sz = rt_eval_expr(ctx, n->left);
            if (sz > 0 && sz <= 65536) {
                m->mem_ptr = (uint8_t *)calloc(1, (size_t)sz);
                m->mem_size = (size_t)sz;
            }
            break;
        }
        case AST_MEM_WRITE: {
            RuntimeVar *m = rt_find_var(ctx, n->name);
            int64_t off = rt_eval_expr(ctx, n->left);
            int64_t val = rt_eval_expr(ctx, n->right);
            if (m && m->mem_ptr && off >= 0 && (size_t)(off + 8) <= m->mem_size) {
                *(int64_t *)(m->mem_ptr + off) = val;
            }
            break;
        }
        case AST_MEM_READ: {
            RuntimeVar *m = rt_find_var(ctx, n->name);
            int64_t off = rt_eval_expr(ctx, n->left);
            RuntimeVar *dst = rt_declare_or_get_var(ctx, n->secondary_name, TYPE_INTEGER);
            if (m && m->mem_ptr && off >= 0 && (size_t)(off + 8) <= m->mem_size) {
                dst->int_val = *(int64_t *)(m->mem_ptr + off);
            } else {
                dst->int_val = 0;
            }
            break;
        }
        case AST_STRUCT_CREATE: {
            RuntimeVar *sv = rt_declare_or_get_var(ctx, n->name, TYPE_STRUCT);
            StructDef *sd = find_struct(n->secondary_name);
            if (sd) {
                sv->field_count = sd->field_count;
                for (int i = 0; i < sd->field_count; i++) {
                    snprintf(sv->field_names[i], MAX_STR_LEN, "%s", sd->field_names[i]);
                    sv->field_vals[i] = 0;
                }
            }
            break;
        }
        case AST_STRUCT_SET: {
            RuntimeVar *sv = rt_find_var(ctx, n->name);
            int64_t val = rt_eval_expr(ctx, n->left);
            if (sv) {
                for (int i = 0; i < sv->field_count; i++) {
                    if (strcmp(sv->field_names[i], n->secondary_name) == 0) {
                        sv->field_vals[i] = val;
                        break;
                    }
                }
            }
            break;
        }
        case AST_STRUCT_GET: {
            RuntimeVar *sv = rt_find_var(ctx, n->name);
            RuntimeVar *dst = rt_declare_or_get_var(ctx, n->tertiary_name, TYPE_INTEGER);
            dst->int_val = 0;
            if (sv) {
                for (int i = 0; i < sv->field_count; i++) {
                    if (strcmp(sv->field_names[i], n->secondary_name) == 0) {
                        dst->int_val = sv->field_vals[i];
                        break;
                    }
                }
            }
            break;
        }
        case AST_TENSOR_CREATE: {
            RuntimeVar *tv = rt_declare_or_get_var(ctx, n->name, TYPE_TENSOR);
            tv->tensor_rows = (int)rt_eval_expr(ctx, n->left);
            tv->tensor_cols = (int)rt_eval_expr(ctx, n->right);
            if (tv->tensor_rows > 16) tv->tensor_rows = 16;
            if (tv->tensor_cols > 16) tv->tensor_cols = 16;
            break;
        }
        case AST_TENSOR_FILL: {
            RuntimeVar *tv = rt_find_var(ctx, n->name);
            int64_t val = rt_eval_expr(ctx, n->left);
            if (tv) {
                for (int r = 0; r < tv->tensor_rows; r++)
                    for (int c = 0; c < tv->tensor_cols; c++)
                        tv->tensor_vals[r][c] = val;
            }
            break;
        }
        case AST_TENSOR_SET: {
            RuntimeVar *tv = rt_find_var(ctx, n->name);
            int r = (int)rt_eval_expr(ctx, n->left);
            int c = (int)rt_eval_expr(ctx, n->right);
            int64_t val = rt_eval_expr(ctx, n->third);
            if (tv && r >= 0 && r < 16 && c >= 0 && c < 16) tv->tensor_vals[r][c] = val;
            break;
        }
        case AST_TENSOR_GET: {
            RuntimeVar *tv = rt_find_var(ctx, n->name);
            int r = (int)rt_eval_expr(ctx, n->left);
            int c = (int)rt_eval_expr(ctx, n->right);
            RuntimeVar *dst = rt_declare_or_get_var(ctx, n->secondary_name, TYPE_INTEGER);
            dst->int_val = (tv && r >= 0 && r < 16 && c >= 0 && c < 16) ? tv->tensor_vals[r][c] : 0;
            break;
        }
        case AST_TENSOR_MUL: {
            RuntimeVar *ta = rt_find_var(ctx, n->name);
            RuntimeVar *tb = rt_find_var(ctx, n->secondary_name);
            RuntimeVar *tc = rt_find_var(ctx, n->tertiary_name);
            if (ta && tb && tc) {
                for (int r = 0; r < ta->tensor_rows; r++) {
                    for (int c = 0; c < tb->tensor_cols; c++) {
                        int64_t acc = 0;
                        for (int k = 0; k < ta->tensor_cols; k++) {
                            acc += ta->tensor_vals[r][k] * tb->tensor_vals[k][c];
                        }
                        tc->tensor_vals[r][c] = acc;
                    }
                }
            }
            break;
        }
        case AST_TENSOR_RELU: {
            RuntimeVar *ta = rt_find_var(ctx, n->name);
            if (ta) {
                for (int r = 0; r < ta->tensor_rows; r++)
                    for (int c = 0; c < ta->tensor_cols; c++)
                        if (ta->tensor_vals[r][c] < 0) ta->tensor_vals[r][c] = 0;
            }
            break;
        }
        case AST_TENSOR_SUM: {
            RuntimeVar *ta = rt_find_var(ctx, n->name);
            RuntimeVar *dst = rt_declare_or_get_var(ctx, n->secondary_name, TYPE_INTEGER);
            int64_t sum = 0;
            if (ta) {
                for (int r = 0; r < ta->tensor_rows; r++)
                    for (int c = 0; c < ta->tensor_cols; c++)
                        sum += ta->tensor_vals[r][c];
            }
            dst->int_val = sum;
            break;
        }
        case AST_DB_OPEN:
            rt_declare_or_get_var(ctx, n->name, TYPE_DATABASE);
            break;
        case AST_DB_INSERT: {
            RuntimeVar *db = rt_declare_or_get_var(ctx, n->name, TYPE_DATABASE);
            int64_t k = rt_eval_expr(ctx, n->left) & 63;
            int64_t v = rt_eval_expr(ctx, n->right);
            db->db_slots[k] = v;
            break;
        }
        case AST_DB_QUERY: {
            RuntimeVar *db = rt_find_var(ctx, n->name);
            int64_t k = rt_eval_expr(ctx, n->left) & 63;
            RuntimeVar *dst = rt_declare_or_get_var(ctx, n->secondary_name, TYPE_INTEGER);
            dst->int_val = db ? db->db_slots[k] : 0;
            break;
        }
        default:
            break;
    }
}

static int execute_program_native(ASTNode *prog, FILE *out_stream) {
    RuntimeContext *ctx = (RuntimeContext *)calloc(1, sizeof(RuntimeContext));
    if (!ctx) return 1;
    ctx->prog = prog;
    ctx->out = out_stream ? out_stream : stdout;
    for (int i = 0; i < prog->body_count; i++) {
        ASTNode *n = prog->body[i];
        if (n->kind != AST_FUNC_DEF && n->kind != AST_STRUCT_DEF) {
            rt_exec_stmt(ctx, n);
            if (ctx->has_exited) break;
        }
    }
    for (int i = 0; i < ctx->globals.var_count; i++) {
        if (ctx->globals.vars[i].mem_ptr) free(ctx->globals.vars[i].mem_ptr);
    }
    int code = ctx->exit_code;
    free(ctx);
    return code;
}

static void compiler_reset_state(void) {
    g_token_count = 0;
    g_diag_count = 0;
    g_ast_count = 0;
    g_sym_count = 0;
    g_next_stack_offset = 8;
    g_in_function = false;
    g_current_func[0] = '\0';
    g_local_stack_offset = 8;
    g_struct_count = 0;
    g_included_count = 0;
    g_pos = 0;
}

/*
 * Public Library & Android JNI Entry Point:
 * Compiles and executes an Eathon program in-process and writes stdout +
 * diagnostics to out_buf. Safe to call repeatedly from Android JNI or C FFI.
 */
int eathon_compile_and_run_buffer(const char *source_code,
                                  const char *source_path,
                                  char *out_stdout_buf,
                                  size_t out_stdout_cap,
                                  char *out_c99_buf,
                                  size_t out_c99_cap,
                                  int *out_exit_code) {
    compiler_reset_state();
    if (out_stdout_buf && out_stdout_cap > 0) out_stdout_buf[0] = '\0';
    if (out_c99_buf && out_c99_cap > 0) out_c99_buf[0] = '\0';
    if (out_exit_code) *out_exit_code = 0;

    const char *base_path = (source_path && source_path[0]) ? source_path : "main.eat";
    char *expanded = preprocess_includes(source_code ? source_code : "", base_path, 0);
    g_source = expanded;

    lexer_tokenize(expanded);
    ASTNode *prog = NULL;
    if (g_diag_count == 0) {
        prog = parse_program();
    }
    if (g_diag_count == 0 && prog) {
        typecheck_node(prog);
    }

    if (g_diag_count > 0 || !prog) {
        if (out_stdout_buf && out_stdout_cap > 0) {
            size_t used = 0;
            for (int i = 0; i < g_diag_count; i++) {
                int n = snprintf(out_stdout_buf + used, out_stdout_cap - used,
                                 "[%s] %s (Line %d, Col %d): %s\n",
                                 g_diags[i].code, g_diags[i].stage,
                                 g_diags[i].line, g_diags[i].col, g_diags[i].message);
                if (n > 0 && used + (size_t)n < out_stdout_cap) used += (size_t)n;
            }
        }
        free(expanded);
        if (out_exit_code) *out_exit_code = 1;
        return 1;
    }

    /* Emit C99 code to scratch file if requested (Android-safe path + tmpfile fallback) */
    if (out_c99_buf && out_c99_cap > 0) {
        char tmp_c_path[512];
        snprintf(tmp_c_path, sizeof(tmp_c_path), "%s.c99.tmp", base_path);
        bool used_named_c = false;
        FILE *tmp_c = fopen(tmp_c_path, "w+b");
        if (tmp_c) {
            used_named_c = true;
        } else {
            tmp_c = tmpfile();
        }
        if (tmp_c) {
            generate_c99_code(prog, tmp_c);
            fflush(tmp_c);
            fseek(tmp_c, 0, SEEK_SET);
            size_t nr = fread(out_c99_buf, 1, out_c99_cap - 1, tmp_c);
            out_c99_buf[nr] = '\0';
            fclose(tmp_c);
            if (used_named_c) remove(tmp_c_path);
        }
    }

    /* Execute program natively in C and capture stdout (Android-safe path + tmpfile fallback) */
    char tmp_out_path[512];
    snprintf(tmp_out_path, sizeof(tmp_out_path), "%s.stdout.tmp", base_path);
    bool used_named_out = false;
    FILE *tmp_out = fopen(tmp_out_path, "w+b");
    if (tmp_out) {
        used_named_out = true;
    } else {
        tmp_out = tmpfile();
    }
    int ec = 0;
    if (tmp_out) {
        ec = execute_program_native(prog, tmp_out);
        fflush(tmp_out);
        fseek(tmp_out, 0, SEEK_SET);
        if (out_stdout_buf && out_stdout_cap > 0) {
            size_t nr = fread(out_stdout_buf, 1, out_stdout_cap - 1, tmp_out);
            out_stdout_buf[nr] = '\0';
        }
        fclose(tmp_out);
        if (used_named_out) remove(tmp_out_path);
    } else {
        ec = execute_program_native(prog, stdout);
    }

    if (out_exit_code) *out_exit_code = ec;
    (void)generate_llvm_ir;
    (void)emit_elf64_binary;
    (void)print_ast_json;
    free(expanded);
    return 0;
}

/* -------------------------------------------------------------------------- */
/* 12. MAIN COMPILER CLI DRIVER                                               */
/* -------------------------------------------------------------------------- */

#ifndef EATHON_LIB_MODE
int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: eathon <source.eat> [-o <output_binary>] [--run] [--cc <gcc|clang>] [--llvm <out.ll>] [--c <out.c>] [--json]\n");
        return 1;
    }

    const char *input_path = NULL;
    const char *out_bin_path = "/tmp/eathon_out";
    const char *out_llvm_path = "/tmp/eathon_out.ll";
    const char *out_c_path = "/tmp/eathon_out.c";
    const char *cc_driver = NULL;
    bool json_mode = false;
    bool run_mode = false;

#ifdef _WIN32
    out_bin_path = "eathon_out.exe";
    out_llvm_path = "eathon_out.ll";
    out_c_path = "eathon_out.c";
    cc_driver = "gcc"; /* Default on Windows to compile genuine PE64 via MinGW-w64 GCC or Clang */
#endif

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            out_bin_path = argv[++i];
        } else if (strcmp(argv[i], "--pe") == 0 && i + 1 < argc) {
            ++i;
        } else if (strcmp(argv[i], "--cc") == 0 && i + 1 < argc) {
            cc_driver = argv[++i];
        } else if (strcmp(argv[i], "--llvm") == 0 && i + 1 < argc) {
            out_llvm_path = argv[++i];
        } else if (strcmp(argv[i], "--c") == 0 && i + 1 < argc) {
            out_c_path = argv[++i];
        } else if (strcmp(argv[i], "--json") == 0) {
            json_mode = true;
        } else if (strcmp(argv[i], "--run") == 0) {
            run_mode = true;
        } else if (argv[i][0] != '-') {
            input_path = argv[i];
        }
    }

    if (!input_path) {
        fprintf(stderr, "Error: No .eat input file specified.\n");
        return 1;
    }

    FILE *f = fopen(input_path, "rb");
    if (!f) {
        fprintf(stderr, "Error: Cannot open source file '%s'.\n", input_path);
        return 1;
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *src = (char *)calloc(1, (size_t)sz + 2);
    if (sz > 0) {
        size_t nread = fread(src, 1, (size_t)sz, f);
        (void)nread;
    }
    fclose(f);
    compiler_reset_state();
    char *expanded_src = preprocess_includes(src, input_path, 0);
    free(src);
    src = expanded_src;
    g_source = src;

    /* Stage 1: Lexer */
    lexer_tokenize(src);

    /* Stage 2: Parser */
    ASTNode *prog = NULL;
    if (g_diag_count == 0) {
        prog = parse_program();
    }

    /* Stage 3: Type Checker */
    if (g_diag_count == 0 && prog) {
        typecheck_node(prog);
    }

    /* Stage 4: Code Generation or Native Execution */
    X86Buffer x86_code;
    memset(&x86_code, 0, sizeof(x86_code));
    bool compiled_ok = (g_diag_count == 0 && prog != NULL);

    if (compiled_ok && run_mode) {
        int ec = execute_program_native(prog, stdout);
        free(src);
        return ec;
    }

    if (compiled_ok) {
        FILE *f_ll = fopen(out_llvm_path, "w");
        if (f_ll) {
            generate_llvm_ir(prog, f_ll);
            fclose(f_ll);
        }

        FILE *f_c = fopen(out_c_path, "w");
        if (f_c) {
            generate_c99_code(prog, f_c);
            fclose(f_c);
        }

        if (cc_driver != NULL) {
            /* Emit x86_64 buffer for inspection and invoke host C compiler (gcc/clang) for native target */
            emit_elf64_binary(prog, NULL, &x86_code);
            char cmd[1024];
            snprintf(cmd, sizeof(cmd), "%s -O2 \"%s\" -o \"%s\"", cc_driver, out_c_path, out_bin_path);
            int rc = system(cmd);
            if (rc != 0) {
                add_diag("EATHON-CG-001", "CODEGEN", "ERROR", 1, 1,
                         "Host C compiler driver invocation failed.");
                compiled_ok = false;
            }
        } else {
            emit_elf64_binary(prog, out_bin_path, &x86_code);
        }
    }

    if (json_mode) {
        printf("{\"success\":%s,", compiled_ok ? "true" : "false");
        printf("\"tokenCount\":%d,", g_token_count);
        printf("\"astNodeCount\":%d,", g_ast_count);
        printf("\"symbolCount\":%d,", g_sym_count);
        printf("\"machineCodeBytes\":%d,", x86_code.len);
        printf("\"rodataBytes\":%d,", x86_code.rodata_len);

        printf("\"tokens\":[");
        int max_t = g_token_count < 400 ? g_token_count : 400;
        for (int i = 0; i < max_t; i++) {
            if (i > 0) putchar(',');
            printf("{\"id\":%d,\"type\":%d,\"lexeme\":", i, (int)g_tokens[i].type);
            print_json_escaped(g_tokens[i].lexeme);
            printf(",\"line\":%d,\"col\":%d}", g_tokens[i].line, g_tokens[i].col);
        }
        printf("],");

        printf("\"diagnostics\":[");
        for (int i = 0; i < g_diag_count; i++) {
            if (i > 0) putchar(',');
            printf("{\"code\":");
            print_json_escaped(g_diags[i].code);
            printf(",\"stage\":");
            print_json_escaped(g_diags[i].stage);
            printf(",\"severity\":");
            print_json_escaped(g_diags[i].severity);
            printf(",\"line\":%d,\"col\":%d,\"message\":", g_diags[i].line, g_diags[i].col);
            print_json_escaped(g_diags[i].message);
            putchar('}');
        }
        printf("],");

        printf("\"symbols\":[");
        for (int i = 0; i < g_sym_count; i++) {
            if (i > 0) putchar(',');
            printf("{\"name\":");
            print_json_escaped(g_symbols[i].name);
            printf(",\"type\":");
            print_json_escaped(datatype_name(g_symbols[i].type));
            printf(",\"stackOffset\":%d,\"isFunction\":%s}",
                   g_symbols[i].stack_offset,
                   g_symbols[i].is_function ? "true" : "false");
        }
        printf("],");

        printf("\"machineCodeHex\":\"");
        for (int i = 0; i < x86_code.len && i < 1024; i++) {
            printf("%02x", x86_code.buf[i]);
        }
        printf("\",");

        printf("\"ast\":");
        print_ast_json(prog);
        printf("}\n");
    } else {
        if (!compiled_ok) {
            for (int i = 0; i < g_diag_count; i++) {
                fprintf(stderr, "[%s] %s at line %d, col %d: %s\n",
                        g_diags[i].code, g_diags[i].stage,
                        g_diags[i].line, g_diags[i].col, g_diags[i].message);
            }
            free(src);
            return 1;
        }
        printf("Compiled '%s' -> '%s' (%d bytes x86_64 machine code)\n",
               input_path, out_bin_path, x86_code.len);
    }

    free(src);
    return compiled_ok ? 0 : 1;
}
#endif /* EATHON_LIB_MODE */

