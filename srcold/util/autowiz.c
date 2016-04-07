/* ************************************************************************
*  file:  autowiz.c                                     Part of CircleMUD *
*  Usage: self-updating wizlists                                          *
*  Written by Jeremy Elson                                                *
*                                                                         *
*  Updated to run using Ascii playerfiles                                 *
*  by Caniffe <caniffe@myrealbox.com>                                     *
*                                                                         *
*  All Rights Reserved                                                    *
*  Copyright (C) 1993 The Trustees of The Johns Hopkins University        *
************************************************************************* */

/*
 * NB: This file contains modified code used in Sammy's Ascii playerfiles
 * 'diskio.c' function.  Credit for such things goes to him, obviously!
 * I've simply removed the code relating to writing in fbopen and fbclose
 */

/* 
   WARNING:  THIS CODE IS A HACK.  WE CAN NOT AND WILL NOT BE RESPONSIBLE
   FOR ANY NASUEA, DIZZINESS, VOMITING, OR SHORTNESS OF BREATH RESULTING
   FROM READING THIS CODE.  PREGNANT WOMEN AND INDIVIDUALS WITH BACK
   INJURIES, HEART CONDITIONS, OR ARE UNDER THE CARE OF A PHYSICIAN SHOULD
   NOT READ THIS CODE.

   -- The Management
 */

#include "conf.h"
#include "sysdep.h"

#include <signal.h>
#include <sys/stat.h>
#include <stdarg.h>

#include "structs.h"
#include "utils.h"
#include "db.h"
#include "diskio.h"

#define IMM_LMARG "   "
#define IMM_NSIZE  16
#define LINE_LEN   64
#define MIN_LEVEL LVL_IMMORT

/* max level that should be in columns instead of centered */
#define COL_LEVEL LVL_IMMORT

struct name_rec {
  char name[25];
  struct name_rec *next;
};

struct control_rec {
  int level;
  char *level_name;
};

struct level_rec {
  struct control_rec *params;
  struct level_rec *next;
  struct name_rec *names;
};

struct control_rec level_params[] =
{
  {LVL_IMMORT, "Immortals"},
  {LVL_GOD, "Gods"},
  {LVL_GRGOD, "Greater Gods"},
  {LVL_IMPL, "Implementors"},
  {0, ""}
};


struct level_rec *levels = 0;


/* the "touch" command, essentially. */
int touch(const char *path)
{
  FILE *fl;

  if (!(fl = fopen(path, "a"))) {
    printf("SYSERR: %s: %s", path, strerror(errno));
    return (-1);
  } else {
    fclose(fl);
    return (0);
  }
}

char *CAP(char *txt)
{
  *txt = UPPER(*txt);
  return (txt);
}

void initialize(void)
{
  struct level_rec *tmp;
  int i = 0;

  while (level_params[i].level > 0) {
    tmp = (struct level_rec *) malloc(sizeof(struct level_rec));
    tmp->names = 0;
    tmp->params = &(level_params[i++]);
    tmp->next = levels;
    levels = tmp;
  }
}


void read_file(void)
{
  void add_name(byte level, char *name);
  FBFILE *plr_index;
  int rec_count = 0, i, last = 0, level = 0, autowiz = 0;
  char index_name[40], line[256], bits[64];
  char name[MAX_NAME_LENGTH];
  long id = 0;

  sprintf(index_name, "%s", PLR_INDEX_FILE);
  if(!(plr_index = fbopen(index_name, FB_READ))) {
    perror("What the hell?  No playerfile index!");
    exit(1);
  }

  /* count the number of players in the index */
  while (fbgetline(plr_index, line))
    if(*line != '~')
      rec_count++;
  fbrewind(plr_index);

  for(i = 0; i < rec_count; i++) {
    fbgetline(plr_index, line);
    sscanf(line, "%ld %s %d %s %d %d", &id, name, &level, bits, &last, &autowiz);

    CAP(name);

    if (level >= MIN_LEVEL && !autowiz)
      add_name(level, name);
  }

  fbclose(plr_index);
}


void add_name(byte level, char *name)
{
  struct name_rec *tmp;
  struct level_rec *curr_level;
  char *ptr;

  if (!*name)
    return;

  for (ptr = name; *ptr; ptr++)
    if (!isalpha(*ptr))
      return;

  tmp = (struct name_rec *) malloc(sizeof(struct name_rec));
  strcpy(tmp->name, name);
  tmp->next = 0;

  curr_level = levels;
  while (curr_level->params->level > level)
    curr_level = curr_level->next;

  tmp->next = curr_level->names;
  curr_level->names = tmp;
}


void sort_names(void)
{
  struct level_rec *curr_level;
  struct name_rec *a, *b;
  char temp[100];

  for (curr_level = levels; curr_level; curr_level = curr_level->next) {
    for (a = curr_level->names; a && a->next; a = a->next) {
      for (b = a->next; b; b = b->next) {
	if (strcmp(a->name, b->name) > 0) {
	  strcpy(temp, a->name);
	  strcpy(a->name, b->name);
	  strcpy(b->name, temp);
	}
      }
    }
  }
}


void write_wizlist(FILE * out, int minlev, int maxlev)
{
  char buf[100];
  struct level_rec *curr_level;
  struct name_rec *curr_name;
  int i, j;

  fprintf(out,
"*****************************************************************************\n"
"* Le seguenti persone hanno raggiunto l'immortalita' su Maclandia.          *\n"
"* Devono essere trattati con rispetto e deferenza. E' consigliabile         *\n"
"* rivolgergli ogni tanto qualche preghiera. E' sconsigliato irritarle !!    *\n"
"*****************************************************************************\n\n");

  for (curr_level = levels; curr_level; curr_level = curr_level->next) {
    if (curr_level->params->level < minlev ||
	curr_level->params->level > maxlev)
      continue;
    i = 39 - (strlen(curr_level->params->level_name) >> 1);
    for (j = 1; j <= i; j++)
      fputc(' ', out);
    fprintf(out, "%s\n", curr_level->params->level_name);
    for (j = 1; j <= i; j++)
      fputc(' ', out);
    for (j = 1; j <= strlen(curr_level->params->level_name); j++)
      fputc('~', out);
    fprintf(out, "\n");

    strcpy(buf, "");
    curr_name = curr_level->names;
    while (curr_name) {
      strcat(buf, curr_name->name);
      if (strlen(buf) > LINE_LEN) {
	if (curr_level->params->level <= COL_LEVEL)
	  fprintf(out, IMM_LMARG);
	else {
	  i = 40 - (strlen(buf) >> 1);
	  for (j = 1; j <= i; j++)
	    fputc(' ', out);
	}
	fprintf(out, "%s\n", buf);
	strcpy(buf, "");
      } else {
	if (curr_level->params->level <= COL_LEVEL) {
	  for (j = 1; j <= (IMM_NSIZE - strlen(curr_name->name)); j++)
	    strcat(buf, " ");
	}
	if (curr_level->params->level > COL_LEVEL)
	  strcat(buf, "   ");
      }
      curr_name = curr_name->next;
    }

    if (*buf) {
      if (curr_level->params->level <= COL_LEVEL)
	fprintf(out, "%s%s\n", IMM_LMARG, buf);
      else {
	i = 40 - (strlen(buf) >> 1);
	for (j = 1; j <= i; j++)
	  fputc(' ', out);
	fprintf(out, "%s\n", buf);
      }
    }
    fprintf(out, "\n");
  }
}


int main(int argc, char **argv)
{
  int wizlevel, immlevel, pid = 0;
  FILE *fl;

  if (argc != 5 && argc != 6) {
    printf("Format: %s wizlev wizlistfile immlev immlistfile [pid to signal]\n",
	   argv[0]);
    exit(0);
  }
  wizlevel = atoi(argv[1]);
  immlevel = atoi(argv[3]);

  if (argc == 6)
    pid = atoi(argv[5]);

  initialize();
  read_file();
  sort_names();

  fl = fopen(argv[2], "w");
  write_wizlist(fl, wizlevel, LVL_IMPL);
  fclose(fl);

  fl = fopen(argv[4], "w");
  write_wizlist(fl, immlevel, wizlevel - 1);
  fclose(fl);

  if (pid)
    kill(pid, SIGUSR1);

  return (0);
}


int fbgetline(FBFILE *fbfl, char *line)
{
  char *r = fbfl->ptr, *w = line;

  if(!fbfl || !line || !*fbfl->ptr)
    return FALSE;

  for(; *r && *r != '\n' && r <= fbfl->buf + fbfl->size; r++)
    *(w++) = *r;

  while(*r == '\r' || *r == '\n')
    r++;

  *w = '\0';

  if(r > fbfl->buf + fbfl->size)
    return FALSE;
  else {
    fbfl->ptr = r;
    return TRUE;
  }
}


FBFILE *fbopen_for_read(char *fname)
{
  int err;
  FILE *fl;
  struct stat sb;
  FBFILE *fbfl;

  if(!(fbfl = (FBFILE *)malloc(sizeof(FBFILE) + 1)))
    return NULL;

  if(!(fl = fopen(fname, "r"))) {
    free(fbfl);
    return NULL;
  }

  err = fstat(fl->_fileno, &sb);
  if(err < 0 || sb.st_size <= 0) {
    free(fbfl);
    fclose(fl);
    return NULL;
  }

  fbfl->size = sb.st_size;
  if(!(fbfl->buf = malloc(fbfl->size))) {
    free(fbfl);
    return NULL;
  }
  if(!(fbfl->name = malloc(strlen(fname) + 1))) {
    free(fbfl->buf);
    free(fbfl);
    return NULL;
  }
  fbfl->ptr = fbfl->buf;
  fbfl->flags = FB_READ;
  strcpy(fbfl->name, fname);
  fread(fbfl->buf, sizeof(char), fbfl->size, fl);
  fclose(fl);

  return fbfl;
}


FBFILE *fbopen(char *fname, int mode)
{
  if(!fname || !*fname || !mode)
    return NULL;

  if(IS_SET(mode, FB_READ))
    return fbopen_for_read(fname);
  else
    return NULL;
}


int fbclose_for_read(FBFILE *fbfl)
{
  if(!fbfl)
    return 0;

  if(fbfl->buf)
    free(fbfl->buf);
  if(fbfl->name)
    free(fbfl->name);
  free(fbfl);
  return 1;
}


int fbclose(FBFILE *fbfl)
{
  if(!fbfl)
    return 0;

  if(IS_SET(fbfl->flags, FB_READ))
    return fbclose_for_read(fbfl);
  else
    return 0;
}


void fbrewind(FBFILE *fbfl)
{
  fbfl->ptr = fbfl->buf;
}
