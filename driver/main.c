#include <stdio.h>
#include <string.h>
#include "lexer.h"
#include "parser.h"
#include "semantic.h"
#include "interpreter.h"   // ✅ ADDED

int main(int argc, char *argv[]) {

    if (argc < 2) {
        printf("\n");
        printf("  MyLang Compiler v1.0\n");
        printf("  Usage: ./mylang <file.mll>\n");
        printf("\n");
        return 1;
    }

    const char *filename = argv[1];

    /* check .mll extension */
    const char *ext = strrchr(filename, '.');
    if (!ext || strcmp(ext, ".mll") != 0) {
        printf("  Error: file must have .mll extension!\n");
        return 1;
    }

    printf("\n");
    printf("==========================================\n");
    printf("  MyLang Compiler v1.0\n");
    printf("  Compiling: %s\n", filename);
    printf("==========================================\n");
    fflush(stdout);

    /* ==========================================
       PHASE 1 : LEXER + SECURITY
       ========================================== */
    securityDone = 0;

    FILE *fp = fopen(filename, "r");
    if (!fp) {
        printf("  Error: cannot open: %s\n", filename);
        return 1;
    }

    initLexerWithFile(fp, filename);
    int ok = printAllTokens(fp);
    fclose(fp);
    fflush(stdout);

    if (!ok) {
        printf("  Compilation stopped.\n");
        return 1;
    }

    /* ==========================================
       PHASE 2 : PARSER
       ========================================== */
    securityDone = 1;

    fp = fopen(filename, "r");
    if (!fp) {
        printf("  Error: cannot open: %s\n", filename);
        return 1;
    }

    initLexerWithFile(fp, filename);
    initParser(fp);

    ASTNode *tree = parseProgram();

    fclose(fp);

    printf("\n==========================================\n");
    printf("    PHASE 2 : PARSER\n");
    printf("==========================================\n\n");
    printf("  Parsing statements...\n\n");
    printf("%-6s %-14s %-10s %-10s\n",
           "LINE", "STATEMENT", "NAME", "TYPE");
    printf("------------------------------------------\n");

    printAST(tree, 0);

    printf("\n------------------------------------------\n");
    printf("  PHASE 2 COMPLETE\n");
    printf("==========================================\n\n");
    fflush(stdout);

    // /* ==========================================
    //    PHASE 3 : SEMANTIC ANALYSIS
    //    ========================================== */
    // int semErrors = runSemantic(tree);
    // fflush(stdout);

    // if (semErrors > 0) {
    //     printf("\n  Compilation stopped due to semantic errors.\n");
    //     freeAST(tree);
    //     return 1;
    // }

    // /* ==========================================
    //    PHASE 4 : INTERPRETER   ✅ ADDED
    //    ========================================== */
    // printf("\n==========================================\n");
    // printf("    PHASE 4 : INTERPRETER\n");
    // printf("==========================================\n\n");

    // interpret(tree);

    // printf("\n------------------------------------------\n");
    // printf("  EXECUTION COMPLETE\n");
    // printf("==========================================\n\n");
    // fflush(stdout);

    /* Free AST once, after all phases are done */
    freeAST(tree);
    tree = NULL;

    return 0;
}