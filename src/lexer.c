#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include "lexer.h"

FILE *sourceFile;
int   securityDone = 0;

static int  line           = 1;
static int  blockCount     = 0;
static LockFile gLockFile;
static int      gIsFirstTime  = 0;
static char     gFilename[256] = {0};

/* ===============================================
   INIT LEXER
   =============================================== */
void initLexer(FILE *fp) {
    sourceFile = fp;
    line       = 1;
    blockCount = 0;
}

void initLexerWithFile(FILE *fp, const char *filename) {
    strncpy(gFilename, filename, 255);
    initLexer(fp);
}

/* ===============================================
   HASH FUNCTION
   =============================================== */
unsigned int simpleHash(const char *str) {
    unsigned int hash = 5381;
    while (*str != '\0') {
        unsigned char c = (unsigned char)*str;
        hash = hash * 33;
        hash = hash ^ c;
        str++;
    }
    return hash;
}

void hashToHex(unsigned int hash, char *out) {
    char hexDigits[] = "0123456789abcdef";
    for (int i = 7; i >= 0; i--) {
        out[i] = hexDigits[hash % 16];
        hash   = hash / 16;
    }
    out[8] = '\0';
}

/* ===============================================
   LOCK FILE - READ BINARY
   =============================================== */
static int readLockFile(const char *filename, LockFile *lf) {

    char lockname[256];
    strcpy(lockname, filename);
    char *dot = strrchr(lockname, '.');
    if (dot) strcpy(dot, ".lock");
    else      strcat(lockname, ".lock");

    FILE *fp = fopen(lockname, "rb");
    if (!fp) return 0;

    fread(lf, sizeof(LockFile), 1, fp);
    fclose(fp);
    return 1;
}

/* ===============================================
   LOCK FILE - WRITE BINARY
   =============================================== */
static int writeLockFile(const char *filename, LockFile *lf) {

    char lockname[256];
    strncpy(lockname, filename, 250);
    char *dot = strrchr(lockname, '.');
    if (dot) strcpy(dot, ".lock");
    else      strcat(lockname, ".lock");

    FILE *fp = fopen(lockname, "wb");
    if (!fp) {
        printf("   [SECURITY] ERROR: Cannot create lock file!\n");
        return 0;
    }

    fwrite(lf, sizeof(LockFile), 1, fp);
    fclose(fp);

    printf("   [SECURITY] Lock file updated.\n");
    return 1;
}

/* ===============================================
   CAPTURE SECURE BLOCK
   =============================================== */
static int captureSecureBlock(SecureBlock *block) {

    char lineBuf[512];
    int  pos      = 0;
    int  found    = 0;

    memset(block->content,      0, sizeof(block->content));
    memset(block->computedHash, 0, sizeof(block->computedHash));

    printf("   [SECURITY] Capturing block content...\n");

    while (fgets(lineBuf, sizeof(lineBuf), sourceFile)) {

        line++;

        char *trimmed = lineBuf;
        while (*trimmed == ' ' || *trimmed == '\t') trimmed++;

        /* found end marker */
        if (strncmp(trimmed, "#secure_end", 11) == 0) {
            block->endLine = line;
            found = 1;
            break;
        }

        /* skip blank lines */
        if (strlen(trimmed) == 0 || trimmed[0] == '\n') continue;

        /* add to content */
        int len = strlen(lineBuf);
        if (pos + len < MAX_SECURE_CONTENT - 1) {
            strcpy(block->content + pos, lineBuf);
            pos += len;
        } else {
            printf("   [SECURITY] ERROR: Block too large!\n");
            return 0;
        }
    }

    if (!found) {
        printf("   [SECURITY] ERROR: #secure_end missing!\n");
        return 0;
    }

    printf("   [SECURITY] Content captured  : %d chars\n", pos);
    return 1;
}


/* ===============================================
   CHECK BLOCK
   case 1 - first time  -> ask password -> save
   case 2 - hash match  -> safe -> OK
   case 3 - hash differ -> ask password -> allow or block
   =============================================== */
static int checkBlock(SecureBlock *block, int blockNum) {

    hashToHex(simpleHash(block->content),
              block->computedHash);

    printf("   [SECURITY] Computed hash     : %s\n",
           block->computedHash);

    /* CASE 1 - first time */
    if (gIsFirstTime) {

        printf("\n");
        printf("   [SECURITY] Set password for block %d\n\n",
               blockNum);

        char password[64];
        char confirm[64];

        printf("   Enter password for block %d : ", blockNum);
        scanf("%63s", password);

        printf("   Confirm password           : ");
        scanf("%63s", confirm);

        if (strcmp(password, confirm) != 0) {
            printf("\n   [SECURITY] Passwords do not match!\n\n");
            return 0;
        }

        /* save block hash */
        strncpy(gLockFile.blockHash[blockNum - 1],
                block->computedHash, HASH_SIZE - 1);

        /* save lock = password hash */
        char lockHash[HASH_SIZE];
        hashToHex(simpleHash(password), lockHash);
        strncpy(gLockFile.blockLock[blockNum - 1],
                lockHash, HASH_SIZE - 1);

        printf("\n");
        printf("   +------------------------------------------+\n");
        printf("   |  Block %d locked successfully!            |\n",
               blockNum);
        printf("   +------------------------------------------+\n\n");

        return 1;
    }

    /* CASE 2 - hash matches */
    printf("   [SECURITY] Stored hash       : %s\n",
           gLockFile.blockHash[blockNum - 1]);

    if (strcmp(gLockFile.blockHash[blockNum - 1],
               block->computedHash) == 0) {
        printf("   [SECURITY] MATCH!\n");
        printf("   [SECURITY] Block %d is safe. OK\n\n", blockNum);
        return 1;
    }

    /* CASE 3 - hash different */
    printf("\n");
    printf("   +------------------------------------------+\n");
    printf("   |  BLOCK %d WAS MODIFIED!                   |\n", blockNum);
    printf("   |  Enter password to allow this change.    |\n");
    printf("   +------------------------------------------+\n\n");

    char password[64];
    printf("   Enter password for block %d : ", blockNum);
    scanf("%63s", password);

    char checkLock[HASH_SIZE];
    hashToHex(simpleHash(password), checkLock);

    if (strcmp(checkLock,
               gLockFile.blockLock[blockNum - 1]) == 0) {

        /* correct - update hash */
        strncpy(gLockFile.blockHash[blockNum - 1],
                block->computedHash, HASH_SIZE - 1);

        printf("\n");
        printf("   +------------------------------------------+\n");
        printf("   |  Password correct!                       |\n");
        printf("   |  Block %d change accepted.               |\n",
               blockNum);
        printf("   +------------------------------------------+\n\n");
        return 1;

    } else {

        printf("\n");
        printf("   +------------------------------------------+\n");
        printf("   |  WRONG PASSWORD!                         |\n");
        printf("   |  Block %d : COMPILATION BLOCKED!         |\n",
               blockNum);
        printf("   |  Contact the original author to unlock.  |\n");
        printf("   +------------------------------------------+\n\n");
        return 0;
    }
}

/* ===============================================
   KEYWORD CHECK
   =============================================== */
TokenType checkKeyword(const char *str) {
    if (strcmp(str, "FUNC")     == 0) return TOK_FUNC;
    if (strcmp(str, "VAR")      == 0) return TOK_VAR;
    if (strcmp(str, "SET")      == 0) return TOK_SET;
    if (strcmp(str, "IF")       == 0) return TOK_IF;
    if (strcmp(str, "ELSE")     == 0) return TOK_ELSE;
    if (strcmp(str, "WHILE")    == 0) return TOK_WHILE;
    if (strcmp(str, "FOR")      == 0) return TOK_FOR;
    if (strcmp(str, "FROM")     == 0) return TOK_FROM;
    if (strcmp(str, "TO")       == 0) return TOK_TO;
    if (strcmp(str, "STEP")     == 0) return TOK_STEP;
    if (strcmp(str, "RET")      == 0) return TOK_RET;
    if (strcmp(str, "CALL")     == 0) return TOK_CALL;
    if (strcmp(str, "PRINT")    == 0) return TOK_PRINT;
    if (strcmp(str, "READ")     == 0) return TOK_READ;
    if (strcmp(str, "BREAK")    == 0) return TOK_BREAK;
    if (strcmp(str, "CONTINUE") == 0) return TOK_CONTINUE;
    if (strcmp(str, "INT")      == 0) return TOK_INT;
    if (strcmp(str, "BOOL")     == 0) return TOK_BOOL;
    if (strcmp(str, "CHAR")     == 0) return TOK_CHAR;
    if (strcmp(str, "true")     == 0) return TOK_TRUE;
    if (strcmp(str, "false")    == 0) return TOK_FALSE;
    if (strcmp(str, "AND")      == 0) return TOK_AND;
    if (strcmp(str, "OR")       == 0) return TOK_OR;
    if (strcmp(str, "NOT")      == 0) return TOK_NOT;
    if (strcmp(str, "END")      == 0) return TOK_END;
    return TOK_IDENTIFIER;
}

/* ===============================================
   GET NEXT TOKEN
   =============================================== */
Token getNextToken() {

    Token token;
    int   c;

    while ((c = fgetc(sourceFile)) != EOF) {
        if (c == '\n') line++;
        if (!isspace(c)) break;
    }

    if (c == '#') {

        char comment[128];
        int  i = 0;

        while ((c = fgetc(sourceFile)) != EOF && c != '\n') {
            if (i < 127) comment[i++] = c;
        }
        comment[i] = '\0';
        if (c == '\n') line++;

        /* #secure_start */
        if (strcmp(comment, "secure_start") == 0) {

            blockCount++;
            token.type = TOK_SECURE_START;
            strcpy(token.lexeme, "#secure_start");
            token.line = line;

            /* securityDone = 0 means phase 1 - run security */
            /* securityDone = 1 means phase 2+ - skip block  */
           /* ⭐ SAVE POSITION */
long pos = ftell(sourceFile);

if (securityDone == 0) {

    SecureBlock block;
    memset(&block, 0, sizeof(SecureBlock));
    block.startLine   = line;
    block.blockNumber = blockCount;

    if (!captureSecureBlock(&block)) {
        token.type = TOK_UNKNOWN;
        strcpy(token.lexeme, "SECURE_ERROR");
        return token;
    }

    if (!checkBlock(&block, blockCount)) {
        token.type = TOK_UNKNOWN;
        strcpy(token.lexeme, "BLOCKED");
        return token;
    }
}

fseek(sourceFile, pos, SEEK_SET);

            return token;
        }

        /* #secure_end */
        if (strcmp(comment, "secure_end") == 0) {
            token.type = TOK_SECURE_END;
            strcpy(token.lexeme, "#secure_end");
            token.line = line;
            return token;
        }

        /* other comments - skip */
        return getNextToken();
    }

    if (c == EOF) {
        token.type = TOK_EOF;
        strcpy(token.lexeme, "EOF");
        token.line = line;
        return token;
    }

    token.line = line;

    if (isalpha(c)) {
        int i = 0;
        token.lexeme[i++] = c;
        while (isalnum(c = fgetc(sourceFile)) || c == '_') {
            if (i < 63) token.lexeme[i++] = c;
        }
        token.lexeme[i] = '\0';
        ungetc(c, sourceFile);
        token.type = checkKeyword(token.lexeme);
        return token;
    }

    if (isdigit(c)) {
        int i = 0;
        token.lexeme[i++] = c;
        while (isdigit(c = fgetc(sourceFile))) {
            if (i < 63) token.lexeme[i++] = c;
        }
        token.lexeme[i] = '\0';
        ungetc(c, sourceFile);
        token.type = TOK_INT_LITERAL;
        return token;
    }

    if (c == '\'') {
        int ch  = fgetc(sourceFile);
        int end = fgetc(sourceFile);
        if (end != '\'') {
            token.type = TOK_UNKNOWN;
            strcpy(token.lexeme, "INVALID_CHAR");
            return token;
        }
        token.type      = TOK_CHAR_LITERAL;
        token.lexeme[0] = (char)ch;
        token.lexeme[1] = '\0';
        return token;
    }

    switch (c) {
        case '+': token.type = TOK_PLUS;     strcpy(token.lexeme, "+");  break;
        case '-': token.type = TOK_MINUS;    strcpy(token.lexeme, "-");  break;
        case '*': token.type = TOK_MUL;      strcpy(token.lexeme, "*");  break;
        case '/': token.type = TOK_DIV;      strcpy(token.lexeme, "/");  break;
        case '%': token.type = TOK_MOD;      strcpy(token.lexeme, "%");  break;
        case '(': token.type = TOK_LPAREN;   strcpy(token.lexeme, "(");  break;
        case ')': token.type = TOK_RPAREN;   strcpy(token.lexeme, ")");  break;
        case ':': token.type = TOK_COLON;    strcpy(token.lexeme, ":");  break;
        case ',': token.type = TOK_COMMA;    strcpy(token.lexeme, ",");  break;
        case '[': token.type = TOK_LBRACKET; strcpy(token.lexeme, "[");  break;
        case ']': token.type = TOK_RBRACKET; strcpy(token.lexeme, "]");  break;
        case '{': token.type = TOK_SECURE_START; strcpy(token.lexeme, "{"); break;
        case '}': token.type = TOK_SECURE_END;   strcpy(token.lexeme, "}"); break;

        case '=':
            c = fgetc(sourceFile);
            if (c == '=') { token.type = TOK_EQ;  strcpy(token.lexeme, "=="); }
            else { ungetc(c, sourceFile); token.type = TOK_UNKNOWN; strcpy(token.lexeme, "="); }
            break;

        case '!':
            c = fgetc(sourceFile);
            if (c == '=') { token.type = TOK_NEQ; strcpy(token.lexeme, "!="); }
            else { ungetc(c, sourceFile); token.type = TOK_UNKNOWN; strcpy(token.lexeme, "!"); }
            break;

        case '>':
            c = fgetc(sourceFile);
            if (c == '=') { token.type = TOK_GE; strcpy(token.lexeme, ">="); }
            else { ungetc(c, sourceFile); token.type = TOK_GT; strcpy(token.lexeme, ">"); }
            break;

        case '<':
            c = fgetc(sourceFile);
            if (c == '=') { token.type = TOK_LE; strcpy(token.lexeme, "<="); }
            else { ungetc(c, sourceFile); token.type = TOK_LT; strcpy(token.lexeme, "<"); }
            break;

        default:
            token.type      = TOK_UNKNOWN;
            token.lexeme[0] = (char)c;
            token.lexeme[1] = '\0';
            break;
    }

    return token;
}

/* ===============================================
   TOKEN TYPE NAME
   =============================================== */
const char* getTokenTypeName(TokenType type) {
    switch (type) {
        case TOK_FUNC: case TOK_VAR:  case TOK_SET:
        case TOK_IF:   case TOK_ELSE: case TOK_WHILE:
        case TOK_FOR:  case TOK_FROM: case TOK_TO:
        case TOK_STEP: case TOK_RET:  case TOK_CALL:
        case TOK_PRINT: case TOK_READ:
        case TOK_BREAK: case TOK_CONTINUE:
        case TOK_INT:  case TOK_BOOL: case TOK_CHAR:
        case TOK_TRUE: case TOK_FALSE:
        case TOK_END:
            return "KEYWORD";
        case TOK_IDENTIFIER:   return "IDENTIFIER";
        case TOK_INT_LITERAL:  return "NUMBER";
        case TOK_CHAR_LITERAL: return "CHAR";
        case TOK_PLUS: case TOK_MINUS:
        case TOK_MUL:  case TOK_DIV:  case TOK_MOD:
            return "ARITH_OP";
        case TOK_EQ:  case TOK_NEQ:
        case TOK_GT:  case TOK_LT:
        case TOK_GE:  case TOK_LE:
            return "REL_OP";
        case TOK_AND:
        case TOK_OR:
        case TOK_NOT:
            return "LOGICAL_OP";
        case TOK_LPAREN: case TOK_RPAREN:
        case TOK_COMMA:  case TOK_COLON:
        case TOK_LBRACKET: case TOK_RBRACKET:
            return "SYMBOL";
        case TOK_SECURE_START: return "SECURE_START";
        case TOK_SECURE_END:   return "SECURE_END";
        case TOK_EOF:          return "EOF";
        default:               return "UNKNOWN";
    }
}

/* ===============================================
   PRINT ALL TOKENS - PHASE 1 OUTPUT
   returns 1 = ok, 0 = blocked
   =============================================== */
int printAllTokens(FILE *fp) {

    Token tok;
    int   blocked = 0;

    memset(&gLockFile, 0, sizeof(LockFile));

    if (!readLockFile(gFilename, &gLockFile)) {
        gIsFirstTime = 1;
        printf("\n");
        printf("   [SECURITY] No lock file found.\n");
        printf("   [SECURITY] First compilation.\n\n");
    } else {
        gIsFirstTime = 0;
        printf("\n");
        printf("   [SECURITY] Lock file loaded.\n");
        printf("   [SECURITY] Expected blocks : %d\n\n",
               gLockFile.totalBlocks);
    }

    printf("\n==========================================\n");
    printf("    PHASE 1 : LEXICAL ANALYSIS\n");
    printf("==========================================\n\n");
    printf("%-6s %-14s %s\n", "LINE", "TOKEN TYPE", "LEXEME");
    printf("------------------------------------------\n");

    do {
        tok = getNextToken();

        if (tok.type == TOK_UNKNOWN &&
            strcmp(tok.lexeme, "BLOCKED") == 0) {
            blocked = 1;
            break;
        }

        printf("%-6d %-14s %s\n",
               tok.line,
               getTokenTypeName(tok.type),
               tok.lexeme);

    } while (tok.type != TOK_EOF);

    if (blocked) {
        printf("\n==========================================\n");
        printf("  COMPILATION BLOCKED BY SECURITY\n");
        printf("  Phase 2, 3, 4, 5 -> CANCELLED\n");
        printf("==========================================\n\n");
        return 0;
    }

    if (!gIsFirstTime &&
        blockCount != gLockFile.totalBlocks) {
        printf("\n==========================================\n");
        printf("  SECURITY VIOLATION!\n");
        printf("  Expected %d secure blocks, found %d!\n",
               gLockFile.totalBlocks, blockCount);
        printf("  Secure blocks removed or added!\n");
        printf("  COMPILATION BLOCKED!\n");
        printf("==========================================\n\n");
        return 0;
    }

    gLockFile.totalBlocks = blockCount;
    writeLockFile(gFilename, &gLockFile);

    printf("\n==========================================\n");
    printf("  PHASE 1 COMPLETE\n");
    printf("  Total secure blocks found : %d\n", blockCount);
    printf("==========================================\n\n");

    return 1;
}