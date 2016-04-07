
/* ************************************************************************
*   File: spec_procs.c                                  Part of CircleMUD *
*  Usage: implementation of special procedures for mobiles/objects/rooms  *
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
#include "constants.h"
#include "mobskill.h"

/* meo home vars */
struct char_data *tmpch;
struct char_data *tmpch1;
int mastime;
int mastime1;

/*   external vars  */
extern struct room_data *world;
extern struct char_data *character_list;
extern struct descriptor_data *descriptor_list;
extern struct index_data *mob_index;
extern struct index_data *obj_index;
extern struct time_info_data time_info;
extern struct spell_info_type spell_info[];
extern int guild_info[][3];
extern char *shot_types[];
extern int shot_damage[][2];
extern char *types_ammo[][2];

/* extern functions */
void add_follower (struct char_data *ch, struct char_data *leader);
void gain_level (struct char_data *ch);
void look_at_char (struct char_data *i, struct char_data *ch);
ACMD (do_drop);
ACMD (do_gen_door);
ACMD (do_say);
int compute_armor_class (struct char_data *ch);

/* local functions */
void sort_spells (void);
int compare_spells (const void *x, const void *y);
const char *how_good (int percent);
void list_skills (struct char_data *ch);
struct char_data *find_MobInRoomWithFunc (int room,
										  int (name) (struct char_data * ch,
													  void *me, int cmd,char *argument));
void npc_steal (struct char_data *ch, struct char_data *victim);

SPECIAL (guild);
SPECIAL (master);
SPECIAL (dump);
SPECIAL (mayor);
SPECIAL (snake);
SPECIAL (thief);
SPECIAL (magic_user);
SPECIAL (fire_weapons);
SPECIAL (guild_guard);
SPECIAL (puff);
SPECIAL (fido);
SPECIAL (janitor);
SPECIAL (cityguard);
SPECIAL (guard_tower);
SPECIAL (pet_shops);
SPECIAL (bank);
SPECIAL (vigile);
SPECIAL (lvl_aggro);
SPECIAL (massage);
SPECIAL (mob_massage);
SPECIAL (massage_1);
SPECIAL (mob_massage_1);
SPECIAL (tac_machine);
SPECIAL (newbie_guide);




/* ********************************************************************
*  Special procedures for mobiles                                     *
******************************************************************** */

int spell_sort_info[MAX_SKILLS + 1];

int
compare_spells (const void *x, const void *y)
{
	int a = *(const int *) x, b = *(const int *) y;

	return strcmp (spell_info[a].name, spell_info[b].name);
}

void
sort_spells (void)
{
	int a;

	/* initialize array, avoiding reserved. */
	for (a = 1; a <= MAX_SKILLS; a++)
		spell_sort_info[a] = a;

	qsort (&spell_sort_info[1], MAX_SKILLS, sizeof (int), compare_spells);
}

const char *
how_good (int percent)
{
	if (percent < 0)
		return " error)";
	if (percent == 0)
		return " (not learned)";
	if (percent <= 10)
		return " (awful)";
	if (percent <= 20)
		return " (bad)";
	if (percent <= 40)
		return " (poor)";
	if (percent <= 55)
		return " (average)";
	if (percent <= 70)
		return " (fair)";
	if (percent <= 80)
		return " (good)";
	if (percent <= 85)
		return " (very good)";

	return " (superb)";
}

const char *prac_types[] = {
	"spell",
	"skill"
};

#define LEARNED_LEVEL	0		/* % known which is considered "learned" */
#define MAX_PER_PRAC	1		/* max percent gain in skill per practice */
#define MIN_PER_PRAC	2		/* min percent gain in skill per practice */
#define PRAC_TYPE	3			/* should it say 'spell' or 'skill'?     */

/* actual prac_params are in class.c */
extern int prac_params[4][NUM_CLASSES];

#define LEARNED(ch) (prac_params[LEARNED_LEVEL][(int)GET_CLASS(ch)])
#define MINGAIN(ch) (prac_params[MIN_PER_PRAC][(int)GET_CLASS(ch)])
#define MAXGAIN(ch) (prac_params[MAX_PER_PRAC][(int)GET_CLASS(ch)])
#define SPLSKL(ch) (prac_types[prac_params[PRAC_TYPE][(int)GET_CLASS(ch)]])

void
list_skills (struct char_data *ch)
{
	int i, sortpos;

	if (!GET_PRACTICES (ch))
		strcpy (buf, "You have no practice sessions remaining.\r\n");
	else
		sprintf (buf, "You have %d practice session%s remaining.\r\n",
				 GET_PRACTICES (ch), (GET_PRACTICES (ch) == 1 ? "" : "s"));

	sprintf (buf + strlen (buf), "You know of the following %ss:\r\n",
			 SPLSKL (ch));

	strcpy (buf2, buf);

	for (sortpos = 1; sortpos <= MAX_SKILLS; sortpos++)
	{
		i = spell_sort_info[sortpos];
		if (strlen (buf2) >= MAX_STRING_LENGTH - 32)
		{
			strcat (buf2, "**OVERFLOW**\r\n");
			break;
		}
		if (GET_LEVEL (ch) >= spell_info[i].min_level[(int) GET_CLASS (ch)])
		{
			sprintf (buf, "%-20s %s\r\n", spell_info[i].name,
					 how_good (GET_SKILL (ch, i)));
			strcat (buf2, buf);	/* The above, ^ should always be safe to do. */
		}
	}

	page_string (ch->desc, buf2, 1);
}


struct char_data *
find_MobInRoomWithFunc (int room,
						int (name) (struct char_data * ch, void *me, int cmd,
									char *argument))
{
	struct char_data *i, *targ;

	targ = 0;

	if (room > NOWHERE)
	{
		for (i = world[room].people; (!targ) && (i); i = i->next_in_room)
		{

			if (IS_MOB (i))
			{
				if (GET_MOB_SPEC (i) == name)
					targ = i;
			}
		}
	}
	else
	{
		return (0);
	}

	return (targ);
}


SPECIAL (guild)
{
	int skill_num, percent;

	if (IS_NPC (ch) || !CMD_IS ("practice"))
		return (0);

	skip_spaces (&argument);

	if (!*argument)
	{
		list_skills (ch);
		return (1);
	}
	if (GET_PRACTICES (ch) <= 0)
	{
		send_to_char ("You do not seem to be able to practice now.\r\n", ch);
		return (1);
	}

	skill_num = find_skill_num (argument);

	if (skill_num < 1 ||
		GET_LEVEL (ch) <
		spell_info[skill_num].min_level[(int) GET_CLASS (ch)])
	{
		sprintf (buf, "You do not know of that %s.\r\n", SPLSKL (ch));
		send_to_char (buf, ch);
		return (1);
	}
	if (GET_SKILL (ch, skill_num) >= LEARNED (ch))
	{
		send_to_char ("You are already learned in that area.\r\n", ch);
		return (1);
	}
	send_to_char ("You practice for a while...\r\n", ch);
	GET_PRACTICES (ch)--;

	percent = GET_SKILL (ch, skill_num);
	percent +=
		MIN (MAXGAIN (ch), MAX (MINGAIN (ch), int_app[GET_INT (ch)].learn));

	SET_SKILL (ch, skill_num, MIN (LEARNED (ch), percent));

	if (GET_SKILL (ch, skill_num) >= LEARNED (ch))
	{
		send_to_char ("You are now learned in that area.\r\n", ch);
	}

	return (1);
}

SPECIAL (master)
{
	int skill_num, percent;
	struct char_data *guildmaster;

	if (IS_NPC (ch))
		return (0);

	guildmaster = find_MobInRoomWithFunc (IN_ROOM (ch), master);

	if (CMD_IS ("practice"))
	{
		skip_spaces (&argument);

		if (!*argument)
		{
			list_skills (ch);
			return (1);
		}
		if (GET_PRACTICES (ch) <= 0)
		{
			act ("$n dice, 'Non hai sessioni di pratica disponibili.'", FALSE,
				 guildmaster, 0, 0, TO_ROOM);
			return (1);
		}

		skill_num = find_skill_num (argument);

		if (skill_num < 1 ||
			GET_LEVEL (ch) <
			spell_info[skill_num].min_level[(int) GET_CLASS (ch)])
		{
			sprintf (buf, "$n dice, 'Non conosco questa %s.'", SPLSKL (ch));
			act (buf, FALSE, guildmaster, 0, 0, TO_ROOM);
			return (1);
		}
		if (GET_SKILL (ch, skill_num) >= LEARNED (ch))
		{
			act ("$n dice, 'Hai gia imparato tutto in questo campo..'", FALSE,
				 guildmaster, 0, 0, TO_ROOM);
			return (1);
		}
		send_to_char ("Pratichi per un po'...\r\n", ch);
		GET_PRACTICES (ch)--;

		percent = GET_SKILL (ch, skill_num);
		percent +=
			MIN (MAXGAIN (ch),
				 MAX (MINGAIN (ch), int_app[GET_INT (ch)].learn));

		SET_SKILL (ch, skill_num, MIN (LEARNED (ch), percent));

		if (GET_SKILL (ch, skill_num) >= LEARNED (ch))
		{
			send_to_char ("Sai tutto su questo campo.\r\n", ch);
		}
		return (1);
	}

	if (CMD_IS ("gain"))
	{
		skip_spaces (&argument);

		if (!*argument)
		{


			if (GET_LEVEL (ch) < LVL_IMMORT &&
				GET_EXP (ch) >= level_exp (GET_CLASS (ch),
										   GET_LEVEL (ch) + 1))
			{
				act ("$n dice, 'Complimenti $N!'", FALSE, guildmaster, 0, ch,
					 TO_ROOM);
				gain_level (ch);
			}
			else
			{
				sprintf (buf, "$n dice a $N, 'Non sei ancora pront%s!'",
						 (GET_SEX (ch) == SEX_FEMALE ? "a" : "o"));
				act (buf, TRUE, guildmaster, 0, ch, TO_NOTVICT);
				sprintf (buf, "$n ti dice, 'Non sei ancora pront%s!'",
						 (GET_SEX (ch) == SEX_FEMALE ? "a" : "o"));
				act (buf, FALSE, guildmaster, 0, ch, TO_VICT);

			}
			return (1);
		}
		else
		{
			sprintf (buf, "Non so come fare %s.\r\n", argument);
			send_to_char (buf, ch);
			return (1);
		}
	}
	return (0);

}

SPECIAL (dump)
{
	struct obj_data *k;
	int value = 0;

	for (k = world[ch->in_room].contents; k; k = world[ch->in_room].contents)
	{
		act ("$p vanishes in a puff of smoke!", FALSE, 0, k, 0, TO_ROOM);
		extract_obj (k);
	}

	if (!CMD_IS ("drop"))
		return (0);

	do_drop (ch, argument, cmd, 0);

	for (k = world[ch->in_room].contents; k; k = world[ch->in_room].contents)
	{
		act ("$p vanishes in a puff of smoke!", FALSE, 0, k, 0, TO_ROOM);
		value += MAX (1, MIN (50, GET_OBJ_COST (k) / 10));
		extract_obj (k);
	}

	if (value)
	{
		send_to_char ("You are awarded for outstanding performance.\r\n", ch);
		act ("$n has been awarded for being a good citizen.", TRUE, ch, 0, 0,
			 TO_ROOM);

		if (GET_LEVEL (ch) < 3)
			gain_exp (ch, value);
		else
			GET_GOLD (ch) += value;
	}
	return (1);
}


SPECIAL (mayor)
{
	const char open_path[] =
		"W3a3003b33000c111d0d111Oe333333Oe22c222112212111a1S.";
	const char close_path[] =
		"W3a3003b33000c111d0d111CE333333CE22c222112212111a1S.";

	static const char *path = NULL;
	static int index;
	static bool move = FALSE;

	if (!move)
	{
		if (time_info.hours == 6)
		{
			move = TRUE;
			path = open_path;
			index = 0;
		}
		else if (time_info.hours == 20)
		{
			move = TRUE;
			path = close_path;
			index = 0;
		}
	}
	if (cmd || !move || (GET_POS (ch) < POS_SLEEPING) ||
		(GET_POS (ch) == POS_FIGHTING))
		return (FALSE);

	switch (path[index])
	{
	case '0':
	case '1':
	case '2':
	case '3':
		perform_move (ch, path[index] - '0', 1);
		break;

	case 'W':
		GET_POS (ch) = POS_STANDING;
		act ("$n si sveglia e si lamenta rumorosamente.", FALSE, ch, 0, 0,
			 TO_ROOM);
		break;

	case 'S':
		GET_POS (ch) = POS_SLEEPING;
		act ("$n si sdraia e crolla dal sonno.", FALSE, ch, 0, 0, TO_ROOM);
		break;

	case 'a':
		act ("$n dice 'Hello Honey!'", FALSE, ch, 0, 0, TO_ROOM);
		act ("$n sorride compiacente.", FALSE, ch, 0, 0, TO_ROOM);
		break;

	case 'b':
		act
			("$n dice 'Che vista orribile!  Devo fare qualcosa con questa discarica!'",
			 FALSE, ch, 0, 0, TO_ROOM);
		break;

	case 'c':
		act
			("$n dice 'Vandali!  'Sti giovinastri non hanno nessun rispetto!'",
			 FALSE, ch, 0, 0, TO_ROOM);
		break;

	case 'd':
		act ("$n dice 'Buongiorno, cittadini!'", FALSE, ch, 0, 0, TO_ROOM);
		break;

	case 'e':
		act ("$n dice 'Dichiaro solennemente Mclandia aperta!'", FALSE, ch, 0,
			 0, TO_ROOM);
		break;

	case 'E':
		act ("$n dice 'Dichiaro solennemente Mclandia chiusa!'", FALSE, ch, 0,
			 0, TO_ROOM);
		break;

	case 'O':
		do_gen_door (ch, "gate", 0, SCMD_UNLOCK);
		do_gen_door (ch, "gate", 0, SCMD_OPEN);
		break;

	case 'C':
		do_gen_door (ch, "gate", 0, SCMD_CLOSE);
		do_gen_door (ch, "gate", 0, SCMD_LOCK);
		break;

	case '.':
		move = FALSE;
		break;

	}

	index++;
	return (FALSE);
}


/* ********************************************************************
*  General special procedures for mobiles                             *
******************************************************************** */

/* funzione mantenuta per ragioni di compatibilita' */
void
npc_steal (struct char_data *ch, struct char_data *victim)
{
    do_mob_steal(ch,victim);
    return;
}


SPECIAL (snake)
{
	if (cmd)
		return (FALSE);

	if (GET_POS (ch) != POS_FIGHTING)
		return (FALSE);

	if (FIGHTING (ch) && (FIGHTING (ch)->in_room == ch->in_room) &&
		(number (0, 42 - GET_LEVEL (ch)) == 0))
	{
		act ("$n azzanna $N!", 1, ch, 0, FIGHTING (ch), TO_NOTVICT);
		act ("$n ti azzanna!", 1, ch, 0, FIGHTING (ch), TO_VICT);
		call_magic (ch, FIGHTING (ch), 0, SPELL_POISON, GET_LEVEL (ch),
					CAST_SPELL);
		return (TRUE);
	}
	return (FALSE);
}


SPECIAL (thief)
{
	struct char_data *cons;

	if (cmd)
		return (FALSE);

	if (GET_POS (ch) != POS_STANDING)
		return (FALSE);

	for (cons = world[ch->in_room].people; cons; cons = cons->next_in_room)
		if (!IS_NPC (cons) && (GET_LEVEL (cons) < LVL_IMMORT)
			&& (!number (0, 4)))
		{
			npc_steal (ch, cons);
			return (TRUE);
		}
	return (FALSE);
}


SPECIAL (magic_user)
{
	struct char_data *vict;

	if (cmd || GET_POS (ch) != POS_FIGHTING)
		return (FALSE);

	/* pseudo-randomly choose someone in the room who is fighting me */
	for (vict = world[ch->in_room].people; vict; vict = vict->next_in_room)
		if (FIGHTING (vict) == ch && !number (0, 4))
			break;

	/* if I didn't pick any of those, then just slam the guy I'm fighting */
	if (vict == NULL && IN_ROOM (FIGHTING (ch)) == IN_ROOM (ch))
		vict = FIGHTING (ch);

	/* Hm...didn't pick anyone...I'll wait a round. */
	if (vict == NULL)
		return (TRUE);

	if ((GET_LEVEL (ch) > 13) && (number (0, 10) == 0))
		cast_spell (ch, vict, NULL, SPELL_SLEEP);

	if ((GET_LEVEL (ch) > 7) && (number (0, 8) == 0))
		cast_spell (ch, vict, NULL, SPELL_BLINDNESS);

	if ((GET_LEVEL (ch) > 12) && (number (0, 12) == 0))
	{
		if (IS_EVIL (ch))
			cast_spell (ch, vict, NULL, SPELL_ENERGY_DRAIN);
		else if (IS_GOOD (ch))
			cast_spell (ch, vict, NULL, SPELL_DISPEL_EVIL);
	}
	if (number (0, 4))
		return (TRUE);

	switch (GET_LEVEL (ch))
	{
	case 4:
	case 5:
		cast_spell (ch, vict, NULL, SPELL_MAGIC_MISSILE);
		break;
	case 6:
	case 7:
		cast_spell (ch, vict, NULL, SPELL_CHILL_TOUCH);
		break;
	case 8:
	case 9:
		cast_spell (ch, vict, NULL, SPELL_BURNING_HANDS);
		break;
	case 10:
	case 11:
		cast_spell (ch, vict, NULL, SPELL_SHOCKING_GRASP);
		break;
	case 12:
	case 13:
		cast_spell (ch, vict, NULL, SPELL_LIGHTNING_BOLT);
		break;
	case 14:
	case 15:
	case 16:
	case 17:
		cast_spell (ch, vict, NULL, SPELL_COLOR_SPRAY);
		break;
	default:
		cast_spell (ch, vict, NULL, SPELL_FIREBALL);
		break;
	}
	return (TRUE);

}

#define NEW_SPEC_MOB 1

SPECIAL (fire_weapons)
{

#ifdef NEW_SPEC_MOB
    
    struct char_data *vict;
	
	if (cmd || GET_POS (ch) != POS_FIGHTING)
		return (FALSE);

	/* pseudo-randomly choose someone in the room who is fighting me */
	for (vict = world[ch->in_room].people; vict;
		 vict = vict->next_in_room)
	{
		if (FIGHTING (vict) == ch && !number (0, 4))
		{
			break;
		}
	}

	/* if I didn't pick any of those, then just slam the guy I'm fighting */
	if (vict == NULL && IN_ROOM (FIGHTING (ch)) == IN_ROOM (ch))
	{
		vict = FIGHTING (ch);
	}

	/* Hm...didn't pick anyone...I'll wait a round. */
	if (vict == NULL)
	{
		return (FALSE);
	}
    
    return do_mob_fireweapon(ch,vict);

#else 
    /* vecchia routine, da eliminare in seguito */


    struct char_data *vict, *shooter;
	struct obj_data *weapon;
	int num1 = 0;
	int dmg = 0;
	int roll = 0;
	int num_needed = 0;
	char shot[256];
	char argt[255];
	int i = 0;
	int ripple = 0;

	shooter = ch;
	/*shooter = find_MobInRoomWithFunc(IN_ROOM(ch),fire_weapons); */

	if (cmd || GET_POS (ch) != POS_FIGHTING)
		return (FALSE);

	/* pseudo-randomly choose someone in the room who is fighting me */
	for (vict = world[shooter->in_room].people; vict;
		 vict = vict->next_in_room)
	{
		if (FIGHTING (vict) == shooter && !number (0, 4))
		{
			break;
		}
	}

	/* if I didn't pick any of those, then just slam the guy I'm fighting */
	if (vict == NULL && IN_ROOM (FIGHTING (shooter)) == IN_ROOM (shooter))
	{
		vict = FIGHTING (shooter);
	}

	/* Hm...didn't pick anyone...I'll wait a round. */
	if (vict == NULL)
	{
		return (FALSE);
    }

	weapon = GET_EQ (shooter, WEAR_HOLD);

    
	/ non ha nessuna arma */
	if (!weapon)
	{
		return (FALSE);
	}

	/* l'oggetto in mano non e' una arma da fuoco */
	if ((GET_OBJ_TYPE (weapon) != ITEM_FIREWEAPON))
	{
		return (TRUE);
	}

	/* se e' scarica  lo ricarico e perdo il round */
	if (!GET_OBJ_VAL (weapon, 3))
	{
		num_needed = GET_OBJ_VAL (weapon, 2) - GET_OBJ_VAL (weapon, 3);
		GET_OBJ_VAL (weapon, 3) += num_needed;
		act ("Carichi $p", FALSE, shooter, weapon, 0, TO_CHAR);
		act ("$n smette di sparare e carica $p", FALSE, shooter, weapon, 0,
			 TO_ROOM);
		return (TRUE);
	}

	/* finalmnete sparo */
	/* se usa arma automatica, imposto la modalita' a raffica */
	if (IS_AUTO_FIREWEAPON (weapon))
	{
		ripple = 3;
		strcpy (argt, "una raffica");
	}
	else
	{
		ripple = 1;
		strcpy (argt, shot_types[GET_OBJ_VAL (weapon, 0)]);
	}

	num1 = GET_DEX (shooter) - GET_DEX (vict);
	if (num1 > 0)
		num1 = 60;
	if (num1 < 1)
		num1 = 40;


	for (i = 0; i < ripple; i++)
	{
		roll = number (1, 101);
		if (!FIGHTING (shooter))
		{
			break;
		}
		else if (GET_OBJ_VAL (weapon, 3) <= 0)
		{
			send_to_char ("E' scarica.", ch);
			break;
		}
		else if ((num1) >= roll)
		{

			sprintf (shot, "Colpisci $N con %s sparata da $p!", argt);
			act (shot, FALSE, shooter, weapon, vict, TO_CHAR);
			sprintf (shot, "$n ti colpisce con %s sparata da $p!", argt);
			act (shot, FALSE, shooter, weapon, vict, TO_VICT);
			sprintf (shot, "$n colpisce $N con %s sparata da $p!", argt);
			act (shot, FALSE, shooter, weapon, vict, TO_NOTVICT);
			GET_OBJ_VAL (weapon, 3) -= 1;

			dmg =
				dice (shot_damage[GET_OBJ_VAL (weapon, 0)][0],
					  shot_damage[GET_OBJ_VAL (weapon, 0)][1]) +
				(GET_LEVEL (shooter) / 2);

			damage (shooter, vict, dmg, TYPE_UNDEFINED);
		}
		else
		{
			sprintf (shot, "Spari %s a $N e manchi!", argt);
			act (shot, FALSE, shooter, weapon, vict, TO_CHAR);
			sprintf (shot, "$n ti spara %s e manca!", argt);
			act (shot, FALSE, shooter, weapon, vict, TO_VICT);
			sprintf (shot, "$n spara %s a $N e manca!", argt);
			act (shot, FALSE, shooter, weapon, vict, TO_NOTVICT);
			GET_OBJ_VAL (weapon, 3) -= 1;
		}

	}

	return (TRUE);
#endif
}


/* ********************************************************************
*  Special procedures for mobiles                                      *
******************************************************************** */

SPECIAL (guild_guard)
{
	int i;
	struct char_data *guard = (struct char_data *) me;
	const char *buf = "La guardia ti umilia e blocca la tua strada.\r\n";
	const char *buf2 = "La guardia umilia $n, e blocca la strada a $s .";

	if (!IS_MOVE (cmd) || AFF_FLAGGED (guard, AFF_BLIND))
		return (FALSE);

	if (GET_LEVEL (ch) >= LVL_IMMORT)
		return (FALSE);

	for (i = 0; guild_info[i][0] != -1; i++)
	{
		if ((IS_NPC (ch) || GET_CLASS (ch) != guild_info[i][0]) &&
			GET_ROOM_VNUM (IN_ROOM (ch)) == guild_info[i][1] &&
			cmd == guild_info[i][2])
		{
			send_to_char (buf, ch);
			act (buf2, FALSE, ch, 0, 0, TO_ROOM);
			return (TRUE);
		}
	}

	return (FALSE);
}


SPECIAL (puff)
{
	if (cmd)
		return (0);

	switch (number (0, 60))
	{
	case 0:
		do_say (ch, "My god!  It's full of stars!", 0, 0);
		return (1);
	case 1:
		do_say (ch, "How'd all those fish get up here?", 0, 0);
		return (1);
	case 2:
		do_say (ch, "I'm a very female dragon.", 0, 0);
		return (1);
	case 3:
		do_say (ch, "I've got a peaceful, easy feeling.", 0, 0);
		return (1);
	default:
		return (0);
	}
}

SPECIAL (vigile)
{
    struct char_data *tch, *vict;
    int att_type = 0, hit_roll = 0, to_hit = 0;

	if (cmd || !AWAKE (ch))
	{
		return (0);
	}

    if (FIGHTING (ch) && (FIGHTING (ch)->in_room == ch->in_room))
	{

		vict = FIGHTING (ch);
        switch (number (0, 6))
	    {
        case 1:
            do_say (ch, "Strunz mo te facc' o mazz tant!", 0, 0);
		    break;
        case 2:
            do_say (ch, "Scurnacchia', mo te spezz e' corn!", 0, 0);
		    break;
        case 3:
            do_say (ch, "Mo te facc zumpa' tutt e' rient a' vocc.", 0, 0);
		    break;
        default:
            break;
        }

        if (GET_POS (ch) == POS_FIGHTING)
		{
            if ( ( IS_MAGIC_USER(vict) || IS_CLERIC(vict) || IS_LINKER(vict) || IS_PSIONIC(vict) ) &&
                GET_POS(vict) >= POS_FIGHTING )
            {
                if (number(1,10) == 1)
                {
                    do_mob_bash (ch, vict);
                    return (TRUE);
                }
            }
            
            if (number (1, 6) < 6)
            {
                /* Uso l'arma da fuoco se ne ha una in mano */
                if ( do_mob_fireweapon(ch,vict) )
                {
                    return (TRUE);
                }
            }

			att_type = number (1, 40);

			hit_roll = number (1, 100) + GET_STR (ch);
			to_hit = (100 - (int) (100 * GET_LEVEL (ch) / 250));
			if (GET_LEVEL (vict) >= LVL_IMMORT)
				hit_roll = 0;

			switch (att_type)
			{
			case 1:
			case 2:
			case 3:
			case 4:
            case 5:
                do_mob_kick (ch, vict);
                return (TRUE);
				break;
			case 6:
			case 7:
            case 8: 
            case 9: 
            case 10:
                do_mob_bash (ch, vict);
                return (TRUE);
				break;
            case 15:
			    do_mob_disarm (ch, vict);
                return (TRUE);
				break;
			default:
                hit (ch, vict, TYPE_UNDEFINED);
				break;
			}

		}
        else 
        {
            act ("$n scatta all'attacco!", TRUE, ch, 0, 0, TO_ROOM);
			GET_POS (ch) = POS_FIGHTING;
        }
        return FALSE;

    }
    else
    {
        switch (number (0, 20))
	    {
    	case 0:
	    	do_say (ch, "Acca nissun e' fess.", 0, 0);
		    break;
    	case 1:
	    	do_say (ch, "O Gesummaria!", 0, 0);
		    break;
    	case 2:
	    	do_say (ch, "Oi maronnna oi maro'.. he che e' succiess'??", 0, 0);
		    break;
    	case 3:
	    	do_say (ch, "Uueeeeee ma che're tutta 'sta ammuina??", 0, 0);
		    break;
    	case 4:
	    	do_say (ch,
		    "Maro' e che burdell' 'e traffico. Acca... si rischia 'o patatrac !!",0, 0);
		    break;
	    case 5:
		    do_say (ch,
				"Ahhhhh... lassa fa' a maronna..... L'ammuina e' fernuta.", 0,0);
		    break;
        case 6:
		    do_say (ch,"Che iurnata che e' schiarata.......", 0,0);
		    break;
        case 7:
	    	do_say (ch,"Mo quasi quasi me vaco a piglia' o' caffe'", 0,0);
		    break;

        default:
		    break;
	    }

        for (tch = world[ch->in_room].people; tch; tch = tch->next_in_room)
	    {
    		sprintf (buf, "%s", GET_NAME (tch));
    
	    	if (CAN_SEE (ch, tch) && (GET_LEVEL (tch) > 30) && !IS_NPC (tch))
		    {
    
	    		if (dice (1, 6) == 1)
		    	{
			    	if (GET_ALIGNMENT (tch) <= -250)
				    {
                        sprintf(buf,"$n dice, 'Azz ca ce sta' pure $N, che %s e niente'",
                            (GET_SEX (ch) == SEX_FEMALE ? "la femmena" : "l'omm") );
    					act (buf, FALSE, ch, 0,tch, TO_ROOM);
	    				act ("$n ti guarda freddamente negli occhi!", TRUE, ch, 0,
		    				 tch, TO_VICT);
			        	act ("$n guarda freddamente negli occhi di $N", TRUE, ch,
						 0, tch, TO_NOTVICT);
    				}
                    else if (GET_ALIGNMENT (tch) <= 250)
		    		{
			    		act ("$n dice, 'Vir' o' ciel' che te mena $N amabbile.'", FALSE, ch, 0,
				    		 tch, TO_ROOM);
					    act ("$n ti saluta allegramente!", TRUE, ch, 0,
						     tch, TO_VICT);
    					act ("$n saluta allegramente $N", TRUE, ch, 0,
	    					 tch, TO_NOTVICT);
		    		}
			    	else
				    {
    					act ("$n dice, 'Vir' o' ciel' che te mena $N amabbile.'", FALSE, ch, 0,
	    					 tch, TO_ROOM);
		    			act ("$n si inchina solennemente davanti a te!", TRUE, ch,
			    			 0, tch, TO_VICT);
				    	act ("$n si inchina solennemente davanti a $N", TRUE, ch,
					    	 0, tch, TO_NOTVICT);
    				}
    
	    			return (TRUE);
		    	}
		    }   
	    }
    }
    return FALSE;
}



SPECIAL (fido)
{

	struct obj_data *i, *temp, *next_obj;

	if (cmd || !AWAKE (ch))
		return (FALSE);

	for (i = world[ch->in_room].contents; i; i = i->next_content)
	{
		if (IS_CORPSE (i))
		{
			act ("$n divora in modo selvaggio un cadavere.", FALSE, ch, 0, 0,
				 TO_ROOM);
			for (temp = i->contains; temp; temp = next_obj)
			{
				next_obj = temp->next_content;
				obj_from_obj (temp);
				obj_to_room (temp, ch->in_room);
			}
			extract_obj (i);
			return (TRUE);
		}
	}
	return (FALSE);
}



SPECIAL (janitor)
{
	struct obj_data *i;

	if (cmd || !AWAKE (ch))
		return (FALSE);

	for (i = world[ch->in_room].contents; i; i = i->next_content)
	{
		if (!CAN_WEAR (i, ITEM_WEAR_TAKE))
			continue;
		if (GET_OBJ_TYPE (i) != ITEM_DRINKCON && GET_OBJ_COST (i) >= 15)
			continue;
		act ("$n raccoglie della spazzatura.", FALSE, ch, 0, 0, TO_ROOM);
		obj_from_room (i);
		obj_to_char (i, ch);
		return (TRUE);
	}

	return (FALSE);
}


SPECIAL (cityguard)
{
	struct char_data *tch, *evil;
	int max_evil;

	if (cmd || !AWAKE (ch) || FIGHTING (ch))
		return (FALSE);

	max_evil = 1000;
	evil = 0;

	for (tch = world[ch->in_room].people; tch; tch = tch->next_in_room)
	{
		sprintf (buf, "%s", GET_NAME (tch));

		if (CAN_SEE (ch, tch) && (GET_LEVEL (tch) > 30) && !IS_NPC (tch))
		{

			if (dice (1, 6) == 1)
			{
				if (GET_ALIGNMENT (tch) <= -250)
				{
					act ("$n dice, 'Non sei il benvenuto, $N!'", FALSE, ch, 0,
						 tch, TO_ROOM);
					act ("$n ti guarda freddamente negli occhi!", TRUE, ch, 0,
						 tch, TO_VICT);
					act ("$n guarda freddamente negli occhi di $N", TRUE, ch,
						 0, tch, TO_NOTVICT);
				}
				else if (GET_ALIGNMENT (tch) <= 250)
				{
					act ("$n dice, 'Salve $N!'", FALSE, ch, 0, tch, TO_ROOM);
					act ("$n ti saluta allegramente!", TRUE, ch, 0,
						 tch, TO_VICT);
					act ("$n saluta allegramente $N", TRUE, ch, 0,
						 tch, TO_NOTVICT);
				}
				else
				{
					act ("$n dice, 'Onore e Gloria a $N'", FALSE, ch, 0,
						 tch, TO_ROOM);
					act ("$n si inchina solennemente davanti a te!", TRUE, ch,
						 0, tch, TO_VICT);
					act ("$n si inchina solennemente davanti a $N", TRUE, ch,
						 0, tch, TO_NOTVICT);
				}

				return (TRUE);
			}
		}
	}

	for (tch = world[ch->in_room].people; tch; tch = tch->next_in_room)
	{
		if (!IS_NPC (tch) && CAN_SEE (ch, tch)
			&& PLR_FLAGGED (tch, PLR_KILLER))
		{
			act ("$n urla 'HEY!!!  Sei uno dei PLAYER KILLERS!!!!!!'", FALSE,
				 ch, 0, 0, TO_ROOM);
			hit (ch, tch, TYPE_UNDEFINED);
			return (TRUE);
		}
	}

	for (tch = world[ch->in_room].people; tch; tch = tch->next_in_room)
	{
		if (!IS_NPC (tch) && CAN_SEE (ch, tch)
			&& PLR_FLAGGED (tch, PLR_THIEF))
		{
			act ("$n urla 'HEY!!!  Sei uno dei PLAYER THIEVES!!!!!!'", FALSE,
				 ch, 0, 0, TO_ROOM);
			hit (ch, tch, TYPE_UNDEFINED);
			return (TRUE);
		}
	}

	for (tch = world[ch->in_room].people; tch; tch = tch->next_in_room)
	{
		if (CAN_SEE (ch, tch) && FIGHTING (tch))
		{
			if ((GET_ALIGNMENT (tch) < max_evil) &&
				(IS_NPC (tch) || IS_NPC (FIGHTING (tch))))
			{
				max_evil = GET_ALIGNMENT (tch);
				evil = tch;
			}
		}
	}

	if (evil && (GET_ALIGNMENT (FIGHTING (evil)) >= 0))
	{
		act
			("$n urla 'PROTTEGGETE GLI INNOCENTI!  BANZAI!  CARICA!  ARARARAGGGHH!'",
			 FALSE, ch, 0, 0, TO_ROOM);
		hit (ch, evil, TYPE_UNDEFINED);
		return (TRUE);
	}

	return (FALSE);
}


SPECIAL (lvl_aggro)
{
	struct char_data *tch, *max;
	int level;

	if (cmd || !AWAKE (ch) || FIGHTING (ch))
		return (FALSE);


	max = 0;
	level = 0;

	for (tch = world[ch->in_room].people; tch; tch = tch->next_in_room)
	{
		if (!IS_NPC (tch) && CAN_SEE (ch, tch))
		{
			if ((level <= GET_LEVEL (tch)) && GET_LEVEL (tch) < LVL_IMMORT)
			{
				level = GET_LEVEL (tch);
				max = tch;
			}

		}
	}

	if (max != 0)
	{
		act ("$n urla 'GROAAARRR!!!!!!'", FALSE, ch, 0, 0, TO_ROOM);
		hit (ch, max, TYPE_UNDEFINED);
		return (TRUE);
	}

	return (FALSE);
}

SPECIAL (guard_tower)
{
	   
    struct char_data *vict;
	
	if (cmd || GET_POS (ch) != POS_FIGHTING)
		return (FALSE);

	/* pseudo-randomly choose someone in the room who is fighting me */
	for (vict = world[ch->in_room].people; vict;
		 vict = vict->next_in_room)
	{
		if (FIGHTING (vict) == ch && !number (0, 4))
		{
			break;
		}
	}

	/* if I didn't pick any of those, then just slam the guy I'm fighting */
	if (vict == NULL && IN_ROOM (FIGHTING (ch)) == IN_ROOM (ch))
	{
		vict = FIGHTING (ch);
	}

	/* Hm...didn't pick anyone...I'll wait a round. */
	if (vict == NULL)
	{
		return (FALSE);
	}
    
    return do_mob_fireweapon(ch,vict);

}


#define PET_PRICE(pet) (GET_LEVEL(pet) * 300)

SPECIAL (pet_shops)
{
	char buf[MAX_STRING_LENGTH], pet_name[256];
	room_rnum pet_room;
	struct char_data *pet;

	pet_room = ch->in_room + 1;

	if (CMD_IS ("list"))
	{
		send_to_char ("Animali dispinibili:\r\n", ch);
		for (pet = world[pet_room].people; pet; pet = pet->next_in_room)
		{
			if (!IS_NPC (pet))
				continue;
			sprintf (buf, "%8d - %s\r\n", PET_PRICE (pet), GET_NAME (pet));
			send_to_char (buf, ch);
		}
		return (TRUE);
	}
	else if (CMD_IS ("buy"))
	{

		two_arguments (argument, buf, pet_name);

		if (!(pet = get_char_room (buf, NULL, pet_room)) || !IS_NPC (pet))
		{
			send_to_char ("There is no such pet!\r\n", ch);
			return (TRUE);
		}
		if (GET_GOLD (ch) < PET_PRICE (pet))
		{
			send_to_char ("You don't have enough gold!\r\n", ch);
			return (TRUE);
		}
		GET_GOLD (ch) -= PET_PRICE (pet);

		pet = read_mobile (GET_MOB_RNUM (pet), REAL);
		GET_EXP (pet) = 0;
		SET_BIT (AFF_FLAGS (pet), AFF_CHARM);

		if (*pet_name)
		{
			sprintf (buf, "%s %s", pet->player.name, pet_name);
			/* free(pet->player.name); don't free the prototype! */
			pet->player.name = str_dup (buf);

			sprintf (buf,
					 "%sA small sign on a chain around the neck says 'My name is %s'\r\n",
					 pet->player.description, pet_name);
			/* free(pet->player.description); don't free the prototype! */
			pet->player.description = str_dup (buf);
		}
		char_to_room (pet, ch->in_room);
		add_follower (pet, ch);

		/* Be certain that pets can't get/carry/use/wield/wear items */
		IS_CARRYING_W (pet) = 1000;
		IS_CARRYING_N (pet) = 100;

		send_to_char ("May you enjoy your pet.\r\n", ch);
		act ("$n buys $N as a pet.", FALSE, ch, 0, pet, TO_ROOM);

		return (1);
	}
	/* All commands except list and buy */
	return (0);
}



/* ********************************************************************
*  Special procedures for objects                                     *
******************************************************************** */


SPECIAL (bank)
{
	int amount;

	if (CMD_IS ("balance"))
	{
		if (GET_BANK_GOLD (ch) > 0)
			sprintf (buf, "Your current balance is %d coins.\r\n",
					 GET_BANK_GOLD (ch));
		else
			sprintf (buf, "You currently have no money deposited.\r\n");
		send_to_char (buf, ch);
		return (1);
	}
	else if (CMD_IS ("deposit"))
	{
		if ((amount = atoi (argument)) <= 0)
		{
			send_to_char ("How much do you want to deposit?\r\n", ch);
			return (1);
		}
		if (GET_GOLD (ch) < amount)
		{
			send_to_char ("You don't have that many coins!\r\n", ch);
			return (1);
		}
		GET_GOLD (ch) -= amount;
		GET_BANK_GOLD (ch) += amount;
		sprintf (buf, "You deposit %d coins.\r\n", amount);
		send_to_char (buf, ch);
		act ("$n makes a bank transaction.", TRUE, ch, 0, FALSE, TO_ROOM);
		return (1);
	}
	else if (CMD_IS ("withdraw"))
	{
		if ((amount = atoi (argument)) <= 0)
		{
			send_to_char ("How much do you want to withdraw?\r\n", ch);
			return (1);
		}
		if (GET_BANK_GOLD (ch) < amount)
		{
			send_to_char ("You don't have that many coins deposited!\r\n",
						  ch);
			return (1);
		}
		GET_GOLD (ch) += amount;
		GET_BANK_GOLD (ch) -= amount;
		sprintf (buf, "You withdraw %d coins.\r\n", amount);
		send_to_char (buf, ch);
		act ("$n makes a bank transaction.", TRUE, ch, 0, FALSE, TO_ROOM);
		return (1);
	}
	else
		return (0);
}


/* ***************************************
*  Special procedures for Meo Home       *
**************************************** */

SPECIAL (massage)
{
	struct char_data *tmpmobile;
	mob_vnum number;
	mob_rnum r_num;

	if (CMD_IS ("use") && strcmp (argument, " nera") == 0)
	{
		number = 1301;
		r_num = real_mobile (number);
		tmpmobile = read_mobile (r_num, REAL);
		tmpch = ch;
		char_to_room (tmpmobile, ch->in_room);
		send_to_char
			("ti siedi nella poltrona nera e subito ti rilassi in una posizione di riposo.\r\n",
			 ch);
		GET_POS (ch) = POS_RESTING;
		send_to_room ("Thyala entra nella sala.\r\n", ch->in_room);
		send_to_char ("Thyala viene verso di te sorridendoti.\r\n", ch);
		send_to_char
			("Thyala reclina completamente lo schienale della tua poltrona.\r\n",
			 ch);
		return (1);
	}
	return (0);
}

SPECIAL (mob_massage)
{

	if (cmd || !AWAKE (ch))
	{
		return (0);
	}
	++mastime;

	switch (mastime)
	{
	case 1:
		send_to_char
			("Thyala si mette dietro il tuo lettino ed inizia a massagiarti lentamente"
			 " le tempie. I suoi lunghi capelli neri ti sfiorano il viso.\r\n",
			 tmpch);
		return (1);
	case 2:
		send_to_char
			("Thyala scende con le mani a sciogliere dolcemente i muscoli del tuo collo"
			 " comunicandoti una sensazione di rilassamento e abbandono. I suoi seni sono"
			 " a pochi centrimetri dal tuo viso ed ondeggiano dolcemente in armonia con"
			 " i suoi movimenti\r\n", tmpch);
		return (1);
	case 3:
		send_to_char
			("Thyala ti si mette di fianco e le sue dita passano lungo il percorso del"
			 " tuo sterno con una leggera pressione che si trasmette fino al tuo ventre.\r\n",
			 tmpch);
		return (1);
	case 4:
		send_to_char
			("Le mani di Thyala scendono sulla tua pancia e la pressione delle sue dita"
			 " aumenta nel sollecitare i tuoi addominali, ma poi il tocco diventa quasi"
			 " carezzevole all'altezza de i tuoi fianchi e del tuo bacino.\r\n",
			 tmpch);
		return (1);
	case 5:
		send_to_char
			("Thyala si sposta di fronte, ti solleva una gamba e poggia il tuo piede"
			 " sul suo seno caldo e morbido. Inizia a massaggiarti il polpaccio risalendo"
			 " verso il tuo interno coscia. Non riesci a trattenere un fremito.\r\n",
			 tmpch);
		return (1);
	case 6:
		send_to_char
			("Thyala ti fa girare a pancia sotto, sale a cavalcioni sul tuo lettino ed"
			 " inizia a massaggiarti molto dolcemente la schiena.\r\nQuando si china per"
			 " stirarti la spina dorsale senti le sue tette strofinarsi contro i tuoi glutei.\r\n",
			 tmpch);
		return (1);
	case 7:
		send_to_char
			("Il massaggio e' finito, Thyala ti ringrazia e tu alzi gli occhi a guardarla. \r\n",
			 tmpch);
		look_at_char (ch, tmpch);
		return (1);
	case 8:
		send_to_char
			("Thyala ti sorride, si china a sfiorarti le labbra e se ne va. \r\n",
			 tmpch);
		extract_char (ch);
		mastime = 0;
		return (1);

	default:
		return (0);
	}

	return (0);
}


SPECIAL (massage_1)
{
	struct char_data *tmpmobile;
	mob_vnum number;
	mob_rnum r_num;

	if (CMD_IS ("use") && strcmp (argument, " bordeaux") == 0)
	{
		number = 1300;
		r_num = real_mobile (number);
		tmpmobile = read_mobile (r_num, REAL);
		tmpch1 = ch;
		char_to_room (tmpmobile, ch->in_room);
		send_to_char
			("ti siedi nella poltrona bodeaux e subito ti rilassi in una posizione di riposo.\r\n",
			 ch);
		GET_POS (ch) = POS_RESTING;
		send_to_room ("Khyho entra nella sala.\r\n", ch->in_room);
		send_to_char ("Khyho viene verso di te sorridendoti.\r\n", ch);
		send_to_char
			("Khyho reclina completamente lo schienale della tua poltrona.\r\n",
			 ch);
		return (1);
	}
	return (0);
}

SPECIAL (mob_massage_1)
{

	if (cmd || !AWAKE (ch))
	{
		return (0);
	}
	++mastime1;

	switch (mastime1)
	{
	case 1:
		send_to_char
			("Khyho si mette dietro il tuo lettino ed inizia a massagiarti lenamente"
			 " le tempie.\r\n", tmpch1);
		return (1);
	case 2:
		send_to_char
			("Khyho scende con le mani a sciogliere dolcemente i muscoli del tuo"
			 " collo comunicandoti una sensazione di rilassamento e abbandono.\r\n",
			 tmpch1);
		return (1);
	case 3:
		send_to_char
			("Khyho ti si mette di fianco e le sue dita passano lungo il percorso"
			 " del tuo sterno con una leggera pressione che si trasmette fino ai"
			 " tuoi seni appena sfiorati dalle palme delle sue mani.\r\n",
			 tmpch1);
		return (1);
	case 4:
		send_to_char
			("Le mani di Khyho scendono sulla tua pancia e la pressione delle sue"
			 " dita aumenta nel sollecitare i tuoi addominali, ma poi il tocco diventa"
			 " quasi carezzevole all'altezza de i tuoi fianchi e del tuo bacino.\r\n",
			 tmpch1);
		return (1);
	case 5:
		send_to_char
			("Khyho si sposta di fronte, ti solleva una gamba e poggia il tuo piede"
			 " sul suo petto caldo e muscoloso. Inizia a massaggiarti il polpaccio risalendo"
			 " verso il tuo interno coscia. Non riesci a trattenere un fremito.\r\n",
			 tmpch1);
		return (1);
	case 6:
		send_to_char
			("Khyho ti fa girare a pancia sotto, sale a cavalcioni sul tuo lettino ed"
			 " inizia a massaggiarti molto dolcemente la schiena.\r\nQuando si china per"
			 " stirarti la spina dorsale senti il suo torace entrare in contatto con i tuoi glutei.\r\n",
			 tmpch1);
		return (1);
	case 7:
		send_to_char
			("Il massaggio e' finito, Khyho ti ringrazia e tu alzi gli occhi a guardarlo . \r\n",
			 tmpch1);
		look_at_char (ch, tmpch1);
		return (1);
	case 8:
		send_to_char ("Khyho ti sorride e se ne va. \r\n", tmpch1);
		extract_char (ch);
		mastime1 = 0;
		return (1);
	default:
		return (0);
	}

	return (0);
}
SPECIAL (tac_machine)
{

	if (CMD_IS ("use") && strcmp (argument, " tac") == 0)
	{
		sprintf (buf, "Risultato della Tomografia Assiale:\r\n");
		sprintf (buf + strlen (buf), "Nome: %s\r\n", GET_NAME (ch));
		send_to_char (buf, ch);
		if (!IS_NPC (ch))
		{
			sprintf (buf,
					 "Hai %d anni, %d mesi, %d giorni and %d ore.\r\n",
					 age (ch)->year,
					 age (ch)->month, age (ch)->day, age (ch)->hours);
			send_to_char (buf, ch);
		}
		sprintf (buf, "Altezza %d cm, Peso %d pounds\r\n",
				 GET_HEIGHT (ch), GET_WEIGHT (ch));
		sprintf (buf + strlen (buf), "Livello: %d, Hp: %d, RAM: %d\r\n",
				 GET_LEVEL (ch), GET_HIT (ch), GET_MANA (ch));
		sprintf (buf + strlen (buf), "AC: %d, Hit: %d, Dam: %d\r\n",
				 compute_armor_class (ch), GET_HITROLL (ch),
				 GET_DAMROLL (ch));
		sprintf (buf + strlen (buf),
				 "Str: %d/%d, Int: %d, Wis: %d, Dex: %d, Con: %d, Cha: %d\r\n\r\n",
				 GET_STR (ch), GET_ADD (ch), GET_INT (ch),
				 GET_WIS (ch), GET_DEX (ch), GET_CON (ch), GET_CHA (ch));
		sprintf (buf + strlen (buf),
				 "Grazie per avere usato la nostra TAC, %s.\r\n",
				 GET_NAME (ch));
		send_to_char (buf, ch);
		return (1);
	}
	return (0);
}

SPECIAL (newbie_guide)
{
	static char tour_path[] =
		"W530AHNO2B2C10D210E2333F0G22M030L22K030J22P0Q1110R223333S3T111110014Z.";

	static char *path;
	static int index;
	int countfol;
	static bool move = FALSE;
	struct follow_type *fol;

	countfol = 0;
	if (!move)
	{
		fol = ch->followers;
		for (fol = ch->followers; fol; fol = fol->next)
			++countfol;

		if (countfol > 0)
		{
			move = TRUE;
			path = tour_path;
			index = 0;
			act
				("$n dice 'Sta per iniziare il tour per i Newbie.\r\n Chi e' interessato mi segua.'",
				 FALSE, ch, 0, 0, TO_ROOM);
		}
	}

	if (cmd || !move || (GET_POS (ch) < POS_RESTING) ||
		(GET_POS (ch) == POS_FIGHTING))
		return FALSE;

    /* check ch e' realmente un NPC */
    if (!IS_NPC(ch))
        return FALSE;


	switch (path[index])
	{
	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
		perform_move (ch, path[index] - '0', 1);
		break;
	case 'W':
		GET_POS (ch) = POS_STANDING;
		act
			("$n si alza e dice: 'Prima andiamo ai giardinetti e parliamo un po'"
			 " delle classi e delle razze di Mclandia e poi facciamo un piccolo giro della citta'.",
			 FALSE, ch, 0, 0, TO_ROOM);
		break;
	case 'Z':
		act ("$n si siede e riposa in attesa del prossimo newbie da aiutare.",
			 FALSE, ch, 0, 0, TO_ROOM);
		GET_POS (ch) = POS_RESTING;
		break;
	case 'M':
		act
			("$n dice: 'Qui siamo davanti alla base segreta dei Virus Writer.\r\n"
			 "Li' dentro potrete trovare il piu' grande programmatore di Mclandia"
			 " pronto ad insegnarvi tutte le abilita' immaginabili, fino ad aprire"
			 " porte telnet su qualunque locazione o del mondo conosciuto!!'",
			 FALSE, ch, 0, 0, TO_ROOM);
		break;
	case 'L':
		act
			("$n dice: 'Eccoci davanti alla caserma.\r\nQuesto e' il tempio dei"
			 " guerrafondai, i piu' forti guerrieri del mondo si allenano qui dentro"
			 " affinando le loro abilita' ed il loro addestramento.'",
			 FALSE, ch, 0, 0, TO_ROOM);
		break;
	case 'K':
		act
			("$n dice: 'Siamo ora davanti al circolo dei pensatori. Li' dentro si"
			 " medita fino ad affinare i propri poteri mentali al punto di arrivare"
			 " a delle vette impensabili tanto da poter riuscire a teletrasportare"
			 " il proprio corpo in ogni luogo o anche distruggere il cervello di un"
			 " nemico con una potente scarica di energia mentale.'",
			 FALSE, ch, 0, 0, TO_ROOM);
		break;
	case 'J':
		act
			("$n dice: 'Ed eccoci davanti alla scuola per linker. Qui si impara un"
			 " po' di tutto. Attaccare i sistemi. Aggiustare i guasti e combattere"
			 " i nemici. Ideale per chi non ha ancora molta familiarita' con i mondi virtuali.'",
			 FALSE, ch, 0, 0, TO_ROOM);
		break;
	case 'A':
		act
			("$n dice: 'In questo mondo potete scegliere di essere:\r\n Un Hacker,"
			 " un Virus Wiriter un Amministratore di rete, un guerrafondaio un linker,"
			 " uno psionico o un esperto di arti marziali'.",
			 FALSE, ch, 0, 0, TO_ROOM);
		break;
	case 'H':
		act
			("$n dice: 'Un Hacker e' esperto nella violazione di sistemi quindi ha"
			 " tutte le abilita' paragonabili a un ladro come aprire accessi rendersi"
			 " invisibile etc.\r\n Un Virus W. e' un mago della programmazione quindi"
			 " molto potente in attacchi offensivi.\r\n Un Amministratore di rete e'"
			 " un esperto che puo' non solo attaccare ma anche riparare e curare i danni...'",
			 FALSE, ch, 0, 0, TO_ROOM);
		break;
	case 'N':
		act
			("$n dice: 'Uno Psionico usa i suoi poteri mentali per combattere e"
			 " difendersi. Gli Esperti di arti marziali e i Guerrafondai sono specialisti"
			 " del combattimento corpo a corpo mentre infine il Linker e' un tipo molto"
			 " flessibile che ha le abilita' base un po' di tutti ma non eccelle in nulla.'",
			 FALSE, ch, 0, 0, TO_ROOM);
		break;
	case 'O':
		act
			("$n dice: 'Per quanto riguarda le razze:\r\nI Motociclisti sono ottimi per i"
			 " combattimenti corpo a corpo, hanno un bonus sulla forza e destrezza.\r\n"
			 "I Cyborg sono molto intelligenti e hanno anche un bonus sulla destrezza grazie"
			 " ai loro impianti meccanici.\r\nGli alieni invece sono saggi e carismatici ma"
			 " hanno poca forza e sono di costituzione debole. Gli umani infine sono ottimi"
			 " per tutto ed hanno una salute robusta.'",
			 FALSE, ch, 0, 0, TO_ROOM);
		break;
	case 'B':
		act
			("$n dice: 'Qui siamo alla piazzetta principale di Mclandia.\r\nPer bere dalla"
			 " fontana potete dare il comando drink fontana a est c'e' la reception.'",
			 FALSE, ch, 0, 0, TO_ROOM);
		break;
	case 'C':
		act
			("$n dice: 'Qui siamo a piazza linker.\r\nA Ovest c'e' V.le Universita' dove"
			 " potrete allenarvi nelle vostre abilita' e passare di livello.\r\nA Est ci"
			 " sono i negozi.\r\nOra ci diamo uno sguardo.'",
			 FALSE, ch, 0, 0, TO_ROOM);
		break;
	case 'D':
		act
			("$n dice: 'Qui c'e' l'emporio.\r\nNei negozi con il comando list potete vedere"
			 " che cosa si vende mentre per comprare un articolo dovete usare il comando buy.\r\n"
			 "Qui in particolare vi conviene comprare una borsa e una lanterna che potrete"
			 " accendere con il comando hold lanterna.'",
			 FALSE, ch, 0, 0, TO_ROOM);
		break;
	case 'E':
		act
			("$n dice: 'Qui si compra da mangiare.\r\nPotete avere una bellissima fiorentina"
			 " con il comando buy bistecca che potrete mangiare con il comando eat.\r\n"
			 "Dall'altra parte della strada c'e' il negozio di abbigliamento dove potete"
			 " comprare vestiti e bigiotteria da indossare con il comando wear.\r\nOra torniamo"
			 " alla piazza e poi vi faccio vedere dove sono le vostre facolta'.'",
			 FALSE, ch, 0, 0, TO_ROOM);
		break;
	case 'F':
		act
			("$n dice: 'Eccoci nella via delle Universita'.\r\nPer imparare nuove abilita'"
			 " e salire di livello una volta raggiunto il punteggio necessario potete recarvi"
			 " dal vostro professore e usare i comandi prac e gain.'",
			 FALSE, ch, 0, 0, TO_ROOM);
		break;
	case 'G':
		act
			("$n dice: 'Qui siamo davanti la facolta' di informatica.\r\n A nord si trova il"
			 " professore che tiene i corsi per gli amministratori di sistema mentre a est"
			 " c'e' il laboratorio clandestino degli Hacker..'",
			 FALSE, ch, 0, 0, TO_ROOM);
		break;
	case '.':
		move = FALSE;
		break;
	case 'P':
		act
			("$n dice: 'Ed eccoci infine davanti alla palestra di arti marziali.\r\n"
			 "Qui si impara a combattere a mani nude ed i risultati pare siano straordinari!!'",
			 FALSE, ch, 0, 0, TO_ROOM);
		break;
	case 'Q':
		act
			("$n dice: 'Ora torniamo alla piazzeta principale cosi' vedrete la strada che si"
			 " fa da li' per arrivare alla zona dedicata ai newbie.'",
			 FALSE, ch, 0, 0, TO_ROOM);
		break;
	case 'R':
		act
			("$n dice: 'Eccoci di nuovo in piazzetta.\r\nPer andare alla zona dedicata ai"
			 " newbie da qui bisogna andare due volte a sud e poi sempre a ovest fino"
			 " all'entrata di Harlem.\r\n Andiamo!!'",
			 FALSE, ch, 0, 0, TO_ROOM);
		break;
	case 'S':
		act
			("$n dice: 'Questa e' l'entrata alla zona per i Newbie.\r\nQuesta strada"
			 " che si snoda verso ovest e' piena di locali famosi. Potete entrare nei"
			 " locali e combattere contro chiunque trovate, ma fate molta attenzione."
			 " Ai primi livelli sarete al massimo in grado di uccidere degli sprovveduti"
			 " ed inermi spettatori. Man mano che crescete potrete misurarvi con i mob piu'"
			 " forti. Usate il comando consider per farvi un'idea della forza del vostro avversario.'",
			 FALSE, ch, 0, 0, TO_ROOM);
		break;
	case 'T':
		act
			("$n dice: 'Il tour e' finito ed io sono stanchissimo.\r\nSmettete di"
			 " seguirmi con il comando follow vostronome.\r\n Da qui in poi dipende"
			 " tutto da voi.\r\n In bocca al lupo e....\r\nBuona caccia!!'",
			 FALSE, ch, 0, 0, TO_ROOM);
		act ("$n ti saluta allegramente agtiando la mano.", FALSE, ch, 0, 0,
			 TO_ROOM);
		break;
	}
	index++;
	return FALSE;
}
