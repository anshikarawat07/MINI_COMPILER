#ifndef SEMANTIC_H
#define SEMANTIC_H

#include "parser.h"

/* ===============================================
   LIMITS
   =============================================== */
#define MAX_SYMBOLS     256
#define MAX_FUNCTIONS   64
#define SYM_NAME_LEN    64

/* ===============================================
   DATA TYPES
   =============================================== */
typedef enum {
    TYPE_INT,
    TYPE_BOOL,
    TYPE_CHAR,
    TYPE_VOID,
    TYPE_UNKNOWN
} DataType;

/* ===============================================
   SYMBOL KIND
   =============================================== */
typedef enum {
    SYM_VAR,
    SYM_FUNC
} SymbolKind;

/* ===============================================
   SYMBOL ENTRY
   =============================================== */
typedef struct {
    char       name[SYM_NAME_LEN];
    SymbolKind kind;
    DataType   type;

    int scopeLevel;
    int declLine;

    int isUsed;      /* for unused variable warning */
    int initialized; /* 0: No, 1: Yes, 2: Potential */
} Symbol;

/* ===============================================
   SYMBOL TABLE
   =============================================== */
typedef struct {
    Symbol symbols[MAX_SYMBOLS];
    int    count;
    int    currentScope;
} SymbolTable;

/* ===============================================
   FUNCTION TABLE ENTRY
   =============================================== */
typedef struct {
    char     name[SYM_NAME_LEN];
    DataType returnType;
    int      declLine;
} FuncEntry;

/* ===============================================
   SEMANTIC STATS
   =============================================== */
typedef struct {
    int symbolsResolved;
    int typeChecks;
    int scopesPushed;
    int warnings;
    int errors;
} SemanticStats;

/* ===============================================
   PUBLIC FUNCTION
   =============================================== */
int runSemantic(ASTNode *tree);
/* ===============================================
   FORWARD DECLARATIONS   ✅ ADD THIS
   =============================================== */
static void analyzeNode(ASTNode *node);
static DataType analyzeExpr(ASTNode *node);
#endif