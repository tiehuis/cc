/**
 * parser.h
 */

#ifndef PARSER_H
#define PARSER_H

#include <setjmp.h>
#include "compiler.h"
#include "lexer.h"
#include "vec.h"

enum {
    AST_NULLARY,
    AST_UNARY,
    AST_BINARY
};

/**
 * Stores information about a ast node value.
 */
typedef struct node_s {
    int arity;
    int id;
    union {
        /* char, int, long */
        long value;

        /* Binary operator */
        struct {
            struct node_s *left;
            struct node_s *right;
        };

        /* Unary operator */
        struct {
            struct node_s *operand;
        };
    };
} node_t;

/* Stores the context of this recursive descent parser, in this case a stream
 * of tokens. */
typedef struct {
    jmp_buf env;
    token_t **tokens;
    size_t position;
    size_t length;
} rdp_t;

rdp_t* rdp_init(token_t **tokens, size_t length);

void rdp_free(rdp_t *ctx);

node_t* rdp_generate_ast(rdp_t *ctx);

#endif
