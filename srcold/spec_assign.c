
/* ************************************************************************
*   File: spec_assign.c                                 Part of CircleMUD *
*  Usage: Functions to assign function pointers to objs/mobs/rooms        *
*                                                                         *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  Copyright (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */

#include "conf.h"
#include "sysdep.h"

#include "structs.h"
#include "db.h"
#include "interpreter.h"
#include "utils.h"

extern int dts_are_dumps;
extern int mini_mud;
extern struct room_data *world;
extern struct index_data *mob_index;
extern struct index_data *obj_index;

SPECIAL (dump);
SPECIAL (pet_shops);
SPECIAL (postmaster);
SPECIAL (cityguard);
SPECIAL (receptionist);
SPECIAL (cryogenicist);
SPECIAL (guild_guard);
SPECIAL (guild);
SPECIAL (master);
SPECIAL (puff);
SPECIAL (vigile);
SPECIAL (lvl_aggro);
SPECIAL (fido);
SPECIAL (janitor);
SPECIAL (mayor);
SPECIAL (snake);
SPECIAL (thief);
SPECIAL (magic_user);
SPECIAL (bank);
SPECIAL (gen_board);
SPECIAL (massage);
SPECIAL (mob_massage);
SPECIAL (massage_1);
SPECIAL (mob_massage_1);
SPECIAL (fire_weapons);
SPECIAL (guard_tower);
SPECIAL (tac_machine);
SPECIAL (newbie_guide);

void assign_kings_castle (void);

SPECIAL (warrior_class);
SPECIAL (thief_class);
SPECIAL (martial_artist);

/* local functions */
void assign_mobiles (void);
void assign_objects (void);
void assign_rooms (void);
void ASSIGNROOM (room_vnum room, SPECIAL (fname));
void ASSIGNMOB (mob_vnum mob, SPECIAL (fname));
void ASSIGNOBJ (obj_vnum obj, SPECIAL (fname));

/* functions to perform assignments */

void
ASSIGNMOB (mob_vnum mob, SPECIAL (fname))
{
	mob_rnum rnum;

	if ((rnum = real_mobile (mob)) >= 0)
		mob_index[rnum].func = fname;
	else if (!mini_mud)
		log ("SYSERR: Attempt to assign spec to non-existant mob #%d", mob);
}

void
ASSIGNOBJ (obj_vnum obj, SPECIAL (fname))
{
	obj_rnum rnum;

	if ((rnum = real_object (obj)) >= 0)
		obj_index[rnum].func = fname;
	else if (!mini_mud)
		log ("SYSERR: Attempt to assign spec to non-existant obj #%d", obj);
}

void
ASSIGNROOM (room_vnum room, SPECIAL (fname))
{
	room_rnum rnum;

	if ((rnum = real_room (room)) >= 0)
		world[rnum].func = fname;
	else if (!mini_mud)
		log ("SYSERR: Attempt to assign spec to non-existant room #%d", room);
}


/* ********************************************************************
*  Assignments                                                        *
******************************************************************** */

/* assign special procedures to mobiles */
void
assign_mobiles (void)
{
	assign_kings_castle ();

	ASSIGNMOB (1, puff);

	// Mob Area terroristi
	ASSIGNMOB (20000, warrior_class);	// terrorista con pistola
	ASSIGNMOB (20001, warrior_class);	// terrorista con shotgun
	ASSIGNMOB (20002, warrior_class);
	ASSIGNMOB (20100, warrior_class);	// terrorista con pistola
	ASSIGNMOB (20101, warrior_class);	// terrorista con shotgun
	ASSIGNMOB (20102, warrior_class);
	ASSIGNMOB (20200, warrior_class);	// terrorista con pistola
	ASSIGNMOB (20201, warrior_class);	// terrorista con shotgun
	ASSIGNMOB (20202, warrior_class);
	ASSIGNMOB (20300, warrior_class);	// terrorista con pistola
	ASSIGNMOB (20301, warrior_class);	// terrorista con shotgun
	ASSIGNMOB (20302, warrior_class);
	ASSIGNMOB (20400, warrior_class);	// terrorista con pistola
	ASSIGNMOB (20401, warrior_class);	// terrorista con shotgun
	ASSIGNMOB (20402, warrior_class);
	ASSIGNMOB (20500, warrior_class);	// terrorista con pistola
	ASSIGNMOB (20501, warrior_class);	// terrorista con shotgun
	ASSIGNMOB (20502, warrior_class);
	ASSIGNMOB (20600, warrior_class);	// terrorista con pistola
	ASSIGNMOB (20601, warrior_class);	// terrorista con shotgun
	ASSIGNMOB (20602, warrior_class);
	ASSIGNMOB (20700, warrior_class);	// terrorista con pistola
	ASSIGNMOB (20701, warrior_class);	// terrorista con shotgun
	ASSIGNMOB (20702, warrior_class);
	ASSIGNMOB (20800, warrior_class);	// terrorista con pistola
	ASSIGNMOB (20801, warrior_class);	// terrorista con shotgun
	ASSIGNMOB (20802, warrior_class);
	ASSIGNMOB (20900, warrior_class);	// terrorista con pistola
	ASSIGNMOB (20901, warrior_class);	// terrorista con shotgun
	ASSIGNMOB (20902, warrior_class);

	ASSIGNMOB (20003, warrior_class);
	ASSIGNMOB (20004, warrior_class);
	ASSIGNMOB (20005, warrior_class); /* guard tower */
	ASSIGNMOB (20006, warrior_class);
	ASSIGNMOB (20007, warrior_class);
	ASSIGNMOB (20008, warrior_class);
	ASSIGNMOB (20012, warrior_class);
	ASSIGNMOB (20013, warrior_class);
	ASSIGNMOB (20014, magic_user);

	// Mob Area Harlem
	ASSIGNMOB (21001, warrior_class);	// moses con pistola
	ASSIGNMOB (21003, warrior_class);	// mamita con pistola
	ASSIGNMOB (21009, warrior_class);	// duke con pistola
	ASSIGNMOB (21025, magic_user);	    // dama strega
	ASSIGNMOB (21027, magic_user);  	// father divine

	// Mob Area Costruttori Mud
	ASSIGNMOB (30001, warrior_class);	// jerkiens con pistola
	ASSIGNMOB (30002, magic_user);	    // jerkiel
	ASSIGNMOB (30004, warrior_class);	// guardia armata
	ASSIGNMOB (30005, warrior_class);	// capo guardie
	ASSIGNMOB (30006, magic_user);	    // implementor
	ASSIGNMOB (30007, magic_user);	    // programmatore
	ASSIGNMOB (30008, magic_user);	    // builder

	// Mob Area Palazzo Replicanti
	ASSIGNMOB (16002, magic_user);	    // ingegnere
	ASSIGNMOB (16004, magic_user);	    // robot assemblatore
	ASSIGNMOB (16005, warrior_class);	// guardia
	ASSIGNMOB (16008, magic_user);	    // mostro replicante
	ASSIGNMOB (16009, magic_user);	    // Mayanna
	ASSIGNMOB (16010, magic_user);	    // Creatore

	// Mob Area montagne / castello chat
	ASSIGNMOB (18600, lvl_aggro);
	ASSIGNMOB (18601, lvl_aggro);
	ASSIGNMOB (18602, lvl_aggro);
	ASSIGNMOB (18603, lvl_aggro);
	ASSIGNMOB (18604, lvl_aggro);
	ASSIGNMOB (18681, lvl_aggro);
	ASSIGNMOB (18680, magic_user);	//mamma telecom
	ASSIGNMOB (18681, lvl_aggro);	//Bill gates
	ASSIGNMOB (18682, magic_user);	//alan cox
	ASSIGNMOB (18699, lvl_aggro);	//Backoffice        

	// Immortal Zone 
	ASSIGNMOB (1200, receptionist);
	ASSIGNMOB (1201, postmaster);
	ASSIGNMOB (1202, janitor);
/*
    ASSIGNMOB (1299, martial_artist);
*/

	// Mclandia
	ASSIGNMOB (3005, receptionist);
	ASSIGNMOB (3010, postmaster);
	ASSIGNMOB (3020, master);
	ASSIGNMOB (3021, master);
	ASSIGNMOB (3022, master);
	ASSIGNMOB (3023, master);
	ASSIGNMOB (3024, guild_guard);
	ASSIGNMOB (3025, guild_guard);
	ASSIGNMOB (3026, guild_guard);
	ASSIGNMOB (3027, guild_guard);
	ASSIGNMOB (3028, master);
	ASSIGNMOB (3029, guild_guard);
	ASSIGNMOB (3030, master);
	ASSIGNMOB (3031, guild_guard);
	ASSIGNMOB (3032, master);
	ASSIGNMOB (3033, guild_guard);
	ASSIGNMOB (3059, cityguard);
	ASSIGNMOB (3060, cityguard);
	ASSIGNMOB (3061, janitor);
	ASSIGNMOB (3062, fido);
	ASSIGNMOB (3066, fido);
	ASSIGNMOB (3067, cityguard);
	ASSIGNMOB (3068, janitor);
	ASSIGNMOB (3095, cryogenicist);
	ASSIGNMOB (3105, mayor);
	ASSIGNMOB (3108, vigile);
	ASSIGNMOB (3103, cityguard);
	ASSIGNMOB (3097, newbie_guide);
	ASSIGNMOB (3098, martial_artist);

	// MORIA 

	ASSIGNMOB (4000, snake);
	ASSIGNMOB (4001, snake);
	ASSIGNMOB (4053, snake);
	ASSIGNMOB (4100, magic_user);
	ASSIGNMOB (4102, snake);
	ASSIGNMOB (4103, thief_class);

	// Redferne's 

	ASSIGNMOB (7900, cityguard);


	// Pyramid

	ASSIGNMOB (5300, snake);
	ASSIGNMOB (5301, snake);
	ASSIGNMOB (5304, thief_class);
	ASSIGNMOB (5305, thief_class);
	ASSIGNMOB (5309, magic_user);	// should breath fire 
	ASSIGNMOB (5311, magic_user);
	ASSIGNMOB (5313, magic_user);	// should be a cleric 
	ASSIGNMOB (5314, magic_user);	// should be a cleric 
	ASSIGNMOB (5315, magic_user);	// should be a cleric 
	ASSIGNMOB (5316, magic_user);	// should be a cleric 
	ASSIGNMOB (5317, magic_user);


	// High Tower Of Sorcery 

	ASSIGNMOB (2501, magic_user);	// should likely be cleric 
	ASSIGNMOB (2504, magic_user);
	ASSIGNMOB (2507, magic_user);
	ASSIGNMOB (2508, magic_user);
	ASSIGNMOB (2510, magic_user);
	ASSIGNMOB (2511, thief_class);
	ASSIGNMOB (2514, magic_user);
	ASSIGNMOB (2515, magic_user);
	ASSIGNMOB (2516, magic_user);
	ASSIGNMOB (2517, magic_user);
	ASSIGNMOB (2518, magic_user);
	ASSIGNMOB (2520, magic_user);
	ASSIGNMOB (2521, magic_user);
	ASSIGNMOB (2522, magic_user);
	ASSIGNMOB (2523, magic_user);
	ASSIGNMOB (2524, magic_user);
	ASSIGNMOB (2525, magic_user);
	ASSIGNMOB (2526, magic_user);
	ASSIGNMOB (2527, magic_user);
	ASSIGNMOB (2528, magic_user);
	ASSIGNMOB (2529, magic_user);
	ASSIGNMOB (2530, magic_user);
	ASSIGNMOB (2531, magic_user);
	ASSIGNMOB (2532, magic_user);
	ASSIGNMOB (2533, magic_user);
	ASSIGNMOB (2534, magic_user);
	ASSIGNMOB (2536, magic_user);
	ASSIGNMOB (2537, magic_user);
	ASSIGNMOB (2538, magic_user);
	ASSIGNMOB (2540, magic_user);
	ASSIGNMOB (2541, magic_user);
	ASSIGNMOB (2548, magic_user);
	ASSIGNMOB (2549, magic_user);
	ASSIGNMOB (2552, magic_user);
	ASSIGNMOB (2553, magic_user);
	ASSIGNMOB (2554, magic_user);
	ASSIGNMOB (2556, magic_user);
	ASSIGNMOB (2557, magic_user);
	ASSIGNMOB (2559, magic_user);
	ASSIGNMOB (2560, magic_user);
	ASSIGNMOB (2562, magic_user);
	ASSIGNMOB (2564, magic_user);


	// SEWERS 

	ASSIGNMOB (7006, snake);
	ASSIGNMOB (7009, magic_user);
	ASSIGNMOB (7200, magic_user);
	ASSIGNMOB (7201, magic_user);
	ASSIGNMOB (7202, magic_user);


	// FOREST 

	ASSIGNMOB (6112, magic_user);
	ASSIGNMOB (6113, snake);
	ASSIGNMOB (6114, magic_user);
	ASSIGNMOB (6115, magic_user);
	ASSIGNMOB (6116, magic_user);	// should be a cleric 
	ASSIGNMOB (6117, magic_user);


	// ARACHNOS 

	ASSIGNMOB (6302, magic_user);
	ASSIGNMOB (6309, magic_user);
	ASSIGNMOB (6312, magic_user);
	ASSIGNMOB (6314, magic_user);
	ASSIGNMOB (6315, magic_user);


	// Desert 

	ASSIGNMOB (5004, magic_user);
	ASSIGNMOB (5005, guild_guard);	// brass dragon 
	ASSIGNMOB (5010, magic_user);
	ASSIGNMOB (5014, magic_user);


	// Drow City 

	ASSIGNMOB (5103, magic_user);
	ASSIGNMOB (5104, magic_user);
	ASSIGNMOB (5107, magic_user);
	ASSIGNMOB (5108, magic_user);


	// Old Thalos 

	ASSIGNMOB (5200, magic_user);
	ASSIGNMOB (5201, magic_user);
	ASSIGNMOB (5209, magic_user);


	// New Thalos 
// 5481 - Cleric (or Mage... but he IS a high priest... *shrug*) 

	ASSIGNMOB (5404, receptionist);
	ASSIGNMOB (5421, magic_user);
	ASSIGNMOB (5422, magic_user);
	ASSIGNMOB (5423, magic_user);
	ASSIGNMOB (5424, magic_user);
	ASSIGNMOB (5425, magic_user);
	ASSIGNMOB (5426, magic_user);
	ASSIGNMOB (5427, magic_user);
	ASSIGNMOB (5428, magic_user);
	ASSIGNMOB (5434, cityguard);
	ASSIGNMOB (5440, magic_user);
	ASSIGNMOB (5455, magic_user);
	ASSIGNMOB (5461, cityguard);
	ASSIGNMOB (5462, cityguard);
	ASSIGNMOB (5463, cityguard);
	ASSIGNMOB (5482, cityguard);
    ASSIGNMOB (5464, thief_class);


/*
5400 - Guildmaster (Mage)
5401 - Guildmaster (Cleric)
5402 - Guildmaster (Warrior)
5403 - Guildmaster (Thief)
5456 - Guildguard (Mage)
5457 - Guildguard (Cleric)
5458 - Guildguard (Warrior)
5459 - Guildguard (Thief)
*/

	// ROME 

	ASSIGNMOB (12009, magic_user);
	ASSIGNMOB (12018, cityguard);
	ASSIGNMOB (12020, magic_user);
	ASSIGNMOB (12021, cityguard);
	ASSIGNMOB (12025, magic_user);
	ASSIGNMOB (12030, magic_user);
	ASSIGNMOB (12031, magic_user);
	ASSIGNMOB (12032, magic_user);


// King Welmar's Castle (not covered in castle.c) 

	ASSIGNMOB (15015, thief_class);	// Ergan... have a better idea? 
	ASSIGNMOB (15032, magic_user);	// Pit Fiend, have something better?  Use it 

	// DWARVEN KINGDOM 

	ASSIGNMOB (6500, cityguard);
	ASSIGNMOB (6502, magic_user);
	ASSIGNMOB (6509, magic_user);
	ASSIGNMOB (6516, magic_user);

	// MEO HOME

	ASSIGNMOB (1300, mob_massage_1);
	ASSIGNMOB (1301, mob_massage);

	// ASUKAGA

	ASSIGNMOB (6518, magic_user);
	ASSIGNMOB (6504, martial_artist);
}



/* assign special procedures to objects */
void
assign_objects (void)
{
	ASSIGNOBJ (3096, gen_board);	/* social board */
	ASSIGNOBJ (3097, gen_board);	/* freeze board */
	ASSIGNOBJ (3098, gen_board);	/* immortal board */
	ASSIGNOBJ (3099, gen_board);	/* mortal board */

	ASSIGNOBJ (3034, bank);		/* atm */
	ASSIGNOBJ (3036, bank);		/* cashcard */
	ASSIGNOBJ (1331, massage);	/* massaggio casa meo */
	ASSIGNOBJ (1332, massage_1);	/* massaggio casa meo */
	ASSIGNOBJ (3139, tac_machine);	/* macchina tac */
}



/* assign special procedures to rooms */
void
assign_rooms (void)
{
	room_rnum i;

	ASSIGNROOM (3030, dump);
	ASSIGNROOM (3031, pet_shops);

	if (dts_are_dumps)
		for (i = 0; i <= top_of_world; i++)
			if (ROOM_FLAGGED (i, ROOM_DEATH))
				world[i].func = dump;
}
