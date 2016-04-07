
/* ************************************************************************
*   File: act.informative.c                             Part of CircleMUD *
*  Usage: Player-level commands of an informative nature                  *
*                                                                         *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  Copyright (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */

#include "conf.h"
#include "sysdep.h"

#include "structs.h"
#include "utils.h"
#include "comm.h"
#include "interpreter.h"
#include "handler.h"
#include "db.h"
#include "spells.h"
#include "screen.h"
#include "constants.h"

/* extern variables */
extern int top_of_helpt;
extern struct help_index_element *help_table;
extern char *help;
extern struct time_info_data time_info;
extern struct room_data *world;
extern struct descriptor_data *descriptor_list;
extern struct char_data *character_list;
extern struct obj_data *object_list;

extern char *credits;
extern char *news;
extern char *info;
extern char *motd;
extern char *imotd;
extern char *wizlist;
extern char *immlist;
extern char *policies;
extern char *handbook;
extern char *class_abbrevs[];
extern char *race_abbrevs[];
extern char *types_ammo[][2];

/* extern functions */
ACMD (do_action);
long find_class_bitvector (char arg);
int level_exp (int chclass, int level);
char *title_male (int chclass, int level);
char *title_female (int chclass, int level);
struct time_info_data *real_time_passed (time_t t2, time_t t1);
int compute_armor_class (struct char_data *ch);
void show_astral_body (struct char_data *ch);

/* local functions */
void print_object_location (int num, struct obj_data *obj,
							struct char_data *ch, int recur);
void show_obj_to_char (struct obj_data *object, struct char_data *ch,
					   int mode);
void list_obj_to_char (struct obj_data *list, struct char_data *ch, int mode,
					   int show);
ACMD (do_look);
ACMD (do_examine);
ACMD (do_gold);
ACMD (do_score);
ACMD (do_inventory);
ACMD (do_equipment);
ACMD (do_time);
ACMD (do_weather);
ACMD (do_sky);
ACMD (do_help);
ACMD (do_who);
ACMD (do_users);
ACMD (do_gen_ps);
void perform_mortal_where (struct char_data *ch, char *arg);
void perform_immort_where (struct char_data *ch, char *arg);
ACMD (do_where);
ACMD (do_levels);
ACMD (do_consider);
ACMD (do_diagnose);
ACMD (do_color);
ACMD (do_toggle);
ACMD (do_spy);
void sort_commands (void);
ACMD (do_commands);
void diag_char_to_char (struct char_data *i, struct char_data *ch);
void look_at_char (struct char_data *i, struct char_data *ch);
void list_one_char (struct char_data *i, struct char_data *ch);
void list_char_to_char (struct char_data *list, struct char_data *ch);
void do_auto_exits (struct char_data *ch);
ACMD (do_exits);
void look_in_direction (struct char_data *ch, int dir);
void look_in_obj (struct char_data *ch, char *arg);
char *find_exdesc (char *word, struct extra_descr_data *list);
void look_at_target (struct char_data *ch, char *arg);

ACMD (do_group_who);


/*
 * This function screams bitvector... -gg 6/45/98
 *
 * A lot of "grep" later:
 * Mode 0: Object description, and auras.
 * Mode 1: Object short description and auras.
 * Mode 2: Object short description and auras.
 * Mode 3: Object short description.        (Unused)
 * Mode 4: Object short description and auras.  (Unused)
 * Mode 5: Notes, drink containers, fountains.
 * Mode 6: Auras.
 */
void
show_obj_to_char (struct obj_data *object, struct char_data *ch, int mode)
{
	*buf = '\0';
	if ((mode == 0) && object->description)
		strcpy (buf, object->description);
	else if (object->short_description && ((mode == 1) ||
										   (mode == 2) || (mode == 3)
										   || (mode == 4)))
		strcpy (buf, object->short_description);
	else if (mode == 5)
	{
		if (GET_OBJ_TYPE (object) == ITEM_NOTE)
		{
			if (object->action_description)
			{
				strcpy (buf, "C'e' scritto qualcosa:\r\n\r\n");
				strcat (buf, object->action_description);
				page_string (ch->desc, buf, 1);
			}
			else
				send_to_char ("Non c'e' nulla.\r\n", ch);
			return;
		}
		else if (GET_OBJ_TYPE (object) != ITEM_DRINKCON)
		{
			strcpy (buf, "Non vedi niente di speciale..");
		}
		else					/* ITEM_TYPE == ITEM_DRINKCON||FOUNTAIN */
			strcpy (buf, "Sembra un contenitore di liquidi.");
	}
	if (mode != 3)
	{
		if (IS_OBJ_STAT (object, ITEM_INVISIBLE))
			strcat (buf, " (invisible)");
		if (IS_OBJ_STAT (object, ITEM_BLESS)
			&& AFF_FLAGGED (ch, AFF_DETECT_ALIGN))
			strcat (buf, " ..Brilla di una aura blu!");
		if (IS_OBJ_STAT (object, ITEM_MAGIC)
			&& AFF_FLAGGED (ch, AFF_DETECT_MAGIC))
			strcat (buf, " ..Brilla di una aura gialla!");
		if (IS_OBJ_STAT (object, ITEM_GLOW))
			strcat (buf, " ..Ha una soffusa aura luminosa!");
		if (IS_OBJ_STAT (object, ITEM_HUM))
			strcat (buf, " ..Emette un lieve ronzio!");
	}
	strcat (buf, "\r\n");
	page_string (ch->desc, buf, TRUE);
}


void
list_obj_to_char (struct obj_data *list, struct char_data *ch, int mode,
				  int show)
{
	struct obj_data *i;
	bool found = FALSE;

	for (i = list; i; i = i->next_content)
	{
		if (CAN_SEE_OBJ (ch, i))
		{
			show_obj_to_char (i, ch, mode);
			found = TRUE;
		}
	}
	if (!found && show)
		send_to_char (" Nulla.\r\n", ch);
}


void
diag_char_to_char (struct char_data *i, struct char_data *ch)
{
	int percent;

	if (GET_MAX_HIT (i) > 0)
		percent = (100 * GET_HIT (i)) / GET_MAX_HIT (i);
	else
		percent = -1;			/* How could MAX_HIT be < 1?? */

	strcpy (buf, PERS (i, ch));
	CAP (buf);

	if (percent >= 100)
		strcat (buf, " e' in condizioni eccellenti.\r\n");
	else if (percent >= 90)
		strcat (buf, " ha qualche graffio.\r\n");
	else if (percent >= 75)
		strcat (buf, " ha qualche piccola ferita e ammaccatura.\r\n");
	else if (percent >= 50)
		strcat (buf, " ha delle ferite.\r\n");
	else if (percent >= 30)
		strcat (buf, " ha delle brutte ferite e graffi.\r\n");
	else if (percent >= 15)
		strcat (buf, " ha un brutto aspetto.\r\n");
	else if (percent >= 0)
		strcat (buf, " e' in condizioni pietose.\r\n");
	else
		strcat (buf, " sta sanguinando copiosamente da grossi squarci.\r\n");

	send_to_char (buf, ch);
}


void
look_at_char (struct char_data *i, struct char_data *ch)
{
	int j, found;
	struct obj_data *tmp_obj;

	if (!ch->desc)
		return;

	if (i->player.description)
		send_to_char (i->player.description, ch);
	else
		act ("Non vedi niente di speciale su $m.", FALSE, i, 0, ch, TO_VICT);

	diag_char_to_char (i, ch);

	found = FALSE;
	for (j = 0; !found && j < NUM_WEARS; j++)
		if (GET_EQ (i, j) && CAN_SEE_OBJ (ch, GET_EQ (i, j)))
			found = TRUE;

	if (found)
	{
		send_to_char ("\r\n", ch);	/* act() does capitalization. */
		act ("$n sta usando:", FALSE, i, 0, ch, TO_VICT);
		for (j = 0; j < NUM_WEARS; j++)
			if (GET_EQ (i, j) && CAN_SEE_OBJ (ch, GET_EQ (i, j)))
			{
				send_to_char (where[j], ch);
				show_obj_to_char (GET_EQ (i, j), ch, 1);
			}
	}
	if (ch != i && (IS_THIEF (ch) || GET_LEVEL (ch) >= LVL_IMMORT))
	{
		found = FALSE;
		act ("\r\nCerchi di scrutare nel suo inventario:", FALSE, i, 0, ch,
			 TO_VICT);
		for (tmp_obj = i->carrying; tmp_obj; tmp_obj = tmp_obj->next_content)
		{
			if (CAN_SEE_OBJ (ch, tmp_obj)
				&& (number (0, 20) < GET_LEVEL (ch)))
			{
				show_obj_to_char (tmp_obj, ch, 1);
				found = TRUE;
			}
		}

		if (!found)
			send_to_char ("Non ha nulla.\r\n", ch);
	}
}


void
list_one_char (struct char_data *i, struct char_data *ch)
{
	const char *positions[] = {
		" giace qui, morto.",
		" giace qui, ferito mortalmente.",
		" giace qui, svenuto.",
		" giace qui, stordito.",
		" sta dormendo qui.",
		" sta riposando qui.",
		" sta seduto qui.",
		"!FIGHTING!",
		" e' qui in piedi.",
		" vola qui intorno."
	};


	if (IS_NPC (i) && i->player.long_descr
		&& GET_POS (i) == GET_DEFAULT_POS (i))
	{
		if (AFF_FLAGGED (i, AFF_INVISIBLE))
		{
			strcpy (buf, "*");
		}
		else
		{
			*buf = '\0';
		}

		if (AFF_FLAGGED (ch, AFF_DETECT_ALIGN))
		{
			if (IS_EVIL (i))
			{
				strcat (buf, "(Aura Rossa) ");
			}
			else if (IS_GOOD (i))
			{
				strcat (buf, "(Aura Azzurra) ");
			}
		}
		strcat (buf, i->player.long_descr);
		send_to_char (buf, ch);

		if (AFF_FLAGGED (i, AFF_SANCTUARY))
		{
			act ("...Risplende di una aura luminosa!", FALSE, i, 0, ch,
				 TO_VICT);
		}
		if (AFF_FLAGGED (i, AFF_BLIND))
		{
			act ("...Sta brancolando intorno, accecato!", FALSE, i, 0, ch,
				 TO_VICT);
		}

		return;
	}

	if (IS_NPC (i))
	{
		strcpy (buf, i->player.short_descr);
		CAP (buf);
	}
	else
	{
		sprintf (buf, "%s %s", i->player.name, GET_TITLE (i));
	}
	if (AFF_FLAGGED (i, AFF_INVISIBLE))
	{
		strcat (buf, " (invisibile)");
	}
	if (AFF_FLAGGED (i, AFF_HIDE))
	{
		strcat (buf, " (nascosto)");
	}
	if (!IS_NPC (i) && !i->desc)
	{
		strcat (buf, " (linkless)");
	}
	if (!IS_NPC (i) && PLR_FLAGGED (i, PLR_WRITING))
	{
		strcat (buf, " (sta scrivendo)");
	}
	if (GET_POS (i) != POS_FIGHTING)
	{
		strcat (buf, positions[(int) GET_POS (i)]);
	}
	else
	{
		if (FIGHTING (i))
		{
			strcat (buf, " sta combattendo ");
			if (FIGHTING (i) == ch)
			{
				strcat (buf, "con TE!");
			}
			else
			{
				if (i->in_room == FIGHTING (i)->in_room)
				{
					strcat (buf, PERS (FIGHTING (i), ch));
				}
				else
				{
					strcat (buf, "qualcuno che e' appena uscito");
				}

				strcat (buf, "!");
			}
		}
		else					/* NIL fighting pointer */
		{
			strcat (buf, " sta lottando con l'aria.");
		}
	}

	if (AFF_FLAGGED (ch, AFF_DETECT_ALIGN))
	{
		if (IS_EVIL (i))
			strcat (buf, " (Aura Rossa)");
		else if (IS_GOOD (i))
			strcat (buf, " (Aura Azzurra)");
	}
	strcat (buf, "\r\n");
	send_to_char (buf, ch);

	if (AFF_FLAGGED (i, AFF_SANCTUARY))
	{
		act ("...Risplende di una aura luminosa!", FALSE, i, 0, ch, TO_VICT);
	}

}


void
list_char_to_char (struct char_data *list, struct char_data *ch)
{
	struct char_data *i;

	for (i = list; i; i = i->next_in_room)
		if (ch != i)
		{
			if (CAN_SEE (ch, i))
				list_one_char (i, ch);
			else if (IS_DARK (ch->in_room) && !CAN_SEE_IN_DARK (ch) &&
					 AFF_FLAGGED (i, AFF_INFRAVISION))
				send_to_char
					("Vedi un paio di occhi rossi brillare lungo la via.\r\n",
					 ch);
		}
}


void
do_auto_exits (struct char_data *ch)
{
	int door, slen = 0;

	*buf = '\0';

	for (door = 0; door < NUM_OF_DIRS; door++)
		if (EXIT (ch, door) && EXIT (ch, door)->to_room != NOWHERE &&
			!EXIT_FLAGGED (EXIT (ch, door), EX_CLOSED))
			slen += sprintf (buf + slen, "%c ", LOWER (*dirs[door]));

	sprintf (buf2, "%s[ Exits: %s]%s\r\n", CCCYN (ch, C_NRM),
			 *buf ? buf : "Nessuna! ", CCNRM (ch, C_NRM));

	send_to_char (buf2, ch);
}


ACMD (do_exits)
{
	int door;

	*buf = '\0';

	if (AFF_FLAGGED (ch, AFF_BLIND))
	{
		send_to_char ("Non vedi nulla, sei cieco!\r\n", ch);
		return;
	}
	for (door = 0; door < NUM_OF_DIRS; door++)
		if (EXIT (ch, door) && EXIT (ch, door)->to_room != NOWHERE &&
			!EXIT_FLAGGED (EXIT (ch, door), EX_CLOSED))
		{
			if (GET_LEVEL (ch) >= LVL_IMMORT)
				sprintf (buf2, "%-5s - [%5d] %s\r\n", dirs[door],
						 GET_ROOM_VNUM (EXIT (ch, door)->to_room),
						 world[EXIT (ch, door)->to_room].name);
			else
			{
				sprintf (buf2, "%-5s - ", dirs[door]);
				if (IS_DARK (EXIT (ch, door)->to_room)
					&& !CAN_SEE_IN_DARK (ch))
					strcat (buf2, "Troppo buio da vedere\r\n");
				else
				{
					strcat (buf2, world[EXIT (ch, door)->to_room].name);
					strcat (buf2, "\r\n");
				}
			}
			strcat (buf, CAP (buf2));
		}
	send_to_char ("Uscite visibili:\r\n", ch);

	if (*buf)
		send_to_char (buf, ch);
	else
		send_to_char (" Nessuna.\r\n", ch);
}



void
look_at_room (struct char_data *ch, int ignore_brief)
{
	if (!ch->desc)
		return;

	if (IS_DARK (ch->in_room) && !CAN_SEE_IN_DARK (ch))
	{
		send_to_char ("E' molto buio qui...\r\n", ch);
		return;
	}
	else if (AFF_FLAGGED (ch, AFF_BLIND))
	{
		send_to_char ("Vedi solo un'oscurita' infinita...\r\n", ch);
		return;
	}
	send_to_char (CCCYN (ch, C_NRM), ch);
	if (!IS_NPC (ch) && PRF_FLAGGED (ch, PRF_ROOMFLAGS))
	{
		sprintbit (ROOM_FLAGS (ch->in_room), room_bits, buf);
		sprintf (buf2, "[%5d] %s [ %s]", GET_ROOM_VNUM (IN_ROOM (ch)),
				 world[ch->in_room].name, buf);
		send_to_char (buf2, ch);
	}
	else
		send_to_char (world[ch->in_room].name, ch);

	send_to_char (CCNRM (ch, C_NRM), ch);
	send_to_char ("\r\n", ch);

	if ((!IS_NPC (ch) && !PRF_FLAGGED (ch, PRF_BRIEF)) || ignore_brief ||
		ROOM_FLAGGED (ch->in_room, ROOM_DEATH))
		send_to_char (world[ch->in_room].description, ch);

	/* autoexits */
	if (!IS_NPC (ch) && PRF_FLAGGED (ch, PRF_AUTOEXIT))
		do_auto_exits (ch);

	/* now list characters & objects */
	send_to_char (CCGRN (ch, C_NRM), ch);
	list_obj_to_char (world[ch->in_room].contents, ch, 0, FALSE);
	send_to_char (CCYEL (ch, C_NRM), ch);
	list_char_to_char (world[ch->in_room].people, ch);
	send_to_char (CCNRM (ch, C_NRM), ch);
}



void
look_in_direction (struct char_data *ch, int dir)
{
	if (EXIT (ch, dir))
	{
		if (EXIT (ch, dir)->general_description)
			send_to_char (EXIT (ch, dir)->general_description, ch);
		else
			send_to_char ("Non vedi niente di speciale.\r\n", ch);

		if (EXIT_FLAGGED (EXIT (ch, dir), EX_CLOSED)
			&& EXIT (ch, dir)->keyword)
		{
			sprintf (buf, "La %s e' chiusa.\r\n",
					 fname (EXIT (ch, dir)->keyword));
			send_to_char (buf, ch);
		}
		else if (EXIT_FLAGGED (EXIT (ch, dir), EX_ISDOOR)
				 && EXIT (ch, dir)->keyword)
		{
			sprintf (buf, "La %s e' aperta.\r\n",
					 fname (EXIT (ch, dir)->keyword));
			send_to_char (buf, ch);
		}
	}
	else
		send_to_char ("Niente di speciale qui...\r\n", ch);
}



void
look_in_obj (struct char_data *ch, char *arg)
{
	struct obj_data *obj = NULL;
	struct char_data *dummy = NULL;
	int amt, bits;

	if (!*arg)
		send_to_char ("Guardare in cosa?\r\n", ch);
	else if (!(bits = generic_find (arg, FIND_OBJ_INV | FIND_OBJ_ROOM |
									FIND_OBJ_EQUIP, ch, &dummy, &obj)))
	{
		sprintf (buf, "Non vedo come fare %s %s qui.\r\n", AN (arg), arg);
		send_to_char (buf, ch);
	}
	else if ((GET_OBJ_TYPE (obj) != ITEM_DRINKCON) &&
			 (GET_OBJ_TYPE (obj) != ITEM_FOUNTAIN) &&
			 (GET_OBJ_TYPE (obj) != ITEM_CONTAINER))
	{
		if ((GET_OBJ_TYPE (obj) == ITEM_FIREWEAPON) ||
			(GET_OBJ_TYPE (obj) == ITEM_MISSILE))
		{
			if (GET_OBJ_VAL (obj, 3) == 1)
			{
				if ((GET_OBJ_TYPE (obj) == ITEM_MISSILE))
				{
					sprintf (buf, "E' solo una semplice %s!\r\n",
							 types_ammo[GET_OBJ_VAL (obj, 0)][0]);
				}
				else
				{
					sprintf (buf, "Contiene %d %s di munizioni.\r\n",
							 GET_OBJ_VAL (obj, 3),
							 types_ammo[GET_OBJ_VAL (obj, 0)][0]);
				}
			}
			else
			{
				sprintf (buf, "Contiene %d %s di munizioni.\r\n",
						 GET_OBJ_VAL (obj, 3),
						 types_ammo[GET_OBJ_VAL (obj, 0)][1]);
			}
			send_to_char (buf, ch);
		}
		else
		{
			send_to_char ("Non c'e' niente dentro!\r\n", ch);
		}
	}
	else
	{
		if (GET_OBJ_TYPE (obj) == ITEM_CONTAINER)
		{
			if (OBJVAL_FLAGGED (obj, CONT_CLOSED))
				send_to_char ("E' chiuso.\r\n", ch);
			else
			{
				send_to_char (fname (obj->name), ch);
				switch (bits)
				{
				case FIND_OBJ_INV:
					send_to_char (" (contiene): \r\n", ch);
					break;
				case FIND_OBJ_ROOM:
					send_to_char (" (qui): \r\n", ch);
					break;
				case FIND_OBJ_EQUIP:
					send_to_char (" (usato): \r\n", ch);
					break;
				}

				list_obj_to_char (obj->contains, ch, 2, TRUE);
			}
		}
		else
		{						/* item must be a fountain or drink container */
			if (GET_OBJ_VAL (obj, 1) <= 0)
				send_to_char ("E' vuoto.\r\n", ch);
			else
			{
				if (GET_OBJ_VAL (obj, 0) <= 0
					|| GET_OBJ_VAL (obj, 1) > GET_OBJ_VAL (obj, 0))
				{
					sprintf (buf, "Sembra contenere fango.\r\n");	/* BUG */
				}
				else
				{
					amt = (GET_OBJ_VAL (obj, 1) * 3) / GET_OBJ_VAL (obj, 0);
					sprinttype (GET_OBJ_VAL (obj, 2), color_liquid, buf2);
					sprintf (buf, "E' %s e contiene un liquido %s .\r\n",
							 fullness[amt], buf2);
				}
				send_to_char (buf, ch);
			}
		}
	}
}



char *
find_exdesc (char *word, struct extra_descr_data *list)
{
	struct extra_descr_data *i;

	for (i = list; i; i = i->next)
		if (isname (word, i->keyword))
			return (i->description);

	return (NULL);
}


/*
 * Given the argument "look at <target>", figure out what object or char
 * matches the target.  First, see if there is another char in the room
 * with the name.  Then check local objs for exdescs.
 *
 * Thanks to Angus Mezick <angus@EDGIL.CCMAIL.COMPUSERVE.COM> for the
 * suggested fix to this problem.
 */
void
look_at_target (struct char_data *ch, char *arg)
{
	int bits, found = FALSE, j, fnum, i = 0;
	struct char_data *found_char = NULL;
	struct obj_data *obj, *found_obj = NULL;
	char *desc;

	if (!ch->desc)
		return;

	if (!*arg)
	{
		send_to_char ("Guardare a chi?\r\n", ch);
		return;
	}

	bits = generic_find (arg, FIND_OBJ_INV | FIND_OBJ_ROOM | FIND_OBJ_EQUIP |
						 FIND_CHAR_ROOM, ch, &found_char, &found_obj);

	/* Is the target a character? */
	if (found_char != NULL)
	{
		look_at_char (found_char, ch);
		if (ch != found_char)
		{
			if (CAN_SEE (found_char, ch))
				act ("$n ti osserva.", TRUE, ch, 0, found_char, TO_VICT);
			act ("$n guarda $N.", TRUE, ch, 0, found_char, TO_NOTVICT);
		}
		return;
	}

	/* Strip off "number." from 2.foo and friends. */
	if (!(fnum = get_number (&arg)))
	{
		send_to_char ("Guardare a cosa?\r\n", ch);
		return;
	}

	/* Does the argument match an extra desc in the room? */
	if ((desc = find_exdesc (arg, world[ch->in_room].ex_description)) != NULL
		&& ++i == fnum)
	{
		page_string (ch->desc, desc, FALSE);
		return;
	}

	/* Does the argument match an extra desc in the char's equipment? */
	for (j = 0; j < NUM_WEARS && !found; j++)
		if (GET_EQ (ch, j) && CAN_SEE_OBJ (ch, GET_EQ (ch, j)))
			if ((desc = find_exdesc (arg, GET_EQ (ch, j)->ex_description)) !=
				NULL && ++i == fnum)
			{
				send_to_char (desc, ch);
				found = TRUE;
			}

	/* Does the argument match an extra desc in the char's inventory? */
	for (obj = ch->carrying; obj && !found; obj = obj->next_content)
	{
		if (CAN_SEE_OBJ (ch, obj))
			if ((desc = find_exdesc (arg, obj->ex_description)) != NULL
				&& ++i == fnum)
			{
				send_to_char (desc, ch);
				found = TRUE;
			}
	}

	/* Does the argument match an extra desc of an object in the room? */
	for (obj = world[ch->in_room].contents; obj && !found;
		 obj = obj->next_content)
		if (CAN_SEE_OBJ (ch, obj))
			if ((desc = find_exdesc (arg, obj->ex_description)) != NULL
				&& ++i == fnum)
			{
				send_to_char (desc, ch);
				found = TRUE;
			}

	/* If an object was found back in generic_find */
	if (bits)
	{
		if (!found)
			show_obj_to_char (found_obj, ch, 5);	/* Show no-description */
		else
			show_obj_to_char (found_obj, ch, 6);	/* Find hum, glow etc */
	}
	else if (!found)
		send_to_char ("Non lo vedi qui intorno.\r\n", ch);
}


ACMD (do_look)
{
	char arg2[MAX_INPUT_LENGTH];
	int look_type;

	if (!ch->desc)
		return;

	if (GET_POS (ch) < POS_SLEEPING)
		send_to_char
			("Tutto quello che puoi fare ora e' vedere le stelle!\r\n", ch);
	else if (AFF_FLAGGED (ch, AFF_BLIND))
		send_to_char ("Nono puoi vedere nulla, SEI CIECO!!!\r\n", ch);
	else if (IS_DARK (ch->in_room) && !CAN_SEE_IN_DARK (ch))
	{
		send_to_char ("E' molto buio qui...\r\n", ch);
		list_char_to_char (world[ch->in_room].people, ch);	/* glowing red eyes */
	}
	else
	{
		half_chop (argument, arg, arg2);

		if (subcmd == SCMD_READ)
		{
			if (!*arg)
				send_to_char ("Leggere COSA?\r\n", ch);
			else
				look_at_target (ch, arg);
			return;
		}
		if (!*arg)				/* "look" alone, without an argument at all */
			look_at_room (ch, 1);
		else if (is_abbrev (arg, "in"))
			look_in_obj (ch, arg2);
		/* did the char type 'look <direction>?' */
		else if ((look_type = search_block (arg, dirs, FALSE)) >= 0)
			look_in_direction (ch, look_type);
		else if (is_abbrev (arg, "at"))
			look_at_target (ch, arg2);
		else
			look_at_target (ch, arg);
	}
}



ACMD (do_examine)
{
	struct char_data *tmp_char;
	struct obj_data *tmp_object;

	one_argument (argument, arg);

	if (!*arg)
	{
		send_to_char ("Esaminare COSA?\r\n", ch);
		return;
	}
	look_at_target (ch, arg);

	generic_find (arg, FIND_OBJ_INV | FIND_OBJ_ROOM | FIND_CHAR_ROOM |
				  FIND_OBJ_EQUIP, ch, &tmp_char, &tmp_object);

	if (tmp_object)
	{
		if ((GET_OBJ_TYPE (tmp_object) == ITEM_DRINKCON) ||
			(GET_OBJ_TYPE (tmp_object) == ITEM_FOUNTAIN) ||
			(GET_OBJ_TYPE (tmp_object) == ITEM_CONTAINER))
		{
			send_to_char ("Mentre guardi dentro, vedi:\r\n", ch);
			look_in_obj (ch, argument);
		}
	}
}



ACMD (do_gold)
{
	if (GET_GOLD (ch) == 0)
	{
		send_to_char ("Sei povero in canna!\r\n", ch);
	}
	else if (GET_GOLD (ch) == 1)
	{
		send_to_char ("Hai una miserabile moneta d'oro.\r\n", ch);
	}
	else
	{
		sprintf (buf, "Hai %d monete d'oro.\r\n", GET_GOLD (ch));
		send_to_char (buf, ch);
	}
}


ACMD (do_score)
{
	struct time_info_data playing_time;
	char classe[20];
	char razza[20];

	if (IS_NPC (ch))
	{
		return;
	}

	strcpy (classe, "Virus Writer");
	strcpy (razza, (GET_SEX (ch) == SEX_FEMALE ? "Umana" : "Umano"));

	if (GET_RACE (ch) == 1)
	{
		strcpy (razza, "Cyborg");
	}
	if (GET_RACE (ch) == 2)
	{
		strcpy (razza, "Motociclista");
	}
	if (GET_RACE (ch) == 3)
	{
		strcpy (razza, (GET_SEX (ch) == SEX_FEMALE ? "Aliena" : "Alieno"));
	}

	if (GET_CLASS (ch) == 1)
	{
		strcpy (classe,
				(GET_SEX (ch) ==
				 SEX_FEMALE ? "Amministratirce di rete" :
				 "Amministratore di rete"));
	}
	if (GET_CLASS (ch) == 2)
	{
		strcpy (classe, "Hacker");
	}
	if (GET_CLASS (ch) == 3)
	{
		strcpy (classe,
				(GET_SEX (ch) ==
				 SEX_FEMALE ? "Guerrafondaia" : "Guerrafondaio"));
	}
	if (GET_CLASS (ch) == 4)
	{
		strcpy (classe, "Martial Artist");
	}
	if (GET_CLASS (ch) == 5)
	{
		strcpy (classe, "Linker");
	}
	if (GET_CLASS (ch) == 6)
	{
		strcpy (classe,
				(GET_SEX (ch) == SEX_FEMALE ? "Psionica" : "Psionico"));
	}

	sprintf (buf, "Hai %d anni.", GET_AGE (ch));

	if (age (ch)->month == 0 && age (ch)->day == 0)
	{
		strcat (buf, "  Oggi e' il tuo compleanno.\r\n");
	}
	else
	{
		strcat (buf, "\r\n");
	}

	sprintf (buf + strlen (buf), "Sei un%s %s e sei un%s %s.\r\n",
			 (GET_SEX (ch) == SEX_FEMALE ? "a" : ""), razza,
			 (GET_SEX (ch) == SEX_FEMALE ? "a" : ""), classe);

	sprintf (buf + strlen (buf),
			 "Hai %d(%d) Hp, %d(%d) RAM  e %d(%d) punti movimento.\r\n",
			 GET_HIT (ch), GET_MAX_HIT (ch), GET_MANA (ch), GET_MAX_MANA (ch),
			 GET_MOVE (ch), GET_MAX_MOVE (ch));

	sprintf (buf + strlen (buf),
			 "La tua classe di armatura e' %d/10, e il tuo allineamento e' %d.\r\n",
			 compute_armor_class (ch), GET_ALIGNMENT (ch));

	sprintf (buf + strlen (buf),
			 "Hai raccolto %d punti exp, e hai %d monete d'oro.\r\n",
			 GET_EXP (ch), GET_GOLD (ch));

	if (GET_LEVEL (ch) < LVL_IMMORT)
		sprintf (buf + strlen (buf),
				 "Devi fare %d punti exp per raggiungere il prossimo livello.\r\n",
				 level_exp (GET_CLASS (ch),
							GET_LEVEL (ch) + 1) - GET_EXP (ch));

	playing_time = *real_time_passed ((time (0) - ch->player.time.logon) +
									  ch->player.time.played, 0);
	sprintf (buf + strlen (buf),
			 "Hai giocato per %d giorni%s e %d ore%s.\r\n", playing_time.day,
			 playing_time.day == 1 ? "" : "", playing_time.hours,
			 playing_time.hours == 1 ? "" : "");

	sprintf (buf + strlen (buf),
			 "Questo ti qualifica come %s %s (livello %d).\r\n",
			 GET_NAME (ch), GET_TITLE (ch), GET_LEVEL (ch));

/* aggiunto da meo per settare la pos di fly se l'aff_bitvector e' attivo */

	if (AFF_FLAGGED (ch, AFF_FLY) && GET_POS (ch) == POS_STANDING)
	{
		GET_POS (ch) = POS_FLYING;
	}

	switch (GET_POS (ch))
	{
	case POS_DEAD:
		strcat (buf, "Sei morto!\r\n");
		break;
	case POS_MORTALLYW:
		strcat (buf, "Sei ferito mortalmente!  Hai bisogno di aiuto!\r\n");
		break;
	case POS_INCAP:
		strcat (buf, "Sei incapacitato, stai per morire\r\n");
		break;
	case POS_STUNNED:
		strcat (buf, "Sei sordito!  Non puoi muoverti!\r\n");
		break;
	case POS_SLEEPING:
		strcat (buf, "Stai dormendo.\r\n");
		break;
	case POS_RESTING:
		strcat (buf, "Stai riposando.\r\n");
		break;
	case POS_SITTING:
		strcat (buf, "Sei seduto.\r\n");
		break;
	case POS_FIGHTING:
		if (FIGHTING (ch))
		{
			sprintf (buf + strlen (buf), "Stai combattendo %s.\r\n",
					 PERS (FIGHTING (ch), ch));
		}
		else
		{
			strcat (buf, "Stai combattento con l'aria.\r\n");
		}
		break;
	case POS_STANDING:
		strcat (buf, "Sei in piedi.\r\n");
		break;
	case POS_FLYING:
		strcat (buf, "Stai volando.\r\n");
		break;
	default:
		strcat (buf, "Stai galleggiando.\r\n");
		break;
	}

	if (GET_COND (ch, DRUNK) > 10)
	{
		strcat (buf, "Sei ubriaco.\r\n");
	}

	if (GET_COND (ch, FULL) == 0)
	{
		strcat (buf, "Sei affamato.\r\n");
	}

	if (GET_COND (ch, THIRST) == 0)
	{
		strcat (buf, "Sei assetato.\r\n");
	}

	if (AFF_FLAGGED (ch, AFF_BLIND))
	{
		strcat (buf, "Sei stato accecato!\r\n");
	}

	if (AFF_FLAGGED (ch, AFF_INVISIBLE))
	{
		strcat (buf, "Sei invisibile.\r\n");
	}

	if (AFF_FLAGGED (ch, AFF_DETECT_MAGIC))
	{
		strcat (buf, "Percepisci la presenza di magia.\r\n");
	}

	if (AFF_FLAGGED (ch, AFF_DETECT_ALIGN))
	{
		strcat (buf, "Percepisci le aure.\r\n");
	}

	if (AFF_FLAGGED (ch, AFF_DETECT_INVIS))
	{
		strcat (buf, "Percepisci la presenza di cose invisibili.\r\n");
	}

	if (AFF_FLAGGED (ch, AFF_SANCTUARY))
	{
		strcat (buf, "Sei protetto da Sanctuary.\r\n");
	}

	if (AFF_FLAGGED (ch, AFF_POISON))
	{
		strcat (buf, "Sei avvelenato!\r\n");
	}

	if (AFF_FLAGGED (ch, AFF_CHARM))
	{
		strcat (buf, "Sei stato incantato!\r\n");
	}

	if (affected_by_spell (ch, SPELL_ARMOR))
	{
		strcat (buf, "Ti senti protetto.\r\n");
	}

	if (AFF_FLAGGED (ch, AFF_INFRAVISION))
	{
		strcat (buf, "I tuoi occhi brillano di rosso.\r\n");
	}

	if (PRF_FLAGGED (ch, PRF_SUMMONABLE))
	{
		strcat (buf, "Sei evocabile da altri giocatori.\r\n");
	}

	send_to_char (buf, ch);

}


ACMD (do_inventory)
{
	send_to_char ("Stai trasportando:\r\n", ch);
	list_obj_to_char (ch->carrying, ch, 1, TRUE);
}


ACMD (do_equipment)
{
	int i, found = 0;

	send_to_char ("Stai indossando:\r\n", ch);
	for (i = 0; i < NUM_WEARS; i++)
	{
		if (GET_EQ (ch, i))
		{
			if (CAN_SEE_OBJ (ch, GET_EQ (ch, i)))
			{
				send_to_char (where[i], ch);
				show_obj_to_char (GET_EQ (ch, i), ch, 1);
				found = TRUE;
			}
			else
			{
				send_to_char (where[i], ch);
				send_to_char ("Qualcosa.\r\n", ch);
				found = TRUE;
			}
		}
	}
	if (!found)
	{
		send_to_char (" Nulla.\r\n", ch);
	}
}


ACMD (do_time)
{
	int weekday, day;

	switch (time_info.hours)
	{
	case 0:
		sprintf (buf, "E' mezzanotte, del ");
		break;
	case 1:
	case 2:
	case 3:
	case 4:
	case 5:
		sprintf (buf, "Sono le %d di notte, del ", time_info.hours);
		break;
	case 6:
	case 7:
	case 8:
	case 9:
	case 10:
	case 11:
		sprintf (buf, "Sono le %d del mattino, del ", time_info.hours);
		break;
	case 12:
		sprintf (buf, "E' mezzogiorno, del ");
		break;
	case 13:
	case 14:
	case 15:
	case 16:
	case 17:
		sprintf (buf, "Sono le %d del pomeriggio, del ", time_info.hours);
		break;
	case 18:
	case 19:
	case 20:
	case 21:
	case 22:
	case 23:
		sprintf (buf, "Sono le %d di sera, del ", time_info.hours);
		break;
	default:
		sprintf (buf, "Sono le %d, del ",
				 ((time_info.hours == 0) ? 24 : time_info.hours));
		break;
	}

	/* 35 days in a month */
	weekday = ((35 * time_info.month) + time_info.day + 1) % 7;

	strcat (buf, weekdays[weekday]);
	strcat (buf, ",\r\n");
	send_to_char (buf, ch);

	day = time_info.day + 1;	/* day in [1..35] */

	sprintf (buf, "Il %d^ giorno del %s, Anno %d.\r\n",
			 day, month_name[(int) time_info.month], time_info.year);

	send_to_char (buf, ch);
}


ACMD (do_weather)
{
	const char *sky_look[] = {
		"sereno",
		"nuvoloso",
		"piovoso",
		"illuminato da lampi"
	};

	if (OUTSIDE (ch))
	{
		sprintf (buf, "Il cielo e' %s e %s.\r\n", sky_look[weather_info.sky],
				 (weather_info.change >= 0 ? "senti un vento caldo dal sud" :
				  "senti un vento freddo dal nord"));
		send_to_char (buf, ch);
	}
	else
	{
		send_to_char ("Non hai la sensazione del tempo.\r\n", ch);
	}
}

ACMD (do_sky)
{
	if (!OUTSIDE (ch))
	{
		send_to_char ("Non e' meglio uscire se vuoi vedere il cielo?\r\n",
					  ch);
		return;
	}

	send_to_char ("Scruti il cielo...\r\n", ch);
	act ("$n alza la testa e scruta il cielo...", TRUE, ch, 0, 0, TO_NOTVICT);

	show_astral_body (ch);

}



ACMD (do_help)
{
	int chk, bot, top, mid, minlen;

	if (!ch->desc)
		return;

	skip_spaces (&argument);

	if (!*argument)
	{
		page_string (ch->desc, help, 0);
		return;
	}
	if (!help_table)
	{
		send_to_char ("Non c'e' aiuto su questo.\r\n", ch);
		return;
	}

	bot = 0;
	top = top_of_helpt;
	minlen = strlen (argument);

	for (;;)
	{
		mid = (bot + top) / 2;

		if (bot > top)
		{
			send_to_char ("Non c'e' aiuto per quella parola.\r\n", ch);
			return;
		}
		else
			if (!(chk = strn_cmp (argument, help_table[mid].keyword, minlen)))
		{
			/* trace backwards to find first matching entry. Thanks Jeff Fink! */
			while ((mid > 0) &&
				   (!(chk
					  =
					  strn_cmp (argument, help_table[mid - 1].keyword,
								minlen))))
				mid--;
			page_string (ch->desc, help_table[mid].entry, 0);
			return;
		}
		else
		{
			if (chk > 0)
				bot = mid + 1;
			else
				top = mid - 1;
		}
	}
}



#define WHO_FORMAT \
"format: who [minlev[-maxlev]] [-n name] [-c classlist] [-s] [-o] [-q] [-r] [-z]\r\n"

ACMD (do_who)
{
	struct descriptor_data *d;
	struct char_data *tch;
	char name_search[MAX_INPUT_LENGTH];
	char mode;
	size_t i;
	int low = 0, high = LVL_IMPL, localwho = 0, questwho = 0;
	int showclass = 0, short_list = 0, outlaws = 0, num_can_see = 0;
	int who_room = 0;

	skip_spaces (&argument);
	strcpy (buf, argument);
	name_search[0] = '\0';

	while (*buf)
	{
		half_chop (buf, arg, buf1);
		if (isdigit (*arg))
		{
			sscanf (arg, "%d-%d", &low, &high);
			strcpy (buf, buf1);
		}
		else if (*arg == '-')
		{
			mode = *(arg + 1);	/* just in case; we destroy arg in the switch */
			switch (mode)
			{
			case 'o':
			case 'k':
				outlaws = 1;
				strcpy (buf, buf1);
				break;
			case 'z':
				localwho = 1;
				strcpy (buf, buf1);
				break;
			case 's':
				short_list = 1;
				strcpy (buf, buf1);
				break;
			case 'q':
				questwho = 1;
				strcpy (buf, buf1);
				break;
			case 'l':
				half_chop (buf1, arg, buf);
				sscanf (arg, "%d-%d", &low, &high);
				break;
			case 'n':
				half_chop (buf1, name_search, buf);
				break;
			case 'r':
				who_room = 1;
				strcpy (buf, buf1);
				break;
			case 'c':
				half_chop (buf1, arg, buf);
				for (i = 0; i < strlen (arg); i++)
					showclass |= find_class_bitvector (arg[i]);
				break;
			default:
				send_to_char (WHO_FORMAT, ch);
				return;
			}					/* end of switch */

		}
		else
		{						/* endif */
			send_to_char (WHO_FORMAT, ch);
			return;
		}
	}							/* end while (parser) */

	send_to_char ("Giocatori\r\n-------\r\n", ch);

	for (d = descriptor_list; d; d = d->next)
	{
		if (STATE (d) != CON_PLAYING)
			continue;

		if (d->original)
			tch = d->original;
		else if (!(tch = d->character))
			continue;

		if (*name_search && str_cmp (GET_NAME (tch), name_search) &&
			!strstr (GET_TITLE (tch), name_search))
			continue;
		if (!CAN_SEE (ch, tch) || GET_LEVEL (tch) < low
			|| GET_LEVEL (tch) > high)
			continue;
		if (outlaws && !PLR_FLAGGED (tch, PLR_KILLER) &&
			!PLR_FLAGGED (tch, PLR_THIEF))
			continue;
		if (questwho && !PRF_FLAGGED (tch, PRF_QUEST))
			continue;
		if (localwho && world[ch->in_room].zone != world[tch->in_room].zone)
			continue;
		if (who_room && (tch->in_room != ch->in_room))
			continue;
		if (showclass && !(showclass & (1 << GET_CLASS (tch))))
			continue;
		if (short_list)
		{
			sprintf (buf, "%s[%2d %s %s] %-12.12s%s%s",
					 (GET_LEVEL (tch) >= LVL_IMMORT ? CCYEL (ch, C_SPR) : ""),
					 GET_LEVEL (tch), CLASS_ABBR (tch), RACE_ABBR (tch),
					 GET_NAME (tch),
					 (GET_LEVEL (tch) >= LVL_IMMORT ? CCNRM (ch, C_SPR) : ""),
					 ((!(++num_can_see % 4)) ? "\r\n" : ""));
			send_to_char (buf, ch);
		}
		else
		{
			num_can_see++;
			sprintf (buf, "%s[%2d %s %s] %s %s",
					 (GET_LEVEL (tch) >= LVL_IMMORT ? CCYEL (ch, C_SPR) : ""),
					 GET_LEVEL (tch), CLASS_ABBR (tch), RACE_ABBR (tch),
					 GET_NAME (tch), GET_TITLE (tch));

			if (GET_INVIS_LEV (tch))
				sprintf (buf + strlen (buf), " (i%d)", GET_INVIS_LEV (tch));
			else if (AFF_FLAGGED (tch, AFF_INVISIBLE))
				strcat (buf, " (invis)");

			if (PLR_FLAGGED (tch, PLR_MAILING))
				strcat (buf, " (mailing)");
			else if (PLR_FLAGGED (tch, PLR_WRITING))
				strcat (buf, " (writing)");

			if (PRF_FLAGGED (tch, PRF_DEAF))
				strcat (buf, " (deaf)");
			if (PRF_FLAGGED (tch, PRF_NOTELL))
				strcat (buf, " (notell)");
			if (PRF_FLAGGED (tch, PRF_QUEST))
				strcat (buf, " (quest)");
			if (PLR_FLAGGED (tch, PLR_THIEF))
				strcat (buf, " (THIEF)");
			if (PLR_FLAGGED (tch, PLR_KILLER))
				strcat (buf, " (KILLER)");
			if (PRF_FLAGGED (tch, PRF_AFK))
				strcat (buf, " (AFK)");
			if (GET_LEVEL (tch) >= LVL_IMMORT)
				strcat (buf, CCNRM (ch, C_SPR));
			strcat (buf, "\r\n");
			send_to_char (buf, ch);
		}						/* endif shortlist */
	}							/* end of for */
	if (short_list && (num_can_see % 4))
		send_to_char ("\r\n", ch);
	if (num_can_see == 0)
		sprintf (buf, "\r\nNo-one at all!\r\n");
	else if (num_can_see == 1)
		sprintf (buf, "\r\nUn solo giocatore visibile.\r\n");
	else
		sprintf (buf, "\r\n%d giocatori visibili.\r\n", num_can_see);
	send_to_char (buf, ch);
}


#define USERS_FORMAT \
"format: users [-l minlevel[-maxlevel]] [-n name] [-h host] [-c classlist] [-o] [-p]\r\n"

ACMD (do_users)
{
	const char *format = "%3d %-7s %-12s %-14s %-3s %-8s ";
	char line[200], line2[220], idletime[10], classname[20];
	char state[30], *timeptr, mode;
	char name_search[MAX_INPUT_LENGTH], host_search[MAX_INPUT_LENGTH];
	struct char_data *tch;
	struct descriptor_data *d;
	size_t i;
	int low = 0, high = LVL_IMPL, num_can_see = 0;
	int showclass = 0, outlaws = 0, playing = 0, deadweight = 0;

	host_search[0] = name_search[0] = '\0';

	strcpy (buf, argument);
	while (*buf)
	{
		half_chop (buf, arg, buf1);
		if (*arg == '-')
		{
			mode = *(arg + 1);	/* just in case; we destroy arg in the switch */
			switch (mode)
			{
			case 'o':
			case 'k':
				outlaws = 1;
				playing = 1;
				strcpy (buf, buf1);
				break;
			case 'p':
				playing = 1;
				strcpy (buf, buf1);
				break;
			case 'd':
				deadweight = 1;
				strcpy (buf, buf1);
				break;
			case 'l':
				playing = 1;
				half_chop (buf1, arg, buf);
				sscanf (arg, "%d-%d", &low, &high);
				break;
			case 'n':
				playing = 1;
				half_chop (buf1, name_search, buf);
				break;
			case 'h':
				playing = 1;
				half_chop (buf1, host_search, buf);
				break;
			case 'c':
				playing = 1;
				half_chop (buf1, arg, buf);
				for (i = 0; i < strlen (arg); i++)
					showclass |= find_class_bitvector (arg[i]);
				break;
			default:
				send_to_char (USERS_FORMAT, ch);
				return;
			}					/* end of switch */

		}
		else
		{						/* endif */
			send_to_char (USERS_FORMAT, ch);
			return;
		}
	}							/* end while (parser) */
	strcpy (line,
			"Num Class   Name         State          Idl Login@   Site\r\n");
	strcat (line,
			"--- ------- ------------ -------------- --- -------- ------------------------\r\n");
	send_to_char (line, ch);

	one_argument (argument, arg);

	for (d = descriptor_list; d; d = d->next)
	{
		if (STATE (d) != CON_PLAYING && playing)
			continue;
		if (STATE (d) == CON_PLAYING && deadweight)
			continue;
		if (STATE (d) == CON_PLAYING)
		{
			if (d->original)
				tch = d->original;
			else if (!(tch = d->character))
				continue;

			if (*host_search && !strstr (d->host, host_search))
				continue;
			if (*name_search && str_cmp (GET_NAME (tch), name_search))
				continue;
			if (!CAN_SEE (ch, tch) || GET_LEVEL (tch) < low
				|| GET_LEVEL (tch) > high)
				continue;
			if (outlaws && !PLR_FLAGGED (tch, PLR_KILLER) &&
				!PLR_FLAGGED (tch, PLR_THIEF))
				continue;
			if (showclass && !(showclass & (1 << GET_CLASS (tch))))
				continue;
			if (GET_INVIS_LEV (ch) > GET_LEVEL (ch))
				continue;

			if (d->original)
				sprintf (classname, "[%2d %s]", GET_LEVEL (d->original),
						 CLASS_ABBR (d->original));
			else
				sprintf (classname, "[%2d %s]", GET_LEVEL (d->character),
						 CLASS_ABBR (d->character));
		}
		else
			strcpy (classname, "   -   ");

		timeptr = asctime (localtime (&d->login_time));
		timeptr += 11;
		*(timeptr + 8) = '\0';

		if (STATE (d) == CON_PLAYING && d->original)
			strcpy (state, "Switched");
		else
			strcpy (state, connected_types[STATE (d)]);

		if (d->character && STATE (d) == CON_PLAYING
			&& GET_LEVEL (d->character) < LVL_GOD)
			sprintf (idletime, "%3d",
					 d->character->char_specials.timer * SECS_PER_MUD_HOUR /
					 SECS_PER_REAL_MIN);
		else
			strcpy (idletime, "");

		if (d->character && d->character->player.name)
		{
			if (d->original)
				sprintf (line, format, d->desc_num, classname,
						 d->original->player.name, state, idletime, timeptr);
			else
				sprintf (line, format, d->desc_num, classname,
						 d->character->player.name, state, idletime, timeptr);
		}
		else
			sprintf (line, format, d->desc_num, "   -   ", "UNDEFINED",
					 state, idletime, timeptr);

		if (d->host && *d->host)
			sprintf (line + strlen (line), "[%s]\r\n", d->host);
		else
			strcat (line, "[Hostname unknown]\r\n");

		if (STATE (d) != CON_PLAYING)
		{
			sprintf (line2, "%s%s%s", CCGRN (ch, C_SPR), line,
					 CCNRM (ch, C_SPR));
			strcpy (line, line2);
		}
		if (STATE (d) != CON_PLAYING ||
			(STATE (d) == CON_PLAYING && CAN_SEE (ch, d->character)))
		{
			send_to_char (line, ch);
			num_can_see++;
		}
	}

	sprintf (line, "\r\n%d visible sockets connected.\r\n", num_can_see);
	send_to_char (line, ch);
}


/* Generic page_string function for displaying text */
ACMD (do_gen_ps)
{
	switch (subcmd)
	{
	case SCMD_CREDITS:
		page_string (ch->desc, credits, 0);
		break;
	case SCMD_NEWS:
		page_string (ch->desc, news, 0);
		break;
	case SCMD_INFO:
		page_string (ch->desc, info, 0);
		break;
	case SCMD_WIZLIST:
		page_string (ch->desc, wizlist, 0);
		break;
	case SCMD_IMMLIST:
		page_string (ch->desc, immlist, 0);
		break;
	case SCMD_HANDBOOK:
		page_string (ch->desc, handbook, 0);
		break;
	case SCMD_POLICIES:
		page_string (ch->desc, policies, 0);
		break;
	case SCMD_MOTD:
		page_string (ch->desc, motd, 0);
		break;
	case SCMD_IMOTD:
		page_string (ch->desc, imotd, 0);
		break;
	case SCMD_CLEAR:
		send_to_char ("\033[H\033[J", ch);
		break;
	case SCMD_VERSION:
		send_to_char (strcat (strcpy (buf, circlemud_version), "\r\n"), ch);
		break;
	case SCMD_WHOAMI:
		send_to_char (strcat (strcpy (buf, GET_NAME (ch)), "\r\n"), ch);
		break;
	default:
		log ("SYSERR: Unhandled case in do_gen_ps. (%d)", subcmd);
		return;
	}
}


void
perform_mortal_where (struct char_data *ch, char *arg)
{
	register struct char_data *i;
	register struct descriptor_data *d;

	if (!*arg)
	{
		send_to_char ("Giocatori nella tua zona\r\n--------------------\r\n",
					  ch);
		for (d = descriptor_list; d; d = d->next)
		{
			if (STATE (d) != CON_PLAYING || d->character == ch)
				continue;
			if ((i = (d->original ? d->original : d->character)) == NULL)
				continue;
			if (i->in_room == NOWHERE || !CAN_SEE (ch, i))
				continue;
			if (world[ch->in_room].zone != world[i->in_room].zone)
				continue;
			sprintf (buf, "%-20s - %s\r\n", GET_NAME (i),
					 world[i->in_room].name);
			send_to_char (buf, ch);
		}
	}
	else
	{							/* print only FIRST char, not all. */
		for (i = character_list; i; i = i->next)
		{
			if (i->in_room == NOWHERE || i == ch)
				continue;
			if (!CAN_SEE (ch, i)
				|| world[i->in_room].zone != world[ch->in_room].zone)
				continue;
			if (!isname (arg, i->player.name))
				continue;
			sprintf (buf, "%-25s - %s\r\n", GET_NAME (i),
					 world[i->in_room].name);
			send_to_char (buf, ch);
			return;
		}
		send_to_char ("Non trovo nessuno con quel nome.\r\n", ch);
	}
}


void
print_object_location (int num, struct obj_data *obj, struct char_data *ch,
					   int recur)
{
	if (num > 0)
		sprintf (buf, "O%3d. %-25s - ", num, obj->short_description);
	else
		sprintf (buf, "%33s", " - ");

	if (obj->in_room > NOWHERE)
	{
		sprintf (buf + strlen (buf), "[%5d] %s\r\n",
				 GET_ROOM_VNUM (IN_ROOM (obj)), world[obj->in_room].name);
		send_to_char (buf, ch);
	}
	else if (obj->carried_by)
	{
		sprintf (buf + strlen (buf), "portato da %s\r\n",
				 PERS (obj->carried_by, ch));
		send_to_char (buf, ch);
	}
	else if (obj->worn_by)
	{
		sprintf (buf + strlen (buf), "indossato da %s\r\n",
				 PERS (obj->worn_by, ch));
		send_to_char (buf, ch);
	}
	else if (obj->in_obj)
	{
		sprintf (buf + strlen (buf), "dentro %s%s\r\n",
				 obj->in_obj->short_description, (recur ? ", che e'" : " "));
		send_to_char (buf, ch);
		if (recur)
			print_object_location (0, obj->in_obj, ch, recur);
	}
	else
	{
		sprintf (buf + strlen (buf), "in una localita' sconosciuta\r\n");
		send_to_char (buf, ch);
	}
}



void
perform_immort_where (struct char_data *ch, char *arg)
{
	register struct char_data *i;
	register struct obj_data *k;
	struct descriptor_data *d;
	int num = 0, found = 0;

	if (!*arg)
	{
		send_to_char ("GIocatori\r\n-------\r\n", ch);
		for (d = descriptor_list; d; d = d->next)
			if (STATE (d) == CON_PLAYING)
			{
				i = (d->original ? d->original : d->character);
				if (i && CAN_SEE (ch, i) && (i->in_room != NOWHERE))
				{
					if (d->original)
						sprintf (buf, "%-20s - [%5d] %s (in %s)\r\n",
								 GET_NAME (i),
								 GET_ROOM_VNUM (IN_ROOM (d->character)),
								 world[d->character->in_room].name,
								 GET_NAME (d->character));
					else
						sprintf (buf, "%-20s - [%5d] %s\r\n", GET_NAME (i),
								 GET_ROOM_VNUM (IN_ROOM (i)),
								 world[i->in_room].name);
					send_to_char (buf, ch);
				}
			}
	}
	else
	{
		for (i = character_list; i; i = i->next)
			if (CAN_SEE (ch, i) && i->in_room != NOWHERE
				&& isname (arg, i->player.name))
			{
				found = 1;
				sprintf (buf, "M%3d. %-25s - [%5d] %s\r\n", ++num,
						 GET_NAME (i), GET_ROOM_VNUM (IN_ROOM (i)),
						 world[IN_ROOM (i)].name);
				send_to_char (buf, ch);
			}
		for (num = 0, k = object_list; k; k = k->next)
			if (CAN_SEE_OBJ (ch, k) && isname (arg, k->name))
			{
				found = 1;
				print_object_location (++num, k, ch, TRUE);
			}
		if (!found)
			send_to_char ("Couldn't find any such thing.\r\n", ch);
	}
}



ACMD (do_where)
{
	one_argument (argument, arg);

	if (GET_LEVEL (ch) >= LVL_IMMORT)
		perform_immort_where (ch, arg);
	else
		perform_mortal_where (ch, arg);
}



ACMD (do_levels)
{
	int i;

	if (IS_NPC (ch))
	{
		send_to_char ("You ain't nothin' but a hound-dog.\r\n", ch);
		return;
	}
	*buf = '\0';

	for (i = 1; i < LVL_IMMORT; i++)
	{
		sprintf (buf + strlen (buf), "[%2d] %8d-%-8d : ", i,
				 level_exp (GET_CLASS (ch), i), level_exp (GET_CLASS (ch),
														   i + 1) - 1);
		switch (GET_SEX (ch))
		{
		case SEX_MALE:
		case SEX_NEUTRAL:
			strcat (buf, title_male (GET_CLASS (ch), i));
			break;
		case SEX_FEMALE:
			strcat (buf, title_female (GET_CLASS (ch), i));
			break;
		default:
			send_to_char ("Oh dear.  You seem to be sexless.\r\n", ch);
			break;
		}
		strcat (buf, "\r\n");
	}
	sprintf (buf + strlen (buf), "[%2d] %8d          : Immortality\r\n",
			 LVL_IMMORT, level_exp (GET_CLASS (ch), LVL_IMMORT));
	page_string (ch->desc, buf, 1);
}



ACMD (do_consider)
{
	struct char_data *victim;
	int diff;

	one_argument (argument, buf);

	if (!(victim = get_char_vis (ch, buf, NULL, FIND_CHAR_ROOM)))
	{
		send_to_char ("Chi stai considerando di uccidere?\r\n", ch);
		return;
	}
	if (victim == ch)
	{
		send_to_char ("Molto facile!\r\n", ch);
		return;
	}
	if (!IS_NPC (victim))
	{
		send_to_char
			("Hai bisogno di prendere in prestito una lapide e una vanga?\r\n",
			 ch);
		return;
	}
	diff = (GET_LEVEL (victim) - GET_LEVEL (ch));

	if (diff <= -10)
		send_to_char ("Facilissimo!\r\n", ch);
	else if (diff <= -5)
		send_to_char ("Ce la dovresti fare con una mano!\r\n", ch);
	else if (diff <= -2)
		send_to_char ("Facile.\r\n", ch);
	else if (diff <= -1)
		send_to_char ("Facile.\r\n", ch);
	else if (diff == 0)
		send_to_char ("La sfida perfetta!\r\n", ch);
	else if (diff <= 1)
		send_to_char ("Avrai bisogno di un po' di fortuna!\r\n", ch);
	else if (diff <= 2)
		send_to_char ("Avrai bisogno di fortuna!\r\n", ch);
	else if (diff <= 3)
		send_to_char
			("Arai bisogno di fortuna e di un buon equipaggiamento!\r\n", ch);
	else if (diff <= 5)
		send_to_char ("Auguri, in bocca al lupo!\r\n", ch);
	else if (diff <= 10)
		send_to_char ("Sei scemo o cosa?\r\n", ch);
	else if (diff <= 100)
		send_to_char ("Sei SCEMO!\r\n", ch);

}



ACMD (do_diagnose)
{
	struct char_data *vict;

	one_argument (argument, buf);

	if (*buf)
	{
		if (!(vict = get_char_vis (ch, buf, NULL, FIND_CHAR_ROOM)))
			send_to_char (NOPERSON, ch);
		else
			diag_char_to_char (vict, ch);
	}
	else
	{
		if (FIGHTING (ch))
			diag_char_to_char (FIGHTING (ch), ch);
		else
			send_to_char ("Diagnose who?\r\n", ch);
	}
}


const char *ctypes[] = {
	"off", "sparse", "normal", "complete", "\n"
};

ACMD (do_color)
{
	int tp;

	if (IS_NPC (ch))
		return;

	one_argument (argument, arg);

	if (!*arg)
	{
		sprintf (buf, "Your current color level is %s.\r\n",
				 ctypes[COLOR_LEV (ch)]);
		send_to_char (buf, ch);
		return;
	}
	if (((tp = search_block (arg, ctypes, FALSE)) == -1))
	{
		send_to_char ("Usage: color { Off | Sparse | Normal | Complete }\r\n",
					  ch);
		return;
	}
	REMOVE_BIT (PRF_FLAGS (ch), PRF_COLOR_1 | PRF_COLOR_2);
	SET_BIT (PRF_FLAGS (ch),
			 (PRF_COLOR_1 * (tp & 1)) | (PRF_COLOR_2 * (tp & 2) >> 1));

	sprintf (buf, "Your %scolor%s is now %s.\r\n", CCRED (ch, C_SPR),
			 CCNRM (ch, C_OFF), ctypes[tp]);
	send_to_char (buf, ch);
}


ACMD (do_toggle)
{
	if (IS_NPC (ch))
		return;
	if (GET_WIMP_LEV (ch) == 0)
		strcpy (buf2, "OFF");
	else
		sprintf (buf2, "%-3d", GET_WIMP_LEV (ch));

	if (GET_LEVEL (ch) >= LVL_IMMORT)
	{
		sprintf (buf,
				 "      No Hassle: %-3s    "
				 "      Holylight: %-3s    "
				 "     Room Flags: %-3s\r\n",
				 ONOFF (PRF_FLAGGED (ch, PRF_NOHASSLE)),
				 ONOFF (PRF_FLAGGED (ch, PRF_HOLYLIGHT)),
				 ONOFF (PRF_FLAGGED (ch, PRF_ROOMFLAGS)));
		send_to_char (buf, ch);
	}

	sprintf (buf,
			 "Hit Pnt Display: %-3s    "
			 "     Brief Mode: %-3s    "
			 " Summon Protect: %-3s\r\n"
			 "   Move Display: %-3s    "
			 "   Compact Mode: %-3s    "
			 "       On Quest: %-3s\r\n"
			 "   Mana Display: %-3s    "
			 "         NoTell: %-3s    "
			 "   Repeat Comm.: %-3s\r\n"
			 " Auto Show Exit: %-3s    "
			 "           Deaf: %-3s    "
			 "     Wimp Level: %-3s\r\n"
			 " Gossip Channel: %-3s    "
			 "Auction Channel: %-3s    "
			 "  Grats Channel: %-3s\r\n"
			 "    Color Level: %s\r\n",
			 ONOFF (PRF_FLAGGED (ch, PRF_DISPHP)),
			 ONOFF (PRF_FLAGGED (ch, PRF_BRIEF)),
			 ONOFF (!PRF_FLAGGED (ch, PRF_SUMMONABLE)),
			 ONOFF (PRF_FLAGGED (ch, PRF_DISPMOVE)),
			 ONOFF (PRF_FLAGGED (ch, PRF_COMPACT)),
			 YESNO (PRF_FLAGGED (ch, PRF_QUEST)),
			 ONOFF (PRF_FLAGGED (ch, PRF_DISPMANA)),
			 ONOFF (PRF_FLAGGED (ch, PRF_NOTELL)),
			 YESNO (!PRF_FLAGGED (ch, PRF_NOREPEAT)),
			 ONOFF (PRF_FLAGGED (ch, PRF_AUTOEXIT)),
			 YESNO (PRF_FLAGGED (ch, PRF_DEAF)),
			 buf2,
			 ONOFF (!PRF_FLAGGED (ch, PRF_NOGOSS)),
			 ONOFF (!PRF_FLAGGED (ch, PRF_NOAUCT)),
			 ONOFF (!PRF_FLAGGED (ch, PRF_NOGRATZ)), ctypes[COLOR_LEV (ch)]);

	send_to_char (buf, ch);
}


struct sort_struct
{
	int sort_pos;
	byte is_social;
}
 *cmd_sort_info = NULL;

int num_of_cmds;


void
sort_commands (void)
{
	int a, b, tmp;

	num_of_cmds = 0;

	/*
	 * first, count commands (num_of_commands is actually one greater than the
	 * number of commands; it inclues the '\n'.
	 */
	while (*cmd_info[num_of_cmds].command != '\n')
		num_of_cmds++;

	/* create data array */
	CREATE (cmd_sort_info, struct sort_struct, num_of_cmds);

	/* initialize it */
	for (a = 1; a < num_of_cmds; a++)
	{
		cmd_sort_info[a].sort_pos = a;
		cmd_sort_info[a].is_social =
			(cmd_info[a].command_pointer == do_action);
	}

	/* the infernal special case */
	cmd_sort_info[find_command ("insult")].is_social = TRUE;

	/* Sort.  'a' starts at 1, not 0, to remove 'RESERVED' */
	for (a = 1; a < num_of_cmds - 1; a++)
		for (b = a + 1; b < num_of_cmds; b++)
			if (strcmp (cmd_info[cmd_sort_info[a].sort_pos].command,
						cmd_info[cmd_sort_info[b].sort_pos].command) > 0)
			{
				tmp = cmd_sort_info[a].sort_pos;
				cmd_sort_info[a].sort_pos = cmd_sort_info[b].sort_pos;
				cmd_sort_info[b].sort_pos = tmp;
			}
}



ACMD (do_commands)
{
	int no, i, cmd_num;
	int wizhelp = 0, socials = 0;
	struct char_data *vict;

	one_argument (argument, arg);

	if (*arg)
	{
		if (!(vict = get_char_vis (ch, arg, NULL, FIND_CHAR_WORLD))
			|| IS_NPC (vict))
		{
			send_to_char ("Who is that?\r\n", ch);
			return;
		}
		if (GET_LEVEL (ch) < GET_LEVEL (vict))
		{
			send_to_char
				("You can't see the commands of people above your level.\r\n",
				 ch);
			return;
		}
	}
	else
		vict = ch;

	if (subcmd == SCMD_SOCIALS)
		socials = 1;
	else if (subcmd == SCMD_WIZHELP)
		wizhelp = 1;

	sprintf (buf, "The following %s%s are available to %s:\r\n",
			 wizhelp ? "privileged " : "",
			 socials ? "socials" : "commands",
			 vict == ch ? "you" : GET_NAME (vict));

	/* cmd_num starts at 1, not 0, to remove 'RESERVED' */
	for (no = 1, cmd_num = 1; cmd_num < num_of_cmds; cmd_num++)
	{
		i = cmd_sort_info[cmd_num].sort_pos;
		if (cmd_info[i].minimum_level >= 0 &&
			GET_LEVEL (vict) >= cmd_info[i].minimum_level &&
			(cmd_info[i].minimum_level >= LVL_IMMORT) == wizhelp &&
			(wizhelp || socials == cmd_sort_info[i].is_social))
		{
			sprintf (buf + strlen (buf), "%-11s", cmd_info[i].command);
			if (!(no % 7))
				strcat (buf, "\r\n");
			no++;
		}
	}

	strcat (buf, "\r\n");
	send_to_char (buf, ch);
}


ACMD (do_spy)
{
	int percent, prob, spy_type, return_room;
	const char *spy_dirs[] = {
		"north",
		"east",
		"south",
		"west",
		"up",
		"down",
		"\n"
	};

	/* 101% is a complete failure */
	percent = number (1, 101);
	prob = GET_SKILL (ch, SKILL_SPY);
	spy_type = search_block (argument + 1, spy_dirs, FALSE);

	if (spy_type < 0 || !EXIT (ch, spy_type)
		|| EXIT (ch, spy_type)->to_room == NOWHERE)
	{
		send_to_char ("Spiare dove?\r\n", ch);
		return;
	}
	else
	{
		if (!(GET_MOVE (ch) >= 5))
		{
			send_to_char ("Nah, sei troppo stanco.\r\n", ch);
		}
		else
		{
			if (percent > prob)
			{
				send_to_char
					("Fallisci. Hai bisogno di ulteriore pratica!!\r\n", ch);
				GET_MOVE (ch) =
					MAX (0, MIN (GET_MAX_MOVE (ch), GET_MOVE (ch) - 2));
				learned_from_mistake (ch, SKILL_SPY, 0, 90);
			}
			else
			{
				if (IS_SET (EXIT (ch, spy_type)->exit_info, EX_CLOSED)
					&& EXIT (ch, spy_type)->keyword)
				{
					sprintf (buf, "La %s e' chiusa.\r\n",
							 fname (EXIT (ch, spy_type)->keyword));
					send_to_char (buf, ch);
					GET_MOVE (ch) =
						MAX (0, MIN (GET_MAX_MOVE (ch), GET_MOVE (ch) - 2));
				}
				else
				{
					GET_MOVE (ch) =
						MAX (0, MIN (GET_MAX_MOVE (ch), GET_MOVE (ch) - 5));
					return_room = ch->in_room;
					char_from_room (ch);
					char_to_room (ch,
								  world[return_room].dir_option[spy_type]->
								  to_room);
					if (SECT (IN_ROOM (ch)) == SECT_INSIDE)
					{
						send_to_char
							("Cerchi di scrutare l'altra stanza: \r\n\r\n",
							 ch);
						look_at_room (ch, 1);
						char_from_room (ch);
						char_to_room (ch, return_room);
						act ("$n sta scrutando intorno.", TRUE, ch, 0, 0,
							 TO_NOTVICT);
					}
					else
					{
						send_to_char
							("Cerchi di scrutare i dintorni: \r\n\r\n", ch);
						look_at_room (ch, 1);
						char_from_room (ch);
						char_to_room (ch, return_room);
						act ("$n sta scrutando i dintorni.", TRUE, ch, 0, 0,
							 TO_NOTVICT);
					}
				}
			}
		}
	}
}


ACMD (do_group_who)
{
	struct descriptor_data *i;
	struct char_data *person;
	struct follow_type *f;
	int count = 0;
    int group_count = 0;
	char buf[MAX_STRING_LENGTH], tbuf[255];

	sprintf (buf, "[------- Gruppi di avventurieri -------]\n\r");

	/* go through the descriptor list */
	for (i = descriptor_list; i; i = i->next)
	{
		/* find everyone who is a master  */
		if (!i->connected)
		{
			person = (i->original ? i->original : i->character);

			/* list the master and the group name */
			if (person && !person->master && IS_AFFECTED (person, AFF_GROUP))
			{
				if (CAN_SEE (ch, person))
				{
					group_count++;
                    sprintf (tbuf, "\r\n\tGruppo %d\r\n%s <-- Leader\r\n",
                            group_count,
							GET_NAME (person));
					strcat (buf, tbuf);

					/* list the members that ch can see */
					count = 0;
					for (f = person->followers; f; f = f->next)
					{
						if (f == NULL)
						{
							/*
                                mudlog (LOG_ERROR,
									"person is affected by AFF_GROUP and don't "
									"have followers.");
                            */
						}
						else if (IS_AFFECTED (f->follower, AFF_GROUP)
								 && IS_PC (f->follower))
						{
							count++;
							if (CAN_SEE (ch, f->follower) &&
								strlen (GET_NAME (f->follower)) > 1)
							{
								sprintf (tbuf, "%s\r\n",
										 GET_NAME (f->follower));
								strcat (buf, tbuf);
							}
							else
							{
								sprintf (tbuf, "Qualcuno\r\n");
								strcat (buf, tbuf);
							}
						}
					}
					
				}
			}
		}
	}
    if (group_count == 0)
    {
        sprintf (tbuf, "\r\n\tNon ci sono gruppi online!!\r\n");
		strcat (buf, tbuf);
    }
	strcat (buf, "\r\n[---------- Fine lista --------------]\r\n");
    
	page_string (ch->desc, buf, 1);
}
