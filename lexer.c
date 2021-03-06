/**
 * lexer.c
 *
 * Lexes an input file and generates a stream of tokens.
 */

#include <ctype.h>
#include <stdio.h>
#include "compiler.h"
#include "token.h"
#include "lexer.h"

/* Initialize vec_token_t */
VEC_DECLARE(token_t*, token);

/* Initialize vec_char_t */
VEC_DECLARE(char, char);

lex_t* lex_init(char *data, int type)
{
    lex_t *ctx = xmalloc(sizeof(lex_t));
    ctx->type = type;

    if (ctx->type == FILE_BACKED) {
        ctx->fd = fopen(data, "rb");
    }
    else {
        ctx->position = 0;
        ctx->string = data;
    }

    return ctx;
}

void lex_free(lex_t *ctx)
{
    if (ctx->type == FILE_BACKED) {
        fclose(ctx->fd);
    }
    free(ctx);
}

static int lex_getc(lex_t *ctx)
{
    if (ctx->type == FILE_BACKED) {
        return fgetc(ctx->fd);
    }
    else {
        /* FIXME: Uninitialized variable warning */
        const char c = ctx->string[ctx->position++];
        return c ? (int) c : EOF;
    }
}

static void lex_ungetc(lex_t *ctx, char c)
{
    if (ctx->type == FILE_BACKED) {
        ungetc(c, ctx->fd);
    }
    else {
        ctx->position--;
    }
}

/**
 * The 'lex_mk*' functions allocate memory or 'make' a particular token.
 */

static token_t* lex_mkspecial(int type)
{
    token_t *token = xmalloc(sizeof(token_t));
    token->type = type;
    token->is_literal = 0;
    return token;
}

static token_t* lex_mkkw(int type)
{
    token_t *token = xmalloc(sizeof(token_t));
    token->type = type;
    token->is_literal = 0;
    return token;
}

static token_t* lex_mknumber(int type, char *number)
{
    token_t *token = xmalloc(sizeof(token_t));
    token->type = type;
    token->is_literal = 1;
    token->literal = number;
    return token;
}

static token_t* lex_mkident(int type, char *identifier)
{
    token_t *token = xmalloc(sizeof(token_t));
    token->type = type;
    token->is_literal = 1;
    token->literal = identifier;
    return token;
}

/**
 * The 'lex_ch*' functions 'check' the incoming tokens for expected results
 * and call 'lex_mk' functions from them.
 */

static token_t* lex_chnumber(lex_t *ctx, char value)
{
    vec_char_t *number = vec_char_init();
    vec_char_push(number, value);

    int c;
    while ((c = lex_getc(ctx)) != EOF) {
        if (!isdigit(c)) {
            lex_ungetc(ctx, c);
            break;
        }
        vec_char_push(number, c);
    }

    /* FIXME: Memory Leak */
    vec_char_push(number, '\0');
    return lex_mknumber(TOK_NUMBER, number->data);
}

static token_t* lex_chident(lex_t *ctx, char value)
{
    vec_char_t *identifier = vec_char_init();
    vec_char_push(identifier, value);

    int c;
    while ((c = lex_getc(ctx)) != EOF) {
        if (!isalpha(c) || c == '_') {
            lex_ungetc(ctx, c);
            break;
        }
        vec_char_push(identifier, c);
    }

    /* FIXME: Memory Leak */
    vec_char_push(identifier, '\0');
    return lex_mkident(TOK_IDENT, identifier->data);
}

static token_t* lex_chnot(lex_t *ctx)
{
    int c;

    switch ((c = lex_getc(ctx))) {
        case '=':   /* != */
            return lex_mkkw(TOK_NEQUALITY);
        default:
            return NULL;
    }
}

static token_t* lex_cheq(lex_t *ctx)
{
    int c;

    switch ((c = lex_getc(ctx))) {
        case '=':   /* == */
            return lex_mkkw(TOK_EQUALITY);
        default:    /* = */
            lex_ungetc(ctx, c);
            return lex_mkkw(TOK_BWOR);
    }
}

static token_t* lex_chor(lex_t *ctx)
{
    int c;

    switch ((c = lex_getc(ctx))) {
        case '|':   /* || */
            return lex_mkkw(TOK_LOR);
        default:    /* | */
            lex_ungetc(ctx, c);
            return lex_mkkw(TOK_BWOR);
    }
}

static token_t* lex_chand(lex_t *ctx)
{
    int c;

    switch ((c = lex_getc(ctx))) {
        case '&':   /* && */
            return lex_mkkw(TOK_LAND);
        default:    /* & */
            lex_ungetc(ctx, c);
            return lex_mkkw(TOK_BWAND);
    }
}

static token_t* lex_chlbr(lex_t *ctx)
{
    int c;

    switch ((c = lex_getc(ctx))) {
        case '<':   /* << */
            return lex_mkkw(TOK_LSHIFT);
        case '=':   /* <= */
            return lex_mkkw(TOK_LTE);
        default:    /* < */
            lex_ungetc(ctx, c);
            return lex_mkkw(TOK_LT);
    }
}

static token_t* lex_chrbr(lex_t *ctx)
{
    int c;

    switch ((c = lex_getc(ctx))) {
        case '>':   /* >> */
            return lex_mkkw(TOK_RSHIFT);
        case '=':   /* >= */
            return lex_mkkw(TOK_GTE);
        default:    /* > */
            lex_ungetc(ctx, c);
            return lex_mkkw(TOK_GT);
    }
}

static int lex_skip_space(lex_t *ctx)
{
    int c = lex_getc(ctx);

    if (c == EOF)
        return 0;

    if (isspace(c))
        return 1;

    /* Non-whitespace */
    lex_ungetc(ctx, c);
    return 0;
}

/* Return the next found token. This could be implemented as a FSM using switch
 * statements and may at some point, but with a file-backed, char-by-char read
 * approach it makes consuming string-literals and numbers a little less
 * convenient. */
token_t* lex_token(lex_t *ctx)
{
    // Skip all spaces and comments and return space token if successful
    if (lex_skip_space(ctx))
        return lex_mkspecial(TOK_SPACE);

    // Read a character and proceed case by case
    int c = lex_getc(ctx);

    switch (c) {
    case '+':
        return lex_mkkw(TOK_PLUS);
    case '-':
        return lex_mkkw(TOK_MINUS);
    case '/':
        return lex_mkkw(TOK_DIV);
    case '*':
        return lex_mkkw(TOK_MULTIPLY);
    case '%':
        return lex_mkkw(TOK_MOD);
    case '?':
        return lex_mkkw(TOK_QMARK);
    case ':':
        return lex_mkkw(TOK_COLON);
    case '!':
        return lex_chnot(ctx);
    case '=':
        return lex_cheq(ctx);
    case '&':
        return lex_chand(ctx);
    case '|':
        return lex_chor(ctx);
    case '<':
        return lex_chlbr(ctx);
    case '>':
        return lex_chrbr(ctx);
    case '^':
        return lex_mkkw(TOK_BWXOR);
    case '~':
        return lex_mkkw(TOK_BWNEG);
    case '(':
        return lex_mkkw(TOK_LPAREN);
    case ')':
        return lex_mkkw(TOK_RPAREN);
    case ';':
        return lex_mkkw(TOK_SEMICOLON);
    case '0' ... '9':
        return lex_chnumber(ctx, c);
    case 'a' ... 'z':
    case 'A' ... 'Z':
    case '_':
        return lex_chident(ctx, c);
    case EOF:
        return lex_mkspecial(TOK_EOF);
    }

    return lex_mkspecial(TOK_EOF);
}
