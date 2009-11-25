#include <stdio.h>
#include <ctype.h>
#include <string.h>

/* ATTENTION: HORRIBLE, HORRIBLE CODE AHEAD! */
/* Don't even try to fix it, rewrite!        */
/* I suck in parsers.                        */

char *blocktags[] = {"DIV", "PRE", "H1", "H2", "H3", "H4", "H5", "H6",
                     "UL", "LI", "OL", "P", "HR"};
int blocktagscount = 13;

char *pretags[] = {"PRE"};
int pretagscount = 1;

int iswhitespace(char c)
{
  return (c == ' ' || c == '\t' || c == '\n' || c == '\r');
}

int next_whitespace_and_lf(char *buf, size_t start, size_t len)
{
  for (int i=start; i<len; i++) {
    if (!iswhitespace(buf[i]))
      return 0;
    if (buf[i] == '\n')
      return 1;
  }
  return 0;
}

int next_tag(char *buf, size_t start, size_t len, char *tag, int closing)
{
  for (int i=start; i<len; i++) {
    if (iswhitespace(buf[i]))
      continue;
    if (buf[i] == '<') {
      i++;
      if (closing) {
        if (buf[i] != '/')
          return 0;
        i++;
      }
      for (int j=0; j<strlen(tag); j++) {
        if (i+j >= len
           || toupper(buf[i+j]) != tag[j])
          return 0;
      }
      return 1;
    }
    return 0;
  }
  return 0;
}

int next_tag_in_list(char *buf, size_t start, size_t len, 
                     char *list[], int listlen, int closing)
{
  for (int i=0; i<listlen; i++) {
    if (next_tag(buf, start, len, list[i], closing))
      return 1;
  }
  return 0;
}

int next_block_tag(char *buf, size_t start, size_t len, int closing)
{
  return next_tag_in_list(buf, start, len, blocktags, blocktagscount, closing);
}

int next_pre_tag(char *buf, size_t start, size_t len, int closing)
{
  return next_tag_in_list(buf, start, len, pretags, pretagscount, closing);
}

int find_next_char(char *buf, size_t start, size_t len, char c)
{
  for (int i=start; i<len; i++) {
    if (buf[i] == c)
      return i;
  }
  return len;
}

void markup(char *buf, size_t len, FILE *out)
{
  int intag = 0, paragraph = 0, inblock = 0, inpre = 0;
  char c;
  
  for (int i=0; i<len; i++) {
    switch (buf[i]) {
    case '<':
      intag = 1;
      if (next_block_tag(buf, i, len, 0))
        inblock++;
      if (next_pre_tag(buf, i, len, 0))
        inpre++;
      if (i+1 < len && buf[i+1] == '/') { /* closing tag */ 
        if (next_block_tag(buf, i, len, 1))
          inblock--;
        if (next_pre_tag(buf, i, len, 1))
          inpre--;
      }
      if (inblock < 0) inblock = 0; /* just in case */
      if (inpre < 0) inpre = 0;
      //if (paragraph && inblock) {
      //  fprintf(out, "</p>");
      //  paragraph = 0;
      //}
      break;
    case '>':
      intag = 0;
      break;
    case '*':
    case '_':
    case '`':
      c = buf[i];
      char *tag;
      switch (c) {
        case '*': tag = "b"; break;
        case '_': tag = "i"; break;
        case '`': tag = "code"; break;
      }
      if (!intag && !inpre) {
        int wrote = 0;
        for (int j=i+1; j<len; j++) {
          if (buf[j] == '\n')
            break;
          if (buf[j] == c) {
            fprintf(out, "<%s>", tag);
            fwrite(buf+i+1, j-i-1, 1, out);
            fprintf(out, "</%s>", tag);
            i = j;
            wrote = 1;
            break;
          }
        }
        if (wrote)
          continue;
      }
      break;
    case '[':
      if (!intag && !inpre) {
        int wrote = 0;
        for (int j=i+1; j<len-1; j++) {
          if (buf[j] == ']' && buf[j+1] == '(') {
            fprintf(out, "<a href=\"");
            int p = find_next_char(buf, j+2, len, ')');
            fwrite(buf+j+2, p-j-2, 1, out); /* link href */
            fprintf(out, "\">");
            fwrite(buf+i+1, j-i-1, 1, out); /* link text */
            fprintf(out, "</a>");
            i = p;
            wrote = 1;
            break;
          }
        }
        if (wrote)
          continue;
      }
      break;
    case '\n':
      if (!intag && !inpre && next_whitespace_and_lf(buf, i+1, len)) {
        if (paragraph)
          fprintf(out, "</p>");
        /* find next tag */
        if (!next_block_tag(buf, i+1, len, 0)
            && !next_block_tag(buf, i+1, len, 1)) {
          paragraph = 1;
          fprintf(out, "\n\n<p>");
          i++;
          continue;
        } else {
          paragraph = 0;
        }
      }
      break;
    }
    fwrite(buf+i, 1, 1, out);
  }
  if (paragraph) {
    fprintf(out, "</p>");
    paragraph = 0;
  }
}
