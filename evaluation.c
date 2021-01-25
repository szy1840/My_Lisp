#include "mpc.h"

#ifdef _WIN32

static char buffer[2048];
/* fake readline function */
char *readline(char *prompt)
{
    fputs(prompt, stdout);
    fgets(buffer, 2048, stdin);
    char *cpy = malloc(strlen(buffer) + 1);
    strcpy(cpy, buffer);
    cpy[strlen(cpy) - 1] = '\0';
    return cpy;
}
/* fake add_history function */
void add_history(char *unused) {}
#else
#include <editline/readline.h>
#include <editline/history.h>
#endif

typedef struct lval{
    int type;
    long num;
    char* err;
    char* sym;
    int count;
    struct lval** cell;
}lval;
enum {LVAL_ERR,LVAL_NUM,LVAL_SYM,LVAL_SEXPR};

/* prototypes */
void lval_print(lval* v);

/* construct a pointer to a new number type lval */
lval* lval_num(long x){
    lval* v=malloc(sizeof(lval));
    v->type=LVAL_NUM;
    v->num=x;
    return v;
}
/* construct a pointer to a new error type lval */
lval* lval_err(char* m){
    lval* v=malloc(sizeof(lval));
    v->type=LVAL_ERR;
    v->err=malloc(strlen(m)+1);
    strcpy(v->err,m);
    return v;
}
/* construct a pointer to a new symbol type lval */
lval* lval_sym(char* s){
    lval* v=malloc(sizeof(lval));
    v->type=LVAL_SYM;
    v->sym=malloc(strlen(s)+1);
    strcpy(v->sym,s);
    return v;
}
/* construct a pointer to a new empty sexpr lval */
lval* lval_sexpr(){
    lval* v=malloc(sizeof(lval));
    v->type=LVAL_SEXPR;
    v->count=0;
    v->cell=NULL;
    return v;
}
/* destruct lval */
void lval_del(lval* v){
    switch(v->type){
        case LVAL_NUM: break;
        case LVAL_ERR: 
            free(v->err);
            break;
        case LVAL_SYM:
            free(v->sym);
            break;
        case LVAL_SEXPR:
            for (int i = 0; i < v->count; i++){
                lval_del(v->cell[i]);
            }
            free(v->cell);
            break;   
    }

    free(v);
}

/* read */
lval* lval_read_num(mpc_ast_t* t){
    errno=0;
    long x=strtol(t->contents,NULL,10);
    return errno!=ERANGE ? lval_num(x) 
    : lval_err("invalid number");
}

lval* lval_add(lval* v,lval* x){
    v->count++;
    v->cell=realloc(v->cell,sizeof(lval*) * v->count);
    v->cell[v->count-1]=x;
    return v;
}

lval* lval_read(mpc_ast_t* t){
    if(strstr(t->tag,"number")){
        /* numbers need extra check(though seems it's better to
         only give the number to the func to check) */
        return lval_read_num(t);
    }
    if(strstr(t->tag,"symbol")){
        return lval_sym(t->contents);
    }

    /* if root(>) or sexpr then create empty list */
    lval* x=NULL;
    if(strcmp(t->tag,">")==0){
        x=lval_sexpr();
    }
    if(strstr(t->tag,"sexpr")){
        x=lval_sexpr();
    }
    /* fill the list with any valid expression contained within */
    for (int i = 0; i < t->children_num; i++)
    {
        if(strcmp(t->children[i]->contents,"(")==0) continue;
        if(strcmp(t->children[i]->contents,")")==0) continue;
        if(strcmp(t->children[i]->tag,"regex")==0) continue;
        
        x=lval_add(x,lval_read(t->children[i]));
    }
    
    return x;
}
/* print */
void lval_expr_print(lval* v,char open, char close){
    putchar(open);
    for (int i = 0; i < v->count; i++)
    {
        lval_print(v->cell[i]);
        /* avoid print trailing space if it's the last one */
        if(i!=v->count-1) putchar(' ');
    }
    
    putchar(close);
}
void lval_print(lval* v){
    switch(v->type){
        case LVAL_NUM: 
            printf("%li",v->num);
            break;
        case LVAL_ERR: 
            printf("Error:%s",v->err);
            break;
        case LVAL_SYM:
            printf("%s",v->sym);
            break;
        case LVAL_SEXPR:
            lval_expr_print(v,'(',')');
            break;
    }
}

void lval_println(lval* v){
    lval_print(v);
    putchar('\n');
}

int main(int argc, char **argv)
{
    /* create some parsers */
    mpc_parser_t *Number = mpc_new("number");
    mpc_parser_t *Symbol = mpc_new("symbol");
    mpc_parser_t *Sexpr = mpc_new("sexpr");
    mpc_parser_t *Expr = mpc_new("expr");
    mpc_parser_t *Lispy = mpc_new("lispy");

    /* define them with the following language */
    mpca_lang(MPCA_LANG_DEFAULT,
    "                                                       \
        number:     /-?[0-9]+/ ;                            \
        symbol:     '+' | '-' | '*' | '/' ;                 \
        sexpr:      '(' <expr>* ')' ;                       \
        expr:       <number> | <symbol> | <sexpr>;          \
        lispy:      /^/ <operator> <expr>* /$/ ;            \
    ",
    Number, Symbol, Sexpr, Expr, Lispy);
    puts("Lispy Version 0.0.1");
    puts("Press Ctrl+c to Exit\n");

    while(1){
        char *input = readline("lispy> ");
        add_history(input);

        /* attempt to parse the user input */
        mpc_result_t r;
        if(mpc_parse("<stdin>",input,Lispy,&r)){
            lval* x=lval_read(r.output);
            lval_println(x);
            lval_del(x);
        }else{
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        }
        
        free(input);
    }

    /* undefine and delete our parsers */
    mpc_cleanup(5,Number,Symbol,Sexpr,Expr,Lispy);

    return 0;
}