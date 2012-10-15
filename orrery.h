#define FALSE (0)

#define N_SOLAR_SYSTEM_OBJECTS (11)

#define SUN     (0)
#define MERCURY (1)
#define VENUS   (2)
#define EARTH   (3)
#define MOON    (4)
#define MARS    (5)
#define JUPITER (6)
#define SATURN  (7)
#define URANUS  (8)
#define NEPTUNE (9)
#define COMET  (10)

#define DEGREES_TO_RADIANS (M_PI/180.0)

typedef struct ephemEntry { /* Holds one line of data from JPL Horizons Ephemeris */
  double tJD;      /* Time for which data is calculated */
  double rA;       /* Geocentric RA                     */
  double dec;      /* Geocentric Dec                    */
  double eLong;    /* Heliocentric ecliptic longitude   */
  double eLat;     /* Heliocentric ecliptic latitude    */
  double radius;   /* Distance from the sun in AU       */
  double mag;      /* Total visual magnitude            */
  struct ephemEntry *next; /* points to next entry      */
} ephemEntry;

typedef struct cometEphem {
  char *name;        /* Holds the JPL Horizons name for the comet    */
  char *nickName;    /* Holds the polular name of the comet          */
  int valid;         /* True if the associated Ephem. entries are OK */
  double firstTJD;   /* JD of first Ephem. entry                     */
  double lastTJD;    /* JD of the last Ephem. entry                  */
  int nEntries;      /* Number of ephemeris entries                  */
  double *tJD;       /* Points to array of times                     */
  double *rA;        /* Geocentric RA                                */
  double *dec;       /* Geocentric Dec                               */
  double *eLong;     /* Heliocentric ecliptic longitude              */
  double *eLat;      /* Heliocentric ecliptic latitude               */
  double *radius;    /* Distance from the sun in AU                  */
  double *mag;       /* Total visual magnitude                       */
  struct cometEphem *next; /* points to next comet                   */
} cometEphem;
