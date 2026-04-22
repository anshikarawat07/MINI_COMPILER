#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "interpreter.h"

/* ===============================================
   VARIABLE TABLE
   =============================================== */
typedef struct {
    char name[50];
    int  value;
} Var;

static Var vars[100];
static int varCount = 0;

/* ===============================================
   VARIABLE HELPERS
   =============================================== */
int getVarIndex(char *name) {
    for (int i = 0; i < varCount; i++) {
        if (strcmp(vars[i].name, name) == 0)
            return i;
    }
    return -1;
}

void setVar(char *name, int val) {
    int idx = getVarIndex(name);

    if (idx == -1) {
        strcpy(vars[varCount].name, name);
        vars[varCount].value = val;
        varCount++;
    } else {
        vars[idx].value = val;
    }
}

int getVar(char *name) {
    int idx = getVarIndex(name);
    if (idx != -1) return vars[idx].value;
    return 0;
}

/* ===============================================
   EXPRESSION EVALUATION
   =============================================== */
int evalExpr(ASTNode *node) {

    if (!node) return 0;

    /* number */
    if (node->type == NODE_INT_LITERAL)
        return atoi(node->name);

    /* variable */
    if (node->type == NODE_IDENTIFIER)
        return getVar(node->name);

    /* binary operation */
    if (node->type == NODE_BINOP) {

        int l = evalExpr(node->children[0]);
        int r = evalExpr(node->children[1]);

        if (strcmp(node->name, "+") == 0) return l + r;
        if (strcmp(node->name, "-") == 0) return l - r;
        if (strcmp(node->name, "*") == 0) return l * r;
        if (strcmp(node->name, "/") == 0) return r != 0 ? l / r : 0;
        if (strcmp(node->name, "%") == 0) return r != 0 ? l % r : 0;

        /* relational */
        if (strcmp(node->name, "<")  == 0) return l < r;
        if (strcmp(node->name, ">")  == 0) return l > r;
        if (strcmp(node->name, "<=") == 0) return l <= r;
        if (strcmp(node->name, ">=") == 0) return l >= r;
        if (strcmp(node->name, "==") == 0) return l == r;
        if (strcmp(node->name, "!=") == 0) return l != r;
    }

    return 0;
}

/* ===============================================
   INTERPRETER CORE
   =============================================== */
void interpret(ASTNode *node) {

    if (!node) return;

    switch (node->type) {

        case NODE_PROGRAM:
            for (int i = 0; i < node->childCount; i++)
                interpret(node->children[i]);
            break;

        case NODE_FUNC:
            for (int i = 0; i < node->childCount; i++)
                interpret(node->children[i]);
            break;

        case NODE_VAR_DECL:
            setVar(node->name, 0);
            break;

        case NODE_SET:
            if (node->childCount > 0) {
                int val = evalExpr(node->children[0]);
                setVar(node->name, val);
            }
            break;

        case NODE_PRINT:
            if (node->childCount > 0) {
                int val = evalExpr(node->children[0]);
                printf("%d\n", val);
            }
            break;

        case NODE_SECURE_BLOCK:
            /* execute secure block normally */
            for (int i = 0; i < node->childCount; i++)
                interpret(node->children[i]);
            break;

        /* OPTIONAL: WHILE LOOP */
        case NODE_WHILE:
            while (evalExpr(node->children[0])) {
                for (int i = 1; i < node->childCount; i++)
                    interpret(node->children[i]);
            }
            break;

        /* OPTIONAL: IF */
        case NODE_IF:
            if (evalExpr(node->children[0])) {
                for (int i = 1; i < node->childCount; i++)
                    interpret(node->children[i]);
            }
            break;

        default:
            break;
    }
}