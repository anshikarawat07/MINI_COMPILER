#ifndef PARSER_H
#define PARSER_H

#include "lexer.h"

/* ===============================================
   AST NODE TYPES
   =============================================== */
typedef enum {

    NODE_PROGRAM,
    NODE_FUNC,
    NODE_VAR_DECL,
    NODE_SET,
    NODE_PRINT,
    NODE_READ,
    NODE_IF,
    NODE_ELSE,
    NODE_WHILE,
    NODE_FOR,
    NODE_RET,
    NODE_CALL,
    NODE_SECURE_BLOCK,
    NODE_BREAK,
    NODE_CONTINUE,
    NODE_INT_LITERAL,
    NODE_CHAR_LITERAL,
    NODE_BOOL_LITERAL,
    NODE_IDENTIFIER,
    NODE_BINOP,
    NODE_UNOP

} NodeType;

/* ===============================================
   AST NODE STRUCTURE
   =============================================== */
#define MAX_CHILDREN  10
#define MAX_NAME      64

typedef struct ASTNode {

    NodeType       type;
    char           name[MAX_NAME];
    char           dataType[MAX_NAME];
    int            line;
    int            endLine;
    int            isSecure;

    struct ASTNode *children[MAX_CHILDREN];
    int             childCount;

} ASTNode;

/* ===============================================
   PARSER STATS
   =============================================== */
typedef struct {
    int totalStatements;
    int totalFunctions;
    int totalVariables;
    int totalAssignments;
    int totalPrints;
    int totalReturns;
    int totalSecureBlocks;
    int totalIf;
    int totalWhile;
    int totalFor;
    int errors;
} ParserStats;
/* ===============================================
   PARSER STATE
   =============================================== */
typedef struct {
    Token current;
    Token previous;
    int   hadError;
    int   inSecure;
} Parser;

/* ===============================================
   FUNCTION DECLARATIONS
   =============================================== */
void     initParser  (FILE *fp);
ASTNode *parseProgram(void);
void     printAST    (ASTNode *node, int depth);
void     runParser   (FILE *fp);
void     freeAST     (ASTNode *node);

#endif