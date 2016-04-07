
/* ************************************************************************
*   File: db.c                                          Part of CircleMUD *
*  Usage: Loading/saving chars, booting/resetting world, internal funcs   *
*                                                                         *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  Copyright (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */

#define __DB_C__

#include "conf.h"
#include "sysdep.h"


#include "structs.h"
#include "utils.h"
#include "db.h"
#include "comm.h"
#include "handler.h"
#include "spells.h"
#include "mail.h"
#include "interpreter.h"
#include "house.h"
#include "constants.h"
#include "diskio.h"
#include "pfdefaults.h"

/**************************************************************************
*  declarations of most of the 'global' variables                         *
**************************************************************************/

struct room_data *world = NULL;	/* array of rooms        */
room_rnum top_of_world = 0;		/* ref to top element of world   */

struct char_data *character_list = NULL;	/* global linked list of
											   * chars   */
struct index_data *mob_index;	/* index table for mobile file   */
struct char_data *mob_proto;	/* prototypes for mobs       */
mob_rnum top_of_mobt = 0;		/* top of mobile index table     */

struct obj_data *object_list = NULL;	/* global linked list of objs    */
struct index_data *obj_index;	/* index table for object file   */
struct obj_data *obj_proto;		/* prototypes for objs       */
obj_rnum top_of_objt = 0;		/* top of object index table     */

struct zone_data *zone_table;	/* zone table            */
zone_rnum top_of_zone_table = 0;	/* top element of zone tab   */
struct message_list fight_messages[MAX_MESSAGES];	/* fighting messages     */

struct player_index_element *player_table = NULL;	/* index to plr file     */
//FILE *player_fl = NULL;       /* file desc of player file  */
int top_of_p_table = 0;			/* ref to top of table       */
int top_of_p_file = 0;			/* ref of size of p file     */
long top_idnum = 0;				/* highest idnum in use      */

int no_mail = 0;				/* mail disabled?        */
int mini_mud = 0;				/* mini-mud mode?        */
int no_rent_check = 0;			/* skip rent check on boot?  */
time_t boot_time = 0;			/* time of mud boot      */
int circle_restrict = 0;		/* level of game restriction     */
room_rnum r_mortal_start_room;	/* rnum of mortal start room     */
room_rnum r_immort_start_room;	/* rnum of immort start room     */
room_rnum r_frozen_start_room;	/* rnum of frozen start room     */

char *credits = NULL;			/* game credits          */
char *news = NULL;				/* mud news          */
char *motd = NULL;				/* message of the day - mortals */
char *imotd = NULL;				/* message of the day - immorts */
char *GREETINGS = NULL;			/* opening credits screen   */
char *help = NULL;				/* help screen           */
char *info = NULL;				/* info page             */
char *wizlist = NULL;			/* list of higher gods       */
char *immlist = NULL;			/* list of peon gods         */
char *background = NULL;		/* background story      */
char *handbook = NULL;			/* handbook for new immortals    */
char *policies = NULL;			/* policies page         */

struct help_index_element *help_table = 0;	/* the help table    */
int top_of_helpt = 0;			/* top of help index table   */

struct time_info_data time_info;	/* the infomation about the time    */
struct weather_data weather_info;	/* the infomation about the weather */
struct player_special_data dummy_mob;	/* dummy spec area for mobs */
struct reset_q_type reset_q;	/* queue of zones to be reset    */

/* local functions */
int check_object_spell_number (struct obj_data *obj, int val);
int check_object_level (struct obj_data *obj, int val);
void setup_dir (FILE * fl, int room, int dir);
void index_boot (int mode);
void discrete_load (FILE * fl, int mode, char *filename);
int check_object (struct obj_data *);
void parse_room (FILE * fl, int virtual_nr);
void parse_mobile (FILE * mob_f, int nr);
char *parse_object (FILE * obj_f, int nr);
void load_zones (FILE * fl, char *zonename);
void load_help (FILE * fl);
void assign_mobiles (void);
void assign_objects (void);
void assign_rooms (void);
void assign_the_shopkeepers (void);
void build_player_index (void);
int is_empty (zone_rnum zone_nr);
void reset_zone (zone_rnum zone);
int file_to_string (const char *name, char *buf);
int file_to_string_alloc (const char *name, char **buf);
void reboot_wizlists (void);
ACMD (do_reboot);
void boot_world (void);
int count_alias_records (FILE * fl);
int count_hash_records (FILE * fl);
bitvector_t asciiflag_conv (char *flag);
void parse_simple_mob (FILE * mob_f, int i, int nr);
void interpret_espec (const char *keyword, const char *value, int i, int nr);
void parse_espec (char *buf, int i, int nr);
void parse_enhanced_mob (FILE * mob_f, int i, int nr);
void get_one_line (FILE * fl, char *buf);
void save_etext (struct char_data *ch);
void check_start_rooms (void);
void renum_world (void);
void renum_zone_table (void);
void log_zone_error (zone_rnum zone, int cmd_no, const char *message);
void reset_time (void);
long get_ptable_by_name (char *name);

/* external functions */
struct time_info_data *mud_time_passed (time_t t2, time_t t1);
void free_alias (struct alias_data *a);
void load_messages (void);
void weather_and_time (int mode);
void mag_assign_spells (void);
void boot_social_messages (void);
void update_obj_file (void);	/* In objsave.c */
void sort_commands (void);
void sort_spells (void);
void load_banned (void);
void Read_Invalid_List (void);
void boot_the_shops (FILE * shop_f, char *filename, int rec_count);
int find_name (char *name);
int hsort (const void *a, const void *b);
void prune_crlf (char *txt);
void sprintbits (long vektor, char *outstring);
void tag_argument (char *argument, char *tag);
void clean_pfiles (void);
int get_moon_phase (int pos);

/* external vars */
extern int no_specials;
extern int scheck;
extern room_vnum mortal_start_room;
extern room_vnum immort_start_room;
extern room_vnum frozen_start_room;
extern struct descriptor_data *descriptor_list;
extern struct spell_info_type spell_info[];
extern const char *unused_spellname;

/* external ascii pfile vars */
extern struct pclean_criteria_data pclean_criteria[];
extern int selfdelete_fastwipe;
extern int auto_pwipe;

/* ascii pfiles - set this TRUE if you want poofin/poofout
   strings saved in the pfiles
*/
#define ASCII_SAVE_POOFS	TRUE

#define READ_SIZE 256

/*************************************************************************
*  routines for booting the system                                       *
*************************************************************************/

/* this is necessary for the autowiz system */
void
reboot_wizlists (void)
{
	file_to_string_alloc (WIZLIST_FILE, &wizlist);
	file_to_string_alloc (IMMLIST_FILE, &immlist);
}


/*
 * Too bad it doesn't check the return values to let the user
 * know about -1 values.  This will result in an 'Okay.' to a
 * 'reload' command even when the string was not replaced.
 * To fix later, if desired. -gg 6/24/99
 */
ACMD (do_reboot)
{
	int i;

	one_argument (argument, arg);

	if (!str_cmp (arg, "all") || *arg == '*')
	{
		if (file_to_string_alloc (GREETINGS_FILE, &GREETINGS) == 0)
			prune_crlf (GREETINGS);
		file_to_string_alloc (WIZLIST_FILE, &wizlist);
		file_to_string_alloc (IMMLIST_FILE, &immlist);
		file_to_string_alloc (NEWS_FILE, &news);
		file_to_string_alloc (CREDITS_FILE, &credits);
		file_to_string_alloc (MOTD_FILE, &motd);
		file_to_string_alloc (IMOTD_FILE, &imotd);
		file_to_string_alloc (HELP_PAGE_FILE, &help);
		file_to_string_alloc (INFO_FILE, &info);
		file_to_string_alloc (POLICIES_FILE, &policies);
		file_to_string_alloc (HANDBOOK_FILE, &handbook);
		file_to_string_alloc (BACKGROUND_FILE, &background);
	}
	else if (!str_cmp (arg, "wizlist"))
		file_to_string_alloc (WIZLIST_FILE, &wizlist);
	else if (!str_cmp (arg, "immlist"))
		file_to_string_alloc (IMMLIST_FILE, &immlist);
	else if (!str_cmp (arg, "news"))
		file_to_string_alloc (NEWS_FILE, &news);
	else if (!str_cmp (arg, "credits"))
		file_to_string_alloc (CREDITS_FILE, &credits);
	else if (!str_cmp (arg, "motd"))
		file_to_string_alloc (MOTD_FILE, &motd);
	else if (!str_cmp (arg, "imotd"))
		file_to_string_alloc (IMOTD_FILE, &imotd);
	else if (!str_cmp (arg, "help"))
		file_to_string_alloc (HELP_PAGE_FILE, &help);
	else if (!str_cmp (arg, "info"))
		file_to_string_alloc (INFO_FILE, &info);
	else if (!str_cmp (arg, "policy"))
		file_to_string_alloc (POLICIES_FILE, &policies);
	else if (!str_cmp (arg, "handbook"))
		file_to_string_alloc (HANDBOOK_FILE, &handbook);
	else if (!str_cmp (arg, "background"))
		file_to_string_alloc (BACKGROUND_FILE, &background);
	else if (!str_cmp (arg, "greetings"))
	{
		if (file_to_string_alloc (GREETINGS_FILE, &GREETINGS) == 0)
			prune_crlf (GREETINGS);
	}
	else if (!str_cmp (arg, "xhelp"))
	{
		if (help_table)
		{
			for (i = 0; i <= top_of_helpt; i++)
			{
				if (help_table[i].keyword)
					free (help_table[i].keyword);
				if (help_table[i].entry && !help_table[i].duplicate)
					free (help_table[i].entry);
			}
			free (help_table);
		}
		top_of_helpt = 0;
		index_boot (DB_BOOT_HLP);
	}
	else
	{
		send_to_char ("Unknown reload option.\r\n", ch);
		return;
	}

	send_to_char (OK, ch);
}


void
boot_world (void)
{
	log ("Loading zone table.");
	index_boot (DB_BOOT_ZON);

	log ("Loading rooms.");
	index_boot (DB_BOOT_WLD);

	log ("Renumbering rooms.");
	renum_world ();

	log ("Checking start rooms.");
	check_start_rooms ();

	log ("Loading mobs and generating index.");
	index_boot (DB_BOOT_MOB);

	log ("Loading objs and generating index.");
	index_boot (DB_BOOT_OBJ);

	log ("Renumbering zone table.");
	renum_zone_table ();

	if (!no_specials)
	{
		log ("Loading shops.");
		index_boot (DB_BOOT_SHP);
	}
}



/* body of the booting system */
void
boot_db (void)
{
	zone_rnum i;

	log ("Boot db -- BEGIN.");

	log ("Resetting the game time:");
	reset_time ();

	log ("Reading news, credits, help, bground, info & motds.");
	file_to_string_alloc (NEWS_FILE, &news);
	file_to_string_alloc (CREDITS_FILE, &credits);
	file_to_string_alloc (MOTD_FILE, &motd);
	file_to_string_alloc (IMOTD_FILE, &imotd);
	file_to_string_alloc (HELP_PAGE_FILE, &help);
	file_to_string_alloc (INFO_FILE, &info);
	file_to_string_alloc (WIZLIST_FILE, &wizlist);
	file_to_string_alloc (IMMLIST_FILE, &immlist);
	file_to_string_alloc (POLICIES_FILE, &policies);
	file_to_string_alloc (HANDBOOK_FILE, &handbook);
	file_to_string_alloc (BACKGROUND_FILE, &background);
	if (file_to_string_alloc (GREETINGS_FILE, &GREETINGS) == 0)
		prune_crlf (GREETINGS);

	log ("Loading spell definitions.");
	mag_assign_spells ();

	boot_world ();

	log ("Loading help entries.");
	index_boot (DB_BOOT_HLP);

	log ("Generating player index.");
	build_player_index ();

	if (auto_pwipe)
	{
		log ("Cleaning out the pfiles.");
		clean_pfiles ();
	}

	log ("Loading fight messages.");
	load_messages ();

	log ("Loading social messages.");
	boot_social_messages ();

	log ("Assigning function pointers:");

	if (!no_specials)
	{
		log ("   Mobiles.");
		assign_mobiles ();
		log ("   Shopkeepers.");
		assign_the_shopkeepers ();
		log ("   Objects.");
		assign_objects ();
		log ("   Rooms.");
		assign_rooms ();
	}

	log ("Assigning spell and skill levels.");
	init_spell_levels ();

	log ("Sorting command list and spells.");
	sort_commands ();
	sort_spells ();

	log ("Booting mail system.");
	if (!scan_file ())
	{
		log ("    Mail boot failed -- Mail system disabled");
		no_mail = 1;
	}
	log ("Reading banned site and invalid-name list.");
	load_banned ();
	Read_Invalid_List ();

	if (!no_rent_check)
	{
		log ("Deleting timed-out crash and rent files:");
		update_obj_file ();
		log ("   Done.");
	}

	/* Moved here so the object limit code works. -gg 6/24/98 */
	if (!mini_mud)
	{
		log ("Booting houses.");
		House_boot ();
	}

	for (i = 0; i <= top_of_zone_table; i++)
	{
		log ("Resetting %s (rooms %d-%d).", zone_table[i].name,
			 (i ? (zone_table[i - 1].top + 1) : 0), zone_table[i].top);
		reset_zone (i);
	}

	reset_q.head = reset_q.tail = NULL;

	boot_time = time (0);

	log ("Boot db -- DONE.");
}


/* reset the time in the game from file */
void
reset_time (void)
{
#if defined(CIRCLE_MACINTOSH)
	long beginning_of_time = -1561789232;
#else
	long beginning_of_time = 650336715;
#endif

	time_info = *mud_time_passed (time (0), beginning_of_time);

/* modifica anno */
    time_info.year = time_info.year + 1700;
/* fine modifica */

	if (time_info.hours <= 4)
		weather_info.sunlight = SUN_DARK;
	else if (time_info.hours == 5)
		weather_info.sunlight = SUN_RISE;
	else if (time_info.hours <= 20)
		weather_info.sunlight = SUN_LIGHT;
	else if (time_info.hours == 21)
		weather_info.sunlight = SUN_SET;
	else
		weather_info.sunlight = SUN_DARK;

	weather_info.moon1.abs_pos = 180;	// Trammel Start position (piena)
	weather_info.moon2.abs_pos = 0;	// Felucca start position (nuova)

	/* check delle fasi lunari */
	/* luna 1 Trammel */

	weather_info.moon1.phase = get_moon_phase (weather_info.moon1.abs_pos);
	weather_info.moon2.phase = get_moon_phase (weather_info.moon2.abs_pos);

	/* init posizione astri */
	weather_info.sun.rel_pos = 0;
	weather_info.moon1.rel_pos = 180;
	weather_info.moon2.rel_pos = 180;

	if (time_info.hours == 0)
	{
		weather_info.sun.rel_pos = 0;
		weather_info.moon1.rel_pos = 180;
		weather_info.moon2.rel_pos = 180;
	}
	else
	{
		int i;

		for (i = 1; i < time_info.hours + 1; i++)
		{
			weather_info.sun.rel_pos =
				degree_sum (weather_info.sun.rel_pos, 15);
			weather_info.moon1.rel_pos =
				degree_sum (weather_info.moon1.rel_pos, 15);
			weather_info.moon2.rel_pos =
				degree_sum (weather_info.moon2.rel_pos, 15);
		}
	}


	log ("   Current Gametime: %dH %dD %dM %dY.", time_info.hours,
		 time_info.day, time_info.month, time_info.year);

	/* forzato a avere all'inizio cielo sereno alla pressione standard di 1013 mmHg */
	weather_info.pressure = 1013;

	if ((time_info.month >= 7) && (time_info.month <= 12))
		weather_info.pressure += dice (1, 50);
	else
		weather_info.pressure += dice (1, 80);

	weather_info.change = 0;

	if (weather_info.pressure <= 980)
		weather_info.sky = SKY_LIGHTNING;
	else if (weather_info.pressure <= 1000)
		weather_info.sky = SKY_RAINING;
	else if (weather_info.pressure <= 1020)
		weather_info.sky = SKY_CLOUDY;
	else
		weather_info.sky = SKY_CLOUDLESS;

}


/* new version to build player index for ascii pfiles */

/* generate index table for the player file */
void
build_player_index (void)
{
	int rec_count = 0, i;
	FBFILE *plr_index;
	char index_name[40], line[256], bits[64];
	char arg2[80];

	sprintf (index_name, "%s", PLR_INDEX_FILE);
	if (!(plr_index = fbopen (index_name, FB_READ)))
	{
		top_of_p_table = -1;
		log ("No player index file!  First new char will be IMP!");
		return;
	}

	while (fbgetline (plr_index, line))
		if (*line != '~')
			rec_count++;
	fbrewind (plr_index);

	if (rec_count == 0)
	{
		player_table = NULL;
		top_of_p_file = top_of_p_table = -1;
		return;
	}


	CREATE (player_table, struct player_index_element, rec_count);
	for (i = 0; i < rec_count; i++)
	{
		fbgetline (plr_index, line);
		sscanf (line, "%ld %s %d %s %d %d", &player_table[i].id, arg2,
				&player_table[i].level, bits, (int *) &player_table[i].last,
				&player_table[i].autowiz);

		CREATE (player_table[i].name, char, strlen (arg2) + 1);
		strcpy (player_table[i].name, arg2);
		player_table[i].flags = asciiflag_conv (bits);
		top_idnum = MAX (top_idnum, player_table[i].id);
	}

	fbclose (plr_index);
	top_of_p_file = top_of_p_table = i - 1;
}

/*
 * Thanks to Andrey (andrey@alex-ua.com) for this bit of code, although I
 * did add the 'goto' and changed some "while()" into "do { } while()".
 *	-gg 6/24/98 (technically 6/25/98, but I care not.)
 */
int
count_alias_records (FILE * fl)
{
	char key[READ_SIZE], next_key[READ_SIZE];
	char line[READ_SIZE], *scan;
	int total_keywords = 0;

	/* get the first keyword line */
	get_one_line (fl, key);

	while (*key != '$')
	{
		/* skip the text */
		do
		{
			get_one_line (fl, line);
			if (feof (fl))
				goto ackeof;
		}
		while (*line != '#');

		/* now count keywords */
		scan = key;
		do
		{
			scan = one_word (scan, next_key);
			if (*next_key)
				++total_keywords;
		}
		while (*next_key);

		/* get next keyword line (or $) */
		get_one_line (fl, key);

		if (feof (fl))
			goto ackeof;
	}

	return (total_keywords);

	/* No, they are not evil. -gg 6/24/98 */
  ackeof:
	log ("SYSERR: Unexpected end of help file.");
	exit (1);					/* Some day we hope to handle these things better... */
}

/* function to count how many hash-mark delimited records exist in a file */
int
count_hash_records (FILE * fl)
{
	char buf[128];
	int count = 0;

	while (fgets (buf, 128, fl))
		if (*buf == '#')
			count++;

	return (count);
}



void
index_boot (int mode)
{
	const char *index_filename, *prefix = NULL;	/* NULL or egcs 1.1 complains */
	FILE *index, *db_file;
	int rec_count = 0, size[2];

	switch (mode)
	{
	case DB_BOOT_WLD:
		prefix = WLD_PREFIX;
		break;
	case DB_BOOT_MOB:
		prefix = MOB_PREFIX;
		break;
	case DB_BOOT_OBJ:
		prefix = OBJ_PREFIX;
		break;
	case DB_BOOT_ZON:
		prefix = ZON_PREFIX;
		break;
	case DB_BOOT_SHP:
		prefix = SHP_PREFIX;
		break;
	case DB_BOOT_HLP:
		prefix = HLP_PREFIX;
		break;
	default:
		log ("SYSERR: Unknown subcommand %d to index_boot!", mode);
		exit (1);
	}

	if (mini_mud)
		index_filename = MINDEX_FILE;
	else
		index_filename = INDEX_FILE;

	sprintf (buf2, "%s%s", prefix, index_filename);

	if (!(index = fopen (buf2, "r")))
	{
		log ("SYSERR: opening index file '%s': %s", buf2, strerror (errno));
		exit (1);
	}

	/* first, count the number of records in the file so we can malloc */
	fscanf (index, "%s\n", buf1);
	while (*buf1 != '$')
	{
		sprintf (buf2, "%s%s", prefix, buf1);
		if (!(db_file = fopen (buf2, "r")))
		{
			log ("SYSERR: File '%s' listed in '%s/%s': %s", buf2, prefix,
				 index_filename, strerror (errno));
			fscanf (index, "%s\n", buf1);
			continue;
		}
		else
		{
			if (mode == DB_BOOT_ZON)
				rec_count++;
			else if (mode == DB_BOOT_HLP)
				rec_count += count_alias_records (db_file);
			else
				rec_count += count_hash_records (db_file);
		}

		fclose (db_file);
		fscanf (index, "%s\n", buf1);
	}

	/* Exit if 0 records, unless this is shops */
	if (!rec_count)
	{
		if (mode == DB_BOOT_SHP)
			return;
		log ("SYSERR: boot error - 0 records counted in %s/%s.", prefix,
			 index_filename);
		exit (1);
	}

	/* Any idea why you put this here Jeremy? */
	rec_count++;

	/*
	 * NOTE: "bytes" does _not_ include strings or other later malloc'd things.
	 */
	switch (mode)
	{
	case DB_BOOT_WLD:
		CREATE (world, struct room_data, rec_count);
		size[0] = sizeof (struct room_data) * rec_count;
		log ("   %d rooms, %d bytes.", rec_count, size[0]);
		break;
	case DB_BOOT_MOB:
		CREATE (mob_proto, struct char_data, rec_count);
		CREATE (mob_index, struct index_data, rec_count);
		size[0] = sizeof (struct index_data) * rec_count;
		size[1] = sizeof (struct char_data) * rec_count;
		log ("   %d mobs, %d bytes in index, %d bytes in prototypes.",
			 rec_count, size[0], size[1]);
		break;
	case DB_BOOT_OBJ:
		CREATE (obj_proto, struct obj_data, rec_count);
		CREATE (obj_index, struct index_data, rec_count);
		size[0] = sizeof (struct index_data) * rec_count;
		size[1] = sizeof (struct obj_data) * rec_count;
		log ("   %d objs, %d bytes in index, %d bytes in prototypes.",
			 rec_count, size[0], size[1]);
		break;
	case DB_BOOT_ZON:
		CREATE (zone_table, struct zone_data, rec_count);
		size[0] = sizeof (struct zone_data) * rec_count;
		log ("   %d zones, %d bytes.", rec_count, size[0]);
		break;
	case DB_BOOT_HLP:
		CREATE (help_table, struct help_index_element, rec_count);
		size[0] = sizeof (struct help_index_element) * rec_count;
		log ("   %d entries, %d bytes.", rec_count, size[0]);
		break;
	}

	rewind (index);
	fscanf (index, "%s\n", buf1);
	while (*buf1 != '$')
	{
		sprintf (buf2, "%s%s", prefix, buf1);
		if (!(db_file = fopen (buf2, "r")))
		{
			log ("SYSERR: %s: %s", buf2, strerror (errno));
			exit (1);
		}
		switch (mode)
		{
		case DB_BOOT_WLD:
		case DB_BOOT_OBJ:
		case DB_BOOT_MOB:
			discrete_load (db_file, mode, buf2);
			break;
		case DB_BOOT_ZON:
			load_zones (db_file, buf2);
			break;
		case DB_BOOT_HLP:
			/*
			 * If you think about it, we have a race here.  Although, this is the
			 * "point-the-gun-at-your-own-foot" type of race.
			 */
			load_help (db_file);
			break;
		case DB_BOOT_SHP:
			boot_the_shops (db_file, buf2, rec_count);
			break;
		}

		fclose (db_file);
		fscanf (index, "%s\n", buf1);
	}
	fclose (index);

	/* sort the help index */
	if (mode == DB_BOOT_HLP)
	{
		qsort (help_table, top_of_helpt, sizeof (struct help_index_element),
			   hsort);
		top_of_helpt--;
	}
}


void
discrete_load (FILE * fl, int mode, char *filename)
{
	int nr = -1, last;
	char line[256];

	const char *modes[] = { "world", "mob", "obj" };

	for (;;)
	{
		/*
		 * we have to do special processing with the obj files because they have
		 * no end-of-record marker :(
		 */
		if (mode != DB_BOOT_OBJ || nr < 0)
			if (!get_line (fl, line))
			{
				if (nr == -1)
				{
					log ("SYSERR: %s file %s is empty!", modes[mode],
						 filename);
				}
				else
				{
					log ("SYSERR: Format error in %s after %s #%d\n"
						 "...expecting a new %s, but file ended!\n"
						 "(maybe the file is not terminated with '$'?)",
						 filename, modes[mode], nr, modes[mode]);
				}
				exit (1);
			}
		if (*line == '$')
			return;

		if (*line == '#')
		{
			last = nr;
			if (sscanf (line, "#%d", &nr) != 1)
			{
				log ("SYSERR: Format error after %s #%d", modes[mode], last);
				exit (1);
			}
			if (nr >= 99999)
				return;
			else
				switch (mode)
				{
				case DB_BOOT_WLD:
					parse_room (fl, nr);
					break;
				case DB_BOOT_MOB:
					parse_mobile (fl, nr);
					break;
				case DB_BOOT_OBJ:
					strcpy (line, parse_object (fl, nr));
					break;
				}
		}
		else
		{
			log ("SYSERR: Format error in %s file %s near %s #%d",
				 modes[mode], filename, modes[mode], nr);
			log ("SYSERR: ... offending line: '%s'", line);
			exit (1);
		}
	}
}

bitvector_t asciiflag_conv (char *flag)
{
	bitvector_t flags = 0;
	int is_number = 1;
	register char *p;

	for (p = flag; *p; p++)
	{
		if (islower (*p))
			flags |= 1 << (*p - 'a');
		else if (isupper (*p))
			flags |= 1 << (26 + (*p - 'A'));

		if (!isdigit (*p))
			is_number = 0;
	}

	if (is_number)
		flags = atol (flag);

	return (flags);
}


/* load the rooms */
void
parse_room (FILE * fl, int virtual_nr)
{
	static int room_nr = 0, zone = 0;
	int t[10], i;
	char line[256], flags[128];
	struct extra_descr_data *new_descr;

	sprintf (buf2, "room #%d", virtual_nr);

	if (virtual_nr <= (zone ? zone_table[zone - 1].top : -1))
	{
		log ("SYSERR: Room #%d is below zone %d.", virtual_nr, zone);
		exit (1);
	}
	while (virtual_nr > zone_table[zone].top)
		if (++zone > top_of_zone_table)
		{
			log ("SYSERR: Room %d is outside of any zone.", virtual_nr);
			exit (1);
		}
	world[room_nr].zone = zone;
	world[room_nr].number = virtual_nr;
	world[room_nr].name = fread_string (fl, buf2);
	world[room_nr].description = fread_string (fl, buf2);

	if (!get_line (fl, line))
	{
		log
			("SYSERR: Expecting roomflags/sector type of room #%d but file ended!",
			 virtual_nr);
		exit (1);
	}

	if (sscanf (line, " %d %s %d ", t, flags, t + 2) != 3)
	{
		log ("SYSERR: Format error in roomflags/sector type of room #%d",
			 virtual_nr);
		exit (1);
	}
	/* t[0] is the zone number; ignored with the zone-file system */
	world[room_nr].room_flags = asciiflag_conv (flags);
	world[room_nr].sector_type = t[2];

	world[room_nr].func = NULL;
	world[room_nr].contents = NULL;
	world[room_nr].people = NULL;
	world[room_nr].light = 0;	/* Zero light sources */

	for (i = 0; i < NUM_OF_DIRS; i++)
		world[room_nr].dir_option[i] = NULL;

	world[room_nr].ex_description = NULL;

	sprintf (buf, "SYSERR: Format error in room #%d (expecting D/E/S)",
			 virtual_nr);

	for (;;)
	{
		if (!get_line (fl, line))
		{
			log (buf);
			exit (1);
		}
		switch (*line)
		{
		case 'D':
			setup_dir (fl, room_nr, atoi (line + 1));
			break;
		case 'E':
			CREATE (new_descr, struct extra_descr_data, 1);
			new_descr->keyword = fread_string (fl, buf2);
			new_descr->description = fread_string (fl, buf2);
			new_descr->next = world[room_nr].ex_description;
			world[room_nr].ex_description = new_descr;
			break;
		case 'S':				/* end of room */
			top_of_world = room_nr++;
			return;
		default:
			log (buf);
			exit (1);
		}
	}
}



/* read direction data */
void
setup_dir (FILE * fl, int room, int dir)
{
	int t[5];
	char line[256];

	sprintf (buf2, "room #%d, direction D%d", GET_ROOM_VNUM (room), dir);

	CREATE (world[room].dir_option[dir], struct room_direction_data, 1);
	world[room].dir_option[dir]->general_description =
		fread_string (fl, buf2);
	world[room].dir_option[dir]->keyword = fread_string (fl, buf2);

	if (!get_line (fl, line))
	{
		log ("SYSERR: Format error, %s", buf2);
		exit (1);
	}
	if (sscanf (line, " %d %d %d ", t, t + 1, t + 2) != 3)
	{
		log ("SYSERR: Format error, %s", buf2);
		exit (1);
	}
	if (t[0] == 1)
		world[room].dir_option[dir]->exit_info = EX_ISDOOR;
	else if (t[0] == 2)
		world[room].dir_option[dir]->exit_info = EX_ISDOOR | EX_PICKPROOF;
	else
		world[room].dir_option[dir]->exit_info = 0;

	world[room].dir_option[dir]->key = t[1];
	world[room].dir_option[dir]->to_room = t[2];
}


/* make sure the start rooms exist & resolve their vnums to rnums */
void
check_start_rooms (void)
{
	if ((r_mortal_start_room = real_room (mortal_start_room)) < 0)
	{
		log
			("SYSERR:  Mortal start room does not exist.  Change in config.c.");
		exit (1);
	}
	if ((r_immort_start_room = real_room (immort_start_room)) < 0)
	{
		if (!mini_mud)
			log
				("SYSERR:  Warning: Immort start room does not exist.  Change in config.c.");
		r_immort_start_room = r_mortal_start_room;
	}
	if ((r_frozen_start_room = real_room (frozen_start_room)) < 0)
	{
		if (!mini_mud)
			log
				("SYSERR:  Warning: Frozen start room does not exist.  Change in config.c.");
		r_frozen_start_room = r_mortal_start_room;
	}
}


/* resolve all vnums into rnums in the world */
void
renum_world (void)
{
	register int room, door;

	for (room = 0; room <= top_of_world; room++)
		for (door = 0; door < NUM_OF_DIRS; door++)
			if (world[room].dir_option[door])
				if (world[room].dir_option[door]->to_room != NOWHERE)
					world[room].dir_option[door]->to_room =
						real_room (world[room].dir_option[door]->to_room);
}


#define ZCMD zone_table[zone].cmd[cmd_no]

/* resulve vnums into rnums in the zone reset tables */
void
renum_zone_table (void)
{
	int cmd_no, a, b, c, olda, oldb, oldc;
	zone_rnum zone;
	char buf[128];

	for (zone = 0; zone <= top_of_zone_table; zone++)
		for (cmd_no = 0; ZCMD.command != 'S'; cmd_no++)
		{
			a = b = c = 0;
			olda = ZCMD.arg1;
			oldb = ZCMD.arg2;
			oldc = ZCMD.arg3;
			switch (ZCMD.command)
			{
			case 'M':
				a = ZCMD.arg1 = real_mobile (ZCMD.arg1);
				c = ZCMD.arg3 = real_room (ZCMD.arg3);
				break;
			case 'O':
				a = ZCMD.arg1 = real_object (ZCMD.arg1);
				if (ZCMD.arg3 != NOWHERE)
					c = ZCMD.arg3 = real_room (ZCMD.arg3);
				break;
			case 'G':
				a = ZCMD.arg1 = real_object (ZCMD.arg1);
				break;
			case 'E':
				a = ZCMD.arg1 = real_object (ZCMD.arg1);
				break;
			case 'P':
				a = ZCMD.arg1 = real_object (ZCMD.arg1);
				c = ZCMD.arg3 = real_object (ZCMD.arg3);
				break;
			case 'D':
				a = ZCMD.arg1 = real_room (ZCMD.arg1);
				break;
			case 'R':			/* rem obj from room */
				a = ZCMD.arg1 = real_room (ZCMD.arg1);
				b = ZCMD.arg2 = real_object (ZCMD.arg2);
				break;
			}
			if (a < 0 || b < 0 || c < 0)
			{
				if (!mini_mud)
				{
					sprintf (buf, "Invalid vnum %d, cmd disabled",
							 (a < 0) ? olda : ((b < 0) ? oldb : oldc));
					log_zone_error (zone, cmd_no, buf);
				}
				ZCMD.command = '*';
			}
		}
}



void
parse_simple_mob (FILE * mob_f, int i, int nr)
{
	int j, t[10];
	char line[256];

	mob_proto[i].real_abils.str = 11;
	mob_proto[i].real_abils.intel = 11;
	mob_proto[i].real_abils.wis = 11;
	mob_proto[i].real_abils.dex = 11;
	mob_proto[i].real_abils.con = 11;
	mob_proto[i].real_abils.cha = 11;

	if (!get_line (mob_f, line))
	{
		log ("SYSERR: Format error in mob #%d, file ended after S flag!", nr);
		exit (1);
	}

	if (sscanf (line, " %d %d %d %dd%d+%d %dd%d+%d ",
				t, t + 1, t + 2, t + 3, t + 4, t + 5, t + 6, t + 7,
				t + 8) != 9)
	{
		log ("SYSERR: Format error in mob #%d, first line after S flag\n"
			 "...expecting line of form '# # # #d#+# #d#+#'", nr);
		exit (1);
	}

	GET_LEVEL (mob_proto + i) = t[0];
	mob_proto[i].points.hitroll = 20 - t[1];
	mob_proto[i].points.armor = 10 * t[2];

	/* max hit = 0 is a flag that H, M, V is xdy+z */
	mob_proto[i].points.max_hit = 0;
	mob_proto[i].points.hit = t[3];
	mob_proto[i].points.mana = t[4];
	mob_proto[i].points.move = t[5];

	mob_proto[i].points.max_mana = 10;
	mob_proto[i].points.max_move = 50;

	mob_proto[i].mob_specials.damnodice = t[6];
	mob_proto[i].mob_specials.damsizedice = t[7];
	mob_proto[i].points.damroll = t[8];

	if (!get_line (mob_f, line))
	{
		log ("SYSERR: Format error in mob #%d, second line after S flag\n"
			 "...expecting line of form '# #', but file ended!", nr);
		exit (1);
	}

	if (sscanf (line, " %d %d ", t, t + 1) != 2)
	{
		log ("SYSERR: Format error in mob #%d, second line after S flag\n"
			 "...expecting line of form '# #'", nr);
		exit (1);
	}

	GET_GOLD (mob_proto + i) = t[0];
	GET_EXP (mob_proto + i) = t[1];

	if (!get_line (mob_f, line))
	{
		log ("SYSERR: Format error in last line of mob #%d\n"
			 "...expecting line of form '# # #', but file ended!", nr);
		exit (1);
	}

	if (sscanf (line, " %d %d %d %d ", t, t + 1, t + 2, t + 3) != 3)
	{
		log ("SYSERR: Format error in last line of mob #%d\n"
			 "...expecting line of form '# # #'", nr);
		exit (1);
	}

	mob_proto[i].char_specials.position = t[0];
	mob_proto[i].mob_specials.default_pos = t[1];
	mob_proto[i].player.sex = t[2];

	mob_proto[i].player.chclass = -1;
	mob_proto[i].player.race = 0;
	mob_proto[i].player.weight = 200;
	mob_proto[i].player.height = 198;

	/*
	 * these are now save applies; base save numbers for MOBs are now from
	 * the warrior save table.
	 */
	for (j = 0; j < 5; j++)
		GET_SAVE (mob_proto + i, j) = 0;
}


/*
 * interpret_espec is the function that takes espec keywords and values
 * and assigns the correct value to the mob as appropriate.  Adding new
 * e-specs is absurdly easy -- just add a new CASE statement to this
 * function!  No other changes need to be made anywhere in the code.
 */

#define CASE(test) if (!matched && !str_cmp(keyword, test) && (matched = 1))
#define RANGE(low, high) (num_arg = MAX((low), MIN((high), (num_arg))))

void
interpret_espec (const char *keyword, const char *value, int i, int nr)
{
	int num_arg, matched = 0;

	num_arg = atoi (value);

	CASE ("BareHandAttack")
	{
		RANGE (0, 99);
		mob_proto[i].mob_specials.attack_type = num_arg;
	}

	CASE ("Str")
	{
		RANGE (3, 25);
		mob_proto[i].real_abils.str = num_arg;
	}

	CASE ("StrAdd")
	{
		RANGE (0, 100);
		mob_proto[i].real_abils.str_add = num_arg;
	}

	CASE ("Int")
	{
		RANGE (3, 25);
		mob_proto[i].real_abils.intel = num_arg;
	}

	CASE ("Wis")
	{
		RANGE (3, 25);
		mob_proto[i].real_abils.wis = num_arg;
	}

	CASE ("Dex")
	{
		RANGE (3, 25);
		mob_proto[i].real_abils.dex = num_arg;
	}

	CASE ("Con")
	{
		RANGE (3, 25);
		mob_proto[i].real_abils.con = num_arg;
	}

	CASE ("Cha")
	{
		RANGE (3, 25);
		mob_proto[i].real_abils.cha = num_arg;
	}

    CASE ("Class")
    {
        RANGE(0, NUM_NPC_CLASSES);
        mob_proto[i].player.chclass = num_arg;
    }

	if (!matched)
	{
		log ("SYSERR: Warning: unrecognized espec keyword %s in mob #%d",
			 keyword, nr);
	}
}

#undef CASE
#undef RANGE

void
parse_espec (char *buf, int i, int nr)
{
	char *ptr;

	if ((ptr = strchr (buf, ':')) != NULL)
	{
		*(ptr++) = '\0';
		while (isspace (*ptr))
			ptr++;
#if 0							/* Need to evaluate interpret_espec()'s NULL handling. */
	}
#else
	}
	else
		ptr = "";
#endif
	interpret_espec (buf, ptr, i, nr);
}


void
parse_enhanced_mob (FILE * mob_f, int i, int nr)
{
	char line[256];

	parse_simple_mob (mob_f, i, nr);

	while (get_line (mob_f, line))
	{
		if (!strcmp (line, "E"))	/* end of the enhanced section */
			return;
		else if (*line == '#')
		{						/* we've hit the next mob, maybe? */
			log ("SYSERR: Unterminated E section in mob #%d", nr);
			exit (1);
		}
		else
			parse_espec (line, i, nr);
	}

	log ("SYSERR: Unexpected end of file reached after mob #%d", nr);
	exit (1);
}


void
parse_mobile (FILE * mob_f, int nr)
{
	static int i = 0;
	int j, t[10];
	char line[256], *tmpptr, letter;
	char f1[128], f2[128];

	mob_index[i].vnum = nr;
	mob_index[i].number = 0;
	mob_index[i].func = NULL;

	clear_char (mob_proto + i);

	/*
	 * Mobiles should NEVER use anything in the 'player_specials' structure.
	 * The only reason we have every mob in the game share this copy of the
	 * structure is to save newbie coders from themselves. -gg 2/25/98
	 */
	mob_proto[i].player_specials = &dummy_mob;
	sprintf (buf2, "mob vnum %d", nr);

  /***** String data *****/
	mob_proto[i].player.name = fread_string (mob_f, buf2);
	tmpptr = mob_proto[i].player.short_descr = fread_string (mob_f, buf2);
	if (tmpptr && *tmpptr)
		if (!str_cmp (fname (tmpptr), "a") || !str_cmp (fname (tmpptr), "an")
			|| !str_cmp (fname (tmpptr), "the"))
			*tmpptr = LOWER (*tmpptr);
	mob_proto[i].player.long_descr = fread_string (mob_f, buf2);
	mob_proto[i].player.description = fread_string (mob_f, buf2);
	mob_proto[i].player.title = NULL;

	/* *** Numeric data *** */
	if (!get_line (mob_f, line))
	{
		log ("SYSERR: Format error after string section of mob #%d\n"
			 "...expecting line of form '# # # {S | E}', but file ended!",
			 nr);
		exit (1);
	}

#ifdef CIRCLE_ACORN				/* Ugh. */
	if (sscanf (line, "%s %s %d %s", f1, f2, t + 2, &letter) != 4)
	{
#else
	if (sscanf (line, "%s %s %d %c", f1, f2, t + 2, &letter) != 4)
	{
#endif
		log ("SYSERR: Format error after string section of mob #%d\n"
			 "...expecting line of form '# # # {S | E}'", nr);
		exit (1);
	}
	MOB_FLAGS (mob_proto + i) = asciiflag_conv (f1);
	SET_BIT (MOB_FLAGS (mob_proto + i), MOB_ISNPC);
	AFF_FLAGS (mob_proto + i) = asciiflag_conv (f2);
	GET_ALIGNMENT (mob_proto + i) = t[2];

	/* AGGR_TO_ALIGN is ignored if the mob is AGGRESSIVE. */
	if (MOB_FLAGGED (mob_proto + i, MOB_AGGRESSIVE)
		&& MOB_FLAGGED (mob_proto + i,
						MOB_AGGR_GOOD | MOB_AGGR_EVIL | MOB_AGGR_NEUTRAL))
		log ("SYSERR: Mob #%d both Aggressive and Aggressive_to_Alignment.",
			 nr);

	switch (UPPER (letter))
	{
	case 'S':					/* Simple monsters */
		parse_simple_mob (mob_f, i, nr);
		break;
	case 'E':					/* Circle3 Enhanced monsters */
		parse_enhanced_mob (mob_f, i, nr);
		break;
		/* add new mob types here.. */
	default:
		log ("SYSERR: Unsupported mob type '%c' in mob #%d", letter, nr);
		exit (1);
	}

	mob_proto[i].aff_abils = mob_proto[i].real_abils;

	for (j = 0; j < NUM_WEARS; j++)
		mob_proto[i].equipment[j] = NULL;

	mob_proto[i].nr = i;
	mob_proto[i].desc = NULL;

	top_of_mobt = i++;
}




/* read all objects from obj file; generate index and prototypes */
char *
parse_object (FILE * obj_f, int nr)
{
	static int i = 0;
	static char line[256];
	int t[10], j, retval;
	char *tmpptr;
	char f1[256], f2[256], c[255];
	struct extra_descr_data *new_descr;

	obj_index[i].vnum = nr;
	obj_index[i].number = 0;
	obj_index[i].func = NULL;

	clear_object (obj_proto + i);
	obj_proto[i].item_number = i;

	sprintf (buf2, "object #%d", nr);

	/* *** string data *** */
	if ((obj_proto[i].name = fread_string (obj_f, buf2)) == NULL)
	{
		log ("SYSERR: Null obj name or format error at or near %s", buf2);
		exit (1);
	}
	tmpptr = obj_proto[i].short_description = fread_string (obj_f, buf2);
	if (tmpptr && *tmpptr)
		if (!str_cmp (fname (tmpptr), "a") || !str_cmp (fname (tmpptr), "an")
			|| !str_cmp (fname (tmpptr), "the"))
			*tmpptr = LOWER (*tmpptr);

	tmpptr = obj_proto[i].description = fread_string (obj_f, buf2);
	if (tmpptr && *tmpptr)
		CAP (tmpptr);
	obj_proto[i].action_description = fread_string (obj_f, buf2);

	/* *** numeric data *** */
	if (!get_line (obj_f, line))
	{
		log ("SYSERR: Expecting first numeric line of %s, but file ended!",
			 buf2);
		exit (1);
	}
	if ((retval = sscanf (line, " %d %s %s", t, f1, f2)) != 3)
	{
		log
			("SYSERR: Format error in first numeric line (expecting 3 args, got %d), %s",
			 retval, buf2);
		exit (1);
	}
	obj_proto[i].obj_flags.type_flag = t[0];
	obj_proto[i].obj_flags.extra_flags = asciiflag_conv (f1);
	obj_proto[i].obj_flags.wear_flags = asciiflag_conv (f2);

	if (!get_line (obj_f, line))
	{
		log ("SYSERR: Expecting second numeric line of %s, but file ended!",
			 buf2);
		exit (1);
	}
	if ((retval = sscanf (line, "%d %d %d %d", t, t + 1, t + 2, t + 3)) != 4)
	{
		log
			("SYSERR: Format error in second numeric line (expecting 4 args, got %d), %s",
			 retval, buf2);
		exit (1);
	}
	obj_proto[i].obj_flags.value[0] = t[0];
	obj_proto[i].obj_flags.value[1] = t[1];
	obj_proto[i].obj_flags.value[2] = t[2];
	obj_proto[i].obj_flags.value[3] = t[3];

	if (!get_line (obj_f, line))
	{
		log ("SYSERR: Expecting third numeric line of %s, but file ended!",
			 buf2);
		exit (1);
	}
	if ((retval = sscanf (line, "%d %d %d", t, t + 1, t + 2)) != 3)
	{
		log
			("SYSERR: Format error in third numeric line (expecting 3 args, got %d), %s",
			 retval, buf2);
		exit (1);
	}
	obj_proto[i].obj_flags.weight = t[0];
	obj_proto[i].obj_flags.cost = t[1];
	obj_proto[i].obj_flags.cost_per_day = t[2];

	/* check to make sure that weight of containers exceeds curr. quantity */
	if (obj_proto[i].obj_flags.type_flag == ITEM_DRINKCON ||
		obj_proto[i].obj_flags.type_flag == ITEM_FOUNTAIN)
	{
		if (obj_proto[i].obj_flags.weight < obj_proto[i].obj_flags.value[1])
			obj_proto[i].obj_flags.weight =
				obj_proto[i].obj_flags.value[1] + 5;
	}

	/* *** extra descriptions and affect fields *** */

	for (j = 0; j < MAX_OBJ_AFFECT; j++)
	{
		obj_proto[i].affected[j].location = APPLY_NONE;
		obj_proto[i].affected[j].modifier = 0;
	}

	strcat (buf2, ", after numeric constants\n"
			"...expecting 'E', 'A', '$', or next object number");
	j = 0;

	for (;;)
	{
		if (!get_line (obj_f, line))
		{
			log ("SYSERR: Format error in %s", buf2);
			exit (1);
		}
		switch (*line)
		{
		case 'E':
			CREATE (new_descr, struct extra_descr_data, 1);
			new_descr->keyword = fread_string (obj_f, buf2);
			new_descr->description = fread_string (obj_f, buf2);
			new_descr->next = obj_proto[i].ex_description;
			obj_proto[i].ex_description = new_descr;
			break;
		case 'A':
			if (j >= MAX_OBJ_AFFECT)
			{
				log ("SYSERR: Too many A fields (%d max), %s", MAX_OBJ_AFFECT,
					 buf2);
				exit (1);
			}
			if (!get_line (obj_f, line))
			{
				log ("SYSERR: Format error in 'A' field, %s\n"
					 "...expecting 2 numeric constants but file ended!",
					 buf2);
				exit (1);
			}

			if ((retval = sscanf (line, " %d %d ", t, t + 1)) != 2)
			{
				log ("SYSERR: Format error in 'A' field, %s\n"
					 "...expecting 2 numeric arguments, got %d\n"
					 "...offending line: '%s'", buf2, retval, line);
				exit (1);
			}
			obj_proto[i].affected[j].location = t[0];
			obj_proto[i].affected[j].modifier = t[1];
			j++;
			break;
		case 'C':
			get_line (obj_f, line);
			/* old
            sscanf (line, "%d ", t);
            obj_proto[i].obj_flags.bitvector =
				obj_proto[i].obj_flags.bitvector | t[0];
             */
            sscanf (line, "%s ", c);
            obj_proto[i].obj_flags.bitvector = obj_proto[i].obj_flags.bitvector | asciiflag_conv(c);

			break;
		case '$':
		case '#':
			check_object (&obj_proto[i]);
			top_of_objt = i++;
			return (line);
		default:
			log ("SYSERR: Format error in (%c): %s", *line, buf2);
			exit (1);
		}
	}
}


#define Z	zone_table[zone]

/* load the zone table and command tables */
void
load_zones (FILE * fl, char *zonename)
{
	static zone_rnum zone = 0;
	int cmd_no, num_of_cmds = 0, line_num = 0, tmp, error;
	char *ptr, buf[256], zname[256];

	strcpy (zname, zonename);

	/* Skip first 3 lines lest we mistake the zone name for a command. */
	for (tmp = 0; tmp < 3; tmp++)
		get_line (fl, buf);

	/* More accurate count. Previous was always 4 or 5 too high. -gg 2001/1/17 */
	while (get_line (fl, buf))
		if ((strchr ("MOGEPDR", buf[0]) && buf[1] == ' ')
			|| (buf[0] == 'S' && buf[1] == '\0'))
			num_of_cmds++;

	rewind (fl);

	if (num_of_cmds == 0)
	{
		log ("SYSERR: %s is empty!", zname);
		exit (1);
	}
	else
		CREATE (Z.cmd, struct reset_com, num_of_cmds);

	line_num += get_line (fl, buf);

	if (sscanf (buf, "#%hd", &Z.number) != 1)
	{
		log ("SYSERR: Format error in %s, line %d", zname, line_num);
		exit (1);
	}
	sprintf (buf2, "beginning of zone #%d", Z.number);

	line_num += get_line (fl, buf);
	if ((ptr = strchr (buf, '~')) != NULL)	/* take off the '~' if it's there */
		*ptr = '\0';
	Z.name = str_dup (buf);

	line_num += get_line (fl, buf);
	if (sscanf (buf, " %hd %d %d ", &Z.top, &Z.lifespan, &Z.reset_mode) != 3)
	{
		log ("SYSERR: Format error in 3-constant line of %s", zname);
		exit (1);
	}
	cmd_no = 0;

	for (;;)
	{
		if ((tmp = get_line (fl, buf)) == 0)
		{
			log ("SYSERR: Format error in %s - premature end of file", zname);
			exit (1);
		}
		line_num += tmp;
		ptr = buf;
		skip_spaces (&ptr);

		if ((ZCMD.command = *ptr) == '*')
			continue;

		ptr++;

		if (ZCMD.command == 'S' || ZCMD.command == '$')
		{
			ZCMD.command = 'S';
			break;
		}
		error = 0;
		if (strchr ("MOEPD", ZCMD.command) == NULL)
		{						/* a 3-arg command */
			if (sscanf (ptr, " %d %d %d ", &tmp, &ZCMD.arg1, &ZCMD.arg2) != 3)
				error = 1;
		}
		else
		{
			if (sscanf (ptr, " %d %d %d %d ", &tmp, &ZCMD.arg1, &ZCMD.arg2,
						&ZCMD.arg3) != 4)
				error = 1;
		}

		ZCMD.if_flag = tmp;

		if (error)
		{
			log ("SYSERR: Format error in %s, line %d: '%s'", zname, line_num,
				 buf);
			exit (1);
		}
		ZCMD.line = line_num;
		cmd_no++;
	}

	if (num_of_cmds != cmd_no + 1)
	{
		log
			("SYSERR: Zone command count mismatch for %s. Estimated: %d, Actual: %d",
			 zname, num_of_cmds, cmd_no + 1);
		exit (1);
	}

	top_of_zone_table = zone++;
}

#undef Z


void
get_one_line (FILE * fl, char *buf)
{
	if (fgets (buf, READ_SIZE, fl) == NULL)
	{
		log ("SYSERR: error reading help file: not terminated with $?");
		exit (1);
	}

	buf[strlen (buf) - 1] = '\0';	/* take off the trailing \n */
}


void
load_help (FILE * fl)
{
#if defined(CIRCLE_MACINTOSH)
	static char key[READ_SIZE + 1], next_key[READ_SIZE + 1], entry[32384];	/* ? */
#else
	char key[READ_SIZE + 1], next_key[READ_SIZE + 1], entry[32384];
#endif
	char line[READ_SIZE + 1], *scan;
	struct help_index_element el;

	/* get the first keyword line */
	get_one_line (fl, key);
	while (*key != '$')
	{
		/* read in the corresponding help entry */
		strcpy (entry, strcat (key, "\r\n"));
		get_one_line (fl, line);
		while (*line != '#')
		{
			strcat (entry, strcat (line, "\r\n"));
			get_one_line (fl, line);
		}

		/* now, add the entry to the index with each keyword on the keyword line */
		el.duplicate = 0;
		el.entry = str_dup (entry);
		scan = one_word (key, next_key);
		while (*next_key)
		{
			el.keyword = str_dup (next_key);
			help_table[top_of_helpt++] = el;
			el.duplicate++;
			scan = one_word (scan, next_key);
		}

		/* get next keyword line (or $) */
		get_one_line (fl, key);
	}
}


int
hsort (const void *a, const void *b)
{
	const struct help_index_element *a1, *b1;

	a1 = (const struct help_index_element *) a;
	b1 = (const struct help_index_element *) b;

	return (str_cmp (a1->keyword, b1->keyword));
}


/*************************************************************************
*  procedures for resetting, both play-time and boot-time	 	 *
*************************************************************************/



int
vnum_mobile (char *searchname, struct char_data *ch)
{
	int nr, found = 0;

	for (nr = 0; nr <= top_of_mobt; nr++)
	{
		if (isname (searchname, mob_proto[nr].player.name))
		{
			sprintf (buf, "%3d. [%5d] %s\r\n", ++found,
					 mob_index[nr].vnum, mob_proto[nr].player.short_descr);
			send_to_char (buf, ch);
		}
	}

	return (found);
}



int
vnum_object (char *searchname, struct char_data *ch)
{
	int nr, found = 0;

	for (nr = 0; nr <= top_of_objt; nr++)
	{
		if (isname (searchname, obj_proto[nr].name))
		{
			sprintf (buf, "%3d. [%5d] %s\r\n", ++found,
					 obj_index[nr].vnum, obj_proto[nr].short_description);
			send_to_char (buf, ch);
		}
	}
	return (found);
}


/* create a character, and add it to the char list */
struct char_data *
create_char (void)
{
	struct char_data *ch;

	CREATE (ch, struct char_data, 1);
	clear_char (ch);
	ch->next = character_list;
	character_list = ch;

	return (ch);
}


/* create a new mobile from a prototype */
struct char_data *
read_mobile (mob_vnum nr, int type)	/* and mob_rnum */
{
	mob_rnum i;
	struct char_data *mob;

	if (type == VIRTUAL)
	{
		if ((i = real_mobile (nr)) < 0)
		{
			log ("WARNING: Mobile vnum %d does not exist in database.", nr);
			return (NULL);
		}
	}
	else
		i = nr;

	CREATE (mob, struct char_data, 1);
	clear_char (mob);
	*mob = mob_proto[i];
	mob->next = character_list;
	character_list = mob;

	if (!mob->points.max_hit)
	{
		mob->points.max_hit = dice (mob->points.hit, mob->points.mana) +
			mob->points.move;
	}
	else
		mob->points.max_hit = number (mob->points.hit, mob->points.mana);

	mob->points.hit = mob->points.max_hit;
	mob->points.mana = mob->points.max_mana;
	mob->points.move = mob->points.max_move;

	mob->player.time.birth = time (0);
	mob->player.time.played = 0;
	mob->player.time.logon = time (0);

	mob_index[i].number++;

	return (mob);
}


/* create an object, and add it to the object list */
struct obj_data *
create_obj (void)
{
	struct obj_data *obj;

	CREATE (obj, struct obj_data, 1);
	clear_object (obj);
	obj->next = object_list;
	object_list = obj;

	return (obj);
}


/* create a new object from a prototype */
struct obj_data *
read_object (obj_vnum nr, int type)	/* and obj_rnum */
{
	struct obj_data *obj;
	obj_rnum i;

	if (nr < 0)
	{
		log ("SYSERR: Trying to create obj with negative (%d) num!", nr);
		return (NULL);
	}
	if (type == VIRTUAL)
	{
		if ((i = real_object (nr)) < 0)
		{
			log ("Object (V) %d does not exist in database.", nr);
			return (NULL);
		}
	}
	else
		i = nr;

	CREATE (obj, struct obj_data, 1);
	clear_object (obj);
	*obj = obj_proto[i];
	obj->next = object_list;
	object_list = obj;

	obj_index[i].number++;

	return (obj);
}



#define ZO_DEAD  999

/* update zone ages, queue for reset if necessary, and dequeue when possible */
void
zone_update (void)
{
	int i;
	struct reset_q_element *update_u, *temp;
	static int timer = 0;
	char buf[128];

	/* jelson 10/22/92 */
	if (((++timer * PULSE_ZONE) / PASSES_PER_SEC) >= 60)
	{
		/* one minute has passed */
		/*
		 * NOT accurate unless PULSE_ZONE is a multiple of PASSES_PER_SEC or a
		 * factor of 60
		 */

		timer = 0;

		/* since one minute has passed, increment zone ages */
		for (i = 0; i <= top_of_zone_table; i++)
		{
			if (zone_table[i].age < zone_table[i].lifespan &&
				zone_table[i].reset_mode)
				(zone_table[i].age)++;

			if (zone_table[i].age >= zone_table[i].lifespan &&
				zone_table[i].age < ZO_DEAD && zone_table[i].reset_mode)
			{
				/* enqueue zone */

				CREATE (update_u, struct reset_q_element, 1);

				update_u->zone_to_reset = i;
				update_u->next = 0;

				if (!reset_q.head)
					reset_q.head = reset_q.tail = update_u;
				else
				{
					reset_q.tail->next = update_u;
					reset_q.tail = update_u;
				}

				zone_table[i].age = ZO_DEAD;
			}
		}
	}							/* end - one minute has passed */


	/* dequeue zones (if possible) and reset */
	/* this code is executed every 10 seconds (i.e. PULSE_ZONE) */
	for (update_u = reset_q.head; update_u; update_u = update_u->next)
		if (zone_table[update_u->zone_to_reset].reset_mode == 2 ||
			is_empty (update_u->zone_to_reset))
		{
			reset_zone (update_u->zone_to_reset);
			sprintf (buf, "Auto zone reset: %s",
					 zone_table[update_u->zone_to_reset].name);
			mudlog (buf, CMP, LVL_GOD, FALSE);
			/* dequeue */
			if (update_u == reset_q.head)
				reset_q.head = reset_q.head->next;
			else
			{
				for (temp = reset_q.head; temp->next != update_u;
					 temp = temp->next);

				if (!update_u->next)
					reset_q.tail = temp;

				temp->next = update_u->next;
			}

			free (update_u);
			break;
		}
}

void
log_zone_error (zone_rnum zone, int cmd_no, const char *message)
{
	char buf[256];

	sprintf (buf, "SYSERR: zone file: %s", message);
	mudlog (buf, NRM, LVL_GOD, TRUE);

	sprintf (buf, "SYSERR: ...offending cmd: '%c' cmd in zone #%d, line %d",
			 ZCMD.command, zone_table[zone].number, ZCMD.line);
	mudlog (buf, NRM, LVL_GOD, TRUE);
}

#define ZONE_ERROR(message) \
	{ log_zone_error(zone, cmd_no, message); last_cmd = 0; }

/* execute the reset command table of a given zone */
void
reset_zone (zone_rnum zone)
{
	int cmd_no, last_cmd = 0;
	struct char_data *mob = NULL;
	struct obj_data *obj, *obj_to;

	for (cmd_no = 0; ZCMD.command != 'S'; cmd_no++)
	{

		if (ZCMD.if_flag && !last_cmd)
			continue;

		switch (ZCMD.command)
		{
		case '*':				/* ignore command */
			last_cmd = 0;
			break;

		case 'M':				/* read a mobile */
			if (mob_index[ZCMD.arg1].number < ZCMD.arg2)
			{
				mob = read_mobile (ZCMD.arg1, REAL);
				char_to_room (mob, ZCMD.arg3);
				last_cmd = 1;
			}
			else
				last_cmd = 0;
			break;

		case 'O':				/* read an object */
			if (obj_index[ZCMD.arg1].number < ZCMD.arg2)
			{
				if (ZCMD.arg3 >= 0)
				{
					obj = read_object (ZCMD.arg1, REAL);
					obj_to_room (obj, ZCMD.arg3);
					last_cmd = 1;
				}
				else
				{
					obj = read_object (ZCMD.arg1, REAL);
					obj->in_room = NOWHERE;
					last_cmd = 1;
				}
			}
			else
				last_cmd = 0;
			break;

		case 'P':				/* object to object */
			if (obj_index[ZCMD.arg1].number < ZCMD.arg2)
			{
				obj = read_object (ZCMD.arg1, REAL);
				if (!(obj_to = get_obj_num (ZCMD.arg3)))
				{
					ZONE_ERROR ("target obj not found, command disabled");
					ZCMD.command = '*';
					break;
				}
				obj_to_obj (obj, obj_to);
				last_cmd = 1;
			}
			else
				last_cmd = 0;
			break;

		case 'G':				/* obj_to_char */
			if (!mob)
			{
				ZONE_ERROR
					("attempt to give obj to non-existant mob, command disabled");
				ZCMD.command = '*';
				break;
			}
			if (obj_index[ZCMD.arg1].number < ZCMD.arg2)
			{
				obj = read_object (ZCMD.arg1, REAL);
				obj_to_char (obj, mob);
				last_cmd = 1;
			}
			else
				last_cmd = 0;
			break;

		case 'E':				/* object to equipment list */
			if (!mob)
			{
				ZONE_ERROR
					("trying to equip non-existant mob, command disabled");
				ZCMD.command = '*';
				break;
			}
			if (obj_index[ZCMD.arg1].number < ZCMD.arg2)
			{
				if (ZCMD.arg3 < 0 || ZCMD.arg3 >= NUM_WEARS)
				{
					ZONE_ERROR ("invalid equipment pos number");
				}
				else
				{
					obj = read_object (ZCMD.arg1, REAL);
					equip_char (mob, obj, ZCMD.arg3);
					last_cmd = 1;
				}
			}
			else
				last_cmd = 0;
			break;

		case 'R':				/* rem obj from room */
			if (
				(obj =
				 get_obj_in_list_num (ZCMD.arg2,
									  world[ZCMD.arg1].contents)) != NULL)
				extract_obj (obj);
			last_cmd = 1;
			break;


		case 'D':				/* set state of door */
			if (ZCMD.arg2 < 0 || ZCMD.arg2 >= NUM_OF_DIRS ||
				(world[ZCMD.arg1].dir_option[ZCMD.arg2] == NULL))
			{
				ZONE_ERROR ("door does not exist, command disabled");
				ZCMD.command = '*';
			}
			else
				switch (ZCMD.arg3)
				{
				case 0:
					REMOVE_BIT (world[ZCMD.arg1].
								dir_option[ZCMD.arg2]->exit_info, EX_LOCKED);
					REMOVE_BIT (world[ZCMD.arg1].
								dir_option[ZCMD.arg2]->exit_info, EX_CLOSED);
					break;
				case 1:
					SET_BIT (world[ZCMD.arg1].
							 dir_option[ZCMD.arg2]->exit_info, EX_CLOSED);
					REMOVE_BIT (world[ZCMD.arg1].
								dir_option[ZCMD.arg2]->exit_info, EX_LOCKED);
					break;
				case 2:
					SET_BIT (world[ZCMD.arg1].
							 dir_option[ZCMD.arg2]->exit_info, EX_LOCKED);
					SET_BIT (world[ZCMD.arg1].
							 dir_option[ZCMD.arg2]->exit_info, EX_CLOSED);
					break;
				}
			last_cmd = 1;
			break;

		default:
			ZONE_ERROR ("unknown cmd in reset table; cmd disabled");
			ZCMD.command = '*';
			break;
		}
	}

	zone_table[zone].age = 0;
}



/* for use in reset_zone; return TRUE if zone 'nr' is free of PC's  */
int
is_empty (zone_rnum zone_nr)
{
	struct descriptor_data *i;

	for (i = descriptor_list; i; i = i->next)
	{
		if (STATE (i) != CON_PLAYING)
			continue;
		if (IN_ROOM (i->character) == NOWHERE)
			continue;
		if (GET_LEVEL (i->character) >= LVL_IMMORT)
			continue;
		if (world[i->character->in_room].zone != zone_nr)
			continue;

		return (0);
	}

	return (1);
}





/*************************************************************************
*  stuff related to the save/load player system				 *
*************************************************************************/


long
get_ptable_by_name (char *name)
{
	int i;

	one_argument (name, arg);
	for (i = 0; i <= top_of_p_table; i++)
		if (!strcmp (player_table[i].name, arg))
			return (i);

	return (-1);
}


long
get_id_by_name (char *name)
{
	int i;

	one_argument (name, arg);
	for (i = 0; i <= top_of_p_table; i++)
		if (!strcmp (player_table[i].name, arg))
			return (player_table[i].id);

	return (-1);
}


char *
get_name_by_id (long id)
{
	int i;

	for (i = 0; i <= top_of_p_table; i++)
		if (player_table[i].id == id)
			return (player_table[i].name);

	return (NULL);
}

#define NUM_OF_SAVE_THROWS	5

/* new load_char reads ascii pfiles */

/* Load a char, TRUE if loaded, FALSE if not */
int
load_char (char *name, struct char_data *ch)
{
	int id, num = 0, num2 = 0, num3 = 0, num4 = 0, num5 = 0, i;
	FBFILE *fl;
	char filename[40];
	char buf[128], line[MAX_INPUT_LENGTH + 1], tag[6];
	struct affected_type af;

	if ((id = find_name (name)) < 0)
		return (-1);
	else
	{
		sprintf (filename, "%s%s%c%s%s%s", PLR_PREFIX, SLASH,
				 *player_table[id].name, SLASH, player_table[id].name,
				 PLR_SUFFIX);
		if (!(fl = fbopen (filename, FB_READ)))
		{
			sprintf (buf, "SYSERR: Couldn't open player file %s", filename);
			mudlog (buf, NRM, LVL_GOD, TRUE);
			return (-1);
		}

		/* character initializations */
		/* initializations necessary to keep some things straight */
		ch->affected = NULL;
		for (i = 1; i <= MAX_SKILLS; i++)
			GET_SKILL (ch, i) = 0;
		GET_SEX (ch) = PFDEF_SEX;
		GET_CLASS (ch) = PFDEF_CLASS;
		GET_RACE (ch) = PFDEF_RACE;
		GET_LEVEL (ch) = PFDEF_LEVEL;
		GET_HOME (ch) = PFDEF_HOMETOWN;
		GET_HEIGHT (ch) = PFDEF_HEIGHT;
		GET_WEIGHT (ch) = PFDEF_WEIGHT;
		GET_ALIGNMENT (ch) = PFDEF_ALIGNMENT;
		PLR_FLAGS (ch) = PFDEF_PLRFLAGS;
		AFF_FLAGS (ch) = PFDEF_AFFFLAGS;
		for (i = 0; i < NUM_OF_SAVE_THROWS; i++)
			GET_SAVE (ch, i) = PFDEF_SAVETHROW;
		GET_LOADROOM (ch) = PFDEF_LOADROOM;
		GET_INVIS_LEV (ch) = PFDEF_INVISLEV;
		GET_FREEZE_LEV (ch) = PFDEF_FREEZELEV;
		GET_WIMP_LEV (ch) = PFDEF_WIMPLEV;
		GET_COND (ch, FULL) = PFDEF_HUNGER;
		GET_COND (ch, THIRST) = PFDEF_THIRST;
		GET_COND (ch, DRUNK) = PFDEF_DRUNK;
		GET_BAD_PWS (ch) = PFDEF_BADPWS;
		PRF_FLAGS (ch) = PFDEF_PREFFLAGS;
		GET_PRACTICES (ch) = PFDEF_PRACTICES;
		GET_GOLD (ch) = PFDEF_GOLD;
		GET_BANK_GOLD (ch) = PFDEF_BANK;
		GET_EXP (ch) = PFDEF_EXP;
		GET_HITROLL (ch) = PFDEF_HITROLL;
		GET_DAMROLL (ch) = PFDEF_DAMROLL;
		GET_AC (ch) = PFDEF_AC;
		ch->real_abils.str = PFDEF_STR;
		ch->real_abils.str_add = PFDEF_STRADD;
		ch->real_abils.dex = PFDEF_DEX;
		ch->real_abils.intel = PFDEF_INT;
		ch->real_abils.wis = PFDEF_WIS;
		ch->real_abils.con = PFDEF_CON;
		ch->real_abils.cha = PFDEF_CHA;
		GET_HIT (ch) = PFDEF_HIT;
		GET_MAX_HIT (ch) = PFDEF_MAXHIT;
		GET_MANA (ch) = PFDEF_MANA;
		GET_MAX_MANA (ch) = PFDEF_MAXMANA;
		GET_MOVE (ch) = PFDEF_MOVE;
		GET_MAX_MOVE (ch) = PFDEF_MAXMOVE;

		while (fbgetline (fl, line))
		{
			tag_argument (line, tag);
			num = atoi (line);

			switch (*tag)
			{
			case 'A':
				if (!strcmp (tag, "Ac  "))
					GET_AC (ch) = num;
				else if (!strcmp (tag, "Act "))
					PLR_FLAGS (ch) = num;
				else if (!strcmp (tag, "Aff "))
					AFF_FLAGS (ch) = asciiflag_conv (line);
				else if (!strcmp (tag, "Affs"))
				{
					i = 0;
					do
					{
						fbgetline (fl, line);
						sscanf (line, "%d %d %d %d %d", &num, &num2, &num3,
								&num4, &num5);
						if (num > 0)
						{
							af.type = num;
							af.duration = num2;
							af.modifier = num3;
							af.location = num4;
							af.bitvector = num5;
							affect_to_char (ch, &af);
							i++;
						}
					}
					while (num != 0);
				}
				else if (!strcmp (tag, "Alin"))
					GET_ALIGNMENT (ch) = num;
				break;

			case 'B':
				if (!strcmp (tag, "Badp"))
					GET_BAD_PWS (ch) = num;
				else if (!strcmp (tag, "Bank"))
					GET_BANK_GOLD (ch) = num;
				else if (!strcmp (tag, "Brth"))
					ch->player.time.birth = num;
				break;

			case 'C':
				if (!strcmp (tag, "Cha "))
					ch->real_abils.cha = num;
				else if (!strcmp (tag, "Clas"))
					GET_CLASS (ch) = num;
				else if (!strcmp (tag, "Con "))
					ch->real_abils.con = num;
				break;

			case 'D':
				if (!strcmp (tag, "Desc"))
				{
					ch->player.description = fbgetstring (fl);
				}
				else if (!strcmp (tag, "Dex "))
					ch->real_abils.dex = num;
				else if (!strcmp (tag, "Drnk"))
					GET_COND (ch, DRUNK) = num;
				else if (!strcmp (tag, "Drol"))
					GET_DAMROLL (ch) = num;
				break;

			case 'E':
				if (!strcmp (tag, "Exp "))
					GET_EXP (ch) = num;
				break;

			case 'F':
				if (!strcmp (tag, "Frez"))
					GET_FREEZE_LEV (ch) = num;
				break;

			case 'G':
				if (!strcmp (tag, "Gold"))
					GET_GOLD (ch) = num;
				break;

			case 'H':
				if (!strcmp (tag, "Hit "))
				{
					sscanf (line, "%d/%d", &num, &num2);
					GET_HIT (ch) = num;
					GET_MAX_HIT (ch) = num2;
				}
				else if (!strcmp (tag, "Hite"))
					GET_HEIGHT (ch) = num;
				else if (!strcmp (tag, "Home"))
					GET_HOME (ch) = num;
				else if (!strcmp (tag, "Host"))
					ch->player_specials->host = str_dup (line);
				else if (!strcmp (tag, "Hrol"))
					GET_HITROLL (ch) = num;
				else if (!strcmp (tag, "Hung"))
					GET_COND (ch, FULL) = num;
				break;

			case 'I':
				if (!strcmp (tag, "Id  "))
					GET_IDNUM (ch) = num;
				else if (!strcmp (tag, "Int "))
					ch->real_abils.intel = num;
				else if (!strcmp (tag, "Invs"))
					GET_INVIS_LEV (ch) = num;
				break;

			case 'L':
				if (!strcmp (tag, "Last"))
					ch->player.time.logon = num;
				else if (!strcmp (tag, "Lern"))
					GET_PRACTICES (ch) = num;
				else if (!strcmp (tag, "Levl"))
					GET_LEVEL (ch) = num;
				break;

			case 'M':
				if (!strcmp (tag, "Mana"))
				{
					sscanf (line, "%d/%d", &num, &num2);
					GET_MANA (ch) = num;
					GET_MAX_MANA (ch) = num2;
				}
				else if (!strcmp (tag, "Move"))
				{
					sscanf (line, "%d/%d", &num, &num2);
					GET_MOVE (ch) = num;
					GET_MAX_MOVE (ch) = num2;
				}
				break;

			case 'N':
				if (!strcmp (tag, "Name"))
					GET_PC_NAME (ch) = str_dup (line);
				break;

			case 'P':
				if (!strcmp (tag, "Pass"))
					strcpy (GET_PASSWD (ch), line);
				else if (!strcmp (tag, "Plyd"))
					ch->player.time.played = num;
#ifdef ASCII_SAVE_POOFS
				else if (!strcmp (tag, "PfIn"))
					POOFIN (ch) = str_dup (line);
				else if (!strcmp (tag, "PfOt"))
					POOFOUT (ch) = str_dup (line);
#endif
				else if (!strcmp (tag, "Pref"))
					PRF_FLAGS (ch) = asciiflag_conv (line);
				break;

			case 'R':
				if (!strcmp (tag, "Room"))
					GET_LOADROOM (ch) = num;
				else if (!strcmp (tag, "Razz"))
					GET_RACE (ch) = num;
				break;

			case 'S':
				if (!strcmp (tag, "Sex "))
					GET_SEX (ch) = num;
				else if (!strcmp (tag, "Skil"))
				{
					do
					{
						fbgetline (fl, line);
						sscanf (line, "%d %d", &num, &num2);
						if (num != 0)
							GET_SKILL (ch, num) = num2;
					}
					while (num != 0);
				}
				else if (!strcmp (tag, "Str "))
				{
					sscanf (line, "%d/%d", &num, &num2);
					ch->real_abils.str = num;
					ch->real_abils.str_add = num2;
				}
				break;

			case 'T':
				if (!strcmp (tag, "Thir"))
					GET_COND (ch, THIRST) = num;
				else if (!strcmp (tag, "Thr1"))
					GET_SAVE (ch, 0) = num;
				else if (!strcmp (tag, "Thr2"))
					GET_SAVE (ch, 1) = num;
				else if (!strcmp (tag, "Thr3"))
					GET_SAVE (ch, 2) = num;
				else if (!strcmp (tag, "Thr4"))
					GET_SAVE (ch, 3) = num;
				else if (!strcmp (tag, "Thr5"))
					GET_SAVE (ch, 4) = num;
				else if (!strcmp (tag, "Titl"))
					GET_TITLE (ch) = str_dup (line);
				break;

			case 'W':
				if (!strcmp (tag, "Wate"))
					GET_WEIGHT (ch) = num;
				else if (!strcmp (tag, "Wimp"))
					GET_WIMP_LEV (ch) = num;
				else if (!strcmp (tag, "Wis "))
					ch->real_abils.wis = num;
				break;

			default:
				sprintf (buf, "SYSERR: Unknown tag %s in pfile %s", tag,
						 name);
			}
		}
	}

	affect_total (ch);

	/* initialization for imms */
	if (GET_LEVEL (ch) >= LVL_IMMORT)
	{
		for (i = 1; i <= MAX_SKILLS; i++)
			GET_SKILL (ch, i) = 100;
		GET_COND (ch, FULL) = -1;
		GET_COND (ch, THIRST) = -1;
		GET_COND (ch, DRUNK) = -1;
	}
	fbclose (fl);
	return (id);
}


/* remove ^M's from file output */

/* There may be a similar function in Oasis (and I'm sure
   it's part of obuild).  Remove this if you get a
   multiple definition error or if it you want to use a
   substitute
*/
void
kill_ems (char *str)
{
	char *ptr1, *ptr2, *tmp;

	tmp = str;
	ptr1 = str;
	ptr2 = str;

	while (*ptr1)
	{
		if ((*(ptr2++) = *(ptr1++)) == '\r')
			if (*ptr1 == '\r')
				ptr1++;
	}
	*ptr2 = '\0';
}


/*
 * write the vital data of a player to the player file
 *
 * NOTE: load_room should be an *RNUM* now.  It is converted to a vnum here.
 */

/* This is the ascii pfiles save routine */
void
save_char (struct char_data *ch, room_rnum load_room)
{
	FBFILE *fl;
	char outname[40], bits[127], buf[MAX_STRING_LENGTH];
	int i, j, id, save_index = FALSE;
	struct affected_type *aff, tmp_aff[MAX_AFFECT];
	struct obj_data *char_eq[NUM_WEARS];

	if (IS_NPC (ch) || GET_PFILEPOS (ch) < 0)
		return;
	/* This version of save_char allows ch->desc to be null (to allow
	   "set file" to use it).  This causes the player's last host
	   and probably last login to null out.
	 */

	if (!PLR_FLAGGED (ch, PLR_LOADROOM))
	{
		if (load_room == NOWHERE)
			GET_LOADROOM (ch) = NOWHERE;
		else
			GET_LOADROOM (ch) = world[load_room].number;
	}

	/*strcpy(player.pwd, GET_PASSWD(ch)); */

	{
		for (i = 0; (*(bits + i) = LOWER (*(GET_NAME (ch) + i))); i++);
		sprintf (outname, "%s%s%c%s%s%s", PLR_PREFIX, SLASH, *bits,
				 SLASH, bits, PLR_SUFFIX);

		if (!(fl = fbopen (outname, FB_WRITE)))
		{
			sprintf (buf, "SYSERR: Couldn't open player file %s for write",
					 outname);
			mudlog (buf, NRM, LVL_GOD, TRUE);
			return;
		}

		/* remove affects from eq and spells (from char_to_store) */
		/* Unaffect everything a character can be affected by */

		for (i = 0; i < NUM_WEARS; i++)
		{
			if (GET_EQ (ch, i))
				char_eq[i] = unequip_char (ch, i);
			else
				char_eq[i] = NULL;
		}

		for (aff = ch->affected, i = 0; i < MAX_AFFECT; i++)
		{
			if (aff)
			{
				tmp_aff[i] = *aff;
				tmp_aff[i].next = 0;
				aff = aff->next;
			}
			else
			{
				tmp_aff[i].type = 0;	/* Zero signifies not used */
				tmp_aff[i].duration = 0;
				tmp_aff[i].modifier = 0;
				tmp_aff[i].location = 0;
				tmp_aff[i].bitvector = 0;
				tmp_aff[i].next = 0;
			}
		}

		/*
		 * remove the affections so that the raw values are stored; otherwise the
		 * effects are doubled when the char logs back in.
		 */

		while (ch->affected)
			affect_remove (ch, ch->affected);

		if ((i >= MAX_AFFECT) && aff && aff->next)
			log ("SYSERR: WARNING: OUT OF STORE ROOM FOR AFFECTED TYPES!!!");

		ch->aff_abils = ch->real_abils;

		/* end char_to_store code */

		if (GET_NAME (ch))
			fbprintf (fl, "Name: %s\n", GET_NAME (ch));
		if (GET_PASSWD (ch))
			fbprintf (fl, "Pass: %s\n", GET_PASSWD (ch));
		if (GET_TITLE (ch))
			fbprintf (fl, "Titl: %s\n", GET_TITLE (ch));
		if (ch->player.description && *ch->player.description)
		{
			strcpy (buf, ch->player.description);
			kill_ems (buf);
			fbprintf (fl, "Desc:\n%s~\n", buf);
		}
#ifdef ASCII_SAVE_POOFS
		if (POOFIN (ch))
			fbprintf (fl, "PfIn: %s\n", POOFIN (ch));
		if (POOFOUT (ch))
			fbprintf (fl, "PfOt: %s\n", POOFOUT (ch));
#endif
		if (GET_SEX (ch) != PFDEF_SEX)
			fbprintf (fl, "Sex : %d\n", GET_SEX (ch));
		if (GET_CLASS (ch) != PFDEF_CLASS)
			fbprintf (fl, "Clas: %d\n", GET_CLASS (ch));
		if (GET_RACE (ch) != PFDEF_RACE)
			fbprintf (fl, "Razz: %d\n", GET_RACE (ch));
		if (GET_LEVEL (ch) != PFDEF_LEVEL)
			fbprintf (fl, "Levl: %d\n", GET_LEVEL (ch));
		if (GET_HOME (ch) != PFDEF_HOMETOWN)
			fbprintf (fl, "Home: %d\n", GET_HOME (ch));
		fbprintf (fl, "Brth: %d\n", ch->player.time.birth);
		ch->player.time.played += (long) (time (0) - ch->player.time.logon);
		ch->player.time.logon = time (0);
		fbprintf (fl, "Plyd: %d\n", ch->player.time.played);
		fbprintf (fl, "Last: %d\n", ch->player.time.logon);
		if (ch->player_specials->host)
			fbprintf (fl, "Host: %s\n", ch->player_specials->host);
		if (GET_HEIGHT (ch) != PFDEF_HEIGHT)
			fbprintf (fl, "Hite: %d\n", GET_HEIGHT (ch));
		if (GET_WEIGHT (ch) != PFDEF_HEIGHT)
			fbprintf (fl, "Wate: %d\n", GET_WEIGHT (ch));

		if (GET_ALIGNMENT (ch) != PFDEF_ALIGNMENT)
			fbprintf (fl, "Alin: %d\n", GET_ALIGNMENT (ch));
		fbprintf (fl, "Id  : %d\n", GET_IDNUM (ch));
		if (PLR_FLAGS (ch) != PFDEF_PLRFLAGS)
			fbprintf (fl, "Act : %d\n", PLR_FLAGS (ch));
		if (AFF_FLAGS (ch) != PFDEF_AFFFLAGS)
		{
			sprintbits (AFF_FLAGS (ch), bits);
			fbprintf (fl, "Aff : %s\n", bits);
		}
		if (GET_SAVE (ch, 0) != PFDEF_SAVETHROW)
			fbprintf (fl, "Thr1: %d\n", GET_SAVE (ch, 0));
		if (GET_SAVE (ch, 1) != PFDEF_SAVETHROW)
			fbprintf (fl, "Thr2: %d\n", GET_SAVE (ch, 1));
		if (GET_SAVE (ch, 2) != PFDEF_SAVETHROW)
			fbprintf (fl, "Thr3: %d\n", GET_SAVE (ch, 2));
		if (GET_SAVE (ch, 3) != PFDEF_SAVETHROW)
			fbprintf (fl, "Thr4: %d\n", GET_SAVE (ch, 3));
		if (GET_SAVE (ch, 4) != PFDEF_SAVETHROW)
			fbprintf (fl, "Thr5: %d\n", GET_SAVE (ch, 4));

		if (GET_LEVEL (ch) < LVL_IMMORT)
		{
			fbprintf (fl, "Skil:\n");
			for (i = 1; i <= MAX_SKILLS; i++)
			{
				if (GET_SKILL (ch, i))
					fbprintf (fl, "%d %d\n", i, GET_SKILL (ch, i));
			}
			fbprintf (fl, "0 0\n");
		}
		if (GET_WIMP_LEV (ch) != PFDEF_WIMPLEV)
			fbprintf (fl, "Wimp: %d\n", GET_WIMP_LEV (ch));
		if (GET_FREEZE_LEV (ch) != PFDEF_FREEZELEV)
			fbprintf (fl, "Frez: %d\n", GET_FREEZE_LEV (ch));
		if (GET_INVIS_LEV (ch) != PFDEF_INVISLEV)
			fbprintf (fl, "Invs: %d\n", GET_INVIS_LEV (ch));
		if (GET_LOADROOM (ch) != PFDEF_LOADROOM)
			fbprintf (fl, "Room: %d\n", GET_LOADROOM (ch));
		if (PRF_FLAGS (ch) != PFDEF_PREFFLAGS)
		{
			sprintbits (PRF_FLAGS (ch), bits);
			fbprintf (fl, "Pref: %s\n", bits);
		}
		if (GET_BAD_PWS (ch) != PFDEF_BADPWS)
			fbprintf (fl, "Badp: %d\n", GET_BAD_PWS (ch));
		if (GET_COND (ch, FULL) != PFDEF_HUNGER
			&& GET_LEVEL (ch) < LVL_IMMORT)
			fbprintf (fl, "Hung: %d\n", GET_COND (ch, FULL));
		if (GET_COND (ch, THIRST) != PFDEF_THIRST
			&& GET_LEVEL (ch) < LVL_IMMORT)
			fbprintf (fl, "Thir: %d\n", GET_COND (ch, THIRST));
		if (GET_COND (ch, DRUNK) != PFDEF_DRUNK
			&& GET_LEVEL (ch) < LVL_IMMORT)
			fbprintf (fl, "Drnk: %d\n", GET_COND (ch, DRUNK));
		if (GET_PRACTICES (ch) != PFDEF_PRACTICES)
			fbprintf (fl, "Lern: %d\n", GET_PRACTICES (ch));

		if (GET_STR (ch) != PFDEF_STR || GET_ADD (ch) != PFDEF_STRADD)
			fbprintf (fl, "Str : %d/%d\n", GET_STR (ch), GET_ADD (ch));
		if (GET_INT (ch) != PFDEF_INT)
			fbprintf (fl, "Int : %d\n", GET_INT (ch));
		if (GET_WIS (ch) != PFDEF_WIS)
			fbprintf (fl, "Wis : %d\n", GET_WIS (ch));
		if (GET_DEX (ch) != PFDEF_DEX)
			fbprintf (fl, "Dex : %d\n", GET_DEX (ch));
		if (GET_CON (ch) != PFDEF_CON)
			fbprintf (fl, "Con : %d\n", GET_CON (ch));
		if (GET_CHA (ch) != PFDEF_CHA)
			fbprintf (fl, "Cha : %d\n", GET_CHA (ch));

		if (GET_HIT (ch) != PFDEF_HIT || GET_MAX_HIT (ch) != PFDEF_MAXHIT)
			fbprintf (fl, "Hit : %d/%d\n", GET_HIT (ch), GET_MAX_HIT (ch));
		if (GET_MANA (ch) != PFDEF_MANA || GET_MAX_MANA (ch) != PFDEF_MAXMANA)
			fbprintf (fl, "Mana: %d/%d\n", GET_MANA (ch), GET_MAX_MANA (ch));
		if (GET_MOVE (ch) != PFDEF_MOVE || GET_MAX_MOVE (ch) != PFDEF_MAXMOVE)
			fbprintf (fl, "Move: %d/%d\n", GET_MOVE (ch), GET_MAX_MOVE (ch));
		if (GET_AC (ch) != PFDEF_AC)
			fbprintf (fl, "Ac  : %d\n", GET_AC (ch));
		if (GET_GOLD (ch) != PFDEF_GOLD)
			fbprintf (fl, "Gold: %d\n", GET_GOLD (ch));
		if (GET_BANK_GOLD (ch) != PFDEF_BANK)
			fbprintf (fl, "Bank: %d\n", GET_BANK_GOLD (ch));
		if (GET_EXP (ch) != PFDEF_EXP)
			fbprintf (fl, "Exp : %d\n", GET_EXP (ch));
		if (GET_HITROLL (ch) != PFDEF_HITROLL)
			fbprintf (fl, "Hrol: %d\n", GET_HITROLL (ch));
		if (GET_DAMROLL (ch))
			fbprintf (fl, "Drol: %d\n", GET_DAMROLL (ch));

		/* affected_type */
		if (tmp_aff[0].type > 0)
		{
			fbprintf (fl, "Affs:\n");
			for (i = 0; i < MAX_AFFECT; i++)
			{
				aff = &tmp_aff[i];
				if (aff->type)
					fbprintf (fl, "%d %d %d %d %d\n", aff->type,
							  aff->duration, aff->modifier, aff->location,
							  (int) aff->bitvector);
			}
			fbprintf (fl, "0 0 0 0 0\n");
		}

		fbclose (fl);

		/* more char_to_store code to restore affects */

		/* add spell and eq affections back in now */
		for (i = 0; i < MAX_AFFECT; i++)
		{
			if (tmp_aff[i].type)
				affect_to_char (ch, &tmp_aff[i]);
		}

		for (i = 0; i < NUM_WEARS; i++)
		{
			if (char_eq[i])
				equip_char (ch, char_eq[i], i);
		}

		/* end char_to_store code */

		if ((id = find_name (GET_NAME (ch))) < 0)
			return;

		/* update the player in the player index */
		if (player_table[id].level != GET_LEVEL (ch))
		{
			save_index = TRUE;
			player_table[id].level = GET_LEVEL (ch);
		}
		if (player_table[id].last != ch->player.time.logon)
		{
			save_index = TRUE;
			player_table[id].last = ch->player.time.logon;
		}
		i = player_table[id].flags;
		if (PLR_FLAGGED (ch, PLR_DELETED))
			SET_BIT (player_table[id].flags, PINDEX_DELETED);
		else
			REMOVE_BIT (player_table[id].flags, PINDEX_DELETED);
		if (PLR_FLAGGED (ch, PLR_NODELETE) || PLR_FLAGGED (ch, PLR_CRYO))
			SET_BIT (player_table[id].flags, PINDEX_NODELETE);
		else
			REMOVE_BIT (player_table[id].flags, PINDEX_NODELETE);
		if (PLR_FLAGGED (ch, PLR_FROZEN) || PLR_FLAGGED (ch, PLR_NOWIZLIST)
			|| PLR_FLAGGED (ch, PLR_DELETED))
			j = TRUE;
		else
			j = FALSE;
		if (player_table[id].autowiz != j)
		{
			save_index = TRUE;
			player_table[id].autowiz = j;
		}
		save_player_index ();
	}
}




void
save_etext (struct char_data *ch)
{

/* this will be really cool soon */

}


/*
 * Create a new entry in the in-memory index table for the player file.
 * If the name already exists, by overwriting a deleted character, then
 * we re-use the old position.
 */
int
create_entry (char *name)
{
	int i, pos;

	if (top_of_p_table == -1)
	{							/* no table */
		CREATE (player_table, struct player_index_element, 1);
		pos = top_of_p_table = 0;
	}
	else if ((pos = get_ptable_by_name (name)) == -1)
	{							/* new name */
		i = ++top_of_p_table + 1;

		RECREATE (player_table, struct player_index_element, i);
		pos = top_of_p_table;
	}

	CREATE (player_table[pos].name, char, strlen (name) + 1);

	/* copy lowercase equivalent of name to table field */
	for (i = 0; (player_table[pos].name[i] = LOWER (name[i])); i++)
		/* Nothing */ ;

	return (pos);
}



/************************************************************************
*  funcs of a (more or less) general utility nature			*
************************************************************************/


/* read and allocate space for a '~'-terminated string from a given file */
char *
fread_string (FILE * fl, const char *error)
{
	char buf[MAX_STRING_LENGTH], tmp[512], *rslt;
	register char *point;
	int done = 0, length = 0, templength;

	*buf = '\0';

	do
	{
		if (!fgets (tmp, 512, fl))
		{
			log ("SYSERR: fread_string: format error at or near %s", error);
			exit (1);
		}
		/* If there is a '~', end the string; else put an "\r\n" over the '\n'. */
		if ((point = strchr (tmp, '~')) != NULL)
		{
			*point = '\0';
			done = 1;
		}
		else
		{
			point = tmp + strlen (tmp) - 1;
			*(point++) = '\r';
			*(point++) = '\n';
			*point = '\0';
		}

		templength = strlen (tmp);

		if (length + templength >= MAX_STRING_LENGTH)
		{
			log ("SYSERR: fread_string: string too large (db.c)");
			log (error);
			exit (1);
		}
		else
		{
			strcat (buf + length, tmp);
			length += templength;
		}
	}
	while (!done);

	/* allocate space for the new string and copy it */
	if (strlen (buf) > 0)
	{
		CREATE (rslt, char, length + 1);
		strcpy (rslt, buf);
	}
	else
		rslt = NULL;

	return (rslt);
}


/* release memory allocated for a char struct */
void
free_char (struct char_data *ch)
{
	int i;
	struct alias_data *a;

	if (ch->player_specials != NULL && ch->player_specials != &dummy_mob)
	{
		while ((a = GET_ALIASES (ch)) != NULL)
		{
			GET_ALIASES (ch) = (GET_ALIASES (ch))->next;
			free_alias (a);
		}
		if (ch->player_specials->poofin)
			free (ch->player_specials->poofin);
		if (ch->player_specials->poofout)
			free (ch->player_specials->poofout);
		if (ch->player_specials->host)
			free (ch->player_specials->host);
		free (ch->player_specials);
		if (IS_NPC (ch))
			log ("SYSERR: Mob %s (#%d) had player_specials allocated!",
				 GET_NAME (ch), GET_MOB_VNUM (ch));
	}
	if (!IS_NPC (ch) || (IS_NPC (ch) && GET_MOB_RNUM (ch) == -1))
	{
		/* if this is a player, or a non-prototyped non-player, free all */
		if (GET_NAME (ch))
			free (GET_NAME (ch));
		if (ch->player.title)
			free (ch->player.title);
		if (ch->player.short_descr)
			free (ch->player.short_descr);
		if (ch->player.long_descr)
			free (ch->player.long_descr);
		if (ch->player.description)
			free (ch->player.description);
	}
	else if ((i = GET_MOB_RNUM (ch)) >= 0)
	{
		/* otherwise, free strings only if the string is not pointing at proto */
		if (ch->player.name && ch->player.name != mob_proto[i].player.name)
			free (ch->player.name);
		if (ch->player.title && ch->player.title != mob_proto[i].player.title)
			free (ch->player.title);
		if (ch->player.short_descr
			&& ch->player.short_descr != mob_proto[i].player.short_descr)
			free (ch->player.short_descr);
		if (ch->player.long_descr
			&& ch->player.long_descr != mob_proto[i].player.long_descr)
			free (ch->player.long_descr);
		if (ch->player.description
			&& ch->player.description != mob_proto[i].player.description)
			free (ch->player.description);
	}
	while (ch->affected)
		affect_remove (ch, ch->affected);

	if (ch->desc)
		ch->desc->character = NULL;

	free (ch);
}




/* release memory allocated for an obj struct */
void
free_obj (struct obj_data *obj)
{
	int nr;
	struct extra_descr_data *thisd, *next_one;

	if ((nr = GET_OBJ_RNUM (obj)) == -1)
	{
		if (obj->name)
			free (obj->name);
		if (obj->description)
			free (obj->description);
		if (obj->short_description)
			free (obj->short_description);
		if (obj->action_description)
			free (obj->action_description);
		if (obj->ex_description)
			for (thisd = obj->ex_description; thisd; thisd = next_one)
			{
				next_one = thisd->next;
				if (thisd->keyword)
					free (thisd->keyword);
				if (thisd->description)
					free (thisd->description);
				free (thisd);
			}
	}
	else
	{
		if (obj->name && obj->name != obj_proto[nr].name)
			free (obj->name);
		if (obj->description && obj->description != obj_proto[nr].description)
			free (obj->description);
		if (obj->short_description
			&& obj->short_description != obj_proto[nr].short_description)
			free (obj->short_description);
		if (obj->action_description
			&& obj->action_description != obj_proto[nr].action_description)
			free (obj->action_description);
		if (obj->ex_description
			&& obj->ex_description != obj_proto[nr].ex_description)
			for (thisd = obj->ex_description; thisd; thisd = next_one)
			{
				next_one = thisd->next;
				if (thisd->keyword)
					free (thisd->keyword);
				if (thisd->description)
					free (thisd->description);
				free (thisd);
			}
	}

	free (obj);
}


/*
 * Steps:
 *   1: Make sure no one is using the pointer in paging.
 *   2: Read contents of a text file.
 *   3: Allocate space.
 *   4: Point 'buf' to it.
 *
 * We don't want to free() the string that someone may be
 * viewing in the pager.  page_string() keeps the internal
 * str_dup()'d copy on ->showstr_head and it won't care
 * if we delete the original.  Otherwise, strings are kept
 * on ->showstr_vector but we'll only match if the pointer
 * is to the string we're interested in and not a copy.
 */
int
file_to_string_alloc (const char *name, char **buf)
{
	char temp[MAX_STRING_LENGTH];
	struct descriptor_data *in_use;

	for (in_use = descriptor_list; in_use; in_use = in_use->next)
		if (in_use->showstr_vector && *in_use->showstr_vector == *buf)
			return (-1);

	/* Lets not free() what used to be there unless we succeeded. */
	if (file_to_string (name, temp) < 0)
		return (-1);

	if (*buf)
		free (*buf);

	*buf = str_dup (temp);
	return (0);
}


/* read contents of a text file, and place in buf */
int
file_to_string (const char *name, char *buf)
{
	FILE *fl;
	char tmp[READ_SIZE + 3];
	int len;

	*buf = '\0';

	if (!(fl = fopen (name, "r")))
	{
		log ("SYSERR: reading %s: %s", name, strerror (errno));
		return (-1);
	}

	for (;;)
	{
		if (!fgets (tmp, READ_SIZE, fl))	/* EOF check */
			break;
		if ((len = strlen (tmp)) > 0)
			tmp[len - 1] = '\0';	/* take off the trailing \n */
		strcat (tmp, "\r\n");

		if (strlen (buf) + strlen (tmp) + 1 > MAX_STRING_LENGTH)
		{
			log ("SYSERR: %s: string too big (%d max)", name,
				 MAX_STRING_LENGTH);
			*buf = '\0';
			fclose (fl);
			return (-1);
		}
		strcat (buf, tmp);
	}

	fclose (fl);

	return (0);
}



/* clear some of the the working variables of a char */
void
reset_char (struct char_data *ch)
{
	int i;

	for (i = 0; i < NUM_WEARS; i++)
		GET_EQ (ch, i) = NULL;

	ch->followers = NULL;
	ch->master = NULL;
	ch->in_room = NOWHERE;
	ch->carrying = NULL;
	ch->next = NULL;
	ch->next_fighting = NULL;
	ch->next_in_room = NULL;
	FIGHTING (ch) = NULL;
	ch->char_specials.position = POS_STANDING;
	ch->mob_specials.default_pos = POS_STANDING;
	ch->char_specials.carry_weight = 0;
	ch->char_specials.carry_items = 0;

	if (GET_HIT (ch) <= 0)
		GET_HIT (ch) = 1;
	if (GET_MOVE (ch) <= 0)
		GET_MOVE (ch) = 1;
	if (GET_MANA (ch) <= 0)
		GET_MANA (ch) = 1;

	GET_LAST_TELL (ch) = NOBODY;
}



/* clear ALL the working variables of a char; do NOT free any space alloc'ed */
void
clear_char (struct char_data *ch)
{
	memset ((char *) ch, 0, sizeof (struct char_data));

	ch->in_room = NOWHERE;
	GET_PFILEPOS (ch) = -1;
	GET_MOB_RNUM (ch) = NOBODY;
	GET_WAS_IN (ch) = NOWHERE;
	GET_POS (ch) = POS_STANDING;
	ch->mob_specials.default_pos = POS_STANDING;

	GET_AC (ch) = 100;			/* Basic Armor */
	if (ch->points.max_mana < 100)
		ch->points.max_mana = 100;
}


void
clear_object (struct obj_data *obj)
{
	memset ((char *) obj, 0, sizeof (struct obj_data));

	obj->item_number = NOTHING;
	obj->in_room = NOWHERE;
	obj->worn_on = NOWHERE;
}




/* initialize a new character only if class is set */
void
init_char (struct char_data *ch)
{
	int i;

	/* create a player_special structure */
	if (ch->player_specials == NULL)
		CREATE (ch->player_specials, struct player_special_data, 1);

	/* *** if this is our first player --- he be God *** */

	if (top_of_p_table == 0)
	{
		GET_EXP (ch) = 100000000;
		GET_LEVEL (ch) = LVL_IMPL;

		ch->points.max_hit = 500;
		ch->points.max_mana = 100;
		ch->points.max_move = 82;
	}
	set_title (ch, NULL);
	ch->player.short_descr = NULL;
	ch->player.long_descr = NULL;
	ch->player.description = NULL;

	ch->player.hometown = 1;

	ch->player.time.birth = time (0);
	ch->player.time.played = 0;
	ch->player.time.logon = time (0);
	for (i = 0; i < MAX_TONGUE; i++)
		GET_TALK (ch, i) = 0;

	/* make favors for sex */
	if (ch->player.sex == SEX_MALE)
	{
		ch->player.weight = number (120, 180);
		ch->player.height = number (160, 200);
	}
	else
	{
		ch->player.weight = number (100, 160);
		ch->player.height = number (150, 180);
	}

	ch->points.max_mana = 100;
	ch->points.mana = GET_MAX_MANA (ch);
	ch->points.hit = GET_MAX_HIT (ch);
	ch->points.max_move = 82;
	ch->points.move = GET_MAX_MOVE (ch);
	ch->points.armor = 100;

	if ((i = get_ptable_by_name (GET_NAME (ch))) != -1)
		player_table[i].id = GET_IDNUM (ch) = ++top_idnum;
	else
		log ("SYSERR: init_char: Character '%s' not found in player table.",
			 GET_NAME (ch));

	for (i = 1; i <= MAX_SKILLS; i++)
	{
		if (GET_LEVEL (ch) < LVL_IMPL)
			SET_SKILL (ch, i, 0);
		else
			SET_SKILL (ch, i, 100);
	}

	ch->char_specials.saved.affected_by = 0;

	for (i = 0; i < 5; i++)
		GET_SAVE (ch, i) = 0;

	ch->real_abils.intel = 25;
	ch->real_abils.wis = 25;
	ch->real_abils.dex = 25;
	ch->real_abils.str = 25;
	ch->real_abils.str_add = 100;
	ch->real_abils.con = 25;
	ch->real_abils.cha = 25;

	for (i = 0; i < 3; i++)
		GET_COND (ch, i) = (GET_LEVEL (ch) == LVL_IMPL ? -1 : 24);

	GET_LOADROOM (ch) = NOWHERE;

}



/* returns the real number of the room with given virtual number */
room_rnum real_room (room_vnum vnum)
{
	room_rnum bot, top, mid;

	bot = 0;
	top = top_of_world;

	/* perform binary search on world-table */
	for (;;)
	{
		mid = (bot + top) / 2;

		if ((world + mid)->number == vnum)
			return (mid);
		if (bot >= top)
			return (NOWHERE);
		if ((world + mid)->number > vnum)
			top = mid - 1;
		else
			bot = mid + 1;
	}
}



/* returns the real number of the monster with given virtual number */
mob_rnum real_mobile (mob_vnum vnum)
{
	mob_rnum bot, top, mid;

	bot = 0;
	top = top_of_mobt;

	/* perform binary search on mob-table */
	for (;;)
	{
		mid = (bot + top) / 2;

		if ((mob_index + mid)->vnum == vnum)
			return (mid);
		if (bot >= top)
			return (-1);
		if ((mob_index + mid)->vnum > vnum)
			top = mid - 1;
		else
			bot = mid + 1;
	}
}



/* returns the real number of the object with given virtual number */
obj_rnum real_object (obj_vnum vnum)
{
	obj_rnum bot, top, mid;

	bot = 0;
	top = top_of_objt;

	/* perform binary search on obj-table */
	for (;;)
	{
		mid = (bot + top) / 2;

		if ((obj_index + mid)->vnum == vnum)
			return (mid);
		if (bot >= top)
			return (-1);
		if ((obj_index + mid)->vnum > vnum)
			top = mid - 1;
		else
			bot = mid + 1;
	}
}

/*
 * Extend later to include more checks.
 *
 * TODO: Add checks for unknown bitvectors.
 */
int
check_object (struct obj_data *obj)
{
	int error = FALSE;

	if (GET_OBJ_WEIGHT (obj) < 0 && (error = TRUE))
		log ("SYSERR: Object #%d (%s) has negative weight (%d).",
			 GET_OBJ_VNUM (obj), obj->short_description,
			 GET_OBJ_WEIGHT (obj));

	if (GET_OBJ_RENT (obj) < 0 && (error = TRUE))
		log ("SYSERR: Object #%d (%s) has negative cost/day (%d).",
			 GET_OBJ_VNUM (obj), obj->short_description, GET_OBJ_RENT (obj));

	sprintbit (GET_OBJ_WEAR (obj), wear_bits, buf);
	if (strstr (buf, "UNDEFINED") && (error = TRUE))
		log ("SYSERR: Object #%d (%s) has unknown wear flags.",
			 GET_OBJ_VNUM (obj), obj->short_description);

	sprintbit (GET_OBJ_EXTRA (obj), extra_bits, buf);
	if (strstr (buf, "UNDEFINED") && (error = TRUE))
		log ("SYSERR: Object #%d (%s) has unknown extra flags.",
			 GET_OBJ_VNUM (obj), obj->short_description);

	sprintbit (obj->obj_flags.bitvector, affected_bits, buf);
	if (strstr (buf, "UNDEFINED") && (error = TRUE))
		log ("SYSERR: Object #%d (%s) has unknown affection flags.",
			 GET_OBJ_VNUM (obj), obj->short_description);

	switch (GET_OBJ_TYPE (obj))
	{
	case ITEM_DRINKCON:
		{
			char onealias[MAX_INPUT_LENGTH], *space =
				strrchr (obj->name, ' ');

			strcpy (onealias, space ? space + 1 : obj->name);
			if (search_block (onealias, drinknames, TRUE) < 0
				&& (error = TRUE))
				log
					("SYSERR: Object #%d (%s) doesn't have drink type as last alias. (%s)",
					 GET_OBJ_VNUM (obj), obj->short_description, obj->name);
		}
		/* Fall through. */
	case ITEM_FOUNTAIN:
		if (GET_OBJ_VAL (obj, 1) > GET_OBJ_VAL (obj, 0) && (error = TRUE))
			log
				("SYSERR: Object #%d (%s) contains (%d) more than maximum (%d).",
				 GET_OBJ_VNUM (obj), obj->short_description, GET_OBJ_VAL (obj,
																		  1),
				 GET_OBJ_VAL (obj, 0));
		break;
	case ITEM_SCROLL:
	case ITEM_POTION:
		error |= check_object_level (obj, 0);
		error |= check_object_spell_number (obj, 1);
		error |= check_object_spell_number (obj, 2);
		error |= check_object_spell_number (obj, 3);
		break;
	case ITEM_WAND:
	case ITEM_STAFF:
		error |= check_object_level (obj, 0);
		error |= check_object_spell_number (obj, 3);
		if (GET_OBJ_VAL (obj, 2) > GET_OBJ_VAL (obj, 1) && (error = TRUE))
			log
				("SYSERR: Object #%d (%s) has more charges (%d) than maximum (%d).",
				 GET_OBJ_VNUM (obj), obj->short_description, GET_OBJ_VAL (obj,
																		  2),
				 GET_OBJ_VAL (obj, 1));
		break;
	}

	return (error);
}

int
check_object_spell_number (struct obj_data *obj, int val)
{
	int error = FALSE;
	const char *spellname;

	if (GET_OBJ_VAL (obj, val) == -1)	/* i.e.: no spell */
		return (error);

	/*
	 * Check for negative spells, spells beyond the top define, and any
	 * spell which is actually a skill.
	 */
	if (GET_OBJ_VAL (obj, val) < 0)
		error = TRUE;
	if (GET_OBJ_VAL (obj, val) > TOP_SPELL_DEFINE)
		error = TRUE;
	if (GET_OBJ_VAL (obj, val) > MAX_SPELLS
		&& GET_OBJ_VAL (obj, val) <= MAX_SKILLS)
		error = TRUE;
	if (error)
		log ("SYSERR: Object #%d (%s) has out of range spell #%d.",
			 GET_OBJ_VNUM (obj), obj->short_description, GET_OBJ_VAL (obj,
																	  val));

	/*
	 * This bug has been fixed, but if you don't like the special behavior...
	 */
#if 0
	if (GET_OBJ_TYPE (obj) == ITEM_STAFF &&
		HAS_SPELL_ROUTINE (GET_OBJ_VAL (obj, val), MAG_AREAS | MAG_MASSES))
		log ("... '%s' (#%d) uses %s spell '%s'.",
			 obj->short_description, GET_OBJ_VNUM (obj),
			 HAS_SPELL_ROUTINE (GET_OBJ_VAL (obj, val),
								MAG_AREAS) ? "area" : "mass",
			 skill_name (GET_OBJ_VAL (obj, val)));
#endif

	if (scheck)					/* Spell names don't exist in syntax check mode. */
		return (error);

	/* Now check for unnamed spells. */
	spellname = skill_name (GET_OBJ_VAL (obj, val));

	if ((spellname == unused_spellname || !str_cmp ("UNDEFINED", spellname))
		&& (error = TRUE))
		log ("SYSERR: Object #%d (%s) uses '%s' spell #%d.",
			 GET_OBJ_VNUM (obj), obj->short_description, spellname,
			 GET_OBJ_VAL (obj, val));

	return (error);
}

int
check_object_level (struct obj_data *obj, int val)
{
	int error = FALSE;

	if ((GET_OBJ_VAL (obj, val) < 0 || GET_OBJ_VAL (obj, val) > LVL_IMPL)
		&& (error = TRUE))
		log ("SYSERR: Object #%d (%s) has out of range level #%d.",
			 GET_OBJ_VNUM (obj), obj->short_description, GET_OBJ_VAL (obj,
																	  val));

	return (error);
}


/* new functions used by ascii pfiles */

/* Separate a 4-character id tag from the data it precedes */
void
tag_argument (char *argument, char *tag)
{
	char *tmp = argument, *ttag = tag, *wrt = argument;
	int i;

	for (i = 0; i < 4; i++)
		*(ttag++) = *(tmp++);
	*ttag = '\0';

	while (*tmp == ':' || *tmp == ' ')
		tmp++;

	while (*tmp)
		*(wrt++) = *(tmp++);
	*wrt = '\0';
}


/* This function necessary to save a seperate ascii player index */
void
save_player_index (void)
{
	int i;
	char bits[64];
	FBFILE *index_file;

	if (!(index_file = fbopen (PLR_INDEX_FILE, FB_WRITE)))
	{
		log ("SYSERR:  Could not write player index file");
		return;
	}

	for (i = 0; i <= top_of_p_table; i++)
		if (*player_table[i].name)
		{
			sprintbits (player_table[i].flags, bits);
			fbprintf (index_file, "%ld %s %d %s %d %d\n", player_table[i].id,
					  player_table[i].name, player_table[i].level,
					  *bits ? bits : "0", player_table[i].last,
					  player_table[i].autowiz);
		}
	fbprintf (index_file, "~\n");

	fbclose (index_file);
}


/* This is a general purpose function originally from obuild OLC,
   and there is probably a similar function in Oasis.  Feel free to
   remove this and use another
*/

/*thanks to Luis Carvalho for sending this my way..it's probably a
  lot shorter than the one I would have made :)  */
void
sprintbits (long vektor, char *outstring)
{
	int i;
	char flags[53] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

	strcpy (outstring, "");
	for (i = 0; i < 32; i++)
	{
		if (vektor & 1)
		{
			*outstring = flags[i];
			outstring++;
		};
		vektor >>= 1;
	};
	*outstring = 0;
};


/* remove_player removes all files associated with a player who is
   self-deleted, deleted by an immortal, or deleted by the auto-wipe
   system (if enabled).  If you add more character files, you'll want
   to put a remover here.
*/
void
remove_player (int pfilepos)
{
	char pfile_name[128] /*, rent_name[128] */ ;

	if (!*player_table[pfilepos].name)
		return;

	unlink (pfile_name);

	/* Unlink any other player-owned files here if you have them  */

	sprintf (buf, "PCLEAN: %s Lev: %d Last: %s",
			 player_table[pfilepos].name, player_table[pfilepos].level,
			 asctime (localtime (&player_table[pfilepos].last)));
	log (buf);
	player_table[pfilepos].name[0] = '\0';
	save_player_index ();
}


void
clean_pfiles (void)
{
	int i, ci, timeout;

	for (i = 0; i <= top_of_p_table; i++)
	{
		if (IS_SET (player_table[i].flags, PINDEX_NODELETE))
			continue;
		timeout = -1;
		for (ci = 0; ci == 0 || (pclean_criteria[ci].level >
								 pclean_criteria[ci - 1].level); ci++)
		{
			if (
				(pclean_criteria[ci].level == -1
				 && IS_SET (player_table[i].flags, PINDEX_DELETED))
				|| player_table[i].level <= pclean_criteria[ci].level)
			{
				timeout = pclean_criteria[ci].days;
				break;
			}
		}
		if (timeout >= 0)
		{
			timeout *= SECS_PER_REAL_DAY;
			if ((time (0) - player_table[i].last) > timeout)
				remove_player (i);
		}
	}
}

/* end of ascii pfile added functions */
