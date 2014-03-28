/***************************************
  $Header$

  Output formatting functions
  ***************************************/

/* Copyright 1998-2001 Richard P. Curnow */
/* Includes contributions from Björn Gohla to provide the GNUStep
 * interface */
/* LICENCE */

#include "cm.h"

/*+ Which format has been requested +*/
OutputFormat ofmt;

/*+ Line width +*/
int width;

int last_niho;

/* ================================================== */

#define BUFFER_SIZE 512

#ifdef PLIST
static proplist_t dictionary = NULL;
#endif

static char lines[3][BUFFER_SIZE];
static int width_used;

char* escape(char *s)
{
  char *buf;
  char *p, *q;
  buf = malloc(sizeof(char)*2048);
  p = s;
  q = buf;
  while (*p) {
    switch (*p) {
      case '&':
        strcpy(q, "&amp;");
        q += 5;
        p++;
        break;
      case '<':
        strcpy(q, "&lt;");
        q += 4;
        p++;
        break;
      case '>':
        strcpy(q, "&gt;");
        q += 4;
        p++;
        break;
      default:
        *q++ = *p++;
        break;
    }
  }
  *q = 0;

  return buf;
}

/* ================================================== */

static void
block_newline(void)
{
  if (width_used > 0) {
    printf("%s\n", lines[0]);
    printf("%s\n", lines[1]);
    printf("%s\n", lines[2]);
    printf("\n");
  }
  lines[0][0] = lines[1][0] = lines[2][0] = 0;
  width_used = 0;
}

/* ================================================== */

static void
append_content(char *buf, const char *x, int len, int mlen)
{
  int i;
  int d;
  char *p;
  strcat(buf, x);
  d = mlen - len;
  p = buf;
  while (*p) p++;
  for (i=0; i<d; i++) {
    *p++ = ' ';
  }
  *p = 0;
}

/* ================================================== */

static void
do_block(const char *a1, const char *a2, const char *a3)
{
  int l1, l2, l3, ml;

  l1 = strlen(a1);
  l2 = strlen(a2);
  l3 = strlen(a3);
  ml = (l1 > l2) ? l1 : l2;
  ml = (ml > l3) ? ml : l3;

  if (ml + width_used + 1 > width) {
    block_newline();
  }

  append_content(&lines[0][0], a1, l1, ml+1);
  append_content(&lines[1][0], a2, l2, ml+1);
  append_content(&lines[2][0], a3, l3, ml+1);

  width_used += (ml + 1);
  return;
}

/* ================================================== */
/* ================================================== */

void
start_output(void)
{
  last_niho = 1;
  switch (ofmt) {
    case OF_LATEX:
      printf("\\documentclass[10pt]{article}\n"
             "\\usepackage{geometry}\n"
             "\\geometry{left=0.5in,right=0.5in,top=0.5in,bottom=0.5in,a4paper}\n"
             "\\def\\rmdefault{bch}\n"
             "\\setlength{\\parindent}{0pt}\n"
             "\\setlength{\\parskip}{1ex plus0.5ex minus0.5ex}\n"
             "\\setlength{\\baselineskip}{10pt}\n"
             "\\def\\arraystretch{0.7}\n"
             "\\begin{document}\n"
             );
      break;
    case OF_TEXT:
      break;
    case OF_TEXTBLK:
      lines[0][0] = lines[1][0] = lines[2][0] = 0;
      width_used = 0;
      break;
    case OF_XML:
      break;
    case OF_TOKENIZE:
      printf("<p>\n<s>\n");
      break;
#ifdef PLIST
  case OF_PLIST:
      dictionary = PLMakeDictionaryFromEntries(NULL, NULL, NULL);
      break;
#endif
  }

}

/* ================================================== */

void
end_output(void)
{
  switch (ofmt) {
    case OF_LATEX:
      printf("\\end{document}\n");
      break;
    case OF_TEXT:
      break;
    case OF_TEXTBLK:
      block_newline();
      break;
    case OF_XML:
      break;
    case OF_TOKENIZE:
      printf("\n</s>\n</p>\n");
      break;
#ifdef PLIST
    case OF_PLIST:

      /* we could save the dictionary to a named file  */
      /* 	dictionary = PLSetFilename(dictionary, PLMakeString("output.plist")); */
      /* 	PLSave(dictionary, NO); */
      /* but instead for now we just print to stdout */
      printf(PLGetDescription(dictionary));
      break;
#endif //PLIST
  }
}

/* ================================================== */

int isI (const char *str) {
    if (!str[0]) {
        return 0;
    } else {
        if (str[0] == 'I' && (str[1] == '\0' || str[1] == ' ')) {
            return 1;
        } else {
            while (*(str++)) {
                if(str[0] == ' ') {
                    return isI(str++);
                }
            }
            return 0;
        }
    }
}

void
output(char *lojban, const char *trans, char *selmao)
{
  char *safe_lojban;
  char *safe_selmao;
  switch (ofmt) {
    case OF_LATEX:
      printf ("\\begin{tabular}[t]{l}"
          "\\textbf{\\footnotesize %s}\\\\\n"
          "\\textrm{\\footnotesize %s}\\\\\n"
          "\\textit{\\footnotesize %s}\n"
          "\\end{tabular}\n"
          "\\rule{0in}{1.0\\baselineskip}",
          lojban, selmao, trans);
      break;
    case OF_TEXT:
      printf ("%s <%s> [%s] ", lojban, selmao, trans);
      break;
    case OF_TEXTBLK:
      do_block(lojban, selmao, trans);
      break;
    case OF_XML:
      safe_selmao = escape(selmao);
      safe_lojban = escape(lojban);
      printf("<word pos=\"%s\">%s</word>\n", safe_selmao, safe_lojban);
      free(safe_selmao);
      free(safe_lojban);
      break;
    case OF_TOKENIZE:
      if( isI(selmao) && !last_niho) {
        printf("\n</s>\n<s>\n");
      }
      if(strstr(selmao,"NIhO") && !last_niho) {
        printf("\n</s>\n</p>\n<p>\n<s>\n");
        last_niho = 1;
      } else {
        last_niho = 0;
      }
      printf("%s ",lojban);
      break;
#ifdef PLIST
    case OF_PLIST:
      dictionary = PLInsertDictionaryEntry(dictionary, PLMakeString(lojban), PLMakeString(trans));
      break;
#endif //PLIST
  }
}
/* ================================================== */

void
output_newline(void)
{
  switch (ofmt) {
    case OF_LATEX:
    printf("\n\n");
      break;
    case OF_TEXT:
    printf("\n");
      break;
    case OF_TEXTBLK:
      block_newline();
      printf("\n");
      break;
    case OF_XML:
      break;
    case OF_TOKENIZE:
      break;
  }
}

/* ================================================== */

void
output_paren(const char *text)
{
  switch (ofmt) {
    case OF_LATEX:
      printf ("\\textrm{\\footnotesize %s}", text);
      break;
    case OF_TEXT:
      printf ("(%s) ", text);
      break;
    case OF_TEXTBLK:
      do_block("(", "(", "(");
      do_block(text, "", "");
      do_block(")", ")", ")");
      break;
    case OF_XML:
      break;
  }
}

/* ================================================== */
