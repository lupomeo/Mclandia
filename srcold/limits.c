
/* ************************************************************************
*   File: limits.c                                      Part of CircleMUD *
*  Usage: limits & gain funcs for HMV, exp, hunger/thirst, idle time      *
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
#include "spells.h"
#include "comm.h"
#include "db.h"
#include "handler.h"
#include "interpreter.h"

extern struct char_data *character_list;
extern struct obj_data *object_list;
extern struct room_data *world;
extern int max_exp_gain;
extern int max_exp_loss;
extern int idle_rent_time;
extern int idle_max_level;
extern int idle_void;
extern int use_autowiz;
extern int min_wizlist_lev;
extern int free_rent;
extern int min_level_gap;
extern int diff_level_cap;

/* local functions */
int graf (int age, int p0, int p1, int p2, int p3, int p4, int p5, int p6);
void check_autowiz (struct char_data *ch);

void Crash_rentsave (struct char_data *ch, int cost);
int level_exp (int chclass, int level);
char *title_male (int chclass, int level);
char *title_female (int chclass, int level);
void update_char_objects (struct char_data *ch);	/* handler.c */
void reboot_wizlists (void);
void gain_level (struct char_data *ch);
void regen_proc(void);
int level_cap(struct char_data *ch);
int exp_graf(struct char_data *ch,int exp, struct char_data *vict);

/* When age < 15 return the value p0 */

/* When age in 15..29 calculate the line between p1 & p2 */

/* When age in 30..44 calculate the line between p2 & p3 */

/* When age in 45..59 calculate the line between p3 & p4 */

/* When age in 60..79 calculate the line between p4 & p5 */

/* When age >= 80 return the value p6 */
int
graf (int age, int p0, int p1, int p2, int p3, int p4, int p5, int p6)
{

	if (age < 15)
		return (p0);			/* < 15   */
	else if (age <= 29)
		return (int) (p1 + (((age - 15) * (p2 - p1)) / 15));	/* 15..29 */
	else if (age <= 44)
		return (int) (p2 + (((age - 30) * (p3 - p2)) / 15));	/* 30..44 */
	else if (age <= 59)
		return (int) (p3 + (((age - 45) * (p4 - p3)) / 15));	/* 45..59 */
	else if (age <= 79)
		return (int) (p4 + (((age - 60) * (p5 - p4)) / 20));	/* 60..79 */
	else
		return (p6);			/* >= 80 */
}


/*
 * The hit_limit, mana_limit, and move_limit functions are gone.  They
 * added an unnecessary level of complexity to the internal structure,
 * weren't particularly useful, and led to some annoying bugs.  From the
 * players' point of view, the only difference the removal of these
 * functions will make is that a character's age will now only affect
 * the HMV gain per tick, and _not_ the HMV maximums.
 */

void
regen_proc(void)
{
    register struct char_data *ch, *next_ch;
	int gain;
	
	for (ch = character_list; ch; ch = next_ch)
	{
		next_ch = ch->next;

		if (IS_NPC (ch))
		{
			continue;
		}
        gain = graf (age (ch)->year, 4, 8, 12, 16, 12, 10, 8);

        if (IS_AFFECTED(ch,AFF_REGEN_HP))
        {
            GET_HIT (ch) = MIN (GET_HIT (ch) + gain, GET_MAX_HIT (ch));	
        }

        if (IS_AFFECTED(ch,AFF_REGEN_MANA))
        {
            GET_MANA (ch) = MIN (GET_MANA (ch) + gain, GET_MAX_MANA (ch));
        }

        if (IS_CYBORG(ch))
        {
            gain = (gain/2);
            GET_MANA (ch) = MIN (GET_MANA (ch) + gain, GET_MAX_MANA (ch));
        }

        if (IS_ALIENO(ch))
        {
            gain = (gain/2);
            GET_HIT (ch) = MIN (GET_HIT (ch) + gain, GET_MAX_HIT (ch));
        }

    }
}

/* manapoint gain pr. game hour */
int
mana_gain (struct char_data *ch)
{
	int gain;

	if (IS_NPC (ch))
	{
		/* Neat and fast */
		gain = GET_LEVEL (ch);
	}
	else
	{
		gain = graf (age (ch)->year, 4, 8, 12, 16, 12, 10, 8);

		/* Class calculations */

		/* Skill/Spell calculations */
        
		/* Position calculations    */
		switch (GET_POS (ch))
		{
		case POS_SLEEPING:
			gain *= 2;
			break;
		case POS_RESTING:
			gain += (gain / 2);	/* Divide by 2 */
			break;
		case POS_SITTING:
			gain += (gain / 4);	/* Divide by 4 */
			break;
		}

		if (IS_MAGIC_USER (ch) || IS_CLERIC (ch))
			gain *= 2;

		if ((GET_COND (ch, FULL) == 0) || (GET_COND (ch, THIRST) == 0))
			gain /= 4;
	}

	if (AFF_FLAGGED (ch, AFF_POISON))
		gain /= 4;

	return (gain);
}


/* Hitpoint gain pr. game hour */
int
hit_gain (struct char_data *ch)
{
	int gain;

	if (IS_NPC (ch))
	{
		/* Neat and fast */
		gain = GET_LEVEL (ch);
	}
	else
	{

		gain = graf (age (ch)->year, 8, 12, 20, 32, 16, 10, 4);

		/* Class/Level calculations */

		/* Skill/Spell calculations */
        
		/* Position calculations    */

		switch (GET_POS (ch))
		{
		case POS_SLEEPING:
			gain += (gain / 2);	/* Divide by 2 */
			break;
		case POS_RESTING:
			gain += (gain / 4);	/* Divide by 4 */
			break;
		case POS_SITTING:
			gain += (gain / 8);	/* Divide by 8 */
			break;
		}

		if (IS_MAGIC_USER (ch) || IS_CLERIC (ch))
			gain /= 2;			/* Ouch. */

		if ((GET_COND (ch, FULL) == 0) || (GET_COND (ch, THIRST) == 0))
			gain /= 4;
	}

	if (AFF_FLAGGED (ch, AFF_POISON))
		gain /= 4;

	return (gain);
}



/* move gain pr. game hour */
int
move_gain (struct char_data *ch)
{
	int gain;

	if (IS_NPC (ch))
	{
		/* Neat and fast */
		gain = GET_LEVEL (ch);
	}
	else
	{
		gain = graf (age (ch)->year, 16, 20, 24, 20, 16, 12, 10);

		/* Class/Level calculations */

		/* Skill/Spell calculations */


		/* Position calculations    */
		switch (GET_POS (ch))
		{
		case POS_SLEEPING:
			gain += (gain / 2);	/* Divide by 2 */
			break;
		case POS_RESTING:
			gain += (gain / 4);	/* Divide by 4 */
			break;
		case POS_SITTING:
			gain += (gain / 8);	/* Divide by 8 */
			break;
		}

		if ((GET_COND (ch, FULL) == 0) || (GET_COND (ch, THIRST) == 0))
			gain /= 4;
	}

	if (AFF_FLAGGED (ch, AFF_POISON))
		gain /= 4;

	return (gain);
}



void
set_title (struct char_data *ch, char *title)
{
	if (title == NULL)
	{
		if (GET_SEX (ch) == SEX_FEMALE)
			title = title_female (GET_CLASS (ch), GET_LEVEL (ch));
		else
			title = title_male (GET_CLASS (ch), GET_LEVEL (ch));
	}

	if (strlen (title) > MAX_TITLE_LENGTH)
		title[MAX_TITLE_LENGTH] = '\0';

	if (GET_TITLE (ch) != NULL)
		free (GET_TITLE (ch));

	GET_TITLE (ch) = str_dup (title);
}


void
check_autowiz (struct char_data *ch)
{
#if defined(CIRCLE_UNIX) || defined(CIRCLE_WINDOWS)
	if (use_autowiz && GET_LEVEL (ch) >= LVL_IMMORT)
	{
		char buf[128];

#if defined(CIRCLE_UNIX)
		sprintf (buf, "nice ../bin/autowiz %d %s %d %s %d &", min_wizlist_lev,
				 WIZLIST_FILE, LVL_IMMORT, IMMLIST_FILE, (int) getpid ());
#elif defined(CIRCLE_WINDOWS)
		sprintf (buf, "autowiz %d %s %d %s", min_wizlist_lev,
				 WIZLIST_FILE, LVL_IMMORT, IMMLIST_FILE);
#endif /* CIRCLE_WINDOWS */

		mudlog ("Initiating autowiz.", CMP, LVL_IMMORT, FALSE);
		system (buf);
		reboot_wizlists ();
	}
#endif /* CIRCLE_UNIX || CIRCLE_WINDOWS */
}



void
gain_exp (struct char_data *ch, int gain)
{
	//int is_altered = FALSE;
	//int num_levels = 0;
	//char buf[128];

	if (!IS_NPC (ch)
		&& ((GET_LEVEL (ch) < 1 || GET_LEVEL (ch) >= LVL_IMMORT - 1)))

	{
		return;
	}

	if (IS_NPC (ch))
	{
		GET_EXP (ch) += gain;
		return;
	}
	if (gain > 0)
	{
		gain = MIN (max_exp_gain, gain);	/* put a cap on the max gain per kill */
		GET_EXP (ch) += gain;

		/* rimosso l'autogain */
		/*
		   while (GET_LEVEL(ch) < LVL_IMMORT &&
		   GET_EXP(ch) >= level_exp(GET_CLASS(ch), GET_LEVEL(ch) + 1)) 
		   {
		   GET_LEVEL(ch) += 1;
		   num_levels++;
		   advance_level(ch);
		   is_altered = TRUE;
		   }

		   if (is_altered) 
		   {
		   sprintf(buf, "%s avanza di %d levell%s a %d livello.",
		   GET_NAME(ch), num_levels, num_levels == 1 ? "o" : "i",
		   GET_LEVEL(ch));
		   mudlog(buf, BRF, MAX(LVL_IMMORT, GET_INVIS_LEV(ch)), TRUE);
		   if (num_levels == 1)
		   {
		   send_to_char("Sali di un livello!\r\n", ch);
		   }
		   else 
		   {
		   sprintf(buf, "Sali di %d livelli!\r\n", num_levels);
		   send_to_char(buf, ch);
		   }
		   set_title(ch, NULL);
		   check_autowiz(ch);
		   } */
	}
	else if (gain < 0)
	{
		gain = MAX (-max_exp_loss, gain);	/* Cap max exp lost per death */
		GET_EXP (ch) += gain;
		if (GET_EXP (ch) < 0)
		{
			GET_EXP (ch) = 0;
		}
	}
}

/* Riservato solo a implementors */
void
gain_exp_regardless (struct char_data *ch, int gain)
{
	int is_altered = FALSE;
	int num_levels = 0;

	GET_EXP (ch) += gain;
	if (GET_EXP (ch) < 0)
		GET_EXP (ch) = 0;

	if (!IS_NPC (ch))
	{
		while (GET_LEVEL (ch) < LVL_IMPL &&
			   GET_EXP (ch) >= level_exp (GET_CLASS (ch), GET_LEVEL (ch) + 1))
		{
			GET_LEVEL (ch) += 1;
			num_levels++;
			advance_level (ch);
			is_altered = TRUE;
		}

		if (is_altered)
		{
			sprintf (buf, "%s avanza di %d livell%s a %d livello.",
					 GET_NAME (ch), num_levels, num_levels == 1 ? "o" : "i",
					 GET_LEVEL (ch));
			mudlog (buf, BRF, MAX (LVL_IMMORT, GET_INVIS_LEV (ch)), TRUE);
			if (num_levels == 1)
				send_to_char ("Sali di un livello!\r\n", ch);
			else
			{
				sprintf (buf, "Sali di %d livelli!\r\n", num_levels);
				send_to_char (buf, ch);
			}
			set_title (ch, NULL);
			check_autowiz (ch);
		}

	}
}

void
gain_level (struct char_data *ch)
{
	if (!IS_NPC (ch))
	{

		if (GET_LEVEL (ch) < LVL_IMPL &&
			GET_EXP (ch) >= level_exp (GET_CLASS (ch), GET_LEVEL (ch) + 1))
		{
			GET_LEVEL (ch) += 1;
			advance_level (ch);
			sprintf (buf, "%s avanza di 1 livello a %d livello.",
					 GET_NAME (ch), GET_LEVEL (ch));
			mudlog (buf, BRF, MAX (LVL_IMMORT, GET_INVIS_LEV (ch)), TRUE);
			send_to_char ("Sali di un livello!\r\n", ch);
			set_title (ch, NULL);
			check_autowiz (ch);
		}
	}
}

void
gain_condition (struct char_data *ch, int condition, int value)
{
	bool intoxicated;

	if (IS_NPC (ch) || GET_COND (ch, condition) == -1)	/* No change */
		return;

	intoxicated = (GET_COND (ch, DRUNK) > 0);

	GET_COND (ch, condition) += value;

	GET_COND (ch, condition) = MAX (0, GET_COND (ch, condition));
	GET_COND (ch, condition) = MIN (24, GET_COND (ch, condition));

	if (GET_COND (ch, condition) || PLR_FLAGGED (ch, PLR_WRITING))
		return;

	switch (condition)
	{
	case FULL:
		send_to_char ("Sei affamato.\r\n", ch);
		return;
	case THIRST:
		send_to_char ("Sei assetato.\r\n", ch);
		return;
	case DRUNK:
		if (intoxicated)
			send_to_char ("Sei ritornato sobrio.\r\n", ch);
		return;
	default:
		break;
	}

}


void
check_idling (struct char_data *ch)
{
	if (++(ch->char_specials.timer) > idle_void)
	{
		if (GET_WAS_IN (ch) == NOWHERE && ch->in_room != NOWHERE)
		{
			GET_WAS_IN (ch) = ch->in_room;
			if (FIGHTING (ch))
			{
				stop_fighting (FIGHTING (ch));
				stop_fighting (ch);
			}
			act ("$n sparisce nel vuoto.", TRUE, ch, 0, 0, TO_ROOM);
			send_to_char
				("Sei ora in idle, e sei stato portato nel vuoto.\r\n", ch);
			save_char (ch, NOWHERE);
			Crash_crashsave (ch);
			char_from_room (ch);
			char_to_room (ch, 1);
		}
		else if (ch->char_specials.timer > idle_rent_time)
		{
			if (ch->in_room != NOWHERE)
				char_from_room (ch);
			char_to_room (ch, 3);
			if (ch->desc)
			{
				STATE (ch->desc) = CON_DISCONNECT;
				/*
				 * For the 'if (d->character)' test in close_socket().
				 * -gg 3/1/98 (Happy anniversary.)
				 */
				ch->desc->character = NULL;
				ch->desc = NULL;
			}
			if (free_rent)
				Crash_rentsave (ch, 0);
			else
				Crash_idlesave (ch);
			sprintf (buf, "%s force-rented and extracted (idle).",
					 GET_NAME (ch));
			mudlog (buf, CMP, LVL_GOD, TRUE);
			extract_char (ch);
		}
	}
}



/* Update PCs, NPCs, and objects */
void
point_update (void)
{
	struct char_data *i, *next_char;
	struct obj_data *j, *next_thing, *jj, *next_thing2;

	/* characters */
	for (i = character_list; i; i = next_char)
	{
		next_char = i->next;

		gain_condition (i, FULL, -1);
		gain_condition (i, DRUNK, -1);
		gain_condition (i, THIRST, -1);

		if (GET_POS (i) >= POS_STUNNED)
		{
			GET_HIT (i) = MIN (GET_HIT (i) + hit_gain (i), GET_MAX_HIT (i));
			GET_MANA (i) =
				MIN (GET_MANA (i) + mana_gain (i), GET_MAX_MANA (i));
			GET_MOVE (i) =
				MIN (GET_MOVE (i) + move_gain (i), GET_MAX_MOVE (i));
			if (AFF_FLAGGED (i, AFF_POISON))
				if (damage (i, i, 2, SPELL_POISON) == -1)
					continue;	/* Oops, they died. -gg 6/24/98 */
			if (GET_POS (i) <= POS_STUNNED)
				update_pos (i);
		}
		else if (GET_POS (i) == POS_INCAP)
		{
			if (damage (i, i, 1, TYPE_SUFFERING) == -1)
				continue;
		}
		else if (GET_POS (i) == POS_MORTALLYW)
		{
			if (damage (i, i, 2, TYPE_SUFFERING) == -1)
				continue;
		}
		if (!IS_NPC (i))
		{
			update_char_objects (i);
			if (GET_LEVEL (i) < idle_max_level)
				check_idling (i);
		}
	}

	/* objects */
	for (j = object_list; j; j = next_thing)
	{
		next_thing = j->next;	/* Next in object list */


		/* If this is a corpse */
		if (IS_CORPSE (j))
		{
			/* timer count down */
			if (GET_OBJ_TIMER (j) > 0)
				GET_OBJ_TIMER (j)--;

			if (!GET_OBJ_TIMER (j))
			{

				if (j->carried_by)
					act ("$p decade dalle tue mani.", FALSE, j->carried_by, j,
						 0, TO_CHAR);
				else if ((j->in_room != NOWHERE)
						 && (world[j->in_room].people))
				{
					act ("Una orda di bit 0 cancella $p.",
						 TRUE, world[j->in_room].people, j, 0, TO_ROOM);
					act ("Una orda di bit 0 cancella $p.",
						 TRUE, world[j->in_room].people, j, 0, TO_CHAR);
				}
				for (jj = j->contains; jj; jj = next_thing2)
				{
					next_thing2 = jj->next_content;	/* Next in inventory */
					obj_from_obj (jj);

					if (j->in_obj)
						obj_to_obj (jj, j->in_obj);
					else if (j->carried_by)
						obj_to_room (jj, j->carried_by->in_room);
					else if (j->in_room != NOWHERE)
						obj_to_room (jj, j->in_room);
					else
						core_dump ();
				}
				extract_obj (j);

			}
		}

		if (GET_OBJ_TYPE (j) == ITEM_PORTAL)
		{
			if (GET_OBJ_TIMER (j) > 0)
				GET_OBJ_TIMER (j)--;
			if (!GET_OBJ_TIMER (j))
			{
				act
					("Una brillante porta telnet svanisce lentamente dall'esistenza.",
					 TRUE, world[j->in_room].people, j, 0, TO_ROOM);
				act
					("Una brillante porta telnet svanisce lentamente dall'esistenza.",
					 TRUE, world[j->in_room].people, j, 0, TO_CHAR);
				extract_obj (j);
			}
		}
	}
}

int 
level_cap(struct char_data *ch)
{
    if (GET_LEVEL(ch)<=15)
    {
        return min_level_gap;
    }
    else if (GET_LEVEL(ch)<=30)
    {
        return min_level_gap+5;
    }
    else if (GET_LEVEL(ch)<=45)
    {
        return min_level_gap+10;
    }
    else
    {
        return min_level_gap+15;
    }
}

int 
exp_graf(struct char_data *ch,int exp, struct char_data *vict)
{
    int diff, coeff, result;

    if ( diff_level_cap == 1 )
    {
        /* Diff Level Cap */
        diff = (GET_LEVEL (vict) - GET_LEVEL (ch));
        result = 1;

        coeff = (exp/level_cap(ch))*abs(diff);

        if ( diff < 0 )
        {
            if ( diff < (-level_cap(ch) ))
            {
                return 1;
            }
            else
            {
                result = exp - coeff;
            }
        }
        else if ( diff == 0)
        {
            result = exp;
        }
        else if (diff > 0)
        {
            if (diff > level_cap(ch) )
            {
                result = exp + exp/4;
            }
            else
            {
                result = exp + coeff/2;
            }
        }
        return result;
    }
    else
    {
        /* caso diff level cap disabilitato */
        return exp;
    }
}
