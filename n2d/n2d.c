/***************************************
  $Header$

  Main program for NFA to DFA table builder program.
  ***************************************/

/* COPYRIGHT */

/*
  Convert a nondeterminstic finite automaton (NFA) into a deterministic finite
  automaton (DFA).

  The NFA is defined in terms of a set of states, with transitions between the
  states.  The transitions may occur on any one of a set of symbols (specified
  with | characters between the options), or may be 'epsilon' transitions, i.e.
  occurring without consumption of any input.  A state may have multiple
  transitions for the same input symbol (hence 'nondeterministic').  The final
  state encountered within the final block defined in the input file is taken
  to be the start state of the whole NFA.  A state may be entered more than
  once in the file; the transitions in the multiple definitions are combined to
  give the complete transition set.  A state may have an exit value assigned
  (with =); this is the return value of the automaton if the end of string is
  encountered when in that state.  (If the resulting DFA can be in multiple
  exiting NFA states when the end of string is reached, the result is all the
  associated NFA exit values or'd together, so it is best to use distinct bits
  for NFA exit values unless it is known that is safe not to in a particular
  case.) The input grammar allows a BLOCK <name> ... ENDBLOCK construction +
  block instantiation.  This allows common parts of the NFA state machine to be
  reused in multiple places as well as aiding structuring and readability.  See
  morf_nfa.in for an example of the input grammar, and morf.c for a
  (non-trivial) example of how to build the automaton around the tables that
  this script generates.
*/

#include <ctype.h>
#include "n2d.h"

static Block **blocks = NULL;
static int nblocks = 0;
static int maxblocks = 0;

static char **toktable=NULL;
static int ntokens = 0;
static int maxtokens = 0;

/* ================================================================= */

static void
grow_tokens(void)
{
  maxtokens += 32;
  toktable = resize_array(char *, toktable, maxtokens);
}

/* ================================================================= */

static int
create_token(char *name)
{
  int result;
  if (ntokens == maxtokens) {
    grow_tokens();
  }
  result = ntokens++;
  toktable[result] = new_string(name);
  return result;
}

/* ================================================================= */

int
lookup_token(char *name, int create)
{
  int found = -1;
  int i;
  for (i=0; i<ntokens; i++) {
    if (!strcmp(toktable[i], name)) {
      found = i;
      break;
    }
  }

  switch (create) {
    case USE_OLD_MUST_EXIST:
      if (found < 0) {
        fprintf(stderr, "Token '%s' was never declared\n", name);
        exit(1);
      }        
      break;
    case CREATE_MUST_NOT_EXIST:
      if (found >= 0) {
        fprintf(stderr, "Token '%s' already declared\n", name);
        exit(1);
      } else {
        found = create_token(name);
      }
      break;
    case CREATE_OR_USE_OLD:
      if (found < 0) {
        found = create_token(name);
      }
      break;
  }
  
  return found;
}

/* ================================================================= */

static void
grow_blocks(void)
{
  maxblocks += 32;
  blocks = resize_array(Block*, blocks, maxblocks);
}

/* ================================================================= */

static Block *
create_block(char *name)
{
  Block *result;
  if (nblocks == maxblocks) {
    grow_blocks();
  }
  result = blocks[nblocks++] = new(Block);
  result->name = new_string(name);
  result->states = NULL;
  result->nstates = 0;
  result->maxstates = 0;
  return result;
}

/* ================================================================= */


Block *
lookup_block(char *name, int create)
{
  Block *found = NULL;
  int i;
  for (i=0; i<nblocks; i++) {
    if (!strcmp(blocks[i]->name, name)) {
      found = blocks[i];
      break;
    }
  }

  switch (create) {
    case USE_OLD_MUST_EXIST:
      if (!found) {
        fprintf(stderr, "Could not find block '%s' to instantiate\n", name);
        exit(1);
      }        
      break;
    case CREATE_MUST_NOT_EXIST:
      if (found) {
        fprintf(stderr, "Already have a block called '%s', cannot redefine\n", name);
        exit(1);
      } else {
        found = create_block(name);
      }
      break;
    case CREATE_OR_USE_OLD:
      if (!found) {
        found = create_block(name);
      }
      break;
  }
  
  return found;
}

/* ================================================================= */
  
static void
grow_states(Block *b)
{
  b->maxstates += 32;
  b->states = resize_array(State*, b->states, b->maxstates);
}

/* ================================================================= */

static State *
create_state(Block *b, char *name)
{
  State *result;
  if (b->nstates == b->maxstates) {
    grow_states(b);
  }
  result = b->states[b->nstates++] = new(State);
  result->name = new_string(name);
  result->parent = b;
  result->index = b->nstates - 1;
  result->transitions = NULL;
  result->exitvals = NULL;
  return result;
}

/* ================================================================= */

State *
lookup_state(Block *b, char *name, int create)
{
  State *found = NULL;
  int i;
  for (i=0; i<b->nstates; i++) {
    if (!strcmp(b->states[i]->name, name)) {
      found = b->states[i];
      break;
    }
  }

  switch (create) {
    case USE_OLD_MUST_EXIST:
      if (!found) {
        fprintf(stderr, "Could not find a state '%s' in block '%s' to transition to\n", name, b->name);
        exit(1);
      }        
      break;
    case CREATE_MUST_NOT_EXIST:
      if (found) {
        fprintf(stderr, "Warning : already have a state '%s' in block '%s'\n", name, b->name);
      } else {
        found = create_state(b, name);
      }
      break;
    case CREATE_OR_USE_OLD:
      if (!found) {
        found = create_state(b, name);
      }
      break;
  }
  
  return found;
}

/* ================================================================= */
  
Stringlist *
add_token(Stringlist *existing, char *token)
{
  Stringlist *result = new(Stringlist);
  result->string = new_string(token);
  result->next = existing;
  return result;
}

/* ================================================================= */

void
add_transitions(State *curstate, Stringlist *tokens, char *destination)
{
  Stringlist *sl;
  if (tokens) {
    for (sl=tokens; sl; sl=sl->next) {
      Translist *tl;
      tl = new(Translist);
      tl->next = curstate->transitions;
      /* No problem with aliasing, these strings are read-only and have
         lifetime = until end of program */
      tl->token = lookup_token(sl->string, USE_OLD_MUST_EXIST);
      tl->ds_name = destination;
      curstate->transitions = tl;
    }
  } else {
    /* Epsilon transition, handled by setting the associated token to NULL */
    Translist *tl;
    tl = new(Translist);
    tl->next = curstate->transitions;
    tl->token = -1;
    tl->ds_name = destination;
    curstate->transitions = tl;
  }
}

/* ================================================================= */

void
add_exit_value(State *curstate, char *value)
{
  Stringlist *sl;
  sl = new(Stringlist);
  sl->string = value;
  sl->next = curstate->exitvals;
  curstate->exitvals = sl;
}

/* ================================================================= */

void
instantiate_block(Block *curblock, char *block_name, char *instance_name)
{
  Block *master = lookup_block(block_name, USE_OLD_MUST_EXIST);
  char namebuf[1024];
  int i;
  for (i=0; i<master->nstates; i++) {
    State *s = master->states[i];
    State *new_state;
    Translist *tl;
    Stringlist *sl, *ex;
    
    strcpy(namebuf, instance_name);
    strcat(namebuf, ".");
    strcat(namebuf, s->name);
    
    /* In perverse circumstances, we might already have a state called this */
    new_state = lookup_state(curblock, namebuf, CREATE_OR_USE_OLD);
    
    for (tl=s->transitions; tl; tl=tl->next) {
      Translist *new_tl = new(Translist);
      new_tl->token = tl->token;
      strcpy(namebuf, instance_name);
      strcat(namebuf, ".");
      strcat(namebuf, tl->ds_name);
      new_tl->ds_name = new_string(namebuf);
      new_tl->ds_ref = NULL;
      new_tl->next = new_state->transitions;
      new_state->transitions = new_tl;
    }
    
    ex = NULL;
    for (sl=s->exitvals; sl; sl=sl->next) {
      Stringlist *new_sl = new(Stringlist);
      new_sl->string = sl->string;
      new_sl->next = ex;
      ex = new_sl;
    }
    new_state->exitvals = ex;
        
    
  }
}

/* ================================================================= */

void
fixup_state_refs(Block *b)
{
  int i;
  for (i=0; i<b->nstates; i++) {
    State *s = b->states[i];
    Translist *tl;
    for (tl=s->transitions; tl; tl=tl->next) {
      tl->ds_ref = lookup_state(b, tl->ds_name, USE_OLD_MUST_EXIST);
    }
  }
}

/* ================================================================= */

/* Bitmap to contain epsilon closure for NFA */

static unsigned long **eclo;


/* ================================================================= */

static inline const int
round_up(const int x) {
  return (x+31)>>5;
}

/* ================================================================= */

static inline void
set_bit(unsigned long *x, int n)
{
  int r = n>>5;
  unsigned long m = 1UL<<(n&31);
  x[r] |= m;
}

/* ================================================================= */

static inline int
is_set(unsigned long *x, int n)
{
  int r = n>>5;
  unsigned long m = 1UL<<(n&31);
  return !!(x[r] & m);
}

/* ================================================================= */
/* During the algorithm to transitively close the epsilon closure table,
   maintain a stack of indices that have to be rescanned.  This avoids the slow
   approach of repeatedly rescanning the whole table until no changes are
   found. */

typedef struct IntPair {
  struct IntPair *next;
  int i;
  int j;
} IntPair;

static IntPair *freelist=NULL;
static IntPair *stack=NULL;

/* ================================================================= */

static void
push_pair(int i, int j)
{
  static const int grow_by = 32;
  IntPair *np;
  
  if (!freelist) {
    IntPair *ip = new_array(IntPair, grow_by);
    int x;
    for (x=1; x<grow_by; x++) {
      ip[x].next = &ip[x-1];
    }
    ip[0].next = NULL;
    freelist = &ip[grow_by-1];
  }
  np = freelist;
  freelist = freelist->next;
  np->next = stack;
  stack = np;
  np->i = i;
  np->j = j;
}


/* ================================================================= */

static int
pop_pair(int *i, int *j) {
  IntPair *ip;
  if (!stack) {
    return 0;
  } else {
    ip = stack;
    *i = ip->i;
    *j = ip->j;
    stack = ip->next;
    ip->next = freelist;
    freelist = ip;
    return 1;
  }
}

/* ================================================================= */

static void
generate_epsilon_closure(Block *b)
{
  int i, j, N;
  
  N = b->nstates;
  eclo = new_array(unsigned long*, N);
  for (i=0; i<N; i++) {
    eclo[i] = new_array(unsigned long, round_up(N));
    for (j=0; j<round_up(N); j++) {
      eclo[i][j] = 0;
    }
  }

  /* Determine initial immediate transitions */
  for (i=0; i<N; i++) {
    State *s = b->states[i];
    Translist *tl;
    int from_state = s->index;
    set_bit(eclo[from_state], from_state); /* Always reflexive */
    
    for (tl=s->transitions; tl; tl=tl->next) {
      if (tl->token < 0) { /* epsilon trans */
        int to_state = tl->ds_ref->index;
        set_bit(eclo[from_state], to_state);
        push_pair(from_state, to_state);
      }
    }
  }

  /* Now keep on processing until the table is transitively closed */
  while (pop_pair(&i, &j)) {
    int k;
    for (k=0; k<N; k++) {
      if (is_set(eclo[j], k) && !is_set(eclo[i], k)) {
        set_bit(eclo[i], k);
        push_pair(i,k);
      }
    }
  }
}

/* ================================================================= */

static void
print_nfa(Block *b)
{
  int i, j, N;
  N = b->nstates;
  for (i=0; i<N; i++) {
    State *s = b->states[i];
    Translist *tl;
    Stringlist *sl;
    fprintf(stderr, "NFA state %d = %s\n", i, s->name);
    for (tl=s->transitions; tl; tl=tl->next) {
      fprintf(stderr, "  [%s] -> %s\n",
              (tl->token >= 0) ? toktable[tl->token] : "(epsilon)",
              tl->ds_name);
    }
    if (s->exitvals) {
      int first = 1;
      fprintf(stderr, "  Exit value : ");
      for (sl=s->exitvals; sl; sl=sl->next) {
        fprintf(stderr, "%s%s",
                first ? "" : "|",
                s->exitvals->string);
      }
      fprintf(stderr, "\n");
    }
    fprintf(stderr, "  Epsilon closure :\n    (self)\n");
    for (j=0; j<N; j++) {
      if (i!=j && is_set(eclo[i], j)) {
        fprintf(stderr, "    %s\n", b->states[j]->name);
      }
    }
    
    fprintf(stderr, "\n");
  }

}

/* ================================================================= */

/* Indexed [from_state][token][to_state], flag set if there is
   a transition from from_state to to_state, via token then zero or more
   epsilon transitions */

static unsigned long ***transmap;

/* ================================================================= */

static void
build_transmap(Block *b)
{
  int N = b->nstates;
  int Nt = ntokens;
  int i, j, k, m;
  
  transmap = new_array(unsigned long **, N);
  for (i=0; i<N; i++) {
    transmap[i] = new_array(unsigned long *, Nt);
    for (j=0; j<Nt; j++) {
      transmap[i][j] = new_array(unsigned long, round_up(N));
      for (k=0; k<round_up(N); k++) {
        transmap[i][j][k] = 0UL;
      }
    }
  }

  for (i=0; i<N; i++) {
    State *s = b->states[i];
    Translist *tl;
    for (tl=s->transitions; tl; tl=tl->next) {
      if (tl->token >= 0) {
        int dest = tl->ds_ref->index;
        for (m=0; m<round_up(N); m++) {
          transmap[i][tl->token][m] |= eclo[dest][m];
        }
      }
    }
  }

  
}

/* ================================================================= */

typedef struct {
  unsigned long *nfas;
  int *map; /* index by token code */
  Stringlist *exitvals;
} DFAS;

static DFAS **dfas;
static int ndfa=0;
static int maxdfa=0;

/* ================================================================= */

static void
grow_dfa(void)
{ 
  maxdfa += 32;
  dfas = resize_array(DFAS*, dfas, maxdfa);
}

/* ================================================================= */
/* Simple linear search */

static int
find_dfa(unsigned long *nfas, int N)
{
  int res=-1;
  int i, j;
  for(i=0; i<ndfa; i++) {
    int matched=1;
    for (j=0; j<round_up(N); j++) {
      if (nfas[j] != dfas[i]->nfas[j]) {
        matched = 0;
        break;
      }
    }
    if (matched) {
      return i;
    }
  }
  return -1;
}

/* ================================================================= */

static int
add_dfa(Block *b, unsigned long *nfas, int N, int Nt)
{
  int j;
  int result = ndfa;
  Stringlist *ex;

  fprintf(stderr, "Adding DFA state %d\n", ndfa);
  fflush(stderr);
  
  if (maxdfa == ndfa) {
    grow_dfa();
  }

  dfas[ndfa] = new(DFAS);
  dfas[ndfa]->nfas = new_array(unsigned long, round_up(N));
  dfas[ndfa]->map = new_array(int, Nt);
  
  for (j=0; j<round_up(N); j++) {
    dfas[ndfa]->nfas[j] = nfas[j];
  }

  ex = NULL;
  for (j=0; j<N; j++) {
    if (is_set(dfas[ndfa]->nfas, j)) {
      Stringlist *sl;
      State *s = b->states[j];
      for (sl = s->exitvals; sl; sl = sl->next) {
        Stringlist *new_sl;
        new_sl = new(Stringlist);
        new_sl->string = sl->string;
        new_sl->next = ex;
        ex = new_sl;
      }
    }
  }
  
  dfas[ndfa]->exitvals = ex;
        
  ndfa++;
  return result;
}

/* ================================================================= */

static void
clear_nfas(unsigned long *nfas, int N)
{
  int i;
  for (i=0; i<round_up(N); i++) {
    nfas[i] = 0;
  }
}

/* ================================================================= */

static void
build_dfa(Block *b, int start_index)
{
  unsigned long *nfas;
  int i;
  int N, Nt;
  int next_to_do;
  
  N = b->nstates;
  Nt = ntokens;
  
  /* Add initial state */
  nfas = new_array(unsigned long, round_up(N));
  clear_nfas(nfas, N);
  for (i=0; i<round_up(N); i++) {
    nfas[i] |= eclo[start_index][i];
  }
  add_dfa(b, nfas, N, Nt);
  next_to_do = 0;

  /* Now the heart of the program : the subset construction to turn the NFA
     into a DFA. */

  while (next_to_do < ndfa) {

    int t; /* token index */
    int j, k, m;
    int idx;
    int found_any;
    
    for (t=0; t<Nt; t++) {
      clear_nfas(nfas, N);
      found_any = 0;
      for (j=0; j<N; j++) {
        if (is_set(dfas[next_to_do]->nfas, j)) {
          for (k=0; k<round_up(N); k++) {
            unsigned long x;
            x = transmap[j][t][k];
            nfas[k] |= x;
            found_any |= !!x;
          }
        }
      }
      if (found_any) {
        idx = find_dfa(nfas, N);
        if (idx < 0) {
          idx = add_dfa(b, nfas, N, Nt);
        }
      } else {
        idx = -1;
      }
      dfas[next_to_do]->map[t] = idx;
    }
    next_to_do++;
  }
}

/* ================================================================= */

static void
print_dfa(Block *b)
{
  int N = b->nstates;
  int Nt = ntokens;
  
  int i, j, t;
  Stringlist *ex;
  
  for (i=0; i<ndfa; i++) {
    fprintf(stderr, "DFA state %d\n", i);
    fprintf(stderr, "  NFA states :\n");
    for (j=0; j<N; j++) {
      if (is_set(dfas[i]->nfas, j)) {
        fprintf(stderr, "    %s\n", b->states[j]->name);
      }
    }
    fprintf(stderr, "\n");
    fprintf(stderr, "  Transitions :\n");
    for (t=0; t<Nt; t++) {
      int dest = dfas[i]->map[t];
      if (dest >= 0) {
        fprintf(stderr, "    %s -> %d\n", toktable[t], dest);
      }
    }
    fprintf(stderr, "  Exit values :\n");
    for (ex = dfas[i]->exitvals; ex; ex=ex->next) {
      fprintf(stderr, "    %s\n", ex->string);
    }
    
    fprintf(stderr, "\n");
  }
}

/* ================================================================= */

static void
print_tables(Block *b)
{
  int N = b->nstates;
  int Nt = ntokens;
  int n, i, j;
  extern char *prefix;
  char ucprefix[1024];

  if (prefix) {
    printf("static unsigned long %s_exitval[] = {\n", prefix);
  } else {
    printf("static unsigned long exitval[] = {\n");
  }
  for (i=0; i<ndfa; i++) {
    Stringlist *sl;

    printf("0UL");
    for (sl=dfas[i]->exitvals; sl; sl=sl->next) {
      printf("|%s", sl->string);
    }
    putchar ((i<(ndfa-1)) ? ',' : ' ');
    printf(" /* State %d */\n", i);
  }
  printf("};\n\n");

  n = 0;
  if (prefix) {
    printf("static short %s_trans[] = {", prefix);
  } else {
    printf("static short trans[] = {");
  }
  for (i=0; i<ndfa; i++) {
    for (j=0; j<Nt; j++) {
      if (n>0) putchar (',');
      if (n%8 == 0) {
        printf("\n  ");
      } else {
        putchar(' ');
      }
      n++;
      printf("%4d", dfas[i]->map[j]);
    }
  }

  printf("\n};\n\n");

  if (prefix) {
    char *p;
    strcpy(ucprefix, prefix);
    for (p=ucprefix; *p; p++) {
      *p = toupper(*p);
    }
    printf("#define NEXT_%s_STATE(s,t) %s_trans[%d*(s)+(t)]\n",
           ucprefix, prefix, Nt);
  } else {
    printf("#define NEXT_STATE(s,t) trans[%d*(s)+(t)]\n", Nt);
  }
}

/* ================================================================= */

void yyerror (char *s)
{
  extern int lineno;
  fprintf(stderr, "%s at line %d\n", s, lineno);
}

/* ================================================================= */

int yywrap(void) { return -1; }

/* ================================================================= */

int main (int argc, char **argv)
{
  int result;
  State *start_state;
  result = yyparse();
  start_state = get_curstate(); /* The last state to be current in the input file is the entry state of the NFA */
  generate_epsilon_closure(start_state->parent);
  print_nfa(start_state->parent);
  build_transmap(start_state->parent);
  build_dfa(start_state->parent, start_state->index);
  print_dfa(start_state->parent);
  print_tables(start_state->parent);
  
  return result;
}