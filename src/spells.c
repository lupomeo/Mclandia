
/* ************************************************************************
*   File: spells.c                                      Part of CircleMUD *
*  Usage: Implementation of "manual spells".  Circle 2.2 spell compat.    *
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
#include "spells.h"
#include "handler.h"
#include "db.h"
#include "constants.h"
#include "interpreter.h"

extern room_rnum r_mortal_start_room;
extern struct room_data *world;
extern struct obj_data *object_list;
extern struct char_data *character_list;
extern struct index_data *obj_index;
extern struct descriptor_data *descriptor_list;
extern struct zone_data *zone_table;

extern int mini_mud;
extern int pk_allowed;

extern char *types_ammo[][2];
extern int shot_damage[][2];

void clearMemory (struct char_data *ch);
void weight_change_object (struct obj_data *obj, int weight);
void add_follower (struct char_data *ch, struct char_data *leader);
int mag_savingthrow (struct char_data *ch, int type, int modifier);
void name_to_drinkcon (struct obj_data *obj, int type);
void name_from_drinkcon (struct obj_data *obj);
int compute_armor_class (struct char_data *ch);

/*
 * Special spells appear below.
 */

ASPELL (spell_create_water)
{
	int water;

	if (ch == NULL || obj == NULL)
		return;
	/* level = MAX(MIN(level, LVL_IMPL), 1);   - not used */

	if (GET_OBJ_TYPE (obj) == ITEM_DRINKCON)
	{
		if ((GET_OBJ_VAL (obj, 2) != LIQ_WATER)
			&& (GET_OBJ_VAL (obj, 1) != 0))
		{
			name_from_drinkcon (obj);
			GET_OBJ_VAL (obj, 2) = LIQ_SLIME;
			name_to_drinkcon (obj, LIQ_SLIME);
		}
		else
		{
			water = MAX (GET_OBJ_VAL (obj, 0) - GET_OBJ_VAL (obj, 1), 0);
			if (water > 0)
			{
				if (GET_OBJ_VAL (obj, 1) >= 0)
					name_from_drinkcon (obj);
				GET_OBJ_VAL (obj, 2) = LIQ_WATER;
				GET_OBJ_VAL (obj, 1) += water;
				name_to_drinkcon (obj, LIQ_WATER);
				weight_change_object (obj, water);
				act ("$p is filled.", FALSE, ch, obj, 0, TO_CHAR);
			}
		}
	}
}


ASPELL (spell_recall)
{
	if (victim == NULL || IS_NPC (victim))
		return;

    /* se sta combattendo fanculo al recaller */
    if (GET_POS (victim) == POS_FIGHTING)
    {
        send_to_char("non puoi recallare durante il combattimento!\r\n",victim);
        return;
    }
    
	act ("$n scompare.", TRUE, victim, 0, 0, TO_ROOM);
	char_from_room (victim);
	char_to_room (victim, r_mortal_start_room);
	act ("$n appare improvvisamente in mezzo alla stanza.", TRUE, victim, 0,
		 0, TO_ROOM);
	look_at_room (victim, 0);
}


ASPELL (spell_teleport)
{
	room_rnum to_room;

	if (victim == NULL || IS_NPC (victim))
	{
		return;
	}

	do
	{
		to_room = number (0, top_of_world);
	}
	while (ROOM_FLAGGED (to_room, ROOM_PRIVATE | ROOM_DEATH | ROOM_GODROOM));

	act ("$n slowly fades out of existence and is gone.",
		 FALSE, victim, 0, 0, TO_ROOM);
	char_from_room (victim);
	char_to_room (victim, to_room);
	act ("$n slowly fades into existence.", FALSE, victim, 0, 0, TO_ROOM);
	look_at_room (victim, 0);
}

#define SUMMON_FAIL "You failed.\r\n"

ASPELL (spell_summon)
{
	if (ch == NULL || victim == NULL)
		return;

	if (GET_LEVEL (victim) > MIN (LVL_IMMORT - 1, level + 3))
	{
		send_to_char (SUMMON_FAIL, ch);
		return;
	}

	if (MOB_FLAGGED (victim, MOB_NOSUMMON) ||
		(IS_NPC (victim) && mag_savingthrow (victim, SAVING_SPELL, 0)))
	{
		send_to_char (SUMMON_FAIL, ch);
		return;
	}

	act ("$n disappears suddenly.", TRUE, victim, 0, 0, TO_ROOM);

	char_from_room (victim);
	char_to_room (victim, ch->in_room);

	act ("$n arrives suddenly.", TRUE, victim, 0, 0, TO_ROOM);
	act ("$n has summoned you!", FALSE, ch, 0, victim, TO_VICT);
	look_at_room (victim, 0);
}



ASPELL (spell_locate_object)
{
	struct obj_data *i;
	char name[MAX_INPUT_LENGTH];
	int j;

	/*
	 * FIXME: This is broken.  The spell parser routines took the argument
	 * the player gave to the spell and located an object with that keyword.
	 * Since we're passed the object and not the keyword we can only guess
	 * at what the player originally meant to search for. -gg
	 */
	strcpy (name, fname (obj->name));
	j = level / 2;

	for (i = object_list; i && (j > 0); i = i->next)
	{
		if (!isname (name, i->name))
			continue;

		if (i->carried_by)
			sprintf (buf, "%s is being carried by %s.\r\n",
					 i->short_description, PERS (i->carried_by, ch));
		else if (i->in_room != NOWHERE)
			sprintf (buf, "%s is in %s.\r\n", i->short_description,
					 world[i->in_room].name);
		else if (i->in_obj)
			sprintf (buf, "%s is in %s.\r\n", i->short_description,
					 i->in_obj->short_description);
		else if (i->worn_by)
			sprintf (buf, "%s is being worn by %s.\r\n",
					 i->short_description, PERS (i->worn_by, ch));
		else
			sprintf (buf, "%s's location is uncertain.\r\n",
					 i->short_description);

		CAP (buf);
		send_to_char (buf, ch);
		j--;
	}

	if (j == level / 2)
		send_to_char ("You sense nothing.\r\n", ch);
}



ASPELL (spell_charm)
{
	struct affected_type af;

	if (victim == NULL || ch == NULL)
		return;

	if (victim == ch)
		send_to_char ("You like yourself even better!\r\n", ch);
	else if (!IS_NPC (victim) && !PRF_FLAGGED (victim, PRF_SUMMONABLE))
		send_to_char ("You fail because SUMMON protection is on!\r\n", ch);
	else if (AFF_FLAGGED (victim, AFF_SANCTUARY))
		send_to_char ("Your victim is protected by sanctuary!\r\n", ch);
	else if (MOB_FLAGGED (victim, MOB_NOCHARM))
		send_to_char ("Your victim resists!\r\n", ch);
	else if (AFF_FLAGGED (ch, AFF_CHARM))
		send_to_char ("You can't have any followers of your own!\r\n", ch);
	else if (AFF_FLAGGED (victim, AFF_CHARM) || level < GET_LEVEL (victim))
		send_to_char ("You fail.\r\n", ch);
	/* player charming another player - no legal reason for this */
	else if (!pk_allowed && !IS_NPC (victim))
		send_to_char ("You fail - shouldn't be doing it anyway.\r\n", ch);
	else if (circle_follow (victim, ch))
		send_to_char ("Sorry, following in circles can not be allowed.\r\n",
					  ch);
	else if (mag_savingthrow (victim, SAVING_PARA, 0))
		send_to_char ("Your victim resists!\r\n", ch);
	else
	{
		if (victim->master)
			stop_follower (victim);

		add_follower (victim, ch);

		af.type = SPELL_CHARM;

		if (GET_INT (victim))
			af.duration = 24 * 18 / GET_INT (victim);
		else
			af.duration = 24 * 18;

		af.modifier = 0;
		af.location = 0;
		af.bitvector = AFF_CHARM;
		affect_to_char (victim, &af);

		act ("Isn't $n just such a nice fellow?", FALSE, ch, 0, victim,
			 TO_VICT);
		if (IS_NPC (victim))
		{
			REMOVE_BIT (MOB_FLAGS (victim), MOB_AGGRESSIVE);
			REMOVE_BIT (MOB_FLAGS (victim), MOB_SPEC);
		}
	}
}



ASPELL (spell_identify)
{
	int i;
	int found;

	if (obj)
	{
		send_to_char ("You feel informed:\r\n", ch);
		sprintf (buf, "Object '%s', Item type: ", obj->short_description);
		sprinttype (GET_OBJ_TYPE (obj), item_types, buf2);
		strcat (buf, buf2);
		strcat (buf, "\r\n");
		send_to_char (buf, ch);

		if (obj->obj_flags.bitvector)
		{
			send_to_char ("Item will give you following abilities:  ", ch);
			sprintbit (obj->obj_flags.bitvector, affected_bits, buf);
			strcat (buf, "\r\n");
			send_to_char (buf, ch);
		}
		send_to_char ("Item is: ", ch);
		sprintbit (GET_OBJ_EXTRA (obj), extra_bits, buf);
		strcat (buf, "\r\n");
		send_to_char (buf, ch);

		sprintf (buf, "Weight: %d, Value: %d, Rent: %d\r\n",
				 GET_OBJ_WEIGHT (obj), GET_OBJ_COST (obj),
				 GET_OBJ_RENT (obj));
		send_to_char (buf, ch);

		switch (GET_OBJ_TYPE (obj))
		{
		case ITEM_SCROLL:
		case ITEM_POTION:
			sprintf (buf, "This %s casts: ",
					 item_types[(int) GET_OBJ_TYPE (obj)]);

			if (GET_OBJ_VAL (obj, 1) >= 1)
				sprintf (buf + strlen (buf), " %s",
						 skill_name (GET_OBJ_VAL (obj, 1)));
			if (GET_OBJ_VAL (obj, 2) >= 1)
				sprintf (buf + strlen (buf), " %s",
						 skill_name (GET_OBJ_VAL (obj, 2)));
			if (GET_OBJ_VAL (obj, 3) >= 1)
				sprintf (buf + strlen (buf), " %s",
						 skill_name (GET_OBJ_VAL (obj, 3)));
			strcat (buf, "\r\n");
			send_to_char (buf, ch);
			break;
		case ITEM_WAND:
		case ITEM_STAFF:
			sprintf (buf, "This %s casts: ",
					 item_types[(int) GET_OBJ_TYPE (obj)]);
			sprintf (buf + strlen (buf), " %s\r\n",
					 skill_name (GET_OBJ_VAL (obj, 3)));
			sprintf (buf + strlen (buf),
					 "It has %d maximum charge%s and %d remaining.\r\n",
					 GET_OBJ_VAL (obj, 1), GET_OBJ_VAL (obj,
														1) == 1 ? "" : "s",
					 GET_OBJ_VAL (obj, 2));
			send_to_char (buf, ch);
			break;
		case ITEM_WEAPON:
			sprintf (buf, "Damage Dice is '%dD%d'", GET_OBJ_VAL (obj, 1),
					 GET_OBJ_VAL (obj, 2));
			sprintf (buf + strlen (buf),
					 " for an average per-round damage of %.1f.\r\n",
					 (((GET_OBJ_VAL (obj, 2) + 1) / 2.0) *
					  GET_OBJ_VAL (obj, 1)));
			send_to_char (buf, ch);
			break;
        case ITEM_MISSILE:
		    sprintf (buf, "Ammo type:%s Num. Rounds:%d\r\nBase Damage per Round:%dD%d\r\n",
				 types_ammo[GET_OBJ_VAL (obj, 0)][0], 
                 GET_OBJ_VAL (obj, 3),
                 shot_damage[GET_OBJ_VAL (obj, 0)][0],
                 shot_damage[GET_OBJ_VAL (obj, 0)][1]);
            send_to_char(buf,ch);
		    break;
	    case ITEM_FIREWEAPON:
		    sprintf (buf, "Ammo type:%s Max Rounds:%d Rounds:%d Autofire:%s\r\n",
				 types_ammo[GET_OBJ_VAL (obj, 0)][0],
				 GET_OBJ_VAL (obj, 2),
				 GET_OBJ_VAL (obj, 3),
				 ((GET_OBJ_VAL (obj, 1) == 0) ? "No" : "Yes"));
            send_to_char(buf,ch);
		    break;
	    case ITEM_THROW:
		    sprintf (buf, "Damage: %dD%d",
				 GET_OBJ_VAL (obj, 2), GET_OBJ_VAL (obj, 3));
		    break;
	    case ITEM_GRENADE:
		    sprintf (buf, "Timer: %d Damage: %dD%d",
				 GET_OBJ_VAL (obj, 0), GET_OBJ_VAL (obj, 1), GET_OBJ_VAL (obj, 2));
		    break;
		case ITEM_ARMOR:
			sprintf (buf, "AC-apply is %d\r\n", GET_OBJ_VAL (obj, 0));
			send_to_char (buf, ch);
			break;
		}
		found = FALSE;
		for (i = 0; i < MAX_OBJ_AFFECT; i++)
		{
			if ((obj->affected[i].location != APPLY_NONE) &&
				(obj->affected[i].modifier != 0))
			{
				if (!found)
				{
					send_to_char ("Can affect you as :\r\n", ch);
					found = TRUE;
				}
				sprinttype (obj->affected[i].location, apply_types, buf2);
				sprintf (buf, "   Affects: %s By %d\r\n", buf2,
						 obj->affected[i].modifier);
				send_to_char (buf, ch);
			}
		}
	}
	else if (victim)
	{							/* victim */
		sprintf (buf, "Name: %s\r\n", GET_NAME (victim));
		send_to_char (buf, ch);
		if (!IS_NPC (victim))
		{
			sprintf (buf,
					 "%s is %d years, %d months, %d days and %d hours old.\r\n",
					 GET_NAME (victim), age (victim)->year,
					 age (victim)->month, age (victim)->day,
					 age (victim)->hours);
			send_to_char (buf, ch);
		}
		sprintf (buf, "Height %d cm, Weight %d pounds\r\n",
				 GET_HEIGHT (victim), GET_WEIGHT (victim));
		sprintf (buf + strlen (buf), "Level: %d, Hits: %d, Ram: %d\r\n",
				 GET_LEVEL (victim), GET_HIT (victim), GET_MANA (victim));
		sprintf (buf + strlen (buf), "AC: %d, Hitroll: %d, Damroll: %d\r\n",
				 compute_armor_class (victim), GET_HITROLL (victim),
				 GET_DAMROLL (victim));
		sprintf (buf + strlen (buf),
				 "Str: %d/%d, Int: %d, Wis: %d, Dex: %d, Con: %d, Cha: %d\r\n",
				 GET_STR (victim), GET_ADD (victim), GET_INT (victim),
				 GET_WIS (victim), GET_DEX (victim), GET_CON (victim),
				 GET_CHA (victim));
		send_to_char (buf, ch);

	}
}



/*
 * Cannot use this spell on an equipped object or it will mess up the
 * wielding character's hit/dam totals.
 */
ASPELL (spell_enchant_weapon)
{
	int i;

	if (ch == NULL || obj == NULL)
		return;

	/* Either already enchanted or not a weapon. */
	if (GET_OBJ_TYPE (obj) != ITEM_WEAPON || OBJ_FLAGGED (obj, ITEM_MAGIC))
		return;

	/* Make sure no other affections. */
	for (i = 0; i < MAX_OBJ_AFFECT; i++)
		if (obj->affected[i].location != APPLY_NONE)
			return;

	SET_BIT (GET_OBJ_EXTRA (obj), ITEM_MAGIC);

	obj->affected[0].location = APPLY_HITROLL;
	obj->affected[0].modifier = 1 + (level >= 18);

	obj->affected[1].location = APPLY_DAMROLL;
	obj->affected[1].modifier = 1 + (level >= 20);

	if (IS_GOOD (ch))
	{
		SET_BIT (GET_OBJ_EXTRA (obj), ITEM_ANTI_EVIL);
		act ("$p glows blue.", FALSE, ch, obj, 0, TO_CHAR);
	}
	else if (IS_EVIL (ch))
	{
		SET_BIT (GET_OBJ_EXTRA (obj), ITEM_ANTI_GOOD);
		act ("$p glows red.", FALSE, ch, obj, 0, TO_CHAR);
	}
	else
		act ("$p glows yellow.", FALSE, ch, obj, 0, TO_CHAR);
}


ASPELL (spell_detect_poison)
{
	if (victim)
	{
		if (victim == ch)
		{
			if (AFF_FLAGGED (victim, AFF_POISON))
				send_to_char ("You can sense poison in your blood.\r\n", ch);
			else
				send_to_char ("You feel healthy.\r\n", ch);
		}
		else
		{
			if (AFF_FLAGGED (victim, AFF_POISON))
				act ("You sense that $E is poisoned.", FALSE, ch, 0, victim,
					 TO_CHAR);
			else
				act ("You sense that $E is healthy.", FALSE, ch, 0, victim,
					 TO_CHAR);
		}
	}

	if (obj)
	{
		switch (GET_OBJ_TYPE (obj))
		{
		case ITEM_DRINKCON:
		case ITEM_FOUNTAIN:
		case ITEM_FOOD:
			if (GET_OBJ_VAL (obj, 3))
				act ("You sense that $p has been contaminated.", FALSE, ch,
					 obj, 0, TO_CHAR);
			else
				act ("You sense that $p is safe for consumption.", FALSE, ch,
					 obj, 0, TO_CHAR);
			break;
		default:
			send_to_char ("You sense that it should not be consumed.\r\n",
						  ch);
		}
	}
}

#define PORTAL_OBJ  20			/* the vnum of the portal object */

ASPELL (spell_gate)
{
	/* create a magic portal */
	struct obj_data *tmp_obj, *tmp_obj2;
	struct extra_descr_data *ed;
	struct room_data *rp, *nrp;
	struct char_data *tmp_ch = (struct char_data *) victim;
	char buf[512];

	assert (ch);
	assert ((level >= 0) && (level <= LVL_IMPL));


	/*
	   check target room for legality.
	 */
	rp = &world[ch->in_room];
	tmp_obj = read_object (PORTAL_OBJ, VIRTUAL);
	if (!rp || !tmp_obj)
	{
		send_to_char ("Il programma crasha con una exception fallita\n\r",
					  ch);
		extract_obj (tmp_obj);
		return;
	}
	if (IS_SET (rp->room_flags, ROOM_TUNNEL))
	{
		send_to_char ("Non c'e nessuno in quella stanza!\n\r", ch);
		extract_obj (tmp_obj);
		return;
	}

	if (!(nrp = &world[tmp_ch->in_room]))
	{
		char str[180];
		sprintf (str, "%s not in any room", GET_NAME (tmp_ch));
		log (str);
		send_to_char
			("Il programma non riesce a localizzare la destinazione: no route to host\r\n",
			 ch);
		extract_obj (tmp_obj);
		return;
	}

	if (ROOM_FLAGGED (tmp_ch->in_room, ROOM_NOMAGIC))
	{
		send_to_char ("Il tuo bersaglio ha le porte telnet chiuse.\n\r", ch);
		extract_obj (tmp_obj);
		return;
	}

	sprintf (buf, "Attraverso il telnet gateway, intravedi %s", nrp->name);

	CREATE (ed, struct extra_descr_data, 1);
	ed->next = tmp_obj->ex_description;
	tmp_obj->ex_description = ed;
	CREATE (ed->keyword, char, strlen (tmp_obj->name) + 1);
	strcpy (ed->keyword, tmp_obj->name);
	ed->description = str_dup (buf);

	tmp_obj->obj_flags.value[0] = tmp_ch->in_room;
	GET_OBJ_TIMER (tmp_obj) = 2;
	//GET_OBJ_TYPE(tmp_obj2) == ITEM_PORTAL; // chevvordi questo == ???
	GET_OBJ_TYPE (tmp_obj) = ITEM_PORTAL;	// dovrebbe essere corretto
	obj_to_room (tmp_obj, ch->in_room);

	act ("$p appare improvvisamente.", TRUE, ch, tmp_obj, 0, TO_ROOM);
	act ("$p appare improvvisamente.", TRUE, ch, tmp_obj, 0, TO_CHAR);

/* Portal at other side */
	rp = &world[ch->in_room];
	tmp_obj2 = read_object (PORTAL_OBJ, VIRTUAL);
	if (!rp || !tmp_obj2)
	{
		send_to_char ("Il programma fallisce\n\r", ch);
		extract_obj (tmp_obj2);
		return;
	}
	sprintf (buf, "Attraverso il telnet gateway, intravedi %s", rp->name);

	CREATE (ed, struct extra_descr_data, 1);
	ed->next = tmp_obj2->ex_description;
	tmp_obj2->ex_description = ed;
	CREATE (ed->keyword, char, strlen (tmp_obj2->name) + 1);
	strcpy (ed->keyword, tmp_obj2->name);
	ed->description = str_dup (buf);
	tmp_obj2->obj_flags.value[0] = ch->in_room;
	GET_OBJ_TIMER (tmp_obj2) = 2;
	//GET_OBJ_TYPE(tmp_obj2) == ITEM_PORTAL; // anche questo ?????
	GET_OBJ_TYPE (tmp_obj2) = ITEM_PORTAL;
	obj_to_room (tmp_obj2, tmp_ch->in_room);
	act ("$p appare improvvisamente.", TRUE, tmp_ch, tmp_obj2, 0, TO_ROOM);
	act ("$p appare improvvisamente.", TRUE, tmp_ch, tmp_obj2, 0, TO_CHAR);
}

ASPELL (spell_mind_over_body)
{
	if (victim == NULL || IS_NPC (victim))
		return;

	if (GET_LEVEL (victim) < LVL_GOD)
	{
		GET_COND (victim, THIRST) = (char) 24;
		GET_COND (victim, FULL) = (char) 24;
	}
	act ("$n sembra non avere piu' ne' fame e ne' sete", TRUE, victim, 0,
		 0, TO_ROOM);
	send_to_char ("Non sei piu' affamato ne' assetato.\r\n", victim);

}
ASPELL (spell_canibalize)
{
	if (victim == NULL || IS_NPC (victim))
		return;

	if (GET_HIT (victim) < 100)
	{
		send_to_char ("Non hai abbastanza Hit Point.\r\n", victim);
		return;
	}

	if (GET_LEVEL (victim) < LVL_GOD)
	{
		GET_HIT (victim) -= 100;
	}

	GET_MANA (victim) += 100;
	act ("$n si procura danni e ferite nello sforzo di recuperare energia.",
		 TRUE, victim, 0, 0, TO_ROOM);
	send_to_char ("Ti senti meno sano ma con piu' energia.\r\n", victim);

}
