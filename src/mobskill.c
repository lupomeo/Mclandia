
/* ************************************************************************
*   File: mobskill.c                                 Part of MclandiaMUD *
*  Usage: implementation of skills for mobs                               *
*                                                                         *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  (c) 2002 Sidewinder & Meo                                              *
*  Mclandia is based on CircleMUD                                         *
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
struct char_data *find_MobInRoomWithFunc (int room,
										  int (name) (struct char_data * ch,
													  void *me, int cmd,
													  char *argument));
void add_follower (struct char_data *ch, struct char_data *leader);
void gain_level (struct char_data *ch);
void look_at_char (struct char_data *i, struct char_data *ch);
ACMD (do_drop);
ACMD (do_gen_door);
ACMD (do_say);
int compute_armor_class (struct char_data *ch);
void sort_spells (void);
int compare_spells (const void *x, const void *y);
const char *how_good (int percent);
void list_skills (struct char_data *ch);
int check_use_fireweapon (struct char_data *ch);



/* for local functions  see mobskill.h */





/*************************************************************/


void
do_mob_steal (struct char_data *ch, struct char_data *victim)
{
    int gold;

	if (IS_NPC (victim))
		return;
	if (GET_LEVEL (victim) >= LVL_IMMORT)
		return;
	if (!CAN_SEE (ch, victim))
		return;

	if (AWAKE (victim) && (number (0, GET_LEVEL (ch)) == 0))
	{
		act ("You discover that $n has $s hands in your wallet.", FALSE, ch,
			 0, victim, TO_VICT);
		act ("$n tries to steal gold from $N.", TRUE, ch, 0, victim,
			 TO_NOTVICT);
	}
	else
	{
		/* Steal some gold coins */
		gold = (int) ((GET_GOLD (victim) * number (1, 10)) / 100);
		if (gold > 0)
		{
			GET_GOLD (ch) += gold;
			GET_GOLD (victim) -= gold;
		}
	}
}


void
do_mob_bash (struct char_data *ch, struct char_data *vict)
{

	int hit_roll = 0, to_hit = 0;

	hit_roll = number (1, 100) + GET_STR (ch);
	to_hit = (100 - (int) (100 * GET_LEVEL (ch) / 250));
	if (GET_LEVEL (vict) >= LVL_IMMORT)
		hit_roll = 0;

	if (hit_roll < to_hit)
	{
		GET_POS (ch) = POS_SITTING;
		damage (ch, vict, 0, SKILL_BASH);
	}
	else
	{
		GET_POS (vict) = POS_SITTING;
		damage (ch, vict, GET_LEVEL (ch), SKILL_BASH);
		WAIT_STATE (vict, PULSE_VIOLENCE * 2);
		WAIT_STATE (ch, PULSE_VIOLENCE * 3);
	}
}

void
do_mob_disarm (struct char_data *ch, struct char_data *vict)
{

	struct obj_data *weap;
	int hit_roll = 0, to_hit = 0;

	if (check_use_fireweapon (ch) == TRUE ||
		(weap = GET_EQ (ch, WEAR_WIELD)) > 0)
	{
		send_to_char ("Non puoi disarmare con le mani occupate!\r\n", ch);
		return;
	}

    
	if (check_use_fireweapon (vict) == FALSE &&
		!(weap = GET_EQ (vict, WEAR_WIELD)))
	{
		send_to_char ("E' gia disarmato!\r\n", ch);
		return;
	}

	hit_roll = number (1, 100) + GET_DEX (ch)- GET_DEX (vict);
	to_hit = (100 - (int) (100 * GET_LEVEL (ch) / 250));
	if (GET_LEVEL (vict) >= LVL_IMMORT)
		hit_roll = 0;

	if (hit_roll < to_hit)
	{
		damage (ch, vict, 0, SKILL_DISARM);
		act ("Fallisci nel tentativo di disarmare $N.", FALSE, ch, 0, vict,
			 TO_CHAR);
		act ("$n fallisce nel tentativo di disarmarti.", FALSE, ch, 0, vict,
			 TO_VICT);
		act ("$n fallisce nel tentativo di disarmare $N.", FALSE, ch, 0, vict,
			 TO_NOTVICT);
	}
	else
	{
		damage (ch, vict, 0, SKILL_DISARM);
		if (check_use_fireweapon (vict))
		{
			weap = GET_EQ (vict, WEAR_HOLD);

			act ("Con una mossa spettacolare disarmi $p dalle mani di $N.",
				 FALSE, ch, weap, vict, TO_CHAR);
			act
				("$n ti disarma $p dalle tue mani con un abile gioco delle mani!",
				 FALSE, ch, weap, vict, TO_VICT);
			act
				("$n disarma $p dalle mani di $N con un abile gioco delle mani!",
				 FALSE, ch, weap, vict, TO_NOTVICT);
			obj_to_room (unequip_char (vict, WEAR_HOLD), vict->in_room);
		}
		else if (GET_EQ (vict, WEAR_WIELD))
		{
			weap = GET_EQ (vict, WEAR_WIELD);

			act ("Con una mossa spettacolare disarmi $p dalle mani di $N.",
				 FALSE, ch, weap, vict, TO_CHAR);
			act
				("$n ti disarma $p dalle tue mani con un abile gioco delle mani!",
				 FALSE, ch, weap, vict, TO_VICT);
			act
				("$n disarma $p dalle mani di $N con un abile gioco delle mani!",
				 FALSE, ch, weap, vict, TO_NOTVICT);
			obj_to_room (unequip_char (vict, WEAR_WIELD), vict->in_room);
		}
	}
    WAIT_STATE (ch, PULSE_VIOLENCE);
}

void
do_mob_kick (struct char_data *ch, struct char_data *vict)
{

	int hit_roll = 0, to_hit = 0;

	hit_roll = number (1, 100) + GET_STR (ch);
	to_hit = (100 - (int) (100 * GET_LEVEL (ch) / 250));
	if (GET_LEVEL (vict) >= LVL_IMMORT)
		hit_roll = 0;

	if (hit_roll < to_hit)
	{
		damage (ch, vict, 0, SKILL_KICK);
	}
	else
	{
		damage (ch, vict, GET_LEVEL (ch) / 2, SKILL_KICK);
		WAIT_STATE (vict, PULSE_VIOLENCE * 2);
		WAIT_STATE (ch, PULSE_VIOLENCE * 3);
	}
}


void
do_mob_backstab (struct char_data *ch, struct char_data *vict)
{

	int hit_roll = 0, to_hit = 0;

	hit_roll = number (1, 100) + GET_STR (ch);
	to_hit = (100 - (int) (100 * GET_LEVEL (ch) / 250));
	if (GET_LEVEL (vict) >= LVL_IMMORT)
		hit_roll = 0;
	if (FIGHTING (ch))
	{
		act ("$N just tried to backstab you during combat!", FALSE, vict, 0,
			 ch, TO_CHAR);
		act ("$e notices you. You cannot backstab in combat!", FALSE, vict, 0,
			 ch, TO_VICT);
		act ("$N attempts to backstab $n during combat!", FALSE, vict, 0, ch,
			 TO_NOTVICT);
	}
	if (MOB_FLAGGED (vict, MOB_AWARE) && AWAKE (vict))
	{
		act ("You notice $N lunging at you!", FALSE, vict, 0, ch, TO_CHAR);
		act ("$e notices you lunging at $m!", FALSE, vict, 0, ch, TO_VICT);
		act ("$n notices $N lunging at $m!", FALSE, vict, 0, ch, TO_NOTVICT);
		hit (vict, ch, TYPE_UNDEFINED);
		return;
	}

	if (hit_roll < to_hit)
	{
		damage (ch, vict, 0, SKILL_BACKSTAB);
	}
	else
	{
		hit (ch, vict, SKILL_BACKSTAB);
		WAIT_STATE (vict, PULSE_VIOLENCE * 2);
		WAIT_STATE (ch, PULSE_VIOLENCE * 3);
	}
}

bool do_mob_fireweapon (struct char_data *ch, struct char_data *vict)
{
    struct obj_data *weapon;
	int num1 = 0;
	int dmg = 0;
	int roll = 0;
	int num_needed = 0;
	char shot[256];
	char argt[255];
	int i = 0;
	int ripple = 0;

	weapon = GET_EQ (ch, WEAR_HOLD);

	/* non ha nessuna arma */
	if (!weapon)
	{
		return (FALSE);
	}

	/* l'oggetto in mano non e' una arma da fuoco */
	if ((GET_OBJ_TYPE (weapon) != ITEM_FIREWEAPON))
	{
		return (FALSE);
	}

	/* se e' scarica  lo ricarico e perdo il round */
	if (!GET_OBJ_VAL (weapon, 3))
	{
		num_needed = GET_OBJ_VAL (weapon, 2) - GET_OBJ_VAL (weapon, 3);
		GET_OBJ_VAL (weapon, 3) += num_needed;
		act ("Carichi $p", FALSE, ch, weapon, 0, TO_CHAR);
		act ("$n smette di sparare e carica $p", FALSE, ch, weapon, 0,
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

	num1 = GET_DEX (ch) - GET_DEX (vict);
	if (num1 > 0)
		num1 = 60;
	if (num1 < 1)
		num1 = 40;


	for (i = 0; i < ripple; i++)
	{
		roll = number (1, 101);
		if (!FIGHTING (ch))
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
			act (shot, FALSE, ch, weapon, vict, TO_CHAR);
			sprintf (shot, "$n ti colpisce con %s sparata da $p!", argt);
			act (shot, FALSE, ch, weapon, vict, TO_VICT);
			sprintf (shot, "$n colpisce $N con %s sparata da $p!", argt);
			act (shot, FALSE, ch, weapon, vict, TO_NOTVICT);
			GET_OBJ_VAL (weapon, 3) -= 1;

			dmg =
				dice (shot_damage[GET_OBJ_VAL (weapon, 0)][0],
					  shot_damage[GET_OBJ_VAL (weapon, 0)][1]) +
				(GET_LEVEL (ch) / 5);

			damage (ch, vict, dmg, TYPE_UNDEFINED);
		}
		else
		{
			sprintf (shot, "Spari %s a $N e manchi!", argt);
			act (shot, FALSE, ch, weapon, vict, TO_CHAR);
			sprintf (shot, "$n ti spara %s e manca!", argt);
			act (shot, FALSE, ch, weapon, vict, TO_VICT);
			sprintf (shot, "$n spara %s a $N e manca!", argt);
			act (shot, FALSE, ch, weapon, vict, TO_NOTVICT);
			GET_OBJ_VAL (weapon, 3) -= 1;
		}

	}
    return (TRUE);
}

void do_mob_springleap(struct char_data *ch, struct char_data *vict)
{
	int hit_roll = 0, to_hit = 0;

    if ( GET_POS(ch) > POS_SITTING || !FIGHTING(ch))
    {
        send_to_char( "Non sei nella giusta posizione per farlo!\n\r", ch );
        return;
    }

	hit_roll = number (1, 100) + GET_DEX (ch)- GET_DEX (vict);
	to_hit = (100 - (int) (100 * GET_LEVEL (ch) / 250));
	
    act ("$n fa un'abile mossa allungando una gamba verso $N.", FALSE,
		 ch, 0, vict, TO_NOTVICT);
	act ("Sali a gamba tesa verso $N.", FALSE, ch, 0, vict, TO_CHAR);
	act ("$n salta a gamba tesa verso te.", FALSE, ch, 0, vict, TO_VICT);

	if (MOB_FLAGGED (vict, MOB_NOBASH))
	{
		damage (ch, vict, 0, SKILL_SPRING_LEAP);
		act
			("$n rimbalza a gamba tesa contro $N e cade rovinosamente a terra.",
			 FALSE, ch, 0, vict, TO_NOTVICT);
		act ("Rimbalzi a gamba tesa contro $N e cadi rovinosamente a terra.",
			 FALSE, ch, 0, vict, TO_VICT);
		act
			("$n rimbalza a gamba tesa contro di te e cade rovinosamente a terra.",
			 FALSE, ch, 0, vict, TO_CHAR);
		GET_POS (ch) = POS_SITTING;
		WAIT_STATE (ch, PULSE_VIOLENCE * 2);
		return;
	}

	
	if (GET_LEVEL (vict) >= LVL_IMMORT)
		hit_roll = 0;

	if (hit_roll < to_hit)
	{
		damage (ch, vict, 0, SKILL_SPRING_LEAP);
		send_to_char ("Rovini a terra rumorosamente.\n\r", ch);
		act ("$n rovina a terra rumorosamente", FALSE, ch, 0, 0, TO_ROOM);
		GET_POS (ch) = POS_SITTING;
	}
	else
	{
		damage (ch, vict, GET_LEVEL (ch) / 2 + 10, SKILL_KICK);
		if (IN_ROOM (ch) == IN_ROOM (vict))
		{
			act ("$n cade a terra con violenza per la forza dell'impeto.",
				 FALSE, vict, 0, 0, TO_ROOM);
			GET_POS (vict) = POS_SITTING;
            GET_POS (ch) = POS_FIGHTING;
			WAIT_STATE (vict, PULSE_VIOLENCE * 1);
		}

	}
	WAIT_STATE (ch, PULSE_VIOLENCE * 2);
}

void do_mob_sommersault(struct char_data *ch, struct char_data *vict)
{
	int hit_roll = 0, to_hit = 0;

	hit_roll = number (1, 100) + GET_DEX (ch)- GET_DEX (vict);
	to_hit = (100 - (int) (100 * GET_LEVEL (ch) / 250));


	act
		("$n fa una rotazione su se stesso allungando una gamba all'indietro verso $N.",
		 FALSE, ch, 0, vict, TO_NOTVICT);
	act ("Ruoti su se stesso allungando una gamba all'indietro verso $N.",
		 FALSE, ch, 0, vict, TO_CHAR);
	act
		("$n fa una rotazione su se stesso allungando una gamba all'indietro verso te.",
		 FALSE, ch, 0, vict, TO_VICT);

	if (hit_roll < to_hit)
	{
		damage (ch, vict, 0, SKILL_SOMMERSAULT);
		send_to_char ("Cadi a terra rumorosamente.\n\r", ch);
		act ("$n cade a terra rumorosamente", FALSE, ch, 0, 0, TO_ROOM);
		GET_POS (ch) = POS_SITTING;
		WAIT_STATE (ch, PULSE_VIOLENCE * 1);
	}
	else
	{
		damage (ch, vict, (GET_LEVEL (ch) / 4) + 10, SKILL_KICK);
        if (IN_ROOM (ch) == IN_ROOM (vict))
		{
			act ("$n rovina a terra.",
				 FALSE, vict, 0, 0, TO_ROOM);
			GET_POS (vict) = POS_SITTING;
            GET_POS (ch) = POS_FIGHTING;
			WAIT_STATE (vict, PULSE_VIOLENCE * 1);
		}
	}
}


void do_mob_quivering_palm(struct char_data *ch, struct char_data *victim)
{
	struct affected_type af;
	int hit_roll = 0, to_hit = 0;

    send_to_char ("Cominci a generare una vibrazione con il palmo della tua "
				  "mano.\n\r", ch);

	if (affected_by_spell (ch, SKILL_QUIVER_PALM))
	{
		send_to_char ("Puoi farlo solo una volta alla settimana.\n\r", ch);
		return;
	}

	hit_roll = number (1, 100) + GET_DEX (ch)- GET_DEX (victim);
	to_hit = (100 - (int) (100 * GET_LEVEL (ch) / 250));


	if (hit_roll < to_hit)
	{
		send_to_char ("La vibrazione si spegne inefficace.\n\r", ch);
		WAIT_STATE (ch, PULSE_VIOLENCE * 2);
	}
	else
	{
		damage (ch, victim, GET_MAX_HIT (victim) * 20, SKILL_QUIVER_PALM);
		WAIT_STATE (ch, PULSE_VIOLENCE);
	}

	af.type = SKILL_QUIVER_PALM;
	af.duration = (24 * 7);
	af.modifier = 0;
	af.location = APPLY_NONE;
	af.bitvector = 0;
	affect_to_char (ch, &af);
}


void do_mob_kamehameha(struct char_data *ch, struct char_data *vict)
{
	int hit_roll = 0, to_hit = 0;

	if (GET_MOVE (ch) < 25)
	{
		send_to_char ("Sei troppo stanco per generare l'onda energetica.\r\n",
					  ch);
		return;
	}

	send_to_char
		("Le tue mani si illuminano e cominci a generare una sfera di forza "
		 "nei palmi.\n\r", ch);
	act
		("Le mani di $n si illuminano e comincia a generare una sfera di forza nei palmi.",
		 FALSE, ch, 0, 0, TO_ROOM);

	hit_roll = number (1, 100) + GET_DEX (ch)- GET_DEX (vict);
	to_hit = (100 - (int) (100 * GET_LEVEL (ch) / 250));

	if (hit_roll < to_hit)
	{
		send_to_char
			("La sfera energetica si dissolve inefficace nelle mani.\n\r",
			 ch);
		act ("La sfera energetica di $n si dissolve inefficace.", FALSE, ch,
			 0, 0, TO_ROOM);
	}
	else
	{
        act ("$n lancia la sfera energetica verso $N.", FALSE, vict, 0, ch,
			 TO_NOTVICT);
		act ("Lanci la sfera energetica verso $N.", FALSE, vict, 0, ch,
			 TO_CHAR);
		act ("$n lancia la sfera energetica verso te.", FALSE, vict, 0, ch,
			 TO_VICT);
		damage (ch, vict, GET_LEVEL (ch), SKILL_KAMEHAMEHA);	// il danno e' proporzionale al livello
	}

	WAIT_STATE (ch, PULSE_VIOLENCE);
	GET_MOVE (ch) -= 25;
}


void do_mob_retreat(struct char_data *ch, struct char_data *vict)
{
    /* ancora da implementare */
    return;
}


void do_mob_ki_shield(struct char_data *ch, struct char_data *vict)
{
    struct affected_type af;
	int hit_roll = 0, to_hit = 0;
	
    if (affected_by_spell (ch, SKILL_KI_SHIELD))
	{
		return;
	}

	hit_roll = number (1, 100) + GET_DEX (ch)- GET_DEX (vict);
	to_hit = (100 - (int) (100 * GET_LEVEL (ch) / 250));

	if (hit_roll > to_hit)
	{
		act("$n fa espandere la sua aura.", TRUE, ch, 0, 0, TO_ROOM);
		send_to_char ("Sei circondato da una aura luminosa.\n\r", ch);
		
        af.type = SKILL_KI_SHIELD;
        af.duration = 4;
	    af.bitvector = AFF_SANCTUARY;
	    af.modifier = 0;
	    af.location = APPLY_NONE;
	}
	else
	{
		
        act("$n fa espandere la sua aura, ma si dissolve subito.",TRUE, ch, 0, 0, TO_ROOM);
		send_to_char ("La tua aura tremola e si dissolve subito.\n\r", ch);
        af.type = SKILL_KI_SHIELD;
        af.duration = 2;
	    af.bitvector = 0;
	    af.modifier = 0;
	    af.location = APPLY_NONE;
	}

	affect_to_char (ch, &af);
	return;
}
