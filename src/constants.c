
/* ************************************************************************
*   File: constants.c                                   Part of CircleMUD *
*  Usage: Numeric and string contants used by the MUD                     *
*                                                                         *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  Copyright (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */

#include "conf.h"
#include "sysdep.h"

#include "structs.h"
#include "interpreter.h"		/* alias_data */

cpp_extern const char *circlemud_version =
	"CircleMUD, version 3.00 beta patchlevel 18";

/* strings corresponding to ordinals/bitvectors in structs.h ***********/


/* (Note: strings for class definitions in class.c instead of here) */


/* cardinal directions */
const char *dirs[] = {
	"north",
	"east",
	"south",
	"west",
	"up",
	"down",
	"\n"
};

const char *it_dirs[] = {
	"nord",
	"est",
	"sud",
	"ovest",
	"su",
	"giu'",
	"\n"
};

int trap_dir[] = {
	TRAP_EFF_NORTH,
	TRAP_EFF_EAST,
	TRAP_EFF_SOUTH,
	TRAP_EFF_WEST,
	TRAP_EFF_UP,
	TRAP_EFF_DOWN
};



/* ROOM_x */
const char *room_bits[] = {
	"DARK",
	"DEATH",
	"NO_MOB",
	"INDOORS",
	"PEACEFUL",
	"SOUNDPROOF",
	"NO_TRACK",
	"NO_MAGIC",
	"TUNNEL",
	"PRIVATE",
	"GODROOM",
	"HOUSE",
	"HCRSH",
	"ATRIUM",
	"OLC",
	"*",						/* BFS MARK */
	"\n"
};


/* EX_x */
const char *exit_bits[] = {
	"DOOR",
	"CLOSED",
	"LOCKED",
	"PICKPROOF",
	"\n"
};


/* SECT_ */
const char *sector_types[] = {
	"Inside",
	"City",
	"Field",
	"Forest",
	"Hills",
	"Mountains",
	"Water (Swim)",
	"Water (No Swim)",
	"In Flight",
	"Underwater",
	"\n"
};


/*
 * SEX_x
 * Not used in sprinttype() so no \n.
 */
const char *genders[] = {
	"neutral",
	"male",
	"female",
	"\n"
};


/* POS_x */
const char *position_types[] = {
	"Dead",
	"Mortally wounded",
	"Incapacitated",
	"Stunned",
	"Sleeping",
	"Resting",
	"Sitting",
	"Fighting",
	"Standing",
	"Flaying",
	"\n"
};


/* PLR_x */
const char *player_bits[] = {
	"KILLER",
	"THIEF",
	"FROZEN",
	"DONTSET",
	"WRITING",
	"MAILING",
	"CSH",
	"SITEOK",
	"NOSHOUT",
	"NOTITLE",
	"DELETED",
	"LOADRM",
	"NO_WIZL",
	"NO_DEL",
	"INVST",
	"CRYO",
	"\n"
};


/* MOB_x */
const char *action_bits[] = {
	"SPEC",
	"SENTINEL",
	"SCAVENGER",
	"ISNPC",
	"AWARE",
	"AGGR",
	"STAY-ZONE",
	"WIMPY",
	"AGGR_EVIL",
	"AGGR_GOOD",
	"AGGR_NEUTRAL",
	"MEMORY",
	"HELPER",
	"NO_CHARM",
	"NO_SUMMN",
	"NO_SLEEP",
	"NO_BASH",
	"NO_BLIND",
	"\n"
};


/* PRF_x */
const char *preference_bits[] = {
	"BRIEF",
	"COMPACT",
	"DEAF",
	"NO_TELL",
	"D_HP",
	"D_MANA",
	"D_MOVE",
	"AUTOEX",
	"NO_HASS",
	"QUEST",
	"SUMN",
	"NO_REP",
	"LIGHT",
	"C1",
	"C2",
	"NO_WIZ",
	"L1",
	"L2",
	"NO_AUC",
	"NO_GOS",
	"NO_GTZ",
	"RMFLG",
	"\n"
};


/* AFF_x */
const char *affected_bits[] = {
	"BLIND",
	"INVIS",
	"DET-ALIGN",
	"DET-INVIS",
	"DET-MAGIC",
	"SENSE-LIFE",
	"WATWALK",
	"SANCT",
	"GROUP",
	"CURSE",
	"INFRA",
	"POISON",
	"PROT-EVIL",
	"PROT-GOOD",
	"SLEEP",
	"NO_TRACK",
	"UNUSED",
	"UNUSED",
	"SNEAK",
	"HIDE",
	"REGEN_HP",
	"CHARM",
    "FLY",
    "REGEN_RAM",
	"\n"
};


/* CON_x */
const char *connected_types[] = {
	"Playing",
	"Disconnecting",
	"Get name",
	"Confirm name",
	"Get password",
	"Get new PW",
	"Confirm new PW",
	"Select sex",
	"Select class",
	"Reading MOTD",
	"Main Menu",
	"Get descript.",
	"Changing PW 1",
	"Changing PW 2",
	"Changing PW 3",
	"Self-Delete 1",
	"Self-Delete 2",
	"Disconnecting",
	"Select race",
	"\n"
};


/*
 * WEAR_x - for eq list
 * Not use in sprinttype() so no \n.
 */
const char *where[] = {
	"<come luce>          ",
	"<al dito>            ",
	"<al dito>            ",
	"<intorno al collo>   ",
	"<intorno al collo>   ",
	"<sul corpo>          ",
	"<in testa>           ",
	"<sulle gambe>        ",
	"<ai piedi>           ",
	"<sulle mani>         ",
	"<sulle braccia>      ",
	"<come scudo>         ",
	"<intorno al corpo>   ",
	"<intorno alla vita>  ",
	"<al polso>           ",
	"<al polso>           ",
	"<impugnato>          ",
	"<tenuto>             ",
	"<sulla schiena>      ",
	"<sulla spalla>       ",
	"<sulla spalla>       ",
	"<sull'orecchio>      ",
	"<sull'orecchio>      ",
	"<sugli occhi>        "
};


/* WEAR_x - for stat */
const char *equipment_types[] = {
	"Usato come luce",
	"Indossato al dito destro",
	"Indossato al dito sinistro",
	"Indossato sopra intorno al collo",
	"Indossato sotto intorno al collo",
	"Indossato sul corpo",
	"Indossato in testa",
	"Indossato sulle gambe",
	"Indossato sui piedi",
	"Indossato sulle mani",
	"Indossato sulle braccia",
	"Indossato come scudo",
	"Indossato intorno al corpo",
	"Indossato alla vita",
	"Indossato intorno al polso destro",
	"Indossato intorno al polso sinistro",
	"Impugnato",
	"Tenuto",
	"Indossato sulla schiena",
	"Indossato alla spalla destra",
	"Indossato alla spalla sinistra",
	"Indossato sull'orecchio destro",
	"Indossato sull'orecchio sinistro",
	"Indossato sugli occhi",
	"\n"
};


/* ITEM_x (ordinal object types) */
const char *item_types[] = {
	"UNDEFINED",
	"LIGHT",
	"SCROLL",
	"WAND",
	"STAFF",
	"WEAPON",
	"FIRE WEAPON",
	"MISSILE",
	"TREASURE",
	"ARMOR",
	"POTION",
	"WORN",
	"OTHER",
	"TRASH",
	"TRAP",
	"CONTAINER",
	"NOTE",
	"LIQ CONTAINER",
	"KEY",
	"FOOD",
	"MONEY",
	"PEN",
	"BOAT",
	"FOUNTAIN",
	"THROW",
	"GRENADE",
	"PORTAL",
	"\n"
};


/* ITEM_WEAR_ (wear bitvector) */
const char *wear_bits[] = {
	"TAKE",
	"FINGER",
	"NECK",
	"BODY",
	"HEAD",
	"LEGS",
	"FEET",
	"HANDS",
	"ARMS",
	"SHIELD",
	"ABOUT",
	"WAIST",
	"WRIST",
	"WIELD",
	"HOLD",
	"BACK",
	"SHOULDERS",
	"EARS",
	"EYES",
	"\n"
};


/* ITEM_x (extra bits) */
const char *extra_bits[] = {
	"GLOW",
	"HUM",
	"NO_RENT",
	"NO_DONATE",
	"NO_INVIS",
	"INVISIBLE",
	"MAGIC",
	"NO_DROP",
	"BLESS",
	"ANTI_GOOD",
	"ANTI_EVIL",
	"ANTI_NEUTRAL",
	"ANTI_VIRUSWRITER",
	"ANTI_SYSADMIN",
	"ANTI_HACKER",
	"ANTI_WARRIOR",
	"NO_SELL",
	"LIVE_GRENADE",
	"ANTI_HUMAN",
	"ANTI_CYBORG",
	"ANTI_MOTORBIKER",
	"ANTI_ALIEN",
	"ANTI_MARTIAL",
	"ANTI_LINKER",
	"ANTI_PSIONIC",
	"AUTOMATIC_WEAPON",
	"\n"
};

/* Ranged weapons */
const char *shot_types[] = {
	"un missile",
	"una pallottola",
	"una freccia",
	"un dardo",
	"una rosa di pallini",
	"un razzo",
	"una granata",
	"una pallottola",
	"una pallottola",
	"una pallottola",
	"una pallottola",
	"una pallottola",
	"\n"
};

const char *types_ammo[][2] = {
	{"missile", "missili"},
	{"pallottola", "pallottole"},
	{"freccia", "frecce"},
	{"dardo", "dardi"},
	{"cartuccia", "cartucce"},
	{"razzo", "razzi"},
	{"granata", "granate"},
	{"pallottola", "pallottole"},
	{"pallottola", "pallottole"},
	{"pallottola", "pallottole"},
	{"pallottola", "pallottole"},
	{"pallottola", "pallottole"},
	{"\n", "\n"}
};

/* La prima cifra indica il numero di dadi, la seconda le facce */
const int shot_damage[][2] = {
	{1, 4},						/* unused */
	{1, 8},						/* Pallottole cal 22 */
	{1, 10},					/* frecce */
	{2, 10},					/* dardi */
	{2, 8},						/* shotgun */
	{6, 12},					/* razzi */
	{5, 12},					/* granate 40 mm */
	{2, 6},						/* pallottola 7.62 mm NATO */
	{2, 5},						/* pallottole 5.56 mm NATO */
	{2, 7},						/* pallottole 7.62 mm AK */
	{2, 5},						/* pallottole 5.42 mm AKM */
	{3, 4},						/* pallottole 9 mm Parabellum */
	{0, 0}						/* don't remove */
};


/* APPLY_x */
const char *apply_types[] = {
	"NONE",
	"STR",
	"DEX",
	"INT",
	"WIS",
	"CON",
	"CHA",
	"CLASS",
	"LEVEL",
	"AGE",
	"CHAR_WEIGHT",
	"CHAR_HEIGHT",
	"MAXRAM",
	"MAXHIT",
	"MAXMOVE",
	"GOLD",
	"EXP",
	"ARMOR",
	"HITROLL",
	"DAMROLL",
	"SAVING_PARA",
	"SAVING_ROD",
	"SAVING_PETRI",
	"SAVING_BREATH",
	"SAVING_SPELL",
	"RACE",
	"\n"
};


/* CONT_x */
const char *container_bits[] = {
	"CLOSEABLE",
	"PICKPROOF",
	"CLOSED",
	"LOCKED",
	"\n",
};



/* LIQ_x */
const char *drinks[] = {
	"water",
	"beer",
	"wine",
	"ale",
	"dark ale",
	"whisky",
	"lemonade",
	"firebreather",
	"local speciality",
	"slime mold juice",
	"milk",
	"tea",
	"coffee",
	"blood",
	"salt water",
	"clear water",
	"acqua",
	"birra",
	"vino",
	"limonata",
	"the",
	"caffe'",
	"aperitivo",
	"\n"
};


/* other constants for liquids ******************************************/


/* one-word alias for each drink */
const char *drinknames[] = {
	"water",
	"beer",
	"wine",
	"ale",
	"ale",
	"whisky",
	"lemonade",
	"firebreather",
	"local",
	"juice",
	"milk",
	"tea",
	"coffee",
	"blood",
	"salt",
	"water",
	"acqua",
	"birra",
	"vino",
	"limonata",
	"the",
	"caffe'",
	"aperitivo",
	"\n"
};


/* effect of drinks on hunger, thirst, and drunkenness -- see values.doc */
int drink_aff[][3] = {
	{0, 1, 10},
	{3, 2, 5},
	{5, 2, 5},
	{2, 2, 5},
	{1, 2, 5},
	{6, 1, 4},
	{0, 1, 8},
	{10, 0, 0},
	{3, 3, 3},
	{0, 4, -8},
	{0, 3, 6},
	{0, 1, 6},
	{0, 1, 6},
	{0, 2, -1},
	{0, 1, -2},
	{0, 0, 13},
	{0, 1, 10},
	{3, 2, 5},
	{5, 2, 5},
	{0, 1, 8},
	{0, 1, 6},
	{0, 1, 6},
	{0, 1, 6}
};


/* color of the various drinks */
const char *color_liquid[] = {
	"clear",
	"brown",
	"clear",
	"brown",
	"dark",
	"golden",
	"red",
	"green",
	"clear",
	"light green",
	"white",
	"brown",
	"black",
	"red",
	"clear",
	"crystal clear",
	"chiaro",
	"dorato",
	"rosso",
	"giallo",
	"marrone",
	"nero",
	"blu",
	"\n"
};


/*
 * level of fullness for drink containers
 * Not used in sprinttype() so no \n.
 */
const char *fullness[] = {
	"quasi vuoto' ",
	"pieno a meta' ",
	"pieno piu' della meta' ",
	"pieno"
};


/* str, int, wis, dex, con applies **************************************/


/* [ch] strength apply (all) */
cpp_extern const struct str_app_type str_app[] = {
	{-5, -4, 0, 0},				/* str = 0 */
	{-5, -4, 3, 1},				/* str = 1 */
	{-3, -2, 3, 2},
	{-3, -1, 10, 3},
	{-2, -1, 25, 4},
	{-2, -1, 55, 5},			/* str = 5 */
	{-1, 0, 80, 6},
	{-1, 0, 90, 7},
	{0, 0, 100, 8},
	{0, 0, 100, 9},
	{0, 0, 115, 10},			/* str = 10 */
	{0, 0, 115, 11},
	{0, 0, 140, 12},
	{0, 0, 140, 13},
	{0, 0, 170, 14},
	{0, 0, 170, 15},			/* str = 15 */
	{0, 1, 195, 16},
	{1, 1, 220, 18},
	{1, 2, 255, 20},			/* str = 18 */
	{3, 7, 640, 40},
	{3, 8, 700, 40},			/* str = 20 */
	{4, 9, 810, 40},
	{4, 10, 970, 40},
	{5, 11, 1130, 40},
	{6, 12, 1440, 40},
	{7, 14, 1750, 40},			/* str = 25 */
	{1, 3, 280, 22},			/* str = 18/0 - 18-50 */
	{2, 3, 305, 24},			/* str = 18/51 - 18-75 */
	{2, 4, 330, 26},			/* str = 18/76 - 18-90 */
	{2, 5, 380, 28},			/* str = 18/91 - 18-99 */
	{3, 6, 480, 30}				/* str = 18/100 */
};



/* [dex] skill apply (thieves only) */
cpp_extern const struct dex_skill_type dex_app_skill[] = {
	{-99, -99, -90, -99, -60},	/* dex = 0 */
	{-90, -90, -60, -90, -50},	/* dex = 1 */
	{-80, -80, -40, -80, -45},
	{-70, -70, -30, -70, -40},
	{-60, -60, -30, -60, -35},
	{-50, -50, -20, -50, -30},	/* dex = 5 */
	{-40, -40, -20, -40, -25},
	{-30, -30, -15, -30, -20},
	{-20, -20, -15, -20, -15},
	{-15, -10, -10, -20, -10},
	{-10, -5, -10, -15, -5},	/* dex = 10 */
	{-5, 0, -5, -10, 0},
	{0, 0, 0, -5, 0},
	{0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0},			/* dex = 15 */
	{0, 5, 0, 0, 0},
	{5, 10, 0, 5, 5},
	{10, 15, 5, 10, 10},		/* dex = 18 */
	{15, 20, 10, 15, 15},
	{15, 20, 10, 15, 15},		/* dex = 20 */
	{20, 25, 10, 15, 20},
	{20, 25, 15, 20, 20},
	{25, 25, 15, 20, 20},
	{25, 30, 15, 25, 25},
	{25, 30, 15, 25, 25}		/* dex = 25 */
};



/* [dex] apply (all) */
cpp_extern const struct dex_app_type dex_app[] = {
	{-7, -7, 6},				/* dex = 0 */
	{-6, -6, 5},				/* dex = 1 */
	{-4, -4, 5},
	{-3, -3, 4},
	{-2, -2, 3},
	{-1, -1, 2},				/* dex = 5 */
	{0, 0, 1},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},					/* dex = 10 */
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, -1},					/* dex = 15 */
	{1, 1, -2},
	{2, 2, -3},
	{2, 2, -4},					/* dex = 18 */
	{3, 3, -4},
	{3, 3, -4},					/* dex = 20 */
	{4, 4, -5},
	{4, 4, -5},
	{4, 4, -5},
	{5, 5, -6},
	{5, 5, -6}					/* dex = 25 */
};



/* [con] apply (all) */
cpp_extern const struct con_app_type con_app[] = {
	{-4, 20},					/* con = 0 */
	{-3, 25},					/* con = 1 */
	{-2, 30},
	{-2, 35},
	{-1, 40},
	{-1, 45},					/* con = 5 */
	{-1, 50},
	{0, 55},
	{0, 60},
	{0, 65},
	{0, 70},					/* con = 10 */
	{0, 75},
	{0, 80},
	{0, 85},
	{0, 88},
	{1, 90},					/* con = 15 */
	{2, 95},
	{2, 97},
	{3, 99},					/* con = 18 */
	{3, 99},
	{4, 99},					/* con = 20 */
	{5, 99},
	{5, 99},
	{5, 99},
	{6, 99},
	{6, 99}						/* con = 25 */
};



/* [int] apply (all) */
cpp_extern const struct int_app_type int_app[] = {
	{3},						/* int = 0 */
	{5},						/* int = 1 */
	{7},
	{8},
	{9},
	{10},						/* int = 5 */
	{11},
	{12},
	{13},
	{15},
	{17},						/* int = 10 */
	{19},
	{22},
	{25},
	{30},
	{35},						/* int = 15 */
	{40},
	{45},
	{50},						/* int = 18 */
	{53},
	{55},						/* int = 20 */
	{56},
	{57},
	{58},
	{59},
	{60}						/* int = 25 */
};


/* [wis] apply (all) */
cpp_extern const struct wis_app_type wis_app[] = {
	{0},						/* wis = 0 */
	{0},						/* wis = 1 */
	{0},
	{0},
	{0},
	{0},						/* wis = 5 */
	{0},
	{0},
	{0},
	{0},
	{0},						/* wis = 10 */
	{0},
	{2},
	{2},
	{3},
	{3},						/* wis = 15 */
	{3},
	{4},
	{5},						/* wis = 18 */
	{6},
	{6},						/* wis = 20 */
	{6},
	{6},
	{7},
	{7},
	{7}							/* wis = 25 */
};


const char *npc_class_types[] = {
    "Virus Writer",
	"Amministratore di Rete",
	"Hacker",
	"Guerrafondaio",
	"Martial Artist",
	"Linker",
	"Psionico",
	"Non-morto",
    "Umanoide",
    "Animale",
    "Dragone",
    "Gigante",
	"\n"
};


int rev_dir[] = {
	2,
	3,
	0,
	1,
	5,
	4
};


int movement_loss[] = {
	1,							/* Inside     */
	1,							/* City       */
	2,							/* Field      */
	3,							/* Forest     */
	4,							/* Hills      */
	6,							/* Mountains  */
	4,							/* Swimming   */
	1,							/* Unswimable */
	1,							/* Flying     */
	5							/* Underwater */
};


/* Not used in sprinttype(). */
const char *weekdays[] = {
	"Giorno del Clock",        
	"Giorno del Grande Swap",
	"Giorno del Freeze",
	"Giorno delle Subroutine",
	"Giorno del Kernel",
	"Giorno dell'Overclock ",
	"Giorno del Backup"
};


/* Not used in sprinttype(). */
const char *month_name[] = {
	"Mese del Boot",		/* 0 */    
	"Mese del Recursive Tunelling",
	"Mese del BlackOut",
	"Mese dell'Intrusione",
	"Mese della Rivolta dei Linkers",
	"Mese dell'Hijacking",
	"Mese della Preparazione",
	"Mese dell'Attacco al Sistema",
	"Mese dei Flood",
	"Mese del Ping Of Death",
	"Mese del Denial of Service",
	"Mese del Virus",
	"Mese del Sysop",
	"Mese della Ricompilazione del Codice",
	"Mese del Tuning del Kernel",
	"Mese del Debugging",
	"Mese del Grande Reboot"
};