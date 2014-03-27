/***************************************
  Driver for producing XML output from the glosser.
  ***************************************/

/* COPYRIGHT danr 2014 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "functions.h"
#include "output.h"

static char tag_stack[100][100];
static int  tag_head[100];
static char tag_top;
static char last_tag_top;

int ref;
int first;

static void
initialise(void)
{
  ref = 1;
  first = 0;
  tag_top = 0;
  last_tag_top = 0;
}


static void
write_prologue(void) { }

static void
write_epilog(void) { }

static void
clear_eols(void) { }

static void
set_eols(int eols) { }

static void
write_open_bracket(BracketType type, int subscript) { }

static void
write_close_bracket(BracketType type, int subscript) { }

/*++++++++++++++++++++++++++++++
  Make a string safe for setting with HTML.

  static char * make_htmlsafe

  char *s
  ++++++++++++++++++++++++++++++*/

static char *
make_htmlsafe(char *s)
{
  static char buf[2048];
  char *p, *q;
  p = s;
  q = buf;
  while (*p) {
    switch (*p) {
      case '&':
        strcpy(q, "&amp;");
        q += 5;
        break;
      case '<':
        strcpy(q, "&lt;");
        q += 4;
        break;
      case '>':
        strcpy(q, "&gt;");
        q += 4;
        break;
      default:
        *q++ = *p++;
        break;
    }
  }
  *q = 0;

  return buf;
}

static void
write_lojban_text(char *text) { }

static void
write_special(char *text) {
  //printf("<special val=\"%s\"/>\n",text);
}


static void
write_translation(char *text) {
  //printf("<translation val=\"%s\"/>\n",text);
}

static void
start_tags(void)
{
  last_tag_top = tag_top;
}

static void
end_tags(void) { }

static void
start_tag(void) { }

static void
write_tag_text(char *brivla, char *place, char *trans, int brac)
{
  // printf("<tag brivla=\"%s\" place=\"%s\" trans=\"%s\">\n",brivla,place,trans);
  strcpy(tag_stack[tag_top],trans);
  tag_head[tag_top] = ref;
  tag_top++;
  first = 1;
  //printf("<tag state=\"%d %d\">\n",tag_top,last_tag_top);
}

static void
write_stop_tag() {
  //printf("</tag state=\"%d %d\">\n",tag_top,last_tag_top);
  tag_head[tag_top] = 0;
  tag_top--;
  last_tag_top--;
}

static void write_partial_tag_text(char *t) { }

static void write_lojban_word_and_translation(char *loj, char *eng, char *selmaho) {
  int i;
  if(first) {

    printf("<word pos=\"%s\" trans=\"%s\" tags=\"",selmaho,eng);
    for (i=tag_top-1; i>=last_tag_top; i--) {
      printf("%s",tag_stack[i]);
      if (i != last_tag_top) {
        printf("|");
      }
    }
    printf("\" ref=\"%d\">%s</word>\n",ref,loj);
    first = 0;

  } else {

    printf("<word pos=\"%s\" trans=\"%s\" tags=\"\"",selmaho,eng);
    if(tag_head[last_tag_top] != 0) {
      printf(" ref=\"%d\" dephead=\"%d\">%s</word>\n",ref,tag_head[last_tag_top],loj);
    } else {
      printf(" ref=\"%d\">%s</word>\n",ref,loj);
    }
  }
  ref++;
}

DriverVector xml_driver =
{
  initialise,
  write_prologue,
  write_epilog,
  write_open_bracket,
  write_close_bracket,
  set_eols,
  write_lojban_text,
  write_translation,
  start_tags,
  end_tags,
  start_tag,
  write_tag_text,
  write_partial_tag_text,
  write_lojban_word_and_translation,
  write_stop_tag,
};
