
/* ************************************************************************
*   File: class.c                                       Part of CircleMUD *
*  Usage: Source file for class-specific code                             *
*                                                                         *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  Copyright (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */

/*
 * This file attempts to concentrate most of the code which must be changed
 * in order for new classes to be added.  If you're adding a new class,
 * you should go through this entire file from beginning to end and add
 * the appropriate new special cases for your new class.
 */



#include "conf.h"
#include "sysdep.h"

#include "structs.h"
#include "db.h"
#include "utils.h"
#include "spells.h"
#include "interpreter.h"
#include "constants.h"

extern int siteok_everyone;

/* local functions */
int parse_class (char arg);
long find_class_bitvector (char arg);
byte saving_throw_eqs (int class_num, int type, int level);
int thaco (int class_num, int level);
void roll_real_abils (struct char_data *ch);
void do_start (struct char_data *ch);
int backstab_mult (int level);
int saving_throw_eq (int level, int ts_min_lev, int ts_max_lev);
int thaco_eq (int level, int th_min_lev, int th_mid_lev, int th_max_lev);
int invalid_class (struct char_data *ch, struct obj_data *obj);
int level_exp (int chclass, int level);
const char *title_male (int chclass, int level);
const char *title_female (int chclass, int level);

/* Names first */

const char *class_abbrevs[] = {
	"Vw",
	"Sa",
	"Ha",
	"Wa",
	"Ma",
	"Li",
	"Ps",
	"\n"
};


const char *pc_class_types[] = {
	"Virus Writer",
	"Amministratore di Rete",
	"Hacker",
	"Guerrafondaio",
	"Martial Artist",
	"Linker",
	"Psionico",
	"\n"
};

const char *race_abbrevs[] = {
	"Uma",
	"Cyb",
	"Mot",
	"Ali",
	"\n"
};

const char *pc_race_types[] = {
	"Umano",
	"Cyborg",
	"Motociclista",
	"Alieno",
	"\n"
};


/* The menu for choosing a class in interpreter.c: */
const char *class_menu =
	"\r\n"
	"Scegli una Classe:\r\n"
	"  [V]irus Writer\r\n"
	"  [A]mministratore di Rete\r\n"
	"  [H]acker\r\n"
	"  [G]uerrafondaio\r\n"
	"  [M]artial Arts\r\n" "  [L]inker simplex\r\n" "  [P]sionico\r\n";

/* The menu for choosing a race in interpreter.c: */
const char *race_menu =
	"\r\n"
	"Di che razza sei?:\r\n"
	"  [U]mano\r\n" "  [C]yborg\r\n" "  [M]otociclista\r\n" "  [A]lieno\r\n";

/*
 * The code to interpret a class letter -- used in interpreter.c when a
 * new character is selecting a class and by 'set class' in act.wizard.c.
 */

int
parse_class (char arg)
{
	arg = LOWER (arg);

	switch (arg)
	{
	case 'v':
		return CLASS_MAGIC_USER;
	case 'a':
		return CLASS_CLERIC;
	case 'h':
		return CLASS_THIEF;
	case 'g':
		return CLASS_WARRIOR;
	case 'm':
		return CLASS_MARTIAL;
	case 'l':
		return CLASS_LINKER;
	case 'p':
		return CLASS_PSIONIC;
	default:
		return CLASS_UNDEFINED;
	}
}

/*                                                                   
 * The code to interpret a race letter (used in interpreter.c when a 
 * new character is selecting a race).                               
 */
int
parse_race (char arg)
{
	arg = LOWER (arg);

	switch (arg)
	{
	case 'u':
		return RACE_UMANO;
		break;
	case 'c':
		return RACE_CYBORG;
		break;
	case 'm':
		return RACE_MOTOCICLISTA;
		break;
	case 'a':
		return RACE_ALIENO;
		break;
	default:
		return RACE_UNDEFINED;
		break;
	}
}


/*
 * bitvectors (i.e., powers of two) for each class, mainly for use in
 * do_who and do_users.  Add new classes at the end so that all classes
 * use sequential powers of two (1 << 0, 1 << 1, 1 << 2, 1 << 3, 1 << 4,
 * 1 << 5, etc.
 */

long
find_class_bitvector (char arg)
{
	arg = LOWER (arg);

	switch (arg)
	{
	case 'v':
		return (1 << CLASS_MAGIC_USER);
	case 'a':
		return (1 << CLASS_CLERIC);
	case 'h':
		return (1 << CLASS_THIEF);
	case 'g':
		return (1 << CLASS_WARRIOR);
	case 'm':
		return (1 << CLASS_MARTIAL);
	case 'l':
		return (1 << CLASS_LINKER);
	case 'p':
		return (1 << CLASS_PSIONIC);
	default:
		return 0;
	}
}

long
find_race_bitvector (char arg)
{
	arg = LOWER (arg);

	switch (arg)
	{
	case 'u':
		return (1 << RACE_UMANO);
	case 'c':
		return (1 << RACE_CYBORG);
	case 'm':
		return (1 << RACE_MOTOCICLISTA);
	case 'a':
		return (1 << RACE_ALIENO);
	default:
		return 0;
	}
}

/*******************************************************************
 * ts_min_lev = Tiro salvezza a livello 1
 * ts_max_lev = tiro salvezza a livello LVL_IMMORT-1
 *******************************************************************/
int
saving_throw_eq (int level, int ts_min_lev, int ts_max_lev)
{
	int y;
	float coeff, c1, c2, trans;

	if (level == 0)
	{
		return 90;
	}
	else if (level > 0 && level < LVL_IMMORT)
	{
		c1 = (ts_max_lev - ts_min_lev);
		c2 = (LVL_IMMORT - 1) - 1;
		coeff = c1 / c2;
		trans = -(coeff) * 1 + ts_min_lev;
		y = (int) (coeff * level + trans) + 1;

		return y;
	}
	else
	{
		return 0;
	}
}

/*******************************************************************
 * th_min_lev = tiro per colpire a livello 1
 * th_mid_lev = tiro per colpire a livello 20
 * th_max_lev = tiro per colpire a livello LVL_IMMORT-1
 *******************************************************************/
int
thaco_eq (int level, int th_min_lev, int th_mid_lev, int th_max_lev)
{
	float coeff, c1, c2, trans;

	if (level == 0)
	{
		return 100;
	}
	else if (level > 0 && level < 20)
	{
		c1 = (th_mid_lev - th_min_lev);
		c2 = (20 - 1) - 1;
		coeff = c1 / c2;
		trans = -(coeff) * 1 + th_min_lev;
		return (int) (coeff * level + trans) + 1;
	}
	else if (level >= 20 && level < (LVL_IMMORT - 1))
	{
		c1 = (th_max_lev - th_mid_lev);
		c2 = (LVL_IMMORT - 1) - 1;
		coeff = c1 / c2;
		trans = -(coeff) * 1 + th_mid_lev;
		return (int) (coeff * level + trans) + 1;
	}
	else
	{
		return 1;
	}
}

/*
 * These are definitions which control the guildmasters for each class.
 *
 * The first field (top line) controls the highest percentage skill level
 * a character of the class is allowed to attain in any skill.  (After
 * this level, attempts to practice will say "You are already learned in
 * this area."
 * 
 * The second line controls the maximum percent gain in learnedness a
 * character is allowed per practice -- in other words, if the random
 * die throw comes out higher than this number, the gain will only be
 * this number instead.
 *
 * The third line controls the minimu percent gain in learnedness a
 * character is allowed per practice -- in other words, if the random
 * die throw comes out below this number, the gain will be set up to
 * this number.
 * 
 * The fourth line simply sets whether the character knows 'spells'
 * or 'skills'.  This does not affect anything except the message given
 * to the character when trying to practice (i.e. "You know of the
 * following spells" vs. "You know of the following skills"
 */

#define SPELL	0
#define SKILL	1

/* #define LEARNED_LEVEL	0  % known which is considered "learned" */

/* #define MAX_PER_PRAC		1  max percent gain in skill per practice */

/* #define MIN_PER_PRAC		2  min percent gain in skill per practice */

/* #define PRAC_TYPE		3  should it say 'spell' or 'skill'?	*/

int prac_params[4][NUM_CLASSES] = {

/*  MAG     CLE     TH		WAR   	MA   	LIN 	PSI*/
	{95, 95, 85, 80, 85, 80, 95},	/* learned level */
	{90, 90, 12, 12, 35, 50, 90},	/* max per prac */
	{25, 25, 0, 0, 15, 10, 25},	/* min per pac */
	{SPELL, SPELL, SKILL, SKILL, SKILL, SPELL, SKILL}	/* prac name */
};


/*
 * ...And the appropriate rooms for each guildmaster/guildguard; controls
 * which types of people the various guildguards let through.  i.e., the
 * first line shows that from room 3017, only MAGIC_USERS are allowed
 * to go south.
 *
 * Don't forget to visit spec_assign.c if you create any new mobiles that
 * should be a guild master or guard so they can act appropriately. If you
 * "recycle" the existing mobs that are used in other guilds for your new
 * guild, then you don't have to change that file, only here.
 */
int guild_info[][3] = {

/* Midgaard */
	{CLASS_MAGIC_USER, 3027, SCMD_SOUTH},
	{CLASS_CLERIC, 3004, SCMD_NORTH},
	{CLASS_THIEF, 3004, SCMD_EAST},
	{CLASS_WARRIOR, 3021, SCMD_NORTH},
	{CLASS_MARTIAL, 3076, SCMD_SOUTH},
	{CLASS_LINKER, 3074, SCMD_NORTH},
	{CLASS_PSIONIC, 3017, SCMD_SOUTH},

/* Brass Dragon */
	{-999 /* all */ , 5065, SCMD_WEST},

/* this must go last -- add new guards above! */
	{-1, -1, -1}
};

/*
 * Saving throws for:
 * MCTW
 *   PARA, ROD, PETRI, BREATH, SPELL
 *     Levels 0-40
 *
 * Do not forget to change extern declaration in magic.c if you add to this.
 */

byte saving_throws (int class_num, int type, int level)
{
	switch (class_num)
	{
	case CLASS_MAGIC_USER:
		switch (type)
		{
		case SAVING_PARA:		/* Paralyzation */
			return saving_throw_eq (level, 70, 30);

		case SAVING_ROD:		/* Rods */
			return saving_throw_eq (level, 55, 10);

		case SAVING_PETRI:		/* Petrification */
			return saving_throw_eq (level, 65, 15);

		case SAVING_BREATH:	/* Breath weapons */
			return saving_throw_eq (level, 75, 25);

		case SAVING_SPELL:		/* Generic spells */
			return saving_throw_eq (level, 60, 10);

		default:
			log ("SYSERR: Invalid saving throw type.");
			break;
		}
		break;

	case CLASS_CLERIC:
		switch (type)
		{
		case SAVING_PARA:		/* Paralyzation */
			return saving_throw_eq (level, 60, 10);

		case SAVING_ROD:		/* Rods */
			return saving_throw_eq (level, 70, 25);

		case SAVING_PETRI:		/* Petrification */
			return saving_throw_eq (level, 65, 20);

		case SAVING_BREATH:	/* Breath weapons */
			return saving_throw_eq (level, 80, 35);

		case SAVING_SPELL:		/* Generic spells */
			return saving_throw_eq (level, 70, 30);

		default:
			log ("SYSERR: Invalid saving throw type.");
			break;
		}
		break;

	case CLASS_THIEF:
		switch (type)
		{
		case SAVING_PARA:		/* Paralyzation */
			return saving_throw_eq (level, 65, 35);

		case SAVING_ROD:		/* Rods */
			return saving_throw_eq (level, 70, 10);

		case SAVING_PETRI:		/* Petrification */
			return saving_throw_eq (level, 60, 30);

		case SAVING_BREATH:	/* Breath weapons */
			return saving_throw_eq (level, 80, 50);

		case SAVING_SPELL:		/* Generic spells */
			return saving_throw_eq (level, 75, 15);

		default:
			log ("SYSERR: Invalid saving throw type.");
			break;
		}
		break;

	case CLASS_WARRIOR:
		switch (type)
		{
		case SAVING_PARA:		/* Paralyzation */
			return saving_throw_eq (level, 70, 20);

		case SAVING_ROD:		/* Rods */
			return saving_throw_eq (level, 80, 20);

		case SAVING_PETRI:		/* Petrification */
			return saving_throw_eq (level, 75, 15);

		case SAVING_BREATH:	/* Breath weapons */
			return saving_throw_eq (level, 85, 35);

		case SAVING_SPELL:		/* Generic spells */
			return saving_throw_eq (level, 85, 25);

		default:
			log ("SYSERR: Invalid saving throw type.");
			break;
		}
		break;

	case CLASS_MARTIAL:
		switch (type)
		{
		case SAVING_PARA:		/* Paralyzation */
			return saving_throw_eq (level, 60, 10);

		case SAVING_ROD:		/* Rods */
			return saving_throw_eq (level, 70, 25);

		case SAVING_PETRI:		/* Petrification */
			return saving_throw_eq (level, 65, 20);

		case SAVING_BREATH:	/* Breath weapons */
			return saving_throw_eq (level, 80, 35);

		case SAVING_SPELL:		/* Generic spells */
			return saving_throw_eq (level, 70, 30);

		default:
			log ("SYSERR: Invalid saving throw type.");
			break;
		}
		break;

	case CLASS_LINKER:
		switch (type)
		{
		case SAVING_PARA:		/* Paralyzation */
			return saving_throw_eq (level, 65, 35);

		case SAVING_ROD:		/* Rods */
			return saving_throw_eq (level, 70, 10);

		case SAVING_PETRI:		/* Petrification */
			return saving_throw_eq (level, 60, 30);

		case SAVING_BREATH:	/* Breath weapons */
			return saving_throw_eq (level, 80, 50);

		case SAVING_SPELL:		/* Generic spells */
			return saving_throw_eq (level, 75, 15);

		default:
			log ("SYSERR: Invalid saving throw type.");
			break;
		}
		break;

	case CLASS_PSIONIC:
		switch (type)
		{
		case SAVING_PARA:		/* Paralyzation */
			return saving_throw_eq (level, 70, 30);

		case SAVING_ROD:		/* Rods */
			return saving_throw_eq (level, 55, 10);

		case SAVING_PETRI:		/* Petrification */
			return saving_throw_eq (level, 65, 15);

		case SAVING_BREATH:	/* Breath weapons */
			return saving_throw_eq (level, 75, 25);

		case SAVING_SPELL:		/* Generic spells */
			return saving_throw_eq (level, 60, 10);

		default:
			log ("SYSERR: Invalid saving throw type.");
			break;
		}
		break;
	default:
		log ("SYSERR: Invalid class saving throw.");
		break;
	}

	/* Should not get here unless something is wrong. */
	return 100;
}

/* THAC0 for classes and levels.  (To Hit Armor Class 0) */
int
thaco (int class_num, int level)
{
	switch (class_num)
	{
	case CLASS_MAGIC_USER:
		return thaco_eq (level, 20, 15, 10);

	case CLASS_CLERIC:
		return thaco_eq (level, 20, 12, 8);

	case CLASS_THIEF:
		return thaco_eq (level, 20, 13, 5);

	case CLASS_WARRIOR:
		return thaco_eq (level, 18, 12, 2);

	case CLASS_MARTIAL:
		return thaco_eq (level, 18, 12, 2);

	case CLASS_LINKER:
		return thaco_eq (level, 18, 15, 5);

	case CLASS_PSIONIC:
		return thaco_eq (level, 20, 15, 10);

	default:
		log ("SYSERR: Unknown class in thac0 chart.");
	}

	/* Will not get there unless something is wrong. */
	return 100;
}


/*
 * Roll the 6 stats for a character... each stat is made of the sum of
 * the best 3 out of 4 rolls of a 6-sided die.  Each class then decides
 * which priority will be given for the best to worst stats.
 */
void
roll_real_abils (struct char_data *ch)
{
	int i, j, k, temp;
	ubyte table[6];
	ubyte rolls[4];

	for (i = 0; i < 6; i++)
	{
		table[i] = 0;
	}

	for (i = 0; i < 6; i++)
	{

		for (j = 0; j < 4; j++)
		{
			rolls[j] = number (1, 6);
		}

		temp = rolls[0] + rolls[1] + rolls[2] + rolls[3] -
			MIN (rolls[0], MIN (rolls[1], MIN (rolls[2], rolls[3])));

		for (k = 0; k < 6; k++)
		{
			if (table[k] < temp)
			{
				temp ^= table[k];
				table[k] ^= temp;
				temp ^= table[k];
			}
		}
	}

	ch->real_abils.str_add = 0;

	switch (GET_CLASS (ch))
	{
	case CLASS_MAGIC_USER:
		ch->real_abils.intel = table[0];
		ch->real_abils.wis = table[1];
		ch->real_abils.dex = table[2];
		ch->real_abils.str = table[3];
		ch->real_abils.con = table[4];
		ch->real_abils.cha = table[5];
		break;
	case CLASS_CLERIC:
		ch->real_abils.wis = table[0];
		ch->real_abils.intel = table[1];
		ch->real_abils.str = table[2];
		ch->real_abils.dex = table[3];
		ch->real_abils.con = table[4];
		ch->real_abils.cha = table[5];
		break;
	case CLASS_THIEF:
		ch->real_abils.dex = table[0];
		ch->real_abils.str = table[1];
		ch->real_abils.con = table[2];
		ch->real_abils.intel = table[3];
		ch->real_abils.wis = table[4];
		ch->real_abils.cha = table[5];
		break;
	case CLASS_WARRIOR:
		ch->real_abils.str = table[0];
		ch->real_abils.dex = table[1];
		ch->real_abils.con = table[2];
		ch->real_abils.wis = table[3];
		ch->real_abils.intel = table[4];
		ch->real_abils.cha = table[5];
		if (ch->real_abils.str == 18)
		{
			ch->real_abils.str_add = number (0, 100);
		}
		break;
	case CLASS_MARTIAL:
		ch->real_abils.dex = table[0];
		ch->real_abils.con = table[1];
		ch->real_abils.str = table[2];
		ch->real_abils.wis = table[3];
		ch->real_abils.intel = table[4];
		ch->real_abils.cha = table[5];
		if (ch->real_abils.str == 18)
		{
			ch->real_abils.str_add = number (0, 100);
		}
		break;
	case CLASS_LINKER:
		ch->real_abils.str = table[0];
		ch->real_abils.con = table[1];
		ch->real_abils.dex = table[2];
		ch->real_abils.wis = table[3];
		ch->real_abils.intel = table[4];
		ch->real_abils.cha = table[5];
		if (ch->real_abils.str == 18)
		{
			ch->real_abils.str_add = number (0, 100);
		}
		break;
	case CLASS_PSIONIC:
		ch->real_abils.intel = table[0];
		ch->real_abils.wis = table[1];
		ch->real_abils.dex = table[2];
		ch->real_abils.str = table[3];
		ch->real_abils.con = table[4];
		ch->real_abils.cha = table[5];
		break;
	}

	switch (GET_RACE (ch))
	{
	case RACE_UMANO:
		++ch->real_abils.con;
		break;
	case RACE_CYBORG:
		++ch->real_abils.dex;
		ch->real_abils.intel += 2;
		ch->real_abils.cha -= 3;
		break;
	case RACE_MOTOCICLISTA:
		ch->real_abils.str += 3;
		ch->real_abils.intel -= 2;
		--ch->real_abils.cha;
		break;
	case RACE_ALIENO:
		ch->real_abils.dex += 2;
		++ch->real_abils.wis;
		--ch->real_abils.str;
		ch->real_abils.con -= 2;
		break;
	}

	ch->aff_abils = ch->real_abils;
}


/* Some initializations for characters, including initial skills */
void
do_start (struct char_data *ch)
{
	GET_LEVEL (ch) = 1;
	GET_EXP (ch) = 1;

	set_title (ch, NULL);
	roll_real_abils (ch);
	ch->points.max_hit = 10;

	switch (GET_CLASS (ch))
	{

	case CLASS_MAGIC_USER:
		break;

	case CLASS_CLERIC:
		break;

	case CLASS_THIEF:
		SET_SKILL (ch, SKILL_SNEAK, 10);
		SET_SKILL (ch, SKILL_HIDE, 5);
		SET_SKILL (ch, SKILL_SPY, 7);
		SET_SKILL (ch, SKILL_STEAL, 15);
		SET_SKILL (ch, SKILL_BACKSTAB, 20);
		SET_SKILL (ch, SKILL_PICK_LOCK, 10);
		SET_SKILL (ch, SKILL_TRACK, 20);
        SET_SKILL (ch, SKILL_RETREAT,10);
		break;

	case CLASS_WARRIOR:
		break;

	case CLASS_MARTIAL:
        SET_SKILL (ch, SKILL_RETREAT,10);
		break;

	case CLASS_LINKER:
		break;

	case CLASS_PSIONIC:
		break;
	}

	advance_level (ch);
	sprintf (buf, "%s advanced to level %d", GET_NAME (ch), GET_LEVEL (ch));
	mudlog (buf, BRF, MAX (LVL_IMMORT, GET_INVIS_LEV (ch)), TRUE);

	GET_HIT (ch) = GET_MAX_HIT (ch);
	GET_MANA (ch) = GET_MAX_MANA (ch);
	GET_MOVE (ch) = GET_MAX_MOVE (ch);

	GET_COND (ch, THIRST) = 24;
	GET_COND (ch, FULL) = 24;
	GET_COND (ch, DRUNK) = 0;

	ch->player.time.played = 0;
	ch->player.time.logon = time (0);


	if (siteok_everyone)
		SET_BIT (PLR_FLAGS (ch), PLR_SITEOK);
}



/*
 * This function controls the change to maxmove, maxmana, and maxhp for
 * each class every time they gain a level.
 */
void
advance_level (struct char_data *ch)
{
	int add_hp, add_mana = 0, add_move = 0, i;

	add_hp = con_app[GET_CON (ch)].hitp;

	switch (GET_CLASS (ch))
	{

	case CLASS_MAGIC_USER:
		add_hp += number (3, 8);
		add_mana = number (GET_LEVEL (ch), (int) (1.5 * GET_LEVEL (ch)));
		add_mana = MIN (add_mana, 10);
		add_move = number (0, 2);
		break;

	case CLASS_CLERIC:
		add_hp += number (5, 10);
		add_mana = number (GET_LEVEL (ch), (int) (1.5 * GET_LEVEL (ch)));
		add_mana = MIN (add_mana, 10);
		add_move = number (0, 2);
		break;

	case CLASS_THIEF:
		add_hp += number (7, 13);
		add_mana = 0;
		add_move = number (1, 3);
		break;

	case CLASS_WARRIOR:
		add_hp += number (10, 15);
		add_mana = 0;
		add_move = number (1, 3);
		break;

	case CLASS_MARTIAL:
		add_hp += number (5, 10);
		add_mana = MIN (add_mana, 10);
		add_move = number (1, 3);
		break;

	case CLASS_LINKER:
		add_hp += number (8, 13);
		add_mana = number (GET_LEVEL (ch), (int) (1.5 * GET_LEVEL (ch)));
		add_mana = MIN (add_mana, 8);
		add_move = number (0, 2);
		break;

	case CLASS_PSIONIC:
		add_hp += number (3, 8);
		add_mana = number (GET_LEVEL (ch), (int) (1.5 * GET_LEVEL (ch)));
		add_mana = MIN (add_mana, 10);
		add_move = number (0, 2);
		break;

	}

	ch->points.max_hit += MAX (1, add_hp);
	ch->points.max_move += MAX (1, add_move);

	if (GET_LEVEL (ch) > 1)
		ch->points.max_mana += add_mana;

	if (IS_MAGIC_USER (ch) || IS_CLERIC (ch) || IS_LINKER (ch)
		|| IS_PSIONIC (ch))
		GET_PRACTICES (ch) += MAX (2, wis_app[GET_WIS (ch)].bonus);
	else
		GET_PRACTICES (ch) += MIN (2, MAX (1, wis_app[GET_WIS (ch)].bonus));

	if (GET_LEVEL (ch) >= LVL_IMMORT)
	{
		for (i = 0; i < 3; i++)
			GET_COND (ch, i) = (char) -1;
		SET_BIT (PRF_FLAGS (ch), PRF_HOLYLIGHT);
	}

	// Qualche default sul Prompt e sui colori
	SET_BIT (PRF_FLAGS (ch),
			 PRF_DISPHP | PRF_DISPMANA | PRF_DISPMOVE | PRF_DISPGOLD |
			 PRF_DISPEXP | PRF_DISPEXITS);	// Prompt all
	SET_BIT (PRF_FLAGS (ch), PRF_COLOR_1 | PRF_COLOR_2);	// Color complete

	save_char (ch, NOWHERE);
}


/*
 * This simply calculates the backstab multiplier based on a character's
 * level.  This used to be an array, but was changed to be a function so
 * that it would be easier to add more levels to your MUD.  This doesn't
 * really create a big performance hit because it's not used very often.
 */
int
backstab_mult (int level)
{
	if (level <= 0)
		return 1;				/* level 0 */
	else if (level <= 7)
		return 2;				/* level 1 - 9 */
	else if (level <= 15)
		return 3;				/* level 10 - 19 */
	else if (level <= 30)
		return 4;				/* level 14 - 20 */
	else if (level <= 50)
		return 5;				/* level 21 - 58 */
	else if (level < LVL_IMMORT)
		return 6;				/* all remaining mortal levels */
	else
		return 20;				/* immortals */
}


/*
 * invalid_class is used by handler.c to determine if a piece of equipment is
 * usable by a particular class, based on the ITEM_ANTI_{class} bitvectors.
 */
int
invalid_class (struct char_data *ch, struct obj_data *obj)
{
	if (IS_OBJ_STAT (obj, ITEM_ANTI_MAGIC_USER) && IS_MAGIC_USER (ch))
		return 1;

	if (IS_OBJ_STAT (obj, ITEM_ANTI_CLERIC) && IS_CLERIC (ch))
		return 1;

	if (IS_OBJ_STAT (obj, ITEM_ANTI_WARRIOR) && IS_WARRIOR (ch))
		return 1;

	if (IS_OBJ_STAT (obj, ITEM_ANTI_MARTIAL) && IS_MARTIAL (ch))
		return 1;

	if (IS_OBJ_STAT (obj, ITEM_ANTI_LINKER) && IS_LINKER (ch))
		return 1;

	if (IS_OBJ_STAT (obj, ITEM_ANTI_THIEF) && IS_THIEF (ch))
		return 1;

	if (IS_OBJ_STAT (obj, ITEM_ANTI_PSIONICO) && IS_PSIONIC (ch))
		return 1;

	return 0;
}




/*
 * SPELLS AND SKILLS.  This area defines which spells are assigned to
 * which classes, and the minimum level the character must be to use
 * the spell or skill.
 */
void
init_spell_levels (void)
{
	/* Virus Writer */
	spell_level (SKILL_THROW, CLASS_MAGIC_USER, 1);
	spell_level (SPELL_MAGIC_MISSILE, CLASS_MAGIC_USER, 1);
	spell_level (SKILL_SHOOT, CLASS_MAGIC_USER, 2);
	spell_level (SKILL_SEND, CLASS_MAGIC_USER, 3);
	spell_level (SPELL_DETECT_INVIS, CLASS_MAGIC_USER, 3);
	spell_level (SPELL_DETECT_MAGIC, CLASS_MAGIC_USER, 4);
	spell_level (SPELL_CHILL_TOUCH, CLASS_MAGIC_USER, 5);
	spell_level (SPELL_INFRAVISION, CLASS_MAGIC_USER, 6);
	spell_level (SPELL_INVISIBLE, CLASS_MAGIC_USER, 7);
	spell_level (SPELL_ARMOR, CLASS_MAGIC_USER, 8);
	spell_level (SPELL_BURNING_HANDS, CLASS_MAGIC_USER, 9);
	spell_level (SPELL_LOCATE_OBJECT, CLASS_MAGIC_USER, 10);
	spell_level (SPELL_STRENGTH, CLASS_MAGIC_USER, 11);
	spell_level (SPELL_SHOCKING_GRASP, CLASS_MAGIC_USER, 12);
	spell_level (SPELL_SLEEP, CLASS_MAGIC_USER, 13);
	spell_level (SPELL_BLINDNESS, CLASS_MAGIC_USER, 15);
	spell_level (SPELL_DETECT_POISON, CLASS_MAGIC_USER, 18);
	spell_level (SPELL_FLY, CLASS_MAGIC_USER, 20);
	spell_level (SPELL_LIGHTNING_BOLT, CLASS_MAGIC_USER, 22);
	spell_level (SPELL_ENERGY_DRAIN, CLASS_MAGIC_USER, 25);
	spell_level (SPELL_COLOR_SPRAY, CLASS_MAGIC_USER, 27);
	spell_level (SPELL_WORD_OF_RECALL, CLASS_MAGIC_USER, 28);
	spell_level (SPELL_CURSE, CLASS_MAGIC_USER, 30);
	spell_level (SPELL_POISON, CLASS_MAGIC_USER, 32);
	spell_level (SPELL_SUMMON, CLASS_MAGIC_USER, 35);
	spell_level (SPELL_FIREBALL, CLASS_MAGIC_USER, 37);
	spell_level (SPELL_GATE, CLASS_MAGIC_USER, 40);
	spell_level (SPELL_CHARM, CLASS_MAGIC_USER, 42);
	spell_level (SPELL_ENCHANT_WEAPON, CLASS_MAGIC_USER, 46);
	spell_level (SPELL_CLONE, CLASS_MAGIC_USER, 55);

	/* Sysadmins */
	spell_level (SKILL_THROW, CLASS_CLERIC, 1);
	spell_level (SPELL_CURE_LIGHT, CLASS_CLERIC, 1);
	spell_level (SPELL_ARMOR, CLASS_CLERIC, 2);
	spell_level (SKILL_SHOOT, CLASS_CLERIC, 3);
	spell_level (SKILL_SEND, CLASS_CLERIC, 4);
	spell_level (SPELL_CREATE_FOOD, CLASS_CLERIC, 4);
	spell_level (SPELL_MAGIC_MISSILE, CLASS_CLERIC, 5);
	spell_level (SPELL_CREATE_WATER, CLASS_CLERIC, 5);
	spell_level (SPELL_DETECT_POISON, CLASS_CLERIC, 6);
	spell_level (SPELL_DETECT_ALIGN, CLASS_CLERIC, 7);
	spell_level (SPELL_CURE_BLIND, CLASS_CLERIC, 8);
	spell_level (SPELL_BLESS, CLASS_CLERIC, 9);
	spell_level (SPELL_CHILL_TOUCH, CLASS_CLERIC, 10);
	spell_level (SPELL_DETECT_INVIS, CLASS_CLERIC, 10);
	spell_level (SPELL_BLINDNESS, CLASS_CLERIC, 11);
	spell_level (SPELL_INFRAVISION, CLASS_CLERIC, 11);
	spell_level (SPELL_WORD_OF_RECALL, CLASS_CLERIC, 12);
	spell_level (SPELL_INFRAVISION, CLASS_CLERIC, 12);
	spell_level (SPELL_PROT_FROM_EVIL, CLASS_CLERIC, 13);
	spell_level (SPELL_FLY, CLASS_CLERIC, 15);
	spell_level (SPELL_POISON, CLASS_CLERIC, 17);
	spell_level (SPELL_BURNING_HANDS, CLASS_CLERIC, 18);
	spell_level (SPELL_GROUP_ARMOR, CLASS_CLERIC, 20);
	spell_level (SPELL_CURE_CRITIC, CLASS_CLERIC, 22);
	spell_level (SPELL_SUMMON, CLASS_CLERIC, 25);
	spell_level (SPELL_GATE, CLASS_CLERIC, 26);
	spell_level (SPELL_REMOVE_POISON, CLASS_CLERIC, 27);
	spell_level (SPELL_EARTHQUAKE, CLASS_CLERIC, 30);
	spell_level (SPELL_DISPEL_EVIL, CLASS_CLERIC, 31);
	spell_level (SPELL_DISPEL_GOOD, CLASS_CLERIC, 32);
	spell_level (SPELL_SANCTUARY, CLASS_CLERIC, 33);
	spell_level (SPELL_CALL_LIGHTNING, CLASS_CLERIC, 35);
	spell_level (SPELL_HEAL, CLASS_CLERIC, 35);
	spell_level (SPELL_CONTROL_WEATHER, CLASS_CLERIC, 37);
	spell_level (SPELL_SENSE_LIFE, CLASS_CLERIC, 40);
	spell_level (SPELL_HARM, CLASS_CLERIC, 45);
	spell_level (SPELL_GROUP_HEAL, CLASS_CLERIC, 50);
	spell_level (SPELL_REMOVE_CURSE, CLASS_CLERIC, 52);

	/* Hackers */
	spell_level (SKILL_SNEAK, CLASS_THIEF, 1);
	spell_level (SKILL_THROW, CLASS_THIEF, 1);
	spell_level (SKILL_SHOOT, CLASS_THIEF, 2);
	spell_level (SKILL_PICK_LOCK, CLASS_THIEF, 2);
	spell_level (SKILL_BACKSTAB, CLASS_THIEF, 3);
	spell_level (SKILL_STEAL, CLASS_THIEF, 4);
	spell_level (SKILL_HIDE, CLASS_THIEF, 5);
	spell_level (SKILL_TRACK, CLASS_THIEF, 8);
    spell_level (SKILL_RETREAT, CLASS_THIEF,10);
	spell_level (SKILL_SPY, CLASS_THIEF, 20);
	spell_level (SPELL_GATE, CLASS_THIEF, 23);
	spell_level (SKILL_FIRST_AID, CLASS_THIEF, 25);
	spell_level (SKILL_SECOND, CLASS_THIEF, 30);
	spell_level (SKILL_DISARM, CLASS_THIEF, 45);

	/* WARRIOR */
	spell_level (SKILL_KICK, CLASS_WARRIOR, 1);
	spell_level (SKILL_THROW, CLASS_WARRIOR, 1);
	spell_level (SKILL_SHOOT, CLASS_WARRIOR, 2);
	spell_level (SKILL_RESCUE, CLASS_WARRIOR, 3);
	spell_level (SKILL_TRACK, CLASS_WARRIOR, 12);
	spell_level (SKILL_BASH, CLASS_WARRIOR, 15);
	spell_level (SKILL_SECOND, CLASS_WARRIOR, 18);
	spell_level (SKILL_FIRST_AID, CLASS_WARRIOR, 21);
    spell_level (SKILL_RETREAT, CLASS_WARRIOR,30);
	spell_level (SKILL_DISARM, CLASS_WARRIOR, 35);
	spell_level (SKILL_THIRD, CLASS_WARRIOR, 40);

	/* Martial Artist */
	spell_level (SKILL_KICK, CLASS_MARTIAL, 1);
	spell_level (SKILL_THROW, CLASS_MARTIAL, 1);
	spell_level (SKILL_SEND, CLASS_MARTIAL, 5);
	spell_level (SKILL_SPRING_LEAP, CLASS_MARTIAL, 10);
	spell_level (SKILL_FIRST_AID, CLASS_MARTIAL, 10);
	spell_level (SKILL_SOMMERSAULT, CLASS_MARTIAL, 15);
	spell_level (SKILL_DISARM, CLASS_MARTIAL, 18);
    spell_level (SKILL_SNEAK, CLASS_MARTIAL, 20);
    spell_level (SKILL_RETREAT, CLASS_MARTIAL,22);
	spell_level (SKILL_QUIVER_PALM, CLASS_MARTIAL, 25);
	spell_level (SKILL_HIDE, CLASS_MARTIAL, 30);
	spell_level (SKILL_KAMEHAMEHA, CLASS_MARTIAL, 35);
	spell_level (SKILL_RESCUE, CLASS_MARTIAL, 35);
	spell_level (SKILL_KI_SHIELD, CLASS_MARTIAL, 45);

	/* Linker */
	spell_level (SKILL_KICK, CLASS_LINKER, 1);
	spell_level (SKILL_THROW, CLASS_LINKER, 1);
	spell_level (SKILL_SHOOT, CLASS_LINKER, 2);
	spell_level (SKILL_SEND, CLASS_LINKER, 7);
	spell_level (SPELL_MAGIC_MISSILE, CLASS_LINKER, 10);
	spell_level (SPELL_DETECT_ALIGN, CLASS_LINKER, 13);
	spell_level (SPELL_DETECT_MAGIC, CLASS_LINKER, 13);
	spell_level (SPELL_INFRAVISION, CLASS_LINKER, 15);
	spell_level (SPELL_CURE_LIGHT, CLASS_LINKER, 15);
	spell_level (SPELL_STRENGTH, CLASS_LINKER, 17);
	spell_level (SPELL_INVISIBLE, CLASS_LINKER, 20);
	spell_level (SPELL_CHILL_TOUCH, CLASS_LINKER, 22);
	spell_level (SKILL_SECOND, CLASS_LINKER, 30);
	spell_level (SPELL_GATE, CLASS_LINKER, 50);


	/* Psionic */
	spell_level (SPELL_PSI_BLAST, CLASS_PSIONIC, 1);
	spell_level (SKILL_MESSENGER, CLASS_PSIONIC, 2);
	spell_level (SKILL_THROW, CLASS_PSIONIC, 2);
	spell_level (SKILL_SHOOT, CLASS_PSIONIC, 3);
	spell_level (SPELL_PSI_HYPNOSIS, CLASS_PSIONIC, 5);
	spell_level (SPELL_PSI_AURA, CLASS_PSIONIC, 5);
	spell_level (SPELL_PSI_DOORWAY, CLASS_PSIONIC, 7);
	spell_level (SPELL_PSI_SHIELD, CLASS_PSIONIC, 8);
	spell_level (SPELL_PSI_STRENGHT, CLASS_PSIONIC, 9);
	spell_level (SPELL_PSI_BURN, CLASS_PSIONIC, 10);
	spell_level (SPELL_PSI_SUMMON, CLASS_PSIONIC, 11);
	spell_level (SPELL_PSI_LEVITATION, CLASS_PSIONIC, 12);
	spell_level (SPELL_PSI_SIGHT, CLASS_PSIONIC, 15);
	spell_level (SPELL_PSI_TELEPORT, CLASS_PSIONIC, 16);
	spell_level (SPELL_PSI_FLAME, CLASS_PSIONIC, 18);
	spell_level (SPELL_PSI_MIND_OVER_BODY, CLASS_PSIONIC, 22);
	spell_level (SPELL_PSI_NIGHTVISION, CLASS_PSIONIC, 24);
	spell_level (SPELL_PSI_CANIBALIZE, CLASS_PSIONIC, 30);
}


/*
 * This is the exp given to implementors -- it must always be greater
 * than the exp required for immortality, plus at least 20,000 or so.
 */
#define EXP_MAX  100000000

/* Function to return the exp required for each class/level */
int
level_exp (int chclass, int level)
{
	if (level > LVL_IMPL || level < 0)
	{
		log ("SYSERR: Requesting exp for invalid level %d!", level);
		return 0;
	}

	/*
	 * Gods have exp close to EXP_MAX.  This statement should never have to
	 * changed, regardless of how many mortal or immortal levels exist.
	 */
	if (level > LVL_IMMORT)
	{
		return EXP_MAX - ((LVL_IMPL - level) * 1000);
	}

	/* Exp required for normal mortals is below */

	switch (chclass)
	{

	case CLASS_MAGIC_USER:
		switch (level)
		{
		case 0:
			return 0;
		case 1:
			return 1;
		case 2:
			return 2500;
		case 3:
			return 5000;
		case 4:
			return 10000;
		case 5:
			return 20000;
		case 6:
			return 40000;
		case 7:
			return 60000;
		case 8:
			return 90000;
		case 9:
			return 135000;
		case 10:
			return 250000;
		case 11:
			return 375000;
		case 12:
			return 750000;
		case 13:
			return 1125000;
		case 14:
			return 1500000;
		case 15:
			return 1875000;
		case 16:
			return 2250000;
		case 17:
			return 2625000;
		case 18:
			return 3000000;
		case 19:
			return 3375000;
		case 20:
			return 3750000;
		case 21:
			return 4000000;
		case 22:
			return 4300000;
		case 23:
			return 4600000;
		case 24:
			return 4900000;
		case 25:
			return 5200000;
		case 26:
			return 5500000;
		case 27:
			return 5950000;
		case 28:
			return 6400000;
		case 29:
			return 6850000;
		case 30:
			return 7400000;
		case 31:
			return 8400000;
		case 32:
			return 9400000;
		case 33:
			return 10400000;
		case 34:
			return 11400000;
		case 35:
			return 12400000;
		case 36:
			return 13400000;
		case 37:
			return 14400000;
		case 38:
			return 15400000;
		case 39:
			return 16400000;
		case 40:
			return 17400000;
		case 41:
			return 18400000;
		case 42:
			return 19400000;
		case 43:
			return 20400000;
		case 44:
			return 21400000;
		case 45:
			return 21800000;
		case 46:
			return 22400000;
		case 47:
			return 23400000;
		case 48:
			return 24400000;
		case 49:
			return 25400000;
		case 50:
			return 26400000;
		case 51:
			return 27400000;
		case 52:
			return 28400000;
		case 53:
			return 29400000;
		case 54:
			return 30400000;
		case 55:
			return 31400000;
		case 56:
			return 32400000;
		case 57:
			return 33400000;
		case 58:
			return 34400000;
		case 59:
			return 35400000;
		case 60:
			return 36400000;
			/* add new levels here */
		case LVL_IMMORT:
			return 80000000;
		}
		break;

	case CLASS_CLERIC:
		switch (level)
		{
		case 0:
			return 0;
		case 1:
			return 1;
		case 2:
			return 1500;
		case 3:
			return 3000;
		case 4:
			return 6000;
		case 5:
			return 13000;
		case 6:
			return 27500;
		case 7:
			return 55000;
		case 8:
			return 110000;
		case 9:
			return 225000;
		case 10:
			return 450000;
		case 11:
			return 675000;
		case 12:
			return 900000;
		case 13:
			return 1125000;
		case 14:
			return 1350000;
		case 15:
			return 1575000;
		case 16:
			return 1800000;
		case 17:
			return 2100000;
		case 18:
			return 2400000;
		case 19:
			return 2700000;
		case 20:
			return 3000000;
		case 21:
			return 3250000;
		case 22:
			return 3500000;
		case 23:
			return 3800000;
		case 24:
			return 4100000;
		case 25:
			return 4400000;
		case 26:
			return 4800000;
		case 27:
			return 5200000;
		case 28:
			return 5600000;
		case 29:
			return 6000000;
		case 30:
			return 6400000;
		case 31:
			return 8400000;
		case 32:
			return 9400000;
		case 33:
			return 10400000;
		case 34:
			return 11400000;
		case 35:
			return 12400000;
		case 36:
			return 13400000;
		case 37:
			return 14400000;
		case 38:
			return 15400000;
		case 39:
			return 16400000;
		case 40:
			return 17400000;
		case 41:
			return 18400000;
		case 42:
			return 19400000;
		case 43:
			return 20400000;
		case 44:
			return 21400000;
		case 45:
			return 21800000;
		case 46:
			return 22400000;
		case 47:
			return 23400000;
		case 48:
			return 24400000;
		case 49:
			return 25400000;
		case 50:
			return 26400000;
		case 51:
			return 27400000;
		case 52:
			return 28400000;
		case 53:
			return 29400000;
		case 54:
			return 30400000;
		case 55:
			return 31400000;
		case 56:
			return 32400000;
		case 57:
			return 33400000;
		case 58:
			return 34400000;
		case 59:
			return 35400000;
		case 60:
			return 36400000;
			/* add new levels here */
		case LVL_IMMORT:
			return 80000000;
		}
		break;

	case CLASS_THIEF:
		switch (level)
		{
		case 0:
			return 0;
		case 1:
			return 1;
		case 2:
			return 1250;
		case 3:
			return 2500;
		case 4:
			return 5000;
		case 5:
			return 10000;
		case 6:
			return 20000;
		case 7:
			return 30000;
		case 8:
			return 70000;
		case 9:
			return 110000;
		case 10:
			return 160000;
		case 11:
			return 220000;
		case 12:
			return 440000;
		case 13:
			return 660000;
		case 14:
			return 880000;
		case 15:
			return 1100000;
		case 16:
			return 1500000;
		case 17:
			return 2000000;
		case 18:
			return 2500000;
		case 19:
			return 3000000;
		case 20:
			return 3500000;
		case 21:
			return 3650000;
		case 22:
			return 3800000;
		case 23:
			return 4100000;
		case 24:
			return 4400000;
		case 25:
			return 4700000;
		case 26:
			return 5100000;
		case 27:
			return 5500000;
		case 28:
			return 5900000;
		case 29:
			return 6300000;
		case 30:
			return 6650000;
		case 31:
			return 8400000;
		case 32:
			return 9400000;
		case 33:
			return 10400000;
		case 34:
			return 11400000;
		case 35:
			return 12400000;
		case 36:
			return 13400000;
		case 37:
			return 14400000;
		case 38:
			return 15400000;
		case 39:
			return 16400000;
		case 40:
			return 17400000;
		case 41:
			return 18400000;
		case 42:
			return 19400000;
		case 43:
			return 20400000;
		case 44:
			return 21400000;
		case 45:
			return 21800000;
		case 46:
			return 22400000;
		case 47:
			return 23400000;
		case 48:
			return 24400000;
		case 49:
			return 25400000;
		case 50:
			return 26400000;
		case 51:
			return 27400000;
		case 52:
			return 28400000;
		case 53:
			return 29400000;
		case 54:
			return 30400000;
		case 55:
			return 31400000;
		case 56:
			return 32400000;
		case 57:
			return 33400000;
		case 58:
			return 34400000;
		case 59:
			return 35400000;
		case 60:
			return 36400000;
			/* add new levels here */
		case LVL_IMMORT:
			return 80000000;
		}
		break;

	case CLASS_WARRIOR:
		switch (level)
		{
		case 0:
			return 0;
		case 1:
			return 1;
		case 2:
			return 2000;
		case 3:
			return 4000;
		case 4:
			return 8000;
		case 5:
			return 16000;
		case 6:
			return 32000;
		case 7:
			return 64000;
		case 8:
			return 125000;
		case 9:
			return 250000;
		case 10:
			return 500000;
		case 11:
			return 750000;
		case 12:
			return 1000000;
		case 13:
			return 1250000;
		case 14:
			return 1500000;
		case 15:
			return 1850000;
		case 16:
			return 2200000;
		case 17:
			return 2550000;
		case 18:
			return 2900000;
		case 19:
			return 3250000;
		case 20:
			return 3600000;
		case 21:
			return 3900000;
		case 22:
			return 4200000;
		case 23:
			return 4500000;
		case 24:
			return 4800000;
		case 25:
			return 5150000;
		case 26:
			return 5500000;
		case 27:
			return 5950000;
		case 28:
			return 6400000;
		case 29:
			return 6850000;
		case 30:
			return 7400000;
		case 31:
			return 8400000;
		case 32:
			return 9400000;
		case 33:
			return 10400000;
		case 34:
			return 11400000;
		case 35:
			return 12400000;
		case 36:
			return 13400000;
		case 37:
			return 14400000;
		case 38:
			return 15400000;
		case 39:
			return 16400000;
		case 40:
			return 17400000;
		case 41:
			return 18400000;
		case 42:
			return 19400000;
		case 43:
			return 20400000;
		case 44:
			return 21400000;
		case 45:
			return 21800000;
		case 46:
			return 22400000;
		case 47:
			return 23400000;
		case 48:
			return 24400000;
		case 49:
			return 25400000;
		case 50:
			return 26400000;
		case 51:
			return 27400000;
		case 52:
			return 28400000;
		case 53:
			return 29400000;
		case 54:
			return 30400000;
		case 55:
			return 31400000;
		case 56:
			return 32400000;
		case 57:
			return 33400000;
		case 58:
			return 34400000;
		case 59:
			return 35400000;
		case 60:
			return 36400000;
			/* add new levels here */
		case LVL_IMMORT:
			return 80000000;
		}
		break;
	case CLASS_MARTIAL:
		switch (level)
		{
		case 0:
			return 0;
		case 1:
			return 1;
		case 2:
			return 2000;
		case 3:
			return 4000;
		case 4:
			return 8000;
		case 5:
			return 16000;
		case 6:
			return 32000;
		case 7:
			return 64000;
		case 8:
			return 125000;
		case 9:
			return 250000;
		case 10:
			return 500000;
		case 11:
			return 750000;
		case 12:
			return 1000000;
		case 13:
			return 1250000;
		case 14:
			return 1500000;
		case 15:
			return 1850000;
		case 16:
			return 2200000;
		case 17:
			return 2550000;
		case 18:
			return 2900000;
		case 19:
			return 3250000;
		case 20:
			return 3600000;
		case 21:
			return 3900000;
		case 22:
			return 4200000;
		case 23:
			return 4500000;
		case 24:
			return 4800000;
		case 25:
			return 5150000;
		case 26:
			return 5500000;
		case 27:
			return 5950000;
		case 28:
			return 6400000;
		case 29:
			return 6850000;
		case 30:
			return 7400000;
		case 31:
			return 8400000;
		case 32:
			return 9400000;
		case 33:
			return 10400000;
		case 34:
			return 11400000;
		case 35:
			return 12400000;
		case 36:
			return 13400000;
		case 37:
			return 14400000;
		case 38:
			return 15400000;
		case 39:
			return 16400000;
		case 40:
			return 17400000;
		case 41:
			return 18400000;
		case 42:
			return 19400000;
		case 43:
			return 20400000;
		case 44:
			return 21400000;
		case 45:
			return 21800000;
		case 46:
			return 22400000;
		case 47:
			return 23400000;
		case 48:
			return 24400000;
		case 49:
			return 25400000;
		case 50:
			return 26400000;
		case 51:
			return 27400000;
		case 52:
			return 28400000;
		case 53:
			return 29400000;
		case 54:
			return 30400000;
		case 55:
			return 31400000;
		case 56:
			return 32400000;
		case 57:
			return 33400000;
		case 58:
			return 34400000;
		case 59:
			return 35400000;
		case 60:
			return 36400000;
			/* add new levels here */
		case LVL_IMMORT:
			return 80000000;
		}
		break;
	case CLASS_LINKER:
		switch (level)
		{
		case 0:
			return 0;
		case 1:
			return 1;
		case 2:
			return 2000;
		case 3:
			return 4000;
		case 4:
			return 8000;
		case 5:
			return 16000;
		case 6:
			return 32000;
		case 7:
			return 64000;
		case 8:
			return 125000;
		case 9:
			return 250000;
		case 10:
			return 500000;
		case 11:
			return 750000;
		case 12:
			return 1000000;
		case 13:
			return 1250000;
		case 14:
			return 1500000;
		case 15:
			return 1850000;
		case 16:
			return 2200000;
		case 17:
			return 2550000;
		case 18:
			return 2900000;
		case 19:
			return 3250000;
		case 20:
			return 3600000;
		case 21:
			return 3900000;
		case 22:
			return 4200000;
		case 23:
			return 4500000;
		case 24:
			return 4800000;
		case 25:
			return 5150000;
		case 26:
			return 5500000;
		case 27:
			return 5950000;
		case 28:
			return 6400000;
		case 29:
			return 6850000;
		case 30:
			return 7400000;
		case 31:
			return 8400000;
		case 32:
			return 9400000;
		case 33:
			return 10400000;
		case 34:
			return 11400000;
		case 35:
			return 12400000;
		case 36:
			return 13400000;
		case 37:
			return 14400000;
		case 38:
			return 15400000;
		case 39:
			return 16400000;
		case 40:
			return 17400000;
		case 41:
			return 18400000;
		case 42:
			return 19400000;
		case 43:
			return 20400000;
		case 44:
			return 21400000;
		case 45:
			return 21800000;
		case 46:
			return 22400000;
		case 47:
			return 23400000;
		case 48:
			return 24400000;
		case 49:
			return 25400000;
		case 50:
			return 26400000;
		case 51:
			return 27400000;
		case 52:
			return 28400000;
		case 53:
			return 29400000;
		case 54:
			return 30400000;
		case 55:
			return 31400000;
		case 56:
			return 32400000;
		case 57:
			return 33400000;
		case 58:
			return 34400000;
		case 59:
			return 35400000;
		case 60:
			return 36400000;
			/* add new levels here */
		case LVL_IMMORT:
			return 80000000;
		}
		break;

	case CLASS_PSIONIC:
		switch (level)
		{
		case 0:
			return 0;
		case 1:
			return 1;
		case 2:
			return 2500;
		case 3:
			return 5000;
		case 4:
			return 10000;
		case 5:
			return 20000;
		case 6:
			return 40000;
		case 7:
			return 60000;
		case 8:
			return 90000;
		case 9:
			return 135000;
		case 10:
			return 250000;
		case 11:
			return 375000;
		case 12:
			return 750000;
		case 13:
			return 1125000;
		case 14:
			return 1500000;
		case 15:
			return 1875000;
		case 16:
			return 2250000;
		case 17:
			return 2625000;
		case 18:
			return 3000000;
		case 19:
			return 3375000;
		case 20:
			return 3750000;
		case 21:
			return 4000000;
		case 22:
			return 4300000;
		case 23:
			return 4600000;
		case 24:
			return 4900000;
		case 25:
			return 5200000;
		case 26:
			return 5500000;
		case 27:
			return 5950000;
		case 28:
			return 6400000;
		case 29:
			return 6850000;
		case 30:
			return 7400000;
		case 31:
			return 8400000;
		case 32:
			return 9400000;
		case 33:
			return 10400000;
		case 34:
			return 11400000;
		case 35:
			return 12400000;
		case 36:
			return 13400000;
		case 37:
			return 14400000;
		case 38:
			return 15400000;
		case 39:
			return 16400000;
		case 40:
			return 17400000;
		case 41:
			return 18400000;
		case 42:
			return 19400000;
		case 43:
			return 20400000;
		case 44:
			return 21400000;
		case 45:
			return 21800000;
		case 46:
			return 22400000;
		case 47:
			return 23400000;
		case 48:
			return 24400000;
		case 49:
			return 25400000;
		case 50:
			return 26400000;
		case 51:
			return 27400000;
		case 52:
			return 28400000;
		case 53:
			return 29400000;
		case 54:
			return 30400000;
		case 55:
			return 31400000;
		case 56:
			return 32400000;
		case 57:
			return 33400000;
		case 58:
			return 34400000;
		case 59:
			return 35400000;
		case 60:
			return 36400000;
			/* add new levels here */
		case LVL_IMMORT:
			return 80000000;
		}
		break;
	}

	/*
	 * This statement should never be reached if the exp tables in this function
	 * are set up properly.  If you see exp of 123456 then the tables above are
	 * incomplete -- so, complete them!
	 */
	log ("SYSERR: XP tables not set up correctly in class.c!");
	return 123456;
}


/* 
 * Default titles of male characters.
 */
const char *
title_male (int chclass, int level)
{
	if (level <= 0 || level > LVL_IMPL)
		return "the Man";
	if (level == LVL_IMPL)
		return "the Implementor";

	switch (chclass)
	{

	case CLASS_MAGIC_USER:
		switch (level)
		{
		case 1:
			return "the newbie virus writer [1]";
		case 2:
			return "the newbie virus writer [2]";
		case 3:
			return "the newbie virus writer [3]";
		case 4:
			return "the newbie virus writer [4]";
		case 5:
			return "the newbie virus writer [5]";
		case 6:
			return "the newbie virus writer [6]";
		case 7:
			return "the newbie virus writer [7]";
		case 8:
			return "the newbie virus writer [8]";
		case 9:
			return "the newbie virus writer [9]";
		case 10:
			return "the newbie virus writer [10]";
		case 11:
			return "the chump virus writer [1]";
		case 12:
			return "the chump virus writer [2]";
		case 13:
			return "the chump virus writer [3]";
		case 14:
			return "the chump virus writer [4]";
		case 15:
			return "the chump virus writer [5]";
		case 16:
			return "the chump virus writer [6]";
		case 17:
			return "the chump virus writer [7]";
		case 18:
			return "the chump virus writer [8]";
		case 19:
			return "the chump virus writer [9]";
		case 20:
			return "the chump virus writer [10]";
		case 21:
			return "the medium virus writer [1]";
		case 22:
			return "the medium virus writer [2]";
		case 23:
			return "the medium virus writer [3]";
		case 24:
			return "the medium virus writer [4]";
		case 25:
			return "the medium virus writer [5]";
		case 26:
			return "the medium virus writer [6]";
		case 27:
			return "the medium virus writer [7]";
		case 28:
			return "the medium virus writer [8]";
		case 29:
			return "the medium virus writer [9]";
		case 30:
			return "the medium virus writer [10]";
		case 31:
			return "the expert virus writer [1]";
		case 32:
			return "the expert virus writer [2]";
		case 33:
			return "the expert virus writer [3]";
		case 34:
			return "the expert virus writer [4]";
		case 35:
			return "the expert virus writer [5]";
		case 36:
			return "the expert virus writer [6]";
		case 37:
			return "the expert virus writer [7]";
		case 38:
			return "the expert virus writer [8]";
		case 39:
			return "the expert virus writer [9]";
		case 40:
			return "the expert virus writer [10]";
		case 41:
			return "the adept virus writer [1]";
		case 42:
			return "the adept virus writer [2]";
		case 43:
			return "the adept virus writer [3]";
		case 44:
			return "the adept virus writer [4]";
		case 45:
			return "the adept virus writer [5]";
		case 46:
			return "the adept virus writer [6]";
		case 47:
			return "the adept virus writer [7]";
		case 48:
			return "the adept virus writer [8]";
		case 49:
			return "the adept virus writer [9]";
		case 50:
			return "the adept virus writer [10]";
		case 51:
			return "the guru virus writer [1]";
		case 52:
			return "the guru virus writer [2]";
		case 53:
			return "the guru virus writer [3]";
		case 54:
			return "the guru virus writer [4]";
		case 55:
			return "the guru virus writer [5]";
		case 56:
			return "the guru virus writer [6]";
		case 57:
			return "the guru virus writer [7]";
		case 58:
			return "the guru virus writer [8]";
		case 59:
			return "the guru virus writer [9]";
		case 60:
			return "the guru virus writer [10]";
		case LVL_IMMORT:
			return "the Immortal Wirus writer";
		case LVL_GOD:
			return "the Avatar of Virus writer";
		case LVL_GRGOD:
			return "the God of Virus writer";
		default:
			return "the Virus Writer";
		}
		break;

	case CLASS_CLERIC:
		switch (level)
		{
		case 1:
			return "the newbie sysadm [1]";
		case 2:
			return "the newbie sysadm [2]";
		case 3:
			return "the newbie sysadm [3]";
		case 4:
			return "the newbie sysadm [4]";
		case 5:
			return "the newbie sysadm [5]";
		case 6:
			return "the newbie sysadm [6]";
		case 7:
			return "the newbie sysadm [7]";
		case 8:
			return "the newbie sysadm [8]";
		case 9:
			return "the newbie sysadm [9]";
		case 10:
			return "the newbie sysadm [10]";
		case 11:
			return "the chump sysadm [1]";
		case 12:
			return "the chump sysadm [2]";
		case 13:
			return "the chump sysadm [3]";
		case 14:
			return "the chump sysadm [4]";
		case 15:
			return "the chump sysadm [5]";
		case 16:
			return "the chump sysadm [6]";
		case 17:
			return "the chump sysadm [7]";
		case 18:
			return "the chump sysadm [8]";
		case 19:
			return "the chump sysadm [9]";
		case 20:
			return "the chump sysadm [10]";
		case 21:
			return "the medium sysadm [1]";
		case 22:
			return "the medium sysadm [2]";
		case 23:
			return "the medium sysadm [3]";
		case 24:
			return "the medium sysadm [4]";
		case 25:
			return "the medium sysadm [5]";
		case 26:
			return "the medium sysadm [6]";
		case 27:
			return "the medium sysadm [7]";
		case 28:
			return "the medium sysadm [8]";
		case 29:
			return "the medium sysadm [9]";
		case 30:
			return "the medium sysadm [10]";
		case 31:
			return "the expert sysadm [1]";
		case 32:
			return "the expert sysadm [2]";
		case 33:
			return "the expert sysadm [3]";
		case 34:
			return "the expert sysadm [4]";
		case 35:
			return "the expert sysadm [5]";
		case 36:
			return "the expert sysadm [6]";
		case 37:
			return "the expert sysadm [7]";
		case 38:
			return "the expert sysadm [8]";
		case 39:
			return "the expert sysadm [9]";
		case 40:
			return "the expert sysadm [10]";
		case 41:
			return "the adept sysadm [1]";
		case 42:
			return "the adept sysadm [2]";
		case 43:
			return "the adept sysadm [3]";
		case 44:
			return "the adept sysadm [4]";
		case 45:
			return "the adept sysadm [5]";
		case 46:
			return "the adept sysadm [6]";
		case 47:
			return "the adept sysadm [7]";
		case 48:
			return "the adept sysadm [8]";
		case 49:
			return "the adept sysadm [9]";
		case 50:
			return "the adept sysadm [10]";
		case 51:
			return "the guru sysadm [1]";
		case 52:
			return "the guru sysadm [2]";
		case 53:
			return "the guru sysadm [3]";
		case 54:
			return "the guru sysadm [4]";
		case 55:
			return "the guru sysadm [5]";
		case 56:
			return "the guru sysadm [6]";
		case 57:
			return "the guru sysadm [7]";
		case 58:
			return "the guru sysadm [8]";
		case 59:
			return "the guru sysadm [9]";
		case 60:
			return "the guru sysadm [10]";
			/* no one ever thought up these titles 21-30 */
		case LVL_IMMORT:
			return "the Immortal Sysadm";
		case LVL_GOD:
			return "the Gos Sysadm";
		case LVL_GRGOD:
			return "the Spirit of Sysadn";
		default:
			return "the Sysadm";
		}
		break;

	case CLASS_THIEF:
		switch (level)
		{
		case 1:
			return "the newbie hacker [1]";
		case 2:
			return "the newbie hacker [2]";
		case 3:
			return "the newbie hacker [3]";
		case 4:
			return "the newbie hacker [4]";
		case 5:
			return "the newbie hacker [5]";
		case 6:
			return "the newbie hacker [6]";
		case 7:
			return "the newbie hacker [7]";
		case 8:
			return "the newbie hacker [8]";
		case 9:
			return "the newbie hacker [9]";
		case 10:
			return "the newbie hacker [10]";
		case 11:
			return "the chump hacker [1]";
		case 12:
			return "the chump hacker [2]";
		case 13:
			return "the chump hacker [3]";
		case 14:
			return "the chump hacker [4]";
		case 15:
			return "the chump hacker [5]";
		case 16:
			return "the chump hacker [6]";
		case 17:
			return "the chump hacker [7]";
		case 18:
			return "the chump hacker [8]";
		case 19:
			return "the chump hacker [9]";
		case 20:
			return "the chump hacker [10]";
		case 21:
			return "the medium hacker [1]";
		case 22:
			return "the medium hacker [2]";
		case 23:
			return "the medium hacker [3]";
		case 24:
			return "the medium hacker [4]";
		case 25:
			return "the medium hacker [5]";
		case 26:
			return "the medium hacker [6]";
		case 27:
			return "the medium hacker [7]";
		case 28:
			return "the medium hacker [8]";
		case 29:
			return "the medium hacker [9]";
		case 30:
			return "the medium hacker [10]";
		case 31:
			return "the expert hacker [1]";
		case 32:
			return "the expert hacker [2]";
		case 33:
			return "the expert hacker [3]";
		case 34:
			return "the expert hacker [4]";
		case 35:
			return "the expert hacker [5]";
		case 36:
			return "the expert hacker [6]";
		case 37:
			return "the expert hacker [7]";
		case 38:
			return "the expert hacker [8]";
		case 39:
			return "the expert hacker [9]";
		case 40:
			return "the expert hacker [10]";
		case 41:
			return "the adept hacker [1]";
		case 42:
			return "the adept hacker [2]";
		case 43:
			return "the adept hacker [3]";
		case 44:
			return "the adept hacker [4]";
		case 45:
			return "the adept hacker [5]";
		case 46:
			return "the adept hacker [6]";
		case 47:
			return "the adept hacker [7]";
		case 48:
			return "the adept hacker [8]";
		case 49:
			return "the adept hacker [9]";
		case 50:
			return "the adept hacker [10]";
		case 51:
			return "the guru hacker [1]";
		case 52:
			return "the guru hacker [2]";
		case 53:
			return "the guru hacker [3]";
		case 54:
			return "the guru hacker [4]";
		case 55:
			return "the guru hacker [5]";
		case 56:
			return "the guru hacker [6]";
		case 57:
			return "the guru hacker [7]";
		case 58:
			return "the guru hacker [8]";
		case 59:
			return "the guru hacker [9]";
		case 60:
			return "the guru hacker [10]";
			/* no one ever thought up these titles 21-30 */
		case LVL_IMMORT:
			return "the Immortal hacker";
		case LVL_GOD:
			return "the Demi God of hacker";
		case LVL_GRGOD:
			return "the God of hacker";
		default:
			return "the hacker";
		}
		break;

	case CLASS_WARRIOR:
		switch (level)
		{
		case 1:
			return "the newbie warrior [1]";
		case 2:
			return "the newbie warrior [2]";
		case 3:
			return "the newbie warrior [3]";
		case 4:
			return "the newbie warrior [4]";
		case 5:
			return "the newbie warrior [5]";
		case 6:
			return "the newbie warrior [6]";
		case 7:
			return "the newbie warrior [7]";
		case 8:
			return "the newbie warrior [8]";
		case 9:
			return "the newbie warrior [9]";
		case 10:
			return "the newbie warrior [10]";
		case 11:
			return "the chump warrior [1]";
		case 12:
			return "the chump warrior [2]";
		case 13:
			return "the chump warrior [3]";
		case 14:
			return "the chump warrior [4]";
		case 15:
			return "the chump warrior [5]";
		case 16:
			return "the chump warrior [6]";
		case 17:
			return "the chump warrior [7]";
		case 18:
			return "the chump warrior [8]";
		case 19:
			return "the chump warrior [9]";
		case 20:
			return "the chump warrior [10]";
		case 21:
			return "the medium warrior [1]";
		case 22:
			return "the medium warrior [2]";
		case 23:
			return "the medium warrior [3]";
		case 24:
			return "the medium warrior [4]";
		case 25:
			return "the medium warrior [5]";
		case 26:
			return "the medium warrior [6]";
		case 27:
			return "the medium warrior [7]";
		case 28:
			return "the medium warrior [8]";
		case 29:
			return "the medium warrior [9]";
		case 30:
			return "the medium warrior [10]";
		case 31:
			return "the expert warrior [1]";
		case 32:
			return "the expert warrior [2]";
		case 33:
			return "the expert warrior [3]";
		case 34:
			return "the expert warrior [4]";
		case 35:
			return "the expert warrior [5]";
		case 36:
			return "the expert warrior [6]";
		case 37:
			return "the expert warrior [7]";
		case 38:
			return "the expert warrior [8]";
		case 39:
			return "the expert warrior [9]";
		case 40:
			return "the expert warrior [10]";
		case 41:
			return "the adept warrior [1]";
		case 42:
			return "the adept warrior [2]";
		case 43:
			return "the adept warrior [3]";
		case 44:
			return "the adept warrior [4]";
		case 45:
			return "the adept warrior [5]";
		case 46:
			return "the adept warrior [6]";
		case 47:
			return "the adept warrior [7]";
		case 48:
			return "the adept warrior [8]";
		case 49:
			return "the adept warrior [9]";
		case 50:
			return "the adept warrior [10]";
		case 51:
			return "the guru warrior [1]";
		case 52:
			return "the guru warrior [2]";
		case 53:
			return "the guru warrior [3]";
		case 54:
			return "the guru warrior [4]";
		case 55:
			return "the guru warrior [5]";
		case 56:
			return "the guru warrior [6]";
		case 57:
			return "the guru warrior [7]";
		case 58:
			return "the guru warrior [8]";
		case 59:
			return "the guru warrior [9]";
		case 60:
			return "the guru warrior [10]";
			/* no one ever thought up these titles 21-30 */
		case LVL_IMMORT:
			return "the Immortal Warlord";
		case LVL_GOD:
			return "the Extirpator";
		case LVL_GRGOD:
			return "the God of war";
		default:
			return "the Warrior";
		}
		break;
	case CLASS_MARTIAL:
		switch (level)
		{
		case 1:
			return "the newbie martial [1]";
		case 2:
			return "the newbie martial [2]";
		case 3:
			return "the newbie martial [3]";
		case 4:
			return "the newbie martial [4]";
		case 5:
			return "the newbie martial [5]";
		case 6:
			return "the newbie martial [6]";
		case 7:
			return "the newbie martial [7]";
		case 8:
			return "the newbie martial [8]";
		case 9:
			return "the newbie martial [9]";
		case 10:
			return "the newbie martial [10]";
		case 11:
			return "the chump martial [1]";
		case 12:
			return "the chump martial [2]";
		case 13:
			return "the chump martial [3]";
		case 14:
			return "the chump martial [4]";
		case 15:
			return "the chump martial [5]";
		case 16:
			return "the chump martial [6]";
		case 17:
			return "the chump martial [7]";
		case 18:
			return "the chump martial [8]";
		case 19:
			return "the chump martial [9]";
		case 20:
			return "the chump martial [10]";
		case 21:
			return "the medium martial [1]";
		case 22:
			return "the medium martial [2]";
		case 23:
			return "the medium martial [3]";
		case 24:
			return "the medium martial [4]";
		case 25:
			return "the medium martial [5]";
		case 26:
			return "the medium martial [6]";
		case 27:
			return "the medium martial [7]";
		case 28:
			return "the medium martial [8]";
		case 29:
			return "the medium martial [9]";
		case 30:
			return "the medium martial [10]";
		case 31:
			return "the expert martial [1]";
		case 32:
			return "the expert martial [2]";
		case 33:
			return "the expert martial [3]";
		case 34:
			return "the expert martial [4]";
		case 35:
			return "the expert martial [5]";
		case 36:
			return "the expert martial [6]";
		case 37:
			return "the expert martial [7]";
		case 38:
			return "the expert martial [8]";
		case 39:
			return "the expert martial [9]";
		case 40:
			return "the expert martial [10]";
		case 41:
			return "the adept martial [1]";
		case 42:
			return "the adept martial [2]";
		case 43:
			return "the adept martial [3]";
		case 44:
			return "the adept martial [4]";
		case 45:
			return "the adept martial [5]";
		case 46:
			return "the adept martial [6]";
		case 47:
			return "the adept martial [7]";
		case 48:
			return "the adept martial [8]";
		case 49:
			return "the adept martial [9]";
		case 50:
			return "the adept martial [10]";
		case 51:
			return "the guru martial [1]";
		case 52:
			return "the guru martial [2]";
		case 53:
			return "the guru martial [3]";
		case 54:
			return "the guru martial [4]";
		case 55:
			return "the guru martial [5]";
		case 56:
			return "the guru martial [6]";
		case 57:
			return "the guru martial [7]";
		case 58:
			return "the guru martial [8]";
		case 59:
			return "the guru martial [9]";
		case 60:
			return "the guru martial [10]";
			/* No idea 21-30 by def the Legendary Knight */
		case LVL_IMMORT:
			return "il Martial Immortale";
		case LVL_GOD:
			return "il Dio Martial";
		case LVL_GRGOD:
			return "Lo Spirito Martial";
		default:
			return "il Martial leggendario";
		}
		break;
	case CLASS_LINKER:
		switch (level)
		{
		case 1:
			return "the newbie linker [1]";
		case 2:
			return "the newbie linker [2]";
		case 3:
			return "the newbie linker [3]";
		case 4:
			return "the newbie linker [4]";
		case 5:
			return "the newbie linker [5]";
		case 6:
			return "the newbie linker [6]";
		case 7:
			return "the newbie linker [7]";
		case 8:
			return "the newbie linker [8]";
		case 9:
			return "the newbie linker [9]";
		case 10:
			return "the newbie linker [10]";
		case 11:
			return "the chump linker [1]";
		case 12:
			return "the chump linker [2]";
		case 13:
			return "the chump linker [3]";
		case 14:
			return "the chump linker [4]";
		case 15:
			return "the chump linker [5]";
		case 16:
			return "the chump linker [6]";
		case 17:
			return "the chump linker [7]";
		case 18:
			return "the chump linker [8]";
		case 19:
			return "the chump linker [9]";
		case 20:
			return "the chump linker [10]";
		case 21:
			return "the medium linker [1]";
		case 22:
			return "the medium linker [2]";
		case 23:
			return "the medium linker [3]";
		case 24:
			return "the medium linker [4]";
		case 25:
			return "the medium linker [5]";
		case 26:
			return "the medium linker [6]";
		case 27:
			return "the medium linker [7]";
		case 28:
			return "the medium linker [8]";
		case 29:
			return "the medium linker [9]";
		case 30:
			return "the medium linker [10]";
		case 31:
			return "the expert linker [1]";
		case 32:
			return "the expert linker [2]";
		case 33:
			return "the expert linker [3]";
		case 34:
			return "the expert linker [4]";
		case 35:
			return "the expert linker [5]";
		case 36:
			return "the expert linker [6]";
		case 37:
			return "the expert linker [7]";
		case 38:
			return "the expert linker [8]";
		case 39:
			return "the expert linker [9]";
		case 40:
			return "the expert linker [10]";
		case 41:
			return "the adept linker [1]";
		case 42:
			return "the adept linker [2]";
		case 43:
			return "the adept linker [3]";
		case 44:
			return "the adept linker [4]";
		case 45:
			return "the adept linker [5]";
		case 46:
			return "the adept linker [6]";
		case 47:
			return "the adept linker [7]";
		case 48:
			return "the adept linker [8]";
		case 49:
			return "the adept linker [9]";
		case 50:
			return "the adept linker [10]";
		case 51:
			return "the guru linker [1]";
		case 52:
			return "the guru linker [2]";
		case 53:
			return "the guru linker [3]";
		case 54:
			return "the guru linker [4]";
		case 55:
			return "the guru linker [5]";
		case 56:
			return "the guru linker [6]";
		case 57:
			return "the guru linker [7]";
		case 58:
			return "the guru linker [8]";
		case 59:
			return "the guru linker [9]";
		case 60:
			return "the guru linker [10]";
			/* No idea 21-30 by def the Legendary Knight */
		case LVL_IMMORT:
			return "il Linker Immortale";
		case LVL_GOD:
			return "Il Dio dei Linker";
		case LVL_GRGOD:
			return "Lo spirito dei Linker";
		default:
			return "Il linker leggendario";
		}
		break;
	case CLASS_PSIONIC:
		switch (level)
		{
		case 1:
			return "the newbie psionic [1]";
		case 2:
			return "the newbie psionic [2]";
		case 3:
			return "the newbie psionic [3]";
		case 4:
			return "the newbie psionic [4]";
		case 5:
			return "the newbie psionic [5]";
		case 6:
			return "the newbie psionic [6]";
		case 7:
			return "the newbie psionic [7]";
		case 8:
			return "the newbie psionic [8]";
		case 9:
			return "the newbie psionic [9]";
		case 10:
			return "the newbie psionic [10]";
		case 11:
			return "the chump psionic [1]";
		case 12:
			return "the chump psionic [2]";
		case 13:
			return "the chump psionic [3]";
		case 14:
			return "the chump psionic [4]";
		case 15:
			return "the chump psionic [5]";
		case 16:
			return "the chump psionic [6]";
		case 17:
			return "the chump psionic [7]";
		case 18:
			return "the chump psionic [8]";
		case 19:
			return "the chump psionic [9]";
		case 20:
			return "the chump psionic [10]";
		case 21:
			return "the medium psionic [1]";
		case 22:
			return "the medium psionic [2]";
		case 23:
			return "the medium psionic [3]";
		case 24:
			return "the medium psionic [4]";
		case 25:
			return "the medium psionic [5]";
		case 26:
			return "the medium psionic [6]";
		case 27:
			return "the medium psionic [7]";
		case 28:
			return "the medium psionic [8]";
		case 29:
			return "the medium psionic [9]";
		case 30:
			return "the medium psionic [10]";
		case 31:
			return "the expert psionic [1]";
		case 32:
			return "the expert psionic [2]";
		case 33:
			return "the expert psionic [3]";
		case 34:
			return "the expert psionic [4]";
		case 35:
			return "the expert psionic [5]";
		case 36:
			return "the expert psionic [6]";
		case 37:
			return "the expert psionic [7]";
		case 38:
			return "the expert psionic [8]";
		case 39:
			return "the expert psionic [9]";
		case 40:
			return "the expert psionic [10]";
		case 41:
			return "the adept psionic [1]";
		case 42:
			return "the adept psionic [2]";
		case 43:
			return "the adept psionic [3]";
		case 44:
			return "the adept psionic [4]";
		case 45:
			return "the adept psionic [5]";
		case 46:
			return "the adept psionic [6]";
		case 47:
			return "the adept psionic [7]";
		case 48:
			return "the adept psionic [8]";
		case 49:
			return "the adept psionic [9]";
		case 50:
			return "the adept psionic [10]";
		case 51:
			return "the guru psionic [1]";
		case 52:
			return "the guru psionic [2]";
		case 53:
			return "the guru psionic [3]";
		case 54:
			return "the guru psionic [4]";
		case 55:
			return "the guru psionic [5]";
		case 56:
			return "the guru psionic [6]";
		case 57:
			return "the guru psionic [7]";
		case 58:
			return "the guru psionic [8]";
		case 59:
			return "the guru psionic [9]";
		case 60:
			return "the guru psionic [10]";
		case LVL_IMMORT:
			return "the Immortal Wirus writer";
		case LVL_GOD:
			return "the Avatar of psionic";
		case LVL_GRGOD:
			return "the God of psionic";
		default:
			return "the psionic";
		}
		break;
	}

	/* Default title for classes which do not have titles defined */
	return "the Classless";
}


/*
 * Default titles of female characters.
 */
const char *
title_female (int chclass, int level)
{
	if (level <= 0 || level > LVL_IMPL)
		return "the Woman";
	if (level == LVL_IMPL)
		return "the Implementress";

	switch (chclass)
	{

	case CLASS_MAGIC_USER:
		switch (level)
		{
		case 1:
			return "the newbie virus writer [1]";
		case 2:
			return "the newbie virus writer [2]";
		case 3:
			return "the newbie virus writer [3]";
		case 4:
			return "the newbie virus writer [4]";
		case 5:
			return "the newbie virus writer [5]";
		case 6:
			return "the newbie virus writer [6]";
		case 7:
			return "the newbie virus writer [7]";
		case 8:
			return "the newbie virus writer [8]";
		case 9:
			return "the newbie virus writer [9]";
		case 10:
			return "the newbie virus writer [10]";
		case 11:
			return "the chump virus writer [1]";
		case 12:
			return "the chump virus writer [2]";
		case 13:
			return "the chump virus writer [3]";
		case 14:
			return "the chump virus writer [4]";
		case 15:
			return "the chump virus writer [5]";
		case 16:
			return "the chump virus writer [6]";
		case 17:
			return "the chump virus writer [7]";
		case 18:
			return "the chump virus writer [8]";
		case 19:
			return "the chump virus writer [9]";
		case 20:
			return "the chump virus writer [10]";
		case 21:
			return "the medium virus writer [1]";
		case 22:
			return "the medium virus writer [2]";
		case 23:
			return "the medium virus writer [3]";
		case 24:
			return "the medium virus writer [4]";
		case 25:
			return "the medium virus writer [5]";
		case 26:
			return "the medium virus writer [6]";
		case 27:
			return "the medium virus writer [7]";
		case 28:
			return "the medium virus writer [8]";
		case 29:
			return "the medium virus writer [9]";
		case 30:
			return "the medium virus writer [10]";
		case 31:
			return "the expert virus writer [1]";
		case 32:
			return "the expert virus writer [2]";
		case 33:
			return "the expert virus writer [3]";
		case 34:
			return "the expert virus writer [4]";
		case 35:
			return "the expert virus writer [5]";
		case 36:
			return "the expert virus writer [6]";
		case 37:
			return "the expert virus writer [7]";
		case 38:
			return "the expert virus writer [8]";
		case 39:
			return "the expert virus writer [9]";
		case 40:
			return "the expert virus writer [10]";
		case 41:
			return "the adept virus writer [1]";
		case 42:
			return "the adept virus writer [2]";
		case 43:
			return "the adept virus writer [3]";
		case 44:
			return "the adept virus writer [4]";
		case 45:
			return "the adept virus writer [5]";
		case 46:
			return "the adept virus writer [6]";
		case 47:
			return "the adept virus writer [7]";
		case 48:
			return "the adept virus writer [8]";
		case 49:
			return "the adept virus writer [9]";
		case 50:
			return "the adept virus writer [10]";
		case 51:
			return "the guru virus writer [1]";
		case 52:
			return "the guru virus writer [2]";
		case 53:
			return "the guru virus writer [3]";
		case 54:
			return "the guru virus writer [4]";
		case 55:
			return "the guru virus writer [5]";
		case 56:
			return "the guru virus writer [6]";
		case 57:
			return "the guru virus writer [7]";
		case 58:
			return "the guru virus writer [8]";
		case 59:
			return "the guru virus writer [9]";
		case 60:
			return "the guru virus writer [10]";
		case LVL_IMMORT:
			return "the Immortal Virus Writer";
		case LVL_GOD:
			return "the Empress of Virus Writer";
		case LVL_GRGOD:
			return "the Goddess of Virus Writer";
		default:
			return "the Witch";
		}
		break;

	case CLASS_CLERIC:
		switch (level)
		{
		case 1:
			return "the newbie sysadm [1]";
		case 2:
			return "the newbie sysadm [2]";
		case 3:
			return "the newbie sysadm [3]";
		case 4:
			return "the newbie sysadm [4]";
		case 5:
			return "the newbie sysadm [5]";
		case 6:
			return "the newbie sysadm [6]";
		case 7:
			return "the newbie sysadm [7]";
		case 8:
			return "the newbie sysadm [8]";
		case 9:
			return "the newbie sysadm [9]";
		case 10:
			return "the newbie sysadm [10]";
		case 11:
			return "the chump sysadm [1]";
		case 12:
			return "the chump sysadm [2]";
		case 13:
			return "the chump sysadm [3]";
		case 14:
			return "the chump sysadm [4]";
		case 15:
			return "the chump sysadm [5]";
		case 16:
			return "the chump sysadm [6]";
		case 17:
			return "the chump sysadm [7]";
		case 18:
			return "the chump sysadm [8]";
		case 19:
			return "the chump sysadm [9]";
		case 20:
			return "the chump sysadm [10]";
		case 21:
			return "the medium sysadm [1]";
		case 22:
			return "the medium sysadm [2]";
		case 23:
			return "the medium sysadm [3]";
		case 24:
			return "the medium sysadm [4]";
		case 25:
			return "the medium sysadm [5]";
		case 26:
			return "the medium sysadm [6]";
		case 27:
			return "the medium sysadm [7]";
		case 28:
			return "the medium sysadm [8]";
		case 29:
			return "the medium sysadm [9]";
		case 30:
			return "the medium sysadm [10]";
		case 31:
			return "the expert sysadm [1]";
		case 32:
			return "the expert sysadm [2]";
		case 33:
			return "the expert sysadm [3]";
		case 34:
			return "the expert sysadm [4]";
		case 35:
			return "the expert sysadm [5]";
		case 36:
			return "the expert sysadm [6]";
		case 37:
			return "the expert sysadm [7]";
		case 38:
			return "the expert sysadm [8]";
		case 39:
			return "the expert sysadm [9]";
		case 40:
			return "the expert sysadm [10]";
		case 41:
			return "the adept sysadm [1]";
		case 42:
			return "the adept sysadm [2]";
		case 43:
			return "the adept sysadm [3]";
		case 44:
			return "the adept sysadm [4]";
		case 45:
			return "the adept sysadm [5]";
		case 46:
			return "the adept sysadm [6]";
		case 47:
			return "the adept sysadm [7]";
		case 48:
			return "the adept sysadm [8]";
		case 49:
			return "the adept sysadm [9]";
		case 50:
			return "the adept sysadm [10]";
		case 51:
			return "the guru sysadm [1]";
		case 52:
			return "the guru sysadm [2]";
		case 53:
			return "the guru sysadm [3]";
		case 54:
			return "the guru sysadm [4]";
		case 55:
			return "the guru sysadm [5]";
		case 56:
			return "the guru sysadm [6]";
		case 57:
			return "the guru sysadm [7]";
		case 58:
			return "the guru sysadm [8]";
		case 59:
			return "the guru sysadm [9]";
		case 60:
			return "the guru sysadm [10]";
			/* no one ever thought up these titles 21-30 */
		case LVL_IMMORT:
			return "the Immortal Sysadm";
		case LVL_GOD:
			return "the Gos Sysadm";
		case LVL_GRGOD:
			return "the Spirit of Sysadn";
		default:
			return "the Sysadm";
		}
		break;

	case CLASS_THIEF:
		switch (level)
		{
		case 1:
			return "the newbie hacker [1]";
		case 2:
			return "the newbie hacker [2]";
		case 3:
			return "the newbie hacker [3]";
		case 4:
			return "the newbie hacker [4]";
		case 5:
			return "the newbie hacker [5]";
		case 6:
			return "the newbie hacker [6]";
		case 7:
			return "the newbie hacker [7]";
		case 8:
			return "the newbie hacker [8]";
		case 9:
			return "the newbie hacker [9]";
		case 10:
			return "the newbie hacker [10]";
		case 11:
			return "the chump hacker [1]";
		case 12:
			return "the chump hacker [2]";
		case 13:
			return "the chump hacker [3]";
		case 14:
			return "the chump hacker [4]";
		case 15:
			return "the chump hacker [5]";
		case 16:
			return "the chump hacker [6]";
		case 17:
			return "the chump hacker [7]";
		case 18:
			return "the chump hacker [8]";
		case 19:
			return "the chump hacker [9]";
		case 20:
			return "the chump hacker [10]";
		case 21:
			return "the medium hacker [1]";
		case 22:
			return "the medium hacker [2]";
		case 23:
			return "the medium hacker [3]";
		case 24:
			return "the medium hacker [4]";
		case 25:
			return "the medium hacker [5]";
		case 26:
			return "the medium hacker [6]";
		case 27:
			return "the medium hacker [7]";
		case 28:
			return "the medium hacker [8]";
		case 29:
			return "the medium hacker [9]";
		case 30:
			return "the medium hacker [10]";
		case 31:
			return "the expert hacker [1]";
		case 32:
			return "the expert hacker [2]";
		case 33:
			return "the expert hacker [3]";
		case 34:
			return "the expert hacker [4]";
		case 35:
			return "the expert hacker [5]";
		case 36:
			return "the expert hacker [6]";
		case 37:
			return "the expert hacker [7]";
		case 38:
			return "the expert hacker [8]";
		case 39:
			return "the expert hacker [9]";
		case 40:
			return "the expert hacker [10]";
		case 41:
			return "the adept hacker [1]";
		case 42:
			return "the adept hacker [2]";
		case 43:
			return "the adept hacker [3]";
		case 44:
			return "the adept hacker [4]";
		case 45:
			return "the adept hacker [5]";
		case 46:
			return "the adept hacker [6]";
		case 47:
			return "the adept hacker [7]";
		case 48:
			return "the adept hacker [8]";
		case 49:
			return "the adept hacker [9]";
		case 50:
			return "the adept hacker [10]";
		case 51:
			return "the guru hacker [1]";
		case 52:
			return "the guru hacker [2]";
		case 53:
			return "the guru hacker [3]";
		case 54:
			return "the guru hacker [4]";
		case 55:
			return "the guru hacker [5]";
		case 56:
			return "the guru hacker [6]";
		case 57:
			return "the guru hacker [7]";
		case 58:
			return "the guru hacker [8]";
		case 59:
			return "the guru hacker [9]";
		case 60:
			return "the guru hacker [10]";
			/* no one ever thought up these titles 21-30 */
		case LVL_IMMORT:
			return "the Immortal hacker";
		case LVL_GOD:
			return "the Demi God of hacker";
		case LVL_GRGOD:
			return "the God of hacker";
		default:
			return "the hacker";
		}
		break;

	case CLASS_WARRIOR:
		switch (level)
		{
		case 1:
			return "the newbie warrior [1]";
		case 2:
			return "the newbie warrior [2]";
		case 3:
			return "the newbie warrior [3]";
		case 4:
			return "the newbie warrior [4]";
		case 5:
			return "the newbie warrior [5]";
		case 6:
			return "the newbie warrior [6]";
		case 7:
			return "the newbie warrior [7]";
		case 8:
			return "the newbie warrior [8]";
		case 9:
			return "the newbie warrior [9]";
		case 10:
			return "the newbie warrior [10]";
		case 11:
			return "the chump warrior [1]";
		case 12:
			return "the chump warrior [2]";
		case 13:
			return "the chump warrior [3]";
		case 14:
			return "the chump warrior [4]";
		case 15:
			return "the chump warrior [5]";
		case 16:
			return "the chump warrior [6]";
		case 17:
			return "the chump warrior [7]";
		case 18:
			return "the chump warrior [8]";
		case 19:
			return "the chump warrior [9]";
		case 20:
			return "the chump warrior [10]";
		case 21:
			return "the medium warrior [1]";
		case 22:
			return "the medium warrior [2]";
		case 23:
			return "the medium warrior [3]";
		case 24:
			return "the medium warrior [4]";
		case 25:
			return "the medium warrior [5]";
		case 26:
			return "the medium warrior [6]";
		case 27:
			return "the medium warrior [7]";
		case 28:
			return "the medium warrior [8]";
		case 29:
			return "the medium warrior [9]";
		case 30:
			return "the medium warrior [10]";
		case 31:
			return "the expert warrior [1]";
		case 32:
			return "the expert warrior [2]";
		case 33:
			return "the expert warrior [3]";
		case 34:
			return "the expert warrior [4]";
		case 35:
			return "the expert warrior [5]";
		case 36:
			return "the expert warrior [6]";
		case 37:
			return "the expert warrior [7]";
		case 38:
			return "the expert warrior [8]";
		case 39:
			return "the expert warrior [9]";
		case 40:
			return "the expert warrior [10]";
		case 41:
			return "the adept warrior [1]";
		case 42:
			return "the adept warrior [2]";
		case 43:
			return "the adept warrior [3]";
		case 44:
			return "the adept warrior [4]";
		case 45:
			return "the adept warrior [5]";
		case 46:
			return "the adept warrior [6]";
		case 47:
			return "the adept warrior [7]";
		case 48:
			return "the adept warrior [8]";
		case 49:
			return "the adept warrior [9]";
		case 50:
			return "the adept warrior [10]";
		case 51:
			return "the guru warrior [1]";
		case 52:
			return "the guru warrior [2]";
		case 53:
			return "the guru warrior [3]";
		case 54:
			return "the guru warrior [4]";
		case 55:
			return "the guru warrior [5]";
		case 56:
			return "the guru warrior [6]";
		case 57:
			return "the guru warrior [7]";
		case 58:
			return "the guru warrior [8]";
		case 59:
			return "the guru warrior [9]";
		case 60:
			return "the guru warrior [10]";
			/* no one ever thought up these titles 21-30 */
		case LVL_IMMORT:
			return "the Immortal Warlord";
		case LVL_GOD:
			return "the Extirpator";
		case LVL_GRGOD:
			return "the God of war";
		default:
			return "the Warrior";
		}
		break;
	case CLASS_MARTIAL:
		switch (level)
		{
		case 1:
			return "the newbie martial [1]";
		case 2:
			return "the newbie martial [2]";
		case 3:
			return "the newbie martial [3]";
		case 4:
			return "the newbie martial [4]";
		case 5:
			return "the newbie martial [5]";
		case 6:
			return "the newbie martial [6]";
		case 7:
			return "the newbie martial [7]";
		case 8:
			return "the newbie martial [8]";
		case 9:
			return "the newbie martial [9]";
		case 10:
			return "the newbie martial [10]";
		case 11:
			return "the chump martial [1]";
		case 12:
			return "the chump martial [2]";
		case 13:
			return "the chump martial [3]";
		case 14:
			return "the chump martial [4]";
		case 15:
			return "the chump martial [5]";
		case 16:
			return "the chump martial [6]";
		case 17:
			return "the chump martial [7]";
		case 18:
			return "the chump martial [8]";
		case 19:
			return "the chump martial [9]";
		case 20:
			return "the chump martial [10]";
		case 21:
			return "the medium martial [1]";
		case 22:
			return "the medium martial [2]";
		case 23:
			return "the medium martial [3]";
		case 24:
			return "the medium martial [4]";
		case 25:
			return "the medium martial [5]";
		case 26:
			return "the medium martial [6]";
		case 27:
			return "the medium martial [7]";
		case 28:
			return "the medium martial [8]";
		case 29:
			return "the medium martial [9]";
		case 30:
			return "the medium martial [10]";
		case 31:
			return "the expert martial [1]";
		case 32:
			return "the expert martial [2]";
		case 33:
			return "the expert martial [3]";
		case 34:
			return "the expert martial [4]";
		case 35:
			return "the expert martial [5]";
		case 36:
			return "the expert martial [6]";
		case 37:
			return "the expert martial [7]";
		case 38:
			return "the expert martial [8]";
		case 39:
			return "the expert martial [9]";
		case 40:
			return "the expert martial [10]";
		case 41:
			return "the adept martial [1]";
		case 42:
			return "the adept martial [2]";
		case 43:
			return "the adept martial [3]";
		case 44:
			return "the adept martial [4]";
		case 45:
			return "the adept martial [5]";
		case 46:
			return "the adept martial [6]";
		case 47:
			return "the adept martial [7]";
		case 48:
			return "the adept martial [8]";
		case 49:
			return "the adept martial [9]";
		case 50:
			return "the adept martial [10]";
		case 51:
			return "the guru martial [1]";
		case 52:
			return "the guru martial [2]";
		case 53:
			return "the guru martial [3]";
		case 54:
			return "the guru martial [4]";
		case 55:
			return "the guru martial [5]";
		case 56:
			return "the guru martial [6]";
		case 57:
			return "the guru martial [7]";
		case 58:
			return "the guru martial [8]";
		case 59:
			return "the guru martial [9]";
		case 60:
			return "the guru martial [10]";
			/* No idea 21-30 by def the Legendary Knight */
		case LVL_IMMORT:
			return "La Martial Immortale";
		case LVL_GOD:
			return "La Dea Martial";
		case LVL_GRGOD:
			return "Lo Spirito Martial";
		default:
			return "La Martial leggendaria";
		}
		break;
	case CLASS_LINKER:
		switch (level)
		{
		case 1:
			return "the newbie linker [1]";
		case 2:
			return "the newbie linker [2]";
		case 3:
			return "the newbie linker [3]";
		case 4:
			return "the newbie linker [4]";
		case 5:
			return "the newbie linker [5]";
		case 6:
			return "the newbie linker [6]";
		case 7:
			return "the newbie linker [7]";
		case 8:
			return "the newbie linker [8]";
		case 9:
			return "the newbie linker [9]";
		case 10:
			return "the newbie linker [10]";
		case 11:
			return "the chump linker [1]";
		case 12:
			return "the chump linker [2]";
		case 13:
			return "the chump linker [3]";
		case 14:
			return "the chump linker [4]";
		case 15:
			return "the chump linker [5]";
		case 16:
			return "the chump linker [6]";
		case 17:
			return "the chump linker [7]";
		case 18:
			return "the chump linker [8]";
		case 19:
			return "the chump linker [9]";
		case 20:
			return "the chump linker [10]";
		case 21:
			return "the medium linker [1]";
		case 22:
			return "the medium linker [2]";
		case 23:
			return "the medium linker [3]";
		case 24:
			return "the medium linker [4]";
		case 25:
			return "the medium linker [5]";
		case 26:
			return "the medium linker [6]";
		case 27:
			return "the medium linker [7]";
		case 28:
			return "the medium linker [8]";
		case 29:
			return "the medium linker [9]";
		case 30:
			return "the medium linker [10]";
		case 31:
			return "the expert linker [1]";
		case 32:
			return "the expert linker [2]";
		case 33:
			return "the expert linker [3]";
		case 34:
			return "the expert linker [4]";
		case 35:
			return "the expert linker [5]";
		case 36:
			return "the expert linker [6]";
		case 37:
			return "the expert linker [7]";
		case 38:
			return "the expert linker [8]";
		case 39:
			return "the expert linker [9]";
		case 40:
			return "the expert linker [10]";
		case 41:
			return "the adept linker [1]";
		case 42:
			return "the adept linker [2]";
		case 43:
			return "the adept linker [3]";
		case 44:
			return "the adept linker [4]";
		case 45:
			return "the adept linker [5]";
		case 46:
			return "the adept linker [6]";
		case 47:
			return "the adept linker [7]";
		case 48:
			return "the adept linker [8]";
		case 49:
			return "the adept linker [9]";
		case 50:
			return "the adept linker [10]";
		case 51:
			return "the guru linker [1]";
		case 52:
			return "the guru linker [2]";
		case 53:
			return "the guru linker [3]";
		case 54:
			return "the guru linker [4]";
		case 55:
			return "the guru linker [5]";
		case 56:
			return "the guru linker [6]";
		case 57:
			return "the guru linker [7]";
		case 58:
			return "the guru linker [8]";
		case 59:
			return "the guru linker [9]";
		case 60:
			return "the guru linker [10]";
			/* No idea 21-30 by def the Legendary Knight */
		case LVL_IMMORT:
			return "la Linker Immortale";
		case LVL_GOD:
			return "La Dea dei Linker";
		case LVL_GRGOD:
			return "Lo spirito dei Linker";
		default:
			return "La linker leggendaria";
		}
		break;
	case CLASS_PSIONIC:
		switch (level)
		{
		case 1:
			return "the newbie psionic [1]";
		case 2:
			return "the newbie psionic [2]";
		case 3:
			return "the newbie psionic [3]";
		case 4:
			return "the newbie psionic [4]";
		case 5:
			return "the newbie psionic [5]";
		case 6:
			return "the newbie psionic [6]";
		case 7:
			return "the newbie psionic [7]";
		case 8:
			return "the newbie psionic [8]";
		case 9:
			return "the newbie psionic [9]";
		case 10:
			return "the newbie psionic [10]";
		case 11:
			return "the chump psionic [1]";
		case 12:
			return "the chump psionic [2]";
		case 13:
			return "the chump psionic [3]";
		case 14:
			return "the chump psionic [4]";
		case 15:
			return "the chump psionic [5]";
		case 16:
			return "the chump psionic [6]";
		case 17:
			return "the chump psionic [7]";
		case 18:
			return "the chump psionic [8]";
		case 19:
			return "the chump psionic [9]";
		case 20:
			return "the chump psionic [10]";
		case 21:
			return "the medium psionic [1]";
		case 22:
			return "the medium psionic [2]";
		case 23:
			return "the medium psionic [3]";
		case 24:
			return "the medium psionic [4]";
		case 25:
			return "the medium psionic [5]";
		case 26:
			return "the medium psionic [6]";
		case 27:
			return "the medium psionic [7]";
		case 28:
			return "the medium psionic [8]";
		case 29:
			return "the medium psionic [9]";
		case 30:
			return "the medium psionic [10]";
		case 31:
			return "the expert psionic [1]";
		case 32:
			return "the expert psionic [2]";
		case 33:
			return "the expert psionic [3]";
		case 34:
			return "the expert psionic [4]";
		case 35:
			return "the expert psionic [5]";
		case 36:
			return "the expert psionic [6]";
		case 37:
			return "the expert psionic [7]";
		case 38:
			return "the expert psionic [8]";
		case 39:
			return "the expert psionic [9]";
		case 40:
			return "the expert psionic [10]";
		case 41:
			return "the adept psionic [1]";
		case 42:
			return "the adept psionic [2]";
		case 43:
			return "the adept psionic [3]";
		case 44:
			return "the adept psionic [4]";
		case 45:
			return "the adept psionic [5]";
		case 46:
			return "the adept psionic [6]";
		case 47:
			return "the adept psionic [7]";
		case 48:
			return "the adept psionic [8]";
		case 49:
			return "the adept psionic [9]";
		case 50:
			return "the adept psionic [10]";
		case 51:
			return "the guru psionic [1]";
		case 52:
			return "the guru psionic [2]";
		case 53:
			return "the guru psionic [3]";
		case 54:
			return "the guru psionic [4]";
		case 55:
			return "the guru psionic [5]";
		case 56:
			return "the guru psionic [6]";
		case 57:
			return "the guru psionic [7]";
		case 58:
			return "the guru psionic [8]";
		case 59:
			return "the guru psionic [9]";
		case 60:
			return "the guru psionic [10]";
		case LVL_IMMORT:
			return "La psionica Immortale";
		case LVL_GOD:
			return "L'Avatar Psionica";
		case LVL_GRGOD:
			return "La Dea  psionica";
		default:
			return "La psionica";
		}
		break;
	}

	/* Default title for classes which do not have titles defined */
	return "l'indefinito";
}

int
invalid_race (struct char_data *ch, struct obj_data *obj)
{
	if ((IS_OBJ_STAT (obj, ITEM_ANTI_UMANO) && IS_UMANO (ch)) ||
		(IS_OBJ_STAT (obj, ITEM_ANTI_CYBORG) && IS_CYBORG (ch)) ||
		(IS_OBJ_STAT (obj, ITEM_ANTI_MOTOCICLISTA) && IS_MOTOCICLISTA (ch)) ||
		(IS_OBJ_STAT (obj, ITEM_ANTI_ALIENO) && IS_ALIENO (ch)))
	{
		return 1;
	}
	else
	{
		return 0;
	}
}
