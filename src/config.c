
/* ************************************************************************
*   File: config.c                                      Part of CircleMUD *
*  Usage: Configuration of various aspects of CircleMUD operation         *
*                                                                         *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  Copyright (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */

#define __CONFIG_C__

#include "conf.h"
#include "sysdep.h"

#include "structs.h"
#include "interpreter.h"		/* alias_data definition for structs.h */

#define TRUE	1
#define YES	1
#define FALSE	0
#define NO	0

/*
 * Below are several constants which you can change to alter certain aspects
 * of the way CircleMUD acts.  Since this is a .c file, all you have to do
 * to change one of the constants (assuming you keep your object files around)
 * is change the constant in this file and type 'make'.  Make will recompile
 * this file and relink; you don't have to wait for the whole thing to
 * recompile as you do if you change a header file.
 *
 * I realize that it would be slightly more efficient to have lots of
 * #defines strewn about, so that, for example, the autowiz code isn't
 * compiled at all if you don't want to use autowiz.  However, the actual
 * code for the various options is quite small, as is the computational time
 * in checking the option you've selected at run-time, so I've decided the
 * convenience of having all your options in this one file outweighs the
 * efficency of doing it the other way.
 *
 */

/****************************************************************************/

/****************************************************************************/


/* GAME PLAY OPTIONS */

/*
 * pk_allowed sets the tone of the entire game.  If pk_allowed is set to
 * NO, then players will not be allowed to kill, summon, charm, or sleep
 * other players, as well as a variety of other "asshole player" protections.
 * However, if you decide you want to have an all-out knock-down drag-out
 * PK Mud, just set pk_allowed to YES - and anything goes.
 */
int pk_allowed = YES;

/* is playerthieving allowed? */
int pt_allowed = YES;

/* minimum level a player must be to shout/holler/gossip/auction */
int level_can_shout = 2;

/* number of movement points it costs to holler */
int holler_move_cost = 20;

/* exp change limits */
long max_exp_gain        = 150000;		/* max gainable per kill */
long max_exp_group_gain  = 1000000;      /* max gainable for group per kill */
long max_exp_loss = 5000000;		        /* max losable per death */


/* Abilita il cap per la differenza di livelli */
int diff_level_cap = YES;

/* 
 *   gap minimo diff livelli per acquisire exp per giocatori di livello inferiore a 15 
 *   da 15 a 30 il gap vale min_level_gap + 5
 *   da 30 a 45 in su il gap vale min_level_gap + 10
 *   da 45 in su vale min_level_gap + 15
 */
int min_level_gap = 10;         

/* number of tics (usually 75 seconds) before PC/NPC corpses decompose */
int max_npc_corpse_time = 5;
int max_pc_corpse_time = 10;

/* How many ticks before a player is sent to the void or idle-rented. */
int idle_void = 18;
int idle_rent_time = 48;

/* This level and up is immune to idling, LVL_IMPL+1 will disable it. */
int idle_max_level = LVL_GOD;

/* should items in death traps automatically be junked? */
int dts_are_dumps = YES;

/*
 * Whether you want items that immortals load to appear on the ground or not.
 * It is most likely best to set this to 'YES' so that something else doesn't
 * grab the item before the immortal does, but that also means people will be
 * able to carry around things like boards.  That's not necessarily a bad
 * thing, but this will be left at a default of 'NO' for historic reasons.
 */
int load_into_inventory = YES;

/* "okay" etc. */
const char *OK = "Okay.\r\n";
const char *NOPERSON = "Non c'e' nessuno con quel nome qui.\r\n";
const char *NOEFFECT = "Non succede nulla.\r\n";

/*
 * You can define or not define TRACK_THOUGH_DOORS, depending on whether
 * or not you want track to find paths which lead through closed or
 * hidden doors. A setting of 'NO' means to not go through the doors
 * while 'YES' will pass through doors to find the target.
 */
int track_through_doors = NO;

/****************************************************************************/

/****************************************************************************/


/* RENT/CRASHSAVE OPTIONS */

/*
 * Should the MUD allow you to 'rent' for free?  (i.e. if you just quit,
 * your objects are saved at no cost, as in Merc-type MUDs.)
 */
int free_rent = NO;

/* maximum number of items players are allowed to rent */
int max_obj_save = 50;

/* receptionist's surcharge on top of item costs */
int min_rent_cost = 100;

/*
 * Should the game automatically save people?  (i.e., save player data
 * every 4 kills (on average), and Crash-save as defined below.  This
 * option has an added meaning past bpl13.  If auto_save is YES, then
 * the 'save' command will be disabled to prevent item duplication via
 * game crashes.
 */
int auto_save = YES;

/*
 * if auto_save (above) is yes, how often (in minutes) should the MUD
 * Crash-save people's objects?   Also, this number indicates how often
 * the MUD will Crash-save players' houses.
 */
int autosave_time = 5;

/* Lifetime of crashfiles and forced-rent (idlesave) files in days */
int crash_file_timeout = 10;

/* Lifetime of normal rent files in days */
int rent_file_timeout = 30;

/* Do you want to automatically wipe players who've been gone too long? */
int auto_pwipe = NO;

/* Autowipe deletion criteria
   This struct holds information used to determine which players to wipe
   then the mud boots.  The levels must be in ascending order, with a
   descending level marking the end of the array.  A level -1 entry in the
   beginning is the case for players with the PLR_DELETED flag.  The
   values below match the stock purgeplay.c criteria.

   Detailed explanation by array element:
   * Element 0, level -1, days 0: Players with PLR_DELETED flag are always
	wiped.
   * Element 1, level 0, days 0: Players at level 0 have created a
	character, but have never actually entered the game, so always
	wipe them.
   * Element 2, level 1, days 4: Players at level 1 are wiped if they
	haven't logged on in the past 4 days.
   * Element 3, level 4, days 7: Players level 2 through 4 are wiped if
	they haven't logged on in the past 7 days.
   * Element 4, level 10, days 30: Players level 5-10 get 30 days.
   * Element 5, level LVL_IMMORT - 1, days 60: All other mortals get
	60 days.
   * Element 6, level LVL_IMPL, days 90: Immortals get 90 days.
   * Element 7: Because -2 is less than LVL_IMPL, this is assumed to
	be the end of the criteria.  The days entry is not used in this
	case.
*/
struct pclean_criteria_data pclean_criteria[] = {
	/*  LEVEL   DAYS    */
	{-1, 0},					/* players with PLR_DELETE flag */
	{0, 0},						/* level 0 */
	{1, 4},
	{4, 7},
	{10, 30},
	{LVL_IMMORT - 1, 60},		/* highest mortal */
	{LVL_IMPL, 90},				/* all immortals */
	{-2, 0}						/* no more level checks */
};

/* Do you want players who self-delete to be wiped immediately with no
   backup?
*/
int selfdelete_fastwipe = YES;

/****************************************************************************/

/****************************************************************************/


/* ROOM NUMBERS */

/* virtual number of room that mortals should enter at */
room_vnum mortal_start_room = 3008;

/* virtual number of room that immorts should enter at by default */
room_vnum immort_start_room = 1204;

/* virtual number of room that frozen players should enter at */
room_vnum frozen_start_room = 1202;

/*
 * virtual numbers of donation rooms.  note: you must change code in
 * do_drop of act.item.c if you change the number of non-NOWHERE
 * donation rooms.
 */
room_vnum donation_room_1 = 3063;
room_vnum donation_room_2 = NOWHERE;	/* unused - room for expansion */
room_vnum donation_room_3 = NOWHERE;	/* unused - room for expansion */


/****************************************************************************/

/****************************************************************************/


/* GAME OPERATION OPTIONS */

/*
 * This is the default port on which the game should run if no port is
 * given on the command-line.  NOTE WELL: If you're using the
 * 'autorun' script, the port number there will override this setting.
 * Change the PORT= line in autorun instead of (or in addition to)
 * changing this.
 */
ush_int DFLT_PORT = 4000;

/*
 * IP address to which the MUD should bind.  This is only useful if
 * you're running Circle on a host that host more than one IP interface,
 * and you only want to bind to *one* of them instead of all of them.
 * Setting this to NULL (the default) causes Circle to bind to all
 * interfaces on the host.  Otherwise, specify a numeric IP address in
 * dotted quad format, and Circle will only bind to that IP address.  (Of
 * course, that IP address must be one of your host's interfaces, or it
 * won't work.)
 */
const char *DFLT_IP = NULL;		/* bind to all interfaces */

/* const char *DFLT_IP = "192.168.1.1";  -- bind only to one interface */

/* default directory to use as data directory */
const char *DFLT_DIR = "lib";

/*
 * What file to log messages to (ex: "log/syslog").  Setting this to NULL
 * means you want to log to stderr, which was the default in earlier
 * versions of Circle.  If you specify a file, you don't get messages to
 * the screen. (Hint: Try 'tail -f' if you have a UNIX machine.)
 */
const char *LOGNAME = NULL;

/* const char *LOGNAME = "log/syslog";  -- useful for Windows users */

/* maximum number of players allowed before game starts to turn people away */
int max_playing = 300;

/* maximum size of bug, typo and idea files in bytes (to prevent bombing) */
int max_filesize = 50000;

/* maximum number of password attempts before disconnection */
int max_bad_pws = 5;

/*
 * Rationale for enabling this, as explained by naved@bird.taponline.com.
 *
 * Usually, when you select ban a site, it is because one or two people are
 * causing troubles while there are still many people from that site who you
 * want to still log on.  Right now if I want to add a new select ban, I need
 * to first add the ban, then SITEOK all the players from that site except for
 * the one or two who I don't want logging on.  Wouldn't it be more convenient
 * to just have to remove the SITEOK flags from those people I want to ban
 * rather than what is currently done?
 */
int siteok_everyone = TRUE;

/*
 * Some nameservers are very slow and cause the game to lag terribly every 
 * time someone logs in.  The lag is caused by the gethostbyaddr() function
 * which is responsible for resolving numeric IP addresses to alphabetic names.
 * Sometimes, nameservers can be so slow that the incredible lag caused by
 * gethostbyaddr() isn't worth the luxury of having names instead of numbers
 * for players' sitenames.
 *
 * If your nameserver is fast, set the variable below to NO.  If your
 * nameserver is slow, of it you would simply prefer to have numbers
 * instead of names for some other reason, set the variable to YES.
 *
 * You can experiment with the setting of nameserver_is_slow on-line using
 * the SLOWNS command from within the MUD.
 */

int nameserver_is_slow = YES;


const char *MENU =
	"\r\n"
    "Fai la tua scelta:\r\n\r\n"
	"0) Esci.\r\n"
	"1) Entra nel mondo di McLandia.\r\n"
	"2) Immetti la descrizione del personaggio.\r\n"
	"3) Leggi la storia di McLandia.\r\n"
	"4) Cambia la password.\r\n"
	"5) Cancella questo personaggio.\r\n" "\r\n" "   Fai la tua scelta: ";



const char *WELC_MESSG = "\r\n" "Benvenuti in McLandia !!" "\r\n\r\n";

const char *START_MESSG =
	"Benvenuto.  Questo e' il tuo personaggio!  Puoi raccogliere soldi,\r\n"
	"acquisire esperienza, acquisire poteri, e molto altro -- mentre\r\n"
	"incontri persone di tutto il mondo!\r\n Per muoverti in McLandia (nord,"
	"sud, est, ovest), usa i tasti n s e w \r\n"
	"Per guardare un oggetto usa il comando look nomeoggetto\r\n"
	"Il resto lo scoprirai da solo. Buona fortuna !!\r\n\r\n\r\n\r\n";

/****************************************************************************/

/****************************************************************************/


/* AUTOWIZ OPTIONS */

/*
 * Should the game automatically create a new wizlist/immlist every time
 * someone immorts, or is promoted to a higher (or lower) god level?
 * NOTE: this only works under UNIX systems.
 */
int use_autowiz = YES;

/* If yes, what is the lowest level which should be on the wizlist?  (All
   immort levels below the level you specify will go on the immlist instead.) */
int min_wizlist_lev = LVL_GOD;
