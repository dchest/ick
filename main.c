/* ick - stupid (but fast) static site generator
**
** Copyright 2009 Dmitry Chestnykh <dmitry@codingrobots.com>
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**    http://www.apache.org/licenses/LICENSE-2.0

** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
**
*/

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
#ifdef __APPLE__
#include <copyfile.h>
#endif
#include <sys/time.h>
#include <utime.h>

#include "hashtable.h"
#include "markup.h"

/* Global constants */
#define TEMPLATES_DIR "templates"
#define CONTENT_DIR "content"
#define OUTPUT_DIR "output"
#define DEFAULT_TEMPLATE "default.html"
/* Predefined variables for templates */
#define VAR_TEMPLATE "template"
#define VAR_CONTENT  "content"
#define VAR_MARKUP  "markup"
#define VAR_VALUE_NONE  "none"

struct template {
  int fd;     /* File descriptor */
  size_t len; /* Size of buf */
  char *buf;  /* Buffer with contents */
  /* Variables */
  char *varnames[100]; // stack of pointers to names of variables
  char *varplaces[100]; // stack of pointers to places after variables
  int varnum;
};

struct filevars {
  char *names[100]; // stack of pointers to names of variables
  char *values[100]; // stack of pointers to values of variables
  int num;
};

/* Global vars */
struct hashtable *gtemplates;
int grebuild; /* indicates whether to rebuild the whole output */
int gnumfiles, gnumchanged, gnumnew;

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
    hashtable_insert(ht, strdup(ent->d_name), tpl);
  }
  closedir(dir);
  return ht;
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

void processfile(char *filename, FILE *out)
{
  int fd;
  char *buf;
  struct stat st;
  int skip = 0;
  struct filevars fvars;
  struct template *tpl;
  int nomarkup = 0;
    
  fd = open(filename, O_RDONLY);
  if (fd < 0 || fstat(fd, &st))
    panic("cannot open file %s", filename);
  
  if (st.st_size == 0) {
    //printf("(skipping zero-length file) ");
    close(fd);
    return;
  }
          
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
  buf += i;
  st.st_size -= i;
  
  int v;
  
  v = findfilevar(VAR_TEMPLATE, &fvars);
  if (v != -1) {
    /* Use custom template */
    //TODO: load from list of templates, don't read it here
    tpl = (struct template *)hashtable_search(gtemplates, fvars.values[v]);
    if (!tpl)
      panic("unknown template '%s' in file '%s'", fvars.values[v], filename);
  } else {
    // Use default template
    tpl = (struct template *)hashtable_search(gtemplates, DEFAULT_TEMPLATE);
    if (!tpl)
      panic("no default template (file %s)", filename);
  }
  
  v = findfilevar(VAR_MARKUP, &fvars);
  if (v != -1) {
    if (strcmp(fvars.values[v], VAR_VALUE_NONE) == 0)
      nomarkup = 1;
    else
      panic("Unknown markup '%s' in file %s", fvars.values[v], filename);
  }
    
  /* Output */
  fprintf(out, "%s", tpl->buf); /* write up to first variable */
  for (int i=0; i < tpl->varnum; i++) {

    if (strcmp(tpl->varnames[i], VAR_CONTENT) == 0) {
      if (nomarkup)
        fwrite(buf, st.st_size, 1, out); /* write content */
      else
        markup(buf, st.st_size, out);
    }

    v = findfilevar(tpl->varnames[i], &fvars);
    if (v != -1)
      fprintf(out, "%s", fvars.values[v]); /* write variable value */

    else if (strstr(tpl->varnames[i], "if ") == tpl->varnames[i]) {
      char *var = (char *)tpl->varnames[i] + 3; /* if[space] */
      //printf("VAR: %s", var);
      if (findfilevar(var, &fvars) == -1)
        skip = 1; 
    } 
    else if (strstr(tpl->varnames[i], "ifnot ") == tpl->varnames[i]) {
      char *var = (char *)tpl->varnames[i] + 7; /* unless[space] */
      //printf("VAR: %s", var);
      if (findfilevar(var, &fvars) != -1)
        skip = 1; 
    } 
    else if (strstr(tpl->varnames[i], "ifeq ") == tpl->varnames[i]) {
      char *text = strdup((char *)tpl->varnames[i] + 5); /* ifeq[space] */
      char *var;
      var = strsep(&text, " ");
      //printf("VAR: %s\nTEXT: %s\n", var, text);
      v = findfilevar(var, &fvars);
      if (v == -1 || strcmp(fvars.values[v], text) != 0)
        skip = 1; 
      free(var);
    } 
    else if (strcmp(tpl->varnames[i], "endif") == 0)
      skip = 0;

    /* write after var */
    if (skip)
      continue;
    if (i < tpl->varnum-1) {
      fprintf(out, "%s", tpl->varplaces[i]);
    } 
    else {/* write remaining bytes. cannot fprintf, as buf doesn't end with 0. */
      size_t rem = tpl->len - (tpl->varplaces[i] - tpl->buf);
      fwrite(tpl->varplaces[i], rem, 1, out);
    }
  }  
  close(fd);
}

/* Check if file has .ick extension, and remove it if it has. */
/* Return 1 if has .ick extension, 0 if not. */
int ickfile(char *filename)
{
  size_t len = strlen(filename);
  if (len > 4
      && filename[len-4] == '.'
      && filename[len-3] == 'i'
      && filename[len-2] == 'c'
      && filename[len-1] == 'k') {
    filename[len-4] = '\0';
    return 1;
  }
  return 0;
}

void processcontent(char *path, char *outpath)
{
  DIR *dir;
  FILE *f;
  struct dirent *ent;
  char fullpath[PATH_MAX], fulloutpath[PATH_MAX];
  struct template *tpl;
  struct hashtable *ht = create_hashtable_m(53);
  int isick;
  size_t len;
  struct stat stin, stout;
  int outexists;
  struct utimbuf ut;

  dir = opendir(path);
  if (!dir)
    panic("cannot open directory %s", path);

  mkdir(outpath, 0755);
  while ((ent = readdir(dir)) != NULL) {
    if ((ent->d_name[0] == '.') && (ent->d_name[1] == '\0' ||
       ((ent->d_name[1] == '.') && (ent->d_name[2] == '\0'))))
       continue;
    
    if (strcmp(ent->d_name, ".DS_Store") == 0)
      continue; /* ignore crap */
    
    snprintf(fullpath, PATH_MAX, "%s/%s", path, ent->d_name);
    snprintf(fulloutpath, PATH_MAX, "%s/%s", outpath, ent->d_name);
    if (ent->d_type == DT_DIR) {
      processcontent(fullpath, fulloutpath);
      continue;
    }
    
    char *printpath = fullpath + strlen(CONTENT_DIR) + 1;

    gnumfiles++;
    isick = ickfile(fulloutpath);
    stat(fullpath, &stin);
    outexists = (stat(fulloutpath, &stout) == 0);
    if (!grebuild &&  outexists
        && stin.st_mtime == stout.st_mtime) {
      printf("=  %s\n", printpath); /* not changed */
      continue;
    }
    if (outexists)
      gnumchanged++;
    else
      gnumnew++;
  		
    if (isick) {
      // Process ick file
      f = fopen(fulloutpath, "w");
      if (!f)
        panic("cannot open file %s for write", fulloutpath);
      processfile(fullpath, f);
      if (outexists)
        printf("*  %s\n", printpath);
      else
        printf("+  %s\n", printpath);
      fclose(f);
      ut.actime  = stin.st_atime;
      ut.modtime = stin.st_mtime;
      utime(fulloutpath, &ut);
    } else {
      // Just copy file
      #ifdef __APPLE__
      int flags = COPYFILE_ALL | COPYFILE_NOFOLLOW_SRC;
    	if (copyfile(fullpath, fulloutpath, NULL, flags) != 0)
        panic("cannot copy file %s to %s", fullpath, fulloutpath);
      if (outexists)
        printf("*> %s\n", printpath);
      else
        printf("+> %s\n", printpath);      
      #else
      panic("copy is not yet implemented for this platform");
      #endif
    }
  }
  closedir(dir);
}


void closetemplate(void *tpl)
{
  close(((struct template *)tpl)->fd);
}

void main(int argc, char *argv[])
{  
  gnumfiles = gnumchanged = gnumnew = 0;
  grebuild = 0;
  
  if (argc > 1 && strcmp(argv[1], "rebuild") == 0)
    grebuild = 1;
  
  struct timeval tp;
  double start, end;

  gettimeofday(&tp, NULL);
  start = tp.tv_sec + tp.tv_usec/1E6;
  
  gtemplates = gettemplates(TEMPLATES_DIR);
  processcontent(CONTENT_DIR, OUTPUT_DIR);

  gettimeofday(&tp, NULL);
  end = tp.tv_sec + tp.tv_usec/1E6;
 
  printf("\n Done in %0.3f sec\n", end-start);
  printf(" %d files, %d new, %d modified\n", gnumfiles, gnumnew, gnumchanged);
  
  //for (int i=1; i < argc; i++)
  //  processfile(argv[i], gtemplates, stdout);
  
  
  /* Close files */
  //hashtable_iter(templates, closetemplate);
  //hashtable_destroy(templates, 1);
  exit(0);
}