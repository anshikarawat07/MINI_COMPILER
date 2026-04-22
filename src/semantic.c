#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "semantic.h"

/* ===============================================
   GLOBAL STATE
   =============================================== */
static SymbolTable symTable;
static FuncEntry   funcTable[MAX_FUNCTIONS];
static int         funcCount = 0;
static SemanticStats stats;
static int         currentFuncRet = TYPE_VOID;
static int         inUncertainBlock = 0;

/* ===============================================
   FORWARD DECLARATION
   =============================================== */
static void analyzeNode(ASTNode *node);
static DataType analyzeExpr(ASTNode *node);

/* ===============================================
   HELPERS
   =============================================== */
static DataType resolveTypeName(const char *name) {
    if (!name) return TYPE_UNKNOWN;
    if (strcmp(name, "INT") == 0) return TYPE_INT;
    if (strcmp(name, "BOOL") == 0) return TYPE_BOOL;
    if (strcmp(name, "CHAR") == 0) return TYPE_CHAR;
    return TYPE_UNKNOWN;
}

/* ===============================================
   ERROR / WARNING
   =============================================== */
static void semError(int line, const char *msg, const char *detail) {
    printf("   [SEMANTIC] ERROR   line %-4d : %s '%s'\n", line, msg, detail);
    stats.errors++;
}

static void semWarn(int line, const char *msg, const char *detail) {
    printf("   [SEMANTIC] WARNING line %-4d : %s '%s'\n", line, msg, detail);
    stats.warnings++;
}

/* ===============================================
   SCOPE
   =============================================== */
static void pushScope() {
    symTable.currentScope++;
    stats.scopesPushed++;
}

static void popScope() {
    for (int i = 0; i < symTable.count; i++) {
        Symbol *s = &symTable.symbols[i];
        if (s->scopeLevel == symTable.currentScope &&
            s->kind == SYM_VAR && !s->isUsed) {
            semWarn(s->declLine,
                    "Variable declared but never used",
                    s->name);
        }
    }

    int newCount = 0;
    for (int i = 0; i < symTable.count; i++) {
        if (symTable.symbols[i].scopeLevel < symTable.currentScope)
            symTable.symbols[newCount++] = symTable.symbols[i];
    }
    symTable.count = newCount;
    symTable.currentScope--;
}

/* ===============================================
   SYMBOL TABLE
   =============================================== */
static void declareSymbol(const char *name, SymbolKind kind,
                          DataType type, int line) {

    for (int i = 0; i < symTable.count; i++) {
        if (symTable.symbols[i].scopeLevel == symTable.currentScope &&
            strcmp(symTable.symbols[i].name, name) == 0) {
            semError(line, "Duplicate variable", name);
            return;
        }
    }

    Symbol *sym = &symTable.symbols[symTable.count++];
    strcpy(sym->name, name);
    sym->kind = kind;
    sym->type = type;
    sym->scopeLevel = symTable.currentScope;
    sym->declLine = line;
    sym->isUsed = 0;
    sym->initialized = 0;

    stats.symbolsResolved++;   // ⭐ important
}

static Symbol* lookupSymbol(const char *name) {
    for (int i = symTable.count - 1; i >= 0; i--) {
        if (strcmp(symTable.symbols[i].name, name) == 0)
            return &symTable.symbols[i];
    }
    return NULL;
}

static void mark_initialized(Symbol *sym) {
    if (!sym) return;
    /* Basic Safe Approximation:
       If inside IF/WHILE/FOR, mark as Potential (2)
       UNLESS it was already definitely initialized (1) */
    if (sym->initialized != 1) {
        sym->initialized = (inUncertainBlock > 0) ? 2 : 1;
    }
}

/* ===============================================
   FUNCTION TABLE
   =============================================== */
static void declareFunc(const char *name, DataType retType, int line) {

    for (int i = 0; i < funcCount; i++) {
        if (strcmp(funcTable[i].name, name) == 0) {
            semError(line, "Function already declared", name);
            return;
        }
    }

    strcpy(funcTable[funcCount].name, name);
    funcTable[funcCount].returnType = retType;
    funcTable[funcCount].declLine = line;
    funcCount++;
}

/* ===============================================
   EXPRESSION
   =============================================== */
static DataType analyzeExpr(ASTNode *node) {
    if (!node) return TYPE_UNKNOWN;

    switch (node->type) {

        case NODE_INT_LITERAL:
            stats.typeChecks++;
            return TYPE_INT;

        case NODE_IDENTIFIER: {
            Symbol *sym = lookupSymbol(node->name);
            if (!sym) {
                semError(node->line, "Undeclared variable", node->name);
                return TYPE_UNKNOWN;
            }
            sym->isUsed = 1;

            if (sym->initialized == 0) {
                semError(node->line, "Variable used before initialization", node->name);
            } else if (sym->initialized == 2) {
                semWarn(node->line, "Variable possibly used before initialization", node->name);
            }

            stats.typeChecks++;
            return sym->type;
        }

        case NODE_BINOP: {
            DataType l = analyzeExpr(node->children[0]);
            DataType r = analyzeExpr(node->children[1]);
            stats.typeChecks++;

            if (l != r)
                semError(node->line, "Type mismatch", node->name);

            return l;
        }

        default:
            return TYPE_UNKNOWN;
    }
}

/* ===============================================
   STATEMENTS
   =============================================== */
static void analyzeVarDecl(ASTNode *node) {
    DataType t = resolveTypeName(node->dataType);
    declareSymbol(node->name, SYM_VAR, t, node->line);
}

static void analyzeSet(ASTNode *node) {
    Symbol *sym = lookupSymbol(node->name);

    if (!sym) {
        semError(node->line, "Undeclared variable", node->name);
        return;
    }

    sym->isUsed = 1;

    if (node->childCount > 0) {
        int errBefore = stats.errors;
        DataType rhs = analyzeExpr(node->children[0]);
        
        if (sym->type != rhs && rhs != TYPE_UNKNOWN)
            semError(node->line, "Assignment type mismatch", node->name);

        /* Only mark initialized if no new errors (propagation) */
        if (stats.errors == errBefore) {
            mark_initialized(sym);
        } else {
            /* Suppress cascade errors for this variable */
            sym->initialized = 3; 
        }
    }
}

static void analyzePrint(ASTNode *node) {
    if (node->childCount > 0)
        analyzeExpr(node->children[0]);
}

static void analyzeFunc(ASTNode *node) {

    DataType retType = resolveTypeName(node->dataType);

    declareFunc(node->name, retType, node->line);
    declareSymbol(node->name, SYM_FUNC, retType, node->line);

    pushScope();

    for (int i = 0; i < node->childCount; i++)
        analyzeNode(node->children[i]);

    popScope();
}

/* ===============================================
   MAIN ANALYSIS
   =============================================== */
static void analyzeNode(ASTNode *node) {
    if (!node) return;

    switch (node->type) {

        case NODE_PROGRAM:
            for (int i = 0; i < node->childCount; i++)
                analyzeNode(node->children[i]);
            break;

        case NODE_FUNC:     analyzeFunc(node); break;
        case NODE_VAR_DECL: analyzeVarDecl(node); break;
        case NODE_SET:      analyzeSet(node); break;
        case NODE_PRINT:    analyzePrint(node); break;

        case NODE_IF:
            /* condition is child[0] */
            analyzeExpr(node->children[0]);
            
            inUncertainBlock++;
            /* analyze then block (all children except cond and else) */
            for (int i = 1; i < node->childCount; i++) {
                if (node->children[i]->type != NODE_ELSE)
                    analyzeNode(node->children[i]);
            }
            
            /* analyze else block if exists */
            for (int i = 1; i < node->childCount; i++) {
                if (node->children[i]->type == NODE_ELSE)
                    analyzeNode(node->children[i]);
            }
            inUncertainBlock--;
            break;

        case NODE_WHILE:
        case NODE_FOR:
            /* loop condition/bounds might use variables */
            for (int i = 0; i < node->childCount; i++) {
                if (i == 0 || (node->type == NODE_FOR && i < 3))
                     analyzeExpr(node->children[i]);
            }

            inUncertainBlock++;
            for (int i = (node->type == NODE_FOR ? 3 : 1); i < node->childCount; i++)
                analyzeNode(node->children[i]);
            inUncertainBlock--;
            break;

        case NODE_SECURE_BLOCK:
            pushScope();
            for (int i = 0; i < node->childCount; i++)
                analyzeNode(node->children[i]);
            popScope();
            break;

        case NODE_ELSE:
            for (int i = 0; i < node->childCount; i++)
                analyzeNode(node->children[i]);
            break;

        default:
            break;
    }
}

/* ===============================================
   RUN SEMANTIC
   =============================================== */
int runSemantic(ASTNode *tree) {

    memset(&symTable, 0, sizeof(symTable));
    memset(&stats, 0, sizeof(stats));
    funcCount = 0;
    inUncertainBlock = 0;

    printf("\n==========================================\n");
    printf("    PHASE 3 : SEMANTIC ANALYSIS\n");
    printf("==========================================\n\n");

    analyzeNode(tree);

    printf("\n------------------------------------------\n");
    printf("  SUMMARY\n");
    printf("------------------------------------------\n");
    printf("  Symbols resolved  : %d\n", stats.symbolsResolved);
    printf("  Type checks       : %d\n", stats.typeChecks);
    printf("  Scopes pushed     : %d\n", stats.scopesPushed);
    printf("  Warnings          : %d\n", stats.warnings);
    printf("  Errors found      : %d\n", stats.errors);
    printf("------------------------------------------\n");

    if (stats.errors == 0)
        printf("  PHASE 3 COMPLETE\n");
    else
        printf("  PHASE 3 FAILED\n");

    printf("==========================================\n\n");

    return stats.errors;
}