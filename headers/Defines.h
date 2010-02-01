#ifndef DEFINES_H
#define DEFINES_H

#include "../AIExport.h"

#define MAX_INT   2147483647
#define MAX_FLOAT float(MAX_INT)
#define EPSILON   1/MAX_FLOAT

#define ERRORVECTOR float3(-1.0f,0.0f,0.0f)
#define ZEROVECTOR  float3(0.0f,0.0f,0.0f)


/* AI meta data */
#define AI_VERSION_NR  aiexport_getVersion()
#define AI_NAME        std::string("E323AI")
#define AI_VERSION     AI_NAME + AI_VERSION_NR + " - High Templar"
#define AI_CREDITS     "Error323 - folkerthuizinga@gmail.com"
#define AI_NOTES       "This A.I. mainly focusses on the XTA and BA mods"

/* Folders */
#define LOG_FOLDER   "logs/"
#define CFG_FOLDER   "configs/"
#define CACHE_FOLDER "cache/"

/* Templates */
#define CONFIG_TEMPLATE "template-config.cfg"

/* Misc macro's */
#define UD(u) (ai->cb->GetUnitDef(u))
#define UT(u) (&(ai->unittable->units[u]))
#define ID(x,z) ((z)*X+(x))
#define ID_GRAPH(x,z) ((z)*XX+(x))

/* Metal to Energy ratio */
#define METAL2ENERGY 60

/* Max features */
#define MAX_FEATURES 10

/* Max enemies */
#define MAX_ENEMIES 20

/* Max enemy units */
#define MAX_UNITS_AI 500

#define MAX_UNITS_MILITARY 50

/* Map ratios */
#define HEIGHT2REAL 8
#define I_MAP_RES 8 /* Inverse map resolution (must be even) */
#define HEIGHT2SLOPE 2

/* Max factory Assisters */
#define FACTORY_ASSISTERS 6

/* Number of multiplex iterations */
#define MULTIPLEXER 10 

/* Draw time */
#define DRAW_TIME MULTIPLEXER*30


/* Stats url */
#define UPLOAD_URL "http://fhuizing.pythonic.nl/ai-stats.php"

/* We gonna use math.h constants */
#define _USE_MATH_DEFINES

/* Unit categories */
enum unitCategory {
	TECH1        = (1<<0),
	TECH2        = (1<<1),
	TECH3        = (1<<2),

	AIR          = (1<<3),
	SEA          = (1<<4),
	LAND         = (1<<5),
	STATIC       = (1<<6),
	MOBILE       = (1<<7),

	FACTORY      = (1<<8),
	BUILDER      = (1<<9),
	ASSISTER     = (1<<10),
	RESURRECTOR  = (1<<11),

	COMMANDER    = (1<<12),
	ATTACKER     = (1<<13),
	ANTIAIR      = (1<<14),
	SCOUTER      = (1<<15),
	ARTILLERY    = (1<<16),
	SNIPER       = (1<<17),
	ASSAULT      = (1<<18),

	MEXTRACTOR   = (1<<19),
	MMAKER       = (1<<20),
	EMAKER       = (1<<21),
	MSTORAGE     = (1<<22),
	ESTORAGE     = (1<<23),
	WIND         = (1<<24),
	TIDAL        = (1<<25),

	KBOT         = (1<<26),
	VEHICLE      = (1<<27),
	HOVER        = (1<<28),

	DEFENSE      = (1<<29)
};

/* Build priorities */
enum buildPriority {
	LOW    = 0,
	NORMAL = 1,
	HIGH   = 2
};

/* Build tasks */
enum buildType {
	BUILD_MPROVIDER,
	BUILD_EPROVIDER,
	BUILD_AA_DEFENSE,
	BUILD_AG_DEFENSE,
	BUILD_FACTORY,
	BUILD_MSTORAGE,
	BUILD_ESTORAGE
};

#endif
