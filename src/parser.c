#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lexer.h"
#include "parser.h"


static Parser      parser;
static ParserStats stats;

/* ===============================================
   CREATE NEW NODE
   =============================================== */
static ASTNode *newNode(NodeType type,const char *name,int line) {

    ASTNode *node = (ASTNode *)malloc(sizeof(ASTNode));
    if (!node) {
        printf("   [PARSER] ERROR: Memory failed!\n");
        exit(1);
    }

    node->type       = type;
    node->line       = line;
    node->endLine    = line;
    node->childCount = 0;
    node->isSecure   = parser.inSecure;

    strncpy(node->name,     name ? name : "", MAX_NAME - 1);
    strncpy(node->dataType, "",               MAX_NAME - 1);

    for (int i = 0; i < MAX_CHILDREN; i++)
        node->children[i] = NULL;

    return node;
}

/* ===============================================
   ADD CHILD
   =============================================== */
static void addChild(ASTNode *parent, ASTNode *child) {
    if (!parent || !child) return;
    if (parent->childCount < MAX_CHILDREN)
        parent->children[parent->childCount++] = child;
}

/* ===============================================
   ADVANCE
   =============================================== */
static void advance(void) {
    parser.previous = parser.current;
    parser.current  = getNextToken();
}

/* ===============================================
   CHECK
   =============================================== */
static int check(TokenType type) {
    return parser.current.type == type;
}

/* ===============================================
   MATCH
   =============================================== */
static int match(TokenType type) {
    if (check(type)) { advance(); return 1; }
    return 0;
}

/* ===============================================
   EXPECT
   =============================================== */
static int expect(TokenType type, const char *msg) {
    if (check(type)) { advance(); return 1; }
    printf("   [PARSER] ERROR at line %d: %s\n",
           parser.current.line, msg);
    printf("            Got: %s\n",
           parser.current.lexeme);
    parser.hadError = 1;
    stats.errors++;
    return 0;
}

/* ===============================================
   FORWARD DECLARATIONS
   =============================================== */
static ASTNode *parseStatement(void);
static ASTNode *parseExpression(void);

/* ===============================================
   PARSE EXPRESSION
   =============================================== */
static ASTNode *parseExpression(void) {

    ASTNode *left = NULL;

    if (check(TOK_INT_LITERAL)) {
        left = newNode(NODE_INT_LITERAL,
                       parser.current.lexeme,
                       parser.current.line);
        advance();
    }
    else if (check(TOK_CHAR_LITERAL)) {
        left = newNode(NODE_CHAR_LITERAL,
                       parser.current.lexeme,
                       parser.current.line);
        advance();
    }
    else if (check(TOK_TRUE)) {
        left = newNode(NODE_BOOL_LITERAL, "true",
                       parser.current.line);
        advance();
    }
    else if (check(TOK_FALSE)) {
        left = newNode(NODE_BOOL_LITERAL, "false",
                       parser.current.line);
        advance();
    }
    else if (check(TOK_IDENTIFIER)) {
        left = newNode(NODE_IDENTIFIER,
                       parser.current.lexeme,
                       parser.current.line);
        advance();
    }
    else if (check(TOK_NOT)) {
        int ln = parser.current.line;
        advance();
        ASTNode *node  = newNode(NODE_UNOP, "NOT", ln);
        ASTNode *right = parseExpression();
        addChild(node, right);
        return node;
    }

    if (!left) return NULL;

    /* binary operator */
    if (check(TOK_PLUS)  || check(TOK_MINUS) ||
        check(TOK_MUL)   || check(TOK_DIV)   ||
        check(TOK_MOD)   || check(TOK_EQ)    ||
        check(TOK_NEQ)   || check(TOK_LT)    ||
        check(TOK_GT)    || check(TOK_LE)    ||
        check(TOK_GE)    || check(TOK_AND)   ||
        check(TOK_OR)) {

        ASTNode *op = newNode(NODE_BINOP,
                              parser.current.lexeme,
                              parser.current.line);
        advance();
        ASTNode *right = parseExpression();
        addChild(op, left);
        addChild(op, right);
        return op;
    }

    return left;
}

/* ===============================================
   PARSE VAR DECLARATION
   VAR name : TYPE
   =============================================== */
static ASTNode *parseVarDecl(void) {

    int ln = parser.current.line;
    advance();

    if (!check(TOK_IDENTIFIER)) {
        printf("   [PARSER] ERROR at line %d:"
               " Expected variable name\n", ln);
        parser.hadError = 1;
        stats.errors++;
        return NULL;
    }

    ASTNode *node = newNode(NODE_VAR_DECL,
                            parser.current.lexeme, ln);
    advance();

    if (match(TOK_COLON)) {
        if (check(TOK_INT)  ||
            check(TOK_BOOL) ||
            check(TOK_CHAR)) {
            strncpy(node->dataType,
                    parser.current.lexeme,
                    MAX_NAME - 1);
            advance();
        }
    }

    return node;
}

/* ===============================================
   PARSE SET
   SET name value
   =============================================== */
static ASTNode *parseSet(void) {

    int ln = parser.current.line;
    advance();

    if (!check(TOK_IDENTIFIER)) {
        printf("   [PARSER] ERROR at line %d:"
               " Expected variable name after SET\n", ln);
        parser.hadError = 1;
        stats.errors++;
        return NULL;
    }

    ASTNode *node = newNode(NODE_SET,
                            parser.current.lexeme, ln);
    advance();

    ASTNode *val = parseExpression();
    if (val) addChild(node, val);

    return node;
}

/* ===============================================
   PARSE PRINT
   =============================================== */
static ASTNode *parsePrint(void) {

    int ln = parser.current.line;
    advance();

    ASTNode *node = newNode(NODE_PRINT, "PRINT", ln);
    ASTNode *val  = parseExpression();
    if (val) addChild(node, val);

    return node;
}

/* ===============================================
   PARSE READ
   =============================================== */
static ASTNode *parseRead(void) {

    int ln = parser.current.line;
    advance();

    ASTNode *node = newNode(NODE_READ, "READ", ln);

    if (check(TOK_IDENTIFIER)) {
        ASTNode *var = newNode(NODE_IDENTIFIER,
                               parser.current.lexeme, ln);
        advance();
        addChild(node, var);
    }

    return node;
}

/* ===============================================
   PARSE RET
   =============================================== */
static ASTNode *parseRet(void) {

    int ln = parser.current.line;
    advance();

    ASTNode *node = newNode(NODE_RET, "RET", ln);
    ASTNode *val  = parseExpression();
    if (val) addChild(node, val);

    return node;
}

/* ===============================================
   PARSE IF
   =============================================== */
static ASTNode *parseIf(void) {

    int ln = parser.current.line;
    advance();

    ASTNode *node = newNode(NODE_IF, "IF", ln);

    ASTNode *cond = parseExpression();
    if (cond) addChild(node, cond);

    expect(TOK_COLON, "Expected : after IF condition");

    while (!check(TOK_ELSE) &&
           !check(TOK_END)  &&
           !check(TOK_EOF)) {
        ASTNode *stmt = parseStatement();
        if (stmt) addChild(node, stmt);
    }

    if (match(TOK_ELSE)) {
        ASTNode *elseNode = newNode(NODE_ELSE, "ELSE", ln);
        expect(TOK_COLON, "Expected : after ELSE");
        while (!check(TOK_END) && !check(TOK_EOF)) {
            ASTNode *stmt = parseStatement();
            if (stmt) addChild(elseNode, stmt);
        }
        addChild(node, elseNode);
    }

    node->endLine = parser.current.line;
    expect(TOK_END, "Expected END after IF block");
    return node;
}

/* ===============================================
   PARSE WHILE
   =============================================== */
static ASTNode *parseWhile(void) {

    int ln = parser.current.line;
    advance();

    ASTNode *node = newNode(NODE_WHILE, "WHILE", ln);

    ASTNode *cond = parseExpression();
    if (cond) addChild(node, cond);

    expect(TOK_COLON, "Expected : after WHILE condition");

    while (!check(TOK_END) && !check(TOK_EOF)) {
        ASTNode *stmt = parseStatement();
        if (stmt) addChild(node, stmt);
    }

    node->endLine = parser.current.line;
    expect(TOK_END, "Expected END after WHILE block");
    return node;
}

/* ===============================================
   PARSE FOR
   =============================================== */
static ASTNode *parseFor(void) {

    int ln = parser.current.line;
    advance();

    ASTNode *node = newNode(NODE_FOR, "FOR", ln);

    if (check(TOK_IDENTIFIER)) {
        ASTNode *var = newNode(NODE_IDENTIFIER,
                               parser.current.lexeme, ln);
        addChild(node, var);
        advance();
    }

    expect(TOK_FROM, "Expected FROM in FOR");
    ASTNode *from = parseExpression();
    if (from) addChild(node, from);

    expect(TOK_TO, "Expected TO in FOR");
    ASTNode *to = parseExpression();
    if (to) addChild(node, to);

    if (match(TOK_STEP)) {
        ASTNode *step = parseExpression();
        if (step) addChild(node, step);
    }

    expect(TOK_COLON, "Expected : after FOR header");

    while (!check(TOK_END) && !check(TOK_EOF)) {
        ASTNode *stmt = parseStatement();
        if (stmt) addChild(node, stmt);
    }

    node->endLine = parser.current.line;
    expect(TOK_END, "Expected END after FOR block");
    return node;
}

/* ===============================================
   PARSE CALL
   =============================================== */
static ASTNode *parseCall(void) {

    int ln = parser.current.line;
    advance();

    ASTNode *node = newNode(NODE_CALL, "", ln);

    if (check(TOK_IDENTIFIER)) {
        strncpy(node->name,
                parser.current.lexeme,
                MAX_NAME - 1);
        advance();
    }

    if (match(TOK_LPAREN)) {
        expect(TOK_RPAREN, "Expected ) after (");
    }

    return node;
}

/* ===============================================
   PARSE FUNC
   =============================================== */
static ASTNode *parseFunc(void) {

    int ln = parser.current.line;
    advance();

    ASTNode *node = newNode(NODE_FUNC, "", ln);

    if (check(TOK_IDENTIFIER)) {
        strncpy(node->name,
                parser.current.lexeme,
                MAX_NAME - 1);
        advance();
    }

    expect(TOK_LPAREN, "Expected ( after function name");
    expect(TOK_RPAREN, "Expected )");

    if (match(TOK_COLON)) {
        if (check(TOK_INT)  ||
            check(TOK_BOOL) ||
            check(TOK_CHAR)) {
            strncpy(node->dataType,
                    parser.current.lexeme,
                    MAX_NAME - 1);
            advance();
        }
    }

    while (!check(TOK_END) && !check(TOK_EOF)) {
        ASTNode *stmt = parseStatement();
        if (stmt) addChild(node, stmt);
    }

    node->endLine = parser.current.line;
    expect(TOK_END, "Expected END after FUNC body");
    return node;
}

/* ===============================================
   PARSE SECURE BLOCK
   security already done in phase 1
   lexer already skipped block content
   just return node as marker
   =============================================== */
static ASTNode *parseSecureBlock(void) {

    int ln = parser.current.line;
    advance(); // consume #secure_start

    ASTNode *node = newNode(NODE_SECURE_BLOCK, "SECURE_BLOCK", ln);

    /* ⭐ PARSE INSIDE BLOCK */
    while (!check(TOK_SECURE_END) && !check(TOK_EOF)) {
        ASTNode *stmt = parseStatement();
        if (stmt) addChild(node, stmt);
    }

    match(TOK_SECURE_END); // consume #secure_end

    return node;
}

/* ===============================================
   PARSE STATEMENT
   =============================================== */
static ASTNode *parseStatement(void) {

    if (check(TOK_EOF)) return NULL;

    switch (parser.current.type) {

        case TOK_FUNC:         return parseFunc();
        case TOK_VAR:          return parseVarDecl();
        case TOK_SET:          return parseSet();
        case TOK_PRINT:        return parsePrint();
        case TOK_READ:         return parseRead();
        case TOK_RET:          return parseRet();
        case TOK_IF:           return parseIf();
        case TOK_WHILE:        return parseWhile();
        case TOK_FOR:          return parseFor();
        case TOK_CALL:         return parseCall();
        case TOK_SECURE_START: return parseSecureBlock();

        case TOK_BREAK: {
            ASTNode *n = newNode(NODE_BREAK, "BREAK",
                                 parser.current.line);
            advance();
            return n;
        }

        case TOK_CONTINUE: {
            ASTNode *n = newNode(NODE_CONTINUE, "CONTINUE",
                                 parser.current.line);
            advance();
            return n;
        }

        case TOK_END:
        case TOK_ELSE:
        case TOK_SECURE_END:
            return NULL;

        default:
            printf("   [PARSER] WARNING: Unexpected"
                   " token '%s' at line %d\n",
                   parser.current.lexeme,
                   parser.current.line);
            advance();
            return NULL;
    }
}

/* ===============================================
   PARSE PROGRAM
   =============================================== */
ASTNode *parseProgram(void) {

    ASTNode *root = newNode(NODE_PROGRAM, "PROGRAM", 0);
    advance();

    while (!check(TOK_EOF)) {
        ASTNode *stmt = parseStatement();
        if (stmt) addChild(root, stmt);
    }

    return root;
}

/* ===============================================
   PRINT AST AS TABLE
   =============================================== */
void printAST(ASTNode *node, int depth) {

    if (!node) return;

    switch (node->type) {

        case NODE_PROGRAM:
            for (int i = 0; i < node->childCount; i++)
                printAST(node->children[i], depth);
            break;

        case NODE_FUNC:
            printf("%-6d %-14s %-10s %-10s\n",
                   node->line, "FUNC",
                   node->name,
                   node->dataType[0] ?
                   node->dataType : "-");
            stats.totalFunctions++;
            stats.totalStatements++;
            for (int i = 0; i < node->childCount; i++)
                printAST(node->children[i], depth + 1);
            break;

        case NODE_VAR_DECL:
            printf("%-6d %-14s %-10s %-10s\n",
                   node->line, "VAR DECL",
                   node->name,
                   node->dataType[0] ?
                   node->dataType : "-");
            stats.totalVariables++;
            stats.totalStatements++;
            break;

        case NODE_SET:
            printf("%-6d %-14s %-10s %-10s\n",
                   node->line, "ASSIGNMENT",
                   node->name,
                   node->childCount > 0 ?
                   node->children[0]->name : "-");
            stats.totalAssignments++;
            stats.totalStatements++;
            break;

        case NODE_PRINT:
            printf("%-6d %-14s %-10s %-10s\n",
                   node->line, "PRINT",
                   node->childCount > 0 ?
                   node->children[0]->name : "-",
                   "-");
            stats.totalPrints++;
            stats.totalStatements++;
            break;

        case NODE_READ:
            printf("%-6d %-14s %-10s %-10s\n",
                   node->line, "READ",
                   node->childCount > 0 ?
                   node->children[0]->name : "-",
                   "-");
            stats.totalStatements++;
            break;

        case NODE_RET:
            printf("%-6d %-14s %-10s %-10s\n",
                   node->line, "RETURN",
                   node->childCount > 0 ?
                   node->children[0]->name : "-",
                   "-");
            stats.totalReturns++;
            stats.totalStatements++;
            break;

        case NODE_IF:
            printf("%-6d %-14s %-10s %-10s\n",
                   node->line, "IF",
                   "-", "-");
            stats.totalIf++;
            stats.totalStatements++;
            for (int i = 0; i < node->childCount; i++)
                printAST(node->children[i], depth + 1);
            break;

        case NODE_ELSE:
            printf("%-6d %-14s %-10s %-10s\n",
                   node->line, "ELSE",
                   "-", "-");
            stats.totalStatements++;
            for (int i = 0; i < node->childCount; i++)
                printAST(node->children[i], depth + 1);
            break;

        case NODE_WHILE:
            printf("%-6d %-14s %-10s %-10s\n",
                   node->line, "WHILE",
                   "-", "-");
            stats.totalWhile++;
            stats.totalStatements++;
            for (int i = 0; i < node->childCount; i++)
                printAST(node->children[i], depth + 1);
            break;

        case NODE_FOR:
            printf("%-6d %-14s %-10s %-10s\n",
                   node->line, "FOR",
                   "-", "-");
            stats.totalFor++;
            stats.totalStatements++;
            for (int i = 0; i < node->childCount; i++)
                printAST(node->children[i], depth + 1);
            break;

        case NODE_CALL:
            printf("%-6d %-14s %-10s %-10s\n",
                   node->line, "CALL",
                   node->name, "-");
            stats.totalStatements++;
            break;

        case NODE_SECURE_BLOCK:printf("%-6d SECURE BLOCK   -          -\n", node->line);
            for (int i = 0; i < node->childCount; i++) {
                ASTNode *child = node->children[i];

                if (child->type == NODE_SET)
                    printf("       ASSIGNMENT     %s\n", child->name);

                else if (child->type == NODE_PRINT)
                    printf("       PRINT          %s\n", child->name);
            }
            stats.totalSecureBlocks++;
            stats.totalStatements++;
            break;

        case NODE_BREAK:
            printf("%-6d %-14s %-10s %-10s\n",
                   node->line, "BREAK", "-", "-");
            stats.totalStatements++;
            break;

        case NODE_CONTINUE:
            printf("%-6d %-14s %-10s %-10s\n",
                   node->line, "CONTINUE", "-", "-");
            stats.totalStatements++;
            break;

        default:
            break;
    }
}

/* ===============================================
   FREE AST
   =============================================== */
void freeAST(ASTNode *node) {
    if (!node) return;
    for (int i = 0; i < node->childCount; i++)
        freeAST(node->children[i]);
    free(node);
}

/* ===============================================
   INIT PARSER
   =============================================== */
void initParser(FILE *fp) {
    initLexer(fp);
    parser.hadError = 0;
    parser.inSecure = 0;
}
