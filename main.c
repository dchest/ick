#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <strings.h>
#include <fcntl.h>
#include <stdarg.h>
#include <dirent.h>
#include <limits.h>

#include "hashtable.h"

/* Global constants */
const char *templatefile = "template.html";

struct template {
  int fd;     /* File descriptor */
  size_t len; /* Size of buf */
  char *buf;  /* Buffer with contents */
  /* Variables */
  //char *var_content_place;
  char *varnames[100]; // stack of pointers to names of variables
  char *varplaces[100]; // stack of pointers to places after variables
  int varnum;
};

struct filevars {
  char *names[100]; // stack of pointers to names of variables
  char *values[100]; // stack of pointers to values of variables
  int num;
};

//char *var_content_name = "content";
//size_t var_content_size = 7;

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
  tpl->varnum = 0;
  
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
      tpl->varnames[tpl->varnum] = curvar;
      tpl->varplaces[tpl->varnum] = (char *)buf+i;
      tpl->varnum++;
      if (tpl->varnum > sizeof(tpl->varnames)) {
        panic("too many (%d) variables (more than %d) in template %s",
          tpl->varnum, sizeof(tpl->varnames), filename);
      }
    }
  }
}
/* Search for variable in template. Currently not used
int findtplvar(char *var, struct template *tpl)
{
  for (int i=0; i < tpl->varnum; i++) {
    if (strcmp(tpl->varnames[i], var) == 0)
      return i;
  }
  return -1;
}
*/

int findfilevar(char *var, struct filevars *fvars)
{
  for (int i=0; i < fvars->num; i++) {
    if (strcmp(fvars->names[i], var) == 0)
      return i;
  }
  return -1;
}


void processfile(char *filename, struct hashtable *templates, FILE *out)
{
  int fd;
  char *buf;
  struct stat st;
  int skip = 0;
  struct filevars fvars;
  struct template *tpl;
    
  fd = open(filename, O_RDONLY);
  if (fd < 0 || fstat(fd, &st))
    panic("cannot open file %s", filename);
                
  buf = mmap(NULL, st.st_size, PROT_READ|PROT_WRITE, MAP_PRIVATE, fd, 0);
  if (buf == MAP_FAILED)
    panic("mmap on file (%s) failed", filename);
    
  /* Parse variables */
  fvars.num = 0;
  int i;
  for (i = 0; i < st.st_size; i++) {
    if (buf[i] == '=') {
      i++;
      fvars.names[fvars.num] = (char *)buf+i;
      do {
        if (i++ == st.st_size)
          panic("malformed file %s (varname after = required)", filename);        
      } while (buf[i] != ' ' && buf[i] != '\t');
      buf[i] = '\0';
      do {
        if (i++ == st.st_size)
          panic("malformed file %s (value after = required)", filename);
      } while (buf[i] == ' ' || buf[i] == '\t');
      fvars.values[fvars.num] = (char *)buf+i;
      while (buf[++i] != '\n' && i < st.st_size) 
      /*nothing*/;
      buf[i] = '\0';
      //printf("READ VARNAME=%s\n", fvars.names[fvars.num]);
      //printf("READ VARVALUE=%s\n", fvars.values[fvars.num]);
      fvars.num++;
    } else {
      if (buf[i] == '\n') /* skip line feed after variables */
        i++;
      break;
    }
  }
  buf = buf+i;
  
  int v;
  
  v = findfilevar("template", &fvars);
  if (v != -1) {
    /* Use custom template */
    //TODO: load from list of templates, don't read it here
    printf("using custom template");
    tpl = (struct template *)hashtable_search(templates, fvars.values[v]);
  } else {
    // Use default template
    tpl = (struct template *)hashtable_search(templates, "default.html");
    if (!tpl)
      panic("no default template");
  }
    
  /* Output */
  fprintf(out, "%s", tpl->buf); /* write up to first variable */
  for (int i=0; i < tpl->varnum; i++) {

    if (strcmp(tpl->varnames[i], "content") == 0)
      fwrite(buf, st.st_size, 1, out); /* write content */

    v = findfilevar(tpl->varnames[i], &fvars);
    if (v != -1)
      fprintf(out, "%s", fvars.values[v]); /* write variable value */

    else if (strstr(tpl->varnames[i], "if ") == tpl->varnames[i]) {
      char *var = (char *)tpl->varnames[i] + 3; /* if[space] */
      //printf("VAR: %s", var);
      if (findfilevar(var, &fvars) == -1)
        skip = 1; 
    } 
    else if (strcmp(tpl->varnames[i], "endif") == 0)
      skip = 0;

    /* write after var */
    if (!skip)
      fprintf(out, "%s", tpl->varplaces[i]);
  }  
  close(fd);
}

struct hashtable *gettemplates(char *path)
{
  DIR *dir;
  struct dirent *ent;
  char fullpath[PATH_MAX];
  struct template *tpl;
  struct hashtable *ht = create_hashtable_m(53);

  dir = opendir(path);
  if (!dir)
    panic("cannot open directory %s", path);

  while ((ent = readdir(dir)) != NULL) {
    if ((ent->d_name[0] == '.') && (ent->d_name[1] == '\0' ||
       ((ent->d_name[1] == '.') && (ent->d_name[2] == '\0')))
       || ent->d_type == DT_DIR)
       continue;

    snprintf(fullpath, PATH_MAX, "%s/%s", path, ent->d_name);
    tpl = malloc(sizeof(struct template));
    readtemplate(fullpath, tpl);
    hashtable_insert(ht, ent->d_name, tpl);
  }
  closedir(dir);
  return ht;
}

void closetemplate(void *tpl)
{
  close(((struct template *)tpl)->fd);
}

void main(int argc, char *argv[])
{
  struct template default_tpl;
  struct hashtable *templates;
  
  if (argc < 2)
    panic("Use: %s <filenames>", argv[0]);
  
  //readtemplate(templatefile, &default_tpl);
  templates = gettemplates("templates");
  
  for (int i=1; i < argc; i++)
    processfile(argv[i], templates, stdout);
  
  
  /* Close files */
  //hashtable_iter(templates, closetemplate);
  //hashtable_destroy(templates, 1);
}