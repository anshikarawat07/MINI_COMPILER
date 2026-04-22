#include <stdio.h>
#include "../include/lexer.h"
#include "../include/parser.h"

int main() {

    const char *filename = "tests/test.mll";

    /* PHASE 1 - LEXER + SECURITY runs here */
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        printf("Error: cannot open %s\n", filename);
        return 1;
    }
    printAllTokens(fp);
    fclose(fp);

    /* PHASE 2 - PARSER */
    /* security already passed in phase 1 */
    /* open file again but skip security blocks */
    fp = fopen(filename, "r");
    if (!fp) {
        printf("Error: cannot open %s\n", filename);
        return 1;
    }
    runParser(fp);
    fclose(fp);

    return 0;
}