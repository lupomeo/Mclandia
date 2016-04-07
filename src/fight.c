
/* ************************************************************************
*   File: fight.c                                       Part of CircleMUD *
*  Usage: Combat system                                                   *
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
#include "handler.h"
#include "interpreter.h"
#include "db.h"
#include "spells.h"
#include "screen.h"
#include "constants.h"

/* Structures */
struct char_data *combat_list = NULL;	/* head of l-list of fighting chars */
struct char_data *next_combat_list = NULL;

/* External structures */
extern struct index_data *mob_index;
extern struct room_data *world;
extern struct message_list fight_messages[MAX_MESSAGES];
extern struct obj_data *object_list;
extern int pk_allowed;			/* see config.c */
extern int max_exp_gain;		/* see config.c */
extern int max_exp_loss;		/* see config.c */
extern int max_npc_corpse_time, max_pc_corpse_time;

/* External procedures */
char *fread_action (FILE * fl, int nr);
ACMD (do_flee);
int backstab_mult (int level);
int thaco (int ch_class, int level);
int ok_damage_shopkeeper (struct char_data *ch, struct char_data *victim);


/* local functions */
void perform_group_gain (struct char_data *ch, long base,
						 struct char_data *victim);
void dam_message (int dam, struct char_data *ch, struct char_data *victim,
				  int w_type);
void appear (struct char_data *ch);
void load_messages (void);
void check_killer (struct char_data *ch, struct char_data *vict);
void make_corpse (struct char_data *ch);
void change_alignment (struct char_data *ch, struct char_data *victim);
void death_cry (struct char_data *ch);
void raw_kill (struct char_data *ch);
void die (struct char_data *ch);
void group_gain (struct char_data *ch, struct char_data *victim);
void solo_gain (struct char_data *ch, struct char_data *victim);
char *replace_string (const char *str, const char *weapon_singular,
					  const char *weapon_plural);
void perform_violence (void);
int compute_armor_class (struct char_data *ch);
int compute_thaco (struct char_data *ch);
int check_use_fireweapon (struct char_data *ch);
int martial_artist_bare_hand_damage (struct char_data *ch);
void martial_artist_multiple_attack (struct char_data *ch);
void skill_multiple_attack (struct char_data *ch);
int is_in_group(struct char_data *ch, struct char_data *vict);


/* Weapon attack texts */
#ifdef OLD_MESSAGES
struct attack_hit_type attack_hit_text[] = {
	{"hit", "hits"},			/* 0 */
	{"sting", "stings"},
	{"whip", "whips"},
	{"slash", "slashes"},
	{"bite", "bites"},
	{"bludgeon", "bludgeons"},	/* 5 */
	{"crush", "crushes"},
	{"pound", "pounds"},
	{"claw", "claws"},
	{"maul", "mauls"},
	{"thrash", "thrashes"},		/* 10 */
	{"pierce", "pierces"},
	{"blast", "blasts"},
	{"punch", "punches"},
	{"stab", "stabs"},			/* 15 *//* pistol */
	{"fire", "fires"},			/* rifle */
	{"shoot", "shoots"},		/* shotgun */
	{"fire", "fires"},			/* bazooka */
	{"fire", "fires"}			/* grenade launcher */
};
#else
struct attack_hit_type attack_hit_text[] = {
	{"colpisci", "colpisce"},	/* 0 */
	{"pungi", "punge"},
	{"frusti", "frusta"},
	{"tagli", "taglia"},
	{"mordi", "morde"},
	{"martelli", "martella"},	/* 5 */
	{"schiacci", "schiaccia"},
	{"percuoti", "percuote"},
	{"artigli", "artiglia"},
	{"azzanni", "azzanna"},
	{"bastoni", "bastona"},		/* 10 */
	{"perfori", "perfora"},
	{"esplodi", "esplode"},
	{"picchi", "picchia"},
	{"pugnali", "pugnala"},		/* 15 */
};
#endif

#define IS_WEAPON(type) (((type) >= TYPE_HIT) && ((type) < TYPE_SUFFERING))

/* The Fight related routines */

void
appear (struct char_data *ch)
{
	if (affected_by_spell (ch, SPELL_INVISIBLE))
		affect_from_char (ch, SPELL_INVISIBLE);

	REMOVE_BIT (AFF_FLAGS (ch), AFF_INVISIBLE | AFF_HIDE);

	if (GET_LEVEL (ch) < LVL_IMMORT)
		act ("$n appare lentamente dalle ombre.", FALSE, ch, 0, 0, TO_ROOM);
	else
		act ("Senti una strana presenza e vedi $n apparire dal nulla.",
			 FALSE, ch, 0, 0, TO_ROOM);
}


int
compute_armor_class (struct char_data *ch)
{
	int armorclass = GET_AC (ch);

	if (AWAKE (ch))
		armorclass += dex_app[GET_DEX (ch)].defensive * 10;

	return (MAX (-100, armorclass));	/* -100 is lowest */
}


void
load_messages (void)
{
	FILE *fl;
	int i, type;
	struct message_type *messages;
	char chk[128];

	if (!(fl = fopen (MESS_FILE, "r")))
	{
		log ("SYSERR: Error reading combat message file %s: %s", MESS_FILE,
			 strerror (errno));
		exit (1);
	}
	for (i = 0; i < MAX_MESSAGES; i++)
	{
		fight_messages[i].a_type = 0;
		fight_messages[i].number_of_attacks = 0;
		fight_messages[i].msg = 0;
	}


	fgets (chk, 128, fl);
	while (!feof (fl) && (*chk == '\n' || *chk == '*'))
		fgets (chk, 128, fl);

	while (*chk == 'M')
	{
		fgets (chk, 128, fl);
		sscanf (chk, " %d\n", &type);
		for (i = 0;
			 (i < MAX_MESSAGES) && (fight_messages[i].a_type != type)
			 && (fight_messages[i].a_type); i++);
		if (i >= MAX_MESSAGES)
		{
			log
				("SYSERR: Too many combat messages.  Increase MAX_MESSAGES and recompile.");
			exit (1);
		}
		CREATE (messages, struct message_type, 1);
		fight_messages[i].number_of_attacks++;
		fight_messages[i].a_type = type;
		messages->next = fight_messages[i].msg;
		fight_messages[i].msg = messages;

		messages->die_msg.attacker_msg = fread_action (fl, i);
		messages->die_msg.victim_msg = fread_action (fl, i);
		messages->die_msg.room_msg = fread_action (fl, i);
		messages->miss_msg.attacker_msg = fread_action (fl, i);
		messages->miss_msg.victim_msg = fread_action (fl, i);
		messages->miss_msg.room_msg = fread_action (fl, i);
		messages->hit_msg.attacker_msg = fread_action (fl, i);
		messages->hit_msg.victim_msg = fread_action (fl, i);
		messages->hit_msg.room_msg = fread_action (fl, i);
		messages->god_msg.attacker_msg = fread_action (fl, i);
		messages->god_msg.victim_msg = fread_action (fl, i);
		messages->god_msg.room_msg = fread_action (fl, i);
		fgets (chk, 128, fl);
		while (!feof (fl) && (*chk == '\n' || *chk == '*'))
			fgets (chk, 128, fl);
	}

	fclose (fl);
}


void
update_pos (struct char_data *victim)
{
	if ((GET_HIT (victim) > 0) && (GET_POS (victim) > POS_STUNNED))
		return;
	else if (GET_HIT (victim) > 0)
		GET_POS (victim) = POS_STANDING;
	else if (GET_HIT (victim) <= -11)
		GET_POS (victim) = POS_DEAD;
	else if (GET_HIT (victim) <= -6)
		GET_POS (victim) = POS_MORTALLYW;
	else if (GET_HIT (victim) <= -3)
		GET_POS (victim) = POS_INCAP;
	else
		GET_POS (victim) = POS_STUNNED;
}


void
check_killer (struct char_data *ch, struct char_data *vict)
{
	char buf[256];

	if (PLR_FLAGGED (vict, PLR_KILLER) || PLR_FLAGGED (vict, PLR_THIEF))
		return;
	if (PLR_FLAGGED (ch, PLR_KILLER) || IS_NPC (ch) || IS_NPC (vict)
		|| ch == vict)
		return;

	SET_BIT (PLR_FLAGS (ch), PLR_KILLER);
	sprintf (buf,
			 "PC Killer bit set on %s for initiating attack on %s at %s.",
			 GET_NAME (ch), GET_NAME (vict), world[vict->in_room].name);
	mudlog (buf, BRF, LVL_IMMORT, TRUE);
	send_to_char ("Sei diventato PLAYER KILLER...\r\n", ch);
}


/* start one char fighting another (yes, it is horrible, I know... )  */
void
set_fighting (struct char_data *ch, struct char_data *vict)
{
	if (ch == vict)
		return;

	if (FIGHTING (ch))
	{
		core_dump ();
		return;
	}

	ch->next_fighting = combat_list;
	combat_list = ch;

	if (AFF_FLAGGED (ch, AFF_SLEEP))
		affect_from_char (ch, SPELL_SLEEP);

	FIGHTING (ch) = vict;
	GET_POS (ch) = POS_FIGHTING;

	if (!pk_allowed)
		check_killer (ch, vict);
}



/* remove a char from the list of fighting chars */
void
stop_fighting (struct char_data *ch)
{
	struct char_data *temp;

	if (ch == next_combat_list)
		next_combat_list = ch->next_fighting;

	REMOVE_FROM_LIST (ch, combat_list, next_fighting);
	ch->next_fighting = NULL;
	FIGHTING (ch) = NULL;
	GET_POS (ch) = POS_STANDING;

/* aggiunto da meo per settare la pos di fly se l'aff_bitvector e' attivo */
	if (AFF_FLAGGED (ch, AFF_FLY))
		GET_POS (ch) = POS_FLYING;

	update_pos (ch);
}



void
make_corpse (struct char_data *ch)
{
	struct obj_data *corpse, *o;
	struct obj_data *money;
	int i;

	corpse = create_obj ();

	corpse->item_number = NOTHING;
	corpse->in_room = NOWHERE;

	sprintf (buf2, "corpse corpo cadavere %s", GET_NAME (ch));
	corpse->name = str_dup (buf2);

	sprintf (buf2, "il cadavere di %s giace qui.", GET_NAME (ch));
	corpse->description = str_dup (buf2);

	sprintf (buf2, "il cadavere di %s", GET_NAME (ch));
	corpse->short_description = str_dup (buf2);

	GET_OBJ_TYPE (corpse) = ITEM_CONTAINER;
	GET_OBJ_WEAR (corpse) = ITEM_WEAR_TAKE;
	GET_OBJ_EXTRA (corpse) = ITEM_NODONATE;
	GET_OBJ_VAL (corpse, 0) = 0;	/* You can't store stuff in a corpse */
	GET_OBJ_VAL (corpse, 3) = 1;	/* corpse identifier */
	GET_OBJ_WEIGHT (corpse) = GET_WEIGHT (ch) + IS_CARRYING_W (ch);
	GET_OBJ_RENT (corpse) = 100000;
	if (IS_NPC (ch))
		GET_OBJ_TIMER (corpse) = max_npc_corpse_time;
	else
		GET_OBJ_TIMER (corpse) = max_pc_corpse_time;

	/* transfer character's inventory to the corpse */
	corpse->contains = ch->carrying;
	for (o = corpse->contains; o != NULL; o = o->next_content)
		o->in_obj = corpse;
	object_list_new_owner (corpse, NULL);

	/* transfer character's equipment to the corpse */
	for (i = 0; i < NUM_WEARS; i++)
		if (GET_EQ (ch, i))
			obj_to_obj (unequip_char (ch, i), corpse);

	/* transfer gold */
	if (GET_GOLD (ch) > 0)
	{
		/* following 'if' clause added to fix gold duplication loophole */
		if (IS_NPC (ch) || (!IS_NPC (ch) && ch->desc))
		{
			money = create_money (GET_GOLD (ch));
			obj_to_obj (money, corpse);
		}
		GET_GOLD (ch) = 0;
	}
	ch->carrying = NULL;
	IS_CARRYING_N (ch) = 0;
	IS_CARRYING_W (ch) = 0;

	obj_to_room (corpse, ch->in_room);
}


/* When ch kills victim */
void
change_alignment (struct char_data *ch, struct char_data *victim)
{
	/*
	 * new alignment change algorithm: if you kill a monster with alignment A,
	 * you move 1/16th of the way to having alignment -A.  Simple and fast.
	 */
	GET_ALIGNMENT (ch) += (-GET_ALIGNMENT (victim) - GET_ALIGNMENT (ch)) / 16;
}



void
death_cry (struct char_data *ch)
{
	int door;

	act ("Senti l'aggiacciante grido di morte di $n.", FALSE, ch, 0, 0,
		 TO_ROOM);

	for (door = 0; door < NUM_OF_DIRS; door++)
	{
		if (CAN_GO (ch, door))
		{
			send_to_room
				("Senti intorno l'aggiacciante grido di morte di qualcuno.\r\n",
				 world[ch->in_room].dir_option[door]->to_room);
		}
	}
}



void
raw_kill (struct char_data *ch)
{
	if (FIGHTING (ch))
	{
		if (IS_NPC (FIGHTING (ch)))
		{
			forget (FIGHTING (ch), ch);
		}
		stop_fighting (ch);

	}

	while (ch->affected)
	{
		affect_remove (ch, ch->affected);
	}

	death_cry (ch);

	make_corpse (ch);
	extract_char (ch);
}

/* abilita / disabilita nuovo sistema di gain */
#define NEW_HOLE_SYSTEM 1

void
die (struct char_data *ch)
{
	int xp;

	xp = GET_EXP (ch);


#ifdef NEW_HOLE_SYSTEM

	/* Nuovo sistema */
	if (GET_REAL_LEVEL (ch) < 5)
	{
		gain_exp (ch, -(GET_EXP (ch) / 6));
	}
	else if (GET_REAL_LEVEL (ch) < 10)
	{
		gain_exp (ch, -(GET_EXP (ch) / 5));
	}
	else if (GET_REAL_LEVEL (ch) < 15)
	{
		gain_exp (ch, -(GET_EXP (ch) / 4));
	}
	else if (GET_REAL_LEVEL (ch) < 20)
	{
		gain_exp (ch, -(GET_EXP (ch) / 3));
	}
	else if (GET_REAL_LEVEL (ch) < 40)
	{
		gain_exp (ch, -(GET_EXP (ch) / 2));
	}
	else
	{
		gain_exp (ch, -(GET_EXP (ch) / 2));
	}

#else

	/* vecchio sistema di gain system */
	gain_exp (ch, -(GET_EXP (ch) / 2));

#endif

	if (!IS_NPC (ch))
	{
		REMOVE_BIT (PLR_FLAGS (ch), PLR_KILLER | PLR_THIEF);
	}
	raw_kill (ch);
}



void
perform_group_gain (struct char_data *ch, long base, struct char_data *victim)
{
	long share;

	//share = MIN (max_exp_gain, MAX (1, base));
    share = MAX(1,base);
    
	if (share > 1)
	{
		sprintf (buf2,
				 "Ricevi la tua quota di punti esperienza -- %d punti.\r\n",
				 share);
		send_to_char (buf2, ch);
	}
	else
		send_to_char
			("Ricevi la tua quota di esperienza -- un solo misero punto!\r\n",
			 ch);

	gain_exp (ch, share);
	change_alignment (ch, victim);
}


void
group_gain (struct char_data *ch, struct char_data *victim)
{
	int tot_members;
    long base, tot_gain, share;
	struct char_data *k;
	struct follow_type *f;
    int group_max_level = 1;

	if (!(k = ch->master))
    {
		k = ch;
        group_max_level = GET_LEVEL(ch);
    }

	if (AFF_FLAGGED (k, AFF_GROUP) && (k->in_room == ch->in_room))
		tot_members = 1;
	else
		tot_members = 0;

	for (f = k->followers; f; f = f->next)
    {
		if (AFF_FLAGGED (f->follower, AFF_GROUP)
			&& f->follower->in_room == ch->in_room)
        {
			tot_members++;
        }

        if ( group_max_level < GET_LEVEL(f->follower) )
        {
            group_max_level = GET_LEVEL(f->follower);
        }
    }
    

	/* round up to the next highest tot_members */
	//tot_gain = ( GET_EXP(victim) / 3 ) + tot_members - 1;
    //tot_gain = ( GET_EXP(victim) /3 ) * (tot_members - 1);
    tot_gain = ( GET_EXP(victim) ) * (tot_members);

	/* prevent illegal xp creation when killing players */
	if (!IS_NPC (victim))
		tot_gain = MIN (max_exp_loss * 2 / 3, tot_gain);

	if (tot_members >= 1)
    {
		base = MAX (1, tot_gain / tot_members);
    }
	else
    {
		base = 0;
    }

	if (AFF_FLAGGED (k, AFF_GROUP) && k->in_room == ch->in_room)
    {
		base = modify_bonus_exp(k,victim,base);
        share = MIN (group_exp_cap(tot_members), base);
        share = group_level_ratio_exp(k,group_max_level,share);
        perform_group_gain (k, share, victim);
    }

	for (f = k->followers; f; f = f->next)
		if (AFF_FLAGGED (f->follower, AFF_GROUP)
			&& f->follower->in_room == ch->in_room)
        {
			base = modify_bonus_exp(f->follower,victim,base);
            share = MIN (group_exp_cap(tot_members), base);
            share = group_level_ratio_exp(f->follower,group_max_level,share);
            perform_group_gain (f->follower, share, victim);
        }
}


void
solo_gain (struct char_data *ch, struct char_data *victim)
{
	int exp;

	//exp = MIN (max_exp_gain, GET_EXP (victim)/3);
	exp = MIN (max_exp_gain, GET_EXP (victim));

	/* Calculate level-difference bonus */
    if (IS_NPC (ch))
		exp +=
			MAX (0,
				 (exp * MIN (4, (GET_LEVEL (victim) - GET_LEVEL (ch)))) / 8);
	else
		exp +=
			MAX (0,
				 (exp * MIN (8, (GET_LEVEL (victim) - GET_LEVEL (ch)))) / 8);

	exp = MAX (exp, 1);

    /* LEVEL CAP */
    exp = modify_bonus_exp(ch,victim,exp);
    
	if (exp > 1)
	{
		sprintf (buf2, "Ricevi %d punti esperienza.\r\n", exp);
		send_to_char (buf2, ch);
	}
	else
		send_to_char ("Ricevi un solo miserabile punto esperienza.\r\n", ch);

	gain_exp (ch, exp);
	change_alignment (ch, victim);
}


char *
replace_string (const char *str, const char *weapon_singular,
				const char *weapon_plural)
{
	static char buf[256];
	char *cp = buf;

	for (; *str; str++)
	{
		if (*str == '#')
		{
			switch (*(++str))
			{
			case 'W':
				for (; *weapon_plural; *(cp++) = *(weapon_plural++));
				break;
			case 'w':
				for (; *weapon_singular; *(cp++) = *(weapon_singular++));
				break;
			default:
				*(cp++) = '#';
				break;
			}
		}
		else
			*(cp++) = *str;

		*cp = 0;
	}							/* For */

	return (buf);
}


/* message for doing damage with a weapon */
void
dam_message (int dam, struct char_data *ch, struct char_data *victim,
			 int w_type)
{
	char *buf;
	int msgnum;

	static struct dam_weapon_type
	{
		const char *to_room;
		const char *to_char;
		const char *to_victim;
	}
	dam_weapons[] =
	{

		/* use #w for singular (i.e. "slash") and #W for plural (i.e. "slashes") */
#ifdef OLD_MESSAGES
		{
			"$n tries to #w $N, but misses.",	/* 0: 0     */
				"You try to #w $N, but miss.",
				"$n tries to #w you, but misses."}
		,
		{
			"$n tickles $N as $e #W $M.",	/* 1: 1..2  */
		"You tickle $N as you #w $M.", "$n tickles you as $e #W you."}
		,
		{
			"$n barely #W $N.",	/* 2: 3..4  */
		"You barely #w $N.", "$n barely #W you."}
		,
		{
			"$n #W $N.",		/* 3: 5..6  */
		"You #w $N.", "$n #W you."}
		,
		{
			"$n #W $N hard.",	/* 4: 7..10  */
		"You #w $N hard.", "$n #W you hard."}
		,
		{
			"$n #W $N very hard.",	/* 5: 11..14  */
		"You #w $N very hard.", "$n #W you very hard."}
		,
		{
			"$n #W $N extremely hard.",	/* 6: 15..19  */
		"You #w $N extremely hard.", "$n #W you extremely hard."}
		,
		{
			"$n massacres $N to small fragments with $s #w.",	/* 7: 19..23 */
				"You massacre $N to small fragments with your #w.",
				"$n massacres you to small fragments with $s #w."}
		,
		{
			"$n OBLITERATES $N with $s deadly #w!!",	/* 8: > 23   */
				"You OBLITERATE $N with your deadly #w!!",
				"$n OBLITERATES you with $s deadly #w!!"}
#else
		{
			"$n cerca di #w $N, ma manca.",
				"Cerchi di #w $N, ma manchi.", 
                "$n cerca di #w te, ma manca."}
		,
		{
			"$n sfiora appena $N.",
				"Sfiori appena $N.", 
                "$n ti sfiora appena."}
		,
		{
			"$n #W di striscio $N.",
				"#w $N. di striscio.", 
                "$n ti #W di striscio."}
		,
		{
		"$n #W $N.", "#w $N.", "$n ti #W."}
		,
		{
		"$n #W $N con forza.", "#w $N con forza.", "$n ti #W con forza."}
		,
		{
		"$n #W $N duramente.", "#w $N duramente.", "$n ti #W duramente."}
		,
		{
			"$n #W $N MOLTO duramente.",
				"#w $N MOLTO duramente.", 
                "$n ti #W MOLTO duramente."}
		,
		{
			"$n #W $N MASSACRANDOLO in tanti piccoli pezzi.",
				"#w $N MASSACRANDOLO in tanti piccoli pezzi.",
				"$n ti #W MASSACRANDOTI in tanti piccoli pezzi."}
		,
		{
			"$n #W $N cosi' forte che lo DISINTEGRA!!",
				"#w $N cosi' forte che lo DISINTEGRI!!",
				"$n ti #W cosi' forte che ti DISINTEGRA!!"}
#endif
	};


	w_type -= TYPE_HIT;			/* Change to base of table with text */

	if (dam == 0)
		msgnum = 0;
	else if (dam <= 2)
		msgnum = 1;
	else if (dam <= 4)
		msgnum = 2;
	else if (dam <= 6)
		msgnum = 3;
	else if (dam <= 10)
		msgnum = 4;
	else if (dam <= 14)
		msgnum = 5;
	else if (dam <= 19)
		msgnum = 6;
	else if (dam <= 23)
		msgnum = 7;
	else
		msgnum = 8;

	/* damage message to onlookers */
	buf = replace_string (dam_weapons[msgnum].to_room,
						  attack_hit_text[w_type].singular,
						  attack_hit_text[w_type].plural);
	act (buf, FALSE, ch, NULL, victim, TO_NOTVICT);

	/* damage message to damager */
	send_to_char (CCYEL (ch, C_CMP), ch);
	buf = replace_string (dam_weapons[msgnum].to_char,
						  attack_hit_text[w_type].singular,
						  attack_hit_text[w_type].plural);
	act (buf, FALSE, ch, NULL, victim, TO_CHAR);
	send_to_char (CCNRM (ch, C_CMP), ch);

	/* damage message to damagee */
	send_to_char (CCRED (victim, C_CMP), victim);
	buf = replace_string (dam_weapons[msgnum].to_victim,
						  attack_hit_text[w_type].singular,
						  attack_hit_text[w_type].plural);
	act (buf, FALSE, ch, NULL, victim, TO_VICT | TO_SLEEP);
	send_to_char (CCNRM (victim, C_CMP), victim);
}


/*
 * message for doing damage with a spell or skill
 *  C3.0: Also used for weapon damage on miss and death blows
 */
int
skill_message (int dam, struct char_data *ch, struct char_data *vict,
			   int attacktype)
{
	int i, j, nr;
	struct message_type *msg;

	struct obj_data *weap = GET_EQ (ch, WEAR_WIELD);

	for (i = 0; i < MAX_MESSAGES; i++)
	{
		if (fight_messages[i].a_type == attacktype)
		{
			nr = dice (1, fight_messages[i].number_of_attacks);
			for (j = 1, msg = fight_messages[i].msg; (j < nr) && msg; j++)
			{
				msg = msg->next;
			}

			if (!IS_NPC (vict) && (GET_LEVEL (vict) >= LVL_IMMORT))
			{
				act (msg->god_msg.attacker_msg, FALSE, ch, weap, vict,
					 TO_CHAR);
				act (msg->god_msg.victim_msg, FALSE, ch, weap, vict, TO_VICT);
				act (msg->god_msg.room_msg, FALSE, ch, weap, vict,
					 TO_NOTVICT);
			}
			else if (dam != 0)
			{
				if (GET_POS (vict) == POS_DEAD)
				{
					send_to_char (CCYEL (ch, C_CMP), ch);
					act (msg->die_msg.attacker_msg, FALSE, ch, weap, vict,
						 TO_CHAR);
					send_to_char (CCNRM (ch, C_CMP), ch);

					send_to_char (CCRED (vict, C_CMP), vict);
					act (msg->die_msg.victim_msg, FALSE, ch, weap, vict,
						 TO_VICT | TO_SLEEP);
					send_to_char (CCNRM (vict, C_CMP), vict);

					act (msg->die_msg.room_msg, FALSE, ch, weap, vict,
						 TO_NOTVICT);
				}
				else
				{
					send_to_char (CCYEL (ch, C_CMP), ch);
					act (msg->hit_msg.attacker_msg, FALSE, ch, weap, vict,
						 TO_CHAR);
					send_to_char (CCNRM (ch, C_CMP), ch);

					send_to_char (CCRED (vict, C_CMP), vict);
					act (msg->hit_msg.victim_msg, FALSE, ch, weap, vict,
						 TO_VICT | TO_SLEEP);
					send_to_char (CCNRM (vict, C_CMP), vict);

					act (msg->hit_msg.room_msg, FALSE, ch, weap, vict,
						 TO_NOTVICT);
				}
			}
			else if (ch != vict)	/* Dam == 0 */
			{
				send_to_char (CCYEL (ch, C_CMP), ch);
				act (msg->miss_msg.attacker_msg, FALSE, ch, weap, vict,
					 TO_CHAR);
				send_to_char (CCNRM (ch, C_CMP), ch);

				send_to_char (CCRED (vict, C_CMP), vict);
				act (msg->miss_msg.victim_msg, FALSE, ch, weap, vict,
					 TO_VICT | TO_SLEEP);
				send_to_char (CCNRM (vict, C_CMP), vict);

				act (msg->miss_msg.room_msg, FALSE, ch, weap, vict,
					 TO_NOTVICT);
			}
			return (1);
		}
	}
	return (0);
}

/*
 * Alert: As of bpl14, this function returns the following codes:
 *	< 0	Victim died.
 *	= 0	No damage.
 *	> 0	How much damage done.
 */

int
damage (struct char_data *ch, struct char_data *victim, int dam,
		int attacktype)
{
	/* Daniel Houghton's missile modification */
	bool missile = FALSE;

	if (GET_POS (victim) <= POS_DEAD)
	{
		/* This is "normal"-ish now with delayed extraction. -gg 3/15/2001 */
		if (PLR_FLAGGED (victim, PLR_NOTDEADYET)
			|| MOB_FLAGGED (victim, MOB_NOTDEADYET))
		{
			return (-1);
		}

		log ("SYSERR: Attempt to damage corpse '%s' in room #%d by '%s'.",
			 GET_NAME (victim), GET_ROOM_VNUM (IN_ROOM (victim)),
			 GET_NAME (ch));
		die (victim);
		return (-1);			/* -je, 7/7/92 */
	}


	/* Daniel Houghton's missile modification */
	if (ch->in_room != victim->in_room)
	{
		missile = TRUE;
	}


	/* peaceful rooms */
	if (ch != victim && ROOM_FLAGGED (ch->in_room, ROOM_PEACEFUL))
	{
		send_to_char
			("Questa stanza e' pacifica, aaah, che piacevole sensazione...\r\n",
			 ch);
		return (0);
	}

	/* shopkeeper protection */
	if (!ok_damage_shopkeeper (ch, victim))
	{
		return (0);
	}

	/* You can't damage an immortal! */
	if (!IS_NPC (victim) && (GET_LEVEL (victim) >= LVL_IMMORT))
	{
		dam = 0;
	}


	/* Daniel Houghton's missile modification */
	if ((victim != ch) && (!missile))
	{
		/* Start the attacker fighting the victim */
		if (GET_POS (ch) > POS_STUNNED && (FIGHTING (ch) == NULL))
		{
			set_fighting (ch, victim);
		}

		/* Start the victim fighting the attacker */
		if (GET_POS (victim) > POS_STUNNED && (FIGHTING (victim) == NULL))
		{
			set_fighting (victim, ch);
			if (MOB_FLAGGED (victim, MOB_MEMORY) && !IS_NPC (ch))
			{
				remember (victim, ch);
			}
		}
	}

	/* If you attack a pet, it hates your guts */
	if (victim->master == ch)
	{
		stop_follower (victim);
	}

	/* If the attacker is invisible, he becomes visible */
	if (AFF_FLAGGED (ch, AFF_INVISIBLE | AFF_HIDE))
	{
		appear (ch);
	}

	/* Cut damage in half if victim has sanct, to a minimum 1 */
	if (AFF_FLAGGED (victim, AFF_SANCTUARY) && dam >= 2)
	{
		dam /= 2;
	}

	/* Check for PK if this is not a PK MUD */
	if (!pk_allowed)
	{
		check_killer (ch, victim);
		if (PLR_FLAGGED (ch, PLR_KILLER) && (ch != victim))
		{
			dam = 0;
		}
	}

	/* Set the maximum damage per round and subtract the hit points */
	dam = MAX (MIN (dam, 100), 0);
	GET_HIT (victim) -= dam;

	/* Gain exp for the hit */
	if ((ch != victim) && (!missile))
	{
		gain_exp (ch, GET_LEVEL (victim) * dam);
	}

	update_pos (victim);

	/*
	 * skill_message sends a message from the messages file in lib/misc.
	 * dam_message just sends a generic "You hit $n extremely hard.".
	 * skill_message is preferable to dam_message because it is more
	 * descriptive.
	 * 
	 * If we are _not_ attacking with a weapon (i.e. a spell), always use
	 * skill_message. If we are attacking with a weapon: If this is a miss or a
	 * death blow, send a skill_message if one exists; if not, default to a
	 * dam_message. Otherwise, always send a dam_message.
	 */
	if ((!IS_WEAPON (attacktype)) || missile)
	{
		skill_message (dam, ch, victim, attacktype);
	}
	else
	{
		if (GET_POS (victim) == POS_DEAD || dam == 0)
		{
			if (!skill_message (dam, ch, victim, attacktype))
				dam_message (dam, ch, victim, attacktype);
		}
		else
		{
			dam_message (dam, ch, victim, attacktype);
		}
	}

	/* Use send_to_char -- act() doesn't send message if you are DEAD. */
	switch (GET_POS (victim))
	{
	case POS_MORTALLYW:
		act ("$n e' mortalmente ferito e potrebbe morire, se non aiutato.",
			 TRUE, victim, 0, 0, TO_ROOM);
		send_to_char
			("Sei mortalmente ferito e potresti morire, se non ti aiutano\r\n",
			 victim);
		break;

	case POS_INCAP:
		act ("$n e' inabilitato e sta morendo lentamente, se non aiutato.",
			 TRUE, victim, 0, 0, TO_ROOM);
		send_to_char
			("Sei inabilitato e stai morendo lentamente, se non ti aiutano.\r\n",
			 victim);
		break;

	case POS_STUNNED:
		act ("$n e' svenuto, ma potrebbe riprendere conoscenza.", TRUE,
			 victim, 0, 0, TO_ROOM);
		send_to_char
			("Sei svenuto, ma puoi ancora riprendere conoscenza.\r\n",
			 victim);
		break;

	case POS_DEAD:
		act ("$n e' morto!  R.I.P.", FALSE, victim, 0, 0, TO_ROOM);
		send_to_char ("Sei morto!  Spiacente...\r\n", victim);
		break;

	default:					/* >= POSITION SLEEPING */
		if (dam > (GET_MAX_HIT (victim) / 4))
		{
			send_to_char ("Hei! Questo colpo fa MALE!\r\n", victim);
		}

		if (GET_HIT (victim) < (GET_MAX_HIT (victim) / 4))
		{
			sprintf (buf2,
					 "%sSperi che le tue ferite smettano di SANGUINARE cosi' tanto!%s\r\n",
					 CCRED (victim, C_SPR), CCNRM (victim, C_SPR));
			send_to_char (buf2, victim);
			if (ch != victim && MOB_FLAGGED (victim, MOB_WIMPY))
			{
				do_flee (victim, NULL, 0, 0);
			}
		}
		if (!IS_NPC (victim) && GET_WIMP_LEV (victim) && (victim != ch) &&
			GET_HIT (victim) < GET_WIMP_LEV (victim) && GET_HIT (victim) > 0)
		{
			send_to_char ("Sottrai dallo scontro, e cerchi di scappare!\r\n",
						  victim);
			do_flee (victim, NULL, 0, 0);
		}
		break;
	}

	/* Help out poor linkless people who are attacked */
	if (!IS_NPC (victim) && !(victim->desc) && GET_POS (victim) > POS_STUNNED)
	{
		do_flee (victim, NULL, 0, 0);
		if (!FIGHTING (victim))
		{
			act ("$n e' stato salvato da forze divine.", FALSE, victim, 0, 0,
				 TO_ROOM);
			GET_WAS_IN (victim) = victim->in_room;
			char_from_room (victim);
			char_to_room (victim, 0);
		}
	}

	/* stop someone from fighting if they're stunned or worse */
	if ((GET_POS (victim) <= POS_STUNNED) && (FIGHTING (victim) != NULL))
	{
		stop_fighting (victim);
	}

	/* Uh oh.  Victim died. */
	if (GET_POS (victim) == POS_DEAD)
	{
		if ((ch != victim) && (IS_NPC (victim) || victim->desc))
		{
			if (AFF_FLAGGED (ch, AFF_GROUP))
			{
				group_gain (ch, victim);
			}
			else
			{
				solo_gain (ch, victim);
			}
		}

		if (!IS_NPC (victim))
		{
			sprintf (buf2, "%s e' stato ucciso da %s a %s \r\n",
					 GET_NAME (victim), GET_NAME (ch),
					 world[victim->in_room].name);
			mudlog (buf2, BRF, LVL_IMMORT, TRUE);
			send_to_all (buf2);
			if (MOB_FLAGGED (ch, MOB_MEMORY))
			{
				forget (ch, victim);
			}
		}

		die (victim);
		return (-1);
	}
	return (dam);
}



void
hit (struct char_data *ch, struct char_data *victim, int type)
{
	struct obj_data *wielded = GET_EQ (ch, WEAR_WIELD);
	int w_type, victim_ac, calc_thaco, dam, diceroll;

    /* Sanity check, verifico se esiste la vittima */
    if (!victim)
    {
        if (FIGHTING (ch) && FIGHTING (ch) == victim)
		{
			stop_fighting (ch);
		}
		return;
    }


	/* Do some sanity checking, in case someone flees, etc. */
	if (ch->in_room != victim->in_room)
	{
		if (FIGHTING (ch) && FIGHTING (ch) == victim)
		{
			stop_fighting (ch);
		}
		return;
	}

	/* Find the weapon type (for display purposes only) */
	if (wielded && GET_OBJ_TYPE (wielded) == ITEM_WEAPON)
		w_type = GET_OBJ_VAL (wielded, 3) + TYPE_HIT;
	else
	{
		if (IS_NPC (ch) && (ch->mob_specials.attack_type != 0))
		{
			w_type = ch->mob_specials.attack_type + TYPE_HIT;
		}
		else
		{
			w_type = TYPE_HIT;
		}
	}

	/* Calculate the THAC0 of the attacker */
	if (!IS_NPC (ch))
	{
		calc_thaco = thaco ((int) GET_CLASS (ch), (int) GET_LEVEL (ch));
	}
	else						/* THAC0 for monsters is set in the HitRoll */
	{
		calc_thaco = 20;
	}
	calc_thaco -= str_app[STRENGTH_APPLY_INDEX (ch)].tohit;
	calc_thaco -= GET_HITROLL (ch);
	if (IS_MARTIAL (ch))
	{
		calc_thaco -= (int) ((GET_DEX (ch) - 13) / 1.5);
	}
	else
	{
		calc_thaco -= (int) ((GET_INT (ch) - 13) / 1.5);	/* Intelligence helps! */
	}
	calc_thaco -= (int) ((GET_WIS (ch) - 13) / 1.5);	/* So does wisdom */


	/* Calculate the raw armor including magic armor.  Lower AC is better. */
	victim_ac = compute_armor_class (victim) / 10;

	/* roll the die and take your chances... */
	diceroll = number (1, 20);

	/* decide whether this is a hit or a miss */
	if ((((diceroll < 20) && AWAKE (victim)) &&
		 ((diceroll == 1) || ((calc_thaco - diceroll) > victim_ac))))
	{
		/* the attacker missed the victim */
		if (type == SKILL_BACKSTAB)
		{
			damage (ch, victim, 0, SKILL_BACKSTAB);
		}
		else
		{
			damage (ch, victim, 0, w_type);
		}
	}
	else
	{
		/* okay, we know the guy has been hit.  now calculate damage. */

		/* Start with the damage bonuses: the damroll and strength apply */
		dam = str_app[STRENGTH_APPLY_INDEX (ch)].todam;
		dam += GET_DAMROLL (ch);

		/* Maybe holding arrow? */
		if (wielded && GET_OBJ_TYPE (wielded) == ITEM_WEAPON)
		{
			/* Add weapon-based damage if a weapon is being wielded */
			dam += dice (GET_OBJ_VAL (wielded, 1), GET_OBJ_VAL (wielded, 2));
		}
		else
		{
			/* If no weapon, add bare hand damage instead */
			if (IS_NPC (ch))
			{
				dam += 	dice (ch->mob_specials.damnodice,
						  ch->mob_specials.damsizedice);                
			}
			else
			{
				if (IS_MARTIAL (ch))
				{
					dam += martial_artist_bare_hand_damage (ch);
				}
				else
				{
					dam += number (0, 2);	/* Max 2 bare hand damage for players */
				}
			}
		}

		/*
		 * Include a damage multiplier if victim isn't ready to fight:
		 *
		 * Position sitting  1.33 x normal
		 * Position resting  1.66 x normal
		 * Position sleeping 2.00 x normal
		 * Position stunned  2.33 x normal
		 * Position incap    2.66 x normal
		 * Position mortally 3.00 x normal
		 *
		 * Note, this is a hack because it depends on the particular
		 * values of the POSITION_XXX constants.
		 */
		if (GET_POS (victim) < POS_FIGHTING)
		{
			dam *= 1 + (POS_FIGHTING - GET_POS (victim)) / 3;
		}

		/* at least 1 hp damage min per hit */
		dam = MAX (1, dam);

		if (type == SKILL_BACKSTAB)
		{
			dam *= backstab_mult (GET_LEVEL (ch));
			damage (ch, victim, dam, SKILL_BACKSTAB);
		}
		else
		{
			damage (ch, victim, dam, w_type);
		}
	}
}

/* Danno dei Martial Artist a mani nude */
int
martial_artist_bare_hand_damage (struct char_data *ch)
{
	if (GET_LEVEL (ch) <= 4)
	{
		return dice (1, 4);		// 1-4
	}
	else if (GET_LEVEL (ch) <= 8)
	{
		return dice (1, 4) + 2;	// 3-6
	}
	else if (GET_LEVEL (ch) <= 12)
	{
		return dice (1, 6);		// 2-8
	}
	else if (GET_LEVEL (ch) <= 20)
	{
		return dice (2, 6);		// 2-12
	}
	else if (GET_LEVEL (ch) <= 30)
	{
		return dice (4, 4);		// 4-16
	}
	else if (GET_LEVEL (ch) <= 40)
	{
		return dice (4, 4) + 2;	// 6-18
	}
	else if (GET_LEVEL (ch) <= 50)
	{
		return dice (5, 4);		// 5-20
	}
	else if (GET_LEVEL (ch) < LVL_IMMORT)
	{
		return dice (6, 4) + 2;	// 6-24
	}
	else
	{
		return dice (10, 3);	// 10-30
	}
}


void
martial_artist_multiple_attack (struct char_data *ch)
{
	/* corrispondente alla SKILL_SECOND ma intriseco ai MA */
	if (GET_LEVEL (ch) >= 15)
	{
		//if (number (1, 101) > (63 - GET_LEVEL (ch)))
		{
			if (FIGHTING (ch))
			{
				hit (ch, FIGHTING (ch), TYPE_UNDEFINED);
			}
		}
	}
	/* corrispondente alla SKILL_THIRD ma intriseco ai MA */
	if (GET_LEVEL (ch) >= 30)
	{
		//if (number (1, 101) > (63 - GET_LEVEL (ch)))
		{
			if (FIGHTING (ch))
			{
				hit (ch, FIGHTING (ch), TYPE_UNDEFINED);
			}
		}
	}

	/* Quarto attacco, proprio dei MA */
	if (GET_LEVEL (ch) >= 45)
	{
		//if (number (1, 101) > (63 - GET_LEVEL (ch)))
		{
			if (FIGHTING (ch))
			{
				hit (ch, FIGHTING (ch), TYPE_UNDEFINED);
			}
		}
	}
}

void
skill_multiple_attack (struct char_data *ch)
{
	if ((GET_SKILL (ch, SKILL_SECOND) > 0))
	{
		if (GET_SKILL (ch, SKILL_SECOND) >
			number (1, 200) - 5 * GET_LEVEL (ch))
		{
			if (FIGHTING (ch))
			{
				hit (ch, FIGHTING (ch), TYPE_UNDEFINED);
			}
		}
		if ((GET_SKILL (ch, SKILL_THIRD) > 0))
		{
			if (GET_SKILL (ch, SKILL_THIRD) >
				number (1, 500) - 10 * GET_LEVEL (ch))
			{
				if (FIGHTING (ch))
				{
					hit (ch, FIGHTING (ch), TYPE_UNDEFINED);
				}
			}
		}
	}
}


/* control the fights going on.  Called every 2 seconds from comm.c. */
void
perform_violence (void)
{
	struct char_data *ch;

	for (ch = combat_list; ch; ch = next_combat_list)
	{
		next_combat_list = ch->next_fighting;

		if (FIGHTING (ch) == NULL || ch->in_room != FIGHTING (ch)->in_room)
		{
			stop_fighting (ch);
			continue;
		}

		if (IS_NPC (ch))
		{
            /* Rimosso il check del lag per i mob
			if (GET_MOB_WAIT (ch) > 0)
			{
				GET_MOB_WAIT (ch) -= PULSE_VIOLENCE;
				continue;
			}
            */
			GET_MOB_WAIT (ch) = 0;

			if (GET_POS (ch) < POS_FIGHTING)
			{
				if (GET_POS (ch) <= POS_SITTING)
				{
					act ("$n si alza in piedi.", TRUE, ch, 0, 0, TO_ROOM);
					GET_POS (ch) = POS_STANDING;
				}
				else
				{
					GET_POS (ch) = POS_FIGHTING;
					act ("$n scatta all'attacco!", TRUE, ch, 0, 0, TO_ROOM);
				}
				continue;
			}
            if (!check_use_fireweapon (ch)) 
            {
			    if (GET_CLASS (ch) == CLASS_MARTIAL)
			    {
				    martial_artist_multiple_attack (ch);
                }
			}

		}

		if (GET_POS (ch) < POS_FIGHTING)
		{
			send_to_char ("Non puoi combattere da seduto!!\r\n", ch);
			continue;
		}

		if (!check_use_fireweapon (ch))
		{
			hit (ch, FIGHTING (ch), TYPE_UNDEFINED);

			if (!IS_NPC (ch))
			{
                if (IS_MARTIAL (ch))
			    {
				    martial_artist_multiple_attack (ch);
			    }

/* aggiunto da Meo */
			/* SW - Spostato in una nuova funzione skill_multiple_attack() */
				skill_multiple_attack (ch);
			}

/* fine aggiunta meo*/
		}

		/* XXX: Need to see if they can handle "" instead of NULL. */
		if (MOB_FLAGGED (ch, MOB_SPEC)
			&& mob_index[GET_MOB_RNUM (ch)].func != NULL)
		{
			(mob_index[GET_MOB_RNUM (ch)].func) (ch, ch, 0, "");
		}
	}
}

int
check_use_fireweapon (struct char_data *ch)
{
	struct obj_data *weapon;

	weapon = GET_EQ (ch, WEAR_HOLD);

	/* controllo se ho qualcosa in mano  o weapon == NULL */
	if (!weapon)
	{
		return (FALSE);
	}

	/* l'oggetto in mano e' una arma da fuoco */
	if ((GET_OBJ_TYPE (weapon) == ITEM_FIREWEAPON))
	{
		return (TRUE);
	}
	return (FALSE);
}

void 
area_damage(struct char_data *ch, int dam, int spellnum)
{
	struct char_data *tch, *next_tch;

    for (tch = world[ch->in_room].people; tch; tch = next_tch)
	{
		next_tch = tch->next_in_room;

		/*
		 * The skips: 1: the caster
         *            2: the group of caster
		 *            3: immortals
		 *            4: if no pk on this mud, skips over all players
		 *            5: pets (charmed NPCs)
		 */

		if (tch == ch)
			continue;
        if (is_in_group(ch,tch))
            continue;
		if (!IS_NPC (tch) && GET_LEVEL (tch) >= LVL_IMMORT)
			continue;
		if (!pk_allowed && !IS_NPC (ch) && !IS_NPC (tch))
			continue;
		if (!IS_NPC (ch) && IS_NPC (tch) && AFF_FLAGGED (tch, AFF_CHARM))
			continue;

		/* Doesn't matter if they die here so we don't check. -gg 6/24/98 */
		damage (ch, tch, dam, spellnum);
	}
}

int 
is_in_group(struct char_data *ch, struct char_data *vict)
{
    struct char_data *k;
	struct follow_type *f;

	if (!AFF_FLAGGED (ch, AFF_GROUP))
		return (FALSE);
	else
	{
		k = (ch->master ? ch->master : ch);
        if (AFF_FLAGGED (k, AFF_GROUP))
		{
			if (vict == k)
            {
                return TRUE;
            }
            
		}

		for (f = k->followers; f; f = f->next)
		{
			/* fa parte del gruppo */
            if (AFF_FLAGGED (f->follower, AFF_GROUP))
            {
				if (vict == f->follower)
                {
                    return TRUE;
                }
            }
		}
	}
    return FALSE;
}


