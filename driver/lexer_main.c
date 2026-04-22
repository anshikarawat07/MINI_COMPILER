#include <stdio.h>
#include <string.h>
#include "../include/lexer.h"

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
        printf("  Example: ./mylang test.mll\n");
        return 1;
    }

    /* open file */
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        printf("  Error: cannot open: %s\n", filename);
        return 1;
    }

    /* header */
    printf("\n");
    printf("==========================================\n");
    printf("  MyLang Compiler v1.0\n");
    printf("  Compiling: %s\n", filename);
    printf("==========================================\n");

    /* PHASE 1 - LEXER + SECURITY */
    initLexerWithFile(fp, filename);
    printAllTokens(fp);
    fclose(fp);

    return 0;
}