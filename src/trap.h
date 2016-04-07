
/***************************************************************************
 *   File: trap.h                                    Part of CircleMUD     *
 *  Usage: Trap systems                                                    *
 *                                                                         *
 *  All rights reserved.  See license.doc for complete information.        *
 *                                                                         *
 *  (C) 2001 - Sidewinder                                                  *
 *                                                                         *
 *  Copyright (C) 1993, 94 by the Trustees of the Johns Hopkins University *
 *  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
 ***************************************************************************/

#define TRAP_EFFECT_TYPE   0
#define TRAP_DAM_TYPE      1
#define TRAP_LEVEL         2
#define TRAP_CHARGES       3

/*
   trap damage types...
*/

#define TRAP_DAM_POISON    -4
#define TRAP_DAM_SLEEP     -3
#define TRAP_DAM_TELEPORT  -2
#define TRAP_DAM_FIRE      SPELL_FIREBALL
#define TRAP_DAM_COLD      SPELL_FROST_BREATH
#define TRAP_DAM_ENERGY    SPELL_LIGHTNING_BOLT
#define TRAP_DAM_BLUNT     TYPE_BLUDGEON
#define TRAP_DAM_PIERCE    TYPE_PIERCE
#define TRAP_DAM_SLASH     TYPE_SLASH


#define GET_TRAP_LEV(obj) (obj)->obj_flags.value[TRAP_LEVEL]
#define GET_TRAP_EFF(obj) (obj)->obj_flags.value[TRAP_EFFECT_TYPE]
#define GET_TRAP_CHARGES(obj) (obj)->obj_flags.value[TRAP_CHARGES]
#define GET_TRAP_DAM_TYPE(obj) (obj)->obj_flags.value[TRAP_DAM_TYPE]
