#include <stdbool.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "mpc.h"

enum { Val_Num, Val_Err };
enum { Div_Zero, Bad_Op, Bad_Num, Bad_Func};

typedef struct {
  int type;
  long num;
  int err;
} Value;

Value val_num(long x) {
  Value v; 
  v.type = Val_Num;
  v.num = x;
  return v;
}

Value val_err(int x) {
  Value v;
  v.type = Val_Err;
  v.err = x;
  return v;
}

void val_print(Value v) {
  switch (v.type) {
    case Val_Num: printf("%li", v.num); break;
    case Val_Err: 
      switch (v.err) {
        case Div_Zero: printf("Error: Division By Zero"); break;
        case Bad_Op: printf("Error: Invalid Operator"); break;
        case Bad_Num: printf("Error: Invalid Number"); break;
        case Bad_Func: printf("Error: Invalid Function"); break;
      }
    break;
  }
}

void val_println(Value v) {
  val_print(v); putchar('\n');
}

// if it`s windows i just write fake functions because they are no these
// functions in win
#ifdef _WIN32

static char buffer[2048];

// My version of readline function
char *readline(char *prompt) {
  fputs(prompt, stdout);
  fgets(buffer, 2048, stdin);

  char *cpy = malloc(strlen(buffer) + 1);
  strcpy(cpy, buffer);
  cpy[strlen(cpy) - 1] = '\0';

  return cpy;
}

void add_history(char *unused) {}

#else // if its linux just add this lib
#include <editline/readline.h>
#endif

// Get num of childs: 
int num_of_nodes(mpc_ast_t* t) {
  if (t->children_num == 0) { return 1; }
  if (t->children_num >= 1) {
    int total = 1;
    for(int i = 0; i < t->children_num; ++i) {
      total = total + num_of_nodes(t->children[i]);
    }

    return total;
  }

  return 0;
}

Value get_op(Value x, char* operator, Value y) {
  if (x.type == Val_Err) { return x; }
  if (y.type == Val_Err) { return y; }

  if (strcmp(operator, "+") == 0) { return val_num(x.num + y.num); }
  if (strcmp(operator, "-") == 0) { return val_num(x.num - y.num); }
  if (strcmp(operator, "*") == 0) { return val_num(x.num * y.num); }
  if (strcmp(operator, ":") == 0 || strcmp(operator, "/") == 0) {  
    return y.num == 0 ? val_err(Div_Zero) : val_num(x.num / y.num);
  }
  if (strcmp(operator, "%") == 0) { return val_num(x.num % y.num); }
  if (strcmp(operator, "^") == 0 || strcmp(operator, "**")) { return val_num(pow(x.num, y.num)); }
  
  return val_err(Bad_Op);
}

Value get_func(Value x, char* func, Value y) {
  if (strcmp(func, "min") == 0) { return val_num(fmin(x.num, y.num)); }
  if (strcmp(func, "max") == 0) { return val_num(fmax(x.num, y.num)); }

  return val_err(Bad_Func);
}

// evaluate result
Value eval(mpc_ast_t* t) {
  if (strstr(t->tag, "number")) {
    errno = 0;
    long x = strtol(t->contents, NULL, 10);
    return errno != ERANGE ? val_num(x) : val_err(Bad_Num);
  }

  if (strstr(t->children[1]->tag, "operator")) {
    char* operator = t->children[1]->contents;

    Value x = eval(t->children[2]);

    if (t->children[3]->children_num == 0 && strcmp(operator, "-") == 0) {
      return val_num(-x.num);
    }

    int i = 3;
    while (strstr(t->children[i]->tag, "expr")) {
      x = get_op(x, operator, eval(t->children[i]));
      ++i;
    }

    return x;
  } else if (strstr(t->children[1]->tag, "function")) {
    char* function = t->children[1]->contents;

    Value x = eval(t->children[2]);

    int i = 3;
    while (strstr(t->children[i]->tag, "expr")) {
      x = get_func(x, function, eval(t->children[i]));
      ++i;
    }

    return x;
   }

  return val_err(Bad_Num);
}

/* Буффер для пользовательского воода размером 2048 */
int main(int argc, char **argv) {
  mpc_parser_t* Number    = mpc_new("number");
  mpc_parser_t* Operator  = mpc_new("operator");
  mpc_parser_t* Function  = mpc_new("function");
  mpc_parser_t* Expr      = mpc_new("expr");
  mpc_parser_t* Lispy     = mpc_new("lispy");

  mpca_lang(MPCA_LANG_DEFAULT,
      "                                                                                 \
        number    : /-?[0-9]+/ ;                                                        \
        operator  : '+' | '-' | '*' | '/' | '^' | '%' ;                                 \
        function  : \"min\" | \"max\";                                                  \
        expr      : <number> | '(' <operator> <expr>+ ')' | '(' <function> <expr>+ ')'; \
        lispy     : /^/ <operator> <expr>+ /$/ | /^/ <function> <expr>+ /$/;            \
      ", 
      Number, Operator, Function, Expr, Lispy);

  puts("Polish Notation");
  puts("Press Ctrl^c to Exit or type exit\n");

  while (true) {
    // readline removes last symbol from input (\n)
    char *input = readline("PN>>> ");

    // Add something (user`s input in our case) in term history
    add_history(input);

    if (strcmp(input, "exit") == 0) {
      break;
    }

    // Parse the user input 
    mpc_result_t r;
    if (mpc_parse("<stdin>", input, Lispy, &r)) {
      val_println(eval(r.output));
      mpc_ast_delete(r.output);
    } else {
      mpc_err_print(r.error);
      mpc_err_delete(r.error);
    }

    // Because readline`s using dynamic memory alloc
    free(input);
  }

  mpc_cleanup(4, Number, Operator, Expr, Lispy);

  return 0;
}
