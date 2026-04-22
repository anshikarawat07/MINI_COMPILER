#ifndef LEXER_H
#define LEXER_H

#include <stdio.h>

extern FILE *sourceFile;
extern int   securityDone;  /* 0 = run security, 1 = skip */

/* ===============================================
   TOKEN TYPES
   =============================================== */
typedef enum {

    /* Keywords */
    TOK_FUNC, TOK_VAR, TOK_SET, TOK_IF, TOK_ELSE,
    TOK_WHILE, TOK_FOR, TOK_FROM, TOK_TO, TOK_STEP,
    TOK_RET, TOK_CALL,
    TOK_PRINT, TOK_READ,
    TOK_BREAK, TOK_CONTINUE,

    /* Security */
    TOK_SECURE_START,
    TOK_SECURE_END,

    /* Data Types */
    TOK_INT, TOK_BOOL, TOK_CHAR,

    /* Boolean Values */
    TOK_TRUE, TOK_FALSE,

    /* Literals */
    TOK_INT_LITERAL,
    TOK_CHAR_LITERAL,

    /* Identifiers */
    TOK_IDENTIFIER,

    /* Arithmetic Operators */
    TOK_PLUS, TOK_MINUS, TOK_MUL, TOK_DIV, TOK_MOD,

    /* Relational Operators */
    TOK_EQ, TOK_NEQ, TOK_LT, TOK_GT, TOK_LE, TOK_GE,

    /* Logical Operators */
    TOK_AND, TOK_OR, TOK_NOT,

    /* Symbols */
    TOK_LPAREN, TOK_RPAREN,
    TOK_COLON, TOK_COMMA,
    TOK_LBRACKET, TOK_RBRACKET,

    /* Special */
    TOK_END,
    TOK_EOF,
    TOK_UNKNOWN

} TokenType;

/* ===============================================
   TOKEN STRUCTURE
   =============================================== */
typedef struct {
    TokenType type;
    char      lexeme[64];
    int       line;
} Token;

/* ===============================================
   CONSTANTS
   =============================================== */
#define MAX_SECURE_CONTENT  4096
#define HASH_SIZE           9
#define MAX_BLOCKS          10

/* ===============================================
   SECURE BLOCK STRUCTURE
   =============================================== */
typedef struct {
    char content[MAX_SECURE_CONTENT];
    char computedHash[HASH_SIZE];
    int  startLine;
    int  endLine;
    int  blockNumber;
} SecureBlock;

/* ===============================================
   LOCK FILE STRUCTURE
   =============================================== */
typedef struct {
    int  totalBlocks;
    char blockHash[MAX_BLOCKS][HASH_SIZE];
    char blockLock[MAX_BLOCKS][HASH_SIZE];
} LockFile;

/* ===============================================
   FUNCTION DECLARATIONS
   =============================================== */
void        initLexer           (FILE *fp);
void        initLexerWithFile   (FILE *fp, const char *filename);
Token       getNextToken        (void);
int         printAllTokens      (FILE *fp); /* returns 1=ok 0=blocked */
const char *getTokenTypeName    (TokenType type);
unsigned int simpleHash         (const char *str);
void         hashToHex          (unsigned int hash, char *out);

#endif