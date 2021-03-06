/**
 * interpreter.c
 *
 * A simple line-based c interpreter currently used for testing.
 *
 * There are likely large errors with memory not being freed etc. I'll likely
 * implement some memory pool structure for each phase to simplify this
 * process.
 */

#include <stdio.h>
#include "lexer.h"
#include "token.h"
#include "parser.h"
#include "vec.h"
#include "eval.h"

VEC_DECLARE(token_t*, token);

int main(void)
{
    vec_token_t *tokens = vec_token_init();
    char buffer[512];

    while (1) {
        printf(" > ");
        char *line = fgets(buffer, sizeof(buffer), stdin);

        if (!line)
            break;

        /* Lex line */
        lex_t *lctx = lex_init(line, STRING_BACKED);
        vec_token_clear(tokens);

        token_t *tok;
        do {
            tok = lex_token(lctx);

            /* This is messy, but it doesn't matter */
            if (tok == NULL) {
                lex_free(lctx);
                printf("Invalid Syntax\n");
                goto retry;
            }

            vec_token_push(tokens, tok);
        } while (tok->type != TOK_EOF);

        lex_free(lctx);

        /* Parse line */
        rdp_t *rctx = rdp_init(tokens->data, tokens->len);
        node_t *root = rdp_generate_ast(rctx);
        rdp_free(rctx);

        if (root) {
            /* Evaluate AST and print */
            eval_t *ectx = eval_init(root);
            printf("%ld\n", eval_compute(ectx));
            eval_free(ectx);
            continue;
        }
        else {
            printf("Invalid expression\n");
            goto retry;
        }

retry:;
    }

    return 0;
}
