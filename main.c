#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <strings.h>
#include <fcntl.h>
#include <stdarg.h>

/* Global constants */
const char *templatefile = "template.html";

struct template {
  int fd;     /* File descriptor */
  size_t len; /* Size of buf */
  char *buf;  /* Buffer with contents */
  /* Variables */
  char *var_content_place;
};

char *var_content_name = "content";
size_t var_content_size = 7;

void panic(char *fmt, ...)
{
  fprintf(stderr, "Error: ");
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  fprintf(stderr, "\n");
  exit(1);
}

void readtemplate(const char *filename, struct template *tpl)
{
  struct stat st;
  char *curvar;
  
  tpl->fd = open(filename, O_RDONLY);
  if (tpl->fd < 0 || fstat(tpl->fd, &st))
    panic("cannot open template file %s", filename);
  tpl->len = st.st_size;
            
  tpl->buf = mmap(NULL, tpl->len, PROT_READ|PROT_WRITE, MAP_PRIVATE, tpl->fd, 0);
  if (tpl->buf == MAP_FAILED)
    panic("mmap on template file (%s) failed", filename);

  char *buf = tpl->buf; /* to simplify code */
  for (int i = 0; i < st.st_size; i++) {
    if (buf[i] == '$' && i > 0 && buf[i-1] == '{') {
      buf[i-1] = '\0';
      curvar = (char *)(buf+i+1);
      while (buf[i++] != '}') { 
        if (i == st.st_size)
          panic("malformed template (%s): variable is not closed", filename);
      }
      buf[i-1] = '\0';
      //printf("curvar: %s\n", curvar);
      if (strcmp(curvar, var_content_name) == 0) {
        tpl->var_content_place = (char *)buf+i;
      } else {
        panic("no content variable");
      }
    }
  }
}

void processfile(char *filename, struct template *tpl, FILE *out)
{
  int fd;
  char *buf;
  struct stat st;
  
  fd = open(filename, O_RDONLY);
  if (fd < 0 || fstat(fd, &st))
    panic("cannot open file %s", filename);
                
  buf = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
  if (buf == MAP_FAILED)
    panic("mmap on file (%s) failed", filename);
  
  fprintf(out, "%s", tpl->buf);
  /* out content */
  fwrite(buf, st.st_size, 1, out);
  /* out after content */
  fprintf(out, "%s", tpl->var_content_place);
  
  close(fd);
}

void main(int argc, char *argv[])
{
  struct template default_tpl;
  
  readtemplate(templatefile, &default_tpl);
  processfile("index.md", &default_tpl, stdout);
  
  close(default_tpl.fd);
}