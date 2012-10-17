/*
  orrery Rev 3.8
  First Version Aug. 3, 2007
  
  Copyright (C) (2007 ... 2012) Ken Young orrery.moko@gmail.com

  This program is free software; you can redistribute it and/or 
  modify it under the terms of the GNU General Public License 
  as published by the Free Software Foundation; either 
  version 2 of the License, or (at your option) any later 
  version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
  
  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

  This program displays the night (and day!) sky on an Openmoko
  or Maemo device.
*/

#include <stdio.h>
#include <sys/stat.h>
#include <dirent.h>
#include <locale.h>
#include <fcntl.h>
#include <math.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/time.h>
#include <stdlib.h>
#include <memory.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <cairo.h>
#include <cairo-xlib.h>
#include <pango/pangocairo.h>
#include <gtk/gtk.h>
#include <dbus/dbus.h>
#include <hildon-1/hildon/hildon.h>
#include <libosso.h>
#include "orrery.h"
#include "phaseConstants.h"
#include "phenomenaConstants.h"
#include "orreryColors.h"

#define LOCATION_FILE_NAME "/var/lib/gconf/system/nokia/location/lastknown/%gconf.xml"
ssize_t getline(char **lineptr, size_t *n, FILE *stream);

#define ERROR_EXIT  (-1)
#define SUCCESS_EXIT (0)

#define likely(x)   __builtin_expect((x), TRUE)
#define unlikely(x) __builtin_expect((x), FALSE)

#define VISUAL_LIMIT (6.6)
#define CONSTELLATION_LIMIT (5.75)
#define ABSOLUTE_MAX_ZA (95.0 * DEGREES_TO_RADIANS)

#define AU (149.597870691) /* Million kilometers in an AU */
#define SPEED_OF_LIGHT (2.99792458e8)

#define MAX_FILE_NAME_SIZE (256) /* Maximum size a file name is allowed to have */

#define DEGREES_TO_RADIANS (M_PI/180.0)
#define HOURS_TO_RADIANS   (M_PI/12.0)
#define HOURS_TO_DEGREES   (15.0)
#define M_2PI              (2.0*M_PI)
#define M_HALF_PI          (0.5*M_PI)

#define CIVIL_TWILIGHT (-6.0*DEGREES_TO_RADIANS)

#define SIDEREAL_TO_SOLAR (0.997269566419)

/* Star colors */
#define DARK_GREY_LIMIT   (6.25)
#define GREY_LIMIT        (5.75)
#define WHITE_LIMIT       (5.00)

/* Codes for circumpolar specification */
#define WILL_NOT_SET       (1)
#define WILL_NOT_RISE     (-1)

#define FULL_CIRCLE (23040) /* Full circle in calls to draw_arc */

#define VGA_HEIGHT (640)
#define VGA_WIDTH  (480)
#define BUTTON_HEIGHT (70) /* Vertical size of a button */

#define STAR_NAME_V_OFFSET (14) /* Vertical offset of star name from star */

char *orreryVersion = "3.8";
char *userDir;            /* Holds the name of the directory holding private config file */
char *privateCatalogName; /* Holds the full name of the user's private locations catalog */
char dataDir[100];
char dirName[80];

int starSize[12] = {11, 10, 9, 8, 7, 6, 5, 4, 4, 3, 3, 2};

int skyIsVisible = TRUE;
int debugMessagesOn = FALSE;
#define dprintf if (debugMessagesOn) printf /* Print IFF debugging          */

int locationChanged = TRUE;
int northernHemisphere = TRUE;
int gPSLocation = FALSE;
double sinLatitude, cosLatitude, sinLatitudeCutoff;
double smallMooncalDateOffset = 0.0;

int showDeepSky = TRUE;  /* Show deep sky objects */
int showBayer = FALSE;
int showGreatCircles  = FALSE; /* Draw celestial equator, ecliptic and galactic plane */
int dayInc = 0;
float limitingMag   = VISUAL_LIMIT;  /* Faintest star to plot   */
float magScale = 1.0;
float magScale1 = 1.0;
float magScale2 = 1.0;
float faintestStar;

int showPlanets = TRUE; /* Plot planets in some way */
int showComets = FALSE; /* Plot planets in some way */
int showStars = TRUE; /* Plot fixed objects (stars, galaxies, etc) */
int displayConstellations = FALSE; /* Plot constellation stick figures */
int showNames = FALSE; /* Label bright/famous stars */
int showMeteors = FALSE; /* Show meteor show radiants */
int displayPlanetsAsSymbols = FALSE;

char *solarSystemNames[N_SOLAR_SYSTEM_OBJECTS] = {"Sun",     "Mercury",   "Venus",    "Earth",
						  "Moon",    "Mars",      "Jupiter",  "Saturn",
						  "Uranus",  "Neptune",   "Comet"};
/*
  The following array holds the scale factors, in million km,
  used for the to-scale Solar System plot.   Element n is the
  scale factor used when planet n is the outermost displayed
  planet
*/
float solarSystemScales[N_SOLAR_SYSTEM_OBJECTS] = {0.0, 20.0, 50.0, 50.0,
						   0.0, 100.0, 200.0, 500.0,
						   1000.0, 2000.0, 0.0};
						   
typedef struct orbitPlot {
  float *x, *y, maxX, omegaRad, peri;
} orbitPlot;
orbitPlot orbitPlots[N_SOLAR_SYSTEM_OBJECTS];

int shouldSwitchScreens = FALSE;

float listStartYear, listEndYear;

float cYear = 2012.0;
float cMonth = 1.0;
float cDay = 1.0;
float cHour = 0.0;
float cMinute = 0.0;
float cSecond = 0.0;
float cNSecond = 0.0;

gdouble julianDate = 0.0;

/* Parameters which may be overriden by the config file */

int chineseColorScheme     = FALSE; /* Use the chinese color scheme for great circles */
float limitingMagnitude1   = 6.60;  /* Faintest star plotted on page 1                */
float limitingMagnitude2   = 4.50;  /* Faintest star plotted on page 2                */
float initialAzimuth       = 180.;  /* Azimuth initially centered on the display      */
char locationName[100];             /* Name of default location                       */
double latitude            = 0.0;   /* Default latitude                               */
double longitude           = 0.0;   /* Default east longitude                         */
int showGreatCircles1      = FALSE; /* Plot the celestial equator, etc                */
int showGreatCircles2      = TRUE;
int showFaintStars         = FALSE;
int showStars1             = TRUE;  /* Plot stars                                     */
int showStars2             = TRUE;
int showPlanets1           = TRUE;  /* Plot planets                                   */
int showPlanets2           = TRUE;
int showComets1            = FALSE; /* Plot comets                                    */
int showComets2            = FALSE;
int showDeepSky1           = TRUE;  /* Plot deep sky objects                          */
int showDeepSky2           = FALSE;
int showNames1             = FALSE; /* Plot the names of stars on page 1              */
int showNames2             = FALSE; /* Plot the names of stars on page 2              */
int showBayer1             = FALSE; /* Show the Bayer designations on page 1          */
int showBayer2             = FALSE; /* Show the Bayer designations on page 2          */
int showMeteors1           = FALSE; /* Plot meteor shower radiants on page 1          */
int showMeteors2           = FALSE; /* Plot meteor shower radiants on page 2          */
int useAsterisms           = FALSE; /* Plot asterisms instead of constellations       */
int useGPSD                = TRUE;  /* Get position info from gpsd                    */
int jovianMoonsNE          = TRUE;  /* These variables control the orientation of the */
int jovianMoonsNW          = FALSE; /* Jovian Moons page                              */
int jovianMoonsSE          = FALSE;
int jovianMoonsSW          = FALSE;
int listLocalEclipsesOnly  = FALSE; /* List only eclipses visible at current location */
int listPenumbralEclipses  = FALSE; /* Penumbral eclipses are boring                  */
int listPartialEclipses    = TRUE;  /* Partial eclipses are OK                        */
int listTotalEclipses      = TRUE;  /* These are the good ones                        */

int useCurrentTime = TRUE;
int useTextTime = FALSE;
int useJulianTime = FALSE;
int useCalendarTime = FALSE;
int haveFaintStars = FALSE;    /* True if optional faint catalog is available */
int showingFaintStars = FALSE; /* True if optional faint catalog is active    */
int savedTimeMode;
double savedTJD;

/* End of parameters which may be overriden by config file */

#define DEFAULT_UPDATE_RATE (60000)
#define ONE_HZ_UPDATE_RATE  (1000)
#define FAST_UPDATE_RATE (100)
int appHasFocus = TRUE;
int fastUpdates = FALSE;
guint timerID = (guint)0; /* Holds the ID of the periodic timer that triggers screen updates */

int inAzCompassMode = FALSE;

int postageStampUTHH, postageStampUTMM, postageStampLSTHH, postageStampLSTMM;
int imAPostageStamp = FALSE;
int postageModeDisabled = FALSE;

int useCustomLocation = FALSE;
int useMenuLocation = FALSE;
float oneHalf    = 0.5;

struct tm *gMT = NULL;
struct timespec fineTime;
double tJD           = 0.0;
double myLST;
int needNewTime      = TRUE;

int labelMode = FALSE;

int displayWidth, displayHeight, plotWidth, plotHeight;
int xBorder = 5;
int yBorder = 5;
int xLeftLabelSkip = 10;
int yBottomLabelSkip = 18;
int yTopLabelSkip = 14;
int bigTick = 4;
int smallTick = 2;

float centerAz    = 270.0*DEGREES_TO_RADIANS;
/*
  A double precision copy of the above variable is maintained in
  order to reduce the number of float->double conversions required
*/
float azSpan      =  90.0*DEGREES_TO_RADIANS;
float labelClipZA =   3.0;
#define INITIAL_OFFSET (0.0)
double thetaOffset = INITIAL_OFFSET;
double piMinusThetaOffset = M_PI - INITIAL_OFFSET;
double twoPiMinusThetaOffset = M_2PI - INITIAL_OFFSET;
double centerAzD;
double maximumZA  =  ABSOLUTE_MAX_ZA;
double minimumZA  = 0.0;
double maximumDisplayZA = 98.0*DEGREES_TO_RADIANS;
int zoomed = FALSE;

double latDD, latMM, latSS, longDD, longMM, longSS;

int landscapeMode = FALSE;

/* Variables used in transverse Mercator projection */

GdkPoint *darkGreyPoints, *greyPoints, *whitePoints;
int nDarkGreyPoints, nGreyPoints, nWhitePoints;

float mercatorScale, mercatorOffset;

int haveReadStarsFile = FALSE;
int inFlashlightMode = FALSE;
#define ABOUT_SCREEN                  (0)
#define SOLUNI_SCREEN                 (1)
#define BIG_MOONCAL_SCREEN            (2)
#define SMALL_MOONCAL_SCREEN          (3)
#define PLANETCOMPASS_SCREEN          (4)
#define SOLAR_SYSTEM_SCHEMATIC_SCREEN (5)
#define SOLAR_SYSTEM_SCALE_SCREEN     (6)
#define METEOR_SHOWERS_SCREEN         (7)
#define TIMES_PAGE_SCREEN             (8)
#define ANALEMMA_SCREEN               (9)
#define PLANET_ELEVATION_SCREEN      (10)
#define PLANET_PHENOMENA             (11)
#define JOVIAN_MOONS                 (12)
#define CELESTIAL_NAVIGATION         (13)
#define LUNAR_ECLIPSES               (14)
#define COMET_SCREEN               (15)

int aboutScreen = ABOUT_SCREEN;
int displayingAnOptsPage = FALSE;

int nSolarSystemDays = 1;

/* Constellation Types: */
#define MODERN    (0)
#define ZODIAC    (1)
#define PTOLEMAIC (2)
#define ASTERISM  (3)

#define CONST_NAME_DELTA (M_PI/360.0)
typedef struct constellation {
  char *name;
  int type;
  double nameRA;
  double sinNameDec;
  double cosNameDec;
  float nameAngle;
  int nPoints;
  short *starNeeded;
  double *rA;
  double *dec;
  double *sinDec;
  double *cosDec;
  struct constellation *next;
} constellation;

constellation *constellations = NULL;
int constellationsInitialized = FALSE;

int inFullscreenMode = FALSE;
/*
  fullscreenStateChanging is set when going into or out of
  fullscreen mode, to prevent the screen from being redrawn
  twice.
*/
int fullscreenStateChanging = FALSE;

GdkFont *smallFont, *bigFont;
GdkGC *gC[N_COLORS], *planetGC, *starGC[5], *flashlightGC, *aboutGC;
GtkWidget *window, *julSpin, *optionsStackable, *extraStackable, *optsStackable,
  *cDateLabel, *cTimeLabel, *julSpinLabel, *regionMenu, *yearSpin, *monthSpin, *daySpin,
  *hourSpin, *minuteSpin, *secondSpin, *mainBox, *drawingArea, *itemsTable, *locationTable,
  *optionsTable, *controlButtonBox, *itemsButton, *locationButton, *timeButton,
  *optionsButton, *wikiButton, *tipsButton, *locationLatName, *locationLongName,
  *longButton, *latButton, *eastButton, *westButton, *northButton, *southButton,
  *hildonTimeButton, *hildonDateButton, *magButton1, *magButton2, *lunarEclipseStackable,
  *lunarEclipseSelectionLabel, *lunarEclipseSelectionTable, *eclipseTypeButton,
  *lunarEclipseTypeLabel, *lunarEclipseDateLabel, *lunarEclipseStartYearLabel,
  *lunarEclipseEndYearLabel, *startYearSpin, *endYearSpin, *rebuildEclipseListButton;

GdkPixmap *planetImages[N_SOLAR_SYSTEM_OBJECTS];      /* Used for Sky plots */
GdkPixmap *smallPlanetImages[N_SOLAR_SYSTEM_OBJECTS]; /* Used for Solar System plots */
GdkPixmap *firstPointImage, *meteorRadiantImage[3], *moonImage, *moonImageFlipped,
  *moonImage210, *moonImage210Flipped,
  *fullMoonImage, *fullMoonImageFlipped, *blueMoonImage, *blueMoonImageFlipped;

GdkPixmap *pixmap = NULL;
GdkPixmap *cairoPixmap = NULL;
GdkPixmap *lunarEclipsePixmap;

/*
  The following structure keeps track of regions on the
  drawingArea you can click the stylus on, to perform actions.
*/
typedef struct sensitiveArea {
  struct sensitiveArea *forwardPointer;
  int type; /* Type determines the action taken */
  float value;
  int bLCX, bLCY, tRCX, tRCY;
} sensitiveArea;

sensitiveArea *sensitiveAreaRoot = NULL;

/* Types of sensitiveAreas: */
#define SA_TOP_AREA            (1)
#define SA_FINGER_PAN_AREA     (2)
#define SA_MONTH_LEFT_ARROW    (3)
#define SA_MONTH_RIGHT_ARROW   (4)
#define SA_SOLAR_SYSTEM_BUTTON (5)
#define SA_JOVIAN              (6)
#define SA_UPDATE_NAVIGATION   (7)
#define SA_NAVIGATION_OBJECT   (8)
#define SA_DISPLAY_NAV_LIST    (9)

/* Types of deep sky object */
#define SUPERNOVA_REMNENT (0)
#define GLOBULAR_CLUSTER  (1)
#define OPEN_CLUSTER      (2)
#define DIFFUSE_NEBULA    (3)
#define PLANETARY_NEBULA  (4)
#define GALAXY            (5)

typedef struct deepSkyEntry {
  char *name;
  double rA;
  double dec;
  double sinDec;
  double cosDec;
  float mag;
  short type;
  struct deepSkyEntry *next;
} deepSkyEntry;

deepSkyEntry *deepSkyRoot = NULL;
int haveReadDeepSkyObjects = FALSE;

typedef struct objectListEntry {
  struct objectListEntry *forwardPointer;
  int hipNumber;
  double rAJ2000;
  double decJ2000;
  double cosDec;
  double sinDec;
  float parallax;
  float muRA;
  float muDec;
  float mag;
  float color;
  char display;
} objectListEntry;

objectListEntry *objectListRoot = NULL;

typedef struct __attribute__((packed)) hipparcosEntry {
  int number;
  double rAJ2000;
  double decJ2000;
  float parallax;
  float muRA;
  float muDec;
  short mag;
  short color;
} hipparcosEntry;

/* Variables related to star names */

typedef struct __attribute__((packed)) starNameTemp {
  int nHip;
  unsigned short offset;
  unsigned char nameLen;
  unsigned char nGreek;
  unsigned char flags;
} starNameTemp;

typedef struct starNameTempList {
  starNameTemp record;
  int found;
  struct starNameTempList *next;
} starNameTempList;

starNameTempList *starNameTempListRoot = NULL;
char *starNameString;

typedef struct starNameEntry {
  unsigned short offset;
  unsigned char nameLen;
  unsigned char nGreek;
  unsigned char flags;
  objectListEntry *hip;
  struct starNameEntry *next;
} starNameEntry;

starNameEntry *starNameEntryRoot;

extern cometEphem *cometRoot;
extern int cometDataReadIn;

/* Menu-related definitions: */
typedef struct locationEntry {
  char *name;
  float latitude, longitude;
  struct locationEntry *next;
} locationEntry;

typedef struct locationClass {
  char *name;
  int key;
  int menuCreated;
  int nLocations;
  struct locationEntry *entry;
  GtkItemFactoryEntry *itemFactoryEntry;
  GtkItemFactory *itemFactory;
  GtkAccelGroup *accelGroup;
  GtkWidget *menu;
  struct locationClass *next;
} locationClass;

locationClass *locationClassRoot = NULL;

/*
  The variables below are used to hold some values used in the low level coordinate
  transformations.   Some of these variables have constant values, but constants
  are not used, in order to avoid unneeded double - > float type conversions,
  etc.
*/

/* These three constants are used in the mercator function: */
float mC5 = 1.0/24.0;
float mC3 = 1.0/6.0;
float mC1 = 1.0;

/* Constants used by thetaEta */
double maxTheta = 97.0*DEGREES_TO_RADIANS;

/* Put the color thresholds into float variables so that a double->float
   conversion is not needed.
*/
float colorThreshold1 = 2.00;
float colorThreshold2 = 0.01;
float colorThreshold3 = 0.40;
float colorThreshold4 = 0.80;
float colorThreshold5 = 1.30;

/* Variables used for touchpad Gestures */

double buttonPressTime, buttonReleaseTime;
struct timespec timeSpecNow;
GdkPoint *userPoly;
int notAPan = FALSE;
gint nUserPoints = 0;
int buttonPressed = FALSE;

int outermostPlanet = JUPITER; /* Furthest planet shown in to-scale SS view */

int gPSDSocketOpen = FALSE;
int gPSDSocket;

char *dayName[7] = {"Sunday", "Monday", "Tuesday", "Wednesday",
		    "Thursday", "Friday", "Saturday"};
char *monthName[12] = {"January",   "February", "March",    "April",
		       "May",       "June",     "July",     "August",
		       "September", "October",  "November", "December"};
double monthLengths[12] = {31.0, 28.0, 31.0, 30.0, 31.0, 30.0, 31.0, 31.0, 30.0, 31.0, 30.0, 31.0};

float planetRadii[10] = {0.695997, 0.002425, 0.006070, 0.006378, 0.001738, 0.003395,
			 0.071492, 0.060100, 0.024500, 0.025100};

/* Meteor Shower Stuff */

typedef struct radiantPosition { /*
				   Several of these entries per shower, to
				   track the radiant as it moves on sky
				 */
  short month, day;
  double rA, dec;
} radiantPosition;

typedef struct meteorShower { /* One of these entries per meteor shower */
  char *fullName;
  char threeLetterName[4];
  short startMonth, startDay;
  short endMonth, endDay;
  short maxMonth, maxDay;
  short eclipticLong;
  short radRA, radDec;
  short vInf;
  float r;
  short zHR;
  short nRadiantPositions;
  radiantPosition *radiantPositions;
  struct meteorShower *next;
} meteorShower;

meteorShower *meteorShowers = NULL;
int haveReadMeteorShowers = FALSE;

/* Definitions related to the Azimuth SLew Compass */
#define AZ_COMPASS_RADIUS      (173)     /* Radius of compass circle */
#define AZ_COMPASS_POINT_HEIGHT (20)     /* Height of N, E, S, W pointers */
#define AZ_COMPASS_TICK_LENGTH  (10)     /* Length of 5 degree hash marks */
int azCompassCenterX, azCompassCenterY;  /* Pixel coods of compass center */
float chosenAz;                          /* New center azimuth selected by user */

/* Cairo and Pango related globals */

#define N_PANGO_FONTS (7)
#define SMALL_PANGO_FONT       (0)
#define MEDIUM_PANGO_FONT      (1)
#define BIG_PANGO_FONT         (2)
#define SMALL_MONO_PANGO_FONT  (3)
#define GREEK_FONT             (4)
#define TINY_GREEK_FONT        (5)
#define HUGE_PANGO_FONT        (6)
#define BIG_PANGO_FONT_NAME         "Sans Bold 32"
#define MEDIUM_PANGO_FONT_NAME      "Sans Normal 18"
#define SMALL_PANGO_FONT_NAME       "Sans Normal 14"
#define SMALL_MONO_PANGO_FONT_NAME  "Monospace Bold 16"
#define GREEK_FONT_NAME             "Sans 12"
#define TINY_GREEK_FONT_NAME        "Sans 8"
#define HUGE_PANGO_FONT_NAME        "Sans Bold 78"

int backgroundGC = OR_BLACK;
cairo_surface_t *cairoSurface = NULL;
cairo_t *cairoContext = NULL;

int cairoInitialized = FALSE;

int forceNewPosition = TRUE; /* If this is true, a new position will be obtained before the page is drawn */

char greekAlphabet[(3*24)+1]; /* Stores the lower case greek alphabet for Bayer designations
				 - each character is null terminated */

char scoreBoard[48000]; /* Keeps track of which pixels have names printed on them
			   This is used to prevent star labels from being printed over
			   each other. */

/* Variables used for day-of-week calculations */
unsigned char normalYear[12] = {0, 3, 3, 6, 1, 4, 6, 2, 5, 0, 3, 5};
unsigned char leapYear[2] = {6, 2};

int mainBoxInOptsStackable = FALSE;

int moonImagesRead = FALSE; /* Set TRUE after all moon images have been read in */

int optionsModified = FALSE;

GtkCheckButton *starCheckButton1, *starCheckButton2, *starNameCheckButton1,
  *starNameCheckButton2, *meteorCheckButton1, *meteorCheckButton2,
  *planetCheckButton1, *planetCheckButton2, *listLocalEclipsesOnlyButton,
  *cometCheckButton1, *cometCheckButton2,
  *listPenumbralEclipsesButton, *listPartialEclipsesButton, *listTotalEclipsesButton,
  *greatCirclesCheckButton1, *greatCirclesCheckButton2, *useAsterismsCheckButton,
  *deepSkyCheckButton1, *deepSkyCheckButton2, *bayerButton1, *bayerButton2,
  *gPSButton, *customLocationButton, *menuLocationButton, *currentTimeButton,
  *textTimeButton, *julianTimeButton, *calendarTimeButton;
GtkWidget *locationNameLabel, *locationNameText, *lunarEclipseSeparator1,
  *lunarEclipseSeparator2, *lunarEclipseSeparator3;

GtkWidget *displayedLocationName;   /* Holds the name of a location selected from menu */
int locationNameIsDisplayed = FALSE;
int menuShowing = FALSE;
int privateMenuShowing = FALSE;
GtkWidget *activeMenu;

int saveAsPrivateShowing = FALSE;
GtkWidget *saveAsPrivate;
int deleteFromPrivateShowing = FALSE;
GtkWidget *deleteFromPrivate;

int magButtonAdjusted = FALSE;

HildonAppMenu *hildonMenu;

int jovianEvents = FALSE; /* Set TRUE to list events on Jovian moon Page */

int updateCelestialNavigationScreen = TRUE;
int displayIndividualNavObject = FALSE;
int individualNavObject;

/* Eclipse-related variables */
int displayEclipse = FALSE;
int selectedLunarEclipse = -1;

/* Variables used to draw world shorelines */

typedef struct shoreSeg {
  short nVerticies;
  short *lat;
  short *lon;
  struct shoreSeg *next;
} shoreSeg;

shoreSeg *shoreRoot = NULL;

#define ETA 23.4378527778*DEGREES_TO_RADIANS /* Earth axis inclination */
#define ETAG 62.3*DEGREES_TO_RADIANS         /* Galactic pole inclination */

/* Lunar Eclipse Constants */
#define SOLAR_RADIUS            (6.9599e10)       /* cm */
#define EARTH_EQUATORIAL_RADIUS (6.378164e8)
#define EARTH_POLAR_RADIUS      (6.356779e8)
#define MOON_RADIUS             (1.7382e8)

#define PENUMBRAL_LUNAR_ECLIPSE (0x00)
#define PARTIAL_LUNAR_ECLIPSE   (0x01)
#define TOTAL_LUNAR_ECLIPSE     (0x02)
#define N_LUNAR_ECLIPSES       (12064)
#define N_ECLIPSE_TJDS             (7)
#define MID_ECLIPSE_TJD            (0)
#define PEN_ECLIPSE_START_TJD      (1)
#define PAR_ECLIPSE_START_TJD      (2)
#define TOT_ECLIPSE_START_TJD      (3)
#define PEN_ECLIPSE_END_TJD        (4)
#define PAR_ECLIPSE_END_TJD        (5)
#define TOT_ECLIPSE_END_TJD        (6)
/* End of Lunar Eclipse Constants */

typedef struct __attribute__((packed)) lunarEclipse {
  int   date;      /* Calendar Date                                        */
  float tDGE;      /* TD of Greatest Eclipse                               */
  int   dTMinusUT; /* Dynamical TIme (DT) - UT                             */
  short sarosNum;  /* Saros Number                                         */
  char  type1;     /* 0 = Penumbral, 1 = Partial, 2 = Total                */
                   /* Upper nibble:                                        */
                   /* 0x00 = middle eclipse of Saros Cycle                 */
                   /* 0x01 = Central total eclipse                         */
                   /*        (Moon's center passes north of shadow axis).  */
                   /* 0x02 = Central total eclipse                         */
                   /*        (Moon's center passes south of shadow axis).  */
                   /* 0x03 = Total penumbral lunar eclipse.                */
                   /* 0x04 = Saros series begins                           */
                   /*        (first penumbral eclipse in series).          */
                   /* 0x05 = Saros series ends                             */
                   /*        (last penumbral eclipse in series).           */
  float penMag;    /* Penumbral magnitude is the fraction of the Moon's    */
                   /* diameter immersed in the penumbra at the instant of  */
                   /* greatest eclipse.                                    */
  float umbMag;    /* Umbral magnitude is the fraction of the Moon's       */
                   /* diameter immersed in the umbra at the instant of     */
                   /* greatest eclipse.                                    */
  float penDur;    /* Penumbral phase duration in minutes                  */
  float parDur;    /* Partial phase duration in minutes                    */
  float totDur;    /* Total phase duration in minutes                      */
  short zenithLat; /* Latitude on Earth where the Moon appears in the      */
                   /* zenith at instant of greatest eclipse.               */
  short zenithLon; /* Longitude on Earth where the Moon appears in the     */
                   /* zenith at instant of greatest eclipse.               */
} lunarEclipse;

lunarEclipse *lunarEclipses = NULL;

typedef struct eclipseMenuItem {
  char *name;
  int key;
  struct eclipseMenuItem *next;
} eclipseMenuItem;

eclipseMenuItem *eclipseMenuItemRoot = NULL;
int nEclipsesForMenu = 0;
GtkItemFactoryEntry *lunarItemFactoryEntry;
GtkItemFactory *lunarItemFactory;
GtkAccelGroup *lunarAccelGroup;
GtkWidget *lunarMenu;

/*   E N D   O F   G L O B A L   V A R I A B L E   D E C L A R A T I O N S   */

/*   F U N C T I O N   P R O T O T Y P E S   */

double deltaT(double tJD);

static void fullRedraw(int dummy);

float roundf(float x);

void readInCometEphemerides(char *dataDir);
void getCometRADec(char *dataDir, char *name, double tJD, int eq, double *c1, double *c2, double *c3, double *mag);
void planetInfo(char *dataDir, int planetNumber, double tJD, double *rA, double *dec, float *F, float *mag);
void vSOPPlanetInfo(char *dataDir, double tJD, int planet, double *rA, double *dec, double *distance);
void analemma(char *dataDir, double tJD, double *EoT, double *dec, double *EoE, double *eclipticLong);
void moonPosition(double jDE, double *rA, double *dec,
		  double *eLong, double *eLat, double *distance, float *Fr);
void seasons(char *dataDir, int year,
	     double *spring, double *summer, double *fall, double *winter);

void calculatePlanetPosition(int planetNumber, double tJD, double *eX, double *eY, double *eZ);
void getMoonPosition(double tJD, double sunELong, double *rA, double *dec, double *eLong, double *eLat, float *F);

void getCurrentOrbitalElements(int planet,         /* Planet we need elements for                  */
                               double tJD,         /* Time for which elements should be calculated */
                               double *a,          /* Semimajor axis                               */
                               double *e,          /* Eccentricity                                 */
                               double *I,          /* Inclination                                  */
                               double *L,          /* Mean longitude                               */
                               double *smallOmega, /* Longitude of perihelion                      */
                               double *bigOmega    /* Longitude of ascending node                  */
                               );

void scheduleUpdates(char *caller, int rate);

void lowAccuracyJovSats(double jD, double *X, double *Y, double *uR);
void highAccuracyJovSats(double jD, double deltaAU,
			 double jupiterLong, double jupiterLat,
			 double *X, double *Y, double *Z);

void calculateApparentPosition(double tJD,
			       double rAJ2000, double decJ2000,
			       float parallax, float muRA, float muDec,
			       double *rANow,  double *decNow);

void putOptsPage(int page);

/*   E N D   O F   F U N C T I O N   P R O T O T Y P E S   */

#define RENDER_CENTER_X (80)
#define RENDER_CENTER_Y (400)

/*
  Create the structures needed to be able to use Pango and Cairo to
render antialiased fonts
*/
void createPangoCairoWorkspace(void)
{
  cairoContext = gdk_cairo_create(cairoPixmap);
  if (unlikely(cairoContext == NULL)) {
    fprintf(stderr, "cairo_create returned a NULL pointer\n");
    exit(ERROR_EXIT);
  }
  cairo_translate(cairoContext, RENDER_CENTER_X, RENDER_CENTER_Y);
  cairoInitialized = TRUE;
}

/*
  The following function renders a text string using Pango and Cairo, on the
  cairo screen, and copies it to a gtk drawable.   The height and width
  of the area occupied by the text is returned in passed variables.
*/
void renderPangoText(char *theText, unsigned short color, int font,
		     int *width, int *height, GdkDrawable *dest,
		     int x, int y, float angle, int center, int topClip)
{
  static int firstCall = TRUE;
  static PangoFontMap *fontmap = NULL;
  static char *fontName[N_PANGO_FONTS] = {SMALL_PANGO_FONT_NAME, MEDIUM_PANGO_FONT_NAME,
					  BIG_PANGO_FONT_NAME, SMALL_MONO_PANGO_FONT_NAME,
					  GREEK_FONT_NAME, TINY_GREEK_FONT_NAME,
					  HUGE_PANGO_FONT_NAME};
  static PangoContext *pangoContext[N_PANGO_FONTS];
  static cairo_font_options_t *options = NULL;
  static int xC, yC, wC, hC = 0;
  static float currentAngle = 0.0;
  float deltaAngle, cA, sA, fW, fH;
  int pWidth, pHeight, iFont;
  PangoFontDescription *fontDescription;
  PangoLayout *layout;

  if (angle < 0.0)
    angle += M_2PI;
  else if (angle > M_2PI)
    angle -= M_2PI;
  if ((angle > M_HALF_PI) && (angle < M_HALF_PI + M_PI))
    angle -= M_PI;
  if (angle < 0.0)
    angle += M_2PI;
  if (!cairoInitialized)
    createPangoCairoWorkspace();
  if (firstCall) {
    /* Initialize fonts, etc. */
    fontmap = pango_cairo_font_map_get_default();
    if (unlikely(fontmap == NULL)) {
      fprintf(stderr, "pango_cairo_font_map_get_default() returned a NULL pointer\n");
      exit(ERROR_EXIT);
    }
    options = cairo_font_options_create();
    if (unlikely(options == NULL)) {
      fprintf(stderr, "cairo_font_options_create() returned a NULL pointer\n");
      exit(ERROR_EXIT);
    }
    cairo_font_options_set_antialias(options, CAIRO_ANTIALIAS_GRAY);
    cairo_font_options_set_hint_style(options, CAIRO_HINT_STYLE_FULL);
    cairo_font_options_set_hint_metrics(options, CAIRO_HINT_METRICS_ON);
    cairo_font_options_set_subpixel_order(options, CAIRO_SUBPIXEL_ORDER_BGR);
    for (iFont = 0; iFont < N_PANGO_FONTS; iFont++) {
      fontDescription = pango_font_description_new();
      if (unlikely(fontDescription == NULL)) {
	fprintf(stderr, "pango_font_description_new() returned a NULL pointer, font = %d\n", iFont);
	exit(ERROR_EXIT);
      }
      pango_font_description_set_family(fontDescription, (const char*) fontName[iFont]);
      pangoContext[iFont] = pango_cairo_font_map_create_context(PANGO_CAIRO_FONT_MAP(fontmap));
      if (unlikely(pangoContext[iFont] == NULL)) {
	fprintf(stderr, "pango_cairo_font_map_create_context(iFont = %d) returned a NULL pointer\n", iFont);
	exit(ERROR_EXIT);
      }
      pango_context_set_font_description(pangoContext[iFont], fontDescription);
      pango_font_description_free(fontDescription);
      pango_cairo_context_set_font_options(pangoContext[iFont], options);
    }
    firstCall = FALSE;
  } else
    gdk_draw_rectangle(cairoPixmap, gC[backgroundGC], TRUE, RENDER_CENTER_X+xC, RENDER_CENTER_Y+yC,
		       wC, hC);
  layout = pango_layout_new(pangoContext[font]);
  if (unlikely(layout == NULL)) {
    fprintf(stderr, "pango_layout_new() returned a NULL pointer\n");
      exit(ERROR_EXIT);
  }
  pango_layout_set_text(layout, theText, -1);
  fontDescription = pango_font_description_from_string(fontName[font]);
  pango_layout_set_font_description (layout, fontDescription);
  pango_font_description_free(fontDescription);
  cairo_set_source_rgb(cairoContext,
		       ((double)orreryColorRGB[color][0])/DOUBLE_MAX16,
		       ((double)orreryColorRGB[color][1])/DOUBLE_MAX16,
			((double)orreryColorRGB[color][2])/DOUBLE_MAX16);
  deltaAngle = angle - currentAngle;
  cairo_rotate(cairoContext, deltaAngle);
  currentAngle = angle;
  pango_cairo_update_layout(cairoContext, layout);
  pango_layout_get_size(layout, &pWidth, &pHeight);
  fW = (float)pWidth/(float)PANGO_SCALE;
  fH = (float)pHeight/(float)PANGO_SCALE;
  *width = (int)(fW+0.5);
  *height = (int)(fH+0.5);
  cairo_move_to(cairoContext, 0.0, 0.0);
  pango_cairo_show_layout(cairoContext, layout);
  g_object_unref(layout);
  cA = cosf(angle); sA = sinf(angle);
  wC = (int)((fW*fabs(cA) + fH*fabs(sA)) + 0.5);
  hC = (int)((fW*fabs(sA) + fH*fabs(cA)) + 0.5);
  if (angle < M_HALF_PI) {
    xC = (int)((-fH*sA) + 0.5);
    yC = 0;
  } else {
    xC = 0;
    yC = (int)((fW*sA) + 0.5);
  }
  if (dest != NULL) {
    if (center) {
      int xM, yM, ys, yd, h;

      xM = (int)((fW*fabs(cA) + fH*fabs(sA)) + 0.5);
      yM = (int)((fW*fabs(sA) + fH*fabs(cA)) + 0.5);
      yd = y-(yM>>1);
      if (yd < topClip) {
	int delta;

	delta = topClip - yd;
	h = hC - delta;
	ys = RENDER_CENTER_Y + yC + delta;
	yd = topClip;
      } else {
	ys = RENDER_CENTER_Y+yC;
	h = hC;
      }
      gdk_draw_drawable(dest, gC[OR_BLUE], cairoPixmap, RENDER_CENTER_X+xC, ys,
			x-(xM>>1), yd, wC, h);
    } else
      gdk_draw_drawable(dest, gC[OR_BLUE], cairoPixmap, RENDER_CENTER_X+xC, RENDER_CENTER_Y+yC,
			x, y-((*height)>>1), wC, hC);
  }
}

/*
  The following function adds a new sensitive area entry to
  the linked list.
*/
static void addSensitiveArea(int showArea, int type, int x1, int y1, int x2, int y2, float value)
{
  static sensitiveArea *last, *new;
  GdkPoint points[4];

  new = (sensitiveArea *)malloc(sizeof(sensitiveArea));
  if (unlikely(new == NULL)) {
    perror("malloc of new sensitiveArea");
    exit(ERROR_EXIT);
  }
  new->type = type;
  new->bLCX = x1; new->bLCY = y1; new->tRCX = x2; new->tRCY = y2;
  new->value = value;
  new->forwardPointer = NULL;
  if (sensitiveAreaRoot == NULL)
    sensitiveAreaRoot = new;
  else
    last->forwardPointer = new;
  last = new;
  if (showArea) {
    points[0].x = x1; points[0].y = y2;
    points[1].x = x2; points[1].y = y2;
    points[2].x = x2; points[2].y = y1;
    points[3].x = x1; points[3].y = y1;
    gdk_draw_polygon(pixmap, gC[OR_YELLOW], TRUE, points, 4);
  }
}
/*
  Garbage-collect the linked list which stores the areas
on the screen which are supposed to respond to touch.
This is normally done before a new page is plotted, which
may need different parts of the touchscreen to be sensitive.
 */
static void removeAllSensitiveAreas(void)
{
  sensitiveArea *sA = sensitiveAreaRoot;

  while (sA != NULL) {
    sensitiveArea *victim;

    victim = sA;
    sA = victim->forwardPointer;
    free(victim);
  }
  sensitiveAreaRoot = NULL;
}

/*
  Build a Julian date from the UT date and time.
*/
double buildTJD(int year, int month, int day, int hour, int minute, int second, int nsec)
{
  int y, m, a, b, c, dd;
  double tJD, d;

  y = 1900+year;
  m = 1+month;
  d = day + ((double)hour)/24.0 + ((double)minute)/1440.0 +
    ((double)second)/86400.0 + ((double)nsec/(86400.0e9));
  if (m < 3) {
    y -= 1;
    m += 12;
  }
  a = (int)(y/100);
  b = 2 - a + a/4;
  c = (int)(365.25*(double)y);
  dd = (int)(30.6001*(double)(m+1));
  tJD = (double)(b+c+dd) + d + 1720994.5;
  return(tJD);

}

/*
  This function returns the Julian Date
*/
static double getTJD(void)
{
  double tJD;
  time_t t;

  if (likely(useCurrentTime)) {
    t = time(NULL);
    gMT = gmtime(&t);
    clock_gettime(CLOCK_REALTIME, &fineTime);
    cYear    = gMT->tm_year + 1900;
    cMonth   = gMT->tm_mon + 1;
    cDay     = gMT->tm_mday;
    cHour    = gMT->tm_hour;
    cMinute  = gMT->tm_min;
    cSecond  = gMT->tm_sec;
    cNSecond = fineTime.tv_nsec;
  } else if (useJulianTime) {
    return(julianDate);
  } else {
    if (gMT == NULL) {
      gMT = (struct tm *)malloc(sizeof(struct tm));
      if (unlikely(gMT == NULL)) {
	perror("malloc of gMT");
	exit(ERROR_EXIT);
      }
    }
    gMT->tm_isdst = FALSE;
    gMT->tm_year  = cYear-1900;
    gMT->tm_mon   = cMonth-1;
    gMT->tm_mday  = cDay;
    gMT->tm_hour  = cHour;
    gMT->tm_min   = cMinute;
    gMT->tm_sec   = cSecond;
    cNSecond = fineTime.tv_nsec = 0;
  }
  tJD = buildTJD(gMT->tm_year, gMT->tm_mon, gMT->tm_mday, gMT->tm_hour, gMT->tm_min, gMT->tm_sec,
		 fineTime.tv_nsec);
  return(tJD);
}

/*
  Try to read the latitude and longitude from the phone's last position file.
  Return TRUE if successful, FALSE if the position can't be established.
  If TRUE is returned, then *lat and *long will contain the latitude and
  longitude from the phone.
 */
int readPhonePosition(double *latitude, double *longitude)
{
  size_t nBytes;
  double tLongitude, tLatitude;
  char *theLine = NULL;
  FILE *locationFile;

  locationFile = fopen(LOCATION_FILE_NAME, "r");
  if (unlikely(locationFile == NULL)) {
    perror("Opening location file");
    return(FALSE);
  } else {
    while (!feof(locationFile)) {
      int nRead;
      
      nBytes = 1000;
      nRead = getline(&theLine, &nBytes, locationFile);
      if (nRead) {
	if (strstr(theLine, "longitude"))
	  sscanf(strstr(theLine, "value=\"")+7, "%lf", &tLongitude);
	if (strstr(theLine, "latitude"))
	  sscanf(strstr(theLine, "value=\"")+7, "%lf", &tLatitude);
      }
    }
    fclose(locationFile);
    free(theLine);
    *longitude = tLongitude * DEGREES_TO_RADIANS;
    *latitude  = tLatitude * DEGREES_TO_RADIANS;
    return(TRUE);
  }
}

/*
  This function retrieves the latitude and longitude from the file Maemo 5 maintains
  which contains the last position derived for the phone, either from cell tower
  coordinates, or GPS.
*/
void getMaemoLocation(void)
{
  static double lastLongitude, lastLatitude;
  
  if (forceNewPosition)
    lastLongitude = lastLatitude = forceNewPosition = 0;
  gPSLocation = readPhonePosition(&latitude, &longitude);
  if (gPSLocation) {
    if ((longitude != lastLongitude) || (latitude != lastLatitude))
      locationChanged = TRUE;
    lastLatitude = latitude;
    lastLongitude = longitude;
  }
}

#define N_GPS_SATELLITES_MAX (32)
#define MAX_GPSD_RESPONSE_LENGTH (28 + (N_GPS_SATELLITES_MAX*16))
/*
  Calculate some constants we'll need to update anytime the observer
  changes the position.
*/
static void newPosition(void)
{
  if (useGPSD)
    getMaemoLocation();
  if (locationChanged) {
    if (latitude > 0.0) {
      sinLatitudeCutoff = sin((latitude - (5.0*DEGREES_TO_RADIANS)) - M_HALF_PI);
      northernHemisphere = TRUE;
    } else {
      sinLatitudeCutoff = sin((latitude + (5.0*DEGREES_TO_RADIANS)) + M_HALF_PI);
      northernHemisphere = FALSE;
    }
    if (fabs(latitude) <= (5.0*DEGREES_TO_RADIANS))
      sinLatitudeCutoff = atof("NaN");
    sinLatitude = sin(latitude);
    cosLatitude = cos(latitude);
    locationChanged = FALSE;
  }
}

/*
  Return the Greenwich Mean Siderial Time
*/
double gMST(double tJD)
{
  int nDays;
  double tJD0h, s, t, t0;

  tJD0h = (double)((int)tJD) + 0.5;
  s = tJD0h - 2451545.0;
  t = s/36525.0;
  t0 = 6.697374558 +
    (2400.051336 * t) +
    (0.000025862*t*t);
  nDays = (int)(t0/24.0);
  t0 -= 24.0*(double)nDays;
  t0 += (tJD - tJD0h) * 24.0 * 1.002737909;
  while (t0 < 0)
    t0 += 24.0;
  while (t0 >= 24.0)
    t0 -= 24.0;
  return(t0);
}

/*
  Fix up situations where rounding has caused a value of 60 to be stored in a minutes
or seconds field.   Returns TRUE if the hours field wraps through 24.
 */
int fix60s(int *hours, int *min)
{
  int overflow = FALSE;

  if (*min > 59) {
    *hours += 1;
    *min -= 60;
  }
  if (*hours > 23) {
    overflow = TRUE;
    *hours -= 24;
  }
  return(overflow);
}

/*
  Normalize an angle between 0 and 2pi radians
*/
void doubleNormalize0to2pi(double *angle)
{
  if (fabs(*angle) < 2.0e9) {
    int quotient;

    quotient = (int)(*angle / M_2PI);
    *angle = *angle - ((double)quotient)*M_2PI;
    if (*angle < 0.0)
      *angle += M_2PI;
  } else {
    /* Let's hope this path is never taken - it will be expensive! */
    while (*angle > M_2PI)
      *angle -= M_2PI;
    while (*angle < 0.0)
      *angle += M_2PI;
  }
}

void doubleNormalizeMinusPiToPi(double *angle)
{
  doubleNormalize0to2pi(angle);
  while (*angle > M_PI)
    *angle -= M_2PI;
}

void floatNormalize0to24(float *hour)
{
  while (*hour > 24.0)
    *hour -= 24.0;
  while (*hour < 0.0)
    *hour += 24.0;
}

void doubleNormalize0to24(double *hour)
{
  while (*hour > 24.0)
    *hour -= 24.0;
  while (*hour < 0.0)
    *hour += 24.0;
}

void floatNormalize0to2pi(float *angle)
{
  double temp;

  temp = *angle;
  doubleNormalize0to2pi(&temp);
  *angle = (float)temp;
}

void doubleNormalize0to360(double *a)
{
  double temp;

  temp = (*a) * DEGREES_TO_RADIANS;
  doubleNormalize0to2pi(&temp);
  *a = temp / DEGREES_TO_RADIANS;
}

/*
  Return the Local Siderial Time for the given Julian Date
*/
double lSTAtTJD(double tJD)
{
  double theLST;

  theLST = gMST(tJD);
  theLST *= HOURS_TO_DEGREES;
  theLST += longitude/DEGREES_TO_RADIANS;
  doubleNormalize0to360(&theLST);
  theLST *= DEGREES_TO_RADIANS;
  return(theLST);
}

/*
  Return Local Sidereal Time
*/
static double lST(void)
{
  if (unlikely(needNewTime)) {
    newPosition();
    tJD = getTJD();
    myLST = lSTAtTJD(tJD);
    needNewTime = FALSE;
  }
  return(myLST);
}

/*
  refraction(el) returns the refraction correction (which should be added to
  the free-space elevation).   Both the argument and return value are in
  radians.   The formula is derived from Saemundsson, Sky & Tel. Vol 72,
  page 70 (1982).   It should be accurate to within 4 arc seconds.
*/
float refraction(float elevation)
{
  float refrac, denom;

  denom = elevation + 8.9186e-2;
  if (denom > 0.0) {
    denom = tanf(elevation + (3.1376e-3/denom));
    if (likely(denom != 0.0))
      refrac = 2.967059e-4/denom;
    else
      refrac = 0.0;
  } else
    refrac = 0.0;
  return(refrac);
}

/*
  Convert RA & Dec to altitude and azimuth.

  If abort = FALSE, the function will calculate Az and ZA even if it is clear that
  the object will not appear in the display window.   This is done for the Sun, because
  even if the Sun is not displayable, we most know it's Az and ZA if the Moon illumination
  is to be displayed properly.
*/
static int azZA(double ra, double sinDec, double cosDec, double *az, double *zA,
		int abort)
{
  static double localHourAngle, localZA, azimuthT, cosAzimuth, invSinZA, cosHA;

  if (abort) {
    if (northernHemisphere) {
      if (sinDec < sinLatitudeCutoff)
	return(FALSE);
    } else {
      if (sinDec > sinLatitudeCutoff)
	return(FALSE);
    }
  }
  localHourAngle = lST() - ra;
  cosHA = cos(localHourAngle);
  localZA = acos( sinLatitude*sinDec + cosLatitude*cosDec*cosHA );
  if ((localZA >= maximumDisplayZA) && abort)
    return(FALSE);
  else if (unlikely(localZA == 0.0))
    *az = 0.0;
  else {
    invSinZA = 1.0/sin(localZA);
    azimuthT = -asin(cosDec*sin(localHourAngle)*invSinZA);
    cosAzimuth = (cosLatitude*sinDec - sinLatitude*cosDec*cosHA)*invSinZA;
    
    if (cosAzimuth < 0.0) {
      if (azimuthT < 0.0)
	*az = M_PI - azimuthT;
      else
	*az = acos(cosAzimuth);
    } else if (azimuthT < 0.0)
      *az = M_2PI + azimuthT;
    else
      *az = azimuthT;
  }
  *zA = localZA;
  return(TRUE);
}

/*
  Convert from Az-ZA coordinates to theta-eta coordinates, which are used for the
  Transverse Mercator projection.   Azimuth is first rotated, and then the
  sphere is rotated by 90 degrees, so that Az = 0 is on the equator in the theta-eta
  system.
*/
static int thetaEta(double az, double zA, double *theta, double *eta)
{
  double cosTheta, thetaT, cosEta, localEta, sinZA;

  az -= centerAzD;
  sinZA = sin(zA);
  localEta = asin(sinZA*cos(az));
  if (unlikely(localEta == 0.0))
    *theta = 0.0;
  else {
    cosEta = cos(localEta);
    thetaT = -asin(sinZA*sin(az)/cosEta);
    cosTheta = cos(zA)/cosEta;
    if (cosTheta < 0.0) {
      if (thetaT < 0.0)
	*theta = piMinusThetaOffset - thetaT;
      else
	*theta = acos(cosTheta) - thetaOffset;
    } else if (thetaT < 0.0)
      *theta = twoPiMinusThetaOffset + thetaT;
    else
      *theta = thetaT - thetaOffset;
  }
  *eta = localEta;
  if (maxTheta < *theta)
    return(FALSE);
  else
    return(TRUE);
}

static int invThetaEta(float theta, float eta, float *az, float *zA)
{
  float cosAz, azT, cosEl, localEl, tempAzT;

  theta += thetaOffset;
  floatNormalize0to2pi(&theta);
  localEl = asinf(cosf(eta)*cosf(theta));
  if (unlikely(localEl == M_HALF_PI)) {
    *az = 0.0;
  } else {
    cosEl = cosf(localEl);
    azT = -asin(cosf(eta)*sin(theta)/cosEl);
    cosAz = sin(eta)/cosEl;
    if (cosAz < -1.0) {
      cosAz = -1.0;
    } else if (cosAz > 1.0) {
      cosAz = 1.0;
    } if (cosAz < 0.0) {
      if (azT < 0.0) {
	azT = M_PI - azT;
      } else {
	azT = acos(cosAz);
      }
    }
    azT += centerAz;
    tempAzT = azT;
    floatNormalize0to2pi(&azT);
    *az = azT;
  }
  *zA = M_HALF_PI - localEl;
  return(TRUE);
}

/*
  Convert the Mercator Projection coordinates to screen pixel
  coordinates.
*/
static void mercatorToPixels(float mX, float mY, int *x, int *y)
{
  *x = (int)((mX+mercatorOffset)*mercatorScale);
  *y = ((int)(mY*mercatorScale))+yBorder+yTopLabelSkip;
}

static void mercatorToPixelsFloat(float mX, float mY, float *x, float *y)
{
  *x = (mX+mercatorOffset) * mercatorScale;
  *y =  mY*mercatorScale + (float)(yBorder+yTopLabelSkip);
}

static void pixelsToMercator(int x, int y, float *mX, float *mY)
{
  *mX = (((float)x)/mercatorScale) - mercatorOffset;
  *mY = ((float)(y - yBorder - yTopLabelSkip))/mercatorScale;
}

/*
  This function performs the Mercator Projection
  The exact mercator projection would be

  *x = logf(tanf(pi*0.25 + phi*0.5));

  For computational efficiency, I'm approximating it by a 5th degree Taylor
  expansion, which should be accurate to better than one pixel on a 480x640
  display.
*/
static void mercator(float phi, float *x)
{
  float phisq;

  phisq = phi*phi;
  *x = ((mC5*phisq + mC3)*phisq + mC1)*phi;
}

static void invMercator(float x, float *phi)
{
  *phi = 2.0*(atanf(expf(x)) - M_PI*0.25);
}

/*
  Take the B-V color index, and return an index to one of the GCs used to
  plot stars in color.
*/
static int colorIndex(float bV, float mag)
{
  if (mag > colorThreshold1)
    return(1);
  else
    if (bV < colorThreshold2)
      return(0);
    else if (bV < colorThreshold3)
      return(1);
    else if (bV < colorThreshold4)
      return(2);
    else if (bV < colorThreshold5)
      return(3);
    else
      return(4);
}

/*
  The following routine is passed the entry for a given star, and it
  determines if the star is part of a constellation's stick figure.
  We need to know that, because when the constellation stick figures
  are being displayed, we want all the starts that are part of the stick
  figure to be displayed, even if they are fainter than the current
  limiting magnitude for displaying stars.
*/
static void inConstellation(objectListEntry *entry) {
  register int i;
  double rSquared;
  double xx, yy, rA, dec;
  constellation *thisConst;

  rA = entry->rAJ2000;
  dec = entry->decJ2000;
  thisConst = constellations;
  while (thisConst != NULL) {
    for (i = 0; i < thisConst->nPoints; i++) {
      if (thisConst->starNeeded[i]) {
	xx = rA - thisConst->rA[i];
	if (xx > M_PI)
	  xx -= M_2PI;
	else if (xx < -M_PI)
	  xx += M_2PI;
	xx *= xx;
	yy = dec - thisConst->dec[i];
	yy *= yy;
	rSquared = xx*xx + yy*yy;
	if (rSquared > 1.0)
	  break;
	else if (rSquared < 1.0e-9) {
	  thisConst->starNeeded[i] = FALSE;
	  entry->display = TRUE;
	  return;
	}
      }
    }
    thisConst = thisConst->next;
  }
  return;
}

void readMeteorShowers(void)
{
  int nRead;
  int readingRadiantLines;
  char fileName[MAX_FILE_NAME_SIZE];
  FILE *data;
  meteorShower *lastShower = NULL;

  sprintf(fileName, "%s/meteorShowers", dataDir);
  data = fopen(fileName, "r");
  if (unlikely(data == NULL)) {
    perror(fileName);
    exit(ERROR_EXIT);
  }
  readingRadiantLines = FALSE;
  while (!feof(data)) {
    char fullName[40];
    char threeLetterName[4];
    int startMonth, startDay, i;
    int endMonth, endDay;
    int maxMonth, maxDay;
    int eclipticLong;
    int radRA, radDec;
    int vInf, rMonth, rDay, rRA, rDec;
    float r;
    int zHR;
    int nRadiantPositions;
    meteorShower *thisShower;

    nRead = fscanf(data, "%s %s %d %d %d %d %d %d %d %d %d %d %f %d %d",
		   fullName, threeLetterName,
		   &startMonth, &startDay, &endMonth, &endDay, &maxMonth, &maxDay,
		   &eclipticLong, &radRA, &radDec, &vInf, &r, &zHR, &nRadiantPositions);
    if (nRead == 15) {
      thisShower = (meteorShower *)malloc(sizeof(meteorShower));
      if (unlikely(thisShower == NULL)) {
	perror("thisShower malloc");
	exit(ERROR_EXIT);
      }
      for (i = 0; i < strlen(fullName); i++)
	if (fullName[i] == '_')
	  fullName[i] = ' ';
      thisShower->fullName = malloc(strlen(fullName)+1);
      if (unlikely(thisShower->fullName == NULL)) {
	perror("malloc of meteor shower fullName");
	exit(ERROR_EXIT);
      }
      strcpy(thisShower->fullName, fullName);
      strcpy(thisShower->threeLetterName, threeLetterName);
      thisShower->startMonth = startMonth; thisShower->startDay = startDay;
      thisShower->endMonth   = endMonth;   thisShower->endDay   = endDay;
      thisShower->maxMonth   = maxMonth;   thisShower->maxDay   = maxDay;
      thisShower->eclipticLong = eclipticLong;
      thisShower->radRA = radRA; thisShower->radDec = radDec;
      thisShower->vInf = vInf; thisShower->r = r; thisShower->zHR = zHR;
      thisShower->nRadiantPositions = nRadiantPositions;
      thisShower->radiantPositions = (radiantPosition *)malloc(nRadiantPositions *
							       sizeof(radiantPosition));
      if (unlikely(thisShower->radiantPositions == NULL)) {
	perror("malloc of radiantPositions");
	exit(ERROR_EXIT);
      }
      for (i = 0; i < nRadiantPositions; i++) {
	nRead = fscanf(data, "%d %d %d %d", &rMonth, &rDay, &rRA, &rDec);
	if (likely(nRead == 4)) {
	  thisShower->radiantPositions[i].month = rMonth;
	  thisShower->radiantPositions[i].day   = rDay;
	  thisShower->radiantPositions[i].rA    = (double)rRA * DEGREES_TO_RADIANS;
	  thisShower->radiantPositions[i].dec   = (double)rDec * DEGREES_TO_RADIANS;
	} else {
	  fprintf(stderr, "Wrong number of parameters on meteor radiant line");
	  exit(ERROR_EXIT);
	}
      }
      thisShower->next = NULL;
      if (lastShower == NULL)
	meteorShowers = thisShower;
      else
	lastShower->next = thisShower;
      lastShower = thisShower;
    }
  }
  fclose(data);
}

/*
  This function builds a long string containing the entire lower case
  Greek alphabet.   Each character is followed by a zero, for easy
  of printing.
 */
void makeGreekAlphabet(void)
{
  int i, j, code;

  for (i = 0; i < 24; i++) {
    if (i < 17)
      j = i;
    else
      j = i+1;
    code = 0x3b1 + j;
    greekAlphabet[3*i]     = 0xc0 | (code >> 6);
    greekAlphabet[3*i + 1] = 0x80 | (code & 0x3f);
    greekAlphabet[3*i + 2] = 0x00;
  }
}

/*
  The following function reads the individual entries in a star catalog,
  and puts them in a linked list.
*/
void readStarCatalog(int fD, int matchStarNames)
{
  static int firstCall = TRUE;
  int nRead;
  int nStarsRead = 0;
  float tempMagScale = 1.0;
  hipparcosEntry entry;
  objectListEntry *lastEntry = NULL;
  objectListEntry *newEntry = NULL;
  starNameEntry *lastPermanentNameEntry = NULL;
  starNameTempList *lastNameEntry = NULL;

  if (!firstCall) {
    /*
      If this function has been called before, the first step is to free
      up the memory allocated during the previous call.
    */
    free(darkGreyPoints);
    free(greyPoints);
    free(whitePoints);
    newEntry = objectListRoot;
    while (newEntry != NULL) {
      lastEntry = newEntry;
      newEntry = newEntry->forwardPointer;
      free(lastEntry);
    }
    objectListRoot = NULL;
  } else
    makeGreekAlphabet();
  firstCall = nDarkGreyPoints = nGreyPoints = nWhitePoints = 0;
  faintestStar = -30.0;
  do {
    nRead = read(fD, &entry, sizeof(hipparcosEntry));
    if (nRead == sizeof(hipparcosEntry)) {
      /* Allocate a new entry to store this star */
      newEntry = (objectListEntry *)malloc(sizeof(objectListEntry));
      if (unlikely(newEntry == NULL)) {
	perror("allocating new object entry");
	exit(ERROR_EXIT);
      }
      /* Insert the new entry into the doubly linked list of objects */
      if (objectListRoot == NULL)
	/* First entry */
	objectListRoot = newEntry;
      else
	lastEntry->forwardPointer = newEntry;
      newEntry->forwardPointer = NULL;
      newEntry->hipNumber      = entry.number;
      newEntry->rAJ2000        = entry.rAJ2000;
      newEntry->decJ2000       = entry.decJ2000;
      newEntry->muRA           = entry.muRA;
      newEntry->muDec          = entry.muDec;
      newEntry->parallax       = entry.parallax;
      newEntry->sinDec         = sin(entry.decJ2000);
      newEntry->cosDec         = cos(entry.decJ2000);
      newEntry->mag            = ((float)entry.mag)/100.0;
      if (newEntry->mag > faintestStar)
	faintestStar = newEntry->mag;
      newEntry->color          = ((float)entry.color)/1000.0;
      newEntry->display        = FALSE;
      lastEntry = newEntry;
      nStarsRead++;
    }
  } while (nRead == sizeof(hipparcosEntry));
  if (matchStarNames) {
    int found;
    starNameTempList *nameEntry;
    starNameEntry *permanentNameEntry;
    
    nameEntry = starNameTempListRoot;
    lastNameEntry = NULL;
    while (nameEntry != NULL) {
      newEntry = objectListRoot;
      found = FALSE;
      while ((!found) && (newEntry != NULL)) {
	if (nameEntry->record.nHip == newEntry->hipNumber) {
	  /* Found a match in the temporary name list */
	  found = TRUE;
	  permanentNameEntry = (starNameEntry *)malloc(sizeof(starNameEntry));
	  /* Make a name entry in the permanent name list */
	  if (unlikely(permanentNameEntry == NULL)) {
	    perror("permanetNameEntry");
	    exit(ERROR_EXIT);
	  }
	  permanentNameEntry->offset  = nameEntry->record.offset;
	  permanentNameEntry->nameLen = nameEntry->record.nameLen;
	  permanentNameEntry->nGreek  = nameEntry->record.nGreek;
	  permanentNameEntry->flags   = nameEntry->record.flags;
	  permanentNameEntry->hip     = newEntry;
	  permanentNameEntry->next    = NULL;
	  nameEntry->found = TRUE;
	  /* Now enqueue the new, permanent entry */
	  if (lastPermanentNameEntry == NULL)
	    starNameEntryRoot = permanentNameEntry;
	  else
	    lastPermanentNameEntry->next = permanentNameEntry;
	  lastPermanentNameEntry = permanentNameEntry;
	} else
	  newEntry = newEntry->forwardPointer;
      }
      lastNameEntry = nameEntry;
      nameEntry = nameEntry->next;
    }
    /* Free up temp name linked list */
    nameEntry = starNameTempListRoot;
    while (nameEntry != NULL) {
      lastNameEntry = nameEntry;
      nameEntry = nameEntry->next;
      free(lastNameEntry);
    }
  }
  dprintf("nStarsRead = %d\n", nStarsRead);
  newEntry = objectListRoot;
  if (magScale > 1.0) {
    tempMagScale = magScale;
    magScale = 1.0;
  }
  while (newEntry != NULL) {
    if (newEntry->mag*magScale > DARK_GREY_LIMIT)
      nDarkGreyPoints++;
    else if (newEntry->mag*magScale > GREY_LIMIT)
      nGreyPoints++;
    else if (newEntry->mag*magScale > WHITE_LIMIT)
      nWhitePoints++;
    newEntry = newEntry->forwardPointer;
  }
  if (magScale > 1.0)
    magScale = tempMagScale;
  darkGreyPoints = (GdkPoint *)malloc(nDarkGreyPoints*sizeof(GdkPoint));
  if (unlikely(darkGreyPoints == NULL)) {
    perror("malloc of darkGreyPoints");
    exit(ERROR_EXIT);
  }
  greyPoints = (GdkPoint *)malloc(nGreyPoints*sizeof(GdkPoint));
  if (unlikely(greyPoints == NULL)) {
    perror("malloc of greyPoints");
    exit(ERROR_EXIT);
  }
  whitePoints = (GdkPoint *)malloc(nWhitePoints*sizeof(GdkPoint));
  if (unlikely(whitePoints == NULL)) {
    perror("malloc of whitePoints");
    exit(ERROR_EXIT);
  }
  dprintf("nWhitePoints = %d nGreyPoints = %d nDarkGreyPoints = %d\n",
	 nWhitePoints, nGreyPoints, nDarkGreyPoints);
  close(fD);
}

/*
  This function plots the objects outside our solar system.
*/
static void plotFixedObjects(void)
{
  int mercXLow, mercXHigh, mercYLow, mercYHigh;
  int x, y, fD;
  float mag, color, darkGreyLimit, greyLimit, whiteLimit;
  float mX, mY;
  double az, zA, theta, eta;
  char fileName[MAX_FILE_NAME_SIZE];
  objectListEntry *currentEntry;

  if (haveFaintStars && showFaintStars && (!showingFaintStars)) {
    /*
      The faint star catalog is available, and should be show, but it is
      not currently being shown, so we should switch to the faint star
      catalog.
    */
    sprintf(fileName, "%s/faintStars", dataDir);
    fD = open(fileName, O_RDONLY);
    if (fD >= 0)
      readStarCatalog(fD, FALSE);
    showingFaintStars = TRUE;
  }
  nDarkGreyPoints = nGreyPoints = nWhitePoints = 0;
  darkGreyLimit = DARK_GREY_LIMIT/magScale;
  greyLimit = GREY_LIMIT/magScale;
  whiteLimit = WHITE_LIMIT/magScale;
  mercXLow  = xBorder+xLeftLabelSkip;
  mercXHigh = xBorder+xLeftLabelSkip+plotWidth;
  mercYLow  = yBorder+yTopLabelSkip;
  mercYHigh = displayHeight;
  currentEntry = objectListRoot;
  while (currentEntry != NULL) {
    mag = currentEntry->mag;
    color = currentEntry->color;
    if ((mag < limitingMag) || (displayConstellations && (currentEntry->display))) {
      /* The object is bright enough to be displayed */
      if (azZA(currentEntry->rAJ2000, currentEntry->sinDec,
	       currentEntry->cosDec, &az, &zA, TRUE))
	if (thetaEta(az, zA, &theta, &eta)) {
	  mY = (float)theta;
	  mercator((float)eta, &mX);
	  mercatorToPixels(mX, mY, &x, &y);
	  if ((x > mercXLow) && (x < mercXHigh) && (y > mercYLow) && (y < mercYHigh)) {
	    if (mag > darkGreyLimit) {
	      darkGreyPoints[nDarkGreyPoints].x = x;
	      darkGreyPoints[nDarkGreyPoints++].y = y;
	    } else if (mag > greyLimit) {
	      greyPoints[nGreyPoints].x = x;
	      greyPoints[nGreyPoints++].y = y;
	    } else if (mag > whiteLimit) {
	      whitePoints[nWhitePoints].x = x;
	      whitePoints[nWhitePoints++].y = y;
	    } else {
	      int index;
	      
	      index = (int)(2.0*(mag*magScale + 1.0));
	      if (index < 0)
		index = 0;
	      if (index <= 11)
		gdk_draw_arc(pixmap, starGC[colorIndex(color, mag)], TRUE,
			     x-(starSize[index]/2), y-(starSize[index]/2),
			     starSize[index], starSize[index], 0, FULL_CIRCLE);
	      else {
		gdk_draw_point(pixmap, gC[OR_WHITE], x, y);
		gdk_draw_point(pixmap, gC[OR_WHITE], x+1, y);
	      }
	    }
	  }
	}
    }
    currentEntry = currentEntry->forwardPointer;
  }
  if (nDarkGreyPoints > 0)
    gdk_draw_points(pixmap, gC[OR_DARK_GREY], darkGreyPoints, nDarkGreyPoints);
  if (nGreyPoints > 0)
    gdk_draw_points(pixmap, gC[OR_GREY], greyPoints, nGreyPoints);
  if (nWhitePoints > 0)
    gdk_draw_points(pixmap, gC[OR_WHITE], whitePoints, nWhitePoints);
}

void makeTimeString(char *string, int seconds)
{
  int sHH, sMM, sSS;
  float sidereal;

  sidereal = lST()/HOURS_TO_RADIANS;
  sHH = (int)sidereal;
  sMM = (int)(60.0*(sidereal-(float)sHH));
  sSS = (int)(3600.0*(sidereal-(float)sHH - ((float)sMM)/60.0));
  if (useJulianTime)
    if (seconds)
      sprintf(string, "TJD %11.3f  LST %02d:%02d:%02d",
	      julianDate, sHH, sMM, sSS);
    else
      sprintf(string, "TJD %11.3f  LST %02d:%02d",
	      julianDate, sHH, sMM);
  else if (useCurrentTime)
    if (seconds)
      sprintf(string, "UT %02d:%02d:%02d  LST %02d:%02d:%02d",
	      gMT->tm_hour, gMT->tm_min, (int)gMT->tm_sec, sHH, sMM, sSS);
    else
      sprintf(string, "UT  %02d:%02d    LST  %02d:%02d",
	      gMT->tm_hour, gMT->tm_min, sHH, sMM);
  else {
    float lYear;

    if (cYear > 0)
      lYear = cYear;
    else
      lYear = cYear-1.0;
    if (seconds)
      sprintf(string, "%02.0f/%02.0f/%4.0f  UT %02d:%02d:%02d  LST %02d:%02d:%02d",
	      cMonth, cDay, lYear, gMT->tm_hour, gMT->tm_min, (int)gMT->tm_sec,
	      sHH, sMM, sSS);
    else
      sprintf(string, "%02.0f/%02.0f/%4.0f  UT %02d:%02d  LST %02d:%02d",
	      cMonth, cDay, lYear, gMT->tm_hour, gMT->tm_min, sHH, sMM);
  }
}

/*
  Print the UT and LST at the top of the plot.
*/
static void showTime(void)
{
  int tWidth, tHeight;
  char labelString[100];

  makeTimeString(labelString, FALSE);
  if (useCurrentTime && !useGPSD) {
    renderPangoText(labelString, OR_WHITE, MEDIUM_PANGO_FONT,
		    &tWidth, &tHeight, pixmap, 5, 12, 0.0, FALSE, 0);
    yTopLabelSkip = tHeight - 8;
  } else {
    if (useCurrentTime)
      renderPangoText(labelString, OR_WHITE, SMALL_PANGO_FONT,
		      &tWidth, &tHeight, pixmap, 5, 10, 0.0, FALSE, 0);
    else
      renderPangoText(labelString, OR_RED, SMALL_PANGO_FONT,
		      &tWidth, &tHeight, pixmap, 5, 10, 0.0, FALSE, 0);
    yTopLabelSkip = tHeight - 6;
  }
  tWidth += 5;
  if (gPSLocation && useGPSD) {
    int gLatDD, gLatMM, gLongDD, gLongMM;
    double gLatSS, gLongSS, gLatDeg, gLongDeg;

    gLatDeg = latitude/DEGREES_TO_RADIANS;
    gLatDD = (int)gLatDeg;
    gLatMM = (int)((gLatDeg-(double)gLatDD)*60.0);
    gLatSS = (gLatDeg - (double)gLatDD - ((double)gLatMM)/60.0)*3600.0;
    if (latitude < 0.0) {
      gLatMM *= -1;
      gLatSS *= -1.0;
    }
    gLongDeg = longitude/DEGREES_TO_RADIANS;
    gLongDD = (int)gLongDeg;
    gLongMM = (int)((gLongDeg-(double)gLongDD)*60.0);
    gLongSS = (gLongDeg - (double)gLongDD - ((double)gLongMM)/60.0)*3600.0;
    if (longitude < 0.0) {
      gLongMM *= -1;
      gLongSS *= -1.0;
    }
    sprintf(labelString, "Lat  %02d:%02d   Lon  %02d:%02d",
	    gLatDD, gLatMM,  gLongDD, gLongMM);
  } else if (useGPSD) {
    sprintf(labelString, "%s (No phone pos.)",
	    locationName);
  } else
    sprintf(labelString, "%s",
	    locationName);
  if (useCurrentTime && !useGPSD)
    renderPangoText(labelString, OR_WHITE, MEDIUM_PANGO_FONT,
		    &tWidth, &tHeight, pixmap, (displayWidth+tWidth)/2,
		    12, 0.0, TRUE, 0);
  else
    renderPangoText(labelString, OR_WHITE, SMALL_PANGO_FONT,
		    &tWidth, &tHeight, pixmap, (displayWidth+tWidth)/2,
		    10, 0.0, TRUE, 0);
}

/*
  Return the RA and Dec of a pair of ecliptic coordinates (in radians).
*/
static void ecliptic(float lambda, double *ra, double *dec, double *cDec)

{
  static int   firstCall = TRUE;
  static float sinEta, cosEta, cosDec;
  float tRA, tDec, cosRA, sinRA;
  
  if (unlikely(firstCall)) {
    sinEta = sinf(ETA);
    cosEta = cosf(ETA);
    firstCall = FALSE;
  }
  tDec = asinf(sinf(lambda)*sinEta);
  cosDec = cosf(tDec);
    if (likely(cosDec != 0.0)) {
    cosRA = cosf(lambda)/cosDec;
    sinRA = cosEta*sinf(lambda)/cosDec;
  } else {
    cosRA = -1.0;
    sinRA = 0.0;
  }
  tRA = atan2f(sinRA, cosRA);
  *ra = (double)tRA;
  *dec = (double)tDec;
  *cDec = (double)cosDec;
}

/*
  Return the RA and Dec of a pair of galactic coordinates (in radians).
*/
static void galactic(float lambda, double *ra, double *dec, double *cDec)

{
  static int   firstCall = TRUE;
  static float sinEta, cosEta, cosDec, rAOffset, lambdaOffset;
  float tRA, tDec, cosRA, sinRA;
  
  if (unlikely(firstCall)) {
    sinEta = sinf(ETAG);
    cosEta = cosf(ETAG);
    rAOffset = 282.25*DEGREES_TO_RADIANS;
    lambdaOffset = -33.0*DEGREES_TO_RADIANS;
    firstCall = FALSE;
  }
  lambda += lambdaOffset;
  tDec = asinf(sinf(lambda)*sinEta);
  cosDec = cosf(tDec);
    if (likely(cosDec != 0.0)) {
    cosRA = cosf(lambda)/cosDec;
    sinRA = cosEta*sinf(lambda)/cosDec;
  } else {
    cosRA = -1.0;
    sinRA = 0.0;
  }
  tRA = atan2f(sinRA, cosRA);
  *ra = (double)tRA + rAOffset;
  *dec = (double)tDec;
  *cDec = cosDec;
}

/*
  Draw a red line depicting the ecliptic plane.
*/
static void plotEcliptic(void)
#define N_ECLIPTIC_STEPS 360
{
  static int firstCall = TRUE;
  static float eclipticStep;
  int i, targetX, targetY;
  int offset = 0;
  float mX, mY;
  float lambda = 0.0;
  double ra, dec, cDec, za, az;
  double eta, theta;
  GdkPoint eclipticPoints[N_ECLIPTIC_STEPS+1];
  GdkGC *eGC;
                                                                                                            
  if (firstCall) {
    eclipticStep = M_2PI/(double)N_ECLIPTIC_STEPS;
    firstCall = FALSE;
  }
  if (chineseColorScheme)
    eGC = gC[OR_YELLOW];
  else
    eGC = gC[OR_RED];
  for (i = 0; i < N_ECLIPTIC_STEPS+1; i++) {
    ecliptic(lambda, &ra, &dec, &cDec);
    if (azZA(ra, sin(dec), cDec, &az, &za, TRUE))
      if (thetaEta(az, za, &theta, &eta)) {
	mY = (float)theta;
	mercator((float)eta, &mX);
	mercatorToPixels(mX, mY, &targetX, &targetY);
	if ((targetX > xBorder+xLeftLabelSkip)
	    && (targetX < xBorder+xLeftLabelSkip+plotWidth)
	    && (targetY > yBorder+yTopLabelSkip)
	    && (targetY < displayHeight)) {
	  if ((i % 5) == 0)
	    gdk_draw_arc(pixmap, eGC, TRUE,
			 targetX-1, targetY-1,
			 3, 3, 0, FULL_CIRCLE);
	  eclipticPoints[offset].x = targetX;
	  eclipticPoints[offset].y = targetY;
	  offset++;
	}
      }
    lambda += eclipticStep;
  }
  if (offset > 1)
    gdk_draw_points(pixmap, eGC, eclipticPoints, offset);
}

/*
  Draw a green line depicting the galactic plane.
*/
static void plotGalactic(void)
#define N_GALACTIC_STEPS 360
{
  static int firstCall = TRUE;
  static float galacticStep;
  int i, targetX, targetY;
  int offset = 0;
  float mX, mY;
  float lambda = 0.0;
  double ra, dec, cDec, za, az;
  double eta, theta;
  GdkPoint galacticPoints[N_GALACTIC_STEPS+1];
                                                                                                            
  if (firstCall) {
    galacticStep = M_2PI/(double)N_GALACTIC_STEPS;
    firstCall = FALSE;
  }
  for (i = 0; i < N_GALACTIC_STEPS+1; i++) {
    galactic(lambda, &ra, &dec, &cDec);
    if (azZA(ra, sin(dec), cDec, &az, &za, TRUE))
      if (thetaEta(az, za, &theta, &eta)) {
	mY = (float)theta;
	mercator((float)eta, &mX);
	mercatorToPixels(mX, mY, &targetX, &targetY);
	if ((targetX > xBorder+xLeftLabelSkip)
	    && (targetX < xBorder+xLeftLabelSkip+plotWidth)
	    && (targetY > yBorder+yTopLabelSkip)
	    && (targetY < displayHeight)) {
	  if ((i % 5) == 0)
	    gdk_draw_arc(pixmap, gC[OR_BLUE_GREEN], TRUE,
			 targetX-1, targetY-1,
			 3, 3, 0, FULL_CIRCLE);
	  galacticPoints[offset].x = targetX;
	  galacticPoints[offset].y = targetY;
	  offset++;
	}
      }
    lambda += galacticStep;
  }
  if (offset > 1)
    gdk_draw_points(pixmap, gC[OR_BLUE_GREEN], galacticPoints, offset);
}

/*
  Draw a grey line depicting the celestial equator.
*/
static void plotEquator(void)
#define N_EQUATOR_STEPS 360
{
  static int firstCall = TRUE;
  static double equatorStep;
  int i, targetX, targetY;
  int offset = 0;
  float mX, mY;
  double za, az;
  double eta, theta;
  double lambda = 0.0;
  GdkPoint equatorPoints[N_EQUATOR_STEPS+1];
  GdkGC *eGC;
                                                                                                            
  if (firstCall) {
    equatorStep = M_2PI/(double)N_EQUATOR_STEPS;
    firstCall = FALSE;
  }
  if (chineseColorScheme)
    eGC = gC[OR_RED];
  else
    eGC = gC[OR_EQUATOR];
  for (i = 0; i < N_EQUATOR_STEPS+1; i++) {
    if (azZA(lambda, 0.0, 1.0, &az, &za, TRUE))
      if (thetaEta(az, za, &theta, &eta)) {
	mY = (float)theta;
	mercator((float)eta, &mX);
	mercatorToPixels(mX, mY, &targetX, &targetY);
	if ((targetX > xBorder+xLeftLabelSkip)
	    && (targetX < xBorder+xLeftLabelSkip+plotWidth)
	    && (targetY > yBorder+yTopLabelSkip)
	    && (targetY < displayHeight)) {
	  if ((i % 5) == 0)
	    gdk_draw_arc(pixmap, eGC, TRUE,
			 targetX-1, targetY-1,
			 3, 3, 0, FULL_CIRCLE);
	  equatorPoints[offset].x = targetX;
	  equatorPoints[offset].y = targetY;
	  offset++;
	}
      }
    lambda += equatorStep;
  }
  if (offset > 1)
    gdk_draw_points(pixmap, eGC, equatorPoints, offset);
}

/*
  Calculate the angle kappa though which the moon's terminator swings
  (actually 1/2 that angle is returned).
*/
static float terminatorRadius(float P)
{
  float appr;

  appr =
    1.5709                                +
   -1.57949   * P                         +
   -1.70266   * P * P                     +
   -5.049     * P * P * P                 +
   15.7448    * P * P * P * P             +
  -59.3318    * P * P * P * P * P         +
   96.033     * P * P * P * P * P * P     +
  -45.3849    * P * P * P * P * P * P * P;
  return(appr);
}

/*
  Calculate the direction of destination, when seen from source.   Used
  to rotate the moon drawing so that it properly faces the sun.
*/
static float greatCircleDirection(float sLong, float sLat, float dLong, float dLat)
{
  return(-atan2f(sinf(sLong-dLong),cosf(sLat)*tanf(dLat) - sinf(sLat)*cosf(sLong-dLong)));
}

/*
  Draw the moon, with the proper phase and orientation.
*/
static void drawMoon(float moonSize, float illum, float sunAngle, int x, int y,
		     int image, int full, int blue)
{
  int iMoonSize;
  int nPoints = 0;
  float angle = 90.0;
  float phaseSign; /* Controls whether a cresent or gibbous moon is drawn */
  float kappa, rad, offset;
  float angleStep = 5.0;
  GdkPoint points[520];
  GdkPixmap *tMoonImage;

  iMoonSize = (int)(moonSize * 2.0);
  if (image) {
    int w, h;

    if (imAPostageStamp) {
      angleStep = 1.0;
      if (northernHemisphere)
	tMoonImage = moonImage210;
      else
	tMoonImage = moonImage210Flipped;
      gdk_drawable_get_size(moonImage210, &w, &h);
    } else {
      if (northernHemisphere) {
	if (full) {
	  if (blue)
	    tMoonImage = blueMoonImage;
	  else
	    tMoonImage = fullMoonImage;
	} else
	  tMoonImage = moonImage;
      } else {
	if (full) {
	  if (blue)
	    tMoonImage = blueMoonImageFlipped;
	  else
	    tMoonImage = fullMoonImageFlipped;
	} else
	  tMoonImage = moonImageFlipped;
      }
      gdk_drawable_get_size(moonImage, &w, &h);
    }
    gdk_draw_drawable(pixmap, gC[OR_BLUE], tMoonImage, 0, 0, x - w/2, y - h/2, w, h);
    illum = 1.0 - illum;
    sunAngle = M_PI + sunAngle;
    moonSize = w/2 + 1.075;
  }
  if ((illum > 0.98) && (!image)) {
    /* The moon is so close to full, just draw a circle) */
    if (blue && full)
      gdk_draw_arc(pixmap, gC[OR_BLUE_GREEN], TRUE,
		   x-(iMoonSize/2), y-(iMoonSize/2),
		   iMoonSize, iMoonSize, 0, FULL_CIRCLE);
    else
      gdk_draw_arc(pixmap, gC[OR_WHITE], TRUE,
		   x-(iMoonSize/2), y-(iMoonSize/2),
		   iMoonSize, iMoonSize, 0, FULL_CIRCLE);
  } else {
    /* First draw a black circle of the right diameter to obscure backround stars */
    if (!image) {
      if (illum > 0.10)
	gdk_draw_arc(pixmap, drawingArea->style->black_gc, TRUE,
		     x-(iMoonSize/2), y-(iMoonSize/2),
		     iMoonSize, iMoonSize, 0, FULL_CIRCLE);
      else
	/* Draw Earthshine */
	gdk_draw_arc(pixmap, gC[OR_DARK_GREY], TRUE,
		     x-(iMoonSize/2), y-(iMoonSize/2),
		     iMoonSize, iMoonSize, 0, FULL_CIRCLE);
    }
    if (illum > 0.02) {
      float dx, dy, sRot, cRot;

      sRot = sinf(sunAngle);
      cRot = cosf(sunAngle);
      angle = -90.0;
      while (angle <= 90.0) {
	dx = moonSize*cosf(angle*DEGREES_TO_RADIANS);
	dy = moonSize*sinf(angle*DEGREES_TO_RADIANS);
	points[nPoints].x = x + sRot*dy + cRot*dx;
	points[nPoints].y = y + cRot*dy - sRot*dx;
	nPoints++;
	angle += angleStep;
      }
      if ((illum < 0.47) || (illum > 0.53)) {
	/* If the illumination is very close to 50%, just draw a semicircle */
	if (illum < 0.5) {
	  phaseSign = 1.0;
	  kappa = terminatorRadius(illum);
	} else {
	  phaseSign = -1.0;
	  kappa = terminatorRadius(1.0-illum);
	}
	rad = 1.0 / sinf(kappa);
	if (imAPostageStamp)
	  angleStep = kappa/75.0;
	else
	  angleStep = kappa/15.0;
	angle = kappa;
	offset = phaseSign*rad*cosf(kappa);
	while (angle > -kappa) {
	  dx = moonSize*(-offset + phaseSign*rad*cosf(angle));
	  dy = moonSize*(rad*sinf(angle));
	  points[nPoints].x = x + sRot*dy + cRot*dx;
	  points[nPoints].y = y + cRot*dy - sRot*dx;
	  nPoints++;
	  angle -= angleStep;
	}
      }
      if (image)
	gdk_draw_polygon(pixmap, gC[OR_BLACK], TRUE, points, nPoints);
      else if (blue && full)
	gdk_draw_polygon(pixmap, gC[OR_BLUE_GREEN], TRUE, points, nPoints);
      else
	gdk_draw_polygon(pixmap, gC[OR_WHITE], TRUE, points, nPoints);
    }
  }
}

void clearScoreBoard(void) /* Mark all pixels as unused */
{
  bzero(scoreBoard, 48000);
}

void setPixelUsed(int x, int y) /* Mark a single pixel as used */
{
  int v, h;

  v = y*60;
  h = v + (x >> 3);
  if ((h < 0) || (h >= 48000))
    return;
  scoreBoard[h] |= 1 << (x & 0x7);
}

int isPixelUsed(int x, int y) /* Check to see id a particular pixel has been used */
{
 int v, h;

  v = y*60;
  h = v + (x >> 3);
  if ((h < 0) || (h >= 48000))
    return(TRUE);
  if (scoreBoard[h] & (1 << (x & 0x7)))
    return(TRUE);
  else
    return(FALSE);
}

int regionUsed(int xlc, int yuc, int width, int height)
{
  int i, j;

  for (i = xlc; i < xlc+width; i++)
    for (j = yuc; j < yuc+height; j++)
      if (isPixelUsed(i, j))
	return(TRUE);
  return(FALSE);
}

/*
  Set all the pixels in a region as used
*/
void setRegionUsed(int xlc, int yuc, int width, int height)
{
  int i, j;

  for (i = xlc; i < xlc+width; i++)
    for (j = yuc; j < yuc+height; j++)
      setPixelUsed(i, j);
}

static void plotBayer(void)
{
  int up, x, y, stringWidth, stringHeight, color, colorGC, greekExp, gotExp;
  float mX, mY;
  double az, zA, eta, theta;
  starNameEntry *nextStar;
  char greekExpString[2];

  nextStar = starNameEntryRoot;
  while (nextStar != NULL) {
    up = azZA(nextStar->hip->rAJ2000, nextStar->hip->sinDec, nextStar->hip->cosDec,
	      &az, &zA, TRUE);
    if (up) {
      greekExp = (nextStar->flags) & 0x0f;
      if ((greekExp > 0) && (greekExp < 10)) {
	gotExp = TRUE;
	sprintf(greekExpString, "%d", greekExp);
      } else
	gotExp = FALSE;
      color = (nextStar->flags) >> 4;
      switch (color) {
      case ZODIAC:
	if (chineseColorScheme)
	  colorGC = OR_YELLOW;
	else
	  colorGC = OR_RED;
	break;
      case PTOLEMAIC:
	if (chineseColorScheme)
	  colorGC = OR_RED;
	else
	  colorGC = OR_YELLOW;
	break;
      case ASTERISM:
	colorGC = OR_WHITE;
	break;
      default:
	colorGC = OR_BLUE;
      }
      if (thetaEta(az, zA, &theta, &eta)) {
	mY = (float)theta;
	mercator((float)eta, &mX);
	mercatorToPixels(mX, mY, &x, &y);
	y -= (int)(1.5*((float)STAR_NAME_V_OFFSET));
	if ((x >= 0) && (x <= displayWidth) &&
	    (y >= 0) && (y <= displayHeight)) {
	  if ((azSpan > 0.75) && ((M_2PI - azSpan) > 0.75)) {
	    renderPangoText(&greekAlphabet[3*((nextStar->nGreek) & 0x1f)], colorGC, GREEK_FONT,
			    &stringWidth, &stringHeight, NULL, x, y+5, 0.0, TRUE, 0);
	    if ((!regionUsed(x-5-(stringHeight/2), y-(stringWidth/2),
			    stringWidth+10, stringHeight+10) &&
		 (nextStar->hip->mag < 3.5))) {
	      renderPangoText(&greekAlphabet[3*((nextStar->nGreek) & 0x1f)], colorGC, GREEK_FONT,
			      &stringWidth, &stringHeight, pixmap, x, y+5, 0.0, TRUE, 0);
	      setRegionUsed(x-5-(stringHeight/2), y-(stringWidth/2),
			    stringWidth+10, stringHeight+10);
	      if (gotExp) {
		renderPangoText(greekExpString, colorGC, TINY_GREEK_FONT,
				&stringWidth, &stringHeight, pixmap, x+9, y-2, 0.0, TRUE, 0);
		setRegionUsed(x-1-(stringHeight/2), y-(stringWidth/2)-2,
			      stringWidth+10, stringHeight+10);		
	      }
	    }
	  } else {
	    renderPangoText(&greekAlphabet[3*((nextStar->nGreek) & 0x1f)], colorGC, SMALL_PANGO_FONT,
			    &stringWidth, &stringHeight, NULL, x, y+5, 0.0, TRUE, 0);
	    if (!regionUsed(x-5-(stringHeight/2), y-(stringWidth/2),
			    stringWidth+10, stringHeight+10)) {
	      renderPangoText(&greekAlphabet[3*((nextStar->nGreek) & 0x1f)], colorGC, SMALL_PANGO_FONT,
			      &stringWidth, &stringHeight, pixmap, x, y+5, 0.0, TRUE, 0);
	      setRegionUsed(x-5-(stringHeight/2), y-(stringWidth/2),
			    stringWidth+10, stringHeight+10);
	      if (gotExp) {
		renderPangoText(greekExpString, colorGC, GREEK_FONT,
				&stringWidth, &stringHeight, pixmap, x+10, y-2, 0.0, TRUE, 0);
		setRegionUsed(x-(stringHeight/2), y-(stringWidth/2)-2,
			      stringWidth+10, stringHeight+10);		
	      }
	    }
	  }
	}
      }
    } 
    nextStar = nextStar->next;
  }
}

static void plotStarNames(void)
{
  int i, up, x, y, stringWidth, stringHeight, nameLen;
  float mX, mY;
  double az, zA, eta, theta;
  starNameEntry *nextStar;
  char scratchString[100];

  nextStar = starNameEntryRoot;
  while (nextStar != NULL) {
    nameLen = nextStar->nameLen;
    if (nameLen > 0) {
      for (i = 0; i < nameLen; i++)
	scratchString[i] = starNameString[(nextStar->offset) + i];
      scratchString[i] = '\0';
      up = azZA(nextStar->hip->rAJ2000, nextStar->hip->sinDec, nextStar->hip->cosDec,
		&az, &zA, TRUE);
      if (up) {
	if (thetaEta(az, zA, &theta, &eta)) {
	  mY = (float)theta;
	  mercator((float)eta, &mX);
	  mercatorToPixels(mX, mY, &x, &y);
	  y += STAR_NAME_V_OFFSET;
	  stringWidth = gdk_string_width(smallFont, scratchString)/2;
	  if ((x >= 0) && (x <= displayWidth) &&
	      (y >= 0) && (y <= displayHeight)) {
	    if ((azSpan > 0.75) && ((M_2PI - azSpan) > 0.75)) {
	      if ((!regionUsed(x-stringWidth, y, 2*stringWidth, 12))
		  && (nextStar->hip->mag < 2.75)) {
		gdk_draw_string(pixmap, smallFont, gC[OR_GREEN],
				x-stringWidth, y, scratchString);
		setRegionUsed(x-stringWidth, y, 2*stringWidth, 12);
	      }
	    } else {
	      renderPangoText(scratchString, OR_GREEN, SMALL_PANGO_FONT,
			      &stringWidth, &stringHeight, NULL, x, y+5, 0.0, TRUE, 0);
	      if (!regionUsed(x-5-(stringWidth/2), y-(stringHeight/2),
			      stringWidth+10, stringHeight+10)) {
		renderPangoText(scratchString, OR_GREEN, SMALL_PANGO_FONT,
				&stringWidth, &stringHeight, pixmap, x, y+5, 0.0, TRUE, 0);
		setRegionUsed(x-5-(stringWidth/2), y-(stringHeight/2),
			      stringWidth+10, stringHeight+10);
	      }
	    }
	  }
	}
      }
    }
    nextStar = nextStar->next;
  }
}

#define SOLAR_SYSTEM_CENTER_X (263)
#define SOLAR_SYSTEM_CENTER_Y (300)
#define SCHEMATIC_ORBIT_INCREMENT (23)
#define ZOOM_SOLAR_SYSTEM_IN (-10000)
#define ZOOM_SOLAR_SYSTEM_OUT (10000)
void plotComet(int x, int y, float angle)
{
  int i, j;
  float r, tAngle;

  gdk_draw_arc(pixmap, gC[OR_WHITE], TRUE,
	       x-3, y-3, 7, 7, 0, FULL_CIRCLE);
  gdk_draw_arc(pixmap, gC[OR_BLUE_GREEN], TRUE,
	       x-2, y-2, 5, 5, 0, FULL_CIRCLE);
  gdk_draw_arc(pixmap, gC[OR_GREEN], TRUE,
	       x-1, y-1, 3, 3, 0, FULL_CIRCLE);
  for (i = -2; i < 3; i++) {
    tAngle = angle + ((float)i)*M_PI/12.0;
    for (j = 5; j < 19; j += 2) {
      r = sqrtf(2.0)*(float)j;
      gdk_draw_point(pixmap, gC[OR_WHITE], x+(int)roundf(r*cosf(tAngle)), y+(int)roundf(r*sinf(tAngle)));
    }
  }
}

static void plotComets(void)
{
  float sunAz, sunZA, illum, mag;
  double sunRA, sunDec, az, zA;
  cometEphem *comet;

  needNewTime = TRUE;
  lST();
  planetInfo(dataDir, EARTH, tJD, &sunRA, &sunDec, &illum, &mag);
  azZA(sunRA, sin(sunDec), cos(sunDec), &az, &zA, FALSE);
  sunAz = (float)az;
  sunZA = (float)zA;
  if (!cometDataReadIn)
    readInCometEphemerides(dataDir);
  comet = cometRoot;
  while (comet != NULL) {
    if (comet->valid && (tJD >= comet->firstTJD) && (tJD <= comet->lastTJD)) {
      double rA, dec, theta, eta;

      getCometRADec(dataDir, comet->name, tJD, TRUE, &rA, &dec, NULL, NULL);
      if (azZA(rA, sin(dec), cos(dec), &az, &zA, TRUE)) {
	if (thetaEta(az, zA, &theta, &eta)) {
	  int x, y;
	  float mX, mY, sunAngle;

	  mY = (float)theta;
	  mercator((float)eta, &mX);
	  mercatorToPixels(mX, mY, &x, &y);
	  sunAngle = greatCircleDirection((float)az, (float)(M_HALF_PI-zA),
					  sunAz, M_HALF_PI-sunZA);
	  plotComet(x, y, M_HALF_PI+sunAngle);
	}
      }
    }
    comet = comet->next;
  }
}

static void plotPlanets(void)
{
  static int firstCall = TRUE;
  int i, stringWidth;
  int sunSize = 26;
  float illum, mag;
  double rA, dec;
  double sunRA, sunDec;
  char fileName[MAX_FILE_NAME_SIZE];

  planetInfo(dataDir, EARTH, tJD, &sunRA, &sunDec, &illum, &mag);
  if (firstCall) {
    sprintf(fileName, "%s/icons/%s.xpm", dataDir, "firstPointInAries");
    firstPointImage = gdk_pixmap_create_from_xpm(pixmap,
						 NULL, NULL, fileName);
    sprintf(fileName, "%s/icons/%s.xpm", dataDir, "MeteorRadiant");
    meteorRadiantImage[0] = gdk_pixmap_create_from_xpm(pixmap,
						       NULL, NULL, fileName);
    sprintf(fileName, "%s/icons/%s.xpm", dataDir, "WeakMeteorRadiant");
    meteorRadiantImage[1] = gdk_pixmap_create_from_xpm(pixmap,
						       NULL, NULL, fileName);
    sprintf(fileName, "%s/icons/%s.xpm", dataDir, "WeakestMeteorRadiant");
    meteorRadiantImage[2] = gdk_pixmap_create_from_xpm(pixmap,
						       NULL, NULL, fileName);
  }
  for (i = 0; i < N_SOLAR_SYSTEM_OBJECTS; i++) {
    if (firstCall) {
      sprintf(fileName, "%s/icons/%s.xpm", dataDir, solarSystemNames[i]);
      planetImages[i] = gdk_pixmap_create_from_xpm(pixmap,
						   NULL, NULL, fileName);
    }
    planetGC = NULL;
    switch (i) {
    case SUN:
      planetGC = gC[OR_LIGHT_YELLOW]; break;
    case MERCURY:
    case VENUS:
    case MOON:
      planetGC = gC[OR_WHITE];       break;
    case MARS:
      planetGC = starGC[4];     break;
    case JUPITER:
    case SATURN:
      planetGC = gC[OR_CREAM];       break;
    case URANUS:
    case NEPTUNE:
      planetGC = gC[OR_BLUE_GREEN];   break;
    }
    if (planetGC != NULL) {
      if (i == SUN) {
	rA = sunRA;
	dec = sunDec;
	illum = 1.0;
	mag = -26.8;
      } else
	planetInfo(dataDir, i, tJD, &rA, &dec, &illum, &mag);
      if (mag <= limitingMag) {
	int x, y, up;
	float mX, mY;
	static float sunAz, sunZA;
	double az, zA, eta, theta;
	
	if (i == SUN) {
	  up = azZA(rA, sin(dec), cos(dec), &az, &zA, FALSE);
	  sunAz = (float)az;
	  sunZA = (float)zA;
	} else
	  up = azZA(rA, sin(dec), cos(dec), &az, &zA, TRUE);
	if (up) {
	  if (thetaEta(az, zA, &theta, &eta)) {
	    mY = (float)theta;
	    mercator((float)eta, &mX);
	    mercatorToPixels(mX, mY, &x, &y);
	    if ((x > xBorder+xLeftLabelSkip)
		&& (x < xBorder+xLeftLabelSkip+plotWidth)
		&& (y > yBorder+yTopLabelSkip)
		&& (y < displayHeight)) {
	      if (!displayPlanetsAsSymbols) {
		if (i == SUN) {
		  gdk_draw_arc(pixmap, planetGC, TRUE,
			       x-(sunSize/2), y-(sunSize/2),
			       sunSize, sunSize, 0, FULL_CIRCLE);
		} else if (i == MOON) {
		  float sunAngle;
		  
		  sunAngle = greatCircleDirection((float)az, (float)(M_HALF_PI-zA),
						  sunAz, M_HALF_PI-sunZA);
		  drawMoon(13.0, illum, M_HALF_PI-sunAngle, x, y, FALSE, FALSE, FALSE);
		} else {
		  if (mag > DARK_GREY_LIMIT)
		    gdk_draw_point(pixmap, gC[OR_DARK_GREY], x, y);
		  else if (mag > GREY_LIMIT)
		    gdk_draw_point(pixmap, gC[OR_GREY], x, y);
		  else if (mag > WHITE_LIMIT)
		    gdk_draw_point(pixmap, planetGC, x, y);
		  else {
		    int index;
		    
		    index = 2*(int)(mag + 1.5);
		    if (index < 0)
		      index = 0;
		    gdk_draw_arc(pixmap, planetGC, TRUE,
				 x-(starSize[index]/2), y-(starSize[index]/2),
				 starSize[index], starSize[index], 0,
				 FULL_CIRCLE);
		  }
		}
	      } else {
		int w, h;
		
		gdk_drawable_get_size(planetImages[i], &w, &h);
		gdk_draw_drawable(pixmap, gC[OR_BLUE], planetImages[i], 0, 0,
				  x-w/2, y-h/2, 20, 20);
	      }
	      if (showNames && (!displayPlanetsAsSymbols) && (i != SUN) && (i != MOON)) {
		int index;
		char *displayName;
		
		displayName = malloc(strlen(solarSystemNames[i])+1);
		strcpy(displayName, solarSystemNames[i]);
		y += STAR_NAME_V_OFFSET;
		index = 2*(int)(mag + 1.5);
		if (index < 0)
		  index = 0;
		y += starSize[index] / 4;
		stringWidth = gdk_string_width(smallFont, displayName)/2;
		if ((x >= 0) && (x <= displayWidth) &&
		    (y >= 0) && (y <= displayHeight))
		  gdk_draw_string(pixmap, smallFont, planetGC,
				  x-stringWidth, y, displayName);
		free(displayName);
	      }
	    }
	  }
	}
      }
    }
  }
  firstCall = FALSE;
}

/*
  Calculate parameters used by other routines when producing
  a transverse mercator projection.
 */
static void calculateTransverseMercatorParameters(void)
{
  int boxWidth, boxHeight;

  boxWidth   = displayWidth-(2*xBorder)-xLeftLabelSkip;
  boxHeight  = displayHeight-(2*yBorder)-yBottomLabelSkip;
  plotWidth  = boxWidth - 2;
  plotHeight = boxHeight - 2;
  mercatorScale = plotHeight / (maximumZA - minimumZA);
  mercatorOffset = plotWidth / (2.0*mercatorScale);
}

/*
  This function draws the bounding box, and optionally the horizon,
  in Transverse Mercator projection mode.
*/
static void  drawTransverseMercatorBorder(void)
{
  int x1, x2, y1, y2, stringWidth, iStartAz, stringHeight;
  float mX, mY;
  double az, zA, theta, eta, startAz, endAz;

  if (zoomed)
    renderPangoText("ZOOMED", OR_GREY, MEDIUM_PANGO_FONT, &stringWidth,
		    &stringHeight, pixmap, 0, yTopLabelSkip + 20, 0.0,
		    FALSE, 0);
  az = 0.0; zA = M_PI*0.4999;
  thetaEta(az, zA, &theta, &eta);
  mY = (float)theta;
  mercator((float)eta, &mX);
  mercatorToPixels(mX, mY, &x1, &y1);
  renderPangoText("North", OR_GREEN, MEDIUM_PANGO_FONT,
		  &stringWidth, &stringHeight, NULL, x1,
		  8, 0.0, TRUE, 0);
  if ((x1 > xBorder+xLeftLabelSkip+stringWidth/2)
      && (x1 < xBorder+xLeftLabelSkip+plotWidth-stringWidth/2))
    renderPangoText("North", OR_GREEN, MEDIUM_PANGO_FONT,
		    &stringWidth, &stringHeight, pixmap, x1,
		    y1+15, 0.0, TRUE, 0);
  az = M_HALF_PI;
  thetaEta(az, zA, &theta, &eta);
  mY = (float)theta;
  mercator((float)eta, &mX);
  mercatorToPixels(mX, mY, &x1, &y1);
  renderPangoText("East", OR_GREEN, MEDIUM_PANGO_FONT,
		  &stringWidth, &stringHeight, NULL, x1,
		  8, 0.0, TRUE, 0);
  if ((x1 > xBorder+xLeftLabelSkip+stringWidth/2)
      && (x1 < xBorder+xLeftLabelSkip+plotWidth-stringWidth/2))
    renderPangoText("East", OR_GREEN, MEDIUM_PANGO_FONT,
		    &stringWidth, &stringHeight, pixmap, x1,
		    y1+15, 0.0, TRUE, 0);
  az = M_PI;
  thetaEta(az, zA, &theta, &eta);
  mY = (float)theta;
  mercator((float)eta, &mX);
  mercatorToPixels(mX, mY, &x1, &y1);
  renderPangoText("South", OR_GREEN, MEDIUM_PANGO_FONT,
		  &stringWidth, &stringHeight, NULL, x1,
		  8, 0.0, TRUE, 0);
  if ((x1 > xBorder+xLeftLabelSkip+stringWidth/2)
      && (x1 < xBorder+xLeftLabelSkip+plotWidth-stringWidth/2))
    renderPangoText("South", OR_GREEN, MEDIUM_PANGO_FONT,
		    &stringWidth, &stringHeight, pixmap, x1,
		    y1+15, 0.0, TRUE, 0);
  az = M_PI*1.5;
  thetaEta(az, zA, &theta, &eta);
  mY = (float)theta;
  mercator((float)eta, &mX);
  mercatorToPixels(mX, mY, &x1, &y1);
  renderPangoText("West", OR_GREEN, MEDIUM_PANGO_FONT,
		  &stringWidth, &stringHeight, NULL, x1,
		  8, 0.0, TRUE, 0);
  if ((x1 > xBorder+xLeftLabelSkip+stringWidth/2)
      && (x1 < xBorder+xLeftLabelSkip+plotWidth-stringWidth/2))
    renderPangoText("West", OR_GREEN, MEDIUM_PANGO_FONT,
		    &stringWidth, &stringHeight, pixmap, x1,
		    y1+15, 0.0, TRUE, 0);
  startAz = (centerAz - (M_PI+azSpan)*0.5)/DEGREES_TO_RADIANS;
  endAz = startAz+(azSpan/DEGREES_TO_RADIANS);
  if (endAz < 0.0) {
    startAz += 360.0;
    endAz += 360.0;
  }
  if ((startAz < 0.0) && (endAz > 180.0))
    startAz += 360.0;
  iStartAz = (int)(startAz/10.0);
  az = (float)(iStartAz*10);
  zA = M_HALF_PI;
  while (az < endAz) {
    char label[10];
    
    sprintf(label, "%2d", ((int)az) % 360);
    if (thetaEta(az*DEGREES_TO_RADIANS, zA, &theta, &eta)) {
      mY = (float)theta;
      mercator((float)eta, &mX);
      mercatorToPixels(mX, mY, &x1, &y1);
      renderPangoText(label, OR_CREAM, SMALL_PANGO_FONT,
		      &stringWidth, &stringHeight, NULL, x1,
		      displayHeight-8, 0.0, TRUE, 0);
      if ((x1 > xBorder+xLeftLabelSkip+stringWidth/2)
	  && (x1 < xBorder+xLeftLabelSkip+plotWidth-stringWidth/2))
	renderPangoText(label, OR_CREAM, SMALL_PANGO_FONT,
			&stringWidth, &stringHeight, pixmap, x1,
			displayHeight-8, 0.0, TRUE, 0);
    }
    if (azSpan/DEGREES_TO_RADIANS > 50.0)
      az += 10.0;
    else if (azSpan/DEGREES_TO_RADIANS > 15.0)
      az += 5.0;
    else
      az += 1.0;
  }

  if (inFullscreenMode)
    addSensitiveArea(FALSE, SA_TOP_AREA, 0, 0, displayWidth, displayHeight/5, 0.0);

  addSensitiveArea(FALSE, SA_FINGER_PAN_AREA, 0, 4*displayHeight/5, displayWidth,
		   displayHeight, 0.0);
  gdk_draw_line(pixmap, gC[OR_BLUE],
		0, yBorder+yTopLabelSkip, displayWidth, yBorder+yTopLabelSkip);
  /* Draw a line at the horizon */
  az = centerAz-(M_PI*0.25)-M_HALF_PI; zA = M_HALF_PI;
  thetaEta(az, zA, &theta, &eta);
  mY = (float)theta;
  mercator((float)eta, &mX);
  mercatorToPixels(mX, mY, &x1, &y1);
  az += M_HALF_PI;
  thetaEta(az, zA, &theta, &eta);
  mY = (float)theta;
  mercator((float)eta, &mX);
  mercatorToPixels(mX, mY, &x2, &y2);
  gdk_draw_line(pixmap, gC[OR_BLUE],
		x1, y1, x2, y2);
}

/*
  Draw stick-figure constellations
*/
static void plotConstellations(void)
{
  static int firstCall = TRUE;
  int i;
  constellation *thisConst;

  if (firstCall) {
    while (!constellationsInitialized)
      usleep(100000);
    firstCall = FALSE;
  }
 
  /*
    Loop through the constellations twice - the first pass
    draws the constellation names, and the second pass draws
    the stick figures.
   */
  thisConst = constellations;
  /* First pass: Constellation names */
  while (thisConst != NULL) {
    int x, y;
    float mX, mY;
    double az, zA, theta, eta;

    if ((useAsterisms && (thisConst->type == ASTERISM)) ||
	(!useAsterisms && (thisConst->type != ASTERISM))) {
      if (azZA(thisConst->nameRA,
	       thisConst->sinNameDec,
	       thisConst->cosNameDec,
	       &az, &zA, TRUE)) {
	float angleX[2], angleY[2];
	float angleEquator, plotAngle;
	double angleAz[2], angleZA[2], angleTheta[2], angleEta[2];
	
	azZA(thisConst->nameRA - CONST_NAME_DELTA,
	     thisConst->sinNameDec,
	     thisConst->cosNameDec,
	     &angleAz[0], &angleZA[0], FALSE);
	thetaEta(angleAz[0], angleZA[0], &angleTheta[0], &angleEta[0]);
	mY = (float)angleTheta[0];
	mercator((float)angleEta[0], &mX);
	mercatorToPixelsFloat(mX, mY, &angleX[0], &angleY[0]);
	azZA(thisConst->nameRA + CONST_NAME_DELTA,
	     thisConst->sinNameDec,
	     thisConst->cosNameDec,
	     &angleAz[1], &angleZA[1], FALSE);
	thetaEta(angleAz[1], angleZA[1], &angleTheta[1], &angleEta[1]);
	mY = (float)angleTheta[1];
	mercator((float)angleEta[1], &mX);
	mercatorToPixelsFloat(mX, mY, &angleX[1], &angleY[1]);
	angleEquator = -atan2f(angleY[0]-angleY[1], angleX[0]-angleX[1]);
	plotAngle = -(angleEquator + thisConst->nameAngle);
	thetaEta(az, zA, &theta, &eta);
	mY = (float)theta;
	mercator((float)eta, &mX);
	mercatorToPixels(mX, mY, &x, &y);
	if ((x >= 0) && (x <= displayWidth) &&
	    (y >= 0) && (y <= displayHeight)) {
	  int topClip;
	  
	  topClip = yTopLabelSkip + 6;
	  switch (thisConst->type) {
	    int tWidth, tHeight;
	    
	  case ZODIAC:
	    if (chineseColorScheme)
	      renderPangoText(thisConst->name, OR_FAINT_YELLOW, MEDIUM_PANGO_FONT,
			      &tWidth, &tHeight,
			      pixmap, x, y, plotAngle, TRUE, topClip);
	    else {
	      renderPangoText(thisConst->name, OR_PINK, MEDIUM_PANGO_FONT,
			      &tWidth, &tHeight,
			      pixmap, x, y, plotAngle, TRUE, topClip);
	    }
	    break;
	  case PTOLEMAIC:
	    if (chineseColorScheme)
	      renderPangoText(thisConst->name, OR_PINK, MEDIUM_PANGO_FONT,
			      &tWidth, &tHeight,
			      pixmap, x, y, plotAngle, TRUE, topClip);
	    else
	      renderPangoText(thisConst->name, OR_FAINT_YELLOW, MEDIUM_PANGO_FONT,
			      &tWidth, &tHeight,
			      pixmap, x, y, plotAngle, TRUE, topClip);
	    break;
	  case ASTERISM:
	    renderPangoText(thisConst->name, OR_WHITE, MEDIUM_PANGO_FONT,
			    &tWidth, &tHeight,
			    pixmap, x, y, plotAngle, TRUE, topClip);
	    break;
	  default:
	    renderPangoText(thisConst->name, OR_BLUE, SMALL_PANGO_FONT,
			    &tWidth, &tHeight,
			    pixmap, x, y, plotAngle, TRUE, topClip);
	  }
	}
      }
    }
    thisConst = thisConst->next;
  }
  
  thisConst = constellations;
  /* Second pass: Draw constellation stick figures */
  while (thisConst != NULL) {
    int x, y, firstPoint;
    int lastX = 0;
    int lastY = 0;
    float mX, mY;
    double az, zA, theta, eta;

    firstPoint = TRUE;
    if ((useAsterisms && (thisConst->type == ASTERISM)) ||
	(!useAsterisms && (thisConst->type != ASTERISM))) {
      for (i = 0; i < thisConst->nPoints; i++) {
	if ((thisConst->rA[i] == 0.0) && (thisConst->dec[i] == 0.0)) {
	  x = y = -10000;
	} else {
	  if (azZA(thisConst->rA[i],
		   thisConst->sinDec[i],
		   thisConst->cosDec[i],
		   &az, &zA, TRUE)) {
	    if (thetaEta(az, zA, &theta, &eta)) {
	      mY = (float)theta;
	      mercator((float)eta, &mX);
	      mercatorToPixels(mX, mY, &x, &y);
	    } else
	      x = y = -10000;
	  } else
	    x = y = -10000;
	}
	if (firstPoint)
	  firstPoint = FALSE;
	else if ((y <= displayHeight) &&
		 (x != -10000) && (y != -10000) &&
		 (lastX != -10000) && (lastY != -10000) &&
		 (lastY <= displayHeight))
	  switch (thisConst->type) {
	  case ZODIAC:
	    if (chineseColorScheme)
	      gdk_draw_line(pixmap, gC[OR_FAINT_GOLD], x, y, lastX, lastY);
	    else
	      gdk_draw_line(pixmap, gC[OR_FAINT_PINK], x, y, lastX, lastY);
	    break;
	  case PTOLEMAIC:
	    if (chineseColorScheme)
	      gdk_draw_line(pixmap, gC[OR_FAINT_PINK], x, y, lastX, lastY);
	    else
	      gdk_draw_line(pixmap, gC[OR_FAINT_GOLD], x, y, lastX, lastY);
	    break;
	  case ASTERISM:
	    gdk_draw_line(pixmap, gC[OR_CREAM], x, y, lastX, lastY);
	    break;
	  default:
	    gdk_draw_line(pixmap, gC[OR_FAINT_BLUE], x, y, lastX, lastY);
	  }
	lastX = x;
	lastY = y;
      }
    }
    thisConst = thisConst->next;
  }
}

/*
  Read a line of text from the config file, and strip comments (flagged by #).
*/
int getLine(int fD, char *buffer, int *eOF)
{
  char inChar = (char)0;
  int count = 0;
  int sawComment = FALSE;
  int foundSomething = FALSE;

  buffer[0] = (char)0;
  while ((!(*eOF)) && (inChar != '\n') && (count < 132)) {
    int nChar;

    nChar = read(fD, &inChar, 1);
    if (nChar > 0) {
      foundSomething = TRUE;
      if (inChar == '#')
        sawComment = TRUE;
      if (!sawComment)
        buffer[count++] = inChar;
    } else {
      *eOF = TRUE;
    }
  }
  if (foundSomething) {
    if (count > 0)
      buffer[count-1] = (char)0;
    return(TRUE);
  } else
    return(FALSE);
}

/*
  This function reads in all the files in the deepSky directory.
*/
static void readDeepSkyObjects(void)
{
  char dataDirName[1000], sourceName[100];
  int dataFD;
  deepSkyEntry *newEntry;
  deepSkyEntry *lastEntry = NULL;
  DIR *dirPtr;
  struct dirent *nextEnt;

  sprintf(dataDirName, "%s/deepSky/", dataDir);
  dirPtr = opendir(dataDirName);
  while ((nextEnt = readdir(dirPtr)) != NULL)
    if (strstr(nextEnt->d_name, ".") == NULL) {
      sprintf(dataDirName, "%s/deepSky/%s", dataDir, nextEnt->d_name);
      dataFD = open(dataDirName, O_RDONLY);
      if (unlikely(dataFD < 0)) {
	perror("dataDirName");
      } else {
	int eof = FALSE;

	while (!eof) {
	  int nRead, rAHH, rAMM, decDD, decMM, type;
	  float rASS, decSS, mag;

	  getLine(dataFD, &dataDirName[0], &eof);
	  nRead = sscanf(dataDirName, "%s %d %d %f %d %d %f %d %f",
			 &sourceName[0], &rAHH, &rAMM, &rASS,
			 &decDD, &decMM, &decSS, &type, &mag);
	  if (nRead == 9) {
	    double rA, dec, decSign;
	    
	    rA = (double)rAHH + ((double)rAMM)/60.0 + ((double)rASS)/3600.0;
	    if (strstr(dataDirName, "-") == NULL)
	      decSign = 1.0;
	    else
	      decSign = -1.0;
	    dec = decSign*(fabs((double)decDD) + fabs((double)decMM)/60.0 + fabs((double)decSS)/3600.0);
	    newEntry = (deepSkyEntry *)malloc(sizeof(deepSkyEntry));
	    if (unlikely(newEntry == NULL)) {
	      perror("New deep sky entry");
	      exit(ERROR_EXIT);
	    }
	    newEntry->name = (char *)malloc(strlen(sourceName)+1);
	    if (unlikely(newEntry->name == NULL)) {
	      perror("newEntry->name");
	      exit(ERROR_EXIT);
	    }
	    strcpy(newEntry->name, sourceName);
	    newEntry->rA = rA * HOURS_TO_RADIANS;
	    newEntry->dec = dec * DEGREES_TO_RADIANS;
	    newEntry->sinDec = sin(newEntry->dec);
	    newEntry->cosDec = cos(newEntry->dec);
	    newEntry->type = type;
	    newEntry->mag = mag;
	    newEntry->next = NULL;
	    if (deepSkyRoot == NULL)
	      deepSkyRoot = newEntry;
	    else
	      lastEntry->next = newEntry;
	    lastEntry = newEntry;
	  }
	}
	close(dataFD);
      }
    }
  haveReadDeepSkyObjects = TRUE;
}

void plotDeepSkyObject(int kind, int x, int y)
{
  static int firstCall = TRUE;
  int i;
  static int galX[20], galY[20];
  GdkPoint points[20];

  if (firstCall) {
    /* Make template ellipse of galaxy figures */
    for (i = 0; i < 20; i++) {
      float angle;

      angle = M_2PI*0.05*(float)i;
      galX[i] = (int)roundf(2.0*cosf(angle) + 7.0*sinf(angle));
      galY[i] = -(int)roundf(4.0*cosf(angle) + 2.0*sinf(angle));
    }
    firstCall = FALSE;
  }
  switch (kind) {
  case SUPERNOVA_REMNENT:
    gdk_draw_arc(pixmap, gC[OR_YELLOW], FALSE,
		 x-6, y-6, 12, 12, 0, FULL_CIRCLE);
    break;
  case OPEN_CLUSTER:
    gdk_draw_arc(pixmap, gC[OR_YELLOW], TRUE,
		 x-6, y-6, 12, 12, 0, FULL_CIRCLE);
    gdk_draw_arc(pixmap, gC[OR_WHITE], FALSE,
		 x-6, y-6, 12, 12, 0, FULL_CIRCLE);
    break;
  case GLOBULAR_CLUSTER:
    gdk_draw_arc(pixmap, gC[OR_YELLOW], TRUE,
		 x-6, y-6, 12, 12, 0, FULL_CIRCLE);
    gdk_draw_line(pixmap, gC[OR_BLACK], x, y-6, x, y+6);
    gdk_draw_line(pixmap, gC[OR_BLACK], x-6, y, x+6, y);
    break;
  case DIFFUSE_NEBULA:
    points[0].x = x-5; points[0].y = y-5;
    points[1].x = x+5; points[1].y = y-5;
    points[2].x = x+5; points[2].y = y+5;
    points[3].x = x-5; points[3].y = y+5;
    gdk_draw_polygon(pixmap, gC[OR_GREEN], TRUE, points, 4);
    break;
  case PLANETARY_NEBULA:
    gdk_draw_arc(pixmap, gC[OR_BLUE_GREEN], TRUE,
		 x-6, y-6, 12, 12, 0, FULL_CIRCLE);
    gdk_draw_arc(pixmap, gC[OR_BLACK], TRUE,
		 x-3, y-3, 6, 6, 0, FULL_CIRCLE);
    break;
  case GALAXY:
    for (i = 0; i < 20; i++) {
      points[i].x = x + galX[i]; points[i].y = y + galY[i];
    }
    gdk_draw_polygon(pixmap, gC[OR_RED], TRUE, points, 20);
    break;
  }
}

static void plotDeepSkyObjects(void)
{
  int x, y, mercXLow, mercXHigh, mercYLow, mercYHigh, stringWidth;
  float mX, mY;
  double az, zA, theta, eta;
  deepSkyEntry *nextEntry;

  mercXLow  = xBorder+xLeftLabelSkip;
  mercXHigh = xBorder+xLeftLabelSkip+plotWidth;
  mercYLow  = yBorder+yTopLabelSkip;
  mercYHigh = displayHeight;
  nextEntry = deepSkyRoot;
  while (nextEntry != NULL) {
    if (azZA(nextEntry->rA, nextEntry->sinDec,
	     nextEntry->cosDec, &az, &zA, TRUE))
      if (thetaEta(az, zA, &theta, &eta)) {
	mY = (float)theta;
	mercator((float)eta, &mX);
	mercatorToPixels(mX, mY, &x, &y);
	if ((x > mercXLow) && (x < mercXHigh) && (y > mercYLow) && (y < mercYHigh)) {
	  plotDeepSkyObject(nextEntry->type, x, y);
	  stringWidth = gdk_string_width(smallFont, nextEntry->name);
	  gdk_draw_string(pixmap, smallFont, gC[OR_GREEN],
			  x-stringWidth/2, y+18, nextEntry->name);
	}
      }
    nextEntry = nextEntry->next;
  }
}

/*
  Return the day of the year, given the year, month and date.
*/
double makeDayNumber(int year, int month, int day)
{
  int i = 0;
  double dayOffset = 0.0;

  while (i < month - 1)
    dayOffset += monthLengths[i++];
  if ((year % 4 == 0) && (month > 2))
    dayOffset += 1.0;
  dayOffset += (double)day;
  return(dayOffset);
}

/*
  Convert the Julian day into Calendar Date as per
  "Astronomical Algorithms" (Meeus)
*/
void tJDToDate(double tJD, int *year, int *month, int *day)
{
  int Z, alpha, A, B, C, D, E;
  double F;

  Z = (int)(tJD+0.5);
  F = tJD + 0.5 - (double)Z;
  if (Z >= 2299161) {
    alpha = (int)(((double)Z - 1867216.25)/36524.25);
    A = Z + 1 + alpha - alpha/4;
  } else
    A = Z;
  B = A + 1524;
  C = (int)(((double)B - 122.1) / 365.25);
  D = (int)(365.25 * (double)C);
  E = (int)(((double)(B - D))/30.6001);
  *day = (int)((double)B - (double)D - (double)((int)(30.6001*(double)E)) + F);
  if (E < 14)
    *month = E - 1;
  else
    *month = E - 13;
  if (*month > 2)
    *year = C - 4716;
  else
    *year = C - 4715;
}

void plotMeteorShowerRadiants(void)
{
  int yearNow, monthNow, dayNow, occuringNow, flipped, lastDay;
  int mercXLow, mercXHigh, mercYLow, mercYHigh;
  double startDayNumber, endDayNumber;
  double dayOffset, jan0TJD, todayDayNumber, showerTJD;
  meteorShower *shower;
  GdkGC *radiantGC;
  GdkPixmap *radiantImage;
  
  if (!haveReadMeteorShowers) {
    readMeteorShowers();
    haveReadMeteorShowers = TRUE;
  }
  mercXLow  = xBorder+xLeftLabelSkip;
  mercXHigh = xBorder+xLeftLabelSkip+plotWidth;
  mercYLow  = yBorder+yTopLabelSkip;
  mercYHigh = displayHeight;
  tJDToDate(tJD, &yearNow, &monthNow, &dayNow);
  jan0TJD = buildTJD(yearNow-1900, 0, 0, 0, 0, 0, 0);
  shower = meteorShowers;
  while (shower != NULL) {
    startDayNumber= makeDayNumber(yearNow, shower->startMonth, shower->startDay);
    endDayNumber= makeDayNumber(yearNow, shower->endMonth, shower->endDay);
    todayDayNumber= makeDayNumber(yearNow, monthNow, dayNow);
    dayOffset = makeDayNumber(yearNow, shower->maxMonth, shower->maxDay);
    showerTJD = jan0TJD + dayOffset;
    if (startDayNumber < endDayNumber) {
      flipped = FALSE;
      if ((todayDayNumber >= startDayNumber) && (todayDayNumber <= endDayNumber))
	occuringNow = TRUE;
      else
	occuringNow = FALSE;
    } else {
      /* Handle case when shower starts in December and ends in January */
      flipped = TRUE;
      if ((todayDayNumber < startDayNumber) && (todayDayNumber > endDayNumber))
	occuringNow = FALSE;
      else
	occuringNow = TRUE;
    }
    if (occuringNow) {
      int i, x, y, w, h, stringWidth;
      float mX, mY;
      double rA, dec, rA1, dec1, rA2, dec2, entryDayNumber, tJD1, tJD2, deltaDay;
      double az, zA, theta, eta;

      rA1  = rA2 = shower->radiantPositions[0].rA;
      dec1 = dec2 = shower->radiantPositions[0].dec;
      entryDayNumber = makeDayNumber(yearNow, shower->radiantPositions[0].month,
				     shower->radiantPositions[0].day);
      if (flipped && (entryDayNumber > 100.0) && (todayDayNumber < 100.0))
	entryDayNumber -= 365.0;
      else if (flipped && (entryDayNumber < 100.0) && (todayDayNumber > 100.0))
	entryDayNumber += 365.0;
      tJD1 = tJD2 = entryDayNumber;
      i = lastDay = 0;
      while ((i < (shower->nRadiantPositions-1)) && (!lastDay)) {
	i++;
	entryDayNumber = makeDayNumber(yearNow, shower->radiantPositions[i].month,
				       shower->radiantPositions[i].day);
	if (flipped && (entryDayNumber > 100.0) && (todayDayNumber < 100.0))
	  entryDayNumber -= 365.0;
	else if (flipped && (entryDayNumber < 100.0) && (todayDayNumber > 100.0))
	  entryDayNumber += 365.0;
	if (todayDayNumber > entryDayNumber)
	  lastDay = FALSE;
	else
	  lastDay = TRUE;
	rA1 = rA2; dec1 = dec2;
	tJD1 = tJD2;
	rA2 = shower->radiantPositions[i].rA;
	dec2 = shower->radiantPositions[i].dec;
	tJD2 = entryDayNumber;
      }
      if (!lastDay) {
	rA = rA2;
	dec = dec2;
      } else {
	deltaDay = tJD2 - tJD1;
	if (deltaDay <= 0) {
	  rA = rA1;
	  dec = dec1;
	} else {
	  rA  = ((deltaDay + tJD1 - todayDayNumber)*rA1  + (todayDayNumber - tJD1)*rA2)/deltaDay;
	  dec  = ((deltaDay + tJD1 - todayDayNumber)*dec1  + (todayDayNumber - tJD1)*dec2)/deltaDay;
	}
      }
      if (azZA(rA, sin(dec), cos(dec), &az, &zA, TRUE))
	if (thetaEta(az, zA, &theta, &eta)) {
	  mY = (float)theta;
	  mercator((float)eta, &mX);
	  mercatorToPixels(mX, mY, &x, &y);
	  if ((x > mercXLow) && (x < mercXHigh) && (y > mercYLow) && (y < mercYHigh)) {
	    if (shower->zHR >= 50) {
	      radiantImage = meteorRadiantImage[0];
	      radiantGC = gC[OR_WHITE];
	    } else if (shower->zHR >= 20) {
	      radiantImage = meteorRadiantImage[1];
	      radiantGC = gC[OR_CREAM];
	    } else {
	      radiantImage = meteorRadiantImage[2];
	      radiantGC = gC[OR_GREY];
	    }
	    gdk_drawable_get_size(radiantImage, &w, &h);
	    gdk_draw_drawable(pixmap, gC[OR_BLUE], radiantImage, 0, 0,
			      x-w/2, y-h/2, 20, 20);
	    stringWidth = gdk_string_width(smallFont, shower->threeLetterName);
	    gdk_draw_string(pixmap, smallFont, radiantGC,
			    x-stringWidth/2, y+21, shower->threeLetterName);
	  }
	}
    }
    shower = shower->next;
  }
}

/*
  This routine draws the sky for the Transverse Mercator projection.
*/
static void redrawScreenTransverseMercator(void)
{
  calculateTransverseMercatorParameters();
  needNewTime = TRUE;
  if (displayConstellations)
    plotConstellations();
  if (showGreatCircles) {
    plotEcliptic();
    plotEquator();
    plotGalactic();
  }
  clearScoreBoard();
  if (showStars)
    plotFixedObjects();
  if (showPlanets)
    plotPlanets();
  if (showComets)
    plotComets();
  if (showNames)
    plotStarNames();
  if (showBayer)
    plotBayer();
  if (showDeepSky) {
    if (!haveReadDeepSkyObjects)
      readDeepSkyObjects();
    plotDeepSkyObjects();
  }
  if (showMeteors)
    plotMeteorShowerRadiants();
  showTime();
  drawTransverseMercatorBorder();
}

/*
  Calculate the transit time of a Solar System object,   Return full TJD of
transit nearest to time passed in tJDS.
*/
double calcTransitTime(double tJDS, int obj, double *transitEl)
{
  int done = FALSE;
  int loopCount = 0;
  float illum, mag;
  double hA, rA, dec, myLST;

  while (!done && (loopCount++ < 10)) {
    myLST = lSTAtTJD(tJDS);
    planetInfo(dataDir, obj, tJDS, &rA, &dec, &illum, &mag);
    hA = myLST - rA;
    if (fabs(hA) < 3.0e-4)
      done = TRUE;
    else
      tJDS -= hA/M_2PI;
  }
  if (northernHemisphere)
    *transitEl = dec + M_HALF_PI - latitude;
  else
    *transitEl = M_HALF_PI + latitude - dec;
  return(tJDS);
}

/*
  Calculate the rising or setting time for a Solar System object.   h0 contains the
elevation the object has when the combination of refraction, the source's size,
and parallax makes some portion of the object first visible.   The full TJD
of the event time is returned.
*/
double  calcRiseOrSetTime(int rise, int obj, double tJDS, double lSTNow, double h0,
			  int *circumpolar, double *finalH, double *finalCosH, double *finalDec,
			  float *illum)
{
  double theTime = 0.0;
  double rA, dec, cosH;
  float mag;

  if (obj != EARTH)
    planetInfo(dataDir, obj, tJDS, &rA, &dec, illum, &mag);
  planetInfo(dataDir, obj, tJDS, &rA, &dec, illum, &mag);
  cosH = (sin(h0) - sin(latitude)*sin(dec))/(cos(latitude)*cos(dec));
  if (fabs(cosH) > 1.0) {
    if (dec*latitude > 0.0)
      *circumpolar = WILL_NOT_SET;
    else
      *circumpolar = WILL_NOT_RISE;
  } else {
    int iteration = 0;
    int closeEnough = FALSE;
    double h, hA;
    double lastHA = 1.0e6;

    *circumpolar = FALSE;
    while (!closeEnough && (iteration < 20)) {
      if (rise)
	h = -acos(cosH);
      else
	h = acos(cosH);
      hA = lSTNow - rA;
      if (hA > M_PI)
	hA -= M_2PI;
      else if (hA < -M_PI)
	hA += M_2PI;
      iteration++;
      if (fabs(hA - lastHA) < 1.0e-4)
	closeEnough = TRUE;
      else {
	lastHA = hA;
	planetInfo(dataDir, obj, tJDS + SIDEREAL_TO_SOLAR*(h-hA)/M_2PI, &rA, &dec, illum, &mag);
	cosH = (sin(h0) - sin(latitude)*sin(dec))/(cos(latitude)*cos(dec));
      }
    }
    *finalCosH = cosH;
    *finalH = h;
    *finalDec = dec;
    theTime = tJDS + SIDEREAL_TO_SOLAR*(h-hA)/M_2PI;
  }
  return(theTime);
}

/*
  D R A W  C O M P A S S  L I N E

  Draw a radial line within the planet compass.
*/
void drawCompassLine(GdkGC *gC, float angle, int length, int xCen, int yCen, int radius)
{
  float x1, y1, x2, y2, oRadius, iRadius, sinA, cosA;

  oRadius = (float)radius;
  iRadius = oRadius - (float)length;
  sinA = sinf(angle);
  cosA = cosf(angle);
  x1 = x2 = (float)xCen;
  y1 = y2 = (float)yCen;
  x1 += oRadius*sinA;
  y1 -= oRadius*cosA;
  x2 += iRadius*sinA;
  y2 -= iRadius*cosA;
  gdk_draw_line(pixmap, gC, (int)(x1 + 0.5), (int)(y1 + 0.5), (int)(x2 + 0.5), (int)(y2 + 0.5));
}

/*
  P H A S E  T I M E

  This function returns the JD for a particular phase of the moon.
  It implements the algorithm in Astronomical Algorythms (Meeus)
  Chapter 49, 2nd edition.    The code is largely uncommented
  because the book will serve as commentary.
 */
double phaseTime(double k, int i)
{
  double T, E, F, M, MP, OM, testTJD, W;
  double A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14;
  
  T = k/1236.85;
  testTJD = PHASE_C0 + k*PHASE_C1 + T*T*PHASE_C2 + T*T*T*PHASE_C3 +
    T*T*T*T*PHASE_C4;
  E   = PHASE_E0 + PHASE_E1*T + PHASE_E2*T*T;
  M   = PHASE_M0 + PHASE_M1*k + PHASE_M2*T*T + PHASE_M3*T*T*T;
  M  *= DEGREES_TO_RADIANS;
  MP  = PHASE_MP0 + PHASE_MP1*k + PHASE_MP2*T*T + PHASE_MP3*T*T*T + PHASE_MP4*T*T*T*T;
  MP *= DEGREES_TO_RADIANS;
  F   = PHASE_F0 + PHASE_F1*k + PHASE_F2*T*T + PHASE_F3*T*T*T + PHASE_F4*T*T*T*T;
  F  *= DEGREES_TO_RADIANS;
  OM  = PHASE_OM0 + PHASE_OM1*k + PHASE_OM2*T*T + PHASE_OM3*T*T*T;
  OM *= DEGREES_TO_RADIANS;
  A1  = (PHASE_A1_0  + PHASE_A1_1 *k + PHASE_A1_2*T*T) * DEGREES_TO_RADIANS;
  A2  = (PHASE_A2_0  + PHASE_A2_1 *k) * DEGREES_TO_RADIANS;
  A3  = (PHASE_A3_0  + PHASE_A3_1 *k) * DEGREES_TO_RADIANS;
  A4  = (PHASE_A4_0  + PHASE_A4_1 *k) * DEGREES_TO_RADIANS;
  A5  = (PHASE_A5_0  + PHASE_A5_1 *k) * DEGREES_TO_RADIANS;
  A6  = (PHASE_A6_0  + PHASE_A6_1 *k) * DEGREES_TO_RADIANS;
  A7  = (PHASE_A7_0  + PHASE_A7_1 *k) * DEGREES_TO_RADIANS;
  A8  = (PHASE_A8_0  + PHASE_A8_1 *k) * DEGREES_TO_RADIANS;
  A9  = (PHASE_A9_0  + PHASE_A9_1 *k) * DEGREES_TO_RADIANS;
  A10 = (PHASE_A10_0 + PHASE_A10_1*k) * DEGREES_TO_RADIANS;
  A11 = (PHASE_A11_0 + PHASE_A11_1*k) * DEGREES_TO_RADIANS;
  A12 = (PHASE_A12_0 + PHASE_A12_1*k) * DEGREES_TO_RADIANS;
  A13 = (PHASE_A13_0 + PHASE_A13_1*k) * DEGREES_TO_RADIANS;
  A14 = (PHASE_A14_0 + PHASE_A14_1*k) * DEGREES_TO_RADIANS;
  
  switch(i) {
  case 0: /* New Moon */
    testTJD +=
      -0.40720         * sin(MP)
      +0.17241 * E     * sin(M)
      +0.01608         * sin(2.0*MP)
      +0.01039         * sin(2.0*F)
      +0.00739 * E     * sin(MP-M)
      -0.00514 * E     * sin(MP+M)
      +0.00208 * E * E * sin(2.0*M)
      -0.00111         * sin(MP-2.0*F)
      -0.00057         * sin(MP+2.0*F)
      +0.00056 * E     * sin(2.0*MP+M)
      -0.00042         * sin(3.0*MP)
      +0.00042 * E     * sin(M+2.0*F)
      +0.00038 * E     * sin(M-2.0*F)
      -0.00024 * E     * sin(2.0*MP-M)
      -0.00017         * sin(OM)
      -0.00007         * sin(MP+2.0*M)
      +0.00004         * sin(MP-2.0F)
      +0.00004         * sin(3.0*M)
      +0.00003         * sin(MP+M-2.0*F)
      +0.00003         * sin(2.0*(MP+F))
      -0.00003         * sin(MP+M+2.0*F)
      +0.00003         * sin(MP-M+2.0*F)
      -0.00002         * sin(MP-M-2.0*F)
      -0.00002         * sin(3.0*MP+M)
      +0.00002         * sin(4.0*MP);
    break;
  case 1: /* First and last quarters */
  case 3:
    testTJD +=
      -0.62801         * sin(MP)
      +0.17172 * E     * sin(M)
      -0.01183 * E     * sin(MP+M)
      +0.00862         * sin(2.0*MP)
      +0.00804         * sin(2.0*F)
      +0.00454 * E     * sin(MP-M)
      +0.00204 * E * E * sin(2.0*M)
      -0.00180         * sin(MP-2.0*F)
      -0.00070         * sin(MP+2.0*F)
      -0.00040         * sin(3.0*MP)
      -0.00034 * E     * sin(2.0*MP-M)
      +0.00032 * E     * sin(M+2.0*F)
      +0.00032 * E     * sin(M-2.0*F)
      -0.00028 * E * E * sin(MP+2.0*M)
      +0.00027 * E     * sin(2.0*MP+M)
      -0.00017         * sin(OM)
      -0.00005         * sin(MP-M-2.0*F)
      +0.00004         * sin(2.0*(MP+F))
      -0.00004         * sin(MP+M+2.0*F)
      +0.00004         * sin(MP-2.0*M)
      +0.00003         * sin(MP+M-2.0*F)
      +0.00003         * sin(3.0*M)
      +0.00002         * sin(2.0*(MP-F))
      +0.00002         * sin(MP-M+2.0*F)
      -0.00002         * sin(3.0*MP+M);
    W = 0.00306 - 0.00038*E*cos(M) + 0.00026*cos(MP)
      + 0.00002*(cos(MP+M) + cos(2.0*F) - cos(MP-M));
    if (i == 1)
      testTJD += W;
    else
      testTJD -= W;
    break;
  case 2: /* Full Moon */
    testTJD +=
      -0.40614         * sin(MP)
      +0.17302 * E     * sin(M)
      +0.01614         * sin(2.0*MP)
      +0.01043         * sin(2.0*F)
      +0.00734 * E     * sin(MP-M)
      -0.00515 * E     * sin(MP+M)
      +0.00209 * E * E * sin(2.0*M)
      -0.00111         * sin(MP-2.0*F)
      -0.00057         * sin(MP+2.0*F)
      +0.00056 * E     * sin(2.0*MP+M)
      -0.00042         * sin(3.0*MP)
      +0.00042 * E     * sin(M+2.0*F)
      +0.00038 * E     * sin(M-2.0*F)
      -0.00024 * E     * sin(2.0*MP-M)
      -0.00017         * sin(OM)
      -0.00007         * sin(MP+2.0*M)
      +0.00004         * sin(MP-2.0*F)
      +0.00004         * sin(3.0*M)
      +0.00003         * sin(MP+M-2.0F)
      +0.00003         * sin(2.0*(MP+F))
      -0.00003         * sin(MP+M+2.0*F)
      +0.00003         * sin(MP-M+2.0*F)
      -0.00002         * sin(MP-M-2.0*F)
      -0.00002         * sin(3.0*MP+M)
      +0.00002         * sin(4.0*MP);
    break;
  }
  
  testTJD +=
    +0.000325 * sin(A1 ) + 0.000056 * sin(A8 )
    +0.000165 * sin(A2 ) + 0.000047 * sin(A9 )
    +0.000164 * sin(A3 ) + 0.000042 * sin(A10)
    +0.000126 * sin(A4 ) + 0.000040 * sin(A11)
    +0.000110 * sin(A5 ) + 0.000037 * sin(A12)
    +0.000062 * sin(A6 ) + 0.000035 * sin(A13)
    +0.000060 * sin(A7 ) + 0.000023 * sin(A14);
  return(testTJD);
}

#define ONE_MINUTE_IN_DAYS (1.0/1440.0)
/*
  Calculate and return the tJDs for the next new moon, first quarter etc.
  See Meeus Astronomical Algorithms, Chapt 49.
*/
void nextPhases(double tJD, int nRows, double *nextNew,
                double *nextFirstQuarter, double *nextFull,
                double *nextLastQuarter)
{
  int i, done;
  double testTJD, delta, k, dYear;

  dYear = (tJD - 2451545.5)/365.25;
  tJD -= PHASE_OFFSET;
  k = (dYear * 12.3685);
  k = (double)((int)k);
  for (i = 0; i < 4; i++) {
    done = 0;
    do {
      testTJD = phaseTime(k, i);
      delta = testTJD - tJD;
      if (fabs(delta) < ONE_MINUTE_IN_DAYS)
        k += 1.0;
      else if (delta > PHASE_C1)
        k -= 1.0;
      else
        done = 1;
    } while (!done);
    switch (i) {
      double kk;
      int j;

    case 0:
      nextNew[nRows/2] = testTJD + PHASE_OFFSET;
      kk = k - (double)(nRows/2);
      for (j = 0; j < nRows; j++) {
	if (j != nRows/2)
	  nextNew[j] = phaseTime(kk, i) + PHASE_OFFSET;
	kk += 1.0;
      }
      k += 0.25;
      break;
    case 1:
      nextFirstQuarter[nRows/2] = testTJD + PHASE_OFFSET;
      kk = k - (double)(nRows/2);
      for (j = 0; j < nRows; j++) {
	if (j != nRows/2)
	  nextFirstQuarter[j] = phaseTime(kk, i) + PHASE_OFFSET;
	kk += 1.0;
      }
      k += 0.25;
      break;
    case 2:
      nextFull[nRows/2] = testTJD + PHASE_OFFSET;
      kk = k - (double)(nRows/2);
      for (j = 0; j < nRows; j++) {
	if (j != nRows/2)
	  nextFull[j] = phaseTime(kk, i) + PHASE_OFFSET;
	kk += 1.0;
      }
      k += 0.25;
      break;
    case 3:
      nextLastQuarter[nRows/2] = testTJD + PHASE_OFFSET;
      kk = k - (double)(nRows/2);
      for (j = 0; j < nRows; j++) {
	if (j != nRows/2)
	  nextLastQuarter[j] = phaseTime(kk, i) + PHASE_OFFSET;
	kk += 1.0;
      }
      break;
    }
  }
}

/*
  Return the day of the week (0->6) for year, month, day
*/
int dayOfWeek(int year, int month, int day)
{
  int century, c, weekDay, yy, y, m, leap;

  century = year / 100;
  yy = year - century*100;
  c = 2*(3 - (century % 4));
  y = yy + yy/4;
  if ((year % 400) == 0)
    leap = TRUE;
  else if ((year % 100) == 0)
    leap = FALSE;
  else if ((year % 4) == 0)
    leap = TRUE;
  else
    leap = FALSE;
  if (leap && (month < 3))
    m = (int)leapYear[month-1];
  else
    m = (int)normalYear[month-1];
  weekDay = (c + y + m + day) % 7;
  return(weekDay);
}

#define CAL_TOP_OFFSET (40)
#define CAL_LEFT_OFFSET (3)
#define CAL_DAY_WIDTH  (67)
#define CAL_DAY_HEIGHT (92)
#define SMALL_MOONCAL_SHIFT (50)

/*
  Plot a little box for the day, with the moon shown with the proper phase, for
  the one month moon calendar.
*/
void plotDayBox(double tJDS, int week, int weekDay, int day, int today,
		int phase, int blue, int lunarDay)
{
  int isFull = FALSE;
  double dummy;
  float illum2, illum, mag, sunAngle;
  char dayString[3];
  GdkPoint points[4];
  GdkGC *dayGC;

  planetInfo(dataDir, EARTH, tJDS, &dummy, &dummy, &illum2, &mag);
  planetInfo(dataDir, MOON, tJDS+0.04, &dummy, &dummy, &illum2, &mag);
  planetInfo(dataDir, MOON, tJDS, &dummy, &dummy, &illum, &mag);
  if (northernHemisphere) {
    if (illum < illum2)
      sunAngle = 0.0;
    else
      sunAngle = M_PI;
  } else {
    if (illum > illum2)
      sunAngle = 0.0;
    else
      sunAngle = M_PI;
  }
  points[0].x = CAL_LEFT_OFFSET + weekDay*CAL_DAY_WIDTH; points[0].y = CAL_TOP_OFFSET + week*CAL_DAY_HEIGHT + SMALL_MOONCAL_SHIFT;
  points[1].x = points[0].x;                             points[1].y = points[0].y + CAL_DAY_HEIGHT;
  points[2].x = points[1].x + CAL_DAY_WIDTH;             points[2].y = points[1].y;
  points[3].x = points[2].x;                             points[3].y = points[2].y - CAL_DAY_HEIGHT;
  if (phase == 2)
    isFull = TRUE;
  drawMoon(CAL_DAY_WIDTH/2 - 1, illum, sunAngle, points[0].x+CAL_DAY_WIDTH/2 + 1,
	   points[0].y+CAL_DAY_HEIGHT/2 + 1, TRUE, isFull, blue);
  gdk_draw_polygon(pixmap, gC[OR_BLUE], FALSE, points, 4);
  sprintf(dayString, "%2d", day);
  if (today) {
    dayGC = gC[OR_WHITE];
    points[0].x += 1;                                      points[0].y += 1;      
    points[1].x = points[0].x;                             points[1].y = points[0].y + CAL_DAY_HEIGHT - 2;
    points[2].x = points[1].x + CAL_DAY_WIDTH - 2;         points[2].y = points[1].y;
    points[3].x = points[2].x;                             points[3].y = points[2].y - CAL_DAY_HEIGHT + 2;
    gdk_draw_polygon(pixmap, gC[OR_WHITE], FALSE, points, 4);
  } else
    dayGC = gC[OR_BLUE];
  gdk_draw_string(pixmap, smallFont, dayGC, points[0].x+gdk_string_width(smallFont, dayString)/2 - 1,
		  points[0].y+gdk_string_width(smallFont, dayString), dayString);
  sprintf(dayString, "%2d", lunarDay);
  if (today)
    dayGC = gC[OR_PINK];
  else
    dayGC = gC[OR_FAINT_PINK];
  gdk_draw_string(pixmap, smallFont, dayGC, points[0].x+gdk_string_width(smallFont, dayString)/2 + 46,
		  points[0].y + 88, dayString);
  if (phase == 0)
    gdk_draw_string(pixmap, smallFont, gC[OR_GREEN],
		    points[0].x + CAL_DAY_WIDTH/2 - gdk_string_width(smallFont, "New")/2 + 3,
		    points[0].y+gdk_string_width(smallFont, dayString), "New");
  else if (phase == 2)
    gdk_draw_string(pixmap, smallFont, gC[OR_GREEN],
		    points[0].x + CAL_DAY_WIDTH/2 - gdk_string_width(smallFont, "Full")/2 + 4,
		    points[0].y+gdk_string_width(smallFont, dayString), "Full");
}

#define SUN_DARK_EL_LIMIT  (-12.0*DEGREES_TO_RADIANS)
#define MOON_DARK_EL_LIMIT ( -6.0*DEGREES_TO_RADIANS)
/*
   The darkNow() function returns TRUE if the Sun and Moon are far
enough below the horizon for the sky to be very dark.   It returns
FALSE otherwise.   It takes the phase of the moon into account when
deciding how far below the horizon it must be before the sky is
dark.
*/
int darkNow(double tJD)
{
  float dummy;
  double az, zA, el;
  static double savedSunTJD = 0.0;
  static double savedMoonTJD = 0.0;
  static double savedSunRA, savedSunDec, savedMoonRA, savedMoonDec;
  static double sinSavedSunDec, cosSavedSunDec, sinSavedMoonDec, cosSavedMoonDec;
  static float savedPhase, threshold;

  if (fabs(tJD-savedSunTJD) >= 0.5) {
    planetInfo(dataDir, EARTH, tJD, &savedSunRA, &savedSunDec, &dummy, &dummy);
    sinSavedSunDec = sin(savedSunDec);
    cosSavedSunDec = cos(savedSunDec);
    savedSunTJD = tJD;
  }
  azZA(savedSunRA, sinSavedSunDec, cosSavedSunDec, &az, &zA, FALSE);
  el = M_HALF_PI - zA;
  if (el > SUN_DARK_EL_LIMIT)
    return(FALSE);
  if (fabs(tJD-savedMoonTJD) >= 1.0/24.0) {
    planetInfo(dataDir, MOON, tJD, &savedMoonRA, &savedMoonDec, &savedPhase, &dummy);
    threshold = savedPhase*MOON_DARK_EL_LIMIT;
    sinSavedMoonDec = sin(savedMoonDec);
    cosSavedMoonDec = cos(savedMoonDec);
    savedMoonTJD = tJD;
  }
  azZA(savedMoonRA, sinSavedMoonDec, cosSavedMoonDec, &az, &zA, FALSE);
  el = M_HALF_PI - zA;
  if (el > threshold)
    return(FALSE);
  return(TRUE);
}

#define FIFTEEN_MINUTES_IN_DAYS (15.0/1440.0)

/*
  darkTime() returns the number of dark hours between startTJD and endTJD.
*/
float darkTime(double startTJD, double stopTJD, double *begin, double *end)
{
  int savedUseJulianTime;
  float darkHours;
  double tJD;

  savedUseJulianTime = useJulianTime;
  useJulianTime = TRUE;
  darkHours = *begin = *end = 0.0;
  tJD = startTJD;
  while (tJD <= stopTJD) {
    julianDate = tJD;
    myLST = lSTAtTJD(tJD);
    if (darkNow(tJD)) {
      darkHours += 0.25;
      if (*begin == 0)
	*begin = tJD;
    } else if ((*begin != 0.0) && (*end == 0.0))
      *end = tJD;
    tJD += FIFTEEN_MINUTES_IN_DAYS;
  }
  useJulianTime = savedUseJulianTime;
  return(darkHours);
}

#define ABOUT_ROW_STEP    (12)
#define BIGGER_ROW_STEP   (15)
#define BIG_FONT_OFFSET   (10)
#define STAR_SYMBOL_OFFSET (5)
#define REFRACTION_AT_HORIZON (9.89e-3) /* 34 arc minutes - typical */
#define N_RISE_SET_DAYS (5)
#define SUNLINE_X_OFFSET (42)
#define MOONLINE_X_OFFSET (5)
#define MOON_LABEL_SHIFT (112)
#define N_PHASE_DATES (10)
#define MAX_PHASE_DATES (24)

void optsStackableDestroyed(void)
{
  /*
    Here we check to see if the times page has put the app into fast updates
    mode, and if so, we return the app to the default update rate.
  */
  if ((timerID != (guint)0) && fastUpdates) {
    g_source_remove(timerID);
    timerID = (guint)0;
    scheduleUpdates("optsStackableDestroyed", DEFAULT_UPDATE_RATE);
    fastUpdates = FALSE;
  }
  gtk_widget_ref(mainBox);
  gtk_container_remove(GTK_CONTAINER(optsStackable), mainBox);
  gtk_container_add(GTK_CONTAINER(window), mainBox);
  gtk_widget_show(mainBox);
  gtk_widget_unref(mainBox);
  mainBoxInOptsStackable = FALSE;
  smallMooncalDateOffset = 0.0;
  if ((aboutScreen == SOLAR_SYSTEM_SCHEMATIC_SCREEN) ||
      (aboutScreen == SOLAR_SYSTEM_SCALE_SCREEN)) {
    dayInc = 0;
    useCurrentTime = useTextTime = useJulianTime = useCalendarTime = FALSE;
    switch (savedTimeMode) {
    case 1:
      useTextTime = TRUE;
      break;
    case 2:
      useJulianTime = TRUE;
      break;
    case 3:
      useCalendarTime = TRUE;
      break;
    default:
      useCurrentTime = TRUE;
    }
    tJD = savedTJD;
  }
  displayingAnOptsPage = FALSE;
  fullRedraw(FALSE);
}

void radiansToHHMMSS(double radians, int *hH, int *mM, double *sS)
{
  double hours;

  doubleNormalize0to2pi(&radians);
  hours = radians / HOURS_TO_RADIANS;
  *hH = (int)hours;
  *mM = (int)((hours - (double)(*hH))*60.0);
  *sS = (hours - (double)(*hH) - (double)(*mM)/60.0)*3600.0;
}

void tJDToHHMMSS(double tJD, int *hH, int *mM, double *sS)
{
  double dayFrac;

  dayFrac = tJD - (double)((int)tJD) + 0.5;
  while (dayFrac < 0.0)
    dayFrac += 1.0;
  while (dayFrac > 1.0)
    dayFrac -= 1.0;
  radiansToHHMMSS(M_2PI*dayFrac, hH, mM, sS);
}

/*
  Read in all the moon image files into pixmaps.   They are
  used for the monthly moon calendar, etc.
*/
void readMoonImages(void)
{
  char fileName[MAX_FILE_NAME_SIZE];
  
  sprintf(fileName, "%s/icons/%s.xpm", dataDir, "moonImage");
  moonImage = gdk_pixmap_create_from_xpm(pixmap, NULL, NULL, fileName);
  sprintf(fileName, "%s/icons/%s.xpm", dataDir, "moonImage210");
  moonImage210 = gdk_pixmap_create_from_xpm(pixmap, NULL, NULL, fileName);
  sprintf(fileName, "%s/icons/%s.xpm", dataDir, "moonImageFlipped");
  moonImageFlipped = gdk_pixmap_create_from_xpm(pixmap, NULL, NULL, fileName);
  sprintf(fileName, "%s/icons/%s.xpm", dataDir, "moonImage210Flipped");
  moonImage210Flipped = gdk_pixmap_create_from_xpm(pixmap, NULL, NULL, fileName);
  sprintf(fileName, "%s/icons/%s.xpm", dataDir, "blueMoonImage");
  blueMoonImage = gdk_pixmap_create_from_xpm(pixmap, NULL, NULL, fileName);
  sprintf(fileName, "%s/icons/%s.xpm", dataDir, "blueMoonImageFlipped");
  blueMoonImageFlipped = gdk_pixmap_create_from_xpm(pixmap, NULL, NULL, fileName);
  sprintf(fileName, "%s/icons/%s.xpm", dataDir, "fullMoonImage");
  fullMoonImage = gdk_pixmap_create_from_xpm(pixmap, NULL, NULL, fileName);
  sprintf(fileName, "%s/icons/%s.xpm", dataDir, "fullMoonImageFlipped");
  fullMoonImageFlipped = gdk_pixmap_create_from_xpm(pixmap, NULL, NULL, fileName);
  moonImagesRead = TRUE;
}

#define CONJUNCTION          (0)
#define OPPOSITION           (1)
#define GREATEST_WEST_TIME   (2)
#define GREATEST_EAST_TIME   (3)
#define GREATEST_WEST_ANGLE  (4)
#define GREATEST_EAST_ANGLE  (5)

/*
  Calculate the time of planet conjunction or opposition
  (inferior or superior conjunction, for Mercury and Venus)
  using the method given in chapter 36 of Meeus.
*/
double calcConjOrOp(int planet, double m, double t, int phenomenon)
{
  int i;
  double val, tt, di, mRad;
  double *tables[6][7] = {{merIC, venIC, marsC, jupiC, satuC, uranC, neptC},
			  {merSC, venSC, marsO, jupiO, satuO, uranO, neptO},
			  {meGWT, veGWT,  NULL,  NULL,  NULL,  NULL,  NULL},
			  {meGET, veGET,  NULL,  NULL,  NULL,  NULL,  NULL},
			  {meGWD, veGWD,  NULL,  NULL,  NULL,  NULL,  NULL},
			  {meGED, veGED,  NULL,  NULL,  NULL,  NULL,  NULL}};
  double *table;

  table = tables[phenomenon][planet];
  tt = t*t;
  mRad = m*DEGREES_TO_RADIANS;
  val = table[0] + table[1]*t + table[2]*tt;
  for (i = 1; i < 6; i++) {
    di = (double)i;
    val += sin(di*mRad)*(table[6*i - 3] + table[6*i - 2]*t + table[6*i - 1]*tt)
      +    cos(di*mRad)*(table[6*i - 0] + table[6*i + 1]*t + table[6*i + 2]*tt);
  }
  if ((planet > 2) || (phenomenon > 1)) {
    double a, b, c, d, e, f, g;

    a = b = c = d = e = f = g = 0.0;
    if ((planet == 3) || (planet == 4))
      a = 82.74 + 40.76*t;
    if (planet == 4) {
      b = 29.86 + 1181.36*t;
      c = 14.13 + 590.68*t;
      d = 220.02 + 1262.87*t;
    } else if (planet > 4) {
      e = 207.83 + 8.51*t;
      if (planet == 5)
	f = 108.84 + 419.96*t;
      else
	g = 276.74 + 209.98*t;
    }
    doubleNormalize0to360(&a);
    doubleNormalize0to360(&b);
    doubleNormalize0to360(&c);
    doubleNormalize0to360(&d);
    doubleNormalize0to360(&e);
    doubleNormalize0to360(&f);
    doubleNormalize0to360(&g);
    a *= DEGREES_TO_RADIANS;
    b *= DEGREES_TO_RADIANS;
    c *= DEGREES_TO_RADIANS;
    d *= DEGREES_TO_RADIANS;
    e *= DEGREES_TO_RADIANS;
    f *= DEGREES_TO_RADIANS;
    g *= DEGREES_TO_RADIANS;
    val += sin(a)*(table[33] + table[34]*t + table[35]*tt)
      +    cos(a)*(table[36] + table[37]*t + table[38]*tt);
    val += sin(b)*(table[39] + table[40]*t + table[41]*tt)
      +    cos(b)*(table[42] + table[43]*t + table[44]*tt);
    val += sin(c)*(table[45] + table[46]*t + table[47]*tt)
      +    cos(c)*(table[48] + table[49]*t + table[50]*tt);
    val += sin(d)*(table[51] + table[52]*t + table[53]*tt)
      +    cos(d)*(table[54] + table[55]*t + table[56]*tt);
    if (planet > 2)
      val += cos(e)*table[57] + cos(f)*table[60] + cos(g)*table[63];
  }
  return(val);
}

void fixDate(int *year, int *month, int *day, int *hour)
{
  *hour = 0;
  *day = *day+1;
  if (*day > monthLengths[(*month)-1]) {
    *day = 1;
    *month = *month+1;
  } else
    return;
  if (*month > 12) {
    *month = 1;
    *year = *year+1;
  }
}

/*
  These defines are used to set the size of the Planet Elevations
  plot box.
*/
#define PES_LEFT_BORDER   ( 5)
#define PES_RIGHT_BORDER  ( 5)
#define PES_BOTTOM_BORDER (64)
#define PES_TOP_BORDER    (95)
#define PES_PLANET_BASE   (45)

/*
  D R A W  B O U N D E D  S T R I N G

  This routine draws the string passed at location (x, y) unless x is
  too close to the borders of the plot.   If x is too close, then
  the string is printed as close to the border as possible.   This
  prevents the elevation values plotted on the Planet Elevation
  screen from bleeding out of the plot box.
 */
static void drawBoundedString(int color, int x, int y, char *string)
{
  int width;

  width = gdk_string_width(smallFont, string);
  if ((x - width/2) < PES_LEFT_BORDER+2)
    x = PES_LEFT_BORDER + 2 + width/2;
  else if ((x + width/2) > (displayWidth-PES_RIGHT_BORDER-2))
    x = displayWidth - PES_RIGHT_BORDER - width/2 - 2;
  gdk_draw_string(pixmap, smallFont, gC[color], x - width/2, y, string);
}

/*
  C O M P A R E  S T A R  N A M E S

  This function is the comparison function for a call to qsort().
  It compares the names of two stars, allowing an array of them
  to be sorted alphabetically.   It is used by the Celestial
  Navigation page, which wants to have Polaris at the end of
  the list, because it is a special case for navigators.
*/
int compareStarNames(const void *star1, const void *star2)
{
  int nameLen, i, result;
  char *name1, *name2;
  starNameEntry *s1, *s2;

  s1 = *(starNameEntry **)star1;
  s2 = *(starNameEntry **)star2;
  nameLen = s1->nameLen;
  name1 = malloc(nameLen+1);
  if (name1 == NULL) {
    perror("compareStarNames (name1): ");
    exit(ERROR_EXIT);
  }
  for (i = 0; i < nameLen; i++)
    name1[i] = starNameString[s1->offset + i];
  name1[i] = '\0';
  nameLen = s2->nameLen;
  name2 = malloc(nameLen+1);
  if (name2 == NULL) {
    perror("compareStarNames (name2): ");
    exit(ERROR_EXIT);
  }
  for (i = 0; i < nameLen; i++)
    name2[i] = starNameString[s2->offset + i];
  name2[i] = '\0';
  if (strcmp(name1, "Polaris") == 0)
    result = 1;
  else if (strcmp(name2, "Polaris") == 0)
    result = -1;
  else
    result = strcmp(name1, name2);
  free(name1); free(name2);
  return(result);
}

#define EARTH_MAP_WIDTH  (480.0)
#define EARTH_MAP_HEIGHT (250.0)
#define EARTH_MAP_OFFSET (770.0)
#define UMBRA_MAP_WIDTH  (480.0)
#define UMBRA_MAP_HEIGHT (216.0)
#define UMBRA_MAP_OFFSET (410.0)
#define UMBRA_MAP_SCALE  (UMBRA_MAP_HEIGHT/(14.0*MOON_RADIUS))
#define ATMOSPHERIC_UMBRA_EXPANSION (1.05)
/*
  C M  T O  P I X E L S

  Convert positions relative to the center of the earth's umbra,
in cm, to pixels.
*/
void cmToPixels(float x, float y, gint *px, gint *py)
{
  *px = (int)( x * UMBRA_MAP_SCALE + UMBRA_MAP_WIDTH*0.5);
  *py = (int)(-y * UMBRA_MAP_SCALE + UMBRA_MAP_OFFSET);
}

/*
  L A T  L O N  T O  P I X E L S

  Convert latitude and longitude in radians to x and y pixels
  on the display.   Lat runs from -pi/2 to pi/2, long runs
  from -pi to pi.
*/
void latLonToPixels(float lat, float lon, int *x, int *y)
{
  *y = -((lat/M_PI  + 0.5)*EARTH_MAP_HEIGHT) + EARTH_MAP_HEIGHT;
  *x = (lon/M_2PI + 0.5)*EARTH_MAP_WIDTH;
}

/*
  Write out the values that the user can set.
*/
void writeConfigFile(void)
{
  int i;
  char tempName[MAX_FILE_NAME_SIZE], newName[MAX_FILE_NAME_SIZE], oldName[MAX_FILE_NAME_SIZE];
  FILE *newConfigFile;

  sprintf(newName, "%s/config.new", dataDir);
  newConfigFile = fopen(newName, "w");
  if (unlikely(newConfigFile == NULL)) {
    perror("config.new");
    return;
  }
  fprintf(newConfigFile, "CHINESE_COLOR_SCHEME %d\n", chineseColorScheme);
  fprintf(newConfigFile, "LIMITING_MAGNITUDE1 %6.2f\n", limitingMagnitude1);
  fprintf(newConfigFile, "LIMITING_MAGNITUDE2 %6.2f\n", limitingMagnitude2);
  fprintf(newConfigFile, "SHOW_GREAT_CIRCLES1 %d\n", showGreatCircles1);
  fprintf(newConfigFile, "SHOW_GREAT_CIRCLES2 %d\n", showGreatCircles2);
  fprintf(newConfigFile, "USE_ASTERISMS %d\n", useAsterisms);
  fprintf(newConfigFile, "SHOW_DEEP_SKY1 %d\n", showDeepSky1);
  fprintf(newConfigFile, "SHOW_DEEP_SKY2 %d\n", showDeepSky2);
  fprintf(newConfigFile, "SHOW_BAYER1 %d\n", showBayer1);
  fprintf(newConfigFile, "SHOW_BAYER2 %d\n", showBayer2);
  fprintf(newConfigFile, "SHOW_STARS1 %d\n", showStars1);
  fprintf(newConfigFile, "SHOW_STARS2 %d\n", showStars2);
  fprintf(newConfigFile, "SHOW_PLANETS1 %d\n", showPlanets1);
  fprintf(newConfigFile, "SHOW_PLANETS2 %d\n", showPlanets2);
  fprintf(newConfigFile, "SHOW_COMETS1 %d\n", showComets1);
  fprintf(newConfigFile, "SHOW_COMETS2 %d\n", showComets2);
  fprintf(newConfigFile, "SHOW_STAR_NAMES1 %d\n", showNames1);
  fprintf(newConfigFile, "SHOW_STAR_NAMES2 %d\n", showNames2);
  fprintf(newConfigFile, "SHOW_METEORS1 %d\n", showMeteors1);
  fprintf(newConfigFile, "SHOW_METEORS2 %d\n", showMeteors2);
  fprintf(newConfigFile, "USE_GPSD %d\n", useGPSD);
  fprintf(newConfigFile, "INITIAL_AZIMUTH %7.2f\n", initialAzimuth);
  fprintf(newConfigFile, "DEBUG_MESSAGES_ON %d\n", debugMessagesOn);
  fprintf(newConfigFile, "JOVIAN_MOONS_NE %d\n", jovianMoonsNE);
  fprintf(newConfigFile, "JOVIAN_MOONS_NW %d\n", jovianMoonsNW);
  fprintf(newConfigFile, "JOVIAN_MOONS_SE %d\n", jovianMoonsSE);
  fprintf(newConfigFile, "JOVIAN_MOONS_SW %d\n", jovianMoonsSW);
  fprintf(newConfigFile, "LIST_LOCAL_ECLIPSES_ONLY %d\n", listLocalEclipsesOnly);
  fprintf(newConfigFile, "LIST_PENUMBRAL_ECLIPSES %d\n", listPenumbralEclipses);
  fprintf(newConfigFile, "LIST_PARTIAL_ECLIPSES %d\n", listPartialEclipses);
  fprintf(newConfigFile, "LIST_TOTAL_ECLIPSES %d\n", listTotalEclipses);
  sprintf(tempName, "%s", locationName);
  for (i = 0; i < strlen(tempName); i++)
    if (tempName[i] == ' ')
      tempName[i] = '_';
  fprintf(newConfigFile, "DEFAULT_LOCATION_NAME %s\n", tempName);
  fprintf(newConfigFile, "DEFAULT_LOCATION_LATITUDE %f\n", latitude/DEGREES_TO_RADIANS);
  fprintf(newConfigFile, "DEFAULT_LOCATION_LONGITUDE %f\n", longitude/DEGREES_TO_RADIANS);
  fclose(newConfigFile);
  sprintf(oldName, "%s/config", dataDir);
  rename(newName, oldName);
}

/*
  C H E C K  L U N A R  E C L I P S E  S E T T I N G S

  Callback function to read the settings to select an eclipse.
*/
void checkLunarEclipseSettings(void)
{
  int somethingChanged = FALSE;
  int oldValue, i;
  eclipseMenuItem *menuItem, *lastItem = NULL;

  oldValue = listLocalEclipsesOnly;
  if (gtk_toggle_button_get_active((GtkToggleButton *)listLocalEclipsesOnlyButton))
    listLocalEclipsesOnly = TRUE;
  else
    listLocalEclipsesOnly = FALSE;
  if (oldValue != listLocalEclipsesOnly)
    somethingChanged = TRUE;

  oldValue = listPenumbralEclipses;
  if (gtk_toggle_button_get_active((GtkToggleButton *)listPenumbralEclipsesButton))
    listPenumbralEclipses = TRUE;
  else
    listPenumbralEclipses = FALSE;
  if (oldValue != listPenumbralEclipses)
    somethingChanged = TRUE;
  oldValue = listPartialEclipses;
  if (gtk_toggle_button_get_active((GtkToggleButton *)listPartialEclipsesButton))
    listPartialEclipses = TRUE;
  else
    listPartialEclipses = FALSE;
  if (oldValue != listPartialEclipses)
    somethingChanged = TRUE;
  oldValue = listTotalEclipses;
  if (gtk_toggle_button_get_active((GtkToggleButton *)listTotalEclipsesButton))
    listTotalEclipses = TRUE;
  else
    listTotalEclipses = FALSE;
  if (oldValue != listTotalEclipses)
    somethingChanged = TRUE;

  listStartYear = gtk_spin_button_get_value((GtkSpinButton *)startYearSpin);
  listEndYear   = gtk_spin_button_get_value((GtkSpinButton *)endYearSpin);

  /* Do garbage collection */
  lastItem = eclipseMenuItemRoot;
  while (lastItem != NULL) {
    menuItem = lastItem;
    lastItem = lastItem->next;
    free(menuItem->name);
    free(menuItem);
  }
  eclipseMenuItemRoot = NULL;
  for (i = 0; i <= nEclipsesForMenu; i++) {
    free(lunarItemFactoryEntry[i].path);
    if (lunarItemFactoryEntry[i].item_type != NULL)
      free(lunarItemFactoryEntry[i].item_type);
  }
  free(lunarItemFactoryEntry);
  gtk_widget_destroy(optsStackable);
  if (somethingChanged)
    writeConfigFile();
}

void lunarCategoryCallback(gpointer callbackData, guint callbackAction, GtkWidget *widget)
{
  displayEclipse = TRUE;
  selectedLunarEclipse = callbackAction;
  gtk_widget_destroy(lunarEclipseStackable);
  putOptsPage(LUNAR_ECLIPSES);
}

void rebuildEclipseListCallback(gpointer callbackData, guint callbackAction, GtkWidget *widget)
{
  gtk_widget_destroy(lunarEclipseStackable);
  putOptsPage(LUNAR_ECLIPSES);
}

/*
  R E A D  E C L I P S E  D A T A

  Read in the binary file that contains the Cannon of Lunar Eclipses.
*/
void readEclipseData(void)
{
  lunarEclipses = (lunarEclipse *)malloc(N_LUNAR_ECLIPSES*sizeof(lunarEclipse));
  if (lunarEclipses == NULL) {
    perror("Allocating lunarEclipses array");
    exit(ERROR_EXIT);
  } else {
    int i, fD;
    char cannonFileName[MAX_FILE_NAME_SIZE];
    
    sprintf(cannonFileName, "%s/lunarEclipseCannon", dataDir);
    fD = open(cannonFileName, O_RDONLY);
    if (fD < 0) {
      perror(cannonFileName);
      exit(ERROR_EXIT);
    } else {
      for (i = 0; i < N_LUNAR_ECLIPSES; i++) {
	read(fD, &lunarEclipses[i], sizeof(lunarEclipse));
      }
    }
    close(fD);
  }
}

/*
  Draw various pages on selected from the "opts" menu
*/
static void drawOptsScreens(void)
{
  int stringWidth, rAHH, rAMM, decDD, decMM, tWidth, tHeight;
  int row = 1;
  int schematic = FALSE;
  int magIndex = 0;
  int r, c, w, h, sunInfoRow, moonInfoRow;
  float n = 1.0;
  float illum, mag, rAHours, decDegrees, el, sunAngle, sunAz, sunZA, illumR, illumS;
  double lSTNow, hA, az, zA, h0, fracTJD;
  double sunRA, sunDec, cosH;
  double nextNew[MAX_PHASE_DATES], nextFirstQ[MAX_PHASE_DATES], nextFull[MAX_PHASE_DATES], nextLastQ[MAX_PHASE_DATES];
  double tJDS[N_RISE_SET_DAYS], theAz, theTime, hD, tempDay, sunSet;
  double sunRise = 0.0;
  int day, year, month, dayNum, circumpolar;
  int nextRiseDone, nextSetDone;
  char scratchString[100];
  GdkPoint points[3];
  GdkGC *labelGC, *textGC;

  if (!mainBoxInOptsStackable) {
    optsStackable = hildon_stackable_window_new();
    g_signal_connect(G_OBJECT(optsStackable), "destroy",
		     G_CALLBACK(optsStackableDestroyed), NULL);
    gtk_widget_ref(mainBox);
    gtk_widget_hide(mainBox);
    gtk_container_remove(GTK_CONTAINER(window), mainBox);
    gtk_container_add(GTK_CONTAINER(optsStackable), mainBox);
    gtk_widget_unref(mainBox);
    mainBoxInOptsStackable = TRUE;
  }

  gtk_widget_show_all(optsStackable);
  switch (aboutScreen) {
  case ABOUT_SCREEN:
    row = 1;
    sprintf(scratchString, "Orrery Version %s", orreryVersion);
    renderPangoText(scratchString, OR_CREAM, MEDIUM_PANGO_FONT, &tWidth, &tHeight,
		    pixmap, displayWidth >> 1, row*ABOUT_ROW_STEP,
		    0.0, TRUE, 0);
    row++;
    gdk_draw_line(pixmap, gC[OR_BLUE],
		  xBorder, row*BIGGER_ROW_STEP, displayWidth-xBorder, row*BIGGER_ROW_STEP);
    row++;
    renderPangoText("Stellar Magnitudes", OR_WHITE, SMALL_PANGO_FONT, &tWidth, &tHeight,
		    pixmap, displayWidth >> 1, row*BIGGER_ROW_STEP,
		    0.0, TRUE, 0);
    row += 3;
    for (c = 0; c < 2; c++)
      for (r = 0; r < 8; r++) {
	int ii;

	if ((c == 1) && (r == 7))
	  break;
	if ((r == 0) && (c == 0))
	  sprintf(scratchString, "      < %4.1f", -0.5/magScale);
	else if ((r == 7) && (c == 1))
	  sprintf(scratchString, "      > %4.1f", (0.5*n - 1.5)/magScale);
	else
	  sprintf(scratchString, "%4.1f -> %4.1f", (0.5*n - 1.5)/magScale,
		  (0.5*(n+1.0) - 1.5)/magScale);
	n += 1.0;
	scratchString[0] = toupper(scratchString[0]);
	/*
	  The zero character in the monospaced character set has a dot in the center, which I think looks
	  ugly.   So I switch the zero characters to capital Os here.
	 */
	for (ii = 0; ii < strlen(scratchString); ii++)
	  if (scratchString[ii] == '0')
	    scratchString[ii] = 'O';
	renderPangoText(scratchString, OR_CREAM, SMALL_MONO_PANGO_FONT, &tWidth, &tHeight,
			pixmap, (displayWidth/2)*c + 90, (row + 2*r)*(BIGGER_ROW_STEP-2) - 4,
			0.0, TRUE, 0);
	if (magIndex <= 11)
	  gdk_draw_arc(pixmap, gC[OR_WHITE], TRUE,
		       (displayWidth/2)*c + 200-(starSize[magIndex]/2),
		       (row + 2*r)*(BIGGER_ROW_STEP-2)-(starSize[magIndex]/2) - STAR_SYMBOL_OFFSET,
		       starSize[magIndex], starSize[magIndex], 0, FULL_CIRCLE);
	else if (magIndex == 12) {
	  gdk_draw_point(pixmap, gC[OR_WHITE], (displayWidth/2)*c + 200, (row + 2*r)*(BIGGER_ROW_STEP-2) - STAR_SYMBOL_OFFSET);
	  gdk_draw_point(pixmap, gC[OR_WHITE], (displayWidth/2)*c + 201, (row + 2*r)*(BIGGER_ROW_STEP-2) - STAR_SYMBOL_OFFSET);
	} else if (magIndex == 13) 
	  gdk_draw_point(pixmap, gC[OR_GREY], (displayWidth/2)*c + 200, (row + 2*r)*(BIGGER_ROW_STEP-2) - STAR_SYMBOL_OFFSET);
	else
	  gdk_draw_point(pixmap, gC[OR_DARK_GREY], (displayWidth/2)*c + 200, (row + 2*r)*(BIGGER_ROW_STEP-2) - STAR_SYMBOL_OFFSET);
	magIndex += 1;
      }
    row += 17;
    renderPangoText("A decrease of 5 mag. is a 100 x increase in brightness",
		    OR_BLUE, SMALL_PANGO_FONT, &tWidth, &tHeight,
		    pixmap, displayWidth/2, row*ABOUT_ROW_STEP + 2,
		    0.0, TRUE, 0);
    row += 2;
    gdk_draw_line(pixmap, gC[OR_BLUE],
		  xBorder, row*ABOUT_ROW_STEP - 5, displayWidth-xBorder, row*ABOUT_ROW_STEP - 5);
    row += 1;
    renderPangoText("Solar System Symbols",
		    OR_WHITE, SMALL_PANGO_FONT, &tWidth, &tHeight,
		    pixmap, displayWidth/2, row*ABOUT_ROW_STEP - 1,
		    0.0, TRUE, 0);
    row -= 3;
    for (r = 0; r < 4; r++)
      for (c = 0; c < 3; c++) {
	if ((r == 3) && (c == 2)) {
	  renderPangoText("Vernal", OR_CREAM, SMALL_PANGO_FONT, &tWidth, &tHeight,
			  pixmap, (displayWidth/3)*c + 30, (row + 2*r)*BIGGER_ROW_STEP - 5,
			  0.0, FALSE, 0);
	  renderPangoText("Equinox", OR_CREAM, SMALL_PANGO_FONT, &tWidth, &tHeight,
			  pixmap, (displayWidth/3)*c + 30, (row + 2*r)*BIGGER_ROW_STEP + 15,
			  0.0, FALSE, 0);
	  gdk_drawable_get_size(firstPointImage, &w, &h);
	  gdk_draw_drawable(pixmap, gC[OR_BLUE], firstPointImage, 0, 0,
			    (displayWidth/3)*c + 117 - w/2,
			    (row + 2*r)*BIGGER_ROW_STEP-h/2 + 4, w, h);
	} else if ((r == 2) && (c == 2)) {
	  renderPangoText("Meteors", OR_CREAM, SMALL_PANGO_FONT, &tWidth, &tHeight,
			  pixmap, (displayWidth/3)*c + 30, (row + 2*r)*BIGGER_ROW_STEP - 1,
			  0.0, FALSE, 0);
	  gdk_drawable_get_size(meteorRadiantImage[0], &w, &h);
	  gdk_draw_drawable(pixmap, gC[OR_BLUE], meteorRadiantImage[0], 0, 0,
			    (displayWidth/3)*c + 117 - w/2, (row + 2*r)*BIGGER_ROW_STEP-h/2 - 1, 20, 20);
	} else {
	  renderPangoText(solarSystemNames[c*4 + r], OR_CREAM, SMALL_PANGO_FONT, &tWidth, &tHeight,
			  pixmap, (displayWidth/3)*c + 30, (row + 2*r)*BIGGER_ROW_STEP,
			  0.0, FALSE, 0);
	  gdk_drawable_get_size(planetImages[c*4 + r], &w, &h);
	  gdk_draw_drawable(pixmap, gC[OR_BLUE], planetImages[c*4 + r], 0, 0,
			    (displayWidth/3)*c + 117 - w/2, (row + 2*r)*BIGGER_ROW_STEP-h/2, 20, 20);
	}
      }
    row += 15;
    gdk_draw_line(pixmap, gC[OR_BLUE],
		  xBorder, row*ABOUT_ROW_STEP + 7, displayWidth-xBorder, row*ABOUT_ROW_STEP + 7);
    row += 2;
    renderPangoText("Deep Sky Objects",
		    OR_WHITE, SMALL_PANGO_FONT, &tWidth, &tHeight,
		    pixmap, displayWidth/2, row*ABOUT_ROW_STEP - 1,
		    0.0, TRUE, 0);
    row += 2;
    renderPangoText("Supernova Remnant",
		    OR_CREAM, SMALL_PANGO_FONT, &tWidth, &tHeight,
		    pixmap, 10, row*ABOUT_ROW_STEP,
		    0.0, FALSE, 0);
    plotDeepSkyObject(SUPERNOVA_REMNENT, (displayWidth>>1) - 35, row*ABOUT_ROW_STEP);
    renderPangoText("Open Cluster",
		    OR_CREAM, SMALL_PANGO_FONT, &tWidth, &tHeight,
		    pixmap, (displayWidth>>1) + 15, row*ABOUT_ROW_STEP,
		    0.0, FALSE, 0);
    plotDeepSkyObject(OPEN_CLUSTER, displayWidth - 30, row*ABOUT_ROW_STEP);
    row += 2;
    renderPangoText("Globular Cluster",
		    OR_CREAM, SMALL_PANGO_FONT, &tWidth, &tHeight,
		    pixmap, 10, row*ABOUT_ROW_STEP,
		    0.0, FALSE, 0);
    plotDeepSkyObject(GLOBULAR_CLUSTER, (displayWidth>>1) - 35, row*ABOUT_ROW_STEP);
    renderPangoText("Diffuse Nebula",
		    OR_CREAM, SMALL_PANGO_FONT, &tWidth, &tHeight,
		    pixmap, (displayWidth>>1) + 15, row*ABOUT_ROW_STEP,
		    0.0, FALSE, 0);
    plotDeepSkyObject(DIFFUSE_NEBULA, displayWidth - 30, row*ABOUT_ROW_STEP);
    row += 2;
    renderPangoText("Planetary Nebula",
		    OR_CREAM, SMALL_PANGO_FONT, &tWidth, &tHeight,
		    pixmap, 10, row*ABOUT_ROW_STEP,
		    0.0, FALSE, 0);
    plotDeepSkyObject(PLANETARY_NEBULA, (displayWidth>>1) - 35, row*ABOUT_ROW_STEP);
    renderPangoText("Galaxy",
		    OR_CREAM, SMALL_PANGO_FONT, &tWidth, &tHeight,
		    pixmap, (displayWidth>>1) + 15, row*ABOUT_ROW_STEP,
		    0.0, FALSE, 0);
    plotDeepSkyObject(GALAXY, displayWidth - 30, row*ABOUT_ROW_STEP);
    row += 1;
    gdk_draw_line(pixmap, gC[OR_BLUE],
		  xBorder, row*ABOUT_ROW_STEP + 7, displayWidth-xBorder, row*ABOUT_ROW_STEP + 7);
    row += 2;
    renderPangoText("Constellation Colors",
		    OR_WHITE, SMALL_PANGO_FONT, &tWidth, &tHeight,
		    pixmap, displayWidth/2, row*ABOUT_ROW_STEP - 1,
		    0.0, TRUE, 0);
    row += 2;
    if (chineseColorScheme) {
      renderPangoText("Zodiac",
		      OR_YELLOW, SMALL_PANGO_FONT, &tWidth, &tHeight,
		      pixmap, (displayWidth/6) - 20, row*ABOUT_ROW_STEP,
		      0.0, TRUE, 0);
      renderPangoText("Ptolemaic",
		      OR_PINK, SMALL_PANGO_FONT, &tWidth, &tHeight,
		      pixmap, (displayWidth>>1) - 20, row*ABOUT_ROW_STEP,
		      0.0, TRUE, 0);
    } else {
      renderPangoText("Zodiac",
		      OR_PINK, SMALL_PANGO_FONT, &tWidth, &tHeight,
		      pixmap, (displayWidth/6) - 20, row*ABOUT_ROW_STEP,
		      0.0, TRUE, 0);
      renderPangoText("Ptolemaic",
		      OR_YELLOW, SMALL_PANGO_FONT, &tWidth, &tHeight,
		      pixmap, (displayWidth>>1) - 20, row*ABOUT_ROW_STEP,
		      0.0, TRUE, 0);
    }
    renderPangoText("Modern Additions",
		    OR_BLUE, SMALL_PANGO_FONT, &tWidth, &tHeight,
		    pixmap, ((5*displayWidth)/6) - 20, row*ABOUT_ROW_STEP,
		    0.0, TRUE, 0);
    row += 1;
    gdk_draw_line(pixmap, gC[OR_BLUE],
		  xBorder, row*ABOUT_ROW_STEP + 7, displayWidth-xBorder, row*ABOUT_ROW_STEP + 7);
    row += 2;
    renderPangoText("Great Circles",
		    OR_WHITE, SMALL_PANGO_FONT, &tWidth, &tHeight,
		    pixmap, displayWidth/2, row*ABOUT_ROW_STEP - 1,
		    0.0, TRUE, 0);
    row += 2;
    if (chineseColorScheme) {
      renderPangoText("Celestial Equator",
		      OR_RED, SMALL_PANGO_FONT, &tWidth, &tHeight,
		      pixmap, (displayWidth/6), row*ABOUT_ROW_STEP - 1,
		      0.0, TRUE, 0);
      renderPangoText("Ecliptic",
		      OR_EQUATOR, SMALL_PANGO_FONT, &tWidth, &tHeight,
		      pixmap, displayWidth>>1, row*ABOUT_ROW_STEP - 1,
		      0.0, TRUE, 0);
    } else {
      renderPangoText("Celestial Equator",
		      OR_EQUATOR, SMALL_PANGO_FONT, &tWidth, &tHeight,
		      pixmap, (displayWidth/6), row*ABOUT_ROW_STEP - 1,
		      0.0, TRUE, 0);
      renderPangoText("Ecliptic",
		      OR_RED, SMALL_PANGO_FONT, &tWidth, &tHeight,
		      pixmap, displayWidth>>1, row*ABOUT_ROW_STEP - 1,
		      0.0, TRUE, 0);
    }
    renderPangoText("Galactic Plane",
		    OR_BLUE_GREEN, SMALL_PANGO_FONT, &tWidth, &tHeight,
		    pixmap, (5*displayWidth)/6, row*ABOUT_ROW_STEP - 1,
		    0.0, TRUE, 0);
    row = 48;
    break;
  case SOLUNI_SCREEN:
    if (!moonImagesRead)
      readMoonImages();
    textGC = gC[OR_WHITE];
    needNewTime = TRUE;
    lSTNow = lST();
    strcpy(scratchString, "Sun and Moon Information at    ");
    if (useCurrentTime) {
      renderPangoText(scratchString, OR_CREAM, SMALL_PANGO_FONT, &tWidth, &tHeight,
		      pixmap, 143, row*ABOUT_ROW_STEP + BIG_FONT_OFFSET,
		      0.0, TRUE, 0);
      makeTimeString(scratchString, FALSE);
      renderPangoText(scratchString, OR_WHITE, SMALL_PANGO_FONT, &tWidth, &tHeight,
		      pixmap, 367, row*ABOUT_ROW_STEP + BIG_FONT_OFFSET,
		      0.0, TRUE, 0);
    } else {
      renderPangoText(scratchString, OR_CREAM, SMALL_PANGO_FONT, &tWidth, &tHeight,
		      pixmap, displayWidth >> 1, row*ABOUT_ROW_STEP + BIG_FONT_OFFSET,
		      0.0, TRUE, 0);
      makeTimeString(scratchString, FALSE);
      renderPangoText(scratchString, OR_RED, SMALL_PANGO_FONT, &tWidth, &tHeight,
		      pixmap, displayWidth >> 1, (row+2)*ABOUT_ROW_STEP + BIG_FONT_OFFSET,
		      0.0, TRUE, 0);
    }
    strcpy(scratchString, "Sunrise             UT");
    renderPangoText(scratchString, OR_CREAM, MEDIUM_PANGO_FONT, &tWidth, &tHeight,
		    pixmap, displayWidth >> 2, (row+4)*ABOUT_ROW_STEP + BIG_FONT_OFFSET,
		    0.0, TRUE, 0);
    strcpy(scratchString, "Sunset             UT");
    renderPangoText(scratchString, OR_CREAM, MEDIUM_PANGO_FONT, &tWidth, &tHeight,
		    pixmap, (3*displayWidth) >> 2, (row+4)*ABOUT_ROW_STEP + BIG_FONT_OFFSET,
		    0.0, TRUE, 0);
    sunInfoRow = (row+4)*ABOUT_ROW_STEP + BIG_FONT_OFFSET;
    strcpy(scratchString, "Moonrise           UT");
    renderPangoText(scratchString, OR_CREAM, MEDIUM_PANGO_FONT, &tWidth, &tHeight,
		    pixmap, displayWidth >> 2, (row+11)*ABOUT_ROW_STEP + BIG_FONT_OFFSET,
		    0.0, TRUE, 0);
    strcpy(scratchString, "Moonset            UT");
    renderPangoText(scratchString, OR_CREAM, MEDIUM_PANGO_FONT, &tWidth, &tHeight,
		    pixmap, (3*displayWidth) >> 2, (row+11)*ABOUT_ROW_STEP + BIG_FONT_OFFSET,
		    0.0, TRUE, 0);
    moonInfoRow = (row+11)*ABOUT_ROW_STEP + BIG_FONT_OFFSET;
    row += 24;
    renderPangoText("Sun", OR_CREAM, MEDIUM_PANGO_FONT, &tWidth, &tHeight,
		    pixmap, displayWidth >> 1, (row++)*ABOUT_ROW_STEP, 0.0, TRUE, 0);
    renderPangoText("RA", OR_CREAM, SMALL_PANGO_FONT, &tWidth, &tHeight,
		    pixmap, 7, row*ABOUT_ROW_STEP + 11, 0.0, FALSE, 0);
    renderPangoText("Dec", OR_CREAM, SMALL_PANGO_FONT, &tWidth, &tHeight,
		    pixmap, 10 + displayWidth/5, row*ABOUT_ROW_STEP + 11, 0.0, FALSE, 0);
    renderPangoText("HA", OR_CREAM, SMALL_PANGO_FONT, &tWidth, &tHeight,
		    pixmap, 16 + (2*displayWidth)/5, row*ABOUT_ROW_STEP + 11, 0.0, FALSE, 0);
    renderPangoText("Az", OR_CREAM, SMALL_PANGO_FONT, &tWidth, &tHeight,
		    pixmap, 16 + (3*displayWidth)/5, row*ABOUT_ROW_STEP + 11, 0.0, FALSE, 0);
    renderPangoText("El", OR_CREAM, SMALL_PANGO_FONT, &tWidth, &tHeight,
		    pixmap, 16 + (4*displayWidth)/5, row*ABOUT_ROW_STEP + 11, 0.0, FALSE, 0);
    planetInfo(dataDir, EARTH, tJD, &sunRA, &sunDec, &illum, &mag);
    rAHours = sunRA/HOURS_TO_RADIANS; decDegrees = sunDec/DEGREES_TO_RADIANS;
    rAHH = (int)rAHours;
    rAMM = (int)((rAHours - (float)rAHH)*60.0);
    sprintf(scratchString, "%02d:%02d", rAHH, rAMM);
    renderPangoText(scratchString, OR_WHITE, SMALL_PANGO_FONT, &tWidth, &tHeight,
		    pixmap, 18 + displayWidth/10, row*ABOUT_ROW_STEP + 11, 0.0, TRUE, 0);
    decDD = (int)decDegrees;
    decMM = (int)((decDegrees - (float)decDD)*60.0);
    if (decDegrees > 0.0)
      sprintf(scratchString, "%02d:%02d", decDD, decMM);
    else
      sprintf(scratchString, "-%02d:%02d", -decDD, -decMM);
    renderPangoText(scratchString, OR_WHITE, SMALL_PANGO_FONT, &tWidth, &tHeight,
		    pixmap, 28 + (3*displayWidth)/10, row*ABOUT_ROW_STEP + 11, 0.0, TRUE, 0);
    hA = lSTNow - sunRA;
    rAHours = hA/HOURS_TO_RADIANS;
    sprintf(scratchString, "%6.2f", rAHours);
    renderPangoText(scratchString, OR_WHITE, SMALL_PANGO_FONT, &tWidth, &tHeight,
		    pixmap, 21 + (5*displayWidth)/10, row*ABOUT_ROW_STEP + 11, 0.0, TRUE, 0);
    azZA(sunRA, sin(sunDec), cos(sunDec), &az, &zA, FALSE);
    sunAz = az; sunZA = zA;
    az /= DEGREES_TO_RADIANS;
    if (az < 0.0)
      az += 360.0;
    if (az > 360.0)
      az -= 360.0;
    sprintf(scratchString, "%5.1f", az);
    renderPangoText(scratchString, OR_WHITE, SMALL_PANGO_FONT, &tWidth, &tHeight,
		    pixmap, 20 + (7*displayWidth)/10, row*ABOUT_ROW_STEP + 11, 0.0, TRUE, 0);
    el = M_HALF_PI - zA;
    el += refraction(el);
    sprintf(scratchString, "%5.1f", el/DEGREES_TO_RADIANS);
    renderPangoText(scratchString, OR_WHITE, SMALL_PANGO_FONT, &tWidth, &tHeight,
		    pixmap, 16 + (9*displayWidth)/10, row*ABOUT_ROW_STEP + 11, 0.0, TRUE, 0);
    row += 2;
    /* Calculate time of sun rise, transit and setting */
    nextRiseDone = nextSetDone = FALSE;
    fracTJD = tJD - (double)((int)tJD);
    h0 = -(50.0/60.0)*DEGREES_TO_RADIANS;
    for (day = 0; day < N_RISE_SET_DAYS; day++) {
      if (fracTJD < 0.5)
	tJDS[day] = (double)((int)tJD) + 0.5 + longitude/M_2PI + (double)(day - N_RISE_SET_DAYS/2);
      else
	tJDS[day] = (double)((int)tJD) + 1.5 + longitude/M_2PI + (double)(day - N_RISE_SET_DAYS/2);
      tJDToDate(tJDS[day], &year, &month, &dayNum);
      sprintf(scratchString, "%02d/%02d/%04d", month, dayNum, year);
      if (day - N_RISE_SET_DAYS/2 == 0)
	gdk_draw_string(pixmap, smallFont, gC[OR_WHITE],
			SUNLINE_X_OFFSET, row*ABOUT_ROW_STEP + BIG_FONT_OFFSET, scratchString);
      else
	gdk_draw_string(pixmap, smallFont, gC[OR_GREY],
			SUNLINE_X_OFFSET, row*ABOUT_ROW_STEP + BIG_FONT_OFFSET, scratchString);
      lSTNow = lSTAtTJD(tJDS[day]);
      /* Sunrise */
      theTime = calcRiseOrSetTime(TRUE, EARTH, tJDS[day], lSTNow, h0, &circumpolar, &hD,
				  &cosH, &sunDec, &illum);
      if (circumpolar) {
	gdk_draw_string(pixmap, smallFont, gC[OR_DARK_CREAM],
			SUNLINE_X_OFFSET+gdk_string_width(smallFont, "88/88/8888  "),
			row*ABOUT_ROW_STEP + BIG_FONT_OFFSET, "Sunrise");
	gdk_draw_string(pixmap, smallFont, gC[OR_GREY],
			SUNLINE_X_OFFSET+gdk_string_width(smallFont, "88/88/8888  Sunrise "),
			row*ABOUT_ROW_STEP + BIG_FONT_OFFSET, "(none)");
	if (day == (N_RISE_SET_DAYS/2))
	  renderPangoText("(none)      ", OR_WHITE, MEDIUM_PANGO_FONT, &tWidth, &tHeight,
			  pixmap, 117, sunInfoRow, 0.0, FALSE, 0);
      } else {
	theAz = atan2(sin(hD), cosH*sinLatitude-tan(sunDec)*cosLatitude) + M_PI;
	doubleNormalize0to2pi(&theAz);
	if ((!nextRiseDone) && (theTime - tJD >= 0.0)) {
	  labelGC = gC[OR_CREAM];
	  textGC = gC[OR_WHITE];
	  nextRiseDone = TRUE;
	} else {
	  labelGC = gC[OR_DARK_CREAM];
	  textGC = gC[OR_GREY];
	}
	sunRise = theTime;
	theTime -= 0.5;
	theTime = theTime - (double)((int)theTime);
	rAHours = theTime*24.0;
	rAHH = (int)rAHours;
	rAMM = (int)((rAHours - (float)rAHH)*60.0 + 0.5);
	fix60s(&rAHH, &rAMM);
	sprintf(scratchString, "%02d:%02d ", rAHH, rAMM);
	gdk_draw_string(pixmap, smallFont, labelGC,
			SUNLINE_X_OFFSET+gdk_string_width(smallFont, "88/88/8888  "),
			row*ABOUT_ROW_STEP + BIG_FONT_OFFSET, "Sunrise");
	if (day == (N_RISE_SET_DAYS/2))
	  renderPangoText(scratchString, OR_WHITE, MEDIUM_PANGO_FONT, &tWidth, &tHeight,
			  pixmap, 150, sunInfoRow, 0.0, TRUE, 0);
	gdk_draw_string(pixmap, smallFont, textGC,
			SUNLINE_X_OFFSET+gdk_string_width(smallFont, "88/88/8888  Sunrise "),
			row*ABOUT_ROW_STEP + BIG_FONT_OFFSET, scratchString);
	gdk_draw_string(pixmap, smallFont, labelGC,
			SUNLINE_X_OFFSET+gdk_string_width(smallFont, "88/88/8888  Sunrise 88:88 "),
			row*ABOUT_ROW_STEP + BIG_FONT_OFFSET, "UT Az =");
	sprintf(scratchString, "%3.0f", theAz/DEGREES_TO_RADIANS);
	gdk_draw_string(pixmap, smallFont, textGC,
			SUNLINE_X_OFFSET+gdk_string_width(smallFont, "88/88/8888  Sunrise 88:88 UT Az = "),
			row*ABOUT_ROW_STEP + BIG_FONT_OFFSET, scratchString);
      }
      /* Sunset */
      theTime = calcRiseOrSetTime(FALSE, EARTH, tJDS[day], lSTNow, h0, &circumpolar, &hD,
				  &cosH, &sunDec, &illum);
      if (circumpolar) {
	gdk_draw_string(pixmap, smallFont, gC[OR_DARK_CREAM],
			SUNLINE_X_OFFSET+gdk_string_width(smallFont, "88/88/8888  Sunrise 88:88 UT Az = 888.8  "),
			row*ABOUT_ROW_STEP + BIG_FONT_OFFSET, "Sunset");
	gdk_draw_string(pixmap, smallFont, gC[OR_GREY],
			SUNLINE_X_OFFSET+gdk_string_width(smallFont, "88/88/8888  Sunrise 88:88 UT Az = 888.8  Sunset "),
			row*ABOUT_ROW_STEP + BIG_FONT_OFFSET, "(none)");
	if (day == (N_RISE_SET_DAYS/2))
	  renderPangoText("(none)      ", OR_WHITE, MEDIUM_PANGO_FONT, &tWidth, &tHeight,
			  pixmap, 351, sunInfoRow, 0.0, FALSE, 0);
      } else {
	theAz = atan2(sin(hD), cosH*sinLatitude-tan(sunDec)*cosLatitude) + M_PI;
	doubleNormalize0to2pi(&theAz);
	if ((!nextSetDone) && (theTime - tJD >= 0.0)) {
	  labelGC = gC[OR_CREAM];
	  textGC = gC[OR_WHITE];
	  nextSetDone = TRUE;
	} else {
	  labelGC = gC[OR_DARK_CREAM];
	  textGC = gC[OR_GREY];
	}
	sunSet = theTime;
	theTime -= 0.5;
	theTime = theTime - (double)((int)theTime);
	rAHours = theTime*24.0;
	rAHH = (int)rAHours;
	rAMM = (int)((rAHours - (float)rAHH)*60.0 + 0.5);
	fix60s(&rAHH, &rAMM);
	sprintf(scratchString, "%02d:%02d ", rAHH, rAMM);
	gdk_draw_string(pixmap, smallFont, labelGC,
			SUNLINE_X_OFFSET+gdk_string_width(smallFont, "88/88/8888  Sunrise 88:88 UT Az = 888.8  "),
			row*ABOUT_ROW_STEP + BIG_FONT_OFFSET, "Sunset");
	gdk_draw_string(pixmap, smallFont, textGC,
			SUNLINE_X_OFFSET+gdk_string_width(smallFont, "88/88/8888  Sunrise 88:88 UT Az = 888.8  Sunset "),
			row*ABOUT_ROW_STEP + BIG_FONT_OFFSET, scratchString);
	gdk_draw_string(pixmap, smallFont, labelGC,
			SUNLINE_X_OFFSET+gdk_string_width(smallFont, "88/88/8888  Sunrise 88:88 UT Az = 888.8  Sunset 88:88 "),
			row*ABOUT_ROW_STEP + BIG_FONT_OFFSET, "UT Az =");
	if (day == (N_RISE_SET_DAYS/2)) {
	  int hH, mM;
	  double sS;

	  renderPangoText(scratchString, OR_WHITE, MEDIUM_PANGO_FONT, &tWidth, &tHeight,
			  pixmap, 385, sunInfoRow, 0.0, TRUE, 0);
	  radiansToHHMMSS((sunSet - sunRise) * M_2PI, &hH, &mM, &sS);
	  if (sS > 30.0) {
	    mM += 1;
	  }
	  fix60s(&hH, &mM);
	  renderPangoText("Length of day", OR_CREAM, MEDIUM_PANGO_FONT, &tWidth, &tHeight,
			  pixmap, 197, sunInfoRow + 3*ABOUT_ROW_STEP,
			  0.0, TRUE, 0);
	  sprintf(scratchString, "%02d:%02d", hH, mM);
	  renderPangoText(scratchString, OR_WHITE, MEDIUM_PANGO_FONT, &tWidth, &tHeight,
			  pixmap, 315, sunInfoRow + 3*ABOUT_ROW_STEP, 0.0, TRUE, 0);
	}
	sprintf(scratchString, "%3.0f", theAz/DEGREES_TO_RADIANS);
	  gdk_draw_string(pixmap, smallFont, textGC,
			  SUNLINE_X_OFFSET+gdk_string_width(smallFont, "88/88/8888  Sunrise 88:88 UT Az = 888.8  Sunset 88:88 UT Az = "),
			  row*ABOUT_ROW_STEP + BIG_FONT_OFFSET, scratchString);
      }
      row++;
    }
    row++;
    sprintf(scratchString, "Starting Times for %d Seasons", (int)cYear);
    renderPangoText(scratchString, OR_CREAM, SMALL_PANGO_FONT, &tWidth, &tHeight,
		    pixmap, displayWidth >> 1, (row++)*ABOUT_ROW_STEP, 0.0, TRUE, 0);
    row++;
    gdk_draw_string(pixmap, smallFont, gC[OR_CREAM], 3, row*ABOUT_ROW_STEP, "Spring");
    gdk_draw_string(pixmap, smallFont, gC[OR_CREAM], 7 + (displayWidth >> 2), row*ABOUT_ROW_STEP, "Summer");
    gdk_draw_string(pixmap, smallFont, gC[OR_CREAM], 11 + (displayWidth >> 1), row*ABOUT_ROW_STEP, "Fall");
    gdk_draw_string(pixmap, smallFont, gC[OR_CREAM], 5 + 3*(displayWidth >> 2), row*ABOUT_ROW_STEP, "Winter");
    {
      int hh, mm;
      double spring, summer, fall, winter, fDay;
      char shortMonthName[4];

      if (northernHemisphere)
	seasons(dataDir, (int)cYear, &spring, &summer, &fall, &winter);
      else
	seasons(dataDir, (int)cYear, &fall, &winter, &spring, &summer);
      tJDToDate(spring, &year, &month, &dayNum);
      spring -= 0.5;
      fDay = spring - (double)((int)spring);
      hh = (int)(fDay * 24.0);
      mm = (int)((fDay - ((double)hh)/24.0) * 1440.0 + 0.5);
      shortMonthName[3] = (char)0;
      strncpy(shortMonthName, monthName[month-1], 3);
      sprintf(scratchString, "%s %d %02d:%02d", shortMonthName, dayNum, hh, mm);
      gdk_draw_string(pixmap, smallFont, gC[OR_WHITE], gdk_string_width(smallFont, "Spring") + 8, row*ABOUT_ROW_STEP, scratchString);

      tJDToDate(summer, &year, &month, &dayNum);
      summer -= 0.5;
      fDay = summer - (double)((int)summer);
      hh = (int)(fDay * 24.0);
      mm = (int)((fDay - ((double)hh)/24.0) * 1440.0 + 0.5);
      strncpy(shortMonthName, monthName[month-1], 3);
      sprintf(scratchString, "%s %d %02d:%02d", shortMonthName, dayNum, hh, mm);
      gdk_draw_string(pixmap, smallFont, gC[OR_WHITE],
		      (displayWidth >> 2) + gdk_string_width(smallFont, "Summer") + 12, row*ABOUT_ROW_STEP, scratchString);

      tJDToDate(fall, &year, &month, &dayNum);
      fall -= 0.5;
      fDay = fall - (double)((int)fall);
      hh = (int)(fDay * 24.0);
      mm = (int)((fDay - ((double)hh)/24.0) * 1440.0 + 0.5);
      strncpy(shortMonthName, monthName[month-1], 3);
      sprintf(scratchString, "%s %d %02d:%02d", shortMonthName, dayNum, hh, mm);
      gdk_draw_string(pixmap, smallFont, gC[OR_WHITE],
		      (displayWidth >> 1) + gdk_string_width(smallFont, "Fall") + 16, row*ABOUT_ROW_STEP, scratchString);

      tJDToDate(winter, &year, &month, &dayNum);
      winter -= 0.5;
      fDay = winter - (double)((int)winter);
      hh = (int)(fDay * 24.0);
      mm = (int)((fDay - ((double)hh)/24.0) * 1440.0 + 0.5);
      strncpy(shortMonthName, monthName[month-1], 3);
      sprintf(scratchString, "%s %d %02d:%02d", shortMonthName, dayNum, hh, mm);
      gdk_draw_string(pixmap, smallFont, gC[OR_WHITE],
		      3*(displayWidth >> 2) + gdk_string_width(smallFont, "Winter") + 10, row*ABOUT_ROW_STEP, scratchString);

    }
    /* Moon Stuff */
    row += 2;
    renderPangoText("Moon", OR_CREAM, MEDIUM_PANGO_FONT, &tWidth, &tHeight,
		    pixmap, displayWidth >> 1, (row++)*ABOUT_ROW_STEP, 0.0, TRUE, 0);
    renderPangoText("RA", OR_CREAM, SMALL_PANGO_FONT, &tWidth, &tHeight,
		    pixmap, 7, row*ABOUT_ROW_STEP + 11, 0.0, FALSE, 0);
    renderPangoText("Dec", OR_CREAM, SMALL_PANGO_FONT, &tWidth, &tHeight,
		    pixmap, 10 + displayWidth/5, row*ABOUT_ROW_STEP + 11, 0.0, FALSE, 0);
    renderPangoText("HA", OR_CREAM, SMALL_PANGO_FONT, &tWidth, &tHeight,
		    pixmap, 16 + (2*displayWidth)/5, row*ABOUT_ROW_STEP + 11, 0.0, FALSE, 0);
    renderPangoText("Az", OR_CREAM, SMALL_PANGO_FONT, &tWidth, &tHeight,
		    pixmap, 16 + (3*displayWidth)/5, row*ABOUT_ROW_STEP + 11, 0.0, FALSE, 0);
    renderPangoText("El", OR_CREAM, SMALL_PANGO_FONT, &tWidth, &tHeight,
		    pixmap, 16 + (4*displayWidth)/5, row*ABOUT_ROW_STEP + 11, 0.0, FALSE, 0);
    planetInfo(dataDir, EARTH, tJD, &sunRA, &sunDec, &illum, &mag);
    planetInfo(dataDir, MOON, tJD, &sunRA, &sunDec, &illum, &mag);
    sprintf(scratchString, "%2.0f%% Illuminated", illum*100.0);
    renderPangoText(scratchString, OR_WHITE, MEDIUM_PANGO_FONT, &tWidth, &tHeight,
		    pixmap, (displayWidth*4)/5, 225, 0.0, TRUE, 0);
    sunAngle = 0.0;
    if (fabs(illum) < 0.02)
      sprintf(scratchString, "New Moon");
    else if (fabs(illum) > 0.98)
      sprintf(scratchString, "Full Moon");
    else {
      double dummy;
      float illum2;

      planetInfo(dataDir, EARTH, tJD, &dummy, &dummy, &illum2, &mag);
      planetInfo(dataDir, MOON, tJD+0.04, &dummy, &dummy, &illum2, &mag);
      if (northernHemisphere) {
	if (illum2 < illum)
	  sunAngle = M_PI;
      } else {
	if (illum2 > illum)
	  sunAngle = M_PI;
      }
      if (fabs(illum - 0.5) < 0.03) {
	if (illum2 > illum)
	  sprintf(scratchString, "First Quarter");
	else
	  sprintf(scratchString, "Last Quarter");
      } else if (illum > 0.5) {
	if (illum2 > illum)
	  sprintf(scratchString, "Waxing Gibbous");
	else
	  sprintf(scratchString, "Waining Gibbous");
      } else {
	if (illum2 > illum)
	  sprintf(scratchString, "Waxing Crescent");
	else
	  sprintf(scratchString, "Waining Crescent");
      }
    }
    renderPangoText(scratchString, OR_WHITE, MEDIUM_PANGO_FONT, &tWidth, &tHeight,
		    pixmap, displayWidth/5, 225, 0.0, TRUE, 0);
    drawMoon(13.0, illum, sunAngle, displayWidth >> 1, 225, TRUE, FALSE, FALSE);
    rAHours = sunRA/HOURS_TO_RADIANS; decDegrees = sunDec/DEGREES_TO_RADIANS;
    rAHH = (int)rAHours;
    rAMM = (int)((rAHours - (float)rAHH)*60.0);
    fix60s(&rAHH, &rAMM);
    sprintf(scratchString, "%02d:%02d", rAHH, rAMM);
    renderPangoText(scratchString, OR_WHITE, SMALL_PANGO_FONT, &tWidth, &tHeight,
		    pixmap, 18 + displayWidth/10, row*ABOUT_ROW_STEP + 11, 0.0, TRUE, 0);
    decDD = (int)decDegrees;
    decMM = (int)((decDegrees - (float)decDD)*60.0);
    if (decDegrees > 0.0)
      sprintf(scratchString, "%02d:%02d", decDD, decMM);
    else
      sprintf(scratchString, "-%02d:%02d", -decDD, -decMM);
    renderPangoText(scratchString, OR_WHITE, SMALL_PANGO_FONT, &tWidth, &tHeight,
		    pixmap, 28 + (3*displayWidth)/10, row*ABOUT_ROW_STEP + 11, 0.0, TRUE, 0);
    hA = lSTNow - sunRA;
    rAHours = hA/HOURS_TO_RADIANS;
    sprintf(scratchString, "%6.2f", rAHours);
    renderPangoText(scratchString, OR_WHITE, SMALL_PANGO_FONT, &tWidth, &tHeight,
		    pixmap, 21 + (5*displayWidth)/10, row*ABOUT_ROW_STEP + 11, 0.0, TRUE, 0);
    azZA(sunRA, sin(sunDec), cos(sunDec), &az, &zA, FALSE);
    az /= DEGREES_TO_RADIANS;
    if (az < 0.0)
      az += 360.0;
    if (az > 360.0)
      az -= 360.0;
    sprintf(scratchString, "%5.1f", az);
    renderPangoText(scratchString, OR_WHITE, SMALL_PANGO_FONT, &tWidth, &tHeight,
		    pixmap, 20 + (7*displayWidth)/10, row*ABOUT_ROW_STEP + 11, 0.0, TRUE, 0);
    el = M_HALF_PI - zA;
    el += refraction(el);
    sprintf(scratchString, "%5.1f", el/DEGREES_TO_RADIANS);
    renderPangoText(scratchString, OR_WHITE, SMALL_PANGO_FONT, &tWidth, &tHeight,
		    pixmap, 16 + (9*displayWidth)/10, row*ABOUT_ROW_STEP + 11, 0.0, TRUE, 0);
    row += 2;
    /* Calculate time of moon rise, transit and setting */
    nextRiseDone = nextSetDone = FALSE;
    h0 = 0.125*DEGREES_TO_RADIANS;
    for (day = 0; day < N_RISE_SET_DAYS; day++) {
      tJDToDate(tJDS[day], &year, &month, &dayNum);
      sprintf(scratchString, "%02d/%02d/%04d", month, dayNum, year);
      if (day - N_RISE_SET_DAYS/2 == 0)
	gdk_draw_string(pixmap, smallFont, gC[OR_WHITE],
			MOONLINE_X_OFFSET, row*ABOUT_ROW_STEP + BIG_FONT_OFFSET, scratchString);
      else
	gdk_draw_string(pixmap, smallFont, gC[OR_GREY],
			MOONLINE_X_OFFSET, row*ABOUT_ROW_STEP + BIG_FONT_OFFSET, scratchString);
      lSTNow = lSTAtTJD(tJDS[day]);
      /* Moonrise */
      theTime = calcRiseOrSetTime(TRUE, MOON, tJDS[day], lSTNow, h0, &circumpolar, &hD,
				  &cosH, &sunDec, &illumR);
      if (circumpolar) {
	gdk_draw_string(pixmap, smallFont, gC[OR_DARK_CREAM],
			MOONLINE_X_OFFSET+gdk_string_width(smallFont, "88/88/8888  "),
			row*ABOUT_ROW_STEP + BIG_FONT_OFFSET, "Moonrise");
	gdk_draw_string(pixmap, smallFont, gC[OR_GREY],
			MOONLINE_X_OFFSET+gdk_string_width(smallFont, "88/88/8888  Moonrise "),
			row*ABOUT_ROW_STEP + BIG_FONT_OFFSET, "(none)");
	if (day == (N_RISE_SET_DAYS/2))
	  renderPangoText("(none)      ", OR_WHITE, MEDIUM_PANGO_FONT, &tWidth, &tHeight,
			  pixmap, 130, moonInfoRow, 0.0, FALSE, 0);
      } else {
	theAz = atan2(sin(hD), cosH*sinLatitude-tan(sunDec)*cosLatitude) + M_PI;
	doubleNormalize0to2pi(&theAz);
	if ((!nextRiseDone) && (theTime - tJD >= 0.0)) {
	  labelGC = gC[OR_CREAM];
	  textGC = gC[OR_WHITE];
	  nextRiseDone = TRUE;
	} else {
	  labelGC = gC[OR_DARK_CREAM];
	  textGC = gC[OR_GREY];
	}
	theTime -= 0.5;
	theTime = theTime - (double)((int)theTime);
	rAHours = theTime*24.0;
	rAHH = (int)rAHours;
	rAMM = (int)((rAHours - (float)rAHH)*60.0 + 0.5);
	fix60s(&rAHH, &rAMM);
	sprintf(scratchString, "%02d:%02d ", rAHH, rAMM);
	if (day == (N_RISE_SET_DAYS/2))
	  renderPangoText(scratchString, OR_WHITE, MEDIUM_PANGO_FONT, &tWidth, &tHeight,
			  pixmap, 160, moonInfoRow, 0.0, TRUE, 0);
	gdk_draw_string(pixmap, smallFont, labelGC,
			MOONLINE_X_OFFSET+gdk_string_width(smallFont, "88/88/8888  "),
			row*ABOUT_ROW_STEP + BIG_FONT_OFFSET, "Moonrise");
	gdk_draw_string(pixmap, smallFont, textGC,
			MOONLINE_X_OFFSET+gdk_string_width(smallFont, "88/88/8888  Moonrise "),
			row*ABOUT_ROW_STEP + BIG_FONT_OFFSET, scratchString);
	gdk_draw_string(pixmap, smallFont, labelGC,
			MOONLINE_X_OFFSET+gdk_string_width(smallFont, "88/88/8888  Moonrise 88:88 "),
			row*ABOUT_ROW_STEP + BIG_FONT_OFFSET, "UT Az =");
	sprintf(scratchString, "%3.0f", theAz/DEGREES_TO_RADIANS);
	gdk_draw_string(pixmap, smallFont, textGC,
			MOONLINE_X_OFFSET+gdk_string_width(smallFont, "88/88/8888  Moonrise 88:88 UT Az = "),
			row*ABOUT_ROW_STEP + BIG_FONT_OFFSET, scratchString);
      }
      /* Moonset */
      theTime = calcRiseOrSetTime(FALSE, MOON, tJDS[day], lSTNow, h0, &circumpolar, &hD,
				  &cosH, &sunDec, &illumS);
      if (circumpolar) {
	gdk_draw_string(pixmap, smallFont, gC[OR_DARK_CREAM],
			MOONLINE_X_OFFSET+gdk_string_width(smallFont, "88/88/8888  Moonrise 88:88 UT Az = 888  Illum = 88%  "),
			row*ABOUT_ROW_STEP + BIG_FONT_OFFSET, "Moonset");
	gdk_draw_string(pixmap, smallFont, gC[OR_GREY],
			MOONLINE_X_OFFSET+gdk_string_width(smallFont, "88/88/8888  Moonrise 88:88 UT Az = 888  Illum = 88%  Moonset "),
			row*ABOUT_ROW_STEP + BIG_FONT_OFFSET, "(none)");
	if (day == (N_RISE_SET_DAYS/2))
	  renderPangoText("(none)      ", OR_WHITE, MEDIUM_PANGO_FONT, &tWidth, &tHeight,
			  pixmap, 365, moonInfoRow, 0.0, FALSE, 0);
      } else {
	theAz = atan2(sin(hD), cosH*sinLatitude-tan(sunDec)*cosLatitude) + M_PI;
	doubleNormalize0to2pi(&theAz);
	if ((!nextSetDone) && (theTime - tJD >= 0.0)) {
	  labelGC = gC[OR_CREAM];
	  textGC = gC[OR_WHITE];
	  nextSetDone = TRUE;
	} else {
	  labelGC = gC[OR_DARK_CREAM];
	  textGC = gC[OR_GREY];
	}
	theTime -= 0.5;
	theTime = theTime - (double)((int)theTime);
	rAHours = theTime*24.0;
	rAHH = (int)rAHours;
	rAMM = (int)((rAHours - (float)rAHH)*60.0 + 0.5);
	fix60s(&rAHH, &rAMM);
	sprintf(scratchString, "%02d:%02d ", rAHH, rAMM);
	if (day == (N_RISE_SET_DAYS/2))
	  renderPangoText(scratchString, OR_WHITE, MEDIUM_PANGO_FONT, &tWidth, &tHeight,
			  pixmap, 395, moonInfoRow, 0.0, TRUE, 0);
	gdk_draw_string(pixmap, smallFont, labelGC,
			MOONLINE_X_OFFSET+gdk_string_width(smallFont, "88/88/8888  Moonrise 88:88 UT Az = 888  Illum = 88%  "),
			row*ABOUT_ROW_STEP + BIG_FONT_OFFSET, "Moonset");
	gdk_draw_string(pixmap, smallFont, textGC,
			MOONLINE_X_OFFSET+gdk_string_width(smallFont, "88/88/8888  Moonrise 88:88 UT Az = 888  Illum = 88%  Moonset "),
			row*ABOUT_ROW_STEP + BIG_FONT_OFFSET, scratchString);
	gdk_draw_string(pixmap, smallFont, labelGC,
			MOONLINE_X_OFFSET+gdk_string_width(smallFont, "88/88/8888  Moonrise 88:88 UT Az = 888  Illum = 88%  Moonset 88:88 "),
			row*ABOUT_ROW_STEP + BIG_FONT_OFFSET, "UT Az =");
	sprintf(scratchString, "%3.0f", theAz/DEGREES_TO_RADIANS);
	gdk_draw_string(pixmap, smallFont, textGC,
			MOONLINE_X_OFFSET+gdk_string_width(smallFont, "88/88/8888  Moonrise 88:88 UT Az = 888  Illum = 88%  Moonset 88:88 UT Az = "),
			row*ABOUT_ROW_STEP + BIG_FONT_OFFSET, scratchString);
      }
      sprintf(scratchString, "Illum = %2.0f%%", (illumR+illumS)*50.0);
      gdk_draw_string(pixmap, smallFont, textGC,
		      MOONLINE_X_OFFSET+gdk_string_width(smallFont, "88/88/8888  Moonrise 88:88 UT Az = 888  "),
		      row*ABOUT_ROW_STEP + BIG_FONT_OFFSET, scratchString);
      row++;
    }
    row += 2;
    {
      /* Calculate time and date of the next four main moon phases. */
      int year, month, day, iDay, hH, mM, i;
      GdkGC *timeGC;

      renderPangoText("UT Dates and Times of Moon Phases", OR_CREAM,
		      MEDIUM_PANGO_FONT, &tWidth, &tHeight,
		      pixmap, displayWidth >> 1, row*ABOUT_ROW_STEP + 5, 0.0, TRUE, 0);
      row += 2;
      sprintf(scratchString, "New Moon");
      gdk_draw_string(pixmap, smallFont, gC[OR_CREAM],
		      displayWidth/8 - gdk_string_width(smallFont, scratchString)/2,
		      row*ABOUT_ROW_STEP + BIG_FONT_OFFSET, scratchString);
      sprintf(scratchString, "First Quarter");
      gdk_draw_string(pixmap, smallFont, gC[OR_CREAM],
		      3*displayWidth/8 - gdk_string_width(smallFont, scratchString)/2,
		      row*ABOUT_ROW_STEP + BIG_FONT_OFFSET, scratchString);
      sprintf(scratchString, "Full Moon");
      gdk_draw_string(pixmap, smallFont, gC[OR_CREAM],
		      5*displayWidth/8 - gdk_string_width(smallFont, scratchString)/2,
		      row*ABOUT_ROW_STEP + BIG_FONT_OFFSET, scratchString);
      sprintf(scratchString, "Last Quarter");
      gdk_draw_string(pixmap, smallFont, gC[OR_CREAM],
		      7*displayWidth/8 - gdk_string_width(smallFont, scratchString)/2,
		      row*ABOUT_ROW_STEP + BIG_FONT_OFFSET, scratchString);

      tJDToDate(tJD, &year, &month, &day);
      nextPhases(tJD, N_PHASE_DATES, &nextNew[0], &nextFirstQ[0], &nextFull[0], &nextLastQ[0]);

      row++;
      for (i = 0; i < N_PHASE_DATES; i++) {
	if (i == N_PHASE_DATES/2)
	  timeGC = gC[OR_WHITE];
	else
	  timeGC = gC[OR_GREY];
	iDay = (int)nextNew[i];
	tempDay = nextNew[i] - 0.5 - (double)iDay;
	if (tempDay < 0.0)
	  tempDay += 1.0;
	hH = (int)(tempDay * 24.0);
	mM = (int)((tempDay * 24.0 - (double)hH) * 60.0);
	tJDToDate(nextNew[i], &year, &month, &day);
	sprintf(scratchString, "%02d/%02d/%4d %02d:%02d",
		month, day, year, hH, mM);
	gdk_draw_string(pixmap, smallFont, timeGC,
			displayWidth/8 - gdk_string_width(smallFont, scratchString)/2,
			row*ABOUT_ROW_STEP + BIG_FONT_OFFSET, scratchString);
	iDay = (int)nextFirstQ[i];
	tempDay = nextFirstQ[i] - 0.5 - (double)iDay;
	if (tempDay < 0.0)
	  tempDay += 1.0;
	hH = (int)(tempDay * 24.0);
	mM = (int)((tempDay * 24.0 - (double)hH) * 60.0);
	fix60s(&hH, &mM);
	tJDToDate(nextFirstQ[i], &year, &month, &day);
	sprintf(scratchString, "%02d/%02d/%4d %02d:%02d",
		month, day, year, hH, mM);
	gdk_draw_string(pixmap, smallFont, timeGC,
			3*displayWidth/8 - gdk_string_width(smallFont, scratchString)/2,
			row*ABOUT_ROW_STEP + BIG_FONT_OFFSET, scratchString);
	iDay = (int)nextFull[i];
	tempDay = nextFull[i] - 0.5 - (double)iDay;
	if (tempDay < 0.0)
	  tempDay += 1.0;
	hH = (int)(tempDay * 24.0);
	mM = (int)((tempDay * 24.0 - (double)hH) * 60.0);
	fix60s(&rAHH, &rAMM);
	tJDToDate(nextFull[i], &year, &month, &day);
	sprintf(scratchString, "%02d/%02d/%4d %02d:%02d",
		month, day, year, hH, mM);
	gdk_draw_string(pixmap, smallFont, timeGC,
			5*displayWidth/8 - gdk_string_width(smallFont, scratchString)/2,
			row*ABOUT_ROW_STEP + BIG_FONT_OFFSET, scratchString);
	iDay = (int)nextLastQ[i];
	tempDay = nextLastQ[i] - 0.5 - (double)iDay;
	if (tempDay < 0.0)
	  tempDay += 1.0;
	hH = (int)(tempDay * 24.0);
	mM = (int)((tempDay * 24.0 - (double)hH) * 60.0);
	fix60s(&rAHH, &rAMM);
	tJDToDate(nextLastQ[i], &year, &month, &day);
	sprintf(scratchString, "%02d/%02d/%4d %02d:%02d",
		month, day, year, hH, mM);
	gdk_draw_string(pixmap, smallFont, timeGC,
			7*displayWidth/8 - gdk_string_width(smallFont, scratchString)/2,
			row*ABOUT_ROW_STEP + BIG_FONT_OFFSET, scratchString);
	row++;
      }
    }
    break;
  case SMALL_MOONCAL_SCREEN:
    {
      int tWidth, tHeight;
      int thisMonth, thisYear, iDummy, day, testMonth, phase, i, nextMonth, lastMonth;
      int fullSeen = FALSE;
      int blue = FALSE;
      int week = 0;
      int weekDay = 0;
      int lastWeekDay = -1;
      int lunarDay;
      int lastNewI = 0;
      double fLunarDay;
      double tJDS, tJDM, tJDO, plusOneMonth, minusOneMonth;

      if (!moonImagesRead)
	readMoonImages();
      tJDO = tJD + smallMooncalDateOffset;
      nextPhases(tJDO, N_PHASE_DATES, &nextNew[0], &nextFirstQ[0], &nextFull[0], &nextLastQ[0]);
      tJDToDate(tJDO, &thisYear, &thisMonth, &iDummy);
      if ((thisYear % 4) == 0)
	monthLengths[1] += 1.0;
      sprintf(scratchString, "%s %d", monthName[thisMonth-1], thisYear);
      renderPangoText(scratchString, OR_CREAM, BIG_PANGO_FONT, &tWidth, &tHeight,
		      pixmap, displayWidth >> 1, 21, 0.0, TRUE, 0);
      for (day = 0; day < 7; day++) {
	sprintf(scratchString, dayName[day]);
	gdk_draw_string(pixmap, smallFont, gC[OR_BLUE],
			CAL_DAY_WIDTH/2 + day*CAL_DAY_WIDTH + CAL_LEFT_OFFSET -
			gdk_string_width(smallFont, scratchString)/2,
			30 + SMALL_MOONCAL_SHIFT, scratchString);	
      }
      /* Generate days of the week */
      tJDM = (double)((int)(tJDO - 0.5)) + 0.5;
      for (tJDS = tJDM - 32.0; tJDS < tJDM + 32.0; tJDS += 1.0) {
	tJDToDate(tJDS, &thisYear, &testMonth, &day);
	if (testMonth == thisMonth) {
	  int today;

	  weekDay = dayOfWeek(thisYear, thisMonth, day);
	  if (weekDay < lastWeekDay)
	    week++;
	  lastWeekDay = weekDay;
	  if ((fabs(tJDS - tJD + 0.5) < 0.5))
	    today = TRUE;
	  else
	    today = FALSE;
	  phase = -1;
	  for (i = 0; i < N_PHASE_DATES; i++) {
	    if (fabs(nextNew[i] - tJDS - 0.5) < 0.5)
	      phase = 0;
	    else if (fabs(nextFirstQ[i] - tJDS - 0.5) < 0.5)
	      phase = 1;
	    else if (fabs(nextFull[i] - tJDS - 0.5) < 0.5) {
	      if (fullSeen)
		blue = TRUE;
	      else
		fullSeen = TRUE;
	      phase = 2;
	    } else if (fabs(nextLastQ[i] - tJDS - 0.5) < 0.5)
	      phase = 3;
	    if (((nextNew[i] - tJDS - 0.5) < 0.0) && (fabs(nextNew[i] - tJDS - 0.5) < 29.530589))
	      lastNewI = i;
	  }
	  fLunarDay = -(nextNew[lastNewI] - tJDS - 0.5);
	  while (fLunarDay < -0.5)
	    fLunarDay += 29.530589;
	  lunarDay = (int)(fLunarDay + 0.5);
	  if (phase == 0)
	    lunarDay = 0;
	  plotDayBox(tJDS, week, weekDay, day, today, phase, blue, lunarDay);
	}
      }
      lastMonth = thisMonth-1;
      if (lastMonth < 1)
	lastMonth = 12;
      nextMonth = thisMonth + 1;
      if (nextMonth > 12)
	nextMonth = 1;
      plusOneMonth = monthLengths[thisMonth-1] - iDummy + 1;
      minusOneMonth = -iDummy;
      removeAllSensitiveAreas();
      points[0].x = 0;                         points[0].y = displayHeight - 50;
      points[1].x = (displayWidth >> 2) + 60;  points[1].y = points[0].y - 50;
      points[2].x = points[1].x;               points[2].y = points[0].y + 50;
      sprintf(scratchString, "%s", monthName[lastMonth-1]);
      renderPangoText(scratchString, OR_CREAM, MEDIUM_PANGO_FONT, &tWidth, &tHeight,
		      pixmap, (displayWidth >> 3) + 50, points[0].y,
		      0.0, TRUE, 0);
      gdk_draw_polygon(pixmap, gC[OR_CREAM], FALSE, points, 3);
      addSensitiveArea(FALSE, SA_MONTH_LEFT_ARROW,
		       points[0].x-5, points[1].y+5, points[1].x+5, points[2].y-5, minusOneMonth);
      points[0].x = displayWidth;                        points[0].y = displayHeight - 50;
      points[1].x = (points[0].x - displayWidth/4) - 60; points[1].y = points[0].y - 50;
      points[2].x = points[1].x;                         points[2].y = points[0].y + 50;
      sprintf(scratchString, "%s", monthName[nextMonth-1]);
      renderPangoText(scratchString, OR_CREAM, MEDIUM_PANGO_FONT, &tWidth, &tHeight,
		      pixmap, (7*displayWidth)/8 - 50,
		      points[0].y, 0.0, TRUE, 0);
      gdk_draw_polygon(pixmap, gC[OR_CREAM], FALSE, points, 3);
      addSensitiveArea(FALSE, SA_MONTH_RIGHT_ARROW,
		       points[1].x-5, points[1].y+5, points[0].x+5, points[2].y-5, plusOneMonth);
    }
    break;
  case BIG_MOONCAL_SCREEN:
#define MOON_CAL_SHIFT (35)
    /*
      Draw a moon calendar.   I will make a calendar with 21 months of moon
      phases, centered on the current month.
    */
    {
      int centerMonth, centerYear, startMonth, endMonth, iDummy, i, col, lastMonth, centerDay;
      int displayBottom, tWidth, tHeight;
      int blue = FALSE;
      int fullSeen = FALSE;
      double tempTJD, startTJD, endTJD;
      char *monthLetters[12] = {"J","F","M","A","M","J","J","A","S","O","N","D"};
      GdkGC *labelGC;

      displayBottom = displayHeight - 100 + MOON_CAL_SHIFT;
      nextPhases(tJD, MAX_PHASE_DATES, &nextNew[0], &nextFirstQ[0], &nextFull[0], &nextLastQ[0]);
      needNewTime = TRUE;
      lSTNow = lST();
      tJDToDate(tJD, &centerYear, &centerMonth, &centerDay);
      startMonth = centerMonth - 10;
      if (startMonth < 1)
	startMonth += 12;
      endMonth = centerMonth + 11;
      if (endMonth > 12)
	endMonth -= 12;
      startTJD = tJD - 341;
      i = 0;
      startTJD = (double)((int)startTJD);
      while (i != startMonth) {
	startTJD += 1.0;
	tJDToDate(startTJD, &iDummy, &i, &iDummy);
      }
      endTJD = tJD + 250;
      i = 0;
      while (i != endMonth) {
	endTJD += 1.0;
	tJDToDate(endTJD, &iDummy, &i, &iDummy);
      }

      for (i = startMonth-1; i < startMonth+20; i++) {
	if ((i % 12) == centerMonth-1) {
	  renderPangoText(monthLetters[i % 12], OR_CREAM, SMALL_PANGO_FONT, &tWidth, &tHeight,
			  pixmap, 30+(i - startMonth + 1)*21,
			  21 + MOON_CAL_SHIFT, 0.0, TRUE, 0);
	  renderPangoText(monthLetters[i % 12], OR_CREAM, SMALL_PANGO_FONT, &tWidth, &tHeight,
			  pixmap, 30+(i - startMonth + 1)*21,
			  displayBottom - 45 + MOON_CAL_SHIFT,
			  0.0, TRUE, 0);
	} else {
	  labelGC = gC[OR_BLUE];
	  gdk_draw_string(pixmap, smallFont, labelGC, 28+(i - startMonth + 1)*21,
			  28 + MOON_CAL_SHIFT, monthLetters[i % 12]);
	  gdk_draw_string(pixmap, smallFont, labelGC, 28+(i - startMonth + 1)*21,
			  displayBottom - 38 + MOON_CAL_SHIFT, monthLetters[i % 12]);
	}
	if ((i % 12) == 11) {
	  gdk_draw_line(pixmap, gC[OR_GREEN], 39+(i - startMonth + 1)*21, MOON_CAL_SHIFT + 18,
			39+(i - startMonth + 1)*21, displayBottom - 4);
	  gdk_draw_line(pixmap, gC[OR_GREEN], 41+(i - startMonth + 1)*21, MOON_CAL_SHIFT + 18,
			41+(i - startMonth + 1)*21, displayBottom - 4);
	}
      }
      for (i = 1; i < 32; i++) {
	sprintf(scratchString, "%2d", i);
	if (i == centerDay) {
	  renderPangoText(scratchString, OR_CREAM, SMALL_PANGO_FONT, &tWidth, &tHeight,
			  pixmap, 0, 23+i*19 + MOON_CAL_SHIFT,
			  0.0, FALSE, 0);
	  renderPangoText(scratchString, OR_CREAM, SMALL_PANGO_FONT, &tWidth, &tHeight,
			  pixmap, displayWidth - 10,
			  23+i*19 + MOON_CAL_SHIFT, 0.0, TRUE, 0);
	} else {
	  labelGC = gC[OR_BLUE];
	  gdk_draw_string(pixmap, smallFont, labelGC, 0, 26+i*19 + MOON_CAL_SHIFT, scratchString);
	  gdk_draw_string(pixmap, smallFont, labelGC, displayWidth-15,
			  26+i*19 + MOON_CAL_SHIFT, scratchString);
	}
      }
      /* Loop over all days in calendar */
      col = 0; lastMonth = startMonth;
      for (tempTJD = startTJD; tempTJD < endTJD; tempTJD += 1.0) {
	int month, row, full, ii;
	float illum, illum2, sunAngle;
	double dummy, testTJD;

	testTJD = (float)((int)tempTJD) + 0.5;
	tJDToDate(tempTJD, &iDummy, &month, &row);
	if (month != lastMonth) {
	  fullSeen = blue = FALSE;
	  col++;
	  if (col > 20)
	    break;
	  lastMonth = month;
	}
	if ((month == 6) && (row == 1) && (col < 19)) {
	  sprintf(scratchString, "%4d", iDummy);
	  renderPangoText(scratchString, OR_GREEN, BIG_PANGO_FONT, &tWidth, &tHeight,
			  pixmap, 45 + col*21, 21, 0.0, TRUE, 0);
	}
	planetInfo(dataDir, EARTH, tempTJD, &dummy, &dummy, &illum2, &mag);
	planetInfo(dataDir, MOON, tempTJD+0.04, &dummy, &dummy, &illum2, &mag);
	planetInfo(dataDir, MOON, tempTJD, &dummy, &dummy, &illum, &mag);
	if (northernHemisphere) {
	  if (illum < illum2)
	    sunAngle = 0.0;
	  else
	    sunAngle = M_PI;
	} else {
	  if (illum > illum2)
	    sunAngle = 0.0;
	  else
	    sunAngle = M_PI;
	}
	full = FALSE;
	for (ii = 0; ii < MAX_PHASE_DATES; ii++)
	  if (fabs(nextFull[ii] - testTJD) <= 0.5) {
	    full = TRUE;
	    break;
	  }
	/*
	  Handle "blue moons" usung the definition that a blue moon is the second
	  full moon in a calendar month.
	*/
	if (full) {
	  if (fullSeen)
	    blue = TRUE;
	  else
	    fullSeen = TRUE;
	}
	drawMoon(9.0, illum, sunAngle, 30+col*21, 22+row*19 + MOON_CAL_SHIFT, FALSE, full, blue);
	if (fabs(tempTJD - tJD) < 0.1) {
	  GdkPoint points[4];

	  points[0].x = 20+col*21;      points[0].y = 12+row*19 + MOON_CAL_SHIFT;
	  points[1].x = points[0].x+20; points[1].y = points[0].y;
	  points[2].x = points[1].x;    points[2].y = points[1].y+20;
	  points[3].x = points[0].x;    points[3].y = points[2].y;
	  gdk_draw_polygon(pixmap, gC[OR_RED], FALSE, points, 4);
	}
      }
    }
    break;
  case SOLAR_SYSTEM_SCHEMATIC_SCREEN:
    schematic = TRUE;
  case SOLAR_SYSTEM_SCALE_SCREEN:
    {
      int i, j, w, h, k, m, n, eD, year, month, day, tDayIncSign, plotPlanet;
      int ii, x1, x2, y1, y2;
      int plotStart, plotStop, plotInc;
      int tDayInc = 1;
      int solarSystemButtonHeight, solarSystemButtonWidth;
      int x = 0;
      int y = 0;
      static int firstCall = TRUE;
      static int havePlottedOrbits = FALSE;
      int firstIteration = TRUE;
      double eX, eY, eZ, eHyp, dDummy, eLong, dayIncSign;
      double a, e, I, L, smallOmega, bigOmega;
      float fDummy, eR;
      float orbitScale = 0.0;
      float eSin = 0.0;
      float eCos = 0.0;
      double sunELong = 0.0;
      char fileName[MAX_FILE_NAME_SIZE];
      GdkPoint points[4];

      removeAllSensitiveAreas();
      if (dayInc < 0)
	dayIncSign = -1.0;
      else
	dayIncSign = 1.0;
      nSolarSystemDays = abs(dayInc);
      j = 0;
      do {
	if (j == 0)
	  gdk_draw_rectangle(pixmap, drawingArea->style->black_gc,
			     TRUE, 0, 0,
			     drawingArea->allocation.width,
			     drawingArea->allocation.height);
	else
	  gdk_draw_rectangle(pixmap, drawingArea->style->black_gc,
			     TRUE, 0, BIG_FONT_OFFSET,
			     drawingArea->allocation.width,
			     drawingArea->allocation.height*6/7 - 14 - BIG_FONT_OFFSET);
	solarSystemButtonWidth = displayWidth/5 - 1;
	solarSystemButtonHeight = displayHeight/14 + 5;
	if (!schematic) {
	  /*
	    Draw the "in" and out" buttons which allow the user to zoom in and
	    out of the inner Solar System - this is required because with the
	    "to-scale" view makes the outer planet orbits so large, the inner
	    orbits cannot be displayed.
	  */
	  if (outermostPlanet > 2) {
	    points[0].x = 0;
	    points[0].y = displayHeight-2*solarSystemButtonHeight - 5;
	    points[1].x = points[0].x;
	    points[1].y = points[0].y-solarSystemButtonHeight;
	    points[2].x = points[1].x+4*solarSystemButtonWidth/5;
	    points[2].y = points[1].y;
	    points[3].x = points[2].x;
	    points[3].y = points[2].y+solarSystemButtonHeight;
	    strcpy(scratchString, "Zoom In");
	    n = gdk_string_width(smallFont, scratchString);
	    gdk_draw_polygon(pixmap, gC[OR_DARK_CREAM], FALSE, points, 4);
	    gdk_draw_string(pixmap, smallFont, gC[OR_WHITE],
			    (points[0].x + points[2].x - n)/2,
			    (points[0].y + points[1].y)/2 + 4, scratchString);
	    tDayInc = ZOOM_SOLAR_SYSTEM_IN;
	    points[2].x = points[1].x+solarSystemButtonWidth;
	    points[2].y += 10;
	    addSensitiveArea(FALSE, SA_SOLAR_SYSTEM_BUTTON,
			     points[0].x, points[2].y, points[2].x,
			     points[0].y, (float)tDayInc);
	  }
	  if (outermostPlanet < N_SOLAR_SYSTEM_OBJECTS-1) {
	    points[0].x = (displayWidth-1) - 4*solarSystemButtonWidth/5 - 4;
	    points[0].y = displayHeight-2*solarSystemButtonHeight - 5;
	    points[1].x = points[0].x;
	    points[1].y = points[0].y-solarSystemButtonHeight;
	    points[2].x = displayWidth-5;
	    points[2].y = points[1].y;
	    points[3].x = points[2].x;
	    points[3].y = points[2].y+solarSystemButtonHeight;
	    strcpy(scratchString, "Zoom Out");
	    n = gdk_string_width(smallFont, scratchString);
	    gdk_draw_polygon(pixmap, gC[OR_DARK_CREAM], FALSE, points, 4);
	    gdk_draw_string(pixmap, smallFont, gC[OR_WHITE],
			    (points[0].x + points[2].x - n)/2,
			    (points[0].y + points[1].y)/2 + 4, scratchString);
	    tDayInc = ZOOM_SOLAR_SYSTEM_OUT;
	    points[0].x = (displayWidth-1) - solarSystemButtonWidth - 4;
	    points[2].y += 10;
	    addSensitiveArea(FALSE, SA_SOLAR_SYSTEM_BUTTON,
			     points[0].x, points[2].y, points[2].x,
			     points[0].y, (float)tDayInc);
	  }
	}
	if ((j == 0) || (j == nSolarSystemDays-1)) {
	  for (k = 0; k < 2; k++)
	    for (m = 0; m < 5; m++) {
	      points[0].x = m*solarSystemButtonWidth;
	      points[0].y = displayHeight-k*solarSystemButtonHeight - 5;
	      points[1].x = points[0].x;
	      points[1].y = points[0].y-solarSystemButtonHeight;
	      points[2].x = points[1].x+solarSystemButtonWidth;
	      points[2].y = points[1].y;
	      points[3].x = points[2].x;
	      points[3].y = points[2].y+solarSystemButtonHeight;
	      if (k == 0) {
		tDayIncSign = -1;
		planetGC = gC[OR_RED];
	      } else {
		tDayIncSign = 1;
		planetGC = gC[OR_WHITE];
	      }
	      switch (m) {
	      case 0:
		tDayInc = tDayIncSign*2;
		if (k == 0)
		  strcpy(scratchString, "-1 day");
		else
		  strcpy(scratchString, "+1 day");
		break;
	      case 1:
		tDayInc = tDayIncSign*11;
		if (k == 0)
		  strcpy(scratchString, "-10 days");
		else
		  strcpy(scratchString, "+10 days");
		break;
	      case 2:
		tDayInc = tDayIncSign*101;
		if (k == 0)
		  strcpy(scratchString, "-100 days");
		else
		  strcpy(scratchString, "+100 days");
		break;
	      case 3:
		tDayInc = tDayIncSign*366;
		if (k == 0)
		  strcpy(scratchString, "-1 year");
		else
		  strcpy(scratchString, "+1 year");
		break;
	      case 4:
		tDayInc = tDayIncSign*3653;
		if (k == 0)
		  strcpy(scratchString, "-10 years");
		else
		  strcpy(scratchString, "+10 years");
		break;
	      }
	      n = gdk_string_width(smallFont, scratchString);
	      if ((j == 0) && (nSolarSystemDays > 1) &&
		  (nSolarSystemDays == abs(tDayInc)) &&
		  ((int)dayIncSign == tDayIncSign)) {
		gdk_draw_polygon(pixmap, gC[OR_DARK_CREAM], TRUE, points, 4);
		gdk_draw_string(pixmap, smallFont, planetGC,
				(points[0].x + points[2].x - n)/2,
				(points[0].y + points[1].y)/2 + 4, scratchString);
		gdk_draw_drawable(drawingArea->window,
				  drawingArea->style->fg_gc[GTK_WIDGET_STATE (drawingArea)],
				  pixmap, points[0].x, points[1].y, points[0].x, points[1].y,
				  points[2].x-points[0].x, points[0].y-points[1].y);
	      } else if ((j == nSolarSystemDays - 1) && (nSolarSystemDays > 1) &&
			 (nSolarSystemDays == abs(tDayInc)) &&
			 ((int)dayIncSign == tDayIncSign)) {
		gdk_draw_rectangle(pixmap, drawingArea->style->black_gc,
				   TRUE, points[0].x, points[1].y,
				   points[2].x-points[0].x, points[0].y-points[1].y);
		gdk_draw_polygon(pixmap, gC[OR_GREY], FALSE, points, 4);
	      } else if (j == 0)
		gdk_draw_polygon(pixmap, gC[OR_GREY], FALSE, points, 4);
	      addSensitiveArea(FALSE, SA_SOLAR_SYSTEM_BUTTON,
			       points[0].x, points[2].y, points[2].x,
			       points[0].y, (float)tDayInc);
	      gdk_draw_string(pixmap, smallFont, planetGC,
			      (points[0].x + points[2].x - n)/2,
			      (points[0].y + points[1].y)/2 + 4, scratchString);
	    }
	}
	tJDToDate(tJD, &year, &month, &day);
	if (schematic)
	  sprintf(fileName, "Solar System Schematic View on %02d/%02d/%04d", month, day, year);
	else
	  sprintf(fileName, "Solar System on %02d/%02d/%04d", month, day, year);
	if ((j == 0) || (j == nSolarSystemDays-1)) {
	  renderPangoText(fileName, OR_WHITE, SMALL_PANGO_FONT, &tWidth, &tHeight,
			  pixmap, displayWidth>>1, 11, 0.0, TRUE, 0);
	} else {
	  renderPangoText(fileName, OR_WHITE, SMALL_PANGO_FONT, &tWidth, &tHeight,
			  drawingArea->window, displayWidth>>1, 11, 0.0, TRUE, 0);
	  gdk_flush();
	}
	if (!schematic && !havePlottedOrbits) {
	  for (i = 0; i < N_SOLAR_SYSTEM_OBJECTS; i++) {
	    if ((i <= NEPTUNE) && (i != MOON) && (i != SUN)) {
	      int ii;
	      float r, theta, aOneMinusEE;

	      /*
		Build a structure to hold a plot of the planet's orbit;
	      */
	      getCurrentOrbitalElements(i, tJD, &a, &e, &I, &L, &smallOmega, &bigOmega);
	      orbitPlots[i].omegaRad = smallOmega * DEGREES_TO_RADIANS;
	      orbitPlots[i].peri = a - e*a;
	      aOneMinusEE = a*(1.0 - e*e);
	      orbitPlots[i].x = (float *)malloc(360 * sizeof(float));
	      if (unlikely(orbitPlots[i].x == NULL)) {
		perror("malloc of orbit plot xs");
		exit(ERROR_EXIT);
	      }
	      orbitPlots[i].y = (float *)malloc(360 * sizeof(float));
	      if (unlikely(orbitPlots[i].y == NULL)) {
		perror("malloc of orbit plot ys");
		exit(ERROR_EXIT);
	      }
	      orbitPlots[i].maxX = 0.0;
	      for (ii = 0; ii < 360; ii++) {
		theta = ((float)ii) * DEGREES_TO_RADIANS;
		r = aOneMinusEE/(1.0 + e*cosf(theta - orbitPlots[i].omegaRad));
		orbitPlots[i].x[ii] = -r*cosf(theta);
		orbitPlots[i].y[ii] = r*sinf(theta);
		if (r > orbitPlots[i].maxX)
		  orbitPlots[i].maxX = r;
	      }
	    }
	  }
	}
	if (!schematic) {
	  char scaleLabel[30];

	  if (outermostPlanet-1 == MOON)
	    ii = EARTH;
	  else
	    ii = outermostPlanet-1;
	  orbitScale = ((float)(displayWidth - SCHEMATIC_ORBIT_INCREMENT))
	    / (2.0 * orbitPlots[ii].maxX);
	  x1 = x2 = 7*displayWidth/8 - roundf(0.5*solarSystemScales[ii]*orbitScale/AU);
	  y1 = displayHeight/12 - 3;
	  y2 = y1 + 6;
	  gdk_draw_line(pixmap, gC[OR_BLUE], x1, y1, x2, y2);
	  x1 = x2 = 7*displayWidth/8 + roundf(0.5*solarSystemScales[ii]*orbitScale/AU);
	  gdk_draw_line(pixmap, gC[OR_BLUE], x1, y1, x2, y2);
	  x1 = 7*displayWidth/8 - roundf(0.5*solarSystemScales[ii]*orbitScale/AU);
	  y1 = y2 = displayHeight/12;
	  gdk_draw_line(pixmap, gC[OR_BLUE], x1, y1, x2, y2);
	  if (solarSystemScales[ii] < 1000.0)
	    sprintf(scaleLabel, "%1.0f million km", solarSystemScales[ii]);
	  else
	    sprintf(scaleLabel, "%1.0f billion km", solarSystemScales[ii]*0.001);
	  k = gdk_string_width(smallFont, scaleLabel);
	  gdk_draw_string(pixmap, smallFont, gC[OR_WHITE],
			  7*displayWidth/8 - k/2, y1 - 10, scaleLabel);
	}
	gdk_drawable_get_size(firstPointImage, &w, &h);
	gdk_draw_drawable(pixmap, gC[OR_BLUE], firstPointImage, 0, 0,
			  displayWidth/8, displayHeight/12 - 11, w, h);
	x1 = 10; x2 = x1 + 40; y1 = y2 = displayHeight/12 - 1; 
	gdk_draw_line(pixmap, gC[OR_BLUE], x1, y1, x2, y2);
	x2 = x1 + 10; y2 = y1 + 5;
	gdk_draw_line(pixmap, gC[OR_BLUE], x1, y1, x2, y2);
	y2 = y1 - 5;
	gdk_draw_line(pixmap, gC[OR_BLUE], x1, y1, x2, y2);
	if (schematic) {
	  plotStart = 0;
	  plotStop = N_SOLAR_SYSTEM_OBJECTS - 1;
	  plotInc = 1;
	} else {
	  plotStop = -1;
	  plotStart = N_SOLAR_SYSTEM_OBJECTS - 1;
	  plotInc = -1;
	}
	for (i = plotStart; i != plotStop; i += plotInc) {
	  plotPlanet = TRUE;
	  if (firstCall) {
	    sprintf(fileName, "%s/icons/%sSmall.xpm", dataDir, solarSystemNames[i]);
	    smallPlanetImages[i] = gdk_pixmap_create_from_xpm(pixmap,
							      NULL, NULL, fileName);
	  }
	  if (((schematic) || ( i < outermostPlanet)) &&
	      !(!schematic && (i == MOON))) {
	    gdk_drawable_get_size(smallPlanetImages[i], &w, &h);
	    /* Choose colors for the planets */
	    switch (i) {
	    case SUN:
	      planetGC = gC[OR_LIGHT_YELLOW];
	      break;
	    case MERCURY:
	    case MOON:
	      planetGC = gC[OR_GREY];
	      break;
	    case VENUS:
	      planetGC = gC[OR_WHITE];
	      break;
	    case EARTH:
	      planetGC = gC[OR_BLUE_GREEN];
	      break;
	    case MARS:
	      planetGC = gC[OR_RED];
	      break;
	    case JUPITER:
	    case SATURN:
	      planetGC = gC[OR_CREAM];
	      break;
	    case URANUS:
	    case NEPTUNE:
	      planetGC = gC[OR_BLUE];
	    }
	    calculatePlanetPosition(i, tJD, &eX, &eY, &eZ);
	    /* Draw the planet */
	    if (i != SUN) {
	      eHyp = sqrt(eX*eX + eY*eY);
	      eD  = roundf(atan2f(eY, eX) / DEGREES_TO_RADIANS);
	      while (eD < 0)
		eD += 360;
	      while (eD >= 360)
		eD -= 360;
	      eSin = (float)(eY/eHyp);
	      eCos = -(float)(eX/eHyp);
	    } else
	      sunELong = atan2(eY, eX);
	    switch (i) {
	    case SUN:
	      x = SOLAR_SYSTEM_CENTER_X;
	      y = SOLAR_SYSTEM_CENTER_Y;
	      gdk_draw_arc(pixmap, planetGC, TRUE,
			   x-33, y-33, 20, 20, 0, FULL_CIRCLE);
	      gdk_draw_drawable(pixmap, gC[OR_BLUE], smallPlanetImages[i], 0, 0,
				x-w/2-23, y-h/2-23, 20, 20);
	      
	      break;
	    case MERCURY:
	    case VENUS:
	      if (schematic) {
		x = SOLAR_SYSTEM_CENTER_X - SCHEMATIC_ORBIT_INCREMENT*(i+1);
		y = SOLAR_SYSTEM_CENTER_Y - SCHEMATIC_ORBIT_INCREMENT*(i+1);
		gdk_draw_arc(pixmap, gC[OR_BLUE], FALSE,
			     x, y, 2*i*SCHEMATIC_ORBIT_INCREMENT, 2*i*SCHEMATIC_ORBIT_INCREMENT, 0, FULL_CIRCLE);
		x = SOLAR_SYSTEM_CENTER_X + roundf(eCos*(float)(i*SCHEMATIC_ORBIT_INCREMENT));
		y = SOLAR_SYSTEM_CENTER_Y + roundf(eSin*(float)(i*SCHEMATIC_ORBIT_INCREMENT));
	      } else {
		int ii, px1, px2, py1, py2;
		GdkPoint orbit[360];

		if (orbitPlots[i].maxX*orbitScale > roundf(1.5*(float)SCHEMATIC_ORBIT_INCREMENT)) {
		  for (ii = 0; ii < 360; ii++) {
		    orbit[ii].x = SOLAR_SYSTEM_CENTER_X + orbitPlots[i].x[ii]*orbitScale - 23;
		    orbit[ii].y = SOLAR_SYSTEM_CENTER_Y + orbitPlots[i].y[ii]*orbitScale - 23;
		  }
		  gdk_draw_polygon(pixmap, gC[OR_BLUE], FALSE, orbit, 360);
		  px1 = SOLAR_SYSTEM_CENTER_X -
		    roundf(((float)(SCHEMATIC_ORBIT_INCREMENT/2))*cos(orbitPlots[i].omegaRad)) - 23;
		  py1 = SOLAR_SYSTEM_CENTER_Y +
		    roundf(((float)(SCHEMATIC_ORBIT_INCREMENT/2))*sin(orbitPlots[i].omegaRad)) - 23;
		  px2 = SOLAR_SYSTEM_CENTER_X -
		    roundf((orbitPlots[i].peri*orbitScale) * cos(orbitPlots[i].omegaRad)) - 23;
		  py2 = SOLAR_SYSTEM_CENTER_Y +
		    roundf((orbitPlots[i].peri*orbitScale) * sin(orbitPlots[i].omegaRad)) - 23;
		  eR = sqrt(orbitPlots[i].x[eD]*orbitPlots[i].x[eD] +
			    orbitPlots[i].y[eD]*orbitPlots[i].y[eD]);
		  gdk_draw_line(pixmap, gC[OR_DARK_GREY], px1, py1, px2, py2);
		  orbit[0].x = px2-1; orbit[0].y = py2-1;
		  orbit[1].x = px2+1; orbit[1].y = py2-1;
		  orbit[2].x = px2+1; orbit[2].y = py2+1;
		  orbit[3].x = px2-1; orbit[3].y = py2+1;
		  gdk_draw_polygon(pixmap, gC[OR_WHITE], TRUE, orbit, 4);
		  x = SOLAR_SYSTEM_CENTER_X + eR*eCos*orbitScale;
		  y = SOLAR_SYSTEM_CENTER_Y + eR*eSin*orbitScale;
		} else
		  plotPlanet = FALSE;
	      }
	      if (plotPlanet) {
		gdk_draw_arc(pixmap, planetGC, TRUE,
			     x-33, y-33, 20, 20, 0, FULL_CIRCLE);
		gdk_draw_drawable(pixmap, gC[OR_BLUE], smallPlanetImages[i], 0, 0,
				  x-w/2-23, y-h/2-23, 20, 20);
	      }
	      break;
	    case EARTH:
	    case MARS:
	    case JUPITER:
	    case SATURN:
	    case URANUS:
	    case NEPTUNE:
	      if (schematic) {
		x = SOLAR_SYSTEM_CENTER_X - SCHEMATIC_ORBIT_INCREMENT*(i+2);
		y = SOLAR_SYSTEM_CENTER_Y - SCHEMATIC_ORBIT_INCREMENT*(i+2);
		gdk_draw_arc(pixmap, gC[OR_BLUE], FALSE,
			     x, y, 2*(i+1)*SCHEMATIC_ORBIT_INCREMENT, 2*(i+1)*SCHEMATIC_ORBIT_INCREMENT,
			     0, FULL_CIRCLE);
		x = SOLAR_SYSTEM_CENTER_X + roundf(eCos*(float)((i+1)*SCHEMATIC_ORBIT_INCREMENT));
		y = SOLAR_SYSTEM_CENTER_Y + roundf(eSin*(float)((i+1)*SCHEMATIC_ORBIT_INCREMENT));
	      } else {
		int ii, px1, px2, py1, py2;
		GdkPoint orbit[360];
		
		if (orbitPlots[i].maxX*orbitScale > roundf(1.5*(float)SCHEMATIC_ORBIT_INCREMENT)) {
		  for (ii = 0; ii < 360; ii++) {
		    orbit[ii].x = SOLAR_SYSTEM_CENTER_X + orbitPlots[i].x[ii]*orbitScale - 23;
		    orbit[ii].y = SOLAR_SYSTEM_CENTER_Y + orbitPlots[i].y[ii]*orbitScale - 23;
		  }
		  gdk_draw_polygon(pixmap, gC[OR_BLUE], FALSE, orbit, 360);
		  px1 = SOLAR_SYSTEM_CENTER_X -
		    roundf(((float)(SCHEMATIC_ORBIT_INCREMENT/2))*cos(orbitPlots[i].omegaRad)) - 23;
		  py1 = SOLAR_SYSTEM_CENTER_Y +
		    roundf(((float)(SCHEMATIC_ORBIT_INCREMENT/2))*sin(orbitPlots[i].omegaRad)) - 23;
		  px2 = SOLAR_SYSTEM_CENTER_X -
		    roundf((orbitPlots[i].peri*orbitScale) * cos(orbitPlots[i].omegaRad)) - 23;
		  py2 = SOLAR_SYSTEM_CENTER_Y +
		    roundf((orbitPlots[i].peri*orbitScale) * sin(orbitPlots[i].omegaRad)) - 23;
		  eR = sqrt(orbitPlots[i].x[eD]*orbitPlots[i].x[eD] +
			    orbitPlots[i].y[eD]*orbitPlots[i].y[eD]);
		  gdk_draw_line(pixmap, gC[OR_DARK_GREY], px1, py1, px2, py2);
		  orbit[0].x = px2-1; orbit[0].y = py2-1;
		  orbit[1].x = px2+1; orbit[1].y = py2-1;
		  orbit[2].x = px2+1; orbit[2].y = py2+1;
		  orbit[3].x = px2-1; orbit[3].y = py2+1;
		  gdk_draw_polygon(pixmap, gC[OR_WHITE], TRUE, orbit, 4);
		  eR = sqrt(orbitPlots[i].x[eD]*orbitPlots[i].x[eD] +
			    orbitPlots[i].y[eD]*orbitPlots[i].y[eD]);
		  x = SOLAR_SYSTEM_CENTER_X + eR*eCos*orbitScale;
		  y = SOLAR_SYSTEM_CENTER_Y + eR*eSin*orbitScale;
		} else
		  plotPlanet = FALSE;
	      }
	      if (plotPlanet) {
		gdk_draw_arc(pixmap, planetGC, TRUE,
			     x-33, y-33, 20, 20, 0, FULL_CIRCLE);
		gdk_draw_drawable(pixmap, gC[OR_BLUE], smallPlanetImages[i], 0, 0,
				  x-w/2-23, y-h/2-23, 20, 20);
	      }
	      break;
	    case MOON:
	      if (schematic) {
		calculatePlanetPosition(EARTH, tJD, &eX, &eY, &eZ);
		sunELong = atan2(eY, eX);
		x -= 2*SCHEMATIC_ORBIT_INCREMENT;
		y -= 2*SCHEMATIC_ORBIT_INCREMENT;
		gdk_draw_arc(pixmap, gC[OR_BLUE], FALSE,
			     x, y, 2*SCHEMATIC_ORBIT_INCREMENT, 2*SCHEMATIC_ORBIT_INCREMENT, 0, FULL_CIRCLE);
		getMoonPosition(tJD, sunELong, &dDummy, &dDummy, &eLong, &dDummy, &fDummy);
		x += 2*SCHEMATIC_ORBIT_INCREMENT - roundf(cos(eLong)*SCHEMATIC_ORBIT_INCREMENT);
		y += 2*SCHEMATIC_ORBIT_INCREMENT + roundf(sin(eLong)*SCHEMATIC_ORBIT_INCREMENT);
		gdk_draw_arc(pixmap, planetGC, TRUE,
			     x-33, y-33, 20, 20, 0, FULL_CIRCLE);
		gdk_draw_drawable(pixmap, gC[OR_BLUE], smallPlanetImages[i], 0, 0,
				  x-w/2-23, y-h/2-23, 20, 20);
	      }
	      break;
	    }
	  }
	} /* End of loop over i, which plots the planets which fit on the display */
	if (!schematic && showComets) {
	  double lComet, bComet, rComet;
	  cometEphem *comet;

	  if (!cometDataReadIn)
	    readInCometEphemerides(dataDir);
	  comet = cometRoot;
	  while (comet != NULL) {
	    if (comet->valid && (tJD >= comet->firstTJD) && (tJD <= comet->lastTJD)) {
	      int i, nEntries;
	      GdkPoint *orbitPoints;

	      nEntries = comet->nEntries;
	      orbitPoints = (GdkPoint *)malloc(nEntries*sizeof(GdkPoint));
	      if (unlikely(orbitPoints == NULL)) {
		perror("orbitPoints");
		exit(ERROR_EXIT);
	      }
	      /* Plot the comet orbit */
	      for (i = 0; i < nEntries; i++) {
		double lr, cbr, r;

		r = comet->radius[i];
		lr = comet->eLong[i]*DEGREES_TO_RADIANS;
		cbr = cos(comet->eLat[i]*DEGREES_TO_RADIANS);
		orbitPoints[i].x = SOLAR_SYSTEM_CENTER_X - roundf(orbitScale*r*cbr*cos(lr)) - 23;
		orbitPoints[i].y = SOLAR_SYSTEM_CENTER_Y + roundf(orbitScale*r*cbr*sin(lr)) - 23;
	      }
	      gdk_draw_lines(pixmap, gC[OR_GREEN], orbitPoints, nEntries);
	      free(orbitPoints);
	      /* Now plot the commet itself (if it should be) */
	      getCometRADec(dataDir, comet->name, tJD, FALSE, &lComet, &bComet, &rComet, NULL);
	      x = SOLAR_SYSTEM_CENTER_X -
		roundf(orbitScale*rComet*cos(DEGREES_TO_RADIANS*bComet)*cos(DEGREES_TO_RADIANS*lComet));
	      y = SOLAR_SYSTEM_CENTER_Y +
		roundf(orbitScale*rComet*cos(DEGREES_TO_RADIANS*bComet)*sin(DEGREES_TO_RADIANS*lComet));
	      plotComet(x-23, y-23, atan2f(y-SOLAR_SYSTEM_CENTER_Y, x-SOLAR_SYSTEM_CENTER_X));
	    }
	    comet = comet->next;
	  }
	} /* End of comet portion */
	if (j > 0) {
	  if (j < nSolarSystemDays -1)
	    gdk_draw_drawable(drawingArea->window,
			      drawingArea->style->fg_gc[GTK_WIDGET_STATE (drawingArea)],
			      pixmap, 0, BIG_FONT_OFFSET, 0, BIG_FONT_OFFSET,
			      displayWidth, displayHeight*6/7 - 14 - BIG_FONT_OFFSET);
	  else
	    gdk_draw_drawable(drawingArea->window,
			      drawingArea->style->fg_gc[GTK_WIDGET_STATE (drawingArea)],
			      pixmap, 0, 0, 0, 0,
			      displayWidth, displayHeight - 14);
	  gdk_flush();
	}
	firstCall = firstIteration = FALSE;
	j++;
	if (j < nSolarSystemDays)
	  tJD += dayIncSign;
      } while (j < nSolarSystemDays);
      addSensitiveArea(FALSE, SA_SOLAR_SYSTEM_BUTTON,
		       0, displayHeight*3/4, displayWidth,
		       displayHeight, 0.0);
      if (!schematic)
	havePlottedOrbits = TRUE;
    }
    break;
  case METEOR_SHOWERS_SCREEN:
    {
      int yearNow, monthNow, dayNow, occuringNow, circumpolar, darkStartHH, darkEndHH;
      int darkStartMM, darkEndMM;
      float illum, illum2;
      double dayOffset, showerTJD, dummy, startDayNumber, endDayNumber, todayDayNumber;
      double jan0TJD, darkHours, midnight, darkHoursTonight, rA, dec, az, zA, h0, startNight;
      double darkStart, darkEnd;
      char rateString[4], phaseGrad[2];
      meteorShower *shower;
      GdkGC *lineGC;

      if (!haveReadMeteorShowers) {
	readMeteorShowers();
	haveReadMeteorShowers = TRUE;
      }
      needNewTime = TRUE;
      lSTNow = lST();
      planetInfo(dataDir, EARTH, tJD, &rA, &dec, &illum, &illum);
      azZA(rA, sin(dec), cos(dec), &az, &zA, FALSE);
      h0 = -(50.0/60.0)*DEGREES_TO_RADIANS;
      if (zA < M_HALF_PI) {
	/* The sun is up, calculate dark time from next sunset to following sunset */
	startNight = calcRiseOrSetTime(FALSE, EARTH, tJD, lSTAtTJD(tJD),
				       h0, &circumpolar, &dummy, &dummy, &dummy, &illum);
      } else {
	/* The sun is down, calculate dark time from last sunset to next sunset */
	startNight = calcRiseOrSetTime(FALSE, EARTH, tJD-1.0, lSTAtTJD(tJD-1.0),
				       h0, &circumpolar, &dummy, &dummy, &dummy, &illum);
	if ((!circumpolar) && (tJD - startNight > 1.0))
	  startNight = calcRiseOrSetTime(FALSE, EARTH, tJD, lSTAtTJD(tJD),
					 h0, &circumpolar, &dummy, &dummy, &dummy, &illum);
      }
      tJDToDate(startNight, &year, &month, &dayNum);
      if (!circumpolar)
	darkHoursTonight = darkTime(startNight, startNight + 1.0, &darkStart, &darkEnd);
      else
	darkHoursTonight = darkTime(tJD-0.5, tJD+0.5, &darkStart, &darkEnd);
      dummy = 24.0*(darkStart - (double)((int)darkStart)) + 12.0;
      if (dummy >= 24.0)
	dummy -= 24.0;
      darkStartHH = (int)dummy;
      darkStartMM = (int)((dummy - (double)darkStartHH)*60.0);
      dummy = 24.0*(darkEnd - (double)((int)darkEnd)) + 12.0;
      if (dummy >= 24.0)
	dummy -= 24.0;
      darkEndHH = (int)dummy;
      darkEndMM = (int)((dummy - (double)darkEndHH)*60.0);
      tJDToDate(tJD, &yearNow, &monthNow, &dayNow);
      jan0TJD = buildTJD(yearNow-1900, 0, 0, 0, 0, 0, 0);
      sprintf(scratchString, "Meteor Shower Information for %04d", yearNow);
      stringWidth = gdk_string_width(smallFont, scratchString);
      gdk_draw_string(pixmap, smallFont, gC[OR_WHITE],
		      (displayWidth/2)-stringWidth/2,
		      (row++)*ABOUT_ROW_STEP + BIG_FONT_OFFSET, scratchString);
      row++;
      gdk_draw_line(pixmap, gC[OR_BLUE],
		    xBorder, row*ABOUT_ROW_STEP, displayWidth-xBorder, row*ABOUT_ROW_STEP);
      row++;
      sprintf(scratchString, "     Shower Name       Abr  Rate      Dates       Max    V Moon%% Dark Hours");
      stringWidth = gdk_string_width(smallFont, scratchString);
      gdk_draw_string(pixmap, smallFont, gC[OR_BLUE], 0,
		      (row++)*ABOUT_ROW_STEP + BIG_FONT_OFFSET, scratchString);
      shower = meteorShowers;
      while (shower != NULL) {
	startDayNumber= makeDayNumber(yearNow, shower->startMonth, shower->startDay);
	endDayNumber= makeDayNumber(yearNow, shower->endMonth, shower->endDay);
	todayDayNumber= makeDayNumber(yearNow, monthNow, dayNow);
	dayOffset = makeDayNumber(yearNow, shower->maxMonth, shower->maxDay);
	showerTJD = jan0TJD + dayOffset;
	if (startDayNumber < endDayNumber) {
	  if ((todayDayNumber >= startDayNumber) && (todayDayNumber <= endDayNumber))
	    occuringNow = TRUE;
	  else
	    occuringNow = FALSE;
	} else {
	  /* Handle case when shower starts in December and ends in January */
	  if ((todayDayNumber < startDayNumber) && (todayDayNumber > endDayNumber))
	    occuringNow = FALSE;
	  else
	    occuringNow = TRUE;
	}
	planetInfo(dataDir, EARTH, showerTJD, &dummy, &dummy, &illum2, &mag);
	planetInfo(dataDir, MOON, showerTJD+0.04, &dummy, &dummy, &illum2, &mag);
	planetInfo(dataDir, MOON, showerTJD, &dummy, &dummy, &illum, &mag);
	midnight = (double)((int)(showerTJD + 0.5)) + 0.5;
	darkHours = darkTime(midnight - 0.5, midnight + 0.5, &dummy, &dummy);
	if (illum2 > illum)
	  strcpy(phaseGrad, "+");
	else
	  strcpy(phaseGrad, "-");
	if (occuringNow && (darkHoursTonight > 2.0))
	  lineGC = gC[OR_GREEN];
	else if (occuringNow)
	  lineGC = gC[OR_DARK_GREEN];
	else if (darkHours > 2.0)
	  lineGC = gC[OR_CREAM];
	else
	  lineGC = gC[OR_GREY];
	if (shower->zHR > 0)
	  sprintf(rateString, "%3d", shower->zHR);
	else
	  strcpy(rateString, "Var");
	sprintf(scratchString, "%21s  %3s  %3s  %02d/%02d -> %02d/%02d  %02d/%02d  %d  %3.0f%s    %4.1f",
		shower->fullName, shower->threeLetterName,
		rateString, shower->startMonth, shower->startDay,
		shower->endMonth, shower->endDay, shower->maxMonth, shower->maxDay,
		shower->vInf, illum*100, phaseGrad, darkHours);
	gdk_draw_string(pixmap, smallFont, lineGC, 0,
			(row++)*(ABOUT_ROW_STEP+2) + BIG_FONT_OFFSET, scratchString);
	shower = shower->next;
      }
      if (darkHoursTonight == 0.0) {
	sprintf(scratchString, "At no time tonight will the sky be very dark.");
	lineGC = gC[OR_RED];
      } else if (darkHoursTonight > 2.0) {
	sprintf(scratchString,
		"The sky will be very dark for %3.1f hours (%02d:%02d -> %02d:%02d UT) tonight.",
		darkHoursTonight, darkStartHH, darkStartMM, darkEndHH, darkEndMM);
	lineGC = gC[OR_WHITE];
      } else {
	sprintf(scratchString,
		"The sky will be very dark for only %3.1f hours (%02d:%02d -> %02d:%02d UT) tonight.",
		darkHoursTonight, darkStartHH, darkStartMM, darkEndHH, darkEndMM);
	lineGC = gC[OR_RED];
      }
      stringWidth = gdk_string_width(smallFont, scratchString);
      gdk_draw_string(pixmap, smallFont, lineGC, (displayWidth - stringWidth)/2,
		      (row++)*(ABOUT_ROW_STEP+2) + BIG_FONT_OFFSET + 5, scratchString);
      
    }
    break;
  case TIMES_PAGE_SCREEN:
    {
      int sHH, sMM, sSS, eQSign;
      float sidereal;
      double localSolarTime, E, dec, tJD, mJD, EoE;

      needNewTime = TRUE;
      lSTNow = lST();
      sprintf(scratchString, "Times");
      renderPangoText(scratchString, OR_CREAM, BIG_PANGO_FONT, &tWidth, &tHeight,
		      pixmap, displayWidth >> 1, 20, 0.0, TRUE, 0);
      sprintf(scratchString, "----- Global Values -----");
      renderPangoText(scratchString, OR_BLUE, MEDIUM_PANGO_FONT, &tWidth, &tHeight,
		      pixmap, displayWidth >> 1, 65, 0.0, TRUE, 0);
      tJD = getTJD();
      mJD = tJD - 2400000.5;
      sprintf(scratchString, "JD");
      renderPangoText(scratchString, OR_CREAM, BIG_PANGO_FONT, &tWidth, &tHeight,
		      pixmap, 5, 110, 0.0, FALSE, 0);
      sprintf(scratchString, "%14.6f", tJD);
      renderPangoText(scratchString, OR_WHITE, BIG_PANGO_FONT, &tWidth, &tHeight,
		      pixmap, 2 + (displayWidth >> 2), 110, 0.0, FALSE, 0);
      sprintf(scratchString, "MJD");
      renderPangoText(scratchString, OR_CREAM, BIG_PANGO_FONT, &tWidth, &tHeight,
		      pixmap, 5, 170, 0.0, FALSE, 0);
      sprintf(scratchString, "%14.6f", mJD);
      renderPangoText(scratchString, OR_WHITE, BIG_PANGO_FONT, &tWidth, &tHeight,
		      pixmap, 26 + (displayWidth >> 2), 170, 0.0, FALSE, 0);
      sprintf(scratchString, "UT");
      renderPangoText(scratchString, OR_CREAM, BIG_PANGO_FONT, &tWidth, &tHeight,
		      pixmap, 5, 230, 0.0, FALSE, 0);
      sprintf(scratchString, "%02d:%02d:%02d",
	      gMT->tm_hour, gMT->tm_min, (int)gMT->tm_sec);
      renderPangoText(scratchString, OR_WHITE, BIG_PANGO_FONT, &tWidth, &tHeight,
		      pixmap, 31 + (displayWidth >> 1), 230, 0.0, FALSE, 0);
      sprintf(scratchString, "Eq. of Time");
      renderPangoText(scratchString, OR_CREAM, BIG_PANGO_FONT, &tWidth, &tHeight,
		      pixmap, 5, 290, 0.0, FALSE, 0);
      analemma(dataDir, tJD, &E, &dec, &EoE, NULL);
      localSolarTime = E / HOURS_TO_RADIANS;
      if (localSolarTime < 0.0) {
	eQSign = -1;
	localSolarTime *= -1.0;
      } else
	eQSign = 1;
      sHH = (int)localSolarTime;
      sMM = (int)(60.0*(localSolarTime-(float)sHH));
      sSS = (int)((3600.0*(localSolarTime-(float)sHH - ((float)sMM)/60.0)) + 0.5);
      if (eQSign < 0) {
	sprintf(scratchString, "-%02d:%02d:%02d", sHH, sMM, sSS);
	renderPangoText(scratchString, OR_WHITE, BIG_PANGO_FONT, &tWidth, &tHeight,
			pixmap, 16 + (displayWidth >> 1), 290, 0.0, FALSE, 0);
      } else {
	sprintf(scratchString, "%02d:%02d:%02d", sHH, sMM, sSS);
	renderPangoText(scratchString, OR_WHITE, BIG_PANGO_FONT, &tWidth, &tHeight,
			pixmap, 31 + (displayWidth >> 1), 290, 0.0, FALSE, 0);
      }
      sprintf(scratchString, "Eq. of Equin.");
      renderPangoText(scratchString, OR_CREAM, BIG_PANGO_FONT, &tWidth, &tHeight,
		      pixmap, 5, 350, 0.0, FALSE, 0);
      sprintf(scratchString, "%7.4f", EoE);
      renderPangoText(scratchString, OR_WHITE, BIG_PANGO_FONT, &tWidth, &tHeight,
		      pixmap, 59 + (displayWidth >> 1), 350, 0.0, FALSE, 0);
      sprintf(scratchString, "----- Local Values -----");
      renderPangoText(scratchString, OR_BLUE, MEDIUM_PANGO_FONT, &tWidth, &tHeight,
		      pixmap, displayWidth >> 1, 423, 0.0, TRUE, 0);
      sprintf(scratchString, "Mean Solar");
      renderPangoText(scratchString, OR_CREAM, BIG_PANGO_FONT, &tWidth, &tHeight,
		      pixmap, 5, 470, 0.0, FALSE, 0);
      localSolarTime = ((double)gMT->tm_hour
			+ ((double)gMT->tm_min)/60.0
			+ ((double)gMT->tm_sec)/3600.0
			+ cNSecond/(3600.0e9)) * HOURS_TO_RADIANS;
      localSolarTime += longitude;
      doubleNormalize0to2pi(&localSolarTime);
      localSolarTime /= HOURS_TO_RADIANS;
      sHH = (int)localSolarTime;
      sMM = (int)(60.0*(localSolarTime-(float)sHH));
      sSS = (int)(3600.0*(localSolarTime-(float)sHH - ((float)sMM)/60.0));
      sprintf(scratchString, "%02d:%02d:%02d", sHH, sMM, sSS);
      renderPangoText(scratchString, OR_WHITE, BIG_PANGO_FONT, &tWidth, &tHeight,
		      pixmap, 31 + (displayWidth >> 1), 470, 0.0, FALSE, 0);
      sprintf(scratchString, "App. Solar");
      renderPangoText(scratchString, OR_CREAM, BIG_PANGO_FONT, &tWidth, &tHeight,
		      pixmap, 5, 530, 0.0, FALSE, 0);
      localSolarTime += E/HOURS_TO_RADIANS;
      localSolarTime *= HOURS_TO_RADIANS;
      doubleNormalize0to2pi(&localSolarTime);
      localSolarTime /= HOURS_TO_RADIANS;
      sHH = (int)localSolarTime;
      sMM = (int)(60.0*(localSolarTime-(float)sHH));
      sSS = (int)(3600.0*(localSolarTime-(float)sHH - ((float)sMM)/60.0));
      sprintf(scratchString, "%02d:%02d:%02d", sHH, sMM, sSS);
      renderPangoText(scratchString, OR_WHITE, BIG_PANGO_FONT, &tWidth, &tHeight,
		      pixmap, 31 + (displayWidth >> 1), 530, 0.0, FALSE, 0);
      sprintf(scratchString, "Mean LST");
      renderPangoText(scratchString, OR_CREAM, BIG_PANGO_FONT, &tWidth, &tHeight,
		      pixmap, 5, 590, 0.0, FALSE, 0);
      sidereal = lST()/HOURS_TO_RADIANS;
      sHH = (int)sidereal;
      sMM = (int)(60.0*(sidereal-(float)sHH));
      sSS = (int)(3600.0*(sidereal-(float)sHH - ((float)sMM)/60.0));
      sprintf(scratchString, "%02d:%02d:%02d", sHH, sMM, sSS);
      renderPangoText(scratchString, OR_WHITE, BIG_PANGO_FONT, &tWidth, &tHeight,
		      pixmap, 31 + (displayWidth >> 1), 590, 0.0, FALSE, 0);
      sprintf(scratchString, "App. LST");
      renderPangoText(scratchString, OR_CREAM, BIG_PANGO_FONT, &tWidth, &tHeight,
		      pixmap, 5, 650, 0.0, FALSE, 0);
      sidereal = lST() + HOURS_TO_RADIANS*EoE/3600.0;
      floatNormalize0to2pi(&sidereal);
      sidereal /= HOURS_TO_RADIANS;
      sHH = (int)sidereal;
      sMM = (int)(60.0*(sidereal-(float)sHH));
      sSS = (int)(3600.0*(sidereal-(float)sHH - ((float)sMM)/60.0));
      sprintf(scratchString, "%02d:%02d:%02d", sHH, sMM, sSS);
      renderPangoText(scratchString, OR_WHITE, BIG_PANGO_FONT, &tWidth, &tHeight,
		      pixmap, 33 + (displayWidth >> 1), 650, 0.0, FALSE, 0);
    }
    break;
#define ANA_TOP           (  75)
#define ANA_LEFT          (  90)
#define ANA_HEIGHT        ( 575)
#define ANA_WIDTH         ( 380)
#define N_ANALEMMA_POINTS (3654)
  case ANALEMMA_SCREEN:
    {
      int index = 0;
      int nPoints = 0;
      int tWidth, tHeight, i, h, w;
      /*                 J    F    M    A    M    J    J    A    S    O    N    D  */
      int mXOffN[12] = {-17,  10,  10, -60,  10,  10, -10, -60,  10,   8, -60, -65};
      int mYOffN[12] = {-19,   0,  10,   0,  10, -10,  18,   0,   0, -10,   0,  -8};
      int mXOffS[12] = {-17,  10,  10, -58,  10,  10, -40, -55,  13,   8, -57,  12};
      int mYOffS[12] = {-19,   0, -15,   0,   0,   5,  17,   0,   0,  13,   0, -15};
      double startTJD, jD, spring, summer, fall, winter, latOffset, latSign;
      double E[N_ANALEMMA_POINTS], dec[N_ANALEMMA_POINTS], maxE, minE, maxDec, minDec,
	xScale, yScale;
      char shortMonthName[4];
      GdkPoint analemmaPoint[N_ANALEMMA_POINTS];

      needNewTime = TRUE;
      lST();
      sprintf(scratchString, "%d Analemma", (int)cYear);
      renderPangoText(scratchString, OR_CREAM, BIG_PANGO_FONT, &tWidth, &tHeight,
		      pixmap, displayWidth >> 1, 15, 0.0, TRUE, 0);
      jD = startTJD = buildTJD((int)(cYear-1900.0), 0, 1, 0, 0, 0, 0);
      maxE = maxDec = -1.0e10;
      minE = minDec = 1.0e10;
      seasons(dataDir, (int)cYear, &spring, &summer, &fall, &winter);
      latOffset = (M_HALF_PI - latitude) / DEGREES_TO_RADIANS;
      if (northernHemisphere)
	latSign = 1.0;
      else
	latSign = -1.0;
      do {
	analemma(dataDir, jD, &E[index], &dec[index], NULL, NULL);
	dec[index] += latOffset;
	dec[index] *= latSign;
	if (minE > E[index])
	  minE = E[index];
	if (maxE < E[index])
	  maxE = E[index];
	if (minDec > dec[index])
	  minDec = dec[index];
	if (maxDec < dec[index])
	  maxDec = dec[index];
	index++;
	jD += 1.0;
      } while (jD < startTJD + 365.25);
      nPoints = index;
      xScale = (double)ANA_WIDTH/(maxE - minE);
      yScale = (double)ANA_HEIGHT/(maxDec - minDec);
      for (i = -15; i <= 15; i += 5) {
	double min;

	min = ((double)i) * HOURS_TO_RADIANS / 60.0;
	analemmaPoint[0].x = analemmaPoint[1].x = (int)((double)ANA_LEFT + (min - minE)*xScale + 0.5);
	analemmaPoint[0].y = ANA_TOP - 30;
	analemmaPoint[1].y = ANA_TOP + ANA_HEIGHT + 37;
	gdk_draw_lines(pixmap, gC[OR_BLUE], analemmaPoint, 2);
	sprintf(scratchString, "%d", i);
	renderPangoText(scratchString, OR_CREAM, MEDIUM_PANGO_FONT, &tWidth, &tHeight,
			pixmap, analemmaPoint[1].x, analemmaPoint[1].y+10, 0.0, TRUE, 0);
      }
      sprintf(scratchString, "%s", "Equation of Time (minutes)");
      renderPangoText(scratchString, OR_CREAM, MEDIUM_PANGO_FONT, &tWidth, &tHeight,
		      pixmap, ANA_LEFT + (ANA_WIDTH >> 1) , analemmaPoint[1].y+40, 0.0, TRUE, 0);
      analemmaPoint[0].x = ANA_LEFT-25;
      analemmaPoint[1].x = displayWidth;
      for (i = -200; i <= 200; i += 10) {
	int printAlt;
	double deg;

	deg = (double)i;
	if (northernHemisphere) {
	  if (i <= 90)
	    printAlt = i;
	  else
	    printAlt = 180 - i;
	} else {
	  printAlt = 180 + i;
	  if (printAlt > 90)
	    printAlt = 180 - printAlt;
	}
	analemmaPoint[0].y = analemmaPoint[1].y = (int)((double)(ANA_TOP+ANA_HEIGHT) - (deg-minDec)*yScale + 0.5);
	if ((analemmaPoint[0].y > ANA_TOP) && (analemmaPoint[0].y < ANA_TOP + ANA_HEIGHT)) {
	  sprintf(scratchString, "%d", printAlt);
	  if (printAlt >= 0) {
	    gdk_draw_lines(pixmap, gC[OR_BLUE], analemmaPoint, 2);
	    renderPangoText(scratchString, OR_CREAM, MEDIUM_PANGO_FONT, &tWidth, &tHeight,
			    pixmap, analemmaPoint[0].x - 15, analemmaPoint[0].y, 0.0, TRUE, 0);
	  } else {
	    gdk_draw_lines(pixmap, gC[OR_DARK_GREY], analemmaPoint, 2);
	    renderPangoText(scratchString, OR_GREY, MEDIUM_PANGO_FONT, &tWidth, &tHeight,
			    pixmap, analemmaPoint[0].x - 15, analemmaPoint[0].y, 0.0, TRUE, 0);
	  }
	}
      }
      sprintf(scratchString, "%s", "Sun Altitude at Transit (degrees)");
      renderPangoText(scratchString, OR_CREAM, MEDIUM_PANGO_FONT, &tWidth, &tHeight,
		      pixmap, 14, displayHeight >> 1, M_HALF_PI, TRUE, 0);
      for (index = 0; index < nPoints; index++) {
	analemmaPoint[index].x = (int)((double)ANA_LEFT + (E[index]-minE)*xScale + 0.5);
	analemmaPoint[index].y = (int)((double)(ANA_TOP+ANA_HEIGHT) - (dec[index]-minDec)*yScale + 0.5);
      }
      gdk_draw_polygon(pixmap, gC[OR_GREEN], FALSE, analemmaPoint, nPoints);
      jD = startTJD = buildTJD((int)(cYear-1900.0), 0, 1, 0, 0, 0, 0);
      shortMonthName[3] = (char)0;
      for (index = 0; index < 12; index++) {
	strncpy(shortMonthName, monthName[index], 3);
	analemma(dataDir, jD, &E[0], &dec[0], NULL, NULL);
	dec[0] += latOffset;
	dec[0] *= latSign;
	analemmaPoint[0].x = (int)((double)ANA_LEFT + (E[0]-minE)*xScale + 0.5);
	analemmaPoint[0].y = (int)((double)(ANA_TOP+ANA_HEIGHT) - (dec[0]-minDec)*yScale + 0.5);
	gdk_draw_arc(pixmap, gC[OR_CREAM], TRUE, analemmaPoint[0].x - 3, analemmaPoint[0].y - 3,
		     6, 6, 0, FULL_CIRCLE);
	sprintf(scratchString, "%s 1", shortMonthName);
	if (northernHemisphere)
	  renderPangoText(scratchString, OR_CREAM, SMALL_PANGO_FONT, &tWidth, &tHeight,
			  pixmap, analemmaPoint[0].x + mXOffN[index],
			  analemmaPoint[0].y + mYOffN[index], 0.0, FALSE, 0);
	else
	  renderPangoText(scratchString, OR_CREAM, SMALL_PANGO_FONT, &tWidth, &tHeight,
			  pixmap, analemmaPoint[0].x + mXOffS[index],
			  analemmaPoint[0].y + mYOffS[index], 0.0, FALSE, 0);
	jD += (double)monthLengths[index];
      }
      analemma(dataDir, spring, &E[0], &dec[0], NULL, NULL);
      dec[0] += latOffset;
      dec[0] *= latSign;
      analemmaPoint[0].x = (int)((double)ANA_LEFT + (E[0]-minE)*xScale + 0.5);
      analemmaPoint[0].y = (int)((double)(ANA_TOP+ANA_HEIGHT) - (dec[0]-minDec)*yScale + 0.5);
      gdk_draw_arc(pixmap, gC[OR_YELLOW], TRUE, analemmaPoint[0].x - 4, analemmaPoint[0].y - 4,
		     8, 8, 0, FULL_CIRCLE);
      gdk_drawable_get_size(firstPointImage, &w, &h);
      gdk_draw_drawable(pixmap, gC[OR_BLUE], firstPointImage, 0, 0,
			analemmaPoint[0].x - 28, analemmaPoint[0].y - h/2 - 4, w, h);
      analemmaPoint[0].x += 15;
      analemmaPoint[1].x = analemmaPoint[0].x + 30;
      analemmaPoint[1].y = analemmaPoint[0].y;
      gdk_draw_lines(pixmap, gC[OR_YELLOW], analemmaPoint, 2);
      analemmaPoint[1].x = analemmaPoint[0].x + 6;
      analemmaPoint[2].x = analemmaPoint[0].x + 6;
      analemmaPoint[1].y = analemmaPoint[0].y - 4;
      analemmaPoint[2].y = analemmaPoint[0].y + 4;
      gdk_draw_polygon(pixmap, gC[OR_YELLOW], TRUE, analemmaPoint, 3);
      analemma(dataDir, fall, &E[0], &dec[0], NULL, NULL);
      dec[0] += latOffset;
      dec[0] *= latSign;
      analemmaPoint[0].x = (int)((double)ANA_LEFT + (E[0]-minE)*xScale + 0.5);
      analemmaPoint[0].y = (int)((double)(ANA_TOP+ANA_HEIGHT) - (dec[0]-minDec)*yScale + 0.5);
      gdk_draw_arc(pixmap, gC[OR_YELLOW], TRUE, analemmaPoint[0].x - 4, analemmaPoint[0].y - 4,
		     8, 8, 0, FULL_CIRCLE);
      analemmaPoint[0].x -= 15;
      analemmaPoint[1].x = analemmaPoint[0].x - 30;
      analemmaPoint[1].y = analemmaPoint[0].y;
      gdk_draw_lines(pixmap, gC[OR_YELLOW], analemmaPoint, 2);
      analemmaPoint[1].x = analemmaPoint[0].x - 6;
      analemmaPoint[2].x = analemmaPoint[0].x - 6;
      analemmaPoint[1].y = analemmaPoint[0].y - 4;
      analemmaPoint[2].y = analemmaPoint[0].y + 4;
      gdk_draw_polygon(pixmap, gC[OR_YELLOW], TRUE, analemmaPoint, 3);
      sprintf(scratchString, "Equinoxes");
      renderPangoText(scratchString, OR_YELLOW, SMALL_PANGO_FONT, &tWidth, &tHeight,
		      pixmap, analemmaPoint[0].x - 123,
		      analemmaPoint[0].y - 1, 0.0, FALSE, 0);

      analemma(dataDir, summer, &E[0], &dec[0], NULL, NULL);
      dec[0] += latOffset;
      dec[0] *= latSign;
      analemmaPoint[0].x = (int)((double)ANA_LEFT + (E[0]-minE)*xScale + 0.5);
      analemmaPoint[0].y = (int)((double)(ANA_TOP+ANA_HEIGHT) - (dec[0]-minDec)*yScale + 0.5);
      gdk_draw_arc(pixmap, gC[OR_YELLOW], TRUE, analemmaPoint[0].x - 4, analemmaPoint[0].y - 4,
		     8, 8, 0, FULL_CIRCLE);
      sprintf(scratchString, "Solstice");
      if (northernHemisphere)
	renderPangoText(scratchString, OR_YELLOW, SMALL_PANGO_FONT, &tWidth, &tHeight,
			pixmap, analemmaPoint[0].x - 34,
			analemmaPoint[0].y - 15, 0.0, FALSE, 0);
      else
	renderPangoText(scratchString, OR_YELLOW, SMALL_PANGO_FONT, &tWidth, &tHeight,
			pixmap, analemmaPoint[0].x - 32,
			analemmaPoint[0].y - 20, 0.0, FALSE, 0);
      analemma(dataDir, winter, &E[0], &dec[0], NULL, NULL);
      dec[0] += latOffset;
      dec[0] *= latSign;
      analemmaPoint[0].x = (int)((double)ANA_LEFT + (E[0]-minE)*xScale + 0.5);
      analemmaPoint[0].y = (int)((double)(ANA_TOP+ANA_HEIGHT) - (dec[0]-minDec)*yScale + 0.5);
      gdk_draw_arc(pixmap, gC[OR_YELLOW], TRUE, analemmaPoint[0].x - 4, analemmaPoint[0].y - 4,
		     8, 8, 0, FULL_CIRCLE);
      sprintf(scratchString, "Solstice");
      renderPangoText(scratchString, OR_YELLOW, SMALL_PANGO_FONT, &tWidth, &tHeight,
		      pixmap, analemmaPoint[0].x - 32,
		      analemmaPoint[0].y + 17, 0.0, FALSE, 0);
      analemma(dataDir, tJD, &E[0], &dec[0], NULL, NULL);
      dec[0] += latOffset;
      dec[0] *= latSign;
      analemmaPoint[0].x = (int)((double)ANA_LEFT + (E[0]-minE)*xScale + 0.5);
      analemmaPoint[0].y = (int)((double)(ANA_TOP+ANA_HEIGHT) - (dec[0]-minDec)*yScale + 0.5);
      gdk_draw_arc(pixmap, gC[OR_WHITE], TRUE, analemmaPoint[0].x - 4, analemmaPoint[0].y - 4,
		     8, 8, 0, FULL_CIRCLE);
      gdk_draw_arc(pixmap, gC[OR_RED], TRUE, analemmaPoint[0].x - 2, analemmaPoint[0].y - 2,
		     4, 4, 0, FULL_CIRCLE);
      sprintf(scratchString, "Now");
      renderPangoText(scratchString, OR_WHITE, SMALL_PANGO_FONT, &tWidth, &tHeight,
		      pixmap, analemmaPoint[0].x + 10,
		      analemmaPoint[0].y - 10, 0.0, FALSE, 0);
    }
    break;
#define N_NAV_PLANETS  (6)
#define N_NAV_STARS   (58)
  case CELESTIAL_NAVIGATION:
    {
      /* Hipparcos catalog numbers of the 57 Navigational Stars plus Polaris */
      int navStarHipNumber[N_NAV_STARS] = {   677,   2081,   3179,   3419,   7588,
                                             9884,  13847,  14135,  15863,  21421,
                                            24436,  24608,  25336,  25428,  26311,
                                            27989,  30438,  32349,  33579,  37279,
                                            37826,  41037,  44816,  45238,  46390,
                                            49669,  54061,  57632,  59803,  60718,
                                            61084,  62956,  65474,  67301,  68702,
                                            68933,  69673,  71683,  72622,  72607,
                                            76267,  80763,  82273,  84012,  85927,
                                            86032,  87833,  90185,  91262,  92855,
                                            97649, 100751, 102098, 107315, 109268,
                                           113368, 113963,  11767                 };
      int navPlanets[N_NAV_PLANETS] = {EARTH, MOON, VENUS, MARS, JUPITER, SATURN};
      int vSOP87Mapping[N_NAV_PLANETS] = {2, 2, 1, 3, 4, 5};
      int moonVisible = FALSE;
      int color = OR_WHITE;
      int nVisibleStars = 0;
      int indX = displayWidth/3 - 17;
      int indY = 0;
      int nameY = 0;
      int navStar, planet, year, month, day, tWidth, tHeight,
	y, hAD, dD, eD, yLineTop, latDD, latMM, longDD, longMM;
      float dummyFloat, hAM, dM, eM, aD, illum, illum2, ref, latSS, longSS;
      double rA, dec, az, zA, el, hA, dDec, tR, pi, dummy, dummy2, dummy3, tLatitude, tLongitude;
      double parallax = 0.0;
      double sD = 0.0;
      char phaseString[25], latString[6], longString[5];
      starNameEntry *star, *starEntry[N_NAV_STARS], *visibleStars[N_NAV_STARS];

      needNewTime = updateCelestialNavigationScreen;
      updateCelestialNavigationScreen = FALSE;
      lSTNow = lST();
      tJDToDate(tJD, &year, &month, &day);
      sprintf(scratchString, "Navigation Data for %d/%d/%d at %02d:%02d:%02d",
	      month, day, year, gMT->tm_hour, gMT->tm_min, (int)gMT->tm_sec);
      if (displayIndividualNavObject)
	y = 30;
      else
	y = 10;
      renderPangoText(scratchString, OR_CREAM, MEDIUM_PANGO_FONT, &tWidth, &tHeight,
		      pixmap, displayWidth/2, y, 0.0, TRUE, 0);
      y += tHeight;
      tLatitude =  latitude/DEGREES_TO_RADIANS;
      tLongitude = longitude/DEGREES_TO_RADIANS;
      if (tLongitude < 0.0) {
	sprintf(longString, "West");
	tLongitude *= -1.0;
      } else
	sprintf(longString, "East");
      if (tLatitude < 0.0) {
	sprintf(latString, "South");
	tLatitude *= -1.0;
      } else
	sprintf(latString, "North");
      latDD = (int)tLatitude;
      latMM = (int)((tLatitude - (double)latDD) * 60.0);
      latSS = (tLatitude - (double)latDD - ((double)latMM)/60.0) * 3600.0;
      if (latSS >= 59.5) {
	latMM++;
	latSS = 0.0;
      }
      if (latMM > 59) {
	latMM = 0;
	latDD++;
      }
      longDD = (int)tLongitude;
      longMM = (int)((tLongitude - (double)longDD) * 60.0);
      longSS = (tLongitude - (double)longDD - ((double)longMM)/60.0) * 3600.0;
      if (longSS >= 59.5) {
	longMM++;
	longSS = 0.0;
      }
      if (longMM > 59) {
	longMM = 0;
	longDD++;
      }
      sprintf(scratchString, "Lat %02d:%02d:%02.0f %s   Long %03d:%02d:%02.0f %s",
	      latDD, latMM, latSS, latString, longDD, longMM, longSS, longString);
      renderPangoText(scratchString, OR_CREAM, MEDIUM_PANGO_FONT, &tWidth, &tHeight,
		      pixmap, displayWidth/2, y, 0.0, TRUE, 0);
      y += tHeight;
      yLineTop = y-9;
      if (!displayIndividualNavObject) {
	gdk_draw_string(pixmap, smallFont, gC[OR_BLUE], 0, y,
			"                         Almanac Data                  Altitude Corrections");
	y += 12;
	gdk_draw_string(pixmap, smallFont, gC[OR_CREAM], 0, y,
			" Object          GHA       Dec      Hc      Zn       Refr    SD     PA     Sum");
	y += 12;
	gdk_draw_string(pixmap, smallFont, gC[OR_CREAM], 0, y,
			"               o   '     o   '     o  '      o         '      '      '      '");
      } else {
	y += 20;
	nameY = y;
	y += 45;
	renderPangoText("Almanac Data", OR_BLUE, MEDIUM_PANGO_FONT, &tWidth, &tHeight,
			pixmap, displayWidth/2, y, 0.0, TRUE, 0);
	y += tHeight+12;
	indY = y;
	renderPangoText("GHA", OR_CREAM, MEDIUM_PANGO_FONT, &tWidth, &tHeight,
			pixmap, indX - 100, y, 0.0, FALSE, 0);
	y += tHeight+2;
	if (individualNavObject != 1000) {
	  renderPangoText("Dec", OR_CREAM, MEDIUM_PANGO_FONT, &tWidth, &tHeight,
			  pixmap, indX - 100, y, 0.0, FALSE, 0);
	  y += tHeight+2;
	  renderPangoText("Hc", OR_CREAM, MEDIUM_PANGO_FONT, &tWidth, &tHeight,
			  pixmap, indX - 100, y, 0.0, FALSE, 0);
	  y += tHeight+2;
	  renderPangoText("Zn", OR_CREAM, MEDIUM_PANGO_FONT, &tWidth, &tHeight,
			  pixmap, indX - 100, y, 0.0, FALSE, 0);
	  y += tHeight+22;
	  renderPangoText("Altitude Corrections", OR_BLUE, MEDIUM_PANGO_FONT, &tWidth, &tHeight,
			  pixmap, displayWidth/2, y, 0.0, TRUE, 0);
	  y += tHeight+12;
	  renderPangoText("Refr", OR_CREAM, MEDIUM_PANGO_FONT, &tWidth, &tHeight,
			  pixmap, indX - 100, y, 0.0, FALSE, 0);
	  y += tHeight+2;
	  renderPangoText("SD", OR_CREAM, MEDIUM_PANGO_FONT, &tWidth, &tHeight,
			  pixmap, indX - 100, y, 0.0, FALSE, 0);
	  y += tHeight+2;
	  renderPangoText("PA", OR_CREAM, MEDIUM_PANGO_FONT, &tWidth, &tHeight,
			  pixmap, indX - 100, y, 0.0, FALSE, 0);
	  y += tHeight+2;
	  renderPangoText("Sum", OR_CREAM, MEDIUM_PANGO_FONT, &tWidth, &tHeight,
			  pixmap, indX - 100, y, 0.0, FALSE, 0);
	  y += tHeight+2;
	}
      }
      y += 20;
     for (navStar = 0; navStar < N_NAV_PLANETS; navStar++) {
       if (!displayIndividualNavObject || (-(individualNavObject+1) == navStar)) {
	 planet = navPlanets[navStar];
	 if (planet == MOON)
	   moonPosition(tJD, &rA, &dec, &dummy2, &dummy3, &tR, &dummyFloat);
	 else
	   vSOPPlanetInfo(dataDir, tJD + deltaT(tJD)/86400.0, vSOP87Mapping[navStar], &rA, &dec, &tR);
	 azZA(rA, sin(dec), cos(dec), &az, &zA, FALSE);
	 el = (M_HALF_PI - zA)/DEGREES_TO_RADIANS;
	 if (el > 1.0) {
	   if (planet == MOON) {
	     tR *= 1.0e-6/AU;
	     moonVisible = TRUE;
	   }
	   pi = 0.146566666/tR;
	   parallax = pi*cos(el*DEGREES_TO_RADIANS);
	   if (navStar != 0) {
	     if (!displayIndividualNavObject)
	       gdk_draw_string(pixmap, smallFont, gC[OR_RED], 5, y, solarSystemNames[planet]);
	     else
	       renderPangoText(solarSystemNames[planet], OR_WHITE, BIG_PANGO_FONT, &tWidth, &tHeight,
			       pixmap, displayWidth/2, nameY, 0.0, TRUE, 0);
	     sD = 60.0*(planetRadii[planet]/(tR*AU))/DEGREES_TO_RADIANS;
	   } else {
	     if (!displayIndividualNavObject)
	       gdk_draw_string(pixmap, smallFont, gC[OR_RED], 5, y, "Sun");
	     else
	       renderPangoText("Sun", OR_WHITE, BIG_PANGO_FONT, &tWidth, &tHeight,
			       pixmap, displayWidth/2, nameY, 0.0, TRUE, 0);
	     sD = 60.0*(planetRadii[0]/(tR*AU))/DEGREES_TO_RADIANS;
	   }
	   if (!displayIndividualNavObject)
	     addSensitiveArea(FALSE, SA_NAVIGATION_OBJECT, 0, y-13, displayWidth, y+5, -(float)(navStar+1));
	   hA = (lST() - rA - longitude) / DEGREES_TO_RADIANS;
	   doubleNormalize0to360(&hA);
	   hAD = (int)hA;
	   hAM = 60.0*(hA - (double)hAD);
	   hAD = abs(hAD);
	   hAM = fabs(hAM);
	   dDec = dec / DEGREES_TO_RADIANS;
	   dD  = (int)dDec;
	   dM  = fabs(60.0*(dDec - (double)dD));
	   eD  = (int)el;
	   eM  = 60.0*(el - (double)eD);
	   aD = az / DEGREES_TO_RADIANS;
	   ref = -60.0*refraction(M_HALF_PI-zA)/DEGREES_TO_RADIANS;
	   if (!displayIndividualNavObject) {
	     if (dec > 0.0)
	       sprintf(scratchString,
		       "%3d %04.1f  N%02d %04.1f  %02d %04.1f  %5.1f    %5.1f  %5.1f  %5.1f  %5.1f",
		       hAD, hAM, dD, dM, eD, eM, aD,
		       ref, sD, parallax, ref+sD+parallax);
	     else
	       sprintf(scratchString,
		       "%3d %04.1f  S%02d %04.1f  %02d %04.1f  %5.1f    %5.1f  %5.1f  %5.1f  %5.1f",
		       hAD, hAM, abs(dD), dM, eD, eM, aD,
		       ref, sD, parallax, ref+sD+parallax);
	     gdk_draw_string(pixmap, smallFont, gC[OR_RED], 85, y, scratchString);
	     y += 17;
	   } else {
	     sprintf(scratchString, "%3d", hAD);
	     renderPangoText(scratchString, OR_WHITE, MEDIUM_PANGO_FONT, &tWidth, &tHeight,
			     pixmap, indX, indY, 0.0, FALSE, 0);
	     renderPangoText("degrees", OR_CREAM, MEDIUM_PANGO_FONT, &tWidth, &tHeight,
			     pixmap, indX + 50, indY, 0.0, FALSE, 0);
	     sprintf(scratchString, "%04.1f", hAM);
	     renderPangoText(scratchString, OR_WHITE, MEDIUM_PANGO_FONT, &tWidth, &tHeight,
			     pixmap, indX+150, indY, 0.0, FALSE, 0);
	     renderPangoText("minutes", OR_CREAM, MEDIUM_PANGO_FONT, &tWidth, &tHeight,
			     pixmap, indX + 203, indY, 0.0, FALSE, 0);
	     indY += tHeight+2;
	     if (dec > 0.0)
	       sprintf(scratchString, "+%02d", dD);
	     else
	       sprintf(scratchString, "-%02d", abs(dD));
	     renderPangoText(scratchString, OR_WHITE, MEDIUM_PANGO_FONT, &tWidth, &tHeight,
			     pixmap, indX, indY, 0.0, FALSE, 0);
	     renderPangoText("degrees", OR_CREAM, MEDIUM_PANGO_FONT, &tWidth, &tHeight,
			     pixmap, indX + 50, indY, 0.0, FALSE, 0);
	     sprintf(scratchString, "%04.1f", dM);
	     renderPangoText(scratchString, OR_WHITE, MEDIUM_PANGO_FONT, &tWidth, &tHeight,
			     pixmap, indX+150, indY, 0.0, FALSE, 0);
	     renderPangoText("minutes", OR_CREAM, MEDIUM_PANGO_FONT, &tWidth, &tHeight,
			     pixmap, indX + 203, indY, 0.0, FALSE, 0);
	     indY += tHeight+2;
	     sprintf(scratchString, "%2d", eD);
	     renderPangoText(scratchString, OR_WHITE, MEDIUM_PANGO_FONT, &tWidth, &tHeight,
			     pixmap, indX, indY, 0.0, FALSE, 0);
	     renderPangoText("degrees", OR_CREAM, MEDIUM_PANGO_FONT, &tWidth, &tHeight,
			     pixmap, indX + 50, indY, 0.0, FALSE, 0);
	     sprintf(scratchString, "%04.1f", eM);
	     renderPangoText(scratchString, OR_WHITE, MEDIUM_PANGO_FONT, &tWidth, &tHeight,
			     pixmap, indX+150, indY, 0.0, FALSE, 0);
	     renderPangoText("minutes", OR_CREAM, MEDIUM_PANGO_FONT, &tWidth, &tHeight,
			     pixmap, indX + 203, indY, 0.0, FALSE, 0);
	     indY += tHeight+2;
	     sprintf(scratchString, "%5.1f", aD);
	     renderPangoText(scratchString, OR_WHITE, MEDIUM_PANGO_FONT, &tWidth, &tHeight,
			     pixmap, indX-20, indY, 0.0, FALSE, 0);
	     renderPangoText("degrees", OR_CREAM, MEDIUM_PANGO_FONT, &tWidth, &tHeight,
			     pixmap, indX + 50, indY, 0.0, FALSE, 0);
	     indY += 2*tHeight + 34;
	     indX -= 30;
	     sprintf(scratchString, "%5.1f", ref);
	     renderPangoText(scratchString, OR_WHITE, MEDIUM_PANGO_FONT, &tWidth, &tHeight,
			     pixmap, indX, indY, 0.0, FALSE, 0);
	     renderPangoText("minutes", OR_CREAM, MEDIUM_PANGO_FONT, &tWidth, &tHeight,
			     pixmap, indX + 60, indY, 0.0, FALSE, 0);
	     indY += tHeight+2;
	     sprintf(scratchString, "%5.1f", sD);
	     renderPangoText(scratchString, OR_WHITE, MEDIUM_PANGO_FONT, &tWidth, &tHeight,
			     pixmap, indX, indY, 0.0, FALSE, 0);
	     renderPangoText("minutes", OR_CREAM, MEDIUM_PANGO_FONT, &tWidth, &tHeight,
			     pixmap, indX + 60, indY, 0.0, FALSE, 0);
	     indY += tHeight+2;
	     sprintf(scratchString, "%5.1f",parallax);
	     renderPangoText(scratchString, OR_WHITE, MEDIUM_PANGO_FONT, &tWidth, &tHeight,
			     pixmap, indX, indY, 0.0, FALSE, 0);
	     renderPangoText("minutes", OR_CREAM, MEDIUM_PANGO_FONT, &tWidth, &tHeight,
			     pixmap, indX + 60, indY, 0.0, FALSE, 0);
	     indY += tHeight+2;
	     sprintf(scratchString, "%5.1f", ref+sD+parallax);
	     renderPangoText(scratchString, OR_WHITE, MEDIUM_PANGO_FONT, &tWidth, &tHeight,
			     pixmap, indX, indY, 0.0, FALSE, 0);
	     renderPangoText("minutes", OR_CREAM, MEDIUM_PANGO_FONT, &tWidth, &tHeight,
			     pixmap, indX + 60, indY, 0.0, FALSE, 0);
	     indY += tHeight+2;
	   }
	 }
       }
     }
      star = starNameEntryRoot;
      while (star != NULL) {
	for (navStar = 0; navStar < N_NAV_STARS; navStar++) {
	  if (star->hip->hipNumber == navStarHipNumber[navStar])
	    starEntry[navStar] = star;
	}
	star = star->next;
      }
      for (navStar = 0; navStar < N_NAV_STARS; navStar++) {
	star = starEntry[navStar];
	azZA(star->hip->rAJ2000, sin(star->hip->decJ2000), cos(star->hip->decJ2000), &az, &zA, FALSE);
	el = (M_HALF_PI - zA)/DEGREES_TO_RADIANS;
	if (el > 1.0)
	  visibleStars[nVisibleStars++] = star;
      }
      qsort(visibleStars, nVisibleStars, sizeof(star), compareStarNames);
      for (navStar = 0; navStar < nVisibleStars; navStar++) {
	int nameLen, i;
	float muRA, muDec;
	double appRA, appDec;
	
	if (!displayIndividualNavObject || (individualNavObject == navStar)) {
	  star = visibleStars[navStar];
	  rA = star->hip->rAJ2000; dec = star->hip->decJ2000;
	  muRA  = (star->hip->muRA)/1000.0;
	  muDec = (star->hip->muDec)/1000.0;
	  calculateApparentPosition(tJD, rA, dec, 0.0, muRA, muDec, &appRA, &appDec);
	  azZA(appRA, sin(appDec), cos(appDec), &az, &zA, FALSE);
	  el = (M_HALF_PI - zA)/DEGREES_TO_RADIANS;
	  nameLen = star->nameLen;
	  if (nameLen > 12)
	    nameLen = 12;
	  for (i = 0; i < nameLen; i++)
	    scratchString[i] = starNameString[(star->offset) + i];
	  scratchString[i] = '\0';
	  if (!displayIndividualNavObject) {
	    if (navStar == nVisibleStars-1)
	      color = OR_WHITE;
	    else if ((el > 15.0) && (el < 65.0))
	      color = OR_GREEN;
	    else
	      color = OR_GREY;
	    gdk_draw_string(pixmap, smallFont, gC[color], 5, y, scratchString);
	  } else
	    renderPangoText(scratchString, OR_WHITE, BIG_PANGO_FONT, &tWidth, &tHeight,
			    pixmap, displayWidth/2, nameY, 0.0, TRUE, 0);
	  if (!displayIndividualNavObject)
	    addSensitiveArea(FALSE, SA_NAVIGATION_OBJECT, 0, y-13, displayWidth, y+5, (float)navStar);
	  hA = (lST() - appRA - longitude) / DEGREES_TO_RADIANS;
	  doubleNormalize0to360(&hA);
	  hAD = (int)hA;
	  hAM = 60.0*(hA - (double)hAD);
	  hAD = abs(hAD);
	  hAM = fabs(hAM);
	  dDec = appDec / DEGREES_TO_RADIANS;
	  dD  = (int)dDec;
	  dM  = fabs(60.0*(dDec - (double)dD));
	  eD  = (int)el;
	  eM  = 60.0*(el - (double)eD);
	  aD = az / DEGREES_TO_RADIANS;
	  ref = -60.0*refraction(M_HALF_PI-zA)/DEGREES_TO_RADIANS;
	  if (!displayIndividualNavObject) {
	    if (dec > 0.0)
	      sprintf(scratchString,
		      "%3d %04.1f  N%02d %04.1f  %02d %04.1f  %5.1f    %5.1f  %5.1f  %5.1f  %5.1f",
		      hAD, hAM, dD, dM, eD, eM, aD,
		      ref, 0.0, 0.0, ref);
	    else
	      sprintf(scratchString,
		      "%3d %04.1f  S%02d %04.1f  %02d %04.1f  %5.1f    %5.1f  %5.1f  %5.1f  %5.1f",
		      hAD, hAM, abs(dD), dM, eD, eM, aD,
		      ref, 0.0, 0.0, ref);
	    gdk_draw_string(pixmap, smallFont, gC[color], 85, y, scratchString);
	    y += 17;
	  } else {
	    sprintf(scratchString, "%3d", hAD);
	    renderPangoText(scratchString, OR_WHITE, MEDIUM_PANGO_FONT, &tWidth, &tHeight,
			    pixmap, indX, indY, 0.0, FALSE, 0);
	    renderPangoText("degrees", OR_CREAM, MEDIUM_PANGO_FONT, &tWidth, &tHeight,
			    pixmap, indX + 50, indY, 0.0, FALSE, 0);
	    sprintf(scratchString, "%04.1f", hAM);
	    renderPangoText(scratchString, OR_WHITE, MEDIUM_PANGO_FONT, &tWidth, &tHeight,
			    pixmap, indX+150, indY, 0.0, FALSE, 0);
	    renderPangoText("minutes", OR_CREAM, MEDIUM_PANGO_FONT, &tWidth, &tHeight,
			    pixmap, indX + 203, indY, 0.0, FALSE, 0);
	    indY += tHeight+2;
	    if (dec > 0.0)
	      sprintf(scratchString, "+%02d", dD);
	    else
	      sprintf(scratchString, "-%02d", abs(dD));
	    renderPangoText(scratchString, OR_WHITE, MEDIUM_PANGO_FONT, &tWidth, &tHeight,
			    pixmap, indX, indY, 0.0, FALSE, 0);
	    renderPangoText("degrees", OR_CREAM, MEDIUM_PANGO_FONT, &tWidth, &tHeight,
			    pixmap, indX + 50, indY, 0.0, FALSE, 0);
	    sprintf(scratchString, "%04.1f", dM);
	    renderPangoText(scratchString, OR_WHITE, MEDIUM_PANGO_FONT, &tWidth, &tHeight,
			    pixmap, indX+150, indY, 0.0, FALSE, 0);
	    renderPangoText("minutes", OR_CREAM, MEDIUM_PANGO_FONT, &tWidth, &tHeight,
			    pixmap, indX + 203, indY, 0.0, FALSE, 0);
	    indY += tHeight+2;
	    sprintf(scratchString, "%2d", eD);
	    renderPangoText(scratchString, OR_WHITE, MEDIUM_PANGO_FONT, &tWidth, &tHeight,
			    pixmap, indX, indY, 0.0, FALSE, 0);
	    renderPangoText("degrees", OR_CREAM, MEDIUM_PANGO_FONT, &tWidth, &tHeight,
			    pixmap, indX + 50, indY, 0.0, FALSE, 0);
	    sprintf(scratchString, "%04.1f", eM);
	    renderPangoText(scratchString, OR_WHITE, MEDIUM_PANGO_FONT, &tWidth, &tHeight,
			    pixmap, indX+150, indY, 0.0, FALSE, 0);
	    renderPangoText("minutes", OR_CREAM, MEDIUM_PANGO_FONT, &tWidth, &tHeight,
			    pixmap, indX + 203, indY, 0.0, FALSE, 0);
	    indY += tHeight+2;
	    sprintf(scratchString, "%5.1f", aD);
	    renderPangoText(scratchString, OR_WHITE, MEDIUM_PANGO_FONT, &tWidth, &tHeight,
			    pixmap, indX-20, indY, 0.0, FALSE, 0);
	    renderPangoText("degrees", OR_CREAM, MEDIUM_PANGO_FONT, &tWidth, &tHeight,
			    pixmap, indX + 50, indY, 0.0, FALSE, 0);
	    indY += 2*tHeight + 34;
	    indX -= 30;
	    sprintf(scratchString, "%5.1f", ref);
	    renderPangoText(scratchString, OR_WHITE, MEDIUM_PANGO_FONT, &tWidth, &tHeight,
			    pixmap, indX, indY, 0.0, FALSE, 0);
	    renderPangoText("minutes", OR_CREAM, MEDIUM_PANGO_FONT, &tWidth, &tHeight,
			    pixmap, indX + 60, indY, 0.0, FALSE, 0);
	    indY += tHeight+2;
	    sprintf(scratchString, "%5.1f", sD);
	    renderPangoText(scratchString, OR_WHITE, MEDIUM_PANGO_FONT, &tWidth, &tHeight,
			    pixmap, indX, indY, 0.0, FALSE, 0);
	    renderPangoText("minutes", OR_CREAM, MEDIUM_PANGO_FONT, &tWidth, &tHeight,
			    pixmap, indX + 60, indY, 0.0, FALSE, 0);
	    indY += tHeight+2;
	    sprintf(scratchString, "%5.1f",parallax);
	    renderPangoText(scratchString, OR_WHITE, MEDIUM_PANGO_FONT, &tWidth, &tHeight,
			    pixmap, indX, indY, 0.0, FALSE, 0);
	    renderPangoText("minutes", OR_CREAM, MEDIUM_PANGO_FONT, &tWidth, &tHeight,
			    pixmap, indX + 60, indY, 0.0, FALSE, 0);
	    indY += tHeight+2;
	    sprintf(scratchString, "%5.1f", ref+sD+parallax);
	    renderPangoText(scratchString, OR_WHITE, MEDIUM_PANGO_FONT, &tWidth, &tHeight,
			    pixmap, indX, indY, 0.0, FALSE, 0);
	    renderPangoText("minutes", OR_CREAM, MEDIUM_PANGO_FONT, &tWidth, &tHeight,
			    pixmap, indX + 60, indY, 0.0, FALSE, 0);
	    indY += tHeight+2;
	  }
	}
      }
      if (!displayIndividualNavObject || (individualNavObject == 1000)) {
	hA = (lST() - longitude) / DEGREES_TO_RADIANS;
	doubleNormalize0to360(&hA);
	hAD = (int)hA;
	hAM = 60.0*(hA - (double)hAD);
	hAD = abs(hAD);
	hAM = fabs(hAM);
	if (!displayIndividualNavObject) {
	  gdk_draw_string(pixmap, smallFont, gC[color], 5, y, "Aries");
	  addSensitiveArea(FALSE, SA_NAVIGATION_OBJECT, 0, y-12, displayWidth, y+4, 1000.0);
	  sprintf(scratchString, "%3d %04.1f", hAD, hAM);
	  gdk_draw_string(pixmap, smallFont, gC[color], 85, y, scratchString);
	} else {
	  renderPangoText("(first point in) Aries", OR_WHITE, BIG_PANGO_FONT, &tWidth, &tHeight,
			       pixmap, displayWidth/2, nameY, 0.0, TRUE, 0);
	  sprintf(scratchString, "%3d", hAD);
	  renderPangoText(scratchString, OR_WHITE, MEDIUM_PANGO_FONT, &tWidth, &tHeight,
			  pixmap, indX, indY, 0.0, FALSE, 0);
	  renderPangoText("degrees", OR_CREAM, MEDIUM_PANGO_FONT, &tWidth, &tHeight,
			  pixmap, indX + 50, indY, 0.0, FALSE, 0);
	  sprintf(scratchString, "%04.1f", hAM);
	  renderPangoText(scratchString, OR_WHITE, MEDIUM_PANGO_FONT, &tWidth, &tHeight,
			  pixmap, indX+150, indY, 0.0, FALSE, 0);
	  renderPangoText("minutes", OR_CREAM, MEDIUM_PANGO_FONT, &tWidth, &tHeight,
			  pixmap, indX + 203, indY, 0.0, FALSE, 0);
	}
      }
      if (!displayIndividualNavObject)
	gdk_draw_line(pixmap, gC[OR_WHITE], displayWidth/2 + 63, yLineTop,
		      displayWidth/2 + 63, y);
      if (moonVisible && (!displayIndividualNavObject)) {
	y += 30;
	planetInfo(dataDir, EARTH, tJD,      &dummy, &dummy, &illum,  &mag);
	planetInfo(dataDir, MOON,  tJD,      &dummy, &dummy, &illum,  &mag);
	planetInfo(dataDir, EARTH, tJD     , &dummy, &dummy, &illum2, &mag);
	planetInfo(dataDir, MOON,  tJD+0.04, &dummy, &dummy, &illum2, &mag);
	if (fabs(illum) < 0.02)
	  sprintf(scratchString, "new moon");
	else if (fabs(illum) > 0.98)
	  sprintf(scratchString, "full moon");
	else {
	  if (northernHemisphere) {
	    if (illum2 < illum)
	      sunAngle = M_PI;
	  } else {
	    if (illum2 > illum)
	      sunAngle = M_PI;
	  }
	  if (fabs(illum - 0.5) < 0.03) {
	    if (illum2 > illum)
	      sprintf(scratchString, "first quarter");
	    else
	      sprintf(scratchString, "last quarter");
	  } else if (illum > 0.5) {
	    if (illum2 > illum)
	      sprintf(scratchString, "waxing gibbous");
	    else
	      sprintf(scratchString, "waining gibbous");
	  } else {
	    if (illum2 > illum)
	      sprintf(scratchString, "waxing crescent");
	    else
	      sprintf(scratchString, "waining crescent");
	  }
	}
	sprintf(phaseString, "The moon phase is %s, %2.0f%% illuminated.", scratchString, illum*100.0);
	gdk_draw_string(pixmap, smallFont, gC[OR_CREAM], 85, y, phaseString);
      }
      if (displayIndividualNavObject) {
	y = displayHeight*7/9 + 82;
	renderPangoText("Display Full Object List", OR_GREEN, MEDIUM_PANGO_FONT, &tWidth, &tHeight,
			pixmap, displayWidth/2, y, 0.0, TRUE, 0);
	tWidth += 20;
	tHeight += 10;
	gdk_draw_rectangle(pixmap, gC[OR_GREEN], FALSE, (displayWidth-tWidth)/2, y - tHeight/2,
			   tWidth, tHeight);
	addSensitiveArea(FALSE, SA_DISPLAY_NAV_LIST, (displayWidth-tWidth)/2,  y - tHeight/2,
			 (displayWidth+tWidth)/2,  y + tHeight/2, 0.0);
      }
      y = displayHeight*8/9 + 62;
      renderPangoText("Press Here to Update", OR_GREEN, MEDIUM_PANGO_FONT, &tWidth, &tHeight,
		      pixmap, displayWidth/2, y, 0.0, TRUE, 0);
      tWidth += 20;
      tHeight += 10;
      gdk_draw_rectangle(pixmap, gC[OR_GREEN], FALSE, (displayWidth-tWidth)/2, y - tHeight/2,
		       tWidth, tHeight);
      addSensitiveArea(FALSE, SA_UPDATE_NAVIGATION, (displayWidth-tWidth)/2,  y - tHeight/2,
		       (displayWidth+tWidth)/2,  y + tHeight/2, 0.0);
    }
    break;
#define JOV_EVENT_ROW_INDENT (20)
#define IO       (         1)
#define EUROPA   (IO      +1)
#define GANYMEDE (EUROPA  +1)
#define CALLISTO (GANYMEDE+1)
#define N_GAL    (CALLISTO+1)
  case JOVIAN_MOONS:
    {
      static int haveJupiterImage = FALSE;
      int year, month, day, iDay, tWidth, tHeight, y, monthDisplayHeight, maxDay,
	nStepsPerDay, plotWidth, plotCenter, plottedThem, i, j,  galColor[N_GAL],
	xDiskLeft, xDiskRight, lastGal[N_GAL], lastPY, w, h, iGal;
      double lastGalX[N_GAL], lastGalAngle[N_GAL], sunELong, sELat,
	pJD, sJD, eJD, timeStep, rMax, scale,
	aJupiter, bigScale, galX[N_GAL], galY[N_GAL], galAngle[N_GAL], galZ[N_GAL],
	xSign, ySign, tJX, tJY, tJZ, tEX, tEY, tEZ, tR, tLong, tLat, galSR[N_GAL],
	galShadowX[N_GAL], galShadowY[N_GAL], galShadowZ[N_GAL], galR[N_GAL];
      char fileName[MAX_FILE_NAME_SIZE];
      GdkPoint littleCircle[101];
      static GdkPixmap *jupiterImage;

      if (!haveJupiterImage) {
	sprintf(fileName, "%s/icons/%s.xpm", dataDir, "jupiterTiny");
	jupiterImage = gdk_pixmap_create_from_xpm(pixmap, NULL, NULL, fileName);
	haveJupiterImage = TRUE;
      }
      if (jovianMoonsSE || jovianMoonsSW)
	ySign = 1.0;
      else
	ySign = -1.0;
      if (jovianMoonsNW || jovianMoonsSW)
	xSign = 1.0;
      else
	xSign = -1.0;

      plottedThem = FALSE; /* This variable ensures that the satellites are shown only once on the wavey lines plot */
      lastPY = 0;
      galColor[IO] = OR_PINK; galColor[EUROPA] = OR_GREEN;
      galColor[GANYMEDE] = OR_LIGHT_BLUE; galColor[CALLISTO] = OR_YELLOW;
      y = 11;
      nStepsPerDay = 35; /* Number of points/pixels per day in wavey lines display */
      timeStep = 1.0/(double)nStepsPerDay;
      monthDisplayHeight = 4*displayHeight/5;
      plotWidth = displayWidth/2 - 40;
      rMax = 26.5566; /* Maximum distance Callisto can be away from Jupiter, in Jupiter radius units */
      scale = plotWidth/(2.0*rMax);
      bigScale = (displayWidth - 4)/(2.0*rMax);
      plotCenter = displayWidth/4 + 5;
      needNewTime = TRUE;
      lSTNow = lST();
      tJDToDate(tJD, &year, &month, &day);
      sprintf(scratchString, "Jovian Moons for %s, %d", monthName[month-1], year);
      renderPangoText(scratchString, OR_CREAM, MEDIUM_PANGO_FONT, &tWidth, &tHeight,
		      pixmap, displayWidth/2, y, 0.0, TRUE, 0);
      y += tHeight;
      /* List the names of the satellites in the colors used to plot them */
      renderPangoText("Io", galColor[IO], MEDIUM_PANGO_FONT, &tWidth, &tHeight,
		      pixmap, displayWidth/8, y, 0.0, TRUE, 0);
      renderPangoText("Europa", galColor[EUROPA], MEDIUM_PANGO_FONT, &tWidth, &tHeight,
		      pixmap, 3*displayWidth/8, y, 0.0, TRUE, 0);
      renderPangoText("Ganymede", galColor[GANYMEDE], MEDIUM_PANGO_FONT, &tWidth, &tHeight,
		      pixmap, 5*displayWidth/8, y, 0.0, TRUE, 0);
      renderPangoText("Callisto", galColor[CALLISTO], MEDIUM_PANGO_FONT, &tWidth, &tHeight,
		      pixmap, 7*displayWidth/8, y, 0.0, TRUE, 0);

      y += tHeight;
      gdk_drawable_get_size(jupiterImage, &w, &h);
      gdk_draw_drawable(pixmap, gC[OR_BLUE], jupiterImage, 0, 0, (displayWidth - w)/2,
			y + 15 - h/2, w, h);
      calculatePlanetPosition(EARTH,   tJD, &tEX, &tEY, &tEZ);
      calculatePlanetPosition(JUPITER, tJD, &tJX, &tJY, &tJZ);
      sunELong = atan2(tJY, tJX)/DEGREES_TO_RADIANS;
      doubleNormalize0to360(&sunELong);
      sELat  = atan(tJZ/sqrt(tJX*tJX + tJY*tJY))/DEGREES_TO_RADIANS;
      tJX -= tEX; tJY -= tEY; tJZ -= tEZ;
      tR = sqrt(tJX*tJX + tJY*tJY + tJZ*tJZ);
      aJupiter = 3600.0*(planetRadii[JUPITER]/(tR*AU))/DEGREES_TO_RADIANS;
      tLat  = atan(tJZ/sqrt(tJX*tJX + tJY*tJY))/DEGREES_TO_RADIANS;
      tLong = atan2(tJY, tJX)/DEGREES_TO_RADIANS;
      doubleNormalize0to360(&tLong);
      highAccuracyJovSats(tJD + deltaT(tJD)/86400.0, tR, tLong,    tLat,
			  galX,             galY,       galZ);
      highAccuracyJovSats(tJD + deltaT(tJD)/86400.0, tR, sunELong, sELat,
			  galShadowX, galShadowY, galShadowZ);
      for (iGal = IO; iGal <= CALLISTO; iGal++) {
	/*
	  In the following radial distance calculations, the 0.8744599 factor
	  is the square of Jupiters polar/equatorial radius.   Scaling this
	  way allows us to treat it as circular.
	*/
	galR[iGal]  = sqrt(      galX[iGal]*galX[iGal]       +       galY[iGal]*galY[iGal]/0.8744599);
	galSR[iGal] = sqrt(galShadowX[iGal]*galShadowX[iGal] + galShadowY[iGal]*galShadowY[iGal]/0.8744599);
	if ((galShadowZ[iGal] < 0.0) && (galSR[iGal] < 1.0)) {
	  galShadowX[iGal] = xSign*galShadowX[iGal]*bigScale + displayWidth/2;
	  galShadowY[iGal] = ySign*galShadowY[iGal]*bigScale + y + 15;
	  gdk_draw_arc(pixmap, gC[OR_BLACK], TRUE, galShadowX[iGal]-2, galShadowY[iGal]-2, 5, 5, 0, FULL_CIRCLE); 
	}
	if (((galR[iGal] >= 1.0) || (galZ[iGal] < 0.0)) && (!((galSR[iGal] < 1.0) && (galZ[iGal] > 0.0)))) {
	  galX[iGal] = xSign*galX[iGal]*bigScale + displayWidth/2;
	  galY[iGal] = ySign*galY[iGal]*bigScale + y + 15;
	  gdk_draw_arc(pixmap, gC[galColor[iGal]], TRUE, galX[iGal]-2, galY[iGal]-2, 5, 5, 0, FULL_CIRCLE); 
	}
      }
      y += 60;
      if (jovianEvents) { /* Print a page showing times for satellite eclipses, transits, etc */
	int day, month, year, hour, minute, sunUp, jupUp;
	int oldDay = -1;
	int nPrinted = 0;
	int nShadowsTransiting = 0;
	int eventColor = OR_GREEN;
	int haveShownNext = FALSE;
	int inShadow[N_GAL] = {FALSE, FALSE, FALSE, FALSE, FALSE};
	int occulted[N_GAL] = {FALSE, FALSE, FALSE, FALSE, FALSE};
	int transiting[N_GAL] = {FALSE, FALSE, FALSE, FALSE, FALSE};
	int shadowTransiting[N_GAL] = {FALSE, FALSE, FALSE, FALSE, FALSE};
	float illum;
	double jDD, rA, dec, az, zA, savedTJD;
	double rMoon[N_GAL] = {0.0, 1810.0/71300.0, 1480.0/71300.0, 2600.0/71300.0, 2360.0/71300.0};
	char *moonNames[N_GAL] = {"OhOh", "Io", "Europa", "Ganymede", "Callisto"};

	renderPangoText("Jovian Satellite Events", OR_WHITE, MEDIUM_PANGO_FONT, &tWidth, &tHeight,
			pixmap, displayWidth/2, y, 0.0, TRUE, 0);
	y += tHeight + 5;
	tJDToDate(tJD, &year, &month, &day);
	sJD = buildTJD(year-1900, month-1, day, 0, 0, 0, 0);
	savedTJD = tJD;
	for (pJD = sJD; nPrinted < 21; pJD += 1.0/1440.0) {
	  needNewTime = FALSE;
	  myLST = lSTAtTJD(pJD);	  
	  planetInfo(dataDir, EARTH, pJD, &rA, &dec, &illum, &illum);
	  azZA(rA, sin(dec), cos(dec), &az, &zA, FALSE);
	  if (zA < M_HALF_PI)
	    sunUp = TRUE;
	  else
	    sunUp = FALSE;
	  planetInfo(dataDir, JUPITER, pJD, &rA, &dec, &illum, &illum);
	  azZA(rA, sin(dec), cos(dec), &az, &zA, FALSE);
	  if (zA < M_HALF_PI)
	    jupUp = TRUE;
	  else
	    jupUp = FALSE;
	  if (jupUp && !sunUp)
	    eventColor = OR_GREEN;
	  else
	    eventColor = OR_GREY;
	  tJDToDate(pJD, &year, &month, &day);
	  if (day != oldDay) {
	    sprintf(scratchString, "%s, %s %d, %d",
		    dayName[dayOfWeek(year, month, day)], monthName[month-1], day, year);
	    y += 6;
	    renderPangoText(scratchString, OR_CREAM, SMALL_PANGO_FONT, &tWidth, &tHeight,
			pixmap, 10, y, 0.0, FALSE, 0);
	    y += tHeight+1;
	    nPrinted++;
	    oldDay = day;
	  }
	  calculatePlanetPosition(EARTH,   pJD, &tEX, &tEY, &tEZ);
	  calculatePlanetPosition(JUPITER, pJD, &tJX, &tJY, &tJZ);
	  sunELong = atan2(tJY, tJX)/DEGREES_TO_RADIANS;
	  doubleNormalize0to360(&sunELong);
	  sELat  = atan(tJZ/sqrt(tJX*tJX + tJY*tJY))/DEGREES_TO_RADIANS;
	  tJX -= tEX; tJY -= tEY; tJZ -= tEZ;
	  tR = sqrt(tJX*tJX + tJY*tJY + tJZ*tJZ);
	  tLat  = atan(tJZ/sqrt(tJX*tJX + tJY*tJY))/DEGREES_TO_RADIANS;
	  tLong = atan2(tJY, tJX)/DEGREES_TO_RADIANS;
	  doubleNormalize0to360(&tLong);
	  highAccuracyJovSats(pJD + deltaT(pJD)/86400.0, tR, tLong,    tLat,
			      galX,             galY,       galZ);
	  highAccuracyJovSats(pJD + deltaT(pJD)/86400.0, tR, sunELong, sELat,
			      galShadowX, galShadowY, galShadowZ);
	  jDD = pJD - 0.5;
	  jDD = jDD - (double)((int)jDD);
	  jDD *= 24.0;
	  hour = (int)jDD;
	  minute = (int)(((jDD - (double)hour)*60.0));
	  for (iGal = IO; iGal <= CALLISTO; iGal++) {
	    galR[iGal]  = sqrt(      galX[iGal]*galX[iGal]       +       galY[iGal]*galY[iGal]/0.8744599);
	    galSR[iGal] = sqrt(galShadowX[iGal]*galShadowX[iGal] + galShadowY[iGal]*galShadowY[iGal]/0.8744599);

	    /* List moon transits */
	    if ((galR[iGal] < (1.0+rMoon[iGal])) && (galZ[iGal] < 0.0)) {
	      if (!transiting[iGal]) {
		transiting[iGal] = TRUE;
		if (pJD != sJD) {
		  if ((!haveShownNext) && (pJD >= savedTJD) && (eventColor == OR_GREEN)) {
		    eventColor = OR_RED;
		    haveShownNext = TRUE;
		  }
		  sprintf(scratchString, "%02d:%02d UT, %s begins transit of Jupiter",
			  hour, minute, moonNames[iGal]);
		  renderPangoText(scratchString, eventColor, SMALL_PANGO_FONT, &tWidth, &tHeight,
				  pixmap, JOV_EVENT_ROW_INDENT, y, 0.0, FALSE, 0);
		  y += tHeight+1;
		  nPrinted++;
		}
	      }
	    } else if ((galR[iGal] > (1.0-rMoon[iGal])) && (galZ[iGal] < 0.0)) {
	      if (transiting[iGal]) {
		transiting[iGal] = FALSE;
		if (pJD != sJD) {
		  if ((!haveShownNext) && (pJD >= savedTJD) && (eventColor == OR_GREEN)) {
		    eventColor = OR_RED;
		    haveShownNext = TRUE;
		  }
		  sprintf(scratchString, "%02d:%02d UT, %s ends transit of Jupiter", 
			  hour, minute, moonNames[iGal]);
		  renderPangoText(scratchString, eventColor, SMALL_PANGO_FONT, &tWidth, &tHeight,
				  pixmap, JOV_EVENT_ROW_INDENT, y, 0.0, FALSE, 0);
		  y += tHeight+1;
		  nPrinted++;
		}
	      }
	    }

	    /* List moon shadow transits */
	    if ((galSR[iGal] < (1.0+rMoon[iGal])) && (galZ[iGal] < 0.0)) {
	      if (!shadowTransiting[iGal]) {
		shadowTransiting[iGal] = TRUE;
		nShadowsTransiting++;
		if (pJD != sJD) {
		  if ((!haveShownNext) && (pJD >= savedTJD) && (eventColor == OR_GREEN)) {
		    eventColor = OR_RED;
		    haveShownNext = TRUE;
		  }
		  sprintf(scratchString, "%02d:%02d UT, %s's shadow begins to cross Jupiter",
			  hour, minute, moonNames[iGal]);
		  renderPangoText(scratchString, eventColor, SMALL_PANGO_FONT, &tWidth, &tHeight,
				  pixmap, JOV_EVENT_ROW_INDENT, y, 0.0, FALSE, 0);
		  y += tHeight+1;
		  nPrinted++;
		  if (nShadowsTransiting > 1) {
		    sprintf(scratchString, "%02d:%02d UT, *** There are now %d shadows transiting! ***",
			    hour, minute, nShadowsTransiting);
		    renderPangoText(scratchString, eventColor, SMALL_PANGO_FONT, &tWidth, &tHeight,
				    pixmap, JOV_EVENT_ROW_INDENT, y, 0.0, FALSE, 0);
		    y += tHeight+1;
		    nPrinted++;
		  }
		}
	      }
	    } else if ((galSR[iGal] > (1.0-rMoon[iGal])) && (galZ[iGal] < 0.0)) {
	      if (shadowTransiting[iGal]) {
		shadowTransiting[iGal] = FALSE;
		nShadowsTransiting--;
		if (pJD != sJD) {
		  if ((!haveShownNext) && (pJD >= savedTJD) && (eventColor == OR_GREEN)) {
		    eventColor = OR_RED;
		    haveShownNext = TRUE;
		  }
		  sprintf(scratchString, "%02d:%02d UT, %s's shadow leaves Jupiter's disk ",
			  hour, minute, moonNames[iGal]);
		  renderPangoText(scratchString, eventColor, SMALL_PANGO_FONT, &tWidth, &tHeight,
				  pixmap, JOV_EVENT_ROW_INDENT, y, 0.0, FALSE, 0);
		  y += tHeight+1;
		  nPrinted++;
		  if (nShadowsTransiting > 0) {
		    if (nShadowsTransiting == 1)
		      sprintf(scratchString, "%02d:%02d UT, Now only one shadow is still transiting",
			      hour, minute);
		    else
		      sprintf(scratchString, "%02d:%02d UT, Now only %d shadows are still transiting",
			      hour, minute, nShadowsTransiting);
		    renderPangoText(scratchString, eventColor, SMALL_PANGO_FONT, &tWidth, &tHeight,
				    pixmap, JOV_EVENT_ROW_INDENT, y, 0.0, FALSE, 0);
		    y += tHeight+1;
		    nPrinted++;
		  }
		}
	      }
	    }

	    /* List moon occultations */
	    if ((galR[iGal] < (1.0-rMoon[iGal])) && (galZ[iGal] > 0.0)) {
	      if (!occulted[iGal]) {
		occulted[iGal] = TRUE;
		if (!inShadow[iGal] && (pJD != sJD)) {
		  if ((!haveShownNext) && (pJD >= savedTJD) && (eventColor == OR_GREEN)) {
		    eventColor = OR_RED;
		    haveShownNext = TRUE;
		  }
		  sprintf(scratchString, "%02d:%02d UT, %s occulted by Jupiter",
			  hour, minute, moonNames[iGal]);
		  renderPangoText(scratchString, eventColor, SMALL_PANGO_FONT, &tWidth, &tHeight,
				  pixmap, JOV_EVENT_ROW_INDENT, y, 0.0, FALSE, 0);
		  y += tHeight+1;
		  nPrinted++;
		}
	      }
	    } else if ((galR[iGal] > (1.0+rMoon[iGal])) && (galZ[iGal] > 0.0)) {
	      if (occulted[iGal]) {
		occulted[iGal] = FALSE;
		if (!inShadow[iGal] && (pJD != sJD)) {
		  if ((!haveShownNext) && (pJD >= savedTJD) && (eventColor == OR_GREEN)) {
		    eventColor = OR_RED;
		    haveShownNext = TRUE;
		  }
		  sprintf(scratchString, "%02d:%02d UT, %s exits occultation by Jupiter",
			  hour, minute, moonNames[iGal]);
		  renderPangoText(scratchString, eventColor, SMALL_PANGO_FONT, &tWidth, &tHeight,
				  pixmap, JOV_EVENT_ROW_INDENT, y, 0.0, FALSE, 0);
		  y += tHeight+1;
		  nPrinted++;
		}
	      }
	    }

	    /* List moon eclipses */
	    if ((galSR[iGal] < (1.0-rMoon[iGal])) && (galZ[iGal] > 0.0)) {
	      if (!inShadow[iGal]) {
		inShadow[iGal] = TRUE;
		if (!occulted[iGal] && (pJD != sJD)) {
		  if ((!haveShownNext) && (pJD >= savedTJD) && (eventColor == OR_GREEN)) {
		    eventColor = OR_RED;
		    haveShownNext = TRUE;
		  }
		  sprintf(scratchString, "%02d:%02d UT, %s eclipsed by Jupiter's shadow",
			  hour, minute, moonNames[iGal]);
		  renderPangoText(scratchString, eventColor, SMALL_PANGO_FONT, &tWidth, &tHeight,
				  pixmap, JOV_EVENT_ROW_INDENT, y, 0.0, FALSE, 0);
		  y += tHeight+1;
		  nPrinted++;
		}
	      }
	    } else if ((galSR[iGal] > (1.0+rMoon[iGal])) && (galZ[iGal] > 0.0)) {
	      if (inShadow[iGal]) {
		inShadow[iGal] = FALSE;
		if (!occulted[iGal] && (pJD != sJD)) {
		  if ((!haveShownNext) && (pJD >= savedTJD) && (eventColor == OR_GREEN)) {
		    eventColor = OR_RED;
		    haveShownNext = TRUE;
		  }
		  sprintf(scratchString, "%02d:%02d UT, %s emerges from eclipse",
			  hour, minute, moonNames[iGal]);
		  renderPangoText(scratchString, eventColor, SMALL_PANGO_FONT, &tWidth, &tHeight,
				  pixmap, JOV_EVENT_ROW_INDENT, y, 0.0, FALSE, 0);
		  y += tHeight+1;
		  nPrinted++;
		}
	      }
	    }
	  }
	  /*
	    If all the moons are more than 1.5 Jov. radii from both the planet and its
	    shadow, increment the time by 5 minutes, to cut down on unneeded position
	    calculations.
	  */
	  if ((galR[1] > 1.5) && (galSR[1] > 1.5) && (galR[2] > 1.5) && (galSR[2] > 1.5) &&
	      (galR[3] > 1.5) && (galSR[3] > 1.5) && (galR[4] > 1.5) && (galSR[4] > 1.5))
	    pJD += 5.0/1440.0;
	}
	renderPangoText("Click for Graph Page", OR_GREEN, SMALL_PANGO_FONT, &tWidth, &tHeight,
			pixmap, displayWidth/2, displayHeight-30, 0.0, TRUE, 0);
	addSensitiveArea(FALSE, SA_JOVIAN, 0, displayHeight-45,
			 displayWidth, displayHeight, 101.0);
      } else {
	/*
	  Draw "wavy line" plot of the Galilean Moon positions for the current
	  month.   This plot, is like the one shown in the "Satellites of Jupiter"
	  section of the Astronomical Almanac, but the satellite paths are shown in
	  colors, and the current configuration is shown with colored dots.
	*/
	xDiskLeft  = plotCenter-(int)(scale+0.5); /* Left edge of the planet disk */
	xDiskRight = plotCenter+(int)(scale+0.5); /* Right edge of the planet disk */
	maxDay = monthLengths[month-1];
	if ((month == 2) && ((year % 4) == 0))
	  maxDay++; /* Correct for leap year */
	/* Loop over days in the month, printing day number and grey line at 0h */
	for (iDay = 1; iDay <= maxDay; iDay++) {
	  int pY;
	  
	  pY = y + ((iDay-1) % 16)*nStepsPerDay;
	  sprintf(scratchString, "%2d", iDay);
	  renderPangoText(scratchString, OR_WHITE, SMALL_PANGO_FONT, &tWidth, &tHeight,
			  pixmap, 10+(((iDay-1)/16)*displayWidth/2), pY, 0.0, TRUE, 0);
	  gdk_draw_line(pixmap, gC[OR_DARK_GREY],
			(((iDay-1)/16)*displayWidth/2) + plotCenter-plotWidth/2,
			pY,(((iDay-1)/16)*displayWidth/2) + xDiskLeft, pY);
	  gdk_draw_line(pixmap, gC[OR_DARK_GREY],
			(((iDay-1)/16)*displayWidth/2) + xDiskRight,
			pY,(((iDay-1)/16)*displayWidth/2) + plotCenter+plotWidth/2, pY);
	}
	/* Draw a grey line after the last day at 24h, to show the plot end point */
	gdk_draw_line(pixmap, gC[OR_DARK_GREY],
		      (((iDay-1)/16)*displayWidth/2) + plotCenter-plotWidth/2,
		      y + ((iDay-1) % 16)*nStepsPerDay,
		      (((iDay-1)/16)*displayWidth/2) + xDiskLeft,
		      y + ((iDay-1) % 16)*nStepsPerDay);
	gdk_draw_line(pixmap, gC[OR_DARK_GREY],
		      (((iDay-1)/16)*displayWidth/2) + xDiskRight,
		      y + ((iDay-1) % 16)*nStepsPerDay,
		      (((iDay-1)/16)*displayWidth/2) + plotCenter+plotWidth/2,
		      y + ((iDay-1) % 16)*nStepsPerDay);
	renderPangoText("17", OR_WHITE, SMALL_PANGO_FONT, &tWidth, &tHeight,
			pixmap, 10, y + 16*nStepsPerDay, 0.0, TRUE, 0);
	gdk_draw_line(pixmap, gC[OR_DARK_GREY], plotCenter-plotWidth/2, y + 16*nStepsPerDay,
		      xDiskLeft, y + 16*nStepsPerDay);
	gdk_draw_line(pixmap, gC[OR_DARK_GREY], xDiskRight, y + 16*nStepsPerDay,
		      plotCenter+plotWidth/2, y + 16*nStepsPerDay);
	for (i = -750; i <= 750; i += 250) {
	  int x, width;
	  
	  x = plotCenter + (int)(((double)i)*scale/aJupiter + 0.5);
	  if (i != 0) {
	    if (abs(plotCenter-x) <= plotWidth/2) {
	      gdk_draw_line(pixmap, gC[OR_GREY], x, y, x, y - 8);
	      gdk_draw_line(pixmap, gC[OR_GREY], x, y + 16*nStepsPerDay,
			    x, y + 16*nStepsPerDay + 8);
	      gdk_draw_line(pixmap, gC[OR_GREY], x + displayWidth/2, y,
			    x + displayWidth/2, y - 8);
	      gdk_draw_line(pixmap, gC[OR_GREY], x + displayWidth/2,
			    y+(maxDay-16)*nStepsPerDay,
			    x + displayWidth/2, y+(maxDay-16)*nStepsPerDay + 8);
	      sprintf(scratchString, "%d", i);
	      width = gdk_string_width(smallFont, scratchString)/2;
	      gdk_draw_string(pixmap, smallFont, gC[OR_WHITE], x-width,
			      y + 16*nStepsPerDay + 20, scratchString);
	      gdk_draw_string(pixmap, smallFont, gC[OR_WHITE],
			      displayWidth/2 + x-width,
			      y + (maxDay-16)*nStepsPerDay + 20, scratchString);
	    }
	  } else {
	    sprintf(scratchString, "arcsec");
	    width = gdk_string_width(smallFont, scratchString)/2;
	    gdk_draw_string(pixmap, smallFont, gC[OR_CREAM], x-width,
			    y + 16*nStepsPerDay + 10, scratchString);
	    gdk_draw_string(pixmap, smallFont, gC[OR_CREAM], displayWidth/2 + x - width,
			    y+(maxDay-16)*nStepsPerDay + 10, scratchString);
	    if (jovianMoonsNE || jovianMoonsSE)
	      sprintf(scratchString, "east");
	    else
	      sprintf(scratchString, "west");
	    width = gdk_string_width(smallFont, scratchString)/2;
	    gdk_draw_string(pixmap, smallFont, gC[OR_CREAM], x-width,
			    y + 16*nStepsPerDay + 20, scratchString);
	    gdk_draw_string(pixmap, smallFont, gC[OR_CREAM], displayWidth/2 + x - width,
			    y+(maxDay-16)*nStepsPerDay + 20, scratchString);
	  }
	}
	renderPangoText("Click for Events", OR_GREEN, SMALL_PANGO_FONT, &tWidth, &tHeight,
			pixmap, displayWidth/4, displayHeight-15, 0.0, TRUE, 0);
	addSensitiveArea(FALSE, SA_JOVIAN, 0, displayHeight-30,
			 displayWidth/4 + tWidth/2 + 10, displayHeight, 100.0);
	for (i = 0; i < 4; i++) {
	  int color, width;
	  char topLab[2], rightLab[2];
	  
	  if (i < 2)
	    strcpy(topLab, "N");
	  else
	    strcpy(topLab, "S");
	  if (i % 2)
	    strcpy(rightLab, "W");
	  else
	    strcpy(rightLab, "E");
	  for (j = 0; j <= 100; j++) {
	    littleCircle[j].x = roundf(8.0*sinf(DEGREES_TO_RADIANS*(float)(j*10)))
	      + (9+i*2)*displayWidth/16;
	    littleCircle[j].y = y+15*nStepsPerDay + 60
	      + roundf(8.0*cosf(DEGREES_TO_RADIANS*(float)(j*10)));
	  }
	  if ((jovianMoonsNE && (i == 0))
	      || (jovianMoonsNW && (i == 1))
	      || (jovianMoonsSE && (i == 2))
	      || (jovianMoonsSW && (i == 3))) {
	    color = OR_GREEN;
	    gdk_draw_polygon(pixmap, gC[color],  TRUE, littleCircle, 37);
	  } else {
	    color = OR_GREY;
	    gdk_draw_polygon(pixmap, gC[color], FALSE, littleCircle, 37);
	  }
	  gdk_draw_line(pixmap, gC[color],
			displayWidth/2 + i*displayWidth/8 + 14,
			y+15*nStepsPerDay + 75,
			displayWidth/2 + i*displayWidth/8 + 14,
			y+15*nStepsPerDay + 50);
	  gdk_draw_line(pixmap, gC[color],
			displayWidth/2 + i*displayWidth/8 + 14,
			y+15*nStepsPerDay + 75,
			displayWidth/2 + i*displayWidth/8 + 39,
			y+15*nStepsPerDay + 75);
	  width = gdk_string_width(smallFont, topLab);
	  gdk_draw_string(pixmap, smallFont, gC[color],
			  displayWidth/2 + i*displayWidth/8 + 14 - width/2,
			  y+15*nStepsPerDay + 45, topLab);
	  width = gdk_string_width(smallFont, rightLab);
	  gdk_draw_string(pixmap, smallFont, gC[color],
			  displayWidth/2 + i*displayWidth/8 + 48 - width/2,
			  y+15*nStepsPerDay + 80,
			  rightLab);
	  addSensitiveArea(FALSE, SA_JOVIAN,
			   displayWidth/2 + i*displayWidth/8 + 14,
			   y+15*nStepsPerDay + 45,
			   displayWidth/2 + i*displayWidth/8 + 48,
			   y+15*nStepsPerDay + 75,
			   (float)i);
	}
	sJD  = buildTJD(year-1900, month-1, 1, 0, 0, 0, 0);
	sJD += deltaT(sJD)/86400.0;
	eJD  = sJD + (double)maxDay;
	gdk_draw_line(pixmap, gC[OR_CREAM], xDiskLeft,  y, xDiskLeft, y+16*nStepsPerDay);
	gdk_draw_line(pixmap, gC[OR_CREAM], xDiskRight, y, xDiskRight, y+16*nStepsPerDay);
	gdk_draw_line(pixmap, gC[OR_CREAM], displayWidth/2 + xDiskLeft,
		      y, displayWidth/2 + xDiskLeft, y+(maxDay-16)*nStepsPerDay);
	gdk_draw_line(pixmap, gC[OR_CREAM], displayWidth/2 + xDiskRight,
		      y, displayWidth/2 + xDiskRight, y+(maxDay-16)*nStepsPerDay);
	
	/* Draw the actual wavey lines, color coded by satellite */
	for (pJD = sJD; pJD <= eJD; pJD += timeStep) {
	  int gal[N_GAL], pY;
	  
	  if ((pJD - sJD) < 16.0)
	    pY =  y + (int)((pJD-sJD)*((double)nStepsPerDay) + 0.5); /* Plot on left side */
	  else
	    pY =  y + (int)((pJD-sJD-16.0)*((double)nStepsPerDay) + 0.5); /* Plot on right side */
	  lowAccuracyJovSats(pJD, galX, NULL, galAngle);
	  for (iGal = IO; iGal <= CALLISTO; iGal++)
	    gal[iGal] = xSign*galX[iGal]*scale + plotCenter;
	  if ((pJD - sJD) >= 16.0)
	    for (iGal = IO; iGal <= CALLISTO; iGal++)
	      gal[iGal] += displayWidth/2;
	  
	  /*
	    Here we actually draw the line, but only if the satellite is not behind
	    Jupiter's disk.   We also don't draw it if it is too near the current
	    time - that makes a gap into which the moon dots can be plotted, and
	    it enhances their visibility.
	  */
	  if ((pJD != sJD) && (fabs(lastPY-pY) < 100) && (fabs(pJD-tJD) > 0.2))
	    for (iGal = IO; iGal <= CALLISTO; iGal++)
	      if (((abs(galX[iGal]) >= 1.0)     || (fabs(180.0-galAngle[iGal]) > 90.0))
		  && ((abs(lastGalX[iGal]) >= 1.0) || (fabs(180.0-lastGalAngle[iGal]) > 90.0)))
		gdk_draw_line(pixmap, gC[galColor[iGal]], lastGal[iGal], lastPY, gal[iGal], pY);
	  
	  /*
	    Here we draw the little dots on the wavey line plot, which show the
	    current position of each satellite, color coded, of course.
	  */
	  if ((fabs(pJD-tJD) <= timeStep*0.5) && (!plottedThem)) {
	    int dY;
	    
	    if ((pJD - sJD) < 16.0)
	      dY =  y + (int)((tJD-sJD)*((double)nStepsPerDay) + 0.5);
	    else
	      dY =  y + (int)((tJD-sJD-16.0)*((double)nStepsPerDay) + 0.5);
	    for (iGal = IO; iGal <= CALLISTO; iGal++)
	      if (((abs(galX[iGal]) >= 1.0) || (fabs(180.0-galAngle[iGal]) > 90.0))
		  && (!((galSR[iGal] < 1.0) && (galZ[iGal] > 0.0))))
		gdk_draw_arc(pixmap, gC[galColor[iGal]], TRUE, gal[iGal]-2, dY-2, 5, 5, 0, FULL_CIRCLE);
	    plottedThem = TRUE;
	  }
	  lastPY = pY;
	  for (iGal = IO; iGal <= CALLISTO; iGal++) {
	    lastGal[iGal] = gal[iGal];
	    lastGalX[iGal] = galX[iGal];
	    lastGalAngle[iGal] = galAngle[iGal];
	  }
	}
      }
    }
    break;
  case COMET_SCREEN:
#define LINE_SKIP (35)
    {
      int foundADisplayableComet = FALSE;
      int tWidth, tHeight, line = 0;
      double lHelio, bHelio, rHelio;
      char scratchString[100];
      cometEphem *comet;

      needNewTime = TRUE;
      lSTNow = lST();
      if (!cometDataReadIn)
	readInCometEphemerides(dataDir);
      heliocentricEclipticCoordinates(dataDir, tJD, 2, &lHelio, &bHelio, &rHelio);
      comet = cometRoot;
      while (comet != NULL) {
	if (comet->valid && (comet->firstTJD <= tJD) && (comet->lastTJD >= tJD)) {
	  int cometBoldColor, cometColor, rAHH, rAMM, decDD, decMM;
	  int hAHH, hAMM, decSign, hASign;
	  double rA, dec, hA, mag, az, zA, el, lComet, bComet, rComet;
	  double xEarth, yEarth, zEarth, xComet, yComet, zComet, distance;

	  if (!foundADisplayableComet) {
	    sprintf(scratchString, "Comet information for   ");
	    makeTimeString(&scratchString[strlen(scratchString)], TRUE);
	    renderPangoText(scratchString, OR_CREAM, SMALL_PANGO_FONT, &tWidth, &tHeight,
			    pixmap, displayWidth>>1, 11+line, 0.0, TRUE, 0);
	    line += 10+LINE_SKIP;
	  }
	  getCometRADec(dataDir, comet->name, tJD, FALSE, &lComet, &bComet, &rComet, &mag);
	  lComet *= DEGREES_TO_RADIANS; bComet *= DEGREES_TO_RADIANS;
	  getCometRADec(dataDir, comet->name, tJD, TRUE,  &rA,     &dec,    NULL,    &mag);
	  xEarth = rHelio*cos(bHelio)*cos(lHelio);
	  yEarth = rHelio*cos(bHelio)*sin(lHelio);
	  zEarth = rHelio*sin(bHelio);
	  xComet = rComet*cos(bComet)*cos(lComet);
	  yComet = rComet*cos(bComet)*sin(lComet);
	  zComet = rComet*sin(bComet);
	  distance = sqrt(pow(xEarth-xComet, 2)+pow(yEarth-yComet, 2)+pow(zEarth-zComet, 2));
	  azZA(rA, sin(dec), cos(dec), &az, &zA, FALSE);
	  el = M_HALF_PI - zA;
	  if (el > 0.0) {
	    cometBoldColor = OR_WHITE;
	    cometColor = OR_BLUE;
	  } else
	    cometBoldColor = cometColor = OR_GREY;
	  renderPangoText("Comet", cometColor, MEDIUM_PANGO_FONT,
			  &tWidth, &tHeight, pixmap, 15, 20+line, 0.0, FALSE, 0);
	  if (comet->nickName)
	    sprintf(scratchString, "%s\t(%s)", comet->name, comet->nickName);
	  else
	    sprintf(scratchString, "%s", comet->name);
	  renderPangoText(scratchString, cometBoldColor, MEDIUM_PANGO_FONT,
			  &tWidth, &tHeight, pixmap, 25+tWidth, 20+line, 0.0, FALSE, 0);
	  line += LINE_SKIP;
	  if (dec < 0.0) {
	    decSign = -1;
	    dec *= -1.0;
	  } else
	    decSign = 1;
	  hA = lSTNow - rA;
	  doubleNormalize0to2pi(&rA); doubleNormalizeMinusPiToPi(&hA); doubleNormalize0to2pi(&az);
	  if (hA < 0.0) {
	    hASign = -1;
	    hA *= -1.0;
	  } else
	    hASign = 1;
	  rA /= HOURS_TO_RADIANS; hA /= HOURS_TO_RADIANS; dec /= DEGREES_TO_RADIANS; az /= DEGREES_TO_RADIANS; el /= DEGREES_TO_RADIANS;
	  rAHH  = (int)rA;   rAMM = roundf(((rA  - (double)rAHH)*60.0));
	  hAHH  = (int)hA;   hAMM = roundf(((hA  - (double)hAHH)*60.0));
	  decDD = (int)dec; decMM = roundf(((dec - (double)decDD)*60.0));
	  renderPangoText("RA", cometColor, MEDIUM_PANGO_FONT,
			  &tWidth, &tHeight, pixmap, 10, 20+line, 0.0, FALSE, 0);
	  sprintf(scratchString, "%02d:%02d", rAHH, rAMM);
	  renderPangoText(scratchString, cometBoldColor, MEDIUM_PANGO_FONT,
			  &tWidth, &tHeight, pixmap, 55, 20+line, 0.0, FALSE, 0);
	  renderPangoText("Dec", cometColor, MEDIUM_PANGO_FONT,
			  &tWidth, &tHeight, pixmap, displayWidth/3+10, 20+line, 0.0, FALSE, 0);
	  sprintf(scratchString, "%02d:%02d", decDD*decSign, decMM);
	  renderPangoText(scratchString, cometBoldColor, MEDIUM_PANGO_FONT,
			  &tWidth, &tHeight, pixmap, displayWidth/3+65, 20+line, 0.0, FALSE, 0);
	  renderPangoText("HA", cometColor, MEDIUM_PANGO_FONT,
			  &tWidth, &tHeight, pixmap, 2*displayWidth/3+10, 20+line, 0.0, FALSE, 0);
	  sprintf(scratchString, "%02d:%02d", hAHH*hASign, hAMM);
	  renderPangoText(scratchString, cometBoldColor, MEDIUM_PANGO_FONT,
			  &tWidth, &tHeight, pixmap, 2*displayWidth/3+65, 20+line, 0.0, FALSE, 0);
	  line += LINE_SKIP;
	  renderPangoText("Az", cometColor, MEDIUM_PANGO_FONT,
			  &tWidth, &tHeight, pixmap, 10, 20+line, 0.0, FALSE, 0);
	  sprintf(scratchString, "%6.2f", az);
	  renderPangoText(scratchString, cometBoldColor, MEDIUM_PANGO_FONT,
			  &tWidth, &tHeight, pixmap, 55, 20+line, 0.0, FALSE, 0);
	  renderPangoText("El", cometColor, MEDIUM_PANGO_FONT,
			  &tWidth, &tHeight, pixmap, displayWidth/3+10, 20+line, 0.0, FALSE, 0);
	  sprintf(scratchString, "%5.2f", el);
	  renderPangoText(scratchString, cometBoldColor, MEDIUM_PANGO_FONT,
			  &tWidth, &tHeight, pixmap, displayWidth/3+65, 20+line, 0.0, FALSE, 0);
	  renderPangoText("Mag", cometColor, MEDIUM_PANGO_FONT,
			  &tWidth, &tHeight, pixmap, 2*displayWidth/3+10, 20+line, 0.0, FALSE, 0);
	  sprintf(scratchString, "%4.1f", mag);
	  renderPangoText(scratchString, cometBoldColor, MEDIUM_PANGO_FONT,
			  &tWidth, &tHeight, pixmap, 2*displayWidth/3+65, 20+line, 0.0, FALSE, 0);
	  line += LINE_SKIP;
	  renderPangoText("Earth distance", cometColor, MEDIUM_PANGO_FONT,
			  &tWidth, &tHeight, pixmap, 10, 20+line, 0.0, FALSE, 0);
	  sprintf(scratchString, "%6.2f AU   %4.0f light min.", distance, distance*AU*1.0e9/SPEED_OF_LIGHT);
	  renderPangoText(scratchString, cometBoldColor, MEDIUM_PANGO_FONT,
			  &tWidth, &tHeight, pixmap, displayWidth/2 - 50, 20+line, 0.0, FALSE, 0);
	  line += LINE_SKIP;
	  renderPangoText("Sun distance", cometColor, MEDIUM_PANGO_FONT,
			  &tWidth, &tHeight, pixmap, 10, 20+line, 0.0, FALSE, 0);
	  sprintf(scratchString, "%6.2f AU   %4.0f light min.", rComet, rComet*AU*1.0e9/SPEED_OF_LIGHT);
	  renderPangoText(scratchString, cometBoldColor, MEDIUM_PANGO_FONT,
			  &tWidth, &tHeight, pixmap, displayWidth/2 - 50, 20+line, 0.0, FALSE, 0);

	  line += LINE_SKIP;
	  line += LINE_SKIP;
	  foundADisplayableComet = TRUE;
	}
	comet = comet->next;
      }
      if (!foundADisplayableComet) {
	renderPangoText("No comet info for this time", OR_WHITE, MEDIUM_PANGO_FONT,
			&tWidth, &tHeight, pixmap, displayWidth >> 1, 20, 0.0, TRUE, 0);
      }
    }
    break;
  case LUNAR_ECLIPSES:
    {
      int i, j, eclipseType;

      if (displayEclipse) {
	static int haveReadShorelineFile = FALSE;
	int mM, dD, yYYY, dTMinusUT, hH, minute, sS, nShadedRegions, nPoints,
	  penColor, parColor, totColor, region, xHome, pass, mapWidth, moonRadiusPixels,
	  maxEclipseColor, lineOffset, lineWidth;
	int nSteps = 500;
	int lineSkip = 30;
	float lat, lon, step, f1, a, b, theta, eclipticSlope, eclipticX, eclipticY,
	  shadowPlaneX, shadowPlaneY, moonPathAngle;
	float lastLon = 0.0;
	double penEclipseStartTJD, penEclipseEndTJD, parEclipseStartTJD, parEclipseEndTJD,
	  totEclipseStartTJD, totEclipseEndTJD, tDGE, eclipseUTHours, d1, d2, d3, moonDistanceKM,
	  eclipseTJD, rA, dec, sublunarLat, sublunarLon, moonDistanceCM, sunRA, sunDec,
	  sunDistanceKM, sunDistanceCM, rUmbraEquatorial, rUmbraPolar, rPenumbraEquatorial, rPenumbraPolar,
	  lEEquatorial, lEPolar, lPEquatorial, lPPolar, shadowPlaneRA, shadowPlaneDec, x0, y0;
	char title[100], scratchString[100], typeString[17];
	shoreSeg *segment;
	gint shadowPlanePX, shadowPlanePY;
	GdkPoint verticies[504];
	GdkGC *eGC;
	
	if (chineseColorScheme)
	  eGC = gC[OR_YELLOW];
	else
	  eGC = gC[OR_RED];
	if (timerID != (guint)0) {
	  g_source_remove(timerID);
	  timerID = (guint)0;
	}
	if (lunarEclipsePixmap == NULL) {
	  lunarEclipsePixmap = gdk_pixmap_new(drawingArea->window, 
					      (int)EARTH_MAP_WIDTH, (int)EARTH_MAP_HEIGHT, -1);
	  if (lunarEclipsePixmap == NULL) {
	    perror("lunarEclipsePixmap");
	    return;
	  }
	}
	if (!haveReadShorelineFile) {
	  int shoreFD;
	  char fileName[MAX_FILE_NAME_SIZE];
	  
	  /* Read in the binary file containing vectors to plot the earth's shorelines */
	  sprintf(fileName, "%s/shoreline", dataDir);
	  shoreFD = open(fileName, O_RDONLY);
	  if (shoreFD > 0) {
	    short nPairs;
	    int i, nRead;
	    shoreSeg *newSeg, *lastSeg = NULL;
	    
	    nRead = read(shoreFD, &nPairs, 2);
	    while (nRead > 0) {
	      newSeg = (shoreSeg *)malloc(sizeof(shoreSeg));
	      if (newSeg != NULL) {
		newSeg->nVerticies = nPairs;
		newSeg->lat = malloc(nPairs*sizeof(short));
		newSeg->lon = malloc(nPairs*sizeof(short));
		for (i = 0; i < nPairs; i++) {
		  read(shoreFD, &newSeg->lon[i], 2);
		  read(shoreFD, &newSeg->lat[i], 2);
		}
		newSeg->next = NULL;
		if (shoreRoot == NULL)
		  shoreRoot = newSeg;
		else
		  lastSeg->next = newSeg;
		lastSeg = newSeg;
	      } else
		perror("malloc of newSeg");
	      nRead = read(shoreFD, &nPairs, 2);
	    }
	    close(shoreFD);
	    haveReadShorelineFile = TRUE;
	  } else
	    perror("shoreline");
	}
	gdk_draw_rectangle(lunarEclipsePixmap, gC[OR_DARK_GREY], TRUE, 0, 0,
			   displayWidth, EARTH_MAP_HEIGHT);

	/*
	  Calculate the time of maximum eclipse, and the earch longitude and latitude
	  of the point directly under the moon (subLunarxxx) at the time of
	  maximum eclipse.
	*/
	yYYY        =  lunarEclipses[selectedLunarEclipse].date/0x10000;
	mM          = (lunarEclipses[selectedLunarEclipse].date & 0xff00)/0x100;
	dD          =  lunarEclipses[selectedLunarEclipse].date & 0xff;
	eclipseType =  lunarEclipses[selectedLunarEclipse].type1 & 0xf;
	tDGE        =  lunarEclipses[selectedLunarEclipse].tDGE;
	dTMinusUT   =  lunarEclipses[selectedLunarEclipse].dTMinusUT;
	eclipseUTHours = (tDGE - dTMinusUT)/3600.0;
	hH = (int)eclipseUTHours;
	minute = (int)((eclipseUTHours - (double)hH) * 60.0);
	sS = (int)((eclipseUTHours - (double)hH - ((double)minute)/60.0)*3600.0 + 0.5);
	eclipseTJD = buildTJD(yYYY-1900, mM-1, dD, hH, minute, sS, 0);
	vSOPPlanetInfo(dataDir, eclipseTJD, SUN, &sunRA, &sunDec, &sunDistanceKM);
	eclipticSlope = tanf(ETA*cos(sunRA));
	eclipticX = UMBRA_MAP_WIDTH*0.5;
	eclipticY = eclipticX * eclipticSlope;
	if (fabs(eclipticY) > UMBRA_MAP_HEIGHT*0.5) {
	  eclipticY = UMBRA_MAP_HEIGHT * 0.5;
	  eclipticX = eclipticY / eclipticSlope;
	}
 	moonPosition(eclipseTJD, &rA, &dec, &d1, &d2, &moonDistanceKM, &f1);
	shadowPlaneRA = sunRA - M_PI - rA; shadowPlaneDec = -sunDec - dec;
	moonDistanceCM = moonDistanceKM * 1.0e5;
	sunDistanceCM  = sunDistanceKM  * AU * 1.0e11;
	moonRadiusPixels = 1 + (int)(atan(MOON_RADIUS/moonDistanceCM)*moonDistanceCM*UMBRA_MAP_SCALE);
	shadowPlaneX =  sin(shadowPlaneRA)*moonDistanceCM;
	shadowPlaneY = -sin(shadowPlaneDec)*moonDistanceCM;
	cmToPixels(shadowPlaneX, shadowPlaneY, &shadowPlanePX, &shadowPlanePY);
	switch (eclipseType) {
	case TOTAL_LUNAR_ECLIPSE:
	  strcpy(typeString, "Total Lunar");
	  break;
	case PARTIAL_LUNAR_ECLIPSE:
	  strcpy(typeString, "Partial Lunar");
	  break;
	default:
	  strcpy(typeString, "Penumbral");
	}
	sprintf(title, "%s Eclipse %s %d, %d", typeString,
		monthName[mM-1], dD, yYYY);
	backgroundGC = OR_WHITE;
	gdk_draw_rectangle(pixmap, gC[backgroundGC], TRUE, 0, UMBRA_MAP_OFFSET - UMBRA_MAP_HEIGHT/2,
			   UMBRA_MAP_WIDTH, UMBRA_MAP_HEIGHT);
	gdk_draw_line(pixmap, gC[OR_BLACK], 45, UMBRA_MAP_OFFSET, UMBRA_MAP_WIDTH, UMBRA_MAP_OFFSET);
	renderPangoText("North", OR_BLACK, SMALL_PANGO_FONT, &tWidth, &tHeight,
			pixmap, UMBRA_MAP_WIDTH-90, UMBRA_MAP_OFFSET - 96,
			0.0, TRUE, 1);
	renderPangoText("North", OR_BLACK, SMALL_PANGO_FONT, &tWidth, &tHeight,
			pixmap, UMBRA_MAP_WIDTH-90, UMBRA_MAP_OFFSET - 96,
			0.0, TRUE, 1);
	renderPangoText("East", OR_BLACK, SMALL_PANGO_FONT, &tWidth, &tHeight,
			pixmap, 20, UMBRA_MAP_OFFSET,
			0.0, TRUE, 0);
	renderPangoText("East", OR_BLACK, SMALL_PANGO_FONT, &tWidth, &tHeight,
			pixmap, 20, UMBRA_MAP_OFFSET,
			0.0, TRUE, 0);
	/*
	  lEEquatorial is the distance from the Sun to the end of the umbra, in the
	  earth's equatorial plane.
	*/
	lEEquatorial = sunDistanceCM * (1.0 + EARTH_EQUATORIAL_RADIUS/(SOLAR_RADIUS-EARTH_EQUATORIAL_RADIUS));
	lEPolar      = sunDistanceCM * (1.0 + EARTH_POLAR_RADIUS     /(SOLAR_RADIUS-EARTH_POLAR_RADIUS));
	/* Calculate radii for the earth's umbra */
	rUmbraEquatorial  = SOLAR_RADIUS * (lEEquatorial - sunDistanceCM - moonDistanceCM)/lEEquatorial;
	rUmbraEquatorial *= ATMOSPHERIC_UMBRA_EXPANSION;
	rUmbraPolar       = SOLAR_RADIUS * (lEPolar      - sunDistanceCM - moonDistanceCM)/lEPolar;
	rUmbraPolar      *= ATMOSPHERIC_UMBRA_EXPANSION;
	/*
	  lPEquatorial is the distance from the Sun to the apex of the cone whose
	  base is the intersection of the penumbra with the eclipse plane.
	*/
	lPEquatorial = SOLAR_RADIUS * sunDistanceCM / (SOLAR_RADIUS + EARTH_EQUATORIAL_RADIUS);
	lPPolar      = SOLAR_RADIUS * sunDistanceCM / (SOLAR_RADIUS + EARTH_POLAR_RADIUS);
	/* Calculate radii for the earth's penumbra */
	rPenumbraEquatorial  = SOLAR_RADIUS * (sunDistanceCM - lPEquatorial + moonDistanceCM)/lPEquatorial;
	rPenumbraEquatorial *= ATMOSPHERIC_UMBRA_EXPANSION;
	rPenumbraPolar       = SOLAR_RADIUS * (sunDistanceCM - lPPolar + moonDistanceCM)     /lPPolar;
	rPenumbraPolar      *= ATMOSPHERIC_UMBRA_EXPANSION;
	/* Draw the penumbra */
	a = (float)rPenumbraEquatorial; b = (float)rPenumbraPolar;
	nPoints = 0;
	for (theta = 0; theta < M_2PI; theta += M_2PI*0.02) {
	  float x, y;

	  x = a*cosf(theta); y = b*sinf(theta);
	  cmToPixels(x, y, &verticies[nPoints].x, &verticies[nPoints].y);
	  nPoints++;
	}
	gdk_draw_polygon(pixmap, gC[OR_LIGHT_GREY], TRUE, verticies, nPoints);
	/* Draw the umbra */
	a = (float)rUmbraEquatorial; b = (float)rUmbraPolar;
	nPoints = 0;
	for (theta = 0; theta < M_2PI; theta += M_2PI*0.02) {
	  float x, y;

	  x = a*cosf(theta); y = b*sinf(theta);
	  cmToPixels(x, y, &verticies[nPoints].x, &verticies[nPoints].y);
	  nPoints++;
	}
	gdk_draw_polygon(pixmap, gC[OR_DARK_GREY], TRUE, verticies, nPoints);
	eclipticSlope = eclipticY/(UMBRA_MAP_HEIGHT*0.5);
	if (eclipticY < 0.0) {
	  int offset;

	  if (eclipticY > -25.0)
	    offset = -25;
	  else
	    offset = (int)eclipticY;
	  if (offset > 80)
	    offset = 45;
	  if (offset < -80)
	    offset = -45;
	  renderPangoText("Ecliptic", OR_RED, SMALL_PANGO_FONT, &tWidth, &tHeight,
			  pixmap, UMBRA_MAP_WIDTH - 70, UMBRA_MAP_OFFSET - offset,
			  -ETA*cosf(sunRA), TRUE, 1);
	  renderPangoText("Ecliptic", OR_RED, SMALL_PANGO_FONT, &tWidth, &tHeight,
			  pixmap, UMBRA_MAP_WIDTH - 70, UMBRA_MAP_OFFSET - offset,
			  -ETA*cosf(sunRA), TRUE, 1);
	} else {
	  int offset;

	  if (eclipticY < 25.0)
	    offset = 25;
	  else
	    offset = (int)eclipticY;
	  if (offset > 80)
	    offset = 45;
	  if (offset < -80)
	    offset = -45;
	  renderPangoText("Ecliptic", OR_RED, SMALL_PANGO_FONT, &tWidth, &tHeight,
			  pixmap, UMBRA_MAP_WIDTH - 70, UMBRA_MAP_OFFSET - offset,
			  -ETA*cosf(sunRA), TRUE, 1);
	  renderPangoText("Ecliptic", OR_RED, SMALL_PANGO_FONT, &tWidth, &tHeight,
			  pixmap, UMBRA_MAP_WIDTH - 70, UMBRA_MAP_OFFSET - offset,
			  -ETA*cosf(sunRA), TRUE, 1);
	}
	backgroundGC = OR_BLACK;
	gdk_draw_line(pixmap, eGC, -eclipticX + UMBRA_MAP_WIDTH*0.5,
		      eclipticY + UMBRA_MAP_OFFSET,
		      eclipticX + UMBRA_MAP_WIDTH*0.5,
		      -eclipticY + UMBRA_MAP_OFFSET);
	gdk_draw_line(pixmap, gC[OR_BLACK], UMBRA_MAP_WIDTH/2 - 5, UMBRA_MAP_OFFSET,
		      UMBRA_MAP_WIDTH/2 + 5, UMBRA_MAP_OFFSET);
	gdk_draw_line(pixmap, gC[OR_BLACK], UMBRA_MAP_WIDTH/2, UMBRA_MAP_OFFSET - 5,
		      UMBRA_MAP_WIDTH/2, UMBRA_MAP_OFFSET + 5);
	myLST = lSTAtTJD(eclipseTJD);
	sublunarLat = dec;
	sublunarLon = longitude - myLST + rA;
	doubleNormalizeMinusPiToPi(&sublunarLon);
	step = M_2PI/((float)nSteps);
	penEclipseStartTJD = eclipseTJD - lunarEclipses[selectedLunarEclipse].penDur/2880.0;
	penEclipseEndTJD   = eclipseTJD + lunarEclipses[selectedLunarEclipse].penDur/2880.0;
	parEclipseStartTJD = eclipseTJD - lunarEclipses[selectedLunarEclipse].parDur/2880.0;
	parEclipseEndTJD   = eclipseTJD + lunarEclipses[selectedLunarEclipse].parDur/2880.0;
	totEclipseStartTJD = eclipseTJD - lunarEclipses[selectedLunarEclipse].totDur/2880.0;
	totEclipseEndTJD   = eclipseTJD + lunarEclipses[selectedLunarEclipse].totDur/2880.0;
	penColor = parColor = totColor = 0;
	/* Select which color to use for each eclipse phase */
	switch (eclipseType) {
	case TOTAL_LUNAR_ECLIPSE:
	  nShadedRegions  = 6;
	  penColor        = OR_GREY;
	  parColor        = OR_LIGHT_GREY;
	  totColor        = OR_WHITE;
	  maxEclipseColor = OR_YELLOW;
	  lineOffset      = 10 + 3*lineSkip;
	  break;
	case PARTIAL_LUNAR_ECLIPSE:
	  nShadedRegions  = 4;
	  penColor        = OR_LIGHT_GREY;
	  parColor        = OR_WHITE;
	  maxEclipseColor = OR_YELLOW;
	  lineOffset      = 10 + 2*lineSkip;
	  break;
	default:
	  nShadedRegions  = 2;
	  penColor        = OR_WHITE;
	  maxEclipseColor = OR_BLACK;
	  lineOffset      = 10 + lineSkip;
	}
	/* Draw the moon position in the shadow at maximum eclipse */
	gdk_draw_arc(pixmap, gC[maxEclipseColor], FALSE, shadowPlanePX - moonRadiusPixels,
		     shadowPlanePY-moonRadiusPixels,
		     2*moonRadiusPixels, 2*moonRadiusPixels, 0, FULL_CIRCLE);
	renderPangoText("Maximum Eclipse at", OR_BLUE, MEDIUM_PANGO_FONT, &tWidth, &tHeight,
			pixmap, shadowPlanePX - 68, UMBRA_MAP_OFFSET - UMBRA_MAP_HEIGHT/2 - lineOffset - 15,
			0.0, TRUE, 1);
	renderPangoText("Maximum Eclipse at", OR_BLUE, MEDIUM_PANGO_FONT, &tWidth, &tHeight,
			pixmap, shadowPlanePX - 68, UMBRA_MAP_OFFSET - UMBRA_MAP_HEIGHT/2 - lineOffset - 15,
			0.0, TRUE, 1);
	sprintf(scratchString, "%02d:%02d:%02d", hH, minute, sS);
	renderPangoText(scratchString, OR_CREAM, MEDIUM_PANGO_FONT, &tWidth, &tHeight,
			pixmap, shadowPlanePX + 95, UMBRA_MAP_OFFSET - UMBRA_MAP_HEIGHT/2 - lineOffset - 15,
			0.0, TRUE, 1);
	renderPangoText("UT", OR_BLUE, MEDIUM_PANGO_FONT, &tWidth, &tHeight,
			pixmap, shadowPlanePX + 164, UMBRA_MAP_OFFSET - UMBRA_MAP_HEIGHT/2 - lineOffset - 15,
			0.0, TRUE, 1);
	gdk_draw_line(pixmap, gC[OR_GREEN],
		      shadowPlanePX, shadowPlanePY - moonRadiusPixels - 5,
		      shadowPlanePX, UMBRA_MAP_OFFSET - UMBRA_MAP_HEIGHT/2 - lineOffset);
	verticies[0].x = shadowPlanePX;     verticies[0].y = shadowPlanePY - moonRadiusPixels - 5;
	verticies[1].x = shadowPlanePX - 4; verticies[1].y = shadowPlanePY - moonRadiusPixels - 19;
	verticies[2].x = shadowPlanePX + 4; verticies[2].y = shadowPlanePY - moonRadiusPixels - 19;
	lineWidth = 360;
	gdk_draw_line(pixmap, gC[OR_GREEN],
		      shadowPlanePX - lineWidth/2, UMBRA_MAP_OFFSET - UMBRA_MAP_HEIGHT/2 - lineOffset,
		      shadowPlanePX + lineWidth/2, UMBRA_MAP_OFFSET - UMBRA_MAP_HEIGHT/2 - lineOffset);
	gdk_draw_polygon(pixmap, gC[OR_GREEN], TRUE, verticies, 3);
	/*
	  Now draw the circles for first and last contact with the penumbra and
	  (if appropriate) umbra.
	*/
	for (pass = 0; pass < 2; pass++) {
	  int lineSign, mM, textOffset, timeOffset;
	  double hours;

	  if (pass == 0)
	    lineSign = 1;
	  else
	    lineSign = -1;
	  for (region = 0; region < nShadedRegions/2; region++) {
	    int color = 0;
	    double tJD = 0;;
	    
	    if (region == 0) {
	      if (pass == 0) {
		tJD = penEclipseStartTJD;
		strcpy(typeString, "Pen. Starts");
		textOffset = 375;
		timeOffset = 76;
	      } else {
		tJD = penEclipseEndTJD;
		strcpy(typeString, "Pen. Ends");
		textOffset = 50;
		timeOffset = 71;
	      }
	      color = OR_BLACK;
	      lineOffset = 10;
	    } else if (region == 1) {
	      if (pass == 0) {
		tJD = parEclipseStartTJD;
		strcpy(typeString, "Partial Starts");
		textOffset = 366;
		timeOffset = 85;
	      } else {
		tJD = parEclipseEndTJD;
		strcpy(typeString, "Partial Ends");
		textOffset = 59;
		timeOffset = 81;
	      }
	      color = OR_BLACK;
	      lineOffset = 10 + lineSkip;
	    } else {
	      if (pass == 0) {
		tJD = totEclipseStartTJD;
		strcpy(typeString, "Total Eclipse Starts");
		textOffset = 340;
		timeOffset = 111;
	      } else {
		tJD = totEclipseEndTJD;
		strcpy(typeString, "Total Eclipse Ends");
		textOffset = 84;
		timeOffset = 107;
	      }
	      color = maxEclipseColor;
	      lineOffset = 10 + 2*lineSkip;
	    }
	    hours = 24.0*(tJD - (double)((int)tJD)) - 12.0;
	    if (hours < 0.0)
	      hours += 24.0;
	    hH = (int)hours; mM = (int)((hours - (double)hH)*60.0 + 0.5);
	    fix60s(&hH, &mM);
	    renderPangoText(typeString, OR_BLUE, SMALL_PANGO_FONT, &tWidth, &tHeight,
			    pixmap, textOffset,
			    UMBRA_MAP_OFFSET - UMBRA_MAP_HEIGHT/2 - lineOffset - 12,
			    0.0, TRUE, 1);
	    sprintf(scratchString, "%02d:%02d", hH, mM);
	    renderPangoText(scratchString, OR_CREAM, SMALL_PANGO_FONT, &tWidth, &tHeight,
			    pixmap, textOffset + timeOffset,
			    UMBRA_MAP_OFFSET - UMBRA_MAP_HEIGHT/2 - lineOffset - 12,
			    0.0, TRUE, 1);
	    moonPosition(tJD, &rA, &dec, &d1, &d2, &moonDistanceKM, &f1);
	    moonDistanceCM = moonDistanceKM * 1.0e5;
	    vSOPPlanetInfo(dataDir, tJD, SUN, &sunRA, &sunDec, &sunDistanceKM);
	    shadowPlaneRA = sunRA - M_PI - rA; shadowPlaneDec = -sunDec - dec;
	    shadowPlaneX =  sin(shadowPlaneRA)*moonDistanceCM;
	    shadowPlaneY = -sin(shadowPlaneDec)*moonDistanceCM;
	    cmToPixels(shadowPlaneX, shadowPlaneY, &shadowPlanePX, &shadowPlanePY);
	    gdk_draw_arc(pixmap, gC[color], FALSE, shadowPlanePX - moonRadiusPixels,
			 shadowPlanePY-moonRadiusPixels,
			 2*moonRadiusPixels, 2*moonRadiusPixels, 0, FULL_CIRCLE);
	    gdk_draw_line(pixmap, gC[OR_GREEN],
			  shadowPlanePX, shadowPlanePY - moonRadiusPixels - 5,
			  shadowPlanePX, UMBRA_MAP_OFFSET - UMBRA_MAP_HEIGHT/2 - lineOffset);
	    verticies[0].x = shadowPlanePX;     verticies[0].y = shadowPlanePY - moonRadiusPixels - 5;
	    verticies[1].x = shadowPlanePX - 4; verticies[1].y = shadowPlanePY - moonRadiusPixels - 19;
	    verticies[2].x = shadowPlanePX + 4; verticies[2].y = shadowPlanePY - moonRadiusPixels - 19;
	    gdk_draw_polygon(pixmap, gC[OR_GREEN], TRUE, verticies, 3);
	    gdk_draw_line(pixmap, gC[OR_GREEN],
			  shadowPlanePX,                      UMBRA_MAP_OFFSET - UMBRA_MAP_HEIGHT/2 - lineOffset,
			  shadowPlanePX + lineSign*lineWidth, UMBRA_MAP_OFFSET - UMBRA_MAP_HEIGHT/2 - lineOffset);
	  }
	}
	/* Now draw a line along the Moon's path */
	d3 = eclipseTJD - 0.17;
	moonPosition(d3, &rA, &dec, &d1, &d2, &moonDistanceKM, &f1);
	moonDistanceCM = moonDistanceKM * 1.0e5;
	vSOPPlanetInfo(dataDir, d3, SUN, &sunRA, &sunDec, &sunDistanceKM);
	shadowPlaneRA = sunRA - M_PI - rA; shadowPlaneDec = -sunDec - dec;
	shadowPlaneX =  sin(shadowPlaneRA)*moonDistanceCM;
	shadowPlaneY = -sin(shadowPlaneDec)*moonDistanceCM;
	x0 = shadowPlaneX; y0 = shadowPlaneY;
	cmToPixels(shadowPlaneX, shadowPlaneY, &verticies[1].x, &verticies[1].y);
	d3 = eclipseTJD + 0.25;
	moonPosition(d3, &rA, &dec, &d1, &d2, &moonDistanceKM, &f1);
	moonDistanceCM = moonDistanceKM * 1.0e5;
	vSOPPlanetInfo(dataDir, d3, SUN, &sunRA, &sunDec, &sunDistanceKM);
	shadowPlaneRA = sunRA - M_PI - rA; shadowPlaneDec = -sunDec - dec;
	shadowPlaneX =  sin(shadowPlaneRA)*moonDistanceCM;
	shadowPlaneY = -sin(shadowPlaneDec)*moonDistanceCM;
	cmToPixels(shadowPlaneX, shadowPlaneY, &verticies[0].x, &verticies[0].y);
	gdk_draw_line(pixmap, gC[OR_DARK_BLUE_GREEN], verticies[0].x, verticies[0].y
		      ,verticies[1].x, verticies[1].y);
	moonPathAngle = atan2f(y0-shadowPlaneY, shadowPlaneX-x0);
	cmToPixels(shadowPlaneX-4.0e8*cos( 0.25+moonPathAngle),
		   shadowPlaneY+4.0e8*sin( 0.25+moonPathAngle),
		   &verticies[1].x, &verticies[1].y);
	cmToPixels(shadowPlaneX-4.0e8*cos(-0.25+moonPathAngle),
		   shadowPlaneY+4.0e8*sin(-0.25+moonPathAngle),
		   &verticies[2].x, &verticies[2].y);
	gdk_draw_polygon(pixmap, gC[OR_DARK_BLUE_GREEN], TRUE, verticies, 3);
	f1 = 0.0;
	/*
	  Now loop over the number of shaded regions which must be drawn for a particular eclipse
	  type.   Two regions are drawn for each type of eclipse - the regions show where that
	  phase of the eclipse starts at moonrise, and where it ends at moonrise.
	*/
	for (pass = 0; pass < 2; pass++) {
	  for (region = 0; region < nShadedRegions; region++) {
	    int color = 0;
	    float subLat, subLon, cosSubLat, sinSubLat;
	    double tJD = 0.0;
	    
	    switch (region) {
	    case 0:
	      tJD = penEclipseStartTJD;
	      color = penColor;
	      break;
	    case 1:
	      tJD = penEclipseEndTJD;
	      color = penColor;
	      break;
	    case 2:
	      tJD = parEclipseStartTJD;
	      color = parColor;
	      break;
	    case 3:
	      tJD = parEclipseEndTJD;
	      color = parColor;
	      break;
	    case 4:
	      tJD = totEclipseStartTJD;
	      color = totColor;
	      break;
	    case 5:
	      tJD = totEclipseEndTJD;
	      color = totColor;
	      break;
	    }
	    moonPosition(tJD, &rA, &dec, &d1, &d2, &d3, &f1);
	    myLST = lSTAtTJD(tJD);
	    subLat = dec;
	    subLon = longitude - myLST + rA - M_PI;
	    while (subLon < -M_PI)
	      subLon += M_2PI;
	    while (subLon > M_PI)
	      subLon -= M_2PI;
	    cosSubLat = cosf((float)subLat); sinSubLat = sinf((float)subLat);
	    for (i = j = 0; i < nSteps; i++) {
	      lon = atan2f(sinf(f1), -cosf(f1)*sinSubLat) + subLon;
	      if (lon > M_PI)
		lon -= M_2PI;
	      if (lon < -M_PI)
		lon += M_2PI;
	      lat = -asinf(cosSubLat*cosf(f1));
	      if (pass == 0) {
		if ((i > 0) && (fabs(lastLon - lon) > M_PI)) {
		  if ((lon < 0.0) && (subLat > 0.0)) {
		    latLonToPixels(lat,         M_PI, &verticies[i].x,   &verticies[i].y);
		    latLonToPixels(M_HALF_PI,   M_PI, &verticies[i+1].x, &verticies[i+1].y);
		    latLonToPixels(M_HALF_PI,  -M_PI, &verticies[i+2].x, &verticies[i+2].y);
		    latLonToPixels(lat,        -M_PI, &verticies[i+3].x, &verticies[i+3].y);
		  } else if ((lon < 0.0) && (subLat < 0.0)) {
		    latLonToPixels(lat,         M_PI, &verticies[i].x,   &verticies[i].y);
		    latLonToPixels(-M_HALF_PI,  M_PI, &verticies[i+1].x, &verticies[i+1].y);
		    latLonToPixels(-M_HALF_PI, -M_PI, &verticies[i+2].x, &verticies[i+2].y);
		    latLonToPixels(lat,        -M_PI, &verticies[i+3].x, &verticies[i+3].y);
		  } else if ((lon > 0.0) && (subLat > 0.0)) {
		    latLonToPixels(lat,        -M_PI, &verticies[i].x,   &verticies[i].y);
		    latLonToPixels(M_HALF_PI,  -M_PI, &verticies[i+1].x, &verticies[i+1].y);
		    latLonToPixels(M_HALF_PI,   M_PI, &verticies[i+2].x, &verticies[i+2].y);
		    latLonToPixels(lat,         M_PI, &verticies[i+3].x, &verticies[i+3].y);
		  } else {
		    latLonToPixels(lat,        -M_PI, &verticies[i].x,   &verticies[i].y);
		    latLonToPixels(-M_HALF_PI, -M_PI, &verticies[i+1].x, &verticies[i+1].y);
		    latLonToPixels(-M_HALF_PI,  M_PI, &verticies[i+2].x, &verticies[i+2].y);
		    latLonToPixels(lat,         M_PI, &verticies[i+3].x, &verticies[i+3].y);
		  }
		  j = 4;
		}
	      }
	      latLonToPixels(lat, lon, &verticies[i+j].x, &verticies[i+j].y);
	      lastLon = lon;
	      f1 += step;
	    }
	    if (pass == 0)
	      gdk_draw_polygon(lunarEclipsePixmap, gC[color], TRUE, verticies, i+j);
	    else
	      for (j = 0; j < i-1; j++) 
		if (abs(verticies[j].x - verticies[j+1].x) < EARTH_MAP_WIDTH/2)
		  gdk_draw_line(lunarEclipsePixmap, gC[OR_BLACK],
				verticies[j].x, verticies[j].y, verticies[j+1].x, verticies[j+1].y);
	  }
	}
	segment = shoreRoot;
	while (segment != NULL) {
	  int i;

	  if (segment->nVerticies > 1) {
	    for (i = 0; i < segment->nVerticies; i++) {
	      lat = ((float)segment->lat[i])*M_PI/65535.0; /* Convert signed short range to +-pi/2 */
	      lon = ((float)segment->lon[i])*M_PI/32767.0; /* Convert signed short range to +- pi  */
	      latLonToPixels(lat, lon, &verticies[i].x, &verticies[i].y);
	    }
	    gdk_draw_lines(lunarEclipsePixmap, gC[OR_BLACK], verticies, segment->nVerticies);
	  } else {
	    lat = ((float)segment->lat[0])*M_PI/65535.0; /* Convert signed short range to +-pi/2 */
	    lon = ((float)segment->lon[0])*M_PI/32767.0; /* Convert signed short range to +- pi  */
	    latLonToPixels(lat, lon, &verticies[0].x, &verticies[0].y);
	    gdk_draw_point(lunarEclipsePixmap, gC[OR_BLACK], verticies[0].x, verticies[0].y);
	  }
	  segment = segment->next;
	}
	latLonToPixels((float)latitude, (float)longitude, &verticies[0].x, &verticies[0].y);
	xHome = verticies[0].x;
	gdk_draw_arc(lunarEclipsePixmap, gC[OR_RED], TRUE, verticies[0].x-4, verticies[0].y-4,
		     8, 8, 0, FULL_CIRCLE);
	latLonToPixels(-M_HALF_PI, (float)sublunarLon, &verticies[0].x, &verticies[0].y);
	latLonToPixels(M_HALF_PI, (float)sublunarLon, &verticies[1].x, &verticies[1].y);
	gdk_draw_line(lunarEclipsePixmap, gC[OR_BLUE], verticies[0].x, verticies[0].y, verticies[1].x, verticies[1].y);
	if (verticies[0].x > 0)
	  gdk_draw_line(lunarEclipsePixmap, gC[OR_BLUE], verticies[0].x-1, verticies[0].y,
			verticies[1].x-1, verticies[1].y);
	if (verticies[0].x < displayWidth-1)
	  gdk_draw_line(lunarEclipsePixmap, gC[OR_BLUE], verticies[0].x+1, verticies[0].y,
			verticies[1].x+1, verticies[1].y);
	latLonToPixels((float)sublunarLat, (float)sublunarLon, &verticies[0].x, &verticies[0].y);
	gdk_draw_arc(lunarEclipsePixmap, gC[OR_BLUE], TRUE, verticies[0].x-6, verticies[0].y-6,
		     12, 12, 0, FULL_CIRCLE);
	mapWidth = (int)EARTH_MAP_WIDTH;
	if (xHome < mapWidth/2) {
	  gdk_draw_drawable(pixmap, gC[OR_BLUE], lunarEclipsePixmap,
			    mapWidth/2 + xHome, 10, 0, EARTH_MAP_OFFSET-EARTH_MAP_HEIGHT,
			    mapWidth/2 - xHome, ((int)EARTH_MAP_HEIGHT)-20);
	  gdk_draw_drawable(pixmap, gC[OR_BLUE], lunarEclipsePixmap,
			    0, 10, mapWidth/2 - xHome, EARTH_MAP_OFFSET-EARTH_MAP_HEIGHT,
			    mapWidth/2 + xHome, ((int)EARTH_MAP_HEIGHT)-20);
	} else {
	  gdk_draw_drawable(pixmap, gC[OR_BLUE], lunarEclipsePixmap,
			    0, 10, 3*mapWidth/2 - xHome, EARTH_MAP_OFFSET-EARTH_MAP_HEIGHT,
			    xHome - mapWidth/2, ((int)EARTH_MAP_HEIGHT)-20);
	  gdk_draw_drawable(pixmap, gC[OR_BLUE], lunarEclipsePixmap,
			    xHome - mapWidth/2, 10, 0, EARTH_MAP_OFFSET-EARTH_MAP_HEIGHT,
			    mapWidth/2 - xHome, ((int)EARTH_MAP_HEIGHT)-20);
	}
	{
	  int upAtStart, hH, mM;
	  int up, wasUp = FALSE;
	  int neverWasUp = TRUE, alwaysWasUp = TRUE;
	  int risesInPen = FALSE, risesInPar = FALSE, risesInTot = FALSE;
	  int setsInPen = FALSE, setsInPar = FALSE, setsInTot = FALSE;
	  int willSeePen = FALSE, willSeePar = FALSE, willSeeTot = FALSE;
	  int nPenSeen = 0, nParSeen = 0, nTotSeen = 0;
	  float minutesPen, minutesPar, minutesTot;
	  int iMinutesPen, iMinutesPar, iMinutesTot, textY;
	  double tJD, rA, dec, az, zA, h0, sS;
	  double trackStep = 1.0/2880.0;
	  double riseTime = 0.0, setTime = 0.0;

	  needNewTime = FALSE;
	  h0 = 0.125*DEGREES_TO_RADIANS;
	  for (tJD = penEclipseStartTJD; tJD <= penEclipseEndTJD; tJD += trackStep) {
	    moonPosition(tJD, &rA, &dec, &d1, &d2, &moonDistanceKM, &f1);
	    myLST = lSTAtTJD(tJD);
	    azZA(rA, sin(dec), cos(dec), &az, &zA, FALSE);
	    if (zA < M_HALF_PI - h0) {
	      up = TRUE;
	      neverWasUp = FALSE;
	    } else {
	      up = FALSE;
	      alwaysWasUp = FALSE;
	    }
	    if (tJD == penEclipseStartTJD) {
	      if (up)
		upAtStart = TRUE;
	      else
		upAtStart = FALSE;
	    } else {
	      int inPen = FALSE, inPar = FALSE, inTot = FALSE;

	      if (up
		  && ((tJD - parEclipseStartTJD < 0.0) || (tJD - parEclipseEndTJD > 0.0))) {
		willSeePen = TRUE;
		inPen = TRUE;
		nPenSeen++;
	      } else if (up && (eclipseType > PENUMBRAL_LUNAR_ECLIPSE)
			 && (tJD - parEclipseStartTJD > 0.0) && (tJD - parEclipseEndTJD < 0.0)
			 && ((tJD - totEclipseStartTJD < 0.0) || (tJD - totEclipseEndTJD > 0.0))) {
		willSeePar = TRUE;
		inPar = TRUE;
		nParSeen++;
	      } else if (up && (eclipseType == TOTAL_LUNAR_ECLIPSE)
			 && (tJD - totEclipseStartTJD > 0.0) && (tJD - totEclipseEndTJD < 0.0)) {
		willSeeTot = TRUE;
		inTot = TRUE;
		nTotSeen++;
	      }
	      if (!up
		  && ((tJD - parEclipseStartTJD < 0.0) || (tJD - parEclipseEndTJD > 0.0))) {
		inPen = TRUE;
	      } else if (!up && (eclipseType > PENUMBRAL_LUNAR_ECLIPSE)
			 && (tJD - parEclipseStartTJD > 0.0) && (tJD - parEclipseEndTJD < 0.0)
			 && ((tJD - totEclipseStartTJD < 0.0) || (tJD - totEclipseEndTJD > 0.0))) {
		inPar = TRUE;
	      } else if (!up && (eclipseType == TOTAL_LUNAR_ECLIPSE)
			 && (tJD - totEclipseStartTJD > 0.0) && (tJD - totEclipseEndTJD < 0.0)) {
		inTot = TRUE;
	      }
	      if (up && !wasUp) {
		if (inPen)
		  risesInPen = TRUE;
		else if (inPar)
		  risesInPar = TRUE;
		else if (inTot)
		  risesInTot = TRUE;
		else
		  fprintf(stderr, "LOGIC ERROR #1\n");
		riseTime = tJD;
	      } else if (!up && wasUp) {
		if (inPen)
		  setsInPen = TRUE;
		else if (inPar)
		  setsInPar = TRUE;
		else if (inTot)
		  setsInTot = TRUE;
		else
		  fprintf(stderr, "LOGIC ERROR #2\n");
		setTime = tJD;
	      }
	    }
	    wasUp = up;
	  }
	  if (riseTime != 0.0)
	    tJDToHHMMSS(riseTime, &hH, &mM, &sS);
	  else if (setTime != 0.0)
	    tJDToHHMMSS(setTime, &hH, &mM, &sS);
	  if (sS > 30.0)
	    mM += 1;
	  fix60s(&hH, &mM);
	  minutesPen = ((float)nPenSeen)*trackStep*1440.0 + 0.1; iMinutesPen = roundf(minutesPen);
	  minutesPar = ((float)nParSeen)*trackStep*1440.0 + 0.1; iMinutesPar = roundf(minutesPar);
	  minutesTot = ((float)nTotSeen)*trackStep*1440.0 + 0.1; iMinutesTot = roundf(minutesTot);
	  if (risesInPen || risesInPar || risesInTot
	      || setsInPen || setsInPar || setsInTot) {
	    char label[100];

	    if (risesInPen || risesInPar || risesInTot) {
	      tJD = riseTime;
	      strcpy(label, "Moon rises here");
	    } else {
	      tJD = setTime;
	      strcpy(label, "Moon sets here");
	    }
	    moonPosition(tJD, &rA, &dec, &d1, &d2, &moonDistanceKM, &f1);
	    moonDistanceCM = moonDistanceKM * 1.0e5;
	    vSOPPlanetInfo(dataDir, tJD, SUN, &sunRA, &sunDec, &sunDistanceKM);
	    shadowPlaneRA = sunRA - M_PI - rA; shadowPlaneDec = -sunDec - dec;
	    shadowPlaneX =  sin(shadowPlaneRA)*moonDistanceCM;
	    shadowPlaneY = -sin(shadowPlaneDec)*moonDistanceCM;
	    cmToPixels(shadowPlaneX, shadowPlaneY, &shadowPlanePX, &shadowPlanePY);
	    gdk_draw_arc(pixmap, gC[OR_RED], FALSE, shadowPlanePX - moonRadiusPixels,
			 shadowPlanePY-moonRadiusPixels,
			 2*moonRadiusPixels, 2*moonRadiusPixels, 0, FULL_CIRCLE);
	    gdk_draw_arc(pixmap, gC[OR_RED], FALSE, shadowPlanePX - moonRadiusPixels + 1,
			 shadowPlanePY-moonRadiusPixels + 1,
			 2*(moonRadiusPixels-1), 2*(moonRadiusPixels-1), 0, FULL_CIRCLE);
	    backgroundGC = OR_WHITE;
	    renderPangoText(label, OR_RED, SMALL_PANGO_FONT, &tWidth, &tHeight,
			    pixmap, shadowPlanePX, UMBRA_MAP_OFFSET + UMBRA_MAP_HEIGHT/2 - 12,
			    0.0, TRUE, 1);
	    renderPangoText(label, OR_RED, SMALL_PANGO_FONT, &tWidth, &tHeight,
			    pixmap, shadowPlanePX, UMBRA_MAP_OFFSET + UMBRA_MAP_HEIGHT/2 - 12,
			    0.0, TRUE, 1);
	    backgroundGC = OR_BLACK;
	    if ((UMBRA_MAP_OFFSET + UMBRA_MAP_HEIGHT/2 - 22)-(shadowPlanePY + moonRadiusPixels + 6) > 20) {
	      gdk_draw_line(pixmap, gC[OR_RED],
			    shadowPlanePX, shadowPlanePY + moonRadiusPixels + 6,
			    shadowPlanePX, UMBRA_MAP_OFFSET + UMBRA_MAP_HEIGHT/2 - 22);
	      verticies[0].x = shadowPlanePX;     verticies[0].y = shadowPlanePY + moonRadiusPixels + 6;
	      verticies[1].x = shadowPlanePX - 4; verticies[1].y = shadowPlanePY + moonRadiusPixels + 20;
	      verticies[2].x = shadowPlanePX + 4; verticies[2].y = shadowPlanePY + moonRadiusPixels + 20;
	      gdk_draw_polygon(pixmap, gC[OR_RED], TRUE, verticies, 3);
	    }
	  }
	  textY = 20;
	  renderPangoText(title, OR_CREAM, MEDIUM_PANGO_FONT, &tWidth, &tHeight,
			  pixmap, displayWidth/2, textY,
			  0.0, TRUE, 1);
	  renderPangoText(title, OR_CREAM, MEDIUM_PANGO_FONT, &tWidth, &tHeight,
			  pixmap, displayWidth/2, textY,
			  0.0, TRUE, 1);
	  textY += 30;
	  if (alwaysWasUp) {
	    renderPangoText("All of this eclipse is visible from your location.",
			    OR_GREEN, SMALL_PANGO_FONT, &tWidth, &tHeight,
			    pixmap, displayWidth/2, textY,
			    0.0, TRUE, 1);
	  } else if (neverWasUp) {
	    renderPangoText("This eclipse is not visible from your location at all.",
			    OR_RED, SMALL_PANGO_FONT, &tWidth, &tHeight,
			    pixmap, displayWidth/2, textY,
			    0.0, TRUE, 1);
	  } else if (eclipseType == PENUMBRAL_LUNAR_ECLIPSE) {
	    if (risesInPen) {
	      sprintf(scratchString, "The moon rises at %02d:%02d UT, while the eclipse is underway.",
		      hH, mM);
	      renderPangoText(scratchString,
			      OR_CREAM, SMALL_PANGO_FONT, &tWidth, &tHeight,
			      pixmap, displayWidth/2, textY,
			      0.0, TRUE, 1);
	    } else {
	      sprintf(scratchString, "The moon sets at %02d:%02d UT, before the eclipse is over.",
		      hH, mM);
	      renderPangoText(scratchString,
			      OR_CREAM, SMALL_PANGO_FONT, &tWidth, &tHeight,
			      pixmap, displayWidth/2, textY,
			      0.0, TRUE, 1);
	    }
	  } else if (eclipseType == PARTIAL_LUNAR_ECLIPSE) {
	    if (willSeePar) {
	      renderPangoText("You can see the partial eclipse.",
			      OR_CREAM, SMALL_PANGO_FONT, &tWidth, &tHeight,
			      pixmap, displayWidth/2, textY,
			      0.0, TRUE, 1);
	    } else {
	      renderPangoText("You can only see the penumbral phase.",
			      OR_CREAM, SMALL_PANGO_FONT, &tWidth, &tHeight,
			      pixmap, displayWidth/2, textY,
			      0.0, TRUE, 1);
	    }
	    if (risesInPen) {
	      textY += 25;
	      sprintf(scratchString, "The moon rises at %02d:%02d, during the penumbral phase.",
		      hH, mM);
	      renderPangoText(scratchString,
			      OR_CREAM, SMALL_PANGO_FONT, &tWidth, &tHeight,
			      pixmap, displayWidth/2, textY,
			      0.0, TRUE, 1);
	    } else if (risesInPar) {
	      textY += 25;
	      sprintf(scratchString, "The moon rises at %02d:%02d UT, during the partial phase.",
		      hH, mM);
	      renderPangoText(scratchString,
			      OR_CREAM, SMALL_PANGO_FONT, &tWidth, &tHeight,
			      pixmap, displayWidth/2, textY,
			      0.0, TRUE, 1);
	    } else if (setsInPar) {
	      textY += 25;
	      sprintf(scratchString, "The moon sets at %02d:%02d UT, during the partial phase.",
		      hH, mM);
	      renderPangoText(scratchString,
			      OR_CREAM, SMALL_PANGO_FONT, &tWidth, &tHeight,
			      pixmap, displayWidth/2, textY,
			      0.0, TRUE, 1);
	    } else if (setsInPen) {
	      textY += 25;
	      sprintf(scratchString, "The moon sets at %02d:%02d, during the penumbral phase.",
		      hH, mM);
	      renderPangoText(scratchString,
			      OR_CREAM, SMALL_PANGO_FONT, &tWidth, &tHeight,
			      pixmap, displayWidth/2, textY,
			      0.0, TRUE, 1);
	    }
	  } else {
	    if (willSeeTot) {
	      renderPangoText("You can see the total eclipse.",
			      OR_CREAM, SMALL_PANGO_FONT, &tWidth, &tHeight,
			      pixmap, displayWidth/2, textY,
			      0.0, TRUE, 1);
	    } else if (willSeePar) {
	      renderPangoText("You can only see a partial eclipse.",
			      OR_CREAM, SMALL_PANGO_FONT, &tWidth, &tHeight,
			      pixmap, displayWidth/2, textY,
			      0.0, TRUE, 1);
	    } else {
	      renderPangoText("You can only see the penumbral phase.",
			      OR_CREAM, SMALL_PANGO_FONT, &tWidth, &tHeight,
			      pixmap, displayWidth/2, textY,
			      0.0, TRUE, 1);
	    }
	    if (risesInPen) {
	      textY += 25;
	      sprintf(scratchString, "The moon rises at %02d:%02d, during the penumbal phase.",
		      hH, mM);
	      renderPangoText(scratchString,
			      OR_CREAM, SMALL_PANGO_FONT, &tWidth, &tHeight,
			      pixmap, displayWidth/2, textY,
			      0.0, TRUE, 1);
	    } else if (risesInPar) {
	      textY += 25;
	      sprintf(scratchString, "The moon rises at %02d:%02d UT, during the partial phase.",
		      hH, mM);
	      renderPangoText(scratchString,
			      OR_CREAM, SMALL_PANGO_FONT, &tWidth, &tHeight,
			      pixmap, displayWidth/2, textY,
			      0.0, TRUE, 1);
	    } else if (risesInTot) {
	      textY += 25;
	      sprintf(scratchString, "The moon rises at %02d:%02d UT, during the total phase.",
		      hH, mM);
	      renderPangoText(scratchString,
			      OR_CREAM, SMALL_PANGO_FONT, &tWidth, &tHeight,
			      pixmap, displayWidth/2, textY,
			      0.0, TRUE, 1);
	    } else if (setsInTot) {
	      textY += 25;
	      sprintf(scratchString, "The moon sets at %02d:%02d UT, during the total phase.",
		      hH, mM);
	      renderPangoText(scratchString,
			      OR_CREAM, SMALL_PANGO_FONT, &tWidth, &tHeight,
			      pixmap, displayWidth/2, textY,
			      0.0, TRUE, 1);
	    } else if (setsInPar) {
	      textY += 25;
	      sprintf(scratchString, "The moon sets at %02d:%02d UT, during the partial phase.",
		      hH, mM);
	      renderPangoText(scratchString,
			      OR_CREAM, SMALL_PANGO_FONT, &tWidth, &tHeight,
			      pixmap, displayWidth/2, textY,
			      0.0, TRUE, 1);
	    } else if (setsInPen) {
	      textY += 25;
	      sprintf(scratchString, "The moon sets at %02d:%02d, during the penumbral phase.",
		      hH, mM);
	      renderPangoText(scratchString,
			      OR_CREAM, SMALL_PANGO_FONT, &tWidth, &tHeight,
			      pixmap, displayWidth/2, textY,
			      0.0, TRUE, 1);
	    }
	  }
	  if (iMinutesPen || iMinutesPar || iMinutesTot) {
	    char penPlur[2], parPlur[2], totPlur[2];

	    penPlur[0] = parPlur[0] = totPlur[0] = (char)0;
	    if (iMinutesPen != 1)
	      strcpy(penPlur, "s");
	    if (iMinutesPar != 1)
	      strcpy(parPlur, "s");
	    if (iMinutesTot != 1)
	      strcpy(totPlur, "s");

	    if (!iMinutesPen && !iMinutesPar && iMinutesTot)      /* 001 */
	      sprintf(scratchString, "You'll see %d minute%s of the total eclipse.",
		      iMinutesTot, totPlur);
	    else if (!iMinutesPen && iMinutesPar && !iMinutesTot) /* 010 */
	      sprintf(scratchString, "You'll see %d minute%s of the partial eclipse.",
		      iMinutesPar, parPlur);
	    else if (!iMinutesPen && iMinutesPar && iMinutesTot)  /* 011 */
	      sprintf(scratchString, "Visible for %d partial and %d total minutes.",
		      iMinutesPar, iMinutesTot);
	    else if (iMinutesPen && !iMinutesPar && !iMinutesTot) /* 100 */
	      sprintf(scratchString, "You'll see %d minute%s of the penumbral eclipse.",
		      iMinutesPen, penPlur);
	    else if (iMinutesPen && !iMinutesPar && iMinutesTot)  /* 101 */
	      sprintf(scratchString, "Visible for %d penumbral and %d total minutes.",
		      iMinutesPen, iMinutesTot);
	    else if (iMinutesPen && iMinutesPar && !iMinutesTot)  /* 110 */
	      sprintf(scratchString, "Visible for %d penumbral and %d partial minutes.",
		      iMinutesPen, iMinutesPar);
	    else                                                  /* 111 */
	      sprintf(scratchString, "Visible for penumbral: %d partial: %d total: %d minutes.",
		      iMinutesPen, iMinutesPar, iMinutesTot);
	    textY += 25;
	    renderPangoText(scratchString,
			    OR_CREAM, SMALL_PANGO_FONT, &tWidth, &tHeight,
			    pixmap, displayWidth/2, textY,
			    0.0, TRUE, 1);
	  }
	  textY += 40;
	  if (eclipseType == PENUMBRAL_LUNAR_ECLIPSE)
	    sprintf(scratchString,"Saros Number %d Penumbral Magnitude: %5.3f",
		    lunarEclipses[selectedLunarEclipse].sarosNum,
		    lunarEclipses[selectedLunarEclipse].penMag);
	  else
	    sprintf(scratchString,"Saros Number %d Umbral Magnitude: %5.3f",
		    lunarEclipses[selectedLunarEclipse].sarosNum,
		    lunarEclipses[selectedLunarEclipse].umbMag);
	  renderPangoText(scratchString,
			  OR_CREAM, SMALL_PANGO_FONT, &tWidth, &tHeight,
			  pixmap, displayWidth/2, textY,
			  0.0, TRUE, 1);
	  needNewTime = TRUE;
	}
	displayEclipse = FALSE;
      } else { /* Not displayEclipse */
	static int firstCall = TRUE;
	static GtkObject *startYearAdj, *endYearAdj;
	eclipseMenuItem *menuItem, *lastItem = NULL;

	/* Display the page to *select* an eclipse to display */
	if (firstCall) {
	  listStartYear = cYear;
	  listEndYear = listStartYear+10.0;
	  readEclipseData();
	  firstCall = FALSE;
	}
	lunarEclipseSelectionTable = gtk_table_new(5, 12, FALSE);
	
	lunarEclipseSelectionLabel = gtk_label_new("Select Lunar Eclipses to List");
	gtk_table_attach(GTK_TABLE(lunarEclipseSelectionTable), lunarEclipseSelectionLabel, 0, 4, 0, 1,
			 GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
	
	listLocalEclipsesOnlyButton =
	  (GtkCheckButton *)gtk_check_button_new_with_label("List only eclipses visible from here");
	gtk_toggle_button_set_active((GtkToggleButton *)listLocalEclipsesOnlyButton, listLocalEclipsesOnly);
	
	gtk_table_attach(GTK_TABLE(lunarEclipseSelectionTable), GTK_WIDGET(listLocalEclipsesOnlyButton),
			 0, 4, 1, 2, GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
	
	lunarEclipseSeparator1 = gtk_separator_menu_item_new();
	gtk_table_attach(GTK_TABLE(lunarEclipseSelectionTable), lunarEclipseSeparator1, 0, 4, 2, 3,
			 GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
	
	lunarEclipseTypeLabel = gtk_label_new("Types of eclipses to list");
	gtk_table_attach(GTK_TABLE(lunarEclipseSelectionTable), lunarEclipseTypeLabel, 0, 4, 3, 4,
			 GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
	listPenumbralEclipsesButton =
	  (GtkCheckButton *)gtk_check_button_new_with_label("Penumbral");
	gtk_toggle_button_set_active((GtkToggleButton *)listPenumbralEclipsesButton, listPenumbralEclipses);
	
	gtk_table_attach(GTK_TABLE(lunarEclipseSelectionTable), GTK_WIDGET(listPenumbralEclipsesButton),
			 0, 1, 4, 5, GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
	listPartialEclipsesButton =
	  (GtkCheckButton *)gtk_check_button_new_with_label("Partial");
	gtk_toggle_button_set_active((GtkToggleButton *)listPartialEclipsesButton, listPartialEclipses);
	
	gtk_table_attach(GTK_TABLE(lunarEclipseSelectionTable), GTK_WIDGET(listPartialEclipsesButton),
			 1, 2, 4, 5, GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
	listTotalEclipsesButton =
	  (GtkCheckButton *)gtk_check_button_new_with_label("Total");
	gtk_toggle_button_set_active((GtkToggleButton *)listTotalEclipsesButton, listTotalEclipses);
	
	gtk_table_attach(GTK_TABLE(lunarEclipseSelectionTable), GTK_WIDGET(listTotalEclipsesButton),
			 2, 3, 4, 5, GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
	
	lunarEclipseSeparator2 = gtk_separator_menu_item_new();
	gtk_table_attach(GTK_TABLE(lunarEclipseSelectionTable), lunarEclipseSeparator2, 0, 4, 5, 6,
			 GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
	
	lunarEclipseDateLabel = gtk_label_new("Eclipse list date range");
	gtk_table_attach(GTK_TABLE(lunarEclipseSelectionTable), lunarEclipseDateLabel, 0, 4, 6, 7,
			 GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
	lunarEclipseStartYearLabel = gtk_label_new("Start Year");
	gtk_table_attach(GTK_TABLE(lunarEclipseSelectionTable), lunarEclipseStartYearLabel, 0, 1, 7, 8,
			 GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
	startYearAdj = gtk_adjustment_new(listStartYear, -3000.0, 3000.0, 1.0, 1.0, 0.0);
	startYearSpin = gtk_spin_button_new((GtkAdjustment *)startYearAdj, 0.5, 0);
	gtk_table_attach(GTK_TABLE(lunarEclipseSelectionTable), startYearSpin, 1, 2, 7, 8,
			 GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
	endYearAdj = gtk_adjustment_new(listEndYear, -1999.0, 3000.0, 1.0, 1.0, 0.0);
	endYearSpin = gtk_spin_button_new((GtkAdjustment *)endYearAdj, 0.5, 0);
	gtk_table_attach(GTK_TABLE(lunarEclipseSelectionTable), endYearSpin, 1, 2, 7, 8,
			 GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
	lunarEclipseEndYearLabel = gtk_label_new("End Year");
	gtk_table_attach(GTK_TABLE(lunarEclipseSelectionTable), lunarEclipseEndYearLabel, 2, 3, 7, 8,
			 GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
	endYearAdj = gtk_adjustment_new(listEndYear, -1999.0, 3000.0, 1.0, 1.0, 0.0);
	endYearSpin = gtk_spin_button_new((GtkAdjustment *)endYearAdj, 0.5, 0);
	gtk_table_attach(GTK_TABLE(lunarEclipseSelectionTable), endYearSpin, 3, 4, 7, 8,
			 GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);

	rebuildEclipseListButton = gtk_button_new_with_label("Rebuild the Eclipse List");
	gtk_table_attach(GTK_TABLE(lunarEclipseSelectionTable), rebuildEclipseListButton, 0, 4, 8, 9,
			 GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
	g_signal_connect(G_OBJECT(rebuildEclipseListButton), "clicked",
			 G_CALLBACK(rebuildEclipseListCallback), NULL);

	needNewTime = FALSE;
	selectedLunarEclipse = -1;
	nEclipsesForMenu = 0;
	/* Make a list of eclipses to choose from */
	for (i = 0; i < N_LUNAR_ECLIPSES; i++) {
	  int yYYY, mM, dD;
	  double thisYear;
	  char dateString[23];
	  
	  yYYY = lunarEclipses[i].date/0x10000;
	  thisYear = (double)(yYYY);
	  if ((listStartYear <= thisYear) && (thisYear <= listEndYear)) {
	    int dTMinusUT, hH, minute, sS, j, displayable;
	    float f1;
	    double tDGE, eclipseUTHours, d1, d2, d3;
	    double eclipseTJD[N_ECLIPSE_TJDS], el[N_ECLIPSE_TJDS], rA, dec;
	    
	    mM          = (lunarEclipses[i].date & 0xff00)/0x100;
	    dD          =  lunarEclipses[i].date & 0xff;
	    eclipseType =  lunarEclipses[i].type1 & 0xf;
	    tDGE        =  lunarEclipses[i].tDGE;
	    dTMinusUT   =  lunarEclipses[i].dTMinusUT;
	    eclipseUTHours = (tDGE - dTMinusUT)/3600.0;
	    hH = (int)eclipseUTHours;
	    minute = (int)((eclipseUTHours - (double)hH) * 60.0);
	    sS = (int)((eclipseUTHours - (double)hH - ((double)minute)/60.0)*3600.0 + 0.5);
	    eclipseTJD[MID_ECLIPSE_TJD] = buildTJD(yYYY-1900, mM-1, dD, hH, minute, sS, 0);
	    eclipseTJD[PEN_ECLIPSE_START_TJD] =
	      eclipseTJD[MID_ECLIPSE_TJD] - lunarEclipses[i].penDur/2880.0;
	    eclipseTJD[PAR_ECLIPSE_START_TJD] =
	      eclipseTJD[MID_ECLIPSE_TJD] - lunarEclipses[i].parDur/2880.0;
	    eclipseTJD[TOT_ECLIPSE_START_TJD] =
	      eclipseTJD[MID_ECLIPSE_TJD] - lunarEclipses[i].totDur/2880.0;
	    eclipseTJD[PEN_ECLIPSE_END_TJD] =
	      eclipseTJD[MID_ECLIPSE_TJD] + lunarEclipses[i].penDur/2880.0;
	    eclipseTJD[PAR_ECLIPSE_END_TJD] =
	      eclipseTJD[MID_ECLIPSE_TJD] + lunarEclipses[i].parDur/2880.0;
	    eclipseTJD[TOT_ECLIPSE_END_TJD] =
	      eclipseTJD[MID_ECLIPSE_TJD] + lunarEclipses[i].totDur/2880.0;
	    if (listLocalEclipsesOnly)
	      displayable = FALSE;
	    else
	      displayable = TRUE;
	    for (j = 1; j < N_ECLIPSE_TJDS; j++) {
	      double az, zA;
	      
	      moonPosition(eclipseTJD[j], &rA, &dec, &d1, &d2, &d3, &f1);
	      myLST = lSTAtTJD(eclipseTJD[j]);
	      azZA(rA, sin(dec), cos(dec), &az, &zA, FALSE);
	      el[j] = M_HALF_PI - zA;
	      switch (j) {
	      case PEN_ECLIPSE_START_TJD:
	      case PEN_ECLIPSE_END_TJD:
		if (listPenumbralEclipses && (el[j] > 0.0))
		  displayable = TRUE;
		break;
	      case PAR_ECLIPSE_START_TJD:
	      case PAR_ECLIPSE_END_TJD:
		if ((eclipseType >= PARTIAL_LUNAR_ECLIPSE) && listPartialEclipses && (el[j] > 0.0))
		  displayable = TRUE;
		break;
	      case TOT_ECLIPSE_START_TJD:
	      case TOT_ECLIPSE_END_TJD:
		if ((eclipseType == TOTAL_LUNAR_ECLIPSE) && listTotalEclipses && (el[j] > 0.0))
		  displayable = TRUE;
		break;
	      }
	    }
	    
	    if ((((eclipseType == PENUMBRAL_LUNAR_ECLIPSE) && listPenumbralEclipses)
		 || ((eclipseType == PARTIAL_LUNAR_ECLIPSE)   && listPartialEclipses)
		 || ((eclipseType == TOTAL_LUNAR_ECLIPSE)     && listTotalEclipses))
		&& displayable) {
	      if (selectedLunarEclipse < 0)
		selectedLunarEclipse = i;
	      switch (eclipseType) {
	      case PENUMBRAL_LUNAR_ECLIPSE:
		sprintf(dateString, "%04d-%02d-%02d Penumbral", yYYY, mM, dD);
		break;
	      case PARTIAL_LUNAR_ECLIPSE:
		sprintf(dateString, "%04d-%02d-%02d Partial", yYYY, mM, dD);
		break;
	      default:
		sprintf(dateString, "%04d-%02d-%02d Total", yYYY, mM, dD);
	      }
	      nEclipsesForMenu++;
	      menuItem = (eclipseMenuItem *)malloc(sizeof(eclipseMenuItem));
	      if (menuItem == NULL) {
		perror("Lunar eclipse menu item");
		exit(ERROR_EXIT);
	      }
	      menuItem->next = NULL;
	      menuItem->key = i;
	      menuItem->name = malloc(strlen(dateString)+1);
	      if (menuItem->name == NULL) {
		perror("Lunar eclipse menu item name");
		exit(ERROR_EXIT);
	      }
	      strcpy(menuItem->name, dateString);
	      if (eclipseMenuItemRoot == NULL)
		eclipseMenuItemRoot = menuItem;
	      else
		lastItem->next = menuItem;
	      lastItem = menuItem;
	    }
	  }
	}
	if (eclipseMenuItemRoot != NULL) {
	  char *menuHeading = "/Click Here to Select the Eclipse to Display";
	  
	  lunarItemFactoryEntry = (GtkItemFactoryEntry *)malloc((1+nEclipsesForMenu)*
								sizeof(GtkItemFactoryEntry));
	  if (unlikely(lunarItemFactoryEntry == NULL)) {
	    perror("lunarItemFactoryEntry");
	    exit(ERROR_EXIT);
	  }
	  lunarItemFactoryEntry[0].path = (gchar *)malloc(strlen(menuHeading)+1);
	  if (unlikely(lunarItemFactoryEntry[0].path == NULL)) {
	    perror("lunarItemFactoryEntry[0].path");
	    exit(ERROR_EXIT);
	  }
	  strcpy(lunarItemFactoryEntry[0].path, menuHeading);
	  lunarItemFactoryEntry[0].accelerator = NULL;
	  lunarItemFactoryEntry[0].callback = NULL;
	  lunarItemFactoryEntry[0].callback_action = 0;
	  lunarItemFactoryEntry[0].extra_data = NULL;
	  lunarItemFactoryEntry[0].item_type = (gchar *)malloc(strlen("<Branch>")+1);
	  if (unlikely(lunarItemFactoryEntry[0].item_type == NULL)) {
	    perror("lunarItemFactoryEntry[0].item_type");
	    exit(ERROR_EXIT);
	  }
	  strcpy(lunarItemFactoryEntry[0].item_type, "<Branch>");
	  lastItem = eclipseMenuItemRoot;
	  for (i = 1; i <= nEclipsesForMenu; i++) {
	    char path[100];
	    
	    sprintf(path, "%s/%s", menuHeading, lastItem->name);
	    lunarItemFactoryEntry[i].path = (gchar *)malloc(strlen(path)+1);
	    if (unlikely(lunarItemFactoryEntry[i].path == NULL)) {
	      perror("lunatItemFactoryEntry[i].path");
	      exit(ERROR_EXIT);
	    }
	    strcpy(lunarItemFactoryEntry[i].path, path);
	    lunarItemFactoryEntry[i].accelerator = NULL;
	    lunarItemFactoryEntry[i].callback = lunarCategoryCallback;
	    lunarItemFactoryEntry[i].callback_action = lastItem->key;
	    lunarItemFactoryEntry[i].item_type = NULL;
	    lastItem = lastItem->next;
	  }
	  lunarAccelGroup = gtk_accel_group_new();
	  lunarItemFactory = gtk_item_factory_new(GTK_TYPE_MENU_BAR, "<OrreryLunarEclipses>",
						  lunarAccelGroup);
	  gtk_item_factory_create_items(lunarItemFactory, nEclipsesForMenu+1, lunarItemFactoryEntry,
					NULL);
	  lunarMenu = gtk_item_factory_get_widget(lunarItemFactory, "<OrreryLunarEclipses>");
	  gtk_table_attach(GTK_TABLE(lunarEclipseSelectionTable), (GtkWidget *)lunarMenu, 0, 4, 9, 10,
			   GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
	}
	needNewTime = TRUE;
	
	lunarEclipseStackable = hildon_stackable_window_new();
	g_signal_connect(G_OBJECT(lunarEclipseStackable), "destroy",
			 G_CALLBACK(checkLunarEclipseSettings), NULL);
	gtk_container_add(GTK_CONTAINER(lunarEclipseStackable), lunarEclipseSelectionTable);
	gtk_widget_show_all(lunarEclipseStackable);
      }
    }
    break;
  case PLANET_PHENOMENA:
    {
      int k0, kI, k, planet, year, month, day, dYear, dMonth, dDay, x, tWidth, tHeight, h;
      int nameMapping[7] = {1, 2, 5, 6, 7, 8, 9};
      double y, yearLength, jDE0, jDE, m, t;

      needNewTime = TRUE;
      lSTNow = lST();
      x = 11;
      tJDToDate(tJD, &year, &month, &day);
      sprintf(scratchString, "Planet Phenomena for %02d/%02d/%04d", month, day, year);
      renderPangoText(scratchString, OR_CREAM, MEDIUM_PANGO_FONT, &tWidth, &tHeight,
		      pixmap, displayWidth/2, x, 0.0, TRUE, 0);
      x += tHeight;
      renderPangoText("Last", OR_BLUE, MEDIUM_PANGO_FONT,
		      &tWidth, &tHeight, pixmap, displayWidth/3 + 25, x, 0.0, TRUE, 0);
      renderPangoText("Next", OR_BLUE, MEDIUM_PANGO_FONT,
		      &tWidth, &tHeight, pixmap, 2*displayWidth/3 + 58, x, 0.0, TRUE, 0);
      x += tHeight;
      if ((year %4) == 0) {
	yearLength = 366.0;
	if (month > 2)
	  day++;
      } else
	  yearLength = 365.0;
      while (month > 1) {
	month--;
	  day += monthLengths[month];
      }
      y = ((double)year) + ((double)(day-1))/yearLength;
      for (planet = 0; planet < 7; planet++) {
	renderPangoText(solarSystemNames[nameMapping[planet]], OR_CREAM, MEDIUM_PANGO_FONT,
			&tWidth, &tHeight, pixmap, 0, x, 0.0, FALSE, 0);
	x += tHeight;
	k0 = kI = (int)((365.2425*y + 1721060 - phenA[2*planet + 1])/phenB[planet]);
	k = k0 - 2;
	do {
	  jDE0 = phenA[2*planet + 1] + phenB[planet]*(double)k;
	  m = phenM0[2*planet + 1] + phenM1[planet]*(double)k;
	  t = (jDE0 - 2451545.0)/36525.0;
	  doubleNormalize0to360(&m);
	  jDE  = jDE0 + calcConjOrOp(planet, m, t, CONJUNCTION);
	  jDE -= deltaT(jDE)/86400.0;
	  k++;
	} while (jDE < tJD);
	tJDToDate(jDE, &dYear, &dMonth, &dDay);
	h = (int)(24.0*(jDE-buildTJD(dYear-1900, dMonth-1, dDay, 0, 0, 0, 0)) + 0.5);
	if (h >= 24)
	  fixDate(&dYear, &dMonth, &dDay, &h);
	if (planet > 1)
	  renderPangoText(" Conjunction", OR_BLUE, SMALL_PANGO_FONT,
			  &tWidth, &tHeight, pixmap, 0, x, 0.0, FALSE, 0);
	else
	  renderPangoText(" Inf. Conj.", OR_BLUE, SMALL_PANGO_FONT,
			  &tWidth, &tHeight, pixmap, 0, x, 0.0, FALSE, 0);
	sprintf(scratchString, "%02d/%02d/%02d    %02dh", dMonth, dDay, dYear % 100, h);
	renderPangoText(scratchString, OR_WHITE, SMALL_PANGO_FONT,
			&tWidth, &tHeight, pixmap, 2*displayWidth/3 -20, x, 0.0, FALSE, 0);
	k = k0 + 2;
	do {
	  jDE0 = phenA[2*planet + 1] + phenB[planet]*(double)k;
	  m = phenM0[2*planet + 1] + phenM1[planet]*(double)k;
	  t = (jDE0 - 2451545.0)/36525.0;
	  doubleNormalize0to360(&m);
	  jDE  = jDE0 + calcConjOrOp(planet, m, t, CONJUNCTION);
	  jDE -= deltaT(jDE)/86400.0;
	  k--;
	} while (jDE > tJD);
	tJDToDate(jDE, &dYear, &dMonth, &dDay);
	h = (int)(24.0*(jDE-buildTJD(dYear-1900, dMonth-1, dDay, 0, 0, 0, 0)) + 0.5);
	if (h >= 24)
	  fixDate(&dYear, &dMonth, &dDay, &h);
	sprintf(scratchString, "%02d/%02d/%02d    %02dh", dMonth, dDay, dYear % 100, h);
	renderPangoText(scratchString, OR_WHITE, SMALL_PANGO_FONT,
			&tWidth, &tHeight, pixmap, displayWidth/3 - 45, x, 0.0, FALSE, 0);
	x += tHeight;
	if (planet < 2) {
	  int angle;

	  renderPangoText(" Max. West", OR_BLUE, SMALL_PANGO_FONT,
			  &tWidth, &tHeight, pixmap, 0, x, 0.0, FALSE, 0);
	  k = k0 - 2;
	  do {
	    jDE0 = phenA[2*planet + 1] + phenB[planet]*(double)k;
	    m = phenM0[2*planet + 1] + phenM1[planet]*(double)k;
	    t = (jDE0 - 2451545.0)/36525.0;
	    doubleNormalize0to360(&m);
	    jDE  = jDE0 + calcConjOrOp(planet, m, t, GREATEST_WEST_TIME);
	    jDE -= deltaT(jDE)/86400.0;
	    angle = (int)(calcConjOrOp(planet, m, t, GREATEST_WEST_ANGLE) + 0.5);
	    k++;
	  } while (jDE < tJD);
	  tJDToDate(jDE, &dYear, &dMonth, &dDay);
	  h = (int)(24.0*(jDE-buildTJD(dYear-1900, dMonth-1, dDay, 0, 0, 0, 0)) + 0.5);
	  if (h >= 24)
	    fixDate(&dYear, &dMonth, &dDay, &h);
	  sprintf(scratchString, "%02d/%02d/%02d    %02dh   %02d",
		  dMonth, dDay, dYear % 100, h, angle);
	  renderPangoText(scratchString, OR_WHITE, SMALL_PANGO_FONT,
			  &tWidth, &tHeight, pixmap, 2*displayWidth/3 - 20, x, 0.0, FALSE, 0);
	  renderPangoText("o", OR_WHITE, SMALL_PANGO_FONT,
			  &tWidth, &tHeight, pixmap, 2*displayWidth/3 + tWidth - 20,
			  x - tHeight/3, 0.0, FALSE, 0);
	  k = k0 + 2;
	  do {
	    jDE0 = phenA[2*planet + 1] + phenB[planet]*(double)k;
	    m = phenM0[2*planet + 1] + phenM1[planet]*(double)k;
	    t = (jDE0 - 2451545.0)/36525.0;
	    doubleNormalize0to360(&m);
	    jDE  = jDE0 + calcConjOrOp(planet, m, t, GREATEST_WEST_TIME);
	    jDE -= deltaT(jDE)/86400.0;
	    angle = (int)(calcConjOrOp(planet, m, t, GREATEST_WEST_ANGLE) + 0.5);
	    k--;
	  } while (jDE > tJD);
	  tJDToDate(jDE, &dYear, &dMonth, &dDay);
	  h = (int)(24.0*(jDE-buildTJD(dYear-1900, dMonth-1, dDay, 0, 0, 0, 0)) + 0.5);
	  if (h >= 24)
	    fixDate(&dYear, &dMonth, &dDay, &h);
	  sprintf(scratchString, "%02d/%02d/%02d    %02dh   %02d",
		  dMonth, dDay, dYear % 100, h, angle);
	  renderPangoText(scratchString, OR_WHITE, SMALL_PANGO_FONT,
			  &tWidth, &tHeight, pixmap, displayWidth/3 - 45, x, 0.0, FALSE, 0);
	  renderPangoText("o", OR_WHITE, SMALL_PANGO_FONT,
			  &tWidth, &tHeight, pixmap, displayWidth/3 - 45 + tWidth,
			  x - tHeight/3, 0.0, FALSE, 0);
	  x += tHeight;
	}

	k0 = (int)((365.2425*y + 1721060 - phenA[2*planet])/phenB[planet]);
	k = k0 - 2;
	do {
	  jDE0 = phenA[2*planet] + phenB[planet]*(double)k;
	  m = phenM0[2*planet] + phenM1[planet]*(double)k;
	  t = (jDE0 - 2451545.0)/36525.0;
	  doubleNormalize0to360(&m);
	  jDE  = jDE0 + calcConjOrOp(planet, m, t, OPPOSITION);
	  jDE -= deltaT(jDE)/86400.0;
	  k++;
	} while (jDE < tJD);
	tJDToDate(jDE, &dYear, &dMonth, &dDay);
	h = (int)(24.0*(jDE-buildTJD(dYear-1900, dMonth-1, dDay, 0, 0, 0, 0)) + 0.5);
	if (h >= 24)
	  fixDate(&dYear, &dMonth, &dDay, &h);
	if (planet > 1)
	  renderPangoText(" Opposition", OR_BLUE, SMALL_PANGO_FONT,
			  &tWidth, &tHeight, pixmap, 0, x, 0.0, FALSE, 0);
	else
	  renderPangoText(" Sup. Conj.", OR_BLUE, SMALL_PANGO_FONT,
			  &tWidth, &tHeight, pixmap, 0, x, 0.0, FALSE, 0);
	sprintf(scratchString, "%02d/%02d/%02d    %02dh", dMonth, dDay, dYear % 100, h);
	renderPangoText(scratchString, OR_WHITE, SMALL_PANGO_FONT,
			&tWidth, &tHeight, pixmap, 2*displayWidth/3 - 20, x, 0.0, FALSE, 0);
	k = k0 + 2;
	do {
	  jDE0 = phenA[2*planet] + phenB[planet]*(double)k;
	  m = phenM0[2*planet] + phenM1[planet]*(double)k;
	  t = (jDE0 - 2451545.0)/36525.0;
	  doubleNormalize0to360(&m);
	  jDE  = jDE0 + calcConjOrOp(planet, m, t, OPPOSITION);
	  jDE -= deltaT(jDE)/86400.0;
	  k--;
	} while (jDE > tJD);
	tJDToDate(jDE, &dYear, &dMonth, &dDay);
	h = (int)(24.0*(jDE-buildTJD(dYear-1900, dMonth-1, dDay, 0, 0, 0, 0)) + 0.5);
	if (h >= 24)
	  fixDate(&dYear, &dMonth, &dDay, &h);
	sprintf(scratchString, "%02d/%02d/%02d    %02dh", dMonth, dDay, dYear % 100, h);
	renderPangoText(scratchString, OR_WHITE, SMALL_PANGO_FONT,
			&tWidth, &tHeight, pixmap, displayWidth/3 - 45, x, 0.0, FALSE, 0);
	x += tHeight;
	if (planet < 2) {
	  int angle;

	  renderPangoText(" Max. East", OR_BLUE, SMALL_PANGO_FONT,
			  &tWidth, &tHeight, pixmap, 0, x, 0.0, FALSE, 0);
	  k = kI - 2;
	  do {
	    jDE0 = phenA[2*planet + 1] + phenB[planet]*(double)k;
	    m = phenM0[2*planet + 1] + phenM1[planet]*(double)k;
	    t = (jDE0 - 2451545.0)/36525.0;
	    doubleNormalize0to360(&m);
	    jDE  = jDE0 + calcConjOrOp(planet, m, t, GREATEST_EAST_TIME);
	    jDE -= deltaT(jDE)/86400.0;
	    angle = (int)(calcConjOrOp(planet, m, t, GREATEST_EAST_ANGLE) + 0.5);
	    k++;
	  } while (jDE < tJD);
	  tJDToDate(jDE, &dYear, &dMonth, &dDay);
	  h = (int)(24.0*(jDE-buildTJD(dYear-1900, dMonth-1, dDay, 0, 0, 0, 0)) + 0.5);
	  if (h >= 24)
	    fixDate(&dYear, &dMonth, &dDay, &h);
	  sprintf(scratchString, "%02d/%02d/%02d    %02dh   %02d",
		  dMonth, dDay, dYear % 100, h, angle);
	  renderPangoText(scratchString, OR_WHITE, SMALL_PANGO_FONT,
			  &tWidth, &tHeight, pixmap, 2*displayWidth/3 - 20, x, 0.0, FALSE, 0);
	  renderPangoText("o", OR_WHITE, SMALL_PANGO_FONT,
			  &tWidth, &tHeight, pixmap, 2*displayWidth/3 + tWidth - 20,
			  x - tHeight/3, 0.0, FALSE, 0);
	  k = kI + 2;
	  do {
	    jDE0 = phenA[2*planet + 1] + phenB[planet]*(double)k;
	    m = phenM0[2*planet + 1] + phenM1[planet]*(double)k;
	    t = (jDE0 - 2451545.0)/36525.0;
	    doubleNormalize0to360(&m);
	    jDE  = jDE0 + calcConjOrOp(planet, m, t, GREATEST_EAST_TIME);
	    jDE -= deltaT(jDE)/86400.0;
	    angle = (int)(calcConjOrOp(planet, m, t, GREATEST_EAST_ANGLE) + 0.5);
	    k--;
	  } while (jDE > tJD);
	  tJDToDate(jDE, &dYear, &dMonth, &dDay);
	  h = (int)(24.0*(jDE-buildTJD(dYear-1900, dMonth-1, dDay, 0, 0, 0, 0)) + 0.5);
	  if (h >= 24)
	    fixDate(&dYear, &dMonth, &dDay, &h);
	  sprintf(scratchString, "%02d/%02d/%02d    %02dh   %02d",
		  dMonth, dDay, dYear % 100, h, angle);
	  renderPangoText(scratchString, OR_WHITE, SMALL_PANGO_FONT,
			  &tWidth, &tHeight, pixmap, displayWidth/3 - 45, x, 0.0, FALSE, 0);
	  renderPangoText("o", OR_WHITE, SMALL_PANGO_FONT,
			  &tWidth, &tHeight, pixmap, displayWidth/3 - 45 + tWidth,
			  x - tHeight/3, 0.0, FALSE, 0);
	  x += tHeight + 12;
	} else
	  x += 12;
      }
    }
    break;
  case PLANET_ELEVATION_SCREEN:
    /*
      Draw a screen that shows the elevation of the Sun, Moon and each planet as a
      function of time, for the current day.
    */
    {
      int plotWidth, circumpolar, planet, plotHeight, planetRowInc, xNow;
      int plotOrder[9] = {EARTH, MOON, MERCURY, VENUS, MARS, JUPITER, SATURN, URANUS, NEPTUNE};
      int year, month, day, hh, mm, lineColor, i;
      int color = OR_GREEN;
      int xSunrise, xSunset;
      float labelTime, deltaTime, dummyF, localMeanTime;
      double uT, h0, riseTime, setTime, dummyD, tJDMidnight, rawRiseTime, rawSetTime;
      double rA, dec, az, zA;
      GdkPoint box[4];

      needNewTime = TRUE;
      uT = (tJD - (double)((int)tJD) - 0.5) * 24.0;
      doubleNormalize0to24(&uT);
      lSTNow = lST();
      deltaTime = lSTNow/HOURS_TO_RADIANS - uT;
      /* Draw bounding box */
      box[0].x = PES_LEFT_BORDER;                 box[0].y = PES_TOP_BORDER;
      box[1].x = displayWidth - PES_RIGHT_BORDER; box[1].y = box[0].y;
      box[2].x = box[1].x;                        box[2].y = displayHeight - PES_BOTTOM_BORDER;
      box[3].x = PES_LEFT_BORDER;                 box[3].y = box[2].y;
      gdk_draw_polygon(pixmap, gC[OR_BLUE], FALSE, box, 4);
      tJDToDate(tJD, &year, &month, &day);
      sprintf(scratchString, "Planet Elevations for %02d/%02d/%04d", month, day, year);
      renderPangoText(scratchString, OR_CREAM, MEDIUM_PANGO_FONT, &tWidth, &tHeight,
		      pixmap, displayWidth/2, 11, 0.0, TRUE, 0);
      renderPangoText("Universal Time", OR_CREAM, SMALL_PANGO_FONT, &tWidth, &tHeight,
		      pixmap, displayWidth/2, 40, 0.0, TRUE, 0);
      renderPangoText("Local Sidereal Time", OR_CREAM, SMALL_PANGO_FONT, &tWidth, &tHeight,
		      pixmap, displayWidth/2, displayHeight - 9, 0.0, TRUE, 0);
      plotWidth = displayWidth - PES_LEFT_BORDER - PES_RIGHT_BORDER;
      plotHeight = displayHeight - PES_BOTTOM_BORDER - PES_TOP_BORDER;
      planetRowInc = plotHeight / 9 - 2;
      gdk_draw_line(pixmap, gC[OR_DARK_GREY], PES_LEFT_BORDER + plotWidth/2, PES_TOP_BORDER,
		    PES_LEFT_BORDER + plotWidth/2, displayHeight - PES_BOTTOM_BORDER);
      localMeanTime = 12.0 + uT + longitude/HOURS_TO_RADIANS;
      floatNormalize0to24(&localMeanTime);
      xNow = (int)(localMeanTime/24.0 * (float)plotWidth) + PES_LEFT_BORDER;
      planetInfo(dataDir, EARTH, tJD, &rA, &dec, &dummyF, &dummyF);
      azZA(rA, sin(dec), cos(dec), &az, &zA, FALSE);
      if (zA > M_HALF_PI - CIVIL_TWILIGHT)
	lineColor = OR_GREEN;
      else if (zA > M_HALF_PI)
	lineColor = OR_YELLOW;
      else
	lineColor = OR_RED;
      sprintf(scratchString, "%02d:%02d", gMT->tm_hour, gMT->tm_min);
      renderPangoText(scratchString, lineColor, SMALL_PANGO_FONT,
		    &tWidth, &tHeight, pixmap, xNow, PES_TOP_BORDER - 30, 0.0, TRUE, 0);
      if ((xNow - tWidth/2) < PES_LEFT_BORDER)
	renderPangoText(scratchString, lineColor, SMALL_PANGO_FONT,
			&tWidth, &tHeight, pixmap, tWidth/2, PES_TOP_BORDER - 30, 0.0, TRUE, 0);
      else if ((xNow + tWidth/2) > (displayWidth-PES_RIGHT_BORDER))
	renderPangoText(scratchString, lineColor, SMALL_PANGO_FONT,
			&tWidth, &tHeight, pixmap, displayWidth - tWidth/2,
			PES_TOP_BORDER - 30, 0.0, TRUE, 0);
      dummyF = lST()/HOURS_TO_RADIANS;
      hh = (int)dummyF;
      mm = (int)((dummyF - (float)hh)*60.0);
      sprintf(scratchString, "%02d:%02d", hh, mm);
      renderPangoText(scratchString, lineColor, SMALL_PANGO_FONT, &tWidth,
		      &tHeight, pixmap, xNow, displayHeight - PES_BOTTOM_BORDER + 30, 0.0, TRUE, 0);
      if ((xNow - tWidth/2) < PES_LEFT_BORDER)
	renderPangoText(scratchString, lineColor, SMALL_PANGO_FONT, &tWidth, &tHeight,
			pixmap, tWidth/2, displayHeight - PES_BOTTOM_BORDER + 30, 0.0, TRUE, 0);
      else if ((xNow + tWidth/2) > (displayWidth-PES_RIGHT_BORDER))
	renderPangoText(scratchString, lineColor, SMALL_PANGO_FONT,
			&tWidth, &tHeight, pixmap, displayWidth - tWidth/2,
			displayHeight - PES_BOTTOM_BORDER + 30, 0.0, TRUE, 0);
      gdk_draw_line(pixmap, gC[lineColor], xNow, PES_TOP_BORDER - 20, xNow, displayHeight - PES_BOTTOM_BORDER + 20);
      /* Draw the top scale (UT) */
      for (labelTime = 0; labelTime < 24.0; labelTime += 1.0) {
	int x, tickLength, timeColor;

	if ((int)(labelTime+0.5) % 6 == 0) {
	  tickLength = 10;
	  timeColor = OR_WHITE;
	} else if ((int)(labelTime+0.5) % 3 == 0) {
	  tickLength = 7;
	  timeColor = OR_BLUE;
	} else {
	  tickLength = 5;
	  timeColor = OR_BLUE;
	}
	localMeanTime = 12.0 + labelTime + longitude/HOURS_TO_RADIANS;
	floatNormalize0to24(&localMeanTime);
	x = (int)(localMeanTime/24.0 * (float)plotWidth) + PES_LEFT_BORDER;
	gdk_draw_line(pixmap, gC[OR_BLUE], x, PES_TOP_BORDER, x, PES_TOP_BORDER + tickLength);
	sprintf(scratchString, "%d", (int)labelTime);
	stringWidth = gdk_string_width(smallFont, scratchString);
	gdk_draw_string(pixmap, smallFont, gC[timeColor], x - stringWidth/2, PES_TOP_BORDER - 4, scratchString);
      }
      /* Draw the bottom scale (LST) */
      for (labelTime = 0; labelTime < 24.0; labelTime += 1.0) {
	int x, tickLength, timeColor;

	if ((int)(labelTime+0.5) % 6 == 0) {
	  tickLength = 10;
	  timeColor = OR_WHITE;
	} else if ((int)(labelTime+0.5) % 3 == 0) {
	  tickLength = 7;
	  timeColor = OR_BLUE;
	} else {
	  tickLength = 5;
	  timeColor = OR_BLUE;
	}
	localMeanTime = 12.0 - deltaTime + labelTime + longitude/HOURS_TO_RADIANS;
	floatNormalize0to24(&localMeanTime);
	x = (int)(localMeanTime/24.0 * (float)plotWidth) + PES_LEFT_BORDER;
	gdk_draw_line(pixmap, gC[OR_BLUE], x, displayHeight - PES_BOTTOM_BORDER, x,
		      displayHeight - PES_BOTTOM_BORDER - tickLength);
	sprintf(scratchString, "%d", (int)labelTime);
	stringWidth = gdk_string_width(smallFont, scratchString);
	gdk_draw_string(pixmap, smallFont, gC[timeColor], x - stringWidth/2,
			displayHeight - PES_BOTTOM_BORDER + 12, scratchString);
      }
      h0 = -(50.0/60.0)*DEGREES_TO_RADIANS;
      lSTNow = lST();
      tJDMidnight = (double)((int)(tJD + longitude/M_2PI)) + 0.5 - longitude/M_2PI;
      lSTNow = lSTAtTJD(tJDMidnight);
      xSunrise = xSunset = -100;
      for (i = 0; i < 9; i++) {
	int xRise, xSet, xTransit, planetIndex, planetNameIndex, transitEl, planetNameColor;
	float localRiseTime, localSetTime, elevation;
	double transitTime, transitElD;

	planet = plotOrder[i];
	transitTime = calcTransitTime(tJD, planet, &transitElD);
	transitEl = (int)(transitElD/DEGREES_TO_RADIANS + 0.5);
	h0 = -(34.0/60.0)*DEGREES_TO_RADIANS;
	if (planet == EARTH)
	  planetNameIndex = 0;
	else
	  planetNameIndex = planet;
	switch (planet) {
	case EARTH:
	  h0 = -(50.0/60.0)*DEGREES_TO_RADIANS;
	  planetIndex = 0; break;
	case MOON:
	  h0 = 0.125*DEGREES_TO_RADIANS;
	  planetIndex = 1; break;
	case MERCURY:
	case VENUS:
	  planetIndex = planet + 1; break;
	default:
	  planetIndex = planet - 1;
	}
	rawRiseTime = calcRiseOrSetTime(TRUE, planet, tJDMidnight, lSTNow, h0, &circumpolar,
					&dummyD, &dummyD, &dummyD, &dummyF);
	if (!circumpolar) {
	  double currentEl;

	  riseTime = (rawRiseTime - (double)((int)rawRiseTime) - 0.5) * 24.0;
	  doubleNormalize0to24(&riseTime);
	  rawSetTime = calcRiseOrSetTime(FALSE, planet, tJDMidnight, lSTNow, h0, &circumpolar,
					 &dummyD, &dummyD, &dummyD, &dummyF);
	  planetInfo(dataDir, planet, tJD, &rA, &dec, &dummyF, &dummyF);
	  setTime = (rawSetTime - (double)((int)rawSetTime) - 0.5) * 24.0;
	  doubleNormalize0to24(&setTime);
	  localRiseTime = 12.0 + riseTime + longitude/HOURS_TO_RADIANS;
	  floatNormalize0to24(&localRiseTime);
	  xRise = (int)(localRiseTime/24.0 * (float)plotWidth) + PES_LEFT_BORDER;
	  localSetTime = 12.0 + setTime + longitude/HOURS_TO_RADIANS;
	  floatNormalize0to24(&localSetTime);
	  xSet = (int)(localSetTime/24.0 * (float)plotWidth) + PES_LEFT_BORDER;
	  xTransit = (xRise + xSet)/2;
	  azZA(rA, sin(dec), cos(dec), &az, &zA, FALSE);
	  currentEl = M_HALF_PI - zA;
	  if (currentEl > 0.0) {
	    int yNow;

	    planetNameColor = OR_GREEN;
	    sprintf(scratchString, "%2.0f", currentEl/DEGREES_TO_RADIANS);
	    yNow = PES_TOP_BORDER + PES_PLANET_BASE + 27 + planetIndex*planetRowInc;
	    gdk_draw_line(pixmap, gC[OR_BLACK], xNow, yNow - 12, xNow, yNow + 2);
	    drawBoundedString(lineColor, xNow, yNow, scratchString);
	  } else
	    planetNameColor = OR_RED;
	  if (xRise < xSet) {
	    /*
	      The object is not up at local noon - draw just one line.
	      The line will be grey for the portion of the planet's track which
	      is during daylight, and white for the portion of the track during
	      nighttime, so the line may be drawn with as many as three segments.
	    */
	    if ((xRise < xSunset) && (xSet < xSunset)) {
	      gdk_draw_line(pixmap, gC[OR_GREY], xRise, PES_TOP_BORDER + PES_PLANET_BASE + planetIndex*planetRowInc,
			    xSet, PES_TOP_BORDER + PES_PLANET_BASE + planetIndex*planetRowInc);
	    } else if ((xRise < xSunset) && (xSet >= xSunset) && (xSet < xSunrise)) {
	      gdk_draw_line(pixmap, gC[OR_GREY], xRise, PES_TOP_BORDER + PES_PLANET_BASE + planetIndex*planetRowInc,
			    xSunset, PES_TOP_BORDER + PES_PLANET_BASE + planetIndex*planetRowInc);
	      gdk_draw_line(pixmap, gC[OR_WHITE], xSunset, PES_TOP_BORDER + PES_PLANET_BASE + planetIndex*planetRowInc,
			    xSet, PES_TOP_BORDER + PES_PLANET_BASE + planetIndex*planetRowInc);
	    } else if ((xRise < xSunset) && (xSet >= xSunrise)) {
	      gdk_draw_line(pixmap, gC[OR_GREY], xRise, PES_TOP_BORDER + PES_PLANET_BASE + planetIndex*planetRowInc,
			    xSunset, PES_TOP_BORDER + PES_PLANET_BASE + planetIndex*planetRowInc);
	      gdk_draw_line(pixmap, gC[OR_WHITE], xSunset, PES_TOP_BORDER + PES_PLANET_BASE + planetIndex*planetRowInc,
			    xSunrise, PES_TOP_BORDER + PES_PLANET_BASE + planetIndex*planetRowInc);
	      gdk_draw_line(pixmap, gC[OR_GREY], xSunrise, PES_TOP_BORDER + PES_PLANET_BASE + planetIndex*planetRowInc,
			    xSet, PES_TOP_BORDER + PES_PLANET_BASE + planetIndex*planetRowInc);
	    } else if ((xRise >= xSunset) && (xSet <= xSunrise)) {
	      gdk_draw_line(pixmap, gC[OR_WHITE], xRise, PES_TOP_BORDER + PES_PLANET_BASE + planetIndex*planetRowInc,
			    xSet, PES_TOP_BORDER + PES_PLANET_BASE + planetIndex*planetRowInc);
	    } else if ((xRise >= xSunset) && (xSet > xSunset)) {
	      gdk_draw_line(pixmap, gC[OR_WHITE], xRise, PES_TOP_BORDER + PES_PLANET_BASE + planetIndex*planetRowInc,
			    xSunrise, PES_TOP_BORDER + PES_PLANET_BASE + planetIndex*planetRowInc);
	      gdk_draw_line(pixmap, gC[OR_GREY], xSunrise, PES_TOP_BORDER + PES_PLANET_BASE + planetIndex*planetRowInc,
			    xSet, PES_TOP_BORDER + PES_PLANET_BASE + planetIndex*planetRowInc);
	    } else {
	      gdk_draw_line(pixmap, gC[OR_GREY], xRise, PES_TOP_BORDER + PES_PLANET_BASE + planetIndex*planetRowInc,
			    xSet, PES_TOP_BORDER + PES_PLANET_BASE + planetIndex*planetRowInc);
	    }
	    xTransit = (xRise + xSet)/2;
	    if (planet == EARTH) 
	      color = OR_RED;
	    else if ((xTransit > xSunset) && (xTransit < xSunrise))
	      color = OR_WHITE;
	    else
	      color = OR_GREY;
	    gdk_draw_line(pixmap, gC[color], xTransit, PES_TOP_BORDER + PES_PLANET_BASE + planetIndex*planetRowInc,
			  xTransit, PES_TOP_BORDER + PES_PLANET_BASE+15 + planetIndex*planetRowInc);
	    sprintf(scratchString, "%d", transitEl);
	    stringWidth = gdk_string_width(smallFont, scratchString);
	    if (abs(xNow - xTransit) > stringWidth)
	      drawBoundedString(color, xTransit,
				PES_TOP_BORDER + PES_PLANET_BASE+27 + planetIndex*planetRowInc, scratchString);
	    renderPangoText(solarSystemNames[planetNameIndex], planetNameColor, SMALL_PANGO_FONT, &tWidth, &tHeight,
			    pixmap, xTransit, PES_TOP_BORDER + PES_PLANET_BASE-16 + planetIndex*planetRowInc,
			    0.0, TRUE, 0);
	  } else {
	    /* The object is up at local noon, two lines are needed */
	    float transitTime;

	    if (planet == EARTH) {
	      gdk_draw_line(pixmap, gC[OR_RED],
			    PES_LEFT_BORDER, PES_TOP_BORDER + PES_PLANET_BASE + planetIndex*planetRowInc,
			    xSet, PES_TOP_BORDER + PES_PLANET_BASE + planetIndex*planetRowInc);
	      gdk_draw_line(pixmap, gC[OR_RED], xRise, PES_TOP_BORDER + PES_PLANET_BASE + planetIndex*planetRowInc,
			    displayWidth - PES_RIGHT_BORDER,
			    PES_TOP_BORDER + PES_PLANET_BASE + planetIndex*planetRowInc);
	    } else {
	      if (xSet < xSunset) {
		gdk_draw_line(pixmap, gC[OR_GREY],
			      PES_LEFT_BORDER, PES_TOP_BORDER + PES_PLANET_BASE + planetIndex*planetRowInc,
			      xSet, PES_TOP_BORDER + PES_PLANET_BASE + planetIndex*planetRowInc);
	      } else if (xSet < xSunrise) {
		gdk_draw_line(pixmap, gC[OR_GREY],
			      PES_LEFT_BORDER, PES_TOP_BORDER + PES_PLANET_BASE + planetIndex*planetRowInc,
			      xSunset, PES_TOP_BORDER + PES_PLANET_BASE + planetIndex*planetRowInc);
		gdk_draw_line(pixmap, gC[OR_WHITE],
			      xSunset, PES_TOP_BORDER + PES_PLANET_BASE + planetIndex*planetRowInc,
			      xSet, PES_TOP_BORDER + PES_PLANET_BASE + planetIndex*planetRowInc);
	      } else {
		gdk_draw_line(pixmap, gC[OR_GREY],
			      PES_LEFT_BORDER, PES_TOP_BORDER + PES_PLANET_BASE + planetIndex*planetRowInc,
			      xSunset, PES_TOP_BORDER + PES_PLANET_BASE + planetIndex*planetRowInc);
		gdk_draw_line(pixmap, gC[OR_WHITE],
			      xSunset, PES_TOP_BORDER + PES_PLANET_BASE + planetIndex*planetRowInc,
			      xSunrise, PES_TOP_BORDER + PES_PLANET_BASE + planetIndex*planetRowInc);
		gdk_draw_line(pixmap, gC[OR_GREY],
			      xSunrise, PES_TOP_BORDER + PES_PLANET_BASE + planetIndex*planetRowInc,
			      xSet, PES_TOP_BORDER + PES_PLANET_BASE + planetIndex*planetRowInc);
	      }
	      
	      if (xRise > xSunrise) {
		gdk_draw_line(pixmap, gC[OR_GREY], xRise, PES_TOP_BORDER + PES_PLANET_BASE + planetIndex*planetRowInc,
			      displayWidth - PES_RIGHT_BORDER,
			      PES_TOP_BORDER + PES_PLANET_BASE + planetIndex*planetRowInc);
	      } else if (xRise > xSunset) {
		gdk_draw_line(pixmap, gC[OR_GREY], xSunrise,
			      PES_TOP_BORDER + PES_PLANET_BASE + planetIndex*planetRowInc,
			      displayWidth - PES_RIGHT_BORDER,
			      PES_TOP_BORDER + PES_PLANET_BASE + planetIndex*planetRowInc);
		gdk_draw_line(pixmap, gC[OR_WHITE], xRise, PES_TOP_BORDER + PES_PLANET_BASE + planetIndex*planetRowInc,
			      xSunrise, PES_TOP_BORDER + PES_PLANET_BASE + planetIndex*planetRowInc);
	      } else {
		gdk_draw_line(pixmap, gC[OR_GREY], xSunrise,
			      PES_TOP_BORDER + PES_PLANET_BASE + planetIndex*planetRowInc,
			      displayWidth - PES_RIGHT_BORDER,
			      PES_TOP_BORDER + PES_PLANET_BASE + planetIndex*planetRowInc);
		gdk_draw_line(pixmap, gC[OR_WHITE], xSunrise,
			      PES_TOP_BORDER + PES_PLANET_BASE + planetIndex*planetRowInc,
			      xSunset, PES_TOP_BORDER + PES_PLANET_BASE + planetIndex*planetRowInc);
		gdk_draw_line(pixmap, gC[OR_GREY], xRise, PES_TOP_BORDER + PES_PLANET_BASE + planetIndex*planetRowInc,
			      xSunrise, PES_TOP_BORDER + PES_PLANET_BASE + planetIndex*planetRowInc);
	      }
	    }
	    if (setTime < riseTime)
	      transitTime = (riseTime + setTime)/2.0 + longitude/HOURS_TO_RADIANS;
	    else
	      transitTime = (riseTime + setTime - 24.0)/2.0 + longitude/HOURS_TO_RADIANS;
	    floatNormalize0to24(&transitTime);
	    xTransit =  (int)(transitTime/24.0 * (float)plotWidth) + PES_LEFT_BORDER;
	    if (planet == EARTH)
	      color = OR_RED;
	    else if ((xTransit > xSunset) && (xTransit < xSunrise))
	      color = OR_WHITE;
	    else
	      color = OR_GREY;
	    gdk_draw_line(pixmap, gC[color], xTransit, PES_TOP_BORDER + PES_PLANET_BASE + planetIndex*planetRowInc,
			  xTransit, PES_TOP_BORDER + PES_PLANET_BASE+15 + planetIndex*planetRowInc);
	    sprintf(scratchString, "%d", transitEl);
	    stringWidth = gdk_string_width(smallFont, scratchString);
	    if (abs(xNow - xTransit) > stringWidth)
	      drawBoundedString(color, xTransit,
				PES_TOP_BORDER + PES_PLANET_BASE+27 + planetIndex*planetRowInc, scratchString);
	    renderPangoText(solarSystemNames[planetNameIndex], planetNameColor,
			    SMALL_PANGO_FONT, &tWidth, &tHeight, pixmap,
			    PES_LEFT_BORDER + 45, PES_TOP_BORDER + PES_PLANET_BASE-16 + planetIndex*planetRowInc,
			    0.0, TRUE, 0);
	    renderPangoText(solarSystemNames[planetNameIndex], planetNameColor, SMALL_PANGO_FONT, &tWidth, &tHeight,
			    pixmap, displayWidth - PES_RIGHT_BORDER - 45,
			    PES_TOP_BORDER + PES_PLANET_BASE-16 + planetIndex*planetRowInc, 0.0, TRUE, 0);
	    if (planet == EARTH) {
	      xSunrise = xRise; xSunset = xSet;
	      if ((xSunset > 60) && (abs(xNow - xSunset) > 60)) {
		renderPangoText("Sunset", OR_GREY, SMALL_PANGO_FONT, &tWidth, &tHeight,
			    pixmap, xSunset, displayHeight - PES_BOTTOM_BORDER + 30, 0.0, TRUE, 0);
		gdk_draw_line(pixmap, gC[OR_GREY], xSet, PES_TOP_BORDER, xSet, displayHeight - PES_BOTTOM_BORDER + 20);
	      } else
		gdk_draw_line(pixmap, gC[OR_GREY], xSet, PES_TOP_BORDER, xSet, displayHeight - PES_BOTTOM_BORDER);
	      if ((xSunrise < displayWidth-60) && (abs(xNow - xSunrise) > 60)) {
		renderPangoText("Sunrise", OR_GREY, SMALL_PANGO_FONT, &tWidth, &tHeight,
			    pixmap, xSunrise, displayHeight - PES_BOTTOM_BORDER + 30, 0.0, TRUE, 0);
		gdk_draw_line(pixmap, gC[OR_GREY], xRise, PES_TOP_BORDER, xRise, displayHeight - PES_BOTTOM_BORDER + 20);
	      } else
		gdk_draw_line(pixmap, gC[OR_GREY], xRise, PES_TOP_BORDER, xRise, displayHeight - PES_BOTTOM_BORDER);
	    }
	  }
	  if (planet == EARTH)
	    color = OR_RED;
	  else if ((xRise > xSunset) && (xRise < xSunrise))
	    color = OR_WHITE;
	  else
	    color = OR_GREY;
	  gdk_draw_line(pixmap, gC[color], xRise, PES_TOP_BORDER + PES_PLANET_BASE + planetIndex*planetRowInc,
			xRise, PES_TOP_BORDER + PES_PLANET_BASE-5 + planetIndex*planetRowInc);
	  drawBoundedString(color, xRise + 2,
			  PES_TOP_BORDER + PES_PLANET_BASE + planetIndex*planetRowInc + 13 , "0");
	  if (planet == EARTH)
	    color = OR_RED;
	  else if ((xSet > xSunset) && (xSet < xSunrise))
	    color = OR_WHITE;
	  else
	    color = OR_GREY;
	  drawBoundedString(color, xSet + 2,
			  PES_TOP_BORDER + PES_PLANET_BASE + planetIndex*planetRowInc + 13 , "0");
	  gdk_draw_line(pixmap, gC[color], xSet, PES_TOP_BORDER + PES_PLANET_BASE + planetIndex*planetRowInc,
			xSet, PES_TOP_BORDER + PES_PLANET_BASE-5 + planetIndex*planetRowInc);
	  for (elevation = 20.0; elevation < 90.0; elevation += 20.0) {
	    h0 = elevation*DEGREES_TO_RADIANS;
	    rawRiseTime = calcRiseOrSetTime(TRUE, planet, tJDMidnight, lSTNow, h0,
					    &circumpolar, &dummyD, &dummyD, &dummyD, &dummyF);
	    if (!circumpolar) {
	      riseTime = (rawRiseTime - (double)((int)rawRiseTime) - 0.5) * 24.0;
	      doubleNormalize0to24(&riseTime);
	      localRiseTime = 12.0 + riseTime + longitude/HOURS_TO_RADIANS;
	      xRise = (int)(localRiseTime/24.0 * (float)plotWidth) + PES_LEFT_BORDER;
	      if (abs(xRise-xTransit) > 7) {
		if (xRise > displayWidth - PES_RIGHT_BORDER)
		  xRise = xRise + PES_LEFT_BORDER - displayWidth + PES_RIGHT_BORDER;
		if ((xRise > PES_LEFT_BORDER) && (xRise < displayWidth - PES_RIGHT_BORDER)) {
		  if (planet == EARTH)
		    color = OR_RED;
		  else if ((xRise > xSunset) && (xRise < xSunrise))
		    color = OR_WHITE;
		  else
		    color = OR_GREY;
		  gdk_draw_line(pixmap, gC[color], xRise, PES_TOP_BORDER + PES_PLANET_BASE + planetIndex*planetRowInc,
				xRise, PES_TOP_BORDER + PES_PLANET_BASE-5 + planetIndex*planetRowInc);
		  sprintf(scratchString, "%2.0f", elevation);
		  stringWidth = gdk_string_width(smallFont, scratchString);
		  if (xRise - stringWidth/2 + 2 > PES_LEFT_BORDER)
		    drawBoundedString(color, xRise + 2,
				      PES_TOP_BORDER + PES_PLANET_BASE + planetIndex*planetRowInc + 13 , scratchString);
		}
	      }
	    }
	    rawSetTime = calcRiseOrSetTime(FALSE, planet, tJDMidnight, lSTNow, h0,
					   &circumpolar, &dummyD, &dummyD, &dummyD, &dummyF);
	    if (!circumpolar) {
	      setTime = (rawSetTime - (double)((int)rawSetTime) - 0.5) * 24.0;
	      doubleNormalize0to24(&setTime);
	      localSetTime = 12.0 + setTime + longitude/HOURS_TO_RADIANS;
	      xSet = (int)(localSetTime/24.0 * (float)plotWidth) + PES_RIGHT_BORDER;
	      if (abs(xSet-xTransit) > 7) {
		if (xSet > displayWidth - PES_RIGHT_BORDER)
		  xSet = xSet + PES_LEFT_BORDER - displayWidth + PES_RIGHT_BORDER;
		if ((xSet > PES_LEFT_BORDER) && (xSet < displayWidth - PES_RIGHT_BORDER)) {
		  if (planet == EARTH)
		    color = OR_RED;
		  else if ((xSet > xSunset) && (xSet < xSunrise))
		    color = OR_WHITE;
		  else
		    color = OR_GREY;
		  gdk_draw_line(pixmap, gC[color], xSet, PES_TOP_BORDER + PES_PLANET_BASE + planetIndex*planetRowInc,
				xSet, PES_TOP_BORDER + PES_PLANET_BASE-5 + planetIndex*planetRowInc);
		  sprintf(scratchString, "%2.0f", elevation);
		  stringWidth = gdk_string_width(smallFont, scratchString);
		  if (stringWidth/2 + xSet + 2 < displayWidth - PES_RIGHT_BORDER)
		    drawBoundedString(color, xSet + 2,
				      PES_TOP_BORDER + PES_PLANET_BASE + planetIndex*planetRowInc + 13 , scratchString);
		}
	      }
	    }
	  }
	} else {
	  /* The object is circumpolar */
	  if (circumpolar == WILL_NOT_RISE) {
	    /* The object does not rise today */
	    if ((planetNameIndex == SUN) || (planetNameIndex == MOON))
	      sprintf(scratchString, "The %s will not rise today",solarSystemNames[planetNameIndex]);
	    else
	      sprintf(scratchString, "%s will not rise today",solarSystemNames[planetNameIndex]);
	    renderPangoText(scratchString, OR_CREAM, SMALL_PANGO_FONT, &tWidth, &tHeight,
			    pixmap, PES_LEFT_BORDER + plotWidth/2,
			    PES_TOP_BORDER + PES_PLANET_BASE-14 + planetIndex*planetRowInc, 0.0, TRUE, 0);
	  } else {
	    /* The object does not set today */
	    float dummyF;
	    double rA, dummyD;

	    gdk_draw_line(pixmap, gC[OR_WHITE], PES_LEFT_BORDER,
			  PES_TOP_BORDER + PES_PLANET_BASE + planetIndex*planetRowInc,
			  displayWidth - PES_RIGHT_BORDER,
			  PES_TOP_BORDER + PES_PLANET_BASE + planetIndex*planetRowInc);
	    renderPangoText(solarSystemNames[planetNameIndex], OR_CREAM, SMALL_PANGO_FONT, &tWidth, &tHeight,
			    pixmap, PES_LEFT_BORDER + plotWidth/2,
			    PES_TOP_BORDER + PES_PLANET_BASE-16 + planetIndex*planetRowInc, 0.0, TRUE, 0);
	    planetInfo(dataDir, planet, tJDMidnight, &rA, &dummyD, &dummyF, &dummyF);
	    localMeanTime = 12.0 - deltaTime + rA/HOURS_TO_RADIANS + longitude/HOURS_TO_RADIANS;
	    floatNormalize0to24(&localMeanTime);
	    xTransit =  (int)(localMeanTime/24.0 * (float)plotWidth) + PES_LEFT_BORDER;
	    gdk_draw_line(pixmap, gC[OR_WHITE], xTransit, PES_TOP_BORDER + PES_PLANET_BASE + planetIndex*planetRowInc,
			  xTransit, PES_TOP_BORDER + PES_PLANET_BASE+15 + planetIndex*planetRowInc);
	    sprintf(scratchString, "%d", transitEl);
	    stringWidth = gdk_string_width(smallFont, scratchString);
	    if (abs(xNow - xTransit) > stringWidth)
	      drawBoundedString(color, xTransit,
				PES_TOP_BORDER + PES_PLANET_BASE+27 + planetIndex*planetRowInc, scratchString);
	    for (elevation = 20.0; elevation < 90.0; elevation += 20.0) {
	      h0 = elevation*DEGREES_TO_RADIANS;
	      rawRiseTime = calcRiseOrSetTime(TRUE, planet, tJDMidnight, lSTNow, h0,
					      &circumpolar, &dummyD, &dummyD, &dummyD, &dummyF);
	      if (!circumpolar) {
		riseTime = (rawRiseTime - (double)((int)rawRiseTime) - 0.5) * 24.0;
		doubleNormalize0to24(&riseTime);
		localRiseTime = 12.0 + riseTime + longitude/HOURS_TO_RADIANS;
		xRise = (int)(localRiseTime/24.0 * (float)plotWidth) + PES_LEFT_BORDER;
		if (xRise > displayWidth - PES_RIGHT_BORDER)
		  xRise = xRise + PES_LEFT_BORDER - displayWidth + PES_RIGHT_BORDER;
		if ((xRise > PES_LEFT_BORDER) && (xRise < displayWidth - PES_RIGHT_BORDER)) {
		  gdk_draw_line(pixmap, gC[OR_WHITE], xRise,
				PES_TOP_BORDER + PES_PLANET_BASE + planetIndex*planetRowInc,
				xRise, PES_TOP_BORDER + PES_PLANET_BASE-5 + planetIndex*planetRowInc);
		  sprintf(scratchString, "%2.0f", elevation);
		  stringWidth = gdk_string_width(smallFont, scratchString);
		  if (xRise - stringWidth/2 + 2 > PES_LEFT_BORDER)
		    drawBoundedString(OR_WHITE, xRise + 2,
				      PES_TOP_BORDER + PES_PLANET_BASE + planetIndex*planetRowInc + 13 , scratchString);
		}
	      }
	      rawSetTime = calcRiseOrSetTime(FALSE, planet, tJDMidnight, lSTNow, h0,
					     &circumpolar, &dummyD, &dummyD, &dummyD, &dummyF);
	      if (!circumpolar) {
		setTime = (rawSetTime - (double)((int)rawSetTime) - 0.5) * 24.0;
		doubleNormalize0to24(&setTime);
		localSetTime = 12.0 + setTime + longitude/HOURS_TO_RADIANS;
		xSet = (int)(localSetTime/24.0 * (float)plotWidth) + PES_RIGHT_BORDER;
		if (xSet > displayWidth - PES_RIGHT_BORDER)
		  xSet = xSet + PES_LEFT_BORDER - displayWidth + PES_RIGHT_BORDER;
		if ((xSet > PES_LEFT_BORDER) && (xSet < displayWidth - PES_RIGHT_BORDER)) {
		  gdk_draw_line(pixmap, gC[OR_WHITE], xSet,
				PES_TOP_BORDER + PES_PLANET_BASE + planetIndex*planetRowInc,
				xSet, PES_TOP_BORDER + PES_PLANET_BASE-5 + planetIndex*planetRowInc);
		  sprintf(scratchString, "%2.0f", elevation);
		  stringWidth = gdk_string_width(smallFont, scratchString);
		  if (stringWidth/2 + xSet + 2 < displayWidth - PES_RIGHT_BORDER)
		    drawBoundedString(OR_WHITE, xSet + 2,
				      PES_TOP_BORDER + PES_PLANET_BASE + planetIndex*planetRowInc + 13 , scratchString);
		}
	      }
	    }
	  }
	}
      }
    }
    break;
  case PLANETCOMPASS_SCREEN:
    {
      int circleRadius, circleX, circleY, i, planet,
	x[N_SOLAR_SYSTEM_OBJECTS], y[N_SOLAR_SYSTEM_OBJECTS],
	xx[N_SOLAR_SYSTEM_OBJECTS], yy[N_SOLAR_SYSTEM_OBJECTS];
      int plotOrder[9] = {EARTH, MERCURY, VENUS, MOON, MARS, JUPITER, SATURN, URANUS, NEPTUNE};
      float dummy;
      double rA, dec, az, zA;
      GdkPoint triangle[3];
      GdkGC *riseGC, *transitGC, *setGC, *hAGC, *nameGC;

      needNewTime = TRUE;
      lSTNow = lST();
      sprintf(scratchString, "Planet positions at   ");
      makeTimeString(&scratchString[strlen(scratchString)], TRUE);
      renderPangoText(scratchString, OR_WHITE, SMALL_PANGO_FONT, &tWidth, &tHeight,
		      pixmap, displayWidth>>1, 11, 0.0, TRUE, 0);
      row += 5;
      circleRadius = 20 + displayWidth/3;
      circleX = circleRadius-61;
      circleY = row*ABOUT_ROW_STEP;
      gdk_draw_arc(pixmap, gC[OR_CREAM], FALSE,
		   circleX, circleY, circleRadius*2,
		   circleRadius*2, 0, FULL_CIRCLE);
      /* Draw cardinal points */
      triangle[0].x = circleX+circleRadius - 4; triangle[0].y = circleY;
      triangle[1].x = triangle[0].x        + 8; triangle[1].y = triangle[0].y;
      triangle[2].x = triangle[0].x        + 4; triangle[2].y = triangle[0].y + 15;
      renderPangoText("N", OR_GREEN, SMALL_PANGO_FONT, &tWidth, &tHeight, pixmap,
		      circleX+circleRadius, circleY+29, 0.0, TRUE, 0);
      gdk_draw_polygon(pixmap, gC[OR_BLUE], TRUE, triangle, 3);
      triangle[0].x = circleX+circleRadius - 4; triangle[0].y = circleY + 2*circleRadius + 1;
      triangle[1].x = triangle[0].x        + 8; triangle[1].y = triangle[0].y;
      triangle[2].x = triangle[0].x        + 4; triangle[2].y = triangle[0].y - 15;
      renderPangoText("S", OR_GREEN, SMALL_PANGO_FONT, &tWidth, &tHeight, pixmap,
		      circleX+circleRadius, circleY+2*circleRadius-25,
		      0.0, TRUE, 0);
      gdk_draw_polygon(pixmap, gC[OR_BLUE], TRUE, triangle, 3);
      triangle[0].x = circleX;                  triangle[0].y = circleY + circleRadius - 4;
      triangle[1].x = triangle[0].x;            triangle[1].y = triangle[0].y          + 8;
      triangle[2].x = triangle[0].x + 15;       triangle[2].y = triangle[0].y          + 4;
      renderPangoText("W", OR_GREEN, SMALL_PANGO_FONT, &tWidth, &tHeight, pixmap,
		      circleX + 26, circleY + circleRadius,
		      0.0, TRUE, 0);
      gdk_draw_polygon(pixmap, gC[OR_BLUE], TRUE, triangle, 3);
      triangle[0].x = circleX + 2*circleRadius; triangle[0].y = circleY + circleRadius - 4;
      triangle[1].x = triangle[0].x;            triangle[1].y = triangle[0].y          + 8;
      triangle[2].x = triangle[0].x - 15;       triangle[2].y = triangle[0].y          + 4;
      renderPangoText("E", OR_GREEN, SMALL_PANGO_FONT, &tWidth, &tHeight, pixmap,
		      circleX+2*circleRadius - 25, circleY + circleRadius,
		      0.0, TRUE, 0);
      gdk_draw_polygon(pixmap, gC[OR_BLUE], TRUE, triangle, 3);
      drawCompassLine(gC[OR_BLUE], M_PI*0.25, 8, circleX+circleRadius, circleY+circleRadius, circleRadius);
      drawCompassLine(gC[OR_BLUE], M_PI*0.75, 8, circleX+circleRadius, circleY+circleRadius, circleRadius);
      drawCompassLine(gC[OR_BLUE], M_PI*1.25, 8, circleX+circleRadius, circleY+circleRadius, circleRadius);
      drawCompassLine(gC[OR_BLUE], M_PI*1.75, 8, circleX+circleRadius, circleY+circleRadius, circleRadius);
      for (i = 10; i < 360; i += 10) {
	int x, y;
	float angle;

	if (i % 9 != 0) {
	  angle = (float)i * M_2PI/360.0;
	  x = (int)((float)(circleX+circleRadius) + (float)(circleRadius-5) * sin(angle) + 0.5);
	  y = (int)((float)(circleY+circleRadius) + (float)(circleRadius-5) * cos(angle) + 0.5);
	  gdk_draw_arc(pixmap, gC[OR_BLUE], TRUE, x-1, y-1, 3, 3, 0, FULL_CIRCLE);
	}
      }
      gdk_draw_arc(pixmap, gC[OR_CREAM], FALSE,
		   0, circleY, circleRadius*2,
		   circleRadius*2, FULL_CIRCLE/4, FULL_CIRCLE/2);
      triangle[0].x = 0;                        triangle[0].y = circleY + circleRadius - 4;
      triangle[1].x = triangle[0].x;            triangle[1].y = triangle[0].y          + 8;
      triangle[2].x = triangle[0].x + 15;       triangle[2].y = triangle[0].y          + 4;
      gdk_draw_polygon(pixmap, gC[OR_BLUE], TRUE, triangle, 3);
      triangle[0].x = circleRadius - 4;         triangle[0].y = circleY;
      triangle[1].x = triangle[0].x        + 8; triangle[1].y = triangle[0].y;
      triangle[2].x = triangle[0].x        + 4; triangle[2].y = triangle[0].y + 15;
      gdk_draw_polygon(pixmap, gC[OR_BLUE], TRUE, triangle, 3);
      triangle[0].x = circleRadius - 4;         triangle[0].y = circleY + 2*circleRadius + 1;
      triangle[1].x = triangle[0].x        + 8; triangle[1].y = triangle[0].y;
      triangle[2].x = triangle[0].x        + 4; triangle[2].y = triangle[0].y - 15;
      gdk_draw_polygon(pixmap, gC[OR_BLUE], TRUE, triangle, 3);
      drawCompassLine(gC[OR_BLUE], M_PI*1.25, 8, circleRadius, circleY+circleRadius, circleRadius);
      drawCompassLine(gC[OR_BLUE], M_PI*1.75, 8, circleRadius, circleY+circleRadius, circleRadius);
      renderPangoText("Zenith", OR_GREEN, SMALL_PANGO_FONT, &tWidth, &tHeight, pixmap,
		      circleRadius, circleY - 12, 0.0, TRUE, 0);
      renderPangoText("Horizon", OR_GREEN, SMALL_PANGO_FONT, &tWidth, &tHeight, pixmap,
		      50, circleY + circleRadius, 0.0, TRUE, 0);
      renderPangoText("Nadir", OR_RED, SMALL_PANGO_FONT, &tWidth, &tHeight, pixmap,
		      circleRadius, circleY + 2*circleRadius + 14, 0.0, TRUE, 0);
      for (i = 190; i < 360; i += 10) {
	int x, y;
	float angle;

	if (i % 9 != 0) {
	  angle = (float)i * M_2PI/360.0;
	  x = (int)((float)(circleRadius) + (float)(circleRadius-5) * sin(angle) + 0.5);
	  y = (int)((float)(circleY+circleRadius) + (float)(circleRadius-5) * cos(angle) + 0.5);
	  gdk_draw_arc(pixmap, gC[OR_BLUE], TRUE, x-1, y-1, 3, 3, 0, FULL_CIRCLE);
	}
      }
      /* Loop down to 0, so that the Sun doesn't get covered if objects are close to it in sky */
      planetInfo(dataDir, EARTH, tJD, &rA, &dec, &dummy, &dummy);
      for (i = N_SOLAR_SYSTEM_OBJECTS-2; i >=  1; i--) {
	int w, h, boost, clear, j, dSq;

	planetInfo(dataDir, i, tJD, &rA, &dec, &dummy, &dummy);
	azZA(rA, sin(dec), cos(dec), &az, &zA, FALSE);
	boost = 0;
	do {
	  x[i] = (int)((float)(circleX+circleRadius) + (float)(circleRadius-33-boost*20) * sin(az) + 0.5);
	  y[i] = (int)((float)(circleY+circleRadius) - (float)(circleRadius-33-boost*20) * cos(az) + 0.5);
	  clear = TRUE;
	  if (i < N_SOLAR_SYSTEM_OBJECTS-2) {
	    for (j = N_SOLAR_SYSTEM_OBJECTS-2; j > i; j--) {
	      dSq = ((x[i]-x[j])*(x[i]-x[j])) + ((y[i]-y[j])*(y[i]-y[j]));
	      if (dSq < 200) {
		boost++;
		clear = FALSE;
		break;
	      }
	    }
	  }
	} while (!clear);
	if (zA <= M_HALF_PI)
	  drawCompassLine(gC[OR_WHITE], az, 20+boost*20, circleX+circleRadius, circleY+circleRadius, circleRadius);
	else
	  drawCompassLine(gC[OR_RED], az, 20+boost*20, circleX+circleRadius, circleY+circleRadius, circleRadius);
	gdk_drawable_get_size(planetImages[i], &w, &h);
	if (i != EARTH)
	  gdk_draw_drawable(pixmap, gC[OR_BLUE], planetImages[i], 0, 0,
			    x[i]-w/2, y[i]-h/2, 20, 20);
	else
	  gdk_draw_drawable(pixmap, gC[OR_BLUE], planetImages[SUN], 0, 0,
			    x[i]-w/2, y[i]-h/2, 20, 20);
	boost = 0;
	do {
	  xx[i] = (int)((float)(circleRadius) + (float)(circleRadius-30-boost*20) * sin(M_PI+zA) + 0.5);
	  yy[i] = (int)((float)(circleY+circleRadius) + (float)(circleRadius-30-boost*20) * cos(M_PI+zA) + 0.5);
	  clear = TRUE;
	  if (i < N_SOLAR_SYSTEM_OBJECTS-2) {
	    for (j = N_SOLAR_SYSTEM_OBJECTS-2; j > i; j--) {
	      dSq = ((xx[i]-xx[j])*(xx[i]-xx[j])) + ((yy[i]-yy[j])*(yy[i]-yy[j]));
	      if (dSq < 200) {
		boost++;
		clear = FALSE;
		break;
	      }
	    }
	  }
	} while (!clear);
	if (zA <= M_HALF_PI)
	  drawCompassLine(gC[OR_WHITE], M_2PI-zA, 20+boost*20, circleRadius, circleY+circleRadius, circleRadius);
	else
	  drawCompassLine(gC[OR_RED], M_2PI-zA, 20+boost*20, circleRadius, circleY+circleRadius, circleRadius);
	if (i != EARTH)
	  gdk_draw_drawable(pixmap, gC[OR_BLUE], planetImages[i], 0, 0,
			    xx[i]-w/2, yy[i]-h/2, 20, 20);
	else
	  gdk_draw_drawable(pixmap, gC[OR_BLUE], planetImages[SUN], 0, 0,
			    xx[i]-w/2, yy[i]-h/2, 20, 20);
      }
      row += 34;
      sprintf(scratchString, "                         Rising             Transit            Setting     ");
      stringWidth = gdk_string_width(smallFont, scratchString);
      gdk_draw_string(pixmap, smallFont, gC[OR_BLUE],
		      (displayWidth/2)-stringWidth/2,
		      (row++)*ABOUT_ROW_STEP, scratchString);
      sprintf(scratchString, "Planet      HA        Time      Az        Time      El       Time      Az  ");
      stringWidth = gdk_string_width(smallFont, scratchString);
      gdk_draw_string(pixmap, smallFont, gC[OR_BLUE],
		      (displayWidth/2)-stringWidth/2,
		      (row++)*ABOUT_ROW_STEP, scratchString);
      fracTJD = tJD - (double)((int)tJD);
      if (fracTJD < 0.5)
	tJDS[0] = (double)((int)tJD) + 0.5 + longitude/M_2PI;
      else
	tJDS[0] = (double)((int)tJD) + 1.5 + longitude/M_2PI;
      /*
	Display rise, transit and set info for planets, Sun and Moon.
      */
      row -= 8;
      for (i = 0; i < N_SOLAR_SYSTEM_OBJECTS-2; i++) {
	int hASign;
	float fDummy;
	double hA, dec, rA, transitEl, theAz, az, zA;

	planet = plotOrder[i];
	if (planet == EARTH) {
	  h0 = -(50.0/60.0)*DEGREES_TO_RADIANS;
	  sprintf(scratchString, "Sun");
	} else {
	  if (planet == MOON)
	    h0 = 0.125*DEGREES_TO_RADIANS;
	  else
	    h0 = -(34.0/60.0)*DEGREES_TO_RADIANS;
	  sprintf(scratchString, "%s",solarSystemNames[planet]);
	}
	planetInfo(dataDir, planet, tJD, &rA, &dec, &fDummy, &fDummy);
	azZA(rA, sin(dec), cos(dec), &az, &zA, FALSE);
	lSTNow = lST();
	hA = lSTNow - rA;
	if (hA < -M_PI)
	  hA += M_2PI;
	else if (hA > M_PI)
	  hA -= M_2PI;
	riseGC = transitGC = setGC = hAGC = gC[OR_GREY];
	nameGC = gC[OR_GREEN];
	if (zA > M_HALF_PI) {
	  riseGC = gC[OR_WHITE];
	  nameGC = gC[OR_RED];
	} else if (hA < 0.0)
	  transitGC = hAGC = gC[OR_WHITE];
	else
	  setGC = hAGC = gC[OR_WHITE];
	gdk_draw_string(pixmap, smallFont, nameGC,
			(displayWidth/2)-stringWidth/2,
			row*BIGGER_ROW_STEP, scratchString);
	hA /= HOURS_TO_RADIANS;
	if (hA < 0.0) {
	  hASign = -1;
	  hA *= -1.0;
	} else
	  hASign = 1;
	rAHH = (int)hA;
	rAMM = (int)((hA - (double)rAHH)*60.0 + 0.5);
	if (hASign < 0)
	  sprintf(scratchString, "-%02d:%02d", rAHH, rAMM);
	else
	  sprintf(scratchString, " %02d:%02d", rAHH, rAMM);
	gdk_draw_string(pixmap, smallFont, hAGC,
			(displayWidth/2)-stringWidth/2+gdk_string_width(smallFont, "Planet    "),
			row*BIGGER_ROW_STEP, scratchString);
	lSTNow = lSTAtTJD(tJDS[0]);
	theTime = calcRiseOrSetTime(TRUE, planet, tJDS[0], lSTNow, h0, &circumpolar,
				    &hD, &cosH, &sunDec, &illum);
	if (circumpolar) {
	  gdk_draw_string(pixmap, smallFont, gC[OR_GREY],
			  (displayWidth/2)-stringWidth/2+gdk_string_width(smallFont, "Planet    HA        "),
			  row*BIGGER_ROW_STEP, "Not today");
	} else {
	  theTime -= 0.5;
	  theTime = theTime - (double)((int)theTime);
	  rAHours = theTime*24.0;
	  rAHH = (int)rAHours;
	  rAMM = (int)((rAHours - (float)rAHH)*60.0 + 0.5);
	  theAz = atan2(sin(hD), cosH*sinLatitude-tan(sunDec)*cosLatitude) + M_PI;
	  doubleNormalize0to2pi(&theAz);
	  sprintf(scratchString, "%02d:%02d UT   %5.1f", rAHH, rAMM, theAz/DEGREES_TO_RADIANS);
	  gdk_draw_string(pixmap, smallFont, riseGC,
			  (displayWidth/2)-stringWidth/2+gdk_string_width(smallFont, "Planet    HA        "),
			  row*BIGGER_ROW_STEP, scratchString);
	}
	theTime = calcTransitTime(tJDS[0], planet, &transitEl);
	theTime -= 0.5;
	theTime = theTime - (double)((int)theTime);
	rAHours = theTime*24.0;
	rAHH = (int)rAHours;
	rAMM = (int)((rAHours - (float)rAHH)*60.0 + 0.5);
	sprintf(scratchString, "%02d:%02d UT  %5.1f", rAHH, rAMM, transitEl/DEGREES_TO_RADIANS);
	gdk_draw_string(pixmap, smallFont, transitGC,
			(displayWidth/2)-stringWidth/2+gdk_string_width(smallFont, "Planet             Time    Az           "),
			row*BIGGER_ROW_STEP, scratchString);
	theTime = calcRiseOrSetTime(FALSE, planet, tJDS[0], lSTNow, h0, &circumpolar,
				    &hD, &cosH, &sunDec, &illum);
	if (circumpolar) {
	    gdk_draw_string(pixmap, smallFont, gC[OR_GREY],
			    (displayWidth/2)-stringWidth/2+gdk_string_width(smallFont, "Planet      Rise Time  Az        Transit Time   El         "),
			    row*BIGGER_ROW_STEP, "Not today");
	} else {
	  theTime -= 0.5;
	  theTime = theTime - (double)((int)theTime);
	  rAHours = theTime*24.0;
	  rAHH = (int)rAHours;
	  rAMM = (int)((rAHours - (float)rAHH)*60.0 + 0.5);
	  theAz = atan2(sin(hD), cosH*sinLatitude-tan(sunDec)*cosLatitude) + M_PI;
	  doubleNormalize0to2pi(&theAz);
	  sprintf(scratchString, "%02d:%02d UT   %5.1f", rAHH, rAMM, theAz/DEGREES_TO_RADIANS);
	  gdk_draw_string(pixmap, smallFont, setGC,
			  (displayWidth/2)-stringWidth/2+gdk_string_width(smallFont, "Planet       Rise Time  Az      Transit Time   El         S"),
			  row*BIGGER_ROW_STEP, scratchString);
	}
	row++;
      } /* End of for i... over N_SOLAR_SYSTEM_OBJECTS-1 */
    }
    break;
  }
  if ((aboutScreen != PLANETCOMPASS_SCREEN) && (aboutScreen != SMALL_MOONCAL_SCREEN)
      && (aboutScreen != SOLAR_SYSTEM_SCHEMATIC_SCREEN) && (aboutScreen != PLANET_ELEVATION_SCREEN)
      && (aboutScreen != JOVIAN_MOONS) && (aboutScreen != CELESTIAL_NAVIGATION)
      && (aboutScreen != LUNAR_ECLIPSES)
      && (aboutScreen != SOLAR_SYSTEM_SCALE_SCREEN) && (aboutScreen != TIMES_PAGE_SCREEN)
      && (aboutScreen != ANALEMMA_SCREEN) && (aboutScreen != SOLUNI_SCREEN)) {
    
    sprintf(scratchString, "Please send comments, suggestions or questions to orrery.moko@gmail.com");
    stringWidth = gdk_string_width(smallFont, scratchString);
    gdk_draw_string(pixmap, smallFont, gC[OR_BLUE],
		    (displayWidth/2)-stringWidth/2,
		    displayHeight-ABOUT_ROW_STEP, scratchString);
  }
}

/*
  This function is the top functions for redrawing the plot in
  the drawing area.   It calls other functions to draw the
  different plot styles.
*/
static void redrawScreen(void)
{
  if (imAPostageStamp) {
    postageStampUTMM = postageStampLSTMM = -1;
  } else if (inFlashlightMode) {
    gdk_draw_rectangle(pixmap, flashlightGC,
		       TRUE, 0, 0,
		       drawingArea->allocation.width,
		       drawingArea->allocation.height);
  } else {
    gdk_draw_rectangle(pixmap, drawingArea->style->black_gc,
		       TRUE, 0, 0,
		       drawingArea->allocation.width,
		       drawingArea->allocation.height);
    if (displayingAnOptsPage)
      drawOptsScreens();
    else  {
      xBorder = 1;
      yBorder = 5;
      xLeftLabelSkip = 0;
      redrawScreenTransverseMercator();
    }
  }
}

/*
  This function is called when a portion of the drawing area
  is exposed.
*/
static gboolean exposeEvent(GtkWidget *widget, GdkEventExpose *event)
{
  inAzCompassMode = FALSE;
  gdk_draw_drawable (widget->window,
                     widget->style->fg_gc[GTK_WIDGET_STATE (widget)],
                     pixmap,
                     event->area.x, event->area.y,
                     event->area.x, event->area.y,
                     event->area.width, event->area.height);
  return(FALSE);
}

/*
  This function creates and initializes a backing pixmap
  of the appropriate size for the drawing area.
*/
static int configureEvent(GtkWidget *widget, GdkEventConfigure *event)
{
  displayHeight = widget->allocation.height;
  displayWidth  = widget->allocation.width;
  if (!cairoPixmap)
    cairoPixmap = gdk_pixmap_new(widget->window, 600, 800, -1);
  if (pixmap)
    g_object_unref(pixmap);
  pixmap = gdk_pixmap_new(widget->window,
			  widget->allocation.width,
			  widget->allocation.height, -1);
  if (displayWidth > displayHeight)
    landscapeMode = TRUE;
  else
    landscapeMode = FALSE;
  dprintf("In configureEvent, w: %d, h: %d, f = %d, l = %d\n",
	  displayWidth, displayHeight, fullscreenStateChanging,
	 landscapeMode);
  if ((!fullscreenStateChanging || !inFullscreenMode || !inFlashlightMode ||
       (displayHeight == VGA_HEIGHT)) && !mainBoxInOptsStackable) {
    redrawScreen();
  }
  if (displayHeight == VGA_HEIGHT)
    fullscreenStateChanging = FALSE;
  return TRUE;
}

/*
  Make all the Graphic Contexts the program will need.
*/
static void makeGraphicContexts(GtkWidget *widget)
{
  int stat, i;
  short starRed[5] = {55000, 65535, 65535, 65535, 65535};
  short starBlue[5] = {65535, 65535, 40000, 40000, 45000};
  short starGreen[5] = {55000, 65535, 65535, 50000, 45000};
  GdkGCValues gCValues, starGCValues[5];
  GdkColor gColor, star[5];
  GdkGCValuesMask gCValuesMask;

  gCValuesMask = GDK_GC_FOREGROUND;

  for (i = 0; i < N_COLORS; i++) {
    gColor.red   = orreryColorRGB[i][0];
    gColor.green = orreryColorRGB[i][1];
    gColor.blue  = orreryColorRGB[i][2];
    stat = gdk_colormap_alloc_color(widget->style->colormap,
				    &gColor,
				    FALSE,
				    TRUE);
    if (unlikely(stat != TRUE)) {
      fprintf(stderr, "Error allocating color %d\n", i);
      exit(ERROR_EXIT);
    }
    gCValues.foreground = gColor;
    gC[i] = gtk_gc_get(widget->style->depth,
		       widget->style->colormap,
		       &gCValues, gCValuesMask
		       );
    if (unlikely(gC[i] == NULL)) {
      fprintf(stderr, "gtk_gc_get failed for color %d\n", i);
      exit(ERROR_EXIT);
    }
  }

  for (i = 0; i < 5; i++) {
    star[i].red   = starRed[i];
    star[i].green = starGreen[i];
    star[i].blue  = starBlue[i];
    stat = gdk_colormap_alloc_color(widget->style->colormap,
				    &star[i],
				    FALSE,
				    TRUE);
    if (unlikely(stat != TRUE)) {
      fprintf(stderr, "Error allocating star[%d] color\n", i);
      exit(ERROR_EXIT);
    }
    starGCValues[i].foreground = star[i];
    starGC[i] = gtk_gc_get(widget->style->depth,
			   widget->style->colormap,
			   &starGCValues[i], gCValuesMask
			     );
    if (unlikely(starGC[i] == NULL)) {
      fprintf(stderr, "gtk_gc_get failed for starGC[%d]\n", i);
      exit(ERROR_EXIT);
    }
  }
}

/*
  Set up the fonts I need.
*/
static void makeFonts(GtkWidget *widget)
{
  smallFont =
    gdk_font_load("-misc-fixed-medium-r-semicondensed--13-100-100-100-c-60-iso8859-1");
  if (unlikely(smallFont == NULL)) {
    fprintf(stderr, "Error making smallFont\n");
    exit(ERROR_EXIT);
  }
  bigFont =
    gdk_font_load("-misc-fixed-medium-r-semicondensed--13-120-75-75-c-60-iso8859-1");
  if (unlikely(bigFont == NULL)) {
    fprintf(stderr, "Error making bigFont\n");
    exit(ERROR_EXIT);
  }
}

/*
  This function reads in the binary Hipparcos star catalog file, and
  allocates a linked list to store the data.
*/
static void readHipparcosCatalog(void)
{
  int fD, nRead;
  int allStarsRead = FALSE;
  char fileName[MAX_FILE_NAME_SIZE];
  starNameTempList *nextStar, *lastStar = NULL;

  /*
    Read in file containing star names and Bayer designations
    First we make a temporary linked list of the entries.   Then,
    when the Hipparcos catalog is read in, we look for matches in
    this temporatly linked list, and build a permanant liked list
    that has pointers to the Hipparcos entry, so that the final
    name list has access to the Hipparcos data (position, mag,
    color etc) without that data having been stored redundantly in
    the names file.
   */
  sprintf(fileName, "%s/starNames.bin", dataDir);
  fD = open(fileName, O_RDONLY);
  if (unlikely(fD < 0)) {
    perror("Error opening star names file\n");
    exit(ERROR_EXIT);
  }
  while (!allStarsRead) {
    nextStar = (starNameTempList *)malloc(sizeof(starNameTempList));
    if (unlikely(nextStar == NULL)) {
      perror("Cannot malloc starNameTempList structure");
      exit(ERROR_EXIT);
    }
    nextStar->found = FALSE;
    nextStar->next = NULL;
    nRead = read(fD, &nextStar->record, sizeof(starNameTemp));
    if (nRead == sizeof(starNameTemp)) {
      if (nextStar->record.nHip < 0) {
	char nextChar, *tempString;
	int bigStringLength = 0;

	/*
	  The last record has -1 as the Hipparcos number, as a flag that the
	  list has been fully traversed.   The remaining info is a single long
	  string holding the star names.
	 */
	free(nextStar);
	tempString = malloc(4096);
	if (unlikely(tempString == NULL)) {
	  perror("allocating tempString");
	  exit(ERROR_EXIT);
	}
	do {
	  nRead = read(fD, &nextChar, 1);
	  if (nRead == 1)
	    tempString[bigStringLength++] = nextChar;
	} while (nRead == 1);
	tempString[bigStringLength] = '\0';
	starNameString = malloc(strlen(tempString)+1);
	if (unlikely(starNameString == NULL)) {
	  perror("starNameString");
	  exit(ERROR_EXIT);
	}
	strcpy(starNameString, tempString);
	free(tempString);
	allStarsRead = TRUE;
      } else {
	/* Enqueue this star name entry */
	if (starNameTempListRoot == NULL)
	  /* First entry! */
	  starNameTempListRoot = nextStar;
	else
	  lastStar->next = nextStar;
	lastStar = nextStar;
      }
    } else
      allStarsRead = TRUE;
  }
  close(fD);
  /* Done building the temporary star names list */

  /* Check if the faint star catalog exists */
  sprintf(fileName, "%s/faintStars", dataDir);
  fD = open(fileName, O_RDONLY);
  if (fD >= 0) {
    haveFaintStars = TRUE;
    close(fD);
  }
  /*
    Now read in the normal, naked-eye star catalog.
  */
  sprintf(fileName, "%s/hipparcos.dat", dataDir);
  fD = open(fileName, O_RDONLY);
  if (unlikely(fD < 0)) {
    perror("opening Hipparcos star catalog.");
    exit(ERROR_EXIT);
  }
  readStarCatalog(fD, TRUE);
  showingFaintStars = FALSE;
}

/*
  Draw the Sun and Moon, along with lines from the symbol to the azimuth
  circle, when the azimuth slew circle is to be displayed.
 */
void drawPlanetPositionsOnAzCompass(int init)
{
  static int showSun, showMoon;
  static GdkPoint sunRadialLine[2];
  static GdkPoint moonRadialLine[2];
  static int sunPicX, sunPicY, moonPicX, moonPicY;
  int w, h;
  float tSin, tCos;
  double rA, dec, az, zA;
  double sunAz = 0.0;

  if (init) {
    /*
      The first time through this routine, we calculate the Az ans El of
      the Sun and Moon, to determine if they should be plotted on the
      azimuth slew compass, and if so, where to do so.
     */
    planetInfo(dataDir, EARTH, tJD, &rA, &dec, &tSin, &tCos);
    azZA(rA, sin(dec), cos(dec), &az, &zA, FALSE);
    if (zA < M_HALF_PI) {
      /* The Sun is high enough, so plot it. */
      if (zA >  M_PI*(85.0/180.0))
	zA = M_PI*(85.0/180.0);
      zA /= M_HALF_PI;
      sunAz = az;
      showSun = TRUE;
      tSin = sinf(az); tCos = cosf(az);
      sunRadialLine[0].x = azCompassCenterX + AZ_COMPASS_RADIUS*zA*tSin;
      sunRadialLine[0].y = azCompassCenterY - AZ_COMPASS_RADIUS*zA*tCos;
      sunRadialLine[1].x = azCompassCenterX + AZ_COMPASS_RADIUS*tSin;
      sunRadialLine[1].y = azCompassCenterY - AZ_COMPASS_RADIUS*tCos;
      gdk_drawable_get_size(planetImages[SUN], &w, &h);
      sunPicX = sunRadialLine[0].x - AZ_COMPASS_RADIUS*0.07*tSin - w/2;
      sunPicY = sunRadialLine[0].y + AZ_COMPASS_RADIUS*0.07*tCos - h/2;
      gdk_draw_drawable(pixmap, gC[OR_BLUE], planetImages[SUN], 0, 0,
			sunPicX, sunPicY, 20, 20);
      gdk_draw_lines(pixmap, gC[OR_BLUE], sunRadialLine, 2);
    } else
      showSun = FALSE;
    planetInfo(dataDir, MOON, tJD, &rA, &dec, &tSin, &tCos);
    azZA(rA, sin(dec), cos(dec), &az, &zA, FALSE);
    if ((zA < M_HALF_PI)
	&& ((fabs(sunAz-az) > 20.0*DEGREES_TO_RADIANS)
	    || (!showSun))) {
      /*
	Plot the Moon IFF it is high enough and either > 20 degrees from
	the Sun or the Sun isn't being plotted.
       */
      if (zA >  M_PI*(85.0/180.0))
	zA = M_PI*(85.0/180.0);
      zA /= M_HALF_PI;
      showMoon = TRUE;
      tSin = sinf(az); tCos = cosf(az);
      moonRadialLine[0].x = azCompassCenterX + AZ_COMPASS_RADIUS*zA*tSin;
      moonRadialLine[0].y = azCompassCenterY - AZ_COMPASS_RADIUS*zA*tCos;
      moonRadialLine[1].x = azCompassCenterX + AZ_COMPASS_RADIUS*tSin;
      moonRadialLine[1].y = azCompassCenterY - AZ_COMPASS_RADIUS*tCos;
      gdk_drawable_get_size(planetImages[MOON], &w, &h);
      moonPicX = moonRadialLine[0].x - AZ_COMPASS_RADIUS*0.07*tSin - w/2;
      moonPicY = moonRadialLine[0].y + AZ_COMPASS_RADIUS*0.07*tCos - h/2;
      gdk_draw_drawable(pixmap, gC[OR_BLUE], planetImages[MOON], 0, 0,
			moonPicX, moonPicY, 20, 20);
      gdk_draw_lines(pixmap, gC[OR_BLUE], moonRadialLine, 2);
    } else
      showMoon = FALSE;
  } else {
    if (showSun) {
      gdk_draw_drawable(drawingArea->window, gC[OR_BLUE], planetImages[SUN], 0, 0,
			sunPicX, sunPicY, 20, 20);
      gdk_draw_lines(drawingArea->window, gC[OR_BLUE], sunRadialLine, 2);
    }
    if (showMoon) {
      gdk_draw_drawable(drawingArea->window, gC[OR_BLUE], planetImages[MOON], 0, 0,
			moonPicX, moonPicY, 20, 20);
      gdk_draw_lines(drawingArea->window, gC[OR_BLUE], moonRadialLine, 2);
    }
  }
}

#define AZ_LABEL_X (25)
#define AZ_LABEL_Y (27)
/*
  Draw an arrow pointing to a particular aziuth on the azimuth
  slew compass
*/
void drawAzArrow(int eraseOld, float az)
{
  int i, tWidth, tHeight;
  static int lastWidth = 0;
  static int lastHeight = 0;
  float sinAz, cosAz;
  static GdkPoint oldWedge[3];
  GdkPoint newWedge[3], unRotated[3];
  char comment[40];
  
  if (eraseOld)
    gdk_draw_polygon(drawingArea->window, gC[OR_BLACK], TRUE, oldWedge, 3);
  az -= M_HALF_PI;
  if (az < 0.0)
    az += M_2PI;
  sinAz = sinf(az); cosAz = cosf(az);
  unRotated[0].x = - AZ_COMPASS_POINT_HEIGHT/2;
  unRotated[0].y = -(AZ_COMPASS_RADIUS - 2*AZ_COMPASS_TICK_LENGTH - AZ_COMPASS_POINT_HEIGHT);
  unRotated[1].y = unRotated[0].y;
  unRotated[1].x = unRotated[0].x + AZ_COMPASS_POINT_HEIGHT;
  unRotated[2].x = 0.0;
  unRotated[2].y = unRotated[0].y - (3*AZ_COMPASS_POINT_HEIGHT)/2;
  for (i = 0; i < 3; i++) {
    newWedge[i].x = azCompassCenterX + cosAz*unRotated[i].x - sinAz*unRotated[i].y;
    newWedge[i].y = azCompassCenterY + cosAz*unRotated[i].y + sinAz*unRotated[i].x;
    oldWedge[i].x = newWedge[i].x; oldWedge[i].y = newWedge[i].y;
  }
  sprintf(comment, "Center Azimuth = %1.0f", az/DEGREES_TO_RADIANS);
  if (eraseOld) {
    drawPlanetPositionsOnAzCompass(FALSE);
    gdk_draw_polygon(drawingArea->window, gC[OR_WHITE], TRUE, newWedge, 3);
    gdk_draw_rectangle(drawingArea->window, gC[OR_BLACK], TRUE,
		       ((displayWidth-lastWidth)>>1)-1, 0,
		       lastWidth+2, lastHeight);
    renderPangoText(comment, OR_WHITE, BIG_PANGO_FONT, &tWidth, &tHeight,
		    drawingArea->window, displayWidth>>1,
		    AZ_LABEL_Y, 0.0, TRUE, 0);
  } else {
    drawPlanetPositionsOnAzCompass(TRUE);
    gdk_draw_polygon(pixmap, gC[OR_WHITE], TRUE, newWedge, 3);
    renderPangoText(comment, OR_WHITE, BIG_PANGO_FONT, &tWidth, &tHeight, pixmap,
		    displayWidth>>1, AZ_LABEL_Y, 0.0, TRUE, 0);
  }
  lastWidth = tWidth; lastHeight = tHeight;
}

/*
  Draw a large compass in the center of the screen which will allow the
  user to select a new central azimuth for the display.
*/
void drawAzCompass(void)
{
  int i, tWidth, tHeight;
  float az, xi, xo, yi, yo, fScreenCenterX, fScreenCenterY, fCompassRadius;
  float fInnerRadius, fInnerRadius2, cosAz, sinAz, sqr2Over2;
  GdkPoint point[3], smallArrow[3];

  inAzCompassMode = TRUE;
  removeAllSensitiveAreas();
  gdk_draw_rectangle(pixmap, drawingArea->style->black_gc,
		     TRUE, 0, 0,
		     drawingArea->allocation.width,
		     drawingArea->allocation.height);
  azCompassCenterX = displayWidth/2 + 8;
  azCompassCenterY = displayHeight/2 - 50;
  renderPangoText("Select a new center azimuth", OR_CREAM, MEDIUM_PANGO_FONT,
		  &tWidth, &tHeight, pixmap, displayWidth >> 1,
		  displayHeight - 120, 0.0, TRUE, 0);
  renderPangoText("with your finger or stylus", OR_CREAM, MEDIUM_PANGO_FONT,
		  &tWidth, &tHeight, pixmap, displayWidth >> 1,
		  displayHeight - 120 + tHeight, 0.0, TRUE, 0);
  gdk_draw_arc(pixmap, gC[OR_BLUE], FALSE,
	       azCompassCenterX-AZ_COMPASS_RADIUS,
	       azCompassCenterY-AZ_COMPASS_RADIUS, AZ_COMPASS_RADIUS*2,
	       AZ_COMPASS_RADIUS*2, 0, FULL_CIRCLE);
  point[0].x = azCompassCenterX - AZ_COMPASS_POINT_HEIGHT/2;
  point[0].y = azCompassCenterY - AZ_COMPASS_RADIUS;
  point[1].y = point[0].y;
  point[1].x = point[0].x + AZ_COMPASS_POINT_HEIGHT;
  point[2].x = azCompassCenterX;
  point[2].y = point[0].y - (3*AZ_COMPASS_POINT_HEIGHT)/2;
  renderPangoText("N", OR_GREEN, BIG_PANGO_FONT, &tWidth, &tHeight, pixmap,
		  azCompassCenterX, point[2].y - 20, 0.0, TRUE, 0);
  gdk_draw_polygon(pixmap, gC[OR_CREAM], TRUE, point, 3);
  point[0].x = azCompassCenterX - AZ_COMPASS_POINT_HEIGHT/2;
  point[0].y = azCompassCenterY + AZ_COMPASS_RADIUS;
  point[1].y = point[0].y;
  point[1].x = point[0].x + AZ_COMPASS_POINT_HEIGHT;
  point[2].x = azCompassCenterX;
  point[2].y = point[0].y + (3*AZ_COMPASS_POINT_HEIGHT)/2;
  renderPangoText("S", OR_GREEN, BIG_PANGO_FONT, &tWidth, &tHeight, pixmap,
		  azCompassCenterX, point[2].y + 24, 0.0, TRUE, 0);
  gdk_draw_polygon(pixmap, gC[OR_CREAM], TRUE, point, 3);
  point[0].x = azCompassCenterX - AZ_COMPASS_RADIUS;
  point[0].y = azCompassCenterY - AZ_COMPASS_POINT_HEIGHT/2;
  point[1].x = point[0].x;
  point[1].y = point[0].y + AZ_COMPASS_POINT_HEIGHT;
  point[2].y = azCompassCenterY;
  point[2].x = point[0].x - (3*AZ_COMPASS_POINT_HEIGHT)/2;
  renderPangoText("W", OR_GREEN, BIG_PANGO_FONT, &tWidth, &tHeight, pixmap,
		  point[2].x - 25, azCompassCenterY, 0.0, TRUE, 0);
  gdk_draw_polygon(pixmap, gC[OR_CREAM], TRUE, point, 3);
  point[0].x = azCompassCenterX + AZ_COMPASS_RADIUS;
  point[0].y = azCompassCenterY - AZ_COMPASS_POINT_HEIGHT/2;
  point[1].x = point[0].x;
  point[1].y = point[0].y + AZ_COMPASS_POINT_HEIGHT;
  point[2].y = azCompassCenterY;
  point[2].x = point[0].x + (3*AZ_COMPASS_POINT_HEIGHT)/2;
  renderPangoText("E", OR_GREEN, BIG_PANGO_FONT, &tWidth, &tHeight, pixmap,
		  point[2].x + 15, azCompassCenterY, 0.0, TRUE, 0);
  gdk_draw_polygon(pixmap, gC[OR_CREAM], TRUE, point, 3);
  smallArrow[0].x = -AZ_COMPASS_POINT_HEIGHT/4;
  smallArrow[0].y = 0;
  smallArrow[1].x = smallArrow[0].x + AZ_COMPASS_POINT_HEIGHT/2;
  smallArrow[1].y = smallArrow[0].y;
  smallArrow[2].x = 0;
  smallArrow[2].y = (3*AZ_COMPASS_POINT_HEIGHT)/4;
  sqr2Over2 = sqrtf(2.0)*0.5;
  for (i = 0; i < 3; i++) {
    point[i].x = azCompassCenterX +
      roundf(sqr2Over2*((float)( AZ_COMPASS_RADIUS + smallArrow[i].x + smallArrow[i].y)));
    point[i].y = azCompassCenterY +
      roundf(sqr2Over2*((float)( AZ_COMPASS_RADIUS + smallArrow[i].y - smallArrow[i].x)));
  }
  gdk_draw_polygon(pixmap, gC[OR_CREAM], TRUE, point, 3);
  for (i = 0; i < 3; i++) {
    point[i].x = azCompassCenterX +
      roundf(sqr2Over2*((float)(-AZ_COMPASS_RADIUS - smallArrow[i].x - smallArrow[i].y)));
    point[i].y = azCompassCenterY +
      roundf(sqr2Over2*((float)( AZ_COMPASS_RADIUS + smallArrow[i].y - smallArrow[i].x)));
  }
  gdk_draw_polygon(pixmap, gC[OR_CREAM], TRUE, point, 3);
  for (i = 0; i < 3; i++) {
    point[i].x = azCompassCenterX +
      roundf(sqr2Over2*((float)(-AZ_COMPASS_RADIUS + smallArrow[i].x - smallArrow[i].y)));
    point[i].y = azCompassCenterY +
      roundf(sqr2Over2*((float)(-AZ_COMPASS_RADIUS - smallArrow[i].y - smallArrow[i].x)));
  }
  gdk_draw_polygon(pixmap, gC[OR_CREAM], TRUE, point, 3);
  for (i = 0; i < 3; i++) {
    point[i].x = azCompassCenterX +
      roundf(sqr2Over2*((float)( AZ_COMPASS_RADIUS + smallArrow[i].x + smallArrow[i].y)));
    point[i].y = azCompassCenterY +
      roundf(sqr2Over2*((float)(-AZ_COMPASS_RADIUS - smallArrow[i].y + smallArrow[i].x)));
  }
  gdk_draw_polygon(pixmap, gC[OR_CREAM], TRUE, point, 3);
  fScreenCenterX = (float)azCompassCenterX;
  fScreenCenterY = (float)azCompassCenterY;
  fCompassRadius = (float)AZ_COMPASS_RADIUS;
  fInnerRadius   = (float)(AZ_COMPASS_RADIUS - AZ_COMPASS_TICK_LENGTH);
  fInnerRadius2  = (float)(AZ_COMPASS_RADIUS - AZ_COMPASS_TICK_LENGTH/2);
  i = 0;
  for (az = 0.0; az < 360.0*DEGREES_TO_RADIANS; az += 5.0*DEGREES_TO_RADIANS) {
    sinAz = sinf(az); cosAz = cosf(az);
    xo = fScreenCenterX + fCompassRadius*cosAz;
    yo = fScreenCenterY + fCompassRadius*sinAz;
    if (i & 1) {
      xi = fScreenCenterX + fInnerRadius2*cosAz;
      yi = fScreenCenterY + fInnerRadius2*sinAz;
    } else {
      xi = fScreenCenterX + fInnerRadius*cosAz;
      yi = fScreenCenterY + fInnerRadius*sinAz;
    }
    gdk_draw_line(pixmap, gC[OR_BLUE], (int)xi, (int)yi, (int)xo, (int)yo);
    i++;
  }
  drawAzArrow(FALSE, centerAz);
}

static void fullRedraw(int drawAzSlewCompass)
{
  if (drawAzSlewCompass) {
    drawAzCompass();
  } else {
    if (!inFlashlightMode) {
      removeAllSensitiveAreas();
      redrawScreen();
    }
  }
  gdk_draw_drawable(drawingArea->window,
		    drawingArea->style->fg_gc[GTK_WIDGET_STATE (drawingArea)],
		    pixmap,
		    0,0,0,0,
		    displayWidth, displayHeight);
}

/*
  Change the rate at which the main plot is periodically updated.
 */
void changeUpdateRate(char *requester, int newRate)
{
  if (timerID != (guint)0) {
    g_source_remove(timerID);
    timerID = (guint)0;
  }
  scheduleUpdates(requester, newRate);
}

static int periodicUpdate(gpointer data)
{
  static int menuCount = 0;

  if (appHasFocus || hildon_window_get_is_topmost((HildonWindow *)window) || postageModeDisabled) {
    postageStampUTMM = postageStampLSTMM = -1;
    imAPostageStamp = FALSE;
    if (appHasFocus)
      menuCount = 0;
    else
      menuCount++;
    if (menuCount > 2) {
      if (fastUpdates)
	changeUpdateRate("periodicUpdate (fast)", FAST_UPDATE_RATE);
      else
	changeUpdateRate("periodicUpdate (default)", DEFAULT_UPDATE_RATE);
    }
    dayInc = 0;
    if (skyIsVisible && !(inFlashlightMode || inAzCompassMode)
	&& !(displayingAnOptsPage
	     && ((aboutScreen == ABOUT_SCREEN)
		 || (aboutScreen == BIG_MOONCAL_SCREEN) || (aboutScreen == SMALL_MOONCAL_SCREEN)
		 || (aboutScreen == METEOR_SHOWERS_SCREEN)))) {
      fullRedraw(FALSE);
    }
  } else {
    /*
      The block of code below draws a little display when the app has been minimized by the
      window manager.   This way, instead of showing a squashed version of the normal app
      display, which is useless, a legible display of the UT, LST and moon information is
      shown.
    */
    int lHH, lMM;
    int mustUpdate = FALSE;
    double lTJD, lLST, rLST;
    time_t t;
    struct tm *lGMT = NULL;

    t = time(NULL);
    lGMT = gmtime(&t);
    cDay = lGMT->tm_mday;
    imAPostageStamp = TRUE;
    if (postageStampUTMM != lGMT->tm_min) {
      mustUpdate = TRUE;
      postageStampUTHH = lGMT->tm_hour;
      postageStampUTMM = lGMT->tm_min;
    }
    lTJD = buildTJD(lGMT->tm_year, lGMT->tm_mon, lGMT->tm_mday,
		    lGMT->tm_hour, lGMT->tm_min, lGMT->tm_sec, 0);
    rLST = lSTAtTJD(lTJD);
    lLST = rLST / HOURS_TO_RADIANS;
    lHH = (int)lLST;
    lMM = (int)((lLST - (double)lHH)*60.0);
    if (lMM != postageStampLSTMM) {
      mustUpdate = TRUE;
      postageStampLSTMM = lMM;
      postageStampLSTHH = lHH;
    }
    if (mustUpdate) {
      int i, tWidth, tHeight, color, savedUseCurrentTime;
      static float illum, mag, el;
      static double rA, dec, az, zA;
      char tString[20];

      if (!moonImagesRead)
	readMoonImages();
      savedUseCurrentTime = useCurrentTime;
      useCurrentTime = TRUE;
      needNewTime = TRUE;
      dprintf("New times to display:\tUT = %02d:%02d  LST = %02d:%02d\n",
	      postageStampUTHH, postageStampUTMM, postageStampLSTHH, postageStampLSTMM);
      removeAllSensitiveAreas();
      gdk_draw_rectangle(pixmap, drawingArea->style->black_gc,
			 TRUE, 0, 0,
			 drawingArea->allocation.width,
			 drawingArea->allocation.height);
      sprintf(tString, "UT %02d:%02d", postageStampUTHH, postageStampUTMM);
      renderPangoText(tString, OR_WHITE, HUGE_PANGO_FONT,
		      &tWidth, &tHeight, pixmap, displayWidth >> 1,
		      45, 0.0, TRUE, 0);
      sprintf(tString, "LST %02d:%02d", postageStampLSTHH, postageStampLSTMM);
      renderPangoText(tString, OR_WHITE, HUGE_PANGO_FONT,
		      &tWidth, &tHeight, pixmap, displayWidth >> 1,
		      displayHeight - 53, 0.0, TRUE, 0);
      if (landscapeMode) {
	int side, offset;
	float azp;
	float s = 35.0;
	float r = 90.0;
	float ro = 100.0;
	float cosAz, sinAz, cosEl, sinEl, px[3], py[3], ppx[3], ppy[3];
	static float sunAngle, illum2;
	static double dummy, moonRA, moonDec;
	GdkPoint points[4];

	/*
	  I want the little moon display shown when the app is minimized to show the
	  current moon information even if the app itself is showing information for
	  some other point in time.
	*/
	planetInfo(dataDir, EARTH, lTJD, &rA, &dec, &illum, &mag);
	planetInfo(dataDir, MOON, lTJD, &moonRA, &moonDec, &illum, &mag);
	planetInfo(dataDir, EARTH, lTJD, &dummy, &dummy, &illum2, &mag);
	planetInfo(dataDir, MOON, lTJD+0.04, &dummy, &dummy, &illum2, &mag);
	azZA(moonRA, sin(moonDec), cos(moonDec), &az, &zA, FALSE);
	el = M_HALF_PI - zA;
	if (el > 0.0)
	  color = OR_GREEN;
	else
	  color = OR_RED;
	if (northernHemisphere) {
	  if (illum < illum2)
	    sunAngle = 0.0;
	  else
	    sunAngle = M_PI;
	} else {
	  if (illum > illum2)
	    sunAngle = 0.0;
	  else
	    sunAngle = M_PI;
	}
	drawMoon(210.0, illum, sunAngle, displayWidth >> 1, (displayHeight >> 1) - 5, TRUE, FALSE, FALSE); 
	for (side = 0; side < 2; side++) {
	  if (side == 1) {
	    azZA(rA, sin(dec), cos(dec), &az, &zA, FALSE);
	    el = M_HALF_PI - zA;
	    if (el > 0.0)
	      color = OR_GREEN;
	    else
	      color = OR_RED;
	  }
	  offset = side*(displayWidth - 260);
	  sinEl = sinf(el);         cosEl = cosf(el);
	  azp = M_HALF_PI + az;
	  sinAz = sinf((float)azp); cosAz = cosf((float)azp);
	  gdk_draw_arc(pixmap, gC[OR_BLUE], TRUE, 30+offset, (displayHeight >> 1) - 105,
		       200, 200, 0, FULL_CIRCLE);
	  gdk_draw_arc(pixmap, gC[OR_BLACK], TRUE, 40+offset, (displayHeight >> 1) - 95,
		       180, 180, 90*64, 180*64);
	  if (side == 1) {
	    gdk_draw_arc(pixmap, gC[OR_YELLOW], TRUE, 140+offset, (displayHeight >> 1) - 45,
			 80, 80, 0, FULL_CIRCLE);
	    gdk_draw_arc(pixmap, gC[OR_BLACK], TRUE, 152+offset, (displayHeight >> 1) - 33,
			 56, 56, 0, FULL_CIRCLE);
	    gdk_draw_arc(pixmap, gC[OR_YELLOW], TRUE, 165+offset, (displayHeight >> 1) - 20,
			 30, 30, 0, FULL_CIRCLE);
	  } else
	    drawMoon(40, 0.275, 0.0, 179, (displayHeight >> 1) - 5, FALSE, FALSE, FALSE);
	  px[0] = 0.0;           py[0] = 0.0;
	  px[1] = 0.866025404*s; py[1] = s*0.5;
	  px[2] = px[1];         py[2] = -py[1];
	  for (i = 0; i < 3; i++) {
	    ppx[i] = px[i]*cosEl - py[i]*sinEl;
	    ppy[i] = py[i]*cosEl + px[i]*sinEl;
	    points[i].x = offset + 130 - (int)(r*cosEl + 0.5 - ppx[i]);
	    points[i].y = (displayHeight >> 1) - 5 - (int)(r*sinEl + 0.5 - ppy[i]);
	  }
	  gdk_draw_polygon(pixmap, gC[color], TRUE, points, 3);
	  px[0] = px[1];           py[0] = s*0.1;
	  px[1] = px[0] + r - 30;  py[1] = py[0];
	  px[2] = px[1];           py[2] = -s*0.1;
	  px[3] = px[0];           py[3] = py[2];
	  for (i = 0; i < 4; i++) {
	    ppx[i] = px[i]*cosEl - py[i]*sinEl;
	    ppy[i] = py[i]*cosEl + px[i]*sinEl;
	    points[i].x = offset + 130 - (int)(r*cosEl + 0.5 - ppx[i]);
	    points[i].y = (displayHeight >> 1) - 5 - (int)(r*sinEl + 0.5 - ppy[i]);
	  }
	  gdk_draw_polygon(pixmap, gC[color], TRUE, points, 4);
	  px[0] = 0.0;            py[0] = 0.0;
	  px[1] = -0.866025404*s; py[1] = s*0.5;
	  px[2] = px[1];          py[2] = -py[1];
	  for (i = 0; i < 3; i++) {
	    ppx[i] = px[i]*cosAz - py[i]*sinAz;
	    ppy[i] = py[i]*cosAz + px[i]*sinAz;
	    points[i].x = offset + 130 - (int)(ro*cosAz + 0.5 - ppx[i]);
	    points[i].y = (displayHeight >> 1) - 5 - (int)(ro*sinAz + 0.5 - ppy[i]);
	  }
	  gdk_draw_polygon(pixmap, gC[color], TRUE, points, 3);
	}
      } else { /* Portrait Mode */
	int planet;
	int line = 1;
	double hA;

	/*
	  In portrait mode, we just list the planets which are above the horizon,
	  along with their elevations.
	*/
	for (planet = MERCURY; planet <= SATURN; planet++)
	  if (planet != EARTH) {
	    if (line > 5)
	      gdk_draw_rectangle(pixmap, drawingArea->style->black_gc,
				 TRUE, 0, line*107,
				 drawingArea->allocation.width,
				 drawingArea->allocation.height);
	    planetInfo(dataDir, planet, lTJD, &rA, &dec, &illum, &mag);
	    azZA(rA, sin(dec), cos(dec), &az, &zA, FALSE);
	    el = (M_HALF_PI - zA)/DEGREES_TO_RADIANS;
	    if (el > 0.0) {
	      sprintf(tString, "%2.0f", el);
	      if (planet == MERCURY)
		renderPangoText("Merc.", OR_CREAM, HUGE_PANGO_FONT,
				&tWidth, &tHeight, pixmap, 0, 44 + line*107, 0.0, FALSE, 0);
	      else
		renderPangoText(solarSystemNames[planet], OR_CREAM, HUGE_PANGO_FONT,
				&tWidth, &tHeight, pixmap, 0, 44 + line*107, 0.0, FALSE, 0);
	      hA = rLST - rA;
	      while (hA > M_PI)
		hA -= M_2PI;
	      while (hA < -M_PI)
		hA += M_2PI;
	      if (hA > 0.0)
		color = OR_RED;
	      else
		color = OR_GREEN;
	      renderPangoText(tString, color, HUGE_PANGO_FONT,
			      &tWidth, &tHeight, pixmap, 360, 44 + line*107, 0.0, FALSE, 0);
	      line++;
	    }
	  }
      }
      gdk_draw_drawable(drawingArea->window,
			drawingArea->style->fg_gc[GTK_WIDGET_STATE (drawingArea)],
			pixmap,
			0,0,0,0,
			displayWidth, displayHeight);
      useCurrentTime = savedUseCurrentTime;
    }
  }
  return(TRUE);
}

void scheduleUpdates(char *caller, int rate)
{
  dprintf("scheduleUpdates(%s, %d) timerID = %d\n", caller, rate, (int)timerID);
  if (timerID == (guint)0) {
    timerID = g_timeout_add(rate, periodicUpdate, NULL);
    dprintf("timerID = %d\n", (int)timerID);
  }
}

void switchScreens(void)
{
  magScale1 = VISUAL_LIMIT/limitingMagnitude1;
  magScale2 = VISUAL_LIMIT/limitingMagnitude2;
  if (limitingMag > VISUAL_LIMIT)
    showFaintStars = TRUE;
  else
    showFaintStars = FALSE;
  if (labelMode) {
    limitingMag             = limitingMagnitude1;
    magScale                = magScale1;
    displayPlanetsAsSymbols = FALSE;
    displayConstellations   = FALSE;
    showStars               = showStars1;
    showPlanets             = showPlanets1;
    showComets              = showComets1;
    showNames               = showNames1;
    showMeteors             = showMeteors1;
    showGreatCircles        = showGreatCircles1;
    showDeepSky             = showDeepSky1;
    showBayer               = showBayer1;
    labelMode               = FALSE;
  } else {
    limitingMag             = limitingMagnitude2;
    magScale                = magScale2;
    displayPlanetsAsSymbols = TRUE;
    displayConstellations   = TRUE;
    showStars               = showStars2;
    showNames               = showNames2;
    showMeteors             = showMeteors2;
    showPlanets             = showPlanets2;
    showComets              = showComets2;
    showGreatCircles        = showGreatCircles2;
    showDeepSky             = showDeepSky2;
    showBayer               = showBayer2;
    labelMode               = TRUE;
  }
  shouldSwitchScreens = FALSE;
}

int navBoxDisplayed = FALSE;
sensitiveArea *displayedSA = NULL;

static gboolean buttonPressEvent(GtkWidget *widget, GdkEventButton *event)
{
  if ((aboutScreen == CELESTIAL_NAVIGATION) && displayingAnOptsPage) {
    int x, y;
    int found = FALSE;
    sensitiveArea *sA = sensitiveAreaRoot;

    /*
      Here we do a little something to help the user select a navigational
      object with his finger.   The navigation page shows many objects with a
      small font; it's difficult to select one without visible feedback.
      So here we detect where the user is pushing the screen, and if it
      correspnds to one of the navigational source sensitive areas, we draw a
      white box around it.
     */
    x = event->x; y = event->y;
    while ((sA != NULL) && (!found)) {
      if ((x >= sA->bLCX) && (x <= sA->tRCX)
	    && (y >= sA->bLCY) && (y <= sA->tRCY))
	found = TRUE;
      else
	sA = sA->forwardPointer;
    }
    if (found && (sA->type == SA_NAVIGATION_OBJECT)) {
      gdk_draw_rectangle(drawingArea->window ,gC[OR_WHITE], FALSE, 1, sA->bLCY,
			 displayWidth-2, sA->tRCY - sA->bLCY);
      displayedSA = sA;
      buttonPressed = navBoxDisplayed = TRUE;
    }
  } else {
    clock_gettime(CLOCK_REALTIME, &timeSpecNow);
    buttonPressTime = (double)timeSpecNow.tv_sec + ((double)timeSpecNow.tv_nsec)*1.0e-9;
    /* Get rid of the old user drawn outline, if it exists */
    if (nUserPoints > 0) {
      free(userPoly);
      nUserPoints = 0;
    }
    buttonPressed = TRUE;
    notAPan = FALSE;
  }
  return(TRUE);
}

static gboolean motionNotifyEvent(GtkWidget *widget, GdkEventButton *event)
{
  int x, y;

  if (buttonPressed) {
    x = event->x; y = event->y;
    if (navBoxDisplayed) {
      int found = FALSE;
      sensitiveArea *sA = sensitiveAreaRoot;

      /*
	A box has been drawn around one of the Navigation Page's
	objects.   Here we check to see if the user has moved his
	finger enough to move into the sensitive area for a different
	object.   If so, erase the old box (draw in back) and display
	the new box.
      */
      while ((sA != NULL) && (!found)) {
	if ((x >= sA->bLCX) && (x <= sA->tRCX)
	    && (y >= sA->bLCY) && (y <= sA->tRCY))
	  found = TRUE;
	else
	  sA = sA->forwardPointer;
      }
      if (found && (sA->type == SA_NAVIGATION_OBJECT) && (sA != displayedSA)) {
	gdk_draw_rectangle(drawingArea->window ,gC[OR_BLACK], FALSE, 1,
			   displayedSA->bLCY, displayWidth-2,
			   displayedSA->tRCY - displayedSA->bLCY);
	gdk_draw_rectangle(drawingArea->window ,gC[OR_WHITE], FALSE, 1, sA->bLCY,
			   displayWidth-2, sA->tRCY - sA->bLCY);
	displayedSA = sA;
      }
    } else if (inAzCompassMode) {
      int iTheta;
      float dx, dy;
      
      dx = (float)(azCompassCenterX - x); dy = (float)(azCompassCenterY - y);
      chosenAz = atan2(dy, dx);
      iTheta = 5*roundf(chosenAz / (5.0*DEGREES_TO_RADIANS));
      chosenAz = ((float)iTheta)*DEGREES_TO_RADIANS;
      drawAzArrow(TRUE, chosenAz);
      gdk_flush();
    } else if (!displayingAnOptsPage && !inFlashlightMode) {
      if (nUserPoints == 0) {
	userPoly = malloc(sizeof(GdkPoint));
	if (unlikely(userPoly == NULL)) {
	  perror("malloc of userPoly");
	  exit(ERROR_EXIT);
	}
      } else {
	userPoly = realloc(userPoly, (nUserPoints+1)*sizeof(GdkPoint));
	if (unlikely(userPoly == NULL)) {
	  perror("realloc of userPoly");
	  exit(ERROR_EXIT);
	}
      }
      userPoly[nUserPoints  ].x = x;
      userPoly[nUserPoints++].y = y;
      /*
	Check to see if the pointer has moved a lot (> 100 pixels) while
	the screen has been pressed.  If so, set notAPan flag to disable
	presenting the azimuth compass.   This is required to allow zoom gestures
	which begin in the panning area.
      */
      if (!notAPan)
	if ((x-userPoly[0].x)*(x-userPoly[0].x) +
	    (y-userPoly[0].y)*(y-userPoly[0].y) > 10000)
	  notAPan = TRUE;
      gdk_draw_point(drawingArea->window, gC[OR_GREEN], x, y);
      gdk_flush();
    }
  }
  return(TRUE);
}

/*
  Refresh the screen and start periodic updates when the app gains focus
  (is no longer minimized).
 */
static gboolean focusInEvent(GtkWidget *widget, GdkEventButton *event)
{
  appHasFocus = TRUE;
  imAPostageStamp = FALSE;
  periodicUpdate(NULL);
  if (fastUpdates)
    changeUpdateRate("focusInEvent (fast)", FAST_UPDATE_RATE);
  else
    changeUpdateRate("focusInEvent (default)", DEFAULT_UPDATE_RATE);
  return(TRUE);
}

/*
  Stop periodic updates if the app is minimized (does not have focus)
  in order to save power.
*/
static gboolean focusOutEvent(GtkWidget *widget, GdkEventButton *event)
{
  appHasFocus = FALSE;
  changeUpdateRate("focusOutEvent", ONE_HZ_UPDATE_RATE);
  return(TRUE);
}

static gboolean processTap(int x, int y)
{
  int found = FALSE;
  sensitiveArea *sA = sensitiveAreaRoot;

  dprintf("In processTap, (%d, %d)\n", x, y);
  if (inFlashlightMode) {
    fullscreenStateChanging = TRUE;
    gtk_window_unfullscreen(GTK_WINDOW(window));
    inFlashlightMode = FALSE;
    fullRedraw(FALSE);
  } else {
    while ((sA != NULL) && (!found)) {
      dprintf("Checking item of type %d  BLC: %d, %d  TRC: %d, %d\n",
		sA->type, sA->bLCX, sA->bLCY, sA->tRCX, sA->tRCY);
      if ((x >= sA->bLCX) && (x <= sA->tRCX)
	    && (y >= sA->bLCY) && (y <= sA->tRCY))
	found = TRUE;
      else
	sA = sA->forwardPointer;
    }
    if (found) {
      dprintf("Found sensitive area - type = %d value = %f\n",
	      sA->type, sA->value);
      switch(sA->type) {
      case SA_TOP_AREA:
	if (inFullscreenMode) {
	  fullscreenStateChanging = TRUE;
	  gtk_widget_show(controlButtonBox);
	  gtk_window_unfullscreen(GTK_WINDOW(window));
	  inFullscreenMode = FALSE;
	}
	break;
      case SA_FINGER_PAN_AREA:
	{
	  int centerAzInc, slewCompass;
	  
	  centerAzInc = (int)(0.2*(azSpan/DEGREES_TO_RADIANS) *
			      (double)(x - displayWidth/2)/(double)(displayWidth));
	  dprintf("In finger pan area  x = %d, y = %d, inc by %d degrees\n",
		  x, y, centerAzInc*5);
	  if ((centerAzInc == 0) && !zoomed)
	    slewCompass = TRUE;
	  else
	    slewCompass = FALSE;
	  centerAz += ((float)centerAzInc*5)*DEGREES_TO_RADIANS;
	  floatNormalize0to2pi(&centerAz);
	  centerAzD = (double)centerAz;
	  if ((centerAzInc != 0) || !zoomed)
	    fullRedraw(slewCompass);
	}
	break;
      case SA_MONTH_LEFT_ARROW:
      case SA_MONTH_RIGHT_ARROW:
	smallMooncalDateOffset += sA->value;
	fullRedraw(FALSE);
	break;
      case SA_SOLAR_SYSTEM_BUTTON:
	dayInc = (int)sA->value;
	if (dayInc != 0) {
	  if (dayInc == ZOOM_SOLAR_SYSTEM_IN) {
	    outermostPlanet -= 1;
	    if (outermostPlanet == MOON)
	      outermostPlanet = EARTH;
	    dayInc = 0;
	  } else if (dayInc == ZOOM_SOLAR_SYSTEM_OUT) {
	    outermostPlanet += 1;
	    if (outermostPlanet == MOON)
	      outermostPlanet = MARS;
	    dayInc = 0;
	  }
	  fullRedraw(FALSE);
	}
	break;
      case SA_JOVIAN:
	if ((int)sA->value < 4)
	  jovianMoonsNE = jovianMoonsNW = jovianMoonsSE = jovianMoonsSW = FALSE;
	switch ((int)sA->value) {
	case 0:
	  jovianMoonsNE = TRUE;
	  break;
	case 1:
	  jovianMoonsNW = TRUE;
	  break;
	case 2:
	  jovianMoonsSE = TRUE;
	  break;
	case 3:
	  jovianMoonsSW = TRUE;
	  break;
	case 100:
	  jovianEvents = TRUE;
	  break;
	default:
	  jovianEvents = FALSE;
	}
	writeConfigFile();
	fullRedraw(FALSE);
	break;
      case SA_UPDATE_NAVIGATION:
	updateCelestialNavigationScreen = TRUE;
	fullRedraw(FALSE);
	break;
      case SA_NAVIGATION_OBJECT:
	displayIndividualNavObject = TRUE;
	individualNavObject = (int)sA->value;
	fullRedraw(FALSE);
	break;
      case SA_DISPLAY_NAV_LIST:
	displayIndividualNavObject = FALSE;
	fullRedraw(FALSE);
	break;
      default:
	fprintf(stderr, "Unknown SA type (%d)  BLC: %d, %d  TRC: %d, %d\n",
		sA->type, sA->bLCX, sA->bLCY, sA->tRCX, sA->tRCY);
      }
    } else {
      if (!displayingAnOptsPage) {
	shouldSwitchScreens = TRUE;
	if (constellationsInitialized)
	  switchScreens();
	else {
	  return(TRUE);
	}
	fullRedraw(FALSE);
      }
    }
    dprintf("Tap handler, centerAz = %f\n",
	    centerAz/DEGREES_TO_RADIANS);
  }
  return(TRUE);
}

static void unzoom(void)
{
  float centerAzDeg;

  zoomed = FALSE;
  if (isnan(centerAz) || isnan(centerAzD)) {
    printf("Doing NaN centerAz reset\n");
    centerAz = 270.0*DEGREES_TO_RADIANS;
    centerAzD = (double)centerAz;
  }
  maximumZA  =  95.0*DEGREES_TO_RADIANS;
  azSpan = 90.0*DEGREES_TO_RADIANS;
  minimumZA  = thetaOffset = 0.0;
  piMinusThetaOffset = M_PI - thetaOffset;
  twoPiMinusThetaOffset = M_2PI - thetaOffset;
  centerAzDeg = centerAz/DEGREES_TO_RADIANS;
  centerAzDeg = ((float)((int)(centerAzDeg*0.2 + 0.5)))*5.0;
  centerAz = centerAzDeg*DEGREES_TO_RADIANS;
  centerAzD = (double)centerAz;
  fullRedraw(FALSE);
}

static gboolean buttonReleaseEvent(GtkWidget *widget, GdkEventButton *event)
{
  int x, y, tap;
  double touchDuration;

  navBoxDisplayed = FALSE;
  if (inAzCompassMode) {
    centerAz = chosenAz;
    centerAzD = (double)centerAz;
    buttonPressed = FALSE;
    fullRedraw(TRUE);
    inAzCompassMode = FALSE;
    fullRedraw(FALSE);
  } else {
    clock_gettime(CLOCK_REALTIME, &timeSpecNow);
    buttonReleaseTime = (double)timeSpecNow.tv_sec + ((double)timeSpecNow.tv_nsec)*1.0e-9;
    touchDuration = buttonReleaseTime - buttonPressTime;
    x = event->x; y = event->y;
    if (touchDuration < 0.25)
      tap = TRUE;
    else
      tap = FALSE;
    dprintf("In buttonReleaseEvent, event->button %d (%d,%d) centerAz = %f\n",
	    event->button, x, y, centerAz/DEGREES_TO_RADIANS);
    dprintf("touchDuration = %f, tap = %d\n", touchDuration, tap);
    if (tap || displayingAnOptsPage)
      return(processTap(x, y));
    else if (!zoomed && !notAPan && (y > 4*displayHeight/5))
      /*
	This will cause the azimuth compass to appear if the user presses anywhere
	in the finger-pan area for a prolonged time.
      */
      return(processTap(displayWidth/2, y));
    else {
      /* Check for a gesture */
      if (nUserPoints > 2) {
	int xMin, xMax, yMin, yMax;
	int i, iTemp, pan;
	int x1, x2, y1, y2;
	float area = 0.0;
	float perimeter = 0.0;
	float xCen = 0.0;
	float yCen = 0.0;
	float maxArea, mX, mY, eta, azMin, zA1, azMax, zA2, xSide, ySide;
	double tThetaOffset, dummy;
	
	
	for (i = 0; i < nUserPoints; i++) {
	  x1 = userPoly[i].x; y1 = userPoly[i].y;
	  x2 = userPoly[(i+1) % nUserPoints].x; y2 = userPoly[(i+1) % nUserPoints].y;
	  iTemp = x1*y2 - x2*y1;
	  area += (float)iTemp;
	  perimeter += sqrt((float)((x1-x2)*(x1-x2) + (y1-y2)*(y1-y2)));
	  xCen += (float)((x1+x2) * iTemp);
	  yCen += (float)((y1+y2) * iTemp);
	}
	area = fabs(area)*0.5;
	maxArea = perimeter*perimeter/(4.0*M_PI);
	if (area/maxArea < 0.2)
	  pan = TRUE;
	else
	  pan = FALSE;
	if ((area > 300.0) && (!pan)) {
	  GdkPoint equivRec[4];
	  float ratio;
	  char *zooming = "ZOOMING";
	  
	  gdk_draw_string(drawingArea->window, smallFont, gC[OR_GREEN],
			  displayWidth-gdk_string_width(smallFont, zooming)-10,
			  yTopLabelSkip+gdk_string_height(smallFont, zooming)+10,
			  zooming);
	  xCen = fabs(xCen) / (6.0*area);
	  yCen = fabs(yCen) / (6.0*area);
	  gdk_draw_polygon(drawingArea->window, gC[OR_GREEN], FALSE, userPoly, nUserPoints);
	  ratio = (float)(displayHeight)/(float)(displayWidth);
	  xSide = 0.5 * sqrt(area/ratio); ySide = xSide * ratio;
	  xMin = (int)(xCen - xSide); yMin = (int)(yCen - ySide);
	  xMax = (int)(xCen + xSide); yMax = (int)(yCen + ySide);
	  equivRec[0].x = xMin; equivRec[0].y = yMin; 
	  equivRec[1].x = xMax; equivRec[1].y = yMin; 
	  equivRec[2].x = xMax; equivRec[2].y = yMax; 
	  equivRec[3].x = xMin; equivRec[3].y = yMax; 
	  pixelsToMercator(xCen, yMin, &mX, &mY);
	  invMercator(mX, &eta);
	  invThetaEta(mY, eta, &azMin, &zA1);
	  pixelsToMercator(xCen, yMax, &mX, &mY);
	  invMercator(mX, &eta);
	  invThetaEta(mY, eta, &azMax, &zA2);
	  if (zA2 < zA1) {
	    float tempZA;
	    
	    tempZA = zA2;
	    zA2 = zA1;
	    zA1 = tempZA;
	  }
	  if (azMax < azMin) {
	    float azTemp;
	    
	    azTemp = azMax;
	    azMax = azMin;
	    azMin = azTemp;
	  }
	  if (zA2 > ABSOLUTE_MAX_ZA)
	    zA2 = ABSOLUTE_MAX_ZA;
	  maximumZA = zA2;
	  minimumZA = zA1;
	  thetaEta((double)((azMax+azMin)*0.5), minimumZA, &tThetaOffset, &dummy);
	  thetaOffset += tThetaOffset;
	  piMinusThetaOffset = M_PI - thetaOffset;
	  twoPiMinusThetaOffset = piMinusThetaOffset + M_PI;
	  doubleNormalize0to2pi(&piMinusThetaOffset);
	  doubleNormalize0to2pi(&twoPiMinusThetaOffset);
	  centerAz = (azMin+azMax+M_PI)*0.5;
	  centerAzD = (double)centerAz;
	  pixelsToMercator(xMin, yMax, &mX, &mY);
	  invMercator(mX, &eta);
	  invThetaEta(mY, eta, &azMin, &zA1);
	  pixelsToMercator(xMax, yMax, &mX, &mY);
	  invMercator(mX, &eta);
	  invThetaEta(mY, eta, &azMax, &zA2);
	  azSpan = fabs(azMax - azMin);
	  gdk_draw_polygon(drawingArea->window, gC[OR_RED], FALSE, equivRec, 4);
	  zoomed = TRUE;
	  gdk_flush();
	  fullRedraw(FALSE);
	} else if ((maxArea > 750.0) && pan && zoomed) {
	  int deltaX, deltaY;
	  float zACen, azCen, deltaZA, theta;
	  GdkPoint points[5];
	  
	  /*
	    Use the difference in between the first ant last points in the polygon
	    to define the distance (in both X and Y) to pan.
	  */
	  deltaX = userPoly[0].x - userPoly[nUserPoints-1].x;
	  deltaY = userPoly[0].y - userPoly[nUserPoints-1].y;
	  theta = atan2f(-(float)deltaY, -(float)deltaX);
	  xCen = (float)(displayWidth/2 + deltaX);
	  yCen = (float)(displayHeight/2 + deltaY);
	  pixelsToMercator(xCen, yCen, &mX, &mY);
	  invMercator(mX, &eta);
	  invThetaEta(mY, eta, &azCen, &zACen);
	  deltaZA = 0.5*(maximumZA - minimumZA);
	  maximumZA = zACen + deltaZA;
	  minimumZA = zACen - deltaZA;
	  thetaEta((double)azCen, minimumZA, &tThetaOffset, &dummy);
	  thetaOffset += tThetaOffset;
	  piMinusThetaOffset = M_PI - thetaOffset;
	  twoPiMinusThetaOffset = piMinusThetaOffset + M_PI;
	  doubleNormalize0to2pi(&piMinusThetaOffset);
	  doubleNormalize0to2pi(&twoPiMinusThetaOffset);
	  centerAz = azCen + M_HALF_PI;
	  centerAzD = (double)centerAz;
	  points[0].x = userPoly[0].x;        points[0].y = userPoly[0].y;
	  points[1].x = points[0].x - deltaX; points[1].y = points[0].y - deltaY;
	  gdk_draw_lines(drawingArea->window, gC[OR_RED], points, 2);
	  points[2].x = points[1].x + ((int)(20.0*cosf(theta + 0.75*M_PI)));
	  points[2].y = points[1].y + ((int)(20.0*sinf(theta + 0.75*M_PI)));
	  points[3].x = points[1].x + ((int)(20.0*cosf(theta + 1.25*M_PI)));
	  points[3].y = points[1].y + ((int)(20.0*sinf(theta + 1.25*M_PI)));
	  points[4].x = points[1].x;
	  points[4].y = points[1].y;
	  gdk_draw_polygon(drawingArea->window, gC[OR_RED], TRUE, &points[2], 3);
	  gdk_flush();
	  fullRedraw(FALSE);
	} else {
	  if (zoomed)
	    unzoom();
	  else
	    return(processTap(x, y));
	}
      } else {
	if (zoomed)
	  unzoom();
	else
	  return(processTap(x, y));
      }
    }
  }
  buttonPressed = FALSE;
  return(TRUE);
}

int tokenCheck(char *line, char *token, int type, void *value)

#define INT_TOKEN     0
#define DOUBLE_TOKEN  1
#define FLOAT_TOKEN   2
#define STRING_TOKEN  3

/*
  Scan "line" for "token".   If found, read the value into
  "value" as an integer, double or float, depending on "type".

  Return TRUE IFF the token is seen.
*/
{
  if (strstr(line, token)) {
    int nRead;

    switch (type) {
    case INT_TOKEN:
      nRead = sscanf(&((char *)strstr(line, token))[strlen(token)+1], "%d", (int *)value);
      if (nRead != 1) {
	fprintf(stderr, "Unable to parse config file line \"%s\"\n", line);
	return(FALSE);
      }
      break;
    case DOUBLE_TOKEN:
      nRead = sscanf(&((char *)strstr(line, token))[strlen(token)+1], "%lf", (double *)value);
      if (nRead != 1) {
	fprintf(stderr, "Unable to parse config file line \"%s\"\n", line);
	return(FALSE);
      }
      break;
    case FLOAT_TOKEN:
      nRead = sscanf(&((char *)strstr(line, token))[strlen(token)+1], "%f", (float *)value);
      if (nRead != 1) {
	fprintf(stderr, "Unable to parse config file line \"%s\"\n", line);
	return(FALSE);
      }
      break;
    case STRING_TOKEN:
      nRead = sscanf(&((char *)strstr(line, token))[strlen(token)+1], "%s", (char *)value);
      if (nRead != 1) {
	fprintf(stderr, "Unable to parse config file line \"%s\"\n", line);
	return(FALSE);
      }
      break;
    default:
      fprintf(stderr, "Unrecognized type (%d) passed to tokenCheck\n", type);
    }
    return(TRUE);
  } else
    return(FALSE);
}

/*
  Read in the values that the user can set.   All the parameters which can be set
  via the config file have default hard-coded values, so if a particular
  parameter is not present in the config file, or if the entire
*/
void parseConfigFile(void)
{
  int eOF = FALSE;
  int lineNumber = 0;
  int configFD;
  char inLine[MAX_FILE_NAME_SIZE];

  sprintf(inLine, "%s/config", dataDir);
  configFD = open(inLine, O_RDONLY);
  if (configFD < 0) {
    perror("config");
    fprintf(stderr, "Full file name was \"%s\"\n", inLine);
    return;
  }
  while (!eOF) {
    lineNumber++;
    if (getLine(configFD, &inLine[0], &eOF))
      if (strlen(inLine) > 0) {
	if (tokenCheck(inLine, "LIMITING_MAGNITUDE1", FLOAT_TOKEN, &limitingMagnitude1)) {
	  magScale1 = VISUAL_LIMIT/limitingMagnitude1;
	  if (!labelMode) {
	    magScale = magScale1;
	    limitingMag = limitingMagnitude1;
	  }
	}
	if (tokenCheck(inLine, "LIMITING_MAGNITUDE2", FLOAT_TOKEN,
		       &limitingMagnitude2)) {
	  magScale2 = VISUAL_LIMIT/limitingMagnitude2;
	  if (labelMode) {
	    magScale = magScale2;
	    limitingMag = limitingMagnitude2;
	  }
	}
	if (tokenCheck(inLine, "CHINESE_COLOR_SCHEME", INT_TOKEN, &chineseColorScheme))
	  if (tokenCheck(inLine, "SHOW_GREAT_CIRCLES1", INT_TOKEN, &showGreatCircles1))
	    if (!labelMode)
	      showGreatCircles = showGreatCircles1;
	if (tokenCheck(inLine, "SHOW_GREAT_CIRCLES2", INT_TOKEN, &showGreatCircles2))
	  if (labelMode)
	    showGreatCircles = showGreatCircles2;
	tokenCheck(inLine, "JOVIAN_MOONS_NE", INT_TOKEN, &jovianMoonsNE);
	tokenCheck(inLine, "JOVIAN_MOONS_NW", INT_TOKEN, &jovianMoonsNW);
	tokenCheck(inLine, "JOVIAN_MOONS_SE", INT_TOKEN, &jovianMoonsSE);
	tokenCheck(inLine, "JOVIAN_MOONS_SW", INT_TOKEN, &jovianMoonsSW);
	tokenCheck(inLine, "LIST_LOCAL_ECLIPSES_ONLY", INT_TOKEN, &listLocalEclipsesOnly);
	tokenCheck(inLine, "LIST_PENUMBRAL_ECLIPSES", INT_TOKEN, &listPenumbralEclipses);
	tokenCheck(inLine, "LIST_PARTIAL_ECLIPSES", INT_TOKEN, &listPartialEclipses);
	tokenCheck(inLine, "LIST_TOTAL_ECLIPSES", INT_TOKEN, &listTotalEclipses);
	tokenCheck(inLine, "USE_ASTERISMS", INT_TOKEN, &useAsterisms);
	if (tokenCheck(inLine, "SHOW_DEEP_SKY1", INT_TOKEN, &showDeepSky1))
	  if (!labelMode)
	    showDeepSky = showDeepSky1;
	if (tokenCheck(inLine, "SHOW_DEEP_SKY2", INT_TOKEN, &showDeepSky2))
	  if (labelMode)
	    showDeepSky = showDeepSky2;
	if (tokenCheck(inLine, "SHOW_BAYER1", INT_TOKEN, &showBayer1))
	  if (!labelMode)
	    showBayer = showBayer1;
	if (tokenCheck(inLine, "SHOW_BAYER2", INT_TOKEN, &showBayer2))
	  if (labelMode)
	    showBayer = showBayer2;
	if (tokenCheck(inLine, "SHOW_STARS1", INT_TOKEN, &showStars1) && (!labelMode))
	  showStars = showStars1;
	if (tokenCheck(inLine, "SHOW_STARS2", INT_TOKEN, &showStars2) && (labelMode))
	  showStars = showStars2;
	if (tokenCheck(inLine, "SHOW_PLANETS1", INT_TOKEN, &showPlanets1) && (!labelMode))
	  showPlanets = showPlanets1;
	if (tokenCheck(inLine, "SHOW_PLANETS2", INT_TOKEN, &showPlanets2) && (labelMode))
	  showPlanets = showPlanets2;
	if (tokenCheck(inLine, "SHOW_COMETS1", INT_TOKEN, &showComets1) && (!labelMode))
	  showComets = showComets1;
	if (tokenCheck(inLine, "SHOW_COMETS2", INT_TOKEN, &showComets2) && (labelMode))
	  showComets = showComets2;
	if (tokenCheck(inLine, "SHOW_STAR_NAMES1", INT_TOKEN, &showNames1) && (!labelMode))
	  showNames = showNames1;
	if (tokenCheck(inLine, "SHOW_STAR_NAMES2", INT_TOKEN, &showNames2) && (labelMode))
	  showNames = showNames2;
	if (tokenCheck(inLine, "SHOW_METEORS1", INT_TOKEN, &showMeteors1) && (!labelMode))
	  showMeteors = showMeteors1;
	if (tokenCheck(inLine, "SHOW_METEORS2", INT_TOKEN, &showMeteors2) && (labelMode))
	  showMeteors = showMeteors2;
	tokenCheck(inLine, "DEBUG_MESSAGES_ON", INT_TOKEN, &debugMessagesOn);
	tokenCheck(inLine, "USE_GPSD", INT_TOKEN, &useGPSD);
	if (tokenCheck(inLine, "INITIAL_AZIMUTH", FLOAT_TOKEN, &initialAzimuth)) {
	  centerAzD = (initialAzimuth + 90.0)*DEGREES_TO_RADIANS;
	  centerAz  = (float)centerAzD;
	}
	if (tokenCheck(inLine, "DEFAULT_LOCATION_NAME", STRING_TOKEN, &locationName[0])) {
	  int i;

	  for (i = 0; i < strlen(locationName); i++)
	    if (locationName[i] == '_')
	      locationName[i] = ' ';
	}
	if (tokenCheck(inLine, "DEFAULT_LOCATION_LATITUDE", DOUBLE_TOKEN, &latitude)) {
	  latitude *= DEGREES_TO_RADIANS;
	  locationChanged = TRUE;
	}
	if (tokenCheck(inLine, "DEFAULT_LOCATION_LONGITUDE", DOUBLE_TOKEN, &longitude)) {
	  longitude *= DEGREES_TO_RADIANS;
	  locationChanged = TRUE;
	}
      }
  }
  close(configFD);
  if (limitingMag > VISUAL_LIMIT)
    showFaintStars = TRUE;
  else
    showFaintStars = FALSE;
}

void checkTimeSpinBoxes(void)
{
  guint hour, min, year, month, day;

  postageModeDisabled = FALSE;
  needNewTime = TRUE;
  if (useTextTime) {
    cYear   = gtk_spin_button_get_value((GtkSpinButton *)yearSpin);
    if (cYear <= 0)
      cYear++; /* There is no year 0 */
    cMonth   = gtk_spin_button_get_value((GtkSpinButton *)monthSpin);
    cDay     = gtk_spin_button_get_value((GtkSpinButton *)daySpin);
    cHour    = gtk_spin_button_get_value((GtkSpinButton *)hourSpin);
    cMinute  = gtk_spin_button_get_value((GtkSpinButton *)minuteSpin);
    cSecond  = gtk_spin_button_get_value((GtkSpinButton *)secondSpin);
    cNSecond = 0;
  } else if (useJulianTime)
    julianDate   = gtk_spin_button_get_value((GtkSpinButton *)julSpin);
  if (useCalendarTime) {
    hildon_time_button_get_time((HildonTimeButton *)hildonTimeButton, &hour, &min);
    hildon_date_button_get_date((HildonDateButton *)hildonDateButton, &year, &month, &day);
    cYear = year;
    cMonth = month+1;
    cDay = day;
    cHour = hour;
    cMinute = min;
    cSecond = cNSecond = 0;
  }	 
}

static GtkWidget *createMagTouchSelector(double dMin, double dMax)
{
  double deg;
  char degString[10];
  GtkWidget *selector;
  GtkListStore *model;
  GtkTreeIter iter;
  HildonTouchSelectorColumn *column = NULL;

  selector = hildon_touch_selector_new();

  model = gtk_list_store_new(1, G_TYPE_STRING);

  for (deg = dMax; deg >= dMin; deg -= 0.2) {
    sprintf(degString, "%4.1f", deg);
    gtk_list_store_append(model, &iter);
    gtk_list_store_set(model, &iter, 0, degString, -1);
  }

  column = hildon_touch_selector_append_text_column(HILDON_TOUCH_SELECTOR(selector),
                                                     GTK_TREE_MODEL(model), TRUE);
  g_object_set(G_OBJECT(column), "text-column", 0, NULL);

  return selector;
}

static void onMagValueChanged(HildonPickerButton * button, gpointer data)
{
  magButtonAdjusted = TRUE;
}

void gtk_table_remove(GtkContainer *container,
		      GtkWidget    *widget)
{
  GtkTable *table;
  GtkTableChild *child;
  GList *children;
  
  g_return_if_fail (GTK_IS_TABLE (container));
  g_return_if_fail (widget != NULL);
  
  table = GTK_TABLE (container);
  children = table->children;
  
  while (children)
    {
      child = children->data;
      children = children->next;
      
      if (child->widget == widget)
	{
	  gboolean was_visible = GTK_WIDGET_VISIBLE (widget);
	  
	  gtk_widget_unparent (widget);
	  
	  table->children = g_list_remove (table->children, child);
	  g_free (child);
	  
	  if (was_visible && GTK_WIDGET_VISIBLE (container))
	    gtk_widget_queue_resize (GTK_WIDGET (container));
	  break;
	}
    }
}

void setTimeSensitivities(void)
{
  gtk_toggle_button_set_active((GtkToggleButton *)currentTimeButton, useCurrentTime);
  gtk_toggle_button_set_active((GtkToggleButton *)textTimeButton, useTextTime);
  gtk_toggle_button_set_active((GtkToggleButton *)julianTimeButton, useJulianTime);
  gtk_toggle_button_set_active((GtkToggleButton *)calendarTimeButton, useCalendarTime);

  gtk_widget_set_sensitive(cDateLabel, useTextTime);
  gtk_widget_set_sensitive(cTimeLabel, useTextTime);
  gtk_widget_set_sensitive(yearSpin, useTextTime);
  gtk_widget_set_sensitive(monthSpin, useTextTime);
  gtk_widget_set_sensitive(daySpin, useTextTime);
  gtk_widget_set_sensitive(hourSpin, useTextTime);
  gtk_widget_set_sensitive(minuteSpin, useTextTime);
  gtk_widget_set_sensitive(secondSpin, useTextTime);

  gtk_widget_set_sensitive(julSpin, useJulianTime);
  gtk_widget_set_sensitive(julSpinLabel, useJulianTime);

  gtk_widget_set_sensitive(hildonTimeButton, useCalendarTime);
  gtk_widget_set_sensitive(hildonDateButton, useCalendarTime);
  
}

void setLocationSensitivities(void)
{
  gtk_toggle_button_set_active((GtkToggleButton *)gPSButton, useGPSD);
  gtk_toggle_button_set_active((GtkToggleButton *)customLocationButton, useCustomLocation);
  gtk_toggle_button_set_active((GtkToggleButton *)menuLocationButton, useMenuLocation);
  if (!useMenuLocation) {
    if (locationNameIsDisplayed)
      gtk_widget_hide(displayedLocationName);
    if (menuShowing)
      gtk_widget_hide(activeMenu);
  } else {
    if (locationNameIsDisplayed)
      gtk_widget_show(displayedLocationName);
    if (menuShowing) 
      gtk_widget_show(activeMenu);
  }

  gtk_widget_set_sensitive(locationNameLabel, useCustomLocation);
  gtk_widget_set_sensitive(locationNameText, useCustomLocation);
  gtk_widget_set_sensitive(longButton, useCustomLocation);
  gtk_widget_set_sensitive(eastButton, useCustomLocation);
  gtk_widget_set_sensitive(westButton, useCustomLocation);
  gtk_widget_set_sensitive(northButton, useCustomLocation);
  gtk_widget_set_sensitive(southButton, useCustomLocation);
  gtk_widget_set_sensitive(latButton, useCustomLocation);
  gtk_widget_set_sensitive(regionMenu, useMenuLocation);

  /*
    The following complicated nest of conditionals attempts to make
    certain that the "Save in Private Menu" button only shows up
    when it should.   It should be displayed if we are in
    Custom location mode, or menu mode if the menu is not the
    private menu, and if the location name is displayed.
   */
  if (useGPSD) {
    if (saveAsPrivateShowing) {
      gtk_widget_hide(saveAsPrivate);
      saveAsPrivateShowing = FALSE;
    }
    if (deleteFromPrivateShowing) {
      gtk_widget_hide(deleteFromPrivate);
      deleteFromPrivateShowing = FALSE;
    }
  } else if (useCustomLocation) {
    if (!saveAsPrivateShowing) {
      gtk_widget_show(saveAsPrivate);
      saveAsPrivateShowing = TRUE;
    }
    if (deleteFromPrivateShowing) {
      gtk_widget_hide(deleteFromPrivate);
      deleteFromPrivateShowing = FALSE;
    }
  } else if (useMenuLocation && !privateMenuShowing) {
    if (locationNameIsDisplayed) {
      if (!saveAsPrivateShowing) {
	gtk_widget_show(saveAsPrivate);
	saveAsPrivateShowing = TRUE;
      }
    } else {
      if (saveAsPrivateShowing) {
	gtk_widget_hide(saveAsPrivate);
	saveAsPrivateShowing = FALSE;
      }
    }
    if (deleteFromPrivateShowing) {
      gtk_widget_hide(deleteFromPrivate);
      deleteFromPrivateShowing = FALSE;
    }
  } else if (useMenuLocation && privateMenuShowing) {
    if (saveAsPrivateShowing) {
      gtk_widget_hide(saveAsPrivate);
      saveAsPrivateShowing = FALSE;
    }
    if (!deleteFromPrivateShowing) {
      gtk_widget_show(deleteFromPrivate);
      deleteFromPrivateShowing = TRUE;
    }
  } else {
    if (saveAsPrivateShowing) {
      gtk_widget_hide(saveAsPrivate);
      saveAsPrivateShowing = FALSE;
    }
    if (deleteFromPrivateShowing) {
      gtk_widget_hide(deleteFromPrivate);
      deleteFromPrivateShowing = FALSE;
    }
  }
}

void checkButtonCallback(GtkButton *button, gpointer userData)
{
  if (strstr("showStars1", userData) != NULL) {
    if (gtk_toggle_button_get_active((GtkToggleButton *)starCheckButton1)) {
      optionsModified = TRUE;
      showStars1 = TRUE;
    } else {
      optionsModified = TRUE;
      showStars1 = FALSE;
    }
    if (!labelMode)
      showStars = showStars1;
  } else if (strstr("showStars2", userData) != NULL) {
    if (gtk_toggle_button_get_active((GtkToggleButton *)starCheckButton2)) {
      optionsModified = TRUE;
      showStars2 = TRUE;
    } else {
      optionsModified = TRUE;
      showStars2 = FALSE;
    }
    if (labelMode)
      showStars = showStars2;
  } else if (strstr("showNames1", userData) != NULL) {
    if (gtk_toggle_button_get_active((GtkToggleButton *)starNameCheckButton1)) {
      optionsModified = TRUE;
      showNames1 = TRUE;
    } else {
      optionsModified = TRUE;
      showNames1 = FALSE;
    }
    if (!labelMode)
      showNames = showNames1;
  } else if (strstr("showNames2", userData) != NULL) {
    if (gtk_toggle_button_get_active((GtkToggleButton *)starNameCheckButton2)) {
      optionsModified = TRUE;
      showNames2 = TRUE;
    } else {
      optionsModified = TRUE;
      showNames2 = FALSE;
    }
    if (labelMode)
      showNames = showNames2;
  } else if (strstr("showMeteors1", userData) != NULL) {
    if (gtk_toggle_button_get_active((GtkToggleButton *)meteorCheckButton1)) {
      optionsModified = TRUE;
      showMeteors1 = TRUE;
    } else {
      optionsModified = TRUE;
      showMeteors1 = FALSE;
    }
    if (!labelMode)
      showMeteors = showMeteors1;
  } else if (strstr("showMeteors2", userData) != NULL) {
    if (gtk_toggle_button_get_active((GtkToggleButton *)meteorCheckButton2)) {
      optionsModified = TRUE;
      showMeteors2 = TRUE;
    } else {
      optionsModified = TRUE;
      showMeteors2 = FALSE;
    }
    if (labelMode)
      showMeteors = showMeteors2;
  } else if (strstr("showPlanets1", userData) != NULL) {
    if (gtk_toggle_button_get_active((GtkToggleButton *)planetCheckButton1)) {
      optionsModified = TRUE;
      showPlanets1 = TRUE;
    } else {
      optionsModified = TRUE;
      showPlanets1 = FALSE;
    }
    if (!labelMode)
      showPlanets = showPlanets1;
  } else if (strstr("showPlanets2", userData) != NULL) {
    if (gtk_toggle_button_get_active((GtkToggleButton *)planetCheckButton2)) {
      optionsModified = TRUE;
      showPlanets2 = TRUE;
    } else {
      optionsModified = TRUE;
      showPlanets2 = FALSE;
    }
    if (labelMode)
      showPlanets = showPlanets2;
  } else if (strstr("showComets1", userData) != NULL) {
    if (gtk_toggle_button_get_active((GtkToggleButton *)cometCheckButton1)) {
      optionsModified = TRUE;
      showComets1 = TRUE;
    } else {
      optionsModified = TRUE;
      showComets1 = FALSE;
    }
    if (!labelMode)
      showComets = showComets1;
  } else if (strstr("showComets2", userData) != NULL) {
    if (gtk_toggle_button_get_active((GtkToggleButton *)cometCheckButton2)) {
      optionsModified = TRUE;
      showComets2 = TRUE;
    } else {
      optionsModified = TRUE;
      showComets2 = FALSE;
    }
    if (labelMode)
      showComets = showComets2;

  } else if (strstr("showGreatCircles1", userData) != NULL) {
    if (gtk_toggle_button_get_active((GtkToggleButton *)greatCirclesCheckButton1)) {
      optionsModified = TRUE;
      showGreatCircles1 = TRUE;
    } else {
      optionsModified = TRUE;
      showGreatCircles1 = FALSE;
    }
    if (!labelMode)
      showGreatCircles = showGreatCircles1;
  } else if (strstr("showGreatCircles2", userData) != NULL) {
    if (gtk_toggle_button_get_active((GtkToggleButton *)greatCirclesCheckButton2)) {
      optionsModified = TRUE;
      showGreatCircles2 = TRUE;
    } else {
      optionsModified = TRUE;
      showGreatCircles2 = FALSE;
    }
    if (labelMode)
      showGreatCircles = showGreatCircles2;
  } else if (strstr("useAsterisms", userData) != NULL) {
    if (gtk_toggle_button_get_active((GtkToggleButton *)useAsterismsCheckButton)) {
      optionsModified = TRUE;
      useAsterisms = TRUE;
    } else {
      optionsModified = TRUE;
      useAsterisms = FALSE;
    }
  } else if (strstr("showDeepSky1", userData) != NULL) {
    if (gtk_toggle_button_get_active((GtkToggleButton *)deepSkyCheckButton1)) {
      optionsModified = TRUE;
      showDeepSky1 = TRUE;
    } else {
      optionsModified = TRUE;
      showDeepSky1 = FALSE;
    }
    if (!labelMode)
      showDeepSky = showDeepSky1;
  } else if (strstr("showDeepSky2", userData) != NULL) {
    if (gtk_toggle_button_get_active((GtkToggleButton *)deepSkyCheckButton2)) {
      optionsModified = TRUE;
      showDeepSky2 = TRUE;
    } else {
      optionsModified = TRUE;
      showDeepSky2 = FALSE;
    }
    if (labelMode)
      showDeepSky = showDeepSky2;
  } else if (strstr("showBayer1", userData) != NULL) {
    if (gtk_toggle_button_get_active((GtkToggleButton *)bayerButton1)) {
      optionsModified = TRUE;
      showBayer1 = TRUE;
    } else {
      optionsModified = TRUE;
      showBayer1 = FALSE;
    }
    if (!labelMode)
      showBayer = showBayer1;
  } else if (strstr("showBayer2", userData) != NULL) {
    if (gtk_toggle_button_get_active((GtkToggleButton *)bayerButton2)) {
      optionsModified = TRUE;
      showBayer2 = TRUE;
    } else {
      optionsModified = TRUE;
      showBayer2 = FALSE;
    }
    if (labelMode)
      showBayer = showBayer2;
  } else if (strstr("gPSButton", userData) != NULL) {
    if (gtk_toggle_button_get_active((GtkToggleButton *)gPSButton)) {
      optionsModified = TRUE;
      useGPSD = TRUE;
      forceNewPosition = TRUE;
      useCustomLocation = FALSE;
      useMenuLocation = FALSE;
    } else {
      optionsModified = TRUE;
      useGPSD = FALSE;
    }
    setLocationSensitivities();
  } else if (strstr("customLocationButton", userData) != NULL) {
    if (gtk_toggle_button_get_active((GtkToggleButton *)customLocationButton)) {
      optionsModified = TRUE;
      useCustomLocation = TRUE;
      useMenuLocation = FALSE;
      useGPSD = FALSE;
    } else {
      optionsModified = TRUE;
      useCustomLocation = FALSE;
    }
    setLocationSensitivities();
  } else if (strstr("currentTimeButton", userData) != NULL) {
    optionsModified = TRUE;
    if (gtk_toggle_button_get_active((GtkToggleButton *)currentTimeButton)) {
      useCurrentTime = TRUE;
      useTextTime = FALSE;
      useCalendarTime = FALSE;
      useJulianTime = FALSE;
    } else {
      useCurrentTime = FALSE;
    }
    setTimeSensitivities();
  } else if (strstr("textTimeButton", userData) != NULL) {
    optionsModified = TRUE;
    if (gtk_toggle_button_get_active((GtkToggleButton *)textTimeButton)) {
      useCurrentTime = FALSE;
      useCalendarTime = FALSE;
      useTextTime = TRUE;
      useJulianTime = FALSE;
    } else {
      useTextTime = FALSE;
    }
    setTimeSensitivities();
  } else if (strstr("julianTimeButton", userData) != NULL) {
    optionsModified = TRUE;
    if (gtk_toggle_button_get_active((GtkToggleButton *)julianTimeButton)) {
      useCurrentTime = FALSE;
      useTextTime = FALSE;
      useCalendarTime = FALSE;
      useJulianTime = TRUE;
    } else {
      useJulianTime = FALSE;
    }
    setTimeSensitivities();
  } else if (strstr("calendarTimeButton", userData) != NULL) {
    optionsModified = TRUE;
    if (gtk_toggle_button_get_active((GtkToggleButton *)calendarTimeButton)) {
      useCurrentTime = FALSE;
      useTextTime = FALSE;
      useJulianTime = FALSE;
      useCalendarTime = TRUE;
    } else {
      useCalendarTime = FALSE;
    }
    setTimeSensitivities();
  } else if (strstr("menuLocationButton", userData) != NULL) {
    if (gtk_toggle_button_get_active((GtkToggleButton *)menuLocationButton)) {
      optionsModified = TRUE;
      useCustomLocation = FALSE;
      useMenuLocation = TRUE;
      useGPSD = FALSE;
    } else {
      optionsModified = TRUE;
      useMenuLocation = FALSE;
    }
    setLocationSensitivities();
  } else
    fprintf(stderr, "checkButtonCallback passed unknown string \"%s\"\n", (char *)userData);
}

/*
  This function is called when the stackable window holding the
  "Displayed Items" option is destroyed.   Here we write the new
  user choices to the configuration file, and force the screen
  to be redrawn, so that the newly selected options take immediate
  effect.
*/
void checkItems(void)
{
  char *result;

  postageModeDisabled = FALSE;
  if (magButtonAdjusted) {
    result = (char *)hildon_button_get_value(HILDON_BUTTON(magButton1));
    sscanf(result, "%f", &limitingMagnitude1);
    result = (char *)hildon_button_get_value(HILDON_BUTTON(magButton2));
    sscanf(result, "%f", &limitingMagnitude2);
    optionsModified = TRUE;
    magScale1 = VISUAL_LIMIT/limitingMagnitude1;
    magScale2 = VISUAL_LIMIT/limitingMagnitude2;
    if (labelMode) {
      limitingMag = limitingMagnitude2;
      magScale = magScale2;
    } else {
      limitingMag = limitingMagnitude1;
      magScale = magScale1;
    }
    if (limitingMag > VISUAL_LIMIT)
      showFaintStars = TRUE;
    else
      showFaintStars = FALSE;
    magButtonAdjusted = FALSE;
    optionsModified = TRUE;
  }
  if (optionsModified) {
    writeConfigFile();
    fullRedraw(FALSE);
    optionsModified = FALSE;
  }
}

/*
  This function is called when the "Displayed Items" menu option is
  selected.   It allows you to select which types of objects will
  be displayed on each of two pages, set the magnitude limes, etc.
 */
void itemsButtonClicked(GtkButton *button, gpointer userData)
{
  static GtkWidget *column1Label, *column2Label, *magSelector1,
    *itemsStackable, *magSelector2;
  char scratchString[100];
  int row = 0;

  postageModeDisabled = TRUE;
  itemsTable = gtk_table_new(16, 2, FALSE);
  column1Label = gtk_label_new("Screen 1");
  
  column2Label = gtk_label_new("Screen 2");

  if (haveFaintStars)
    magSelector1 = createMagTouchSelector(-1.0, 9.0);
  else
    magSelector1 = createMagTouchSelector(-1.0, VISUAL_LIMIT);
  magButton1 = hildon_picker_button_new(HILDON_SIZE_AUTO, HILDON_BUTTON_ARRANGEMENT_HORIZONTAL);
  hildon_picker_button_set_done_button_text(HILDON_PICKER_BUTTON(magButton1),
                                             "Done");
  hildon_button_set_title(HILDON_BUTTON(magButton1), "Magnitude");
  hildon_picker_button_set_selector(HILDON_PICKER_BUTTON(magButton1),
                                     HILDON_TOUCH_SELECTOR(magSelector1));
  g_signal_connect(G_OBJECT(magButton1), "value-changed",
                    G_CALLBACK(onMagValueChanged), NULL);
  sprintf(scratchString, "%4.1f", limitingMagnitude1);
  hildon_button_set_value(HILDON_BUTTON(magButton1), scratchString);

  magSelector2 = createMagTouchSelector(-1.0, VISUAL_LIMIT);
  magButton2 = hildon_picker_button_new(HILDON_SIZE_AUTO, HILDON_BUTTON_ARRANGEMENT_HORIZONTAL);
  hildon_picker_button_set_done_button_text(HILDON_PICKER_BUTTON(magButton2),
                                             "Done");
  hildon_button_set_title(HILDON_BUTTON(magButton2), "Magnitude");
  hildon_picker_button_set_selector(HILDON_PICKER_BUTTON(magButton2),
                                     HILDON_TOUCH_SELECTOR(magSelector2));
  g_signal_connect(G_OBJECT(magButton2), "value-changed",
                    G_CALLBACK(onMagValueChanged), NULL);
  sprintf(scratchString, "%4.1f", limitingMagnitude2);
  hildon_button_set_value(HILDON_BUTTON(magButton2), scratchString);

  starCheckButton1 = (GtkCheckButton *)gtk_check_button_new_with_label ("Display Stars");
  gtk_toggle_button_set_active((GtkToggleButton *)starCheckButton1, showStars1);
  g_signal_connect (G_OBJECT(starCheckButton1), "clicked",
		    G_CALLBACK(checkButtonCallback), "showStars1");
  
  starCheckButton2 = (GtkCheckButton *)gtk_check_button_new_with_label ("Display Stars");
  gtk_toggle_button_set_active((GtkToggleButton *)starCheckButton2, showStars2);
  g_signal_connect (G_OBJECT(starCheckButton2), "clicked",
		    G_CALLBACK(checkButtonCallback), "showStars2");
  
  starNameCheckButton1 = (GtkCheckButton *)gtk_check_button_new_with_label ("Star Names");
  gtk_toggle_button_set_active((GtkToggleButton *)starNameCheckButton1, showNames1);
  g_signal_connect (G_OBJECT(starNameCheckButton1), "clicked",
		    G_CALLBACK(checkButtonCallback), "showNames1");
  
  starNameCheckButton2 = (GtkCheckButton *)gtk_check_button_new_with_label ("Star Names");
  gtk_toggle_button_set_active((GtkToggleButton *)starNameCheckButton2, showNames2);
  g_signal_connect (G_OBJECT(starNameCheckButton2), "clicked",
		    G_CALLBACK(checkButtonCallback), "showNames2");
  
  planetCheckButton1 = (GtkCheckButton *)gtk_check_button_new_with_label ("Display Planets");
  gtk_toggle_button_set_active((GtkToggleButton *)planetCheckButton1, showPlanets1);
  g_signal_connect (G_OBJECT(planetCheckButton1), "clicked",
		    G_CALLBACK(checkButtonCallback), "showPlanets1");
  
  planetCheckButton2 = (GtkCheckButton *)gtk_check_button_new_with_label ("Display Planets");
  gtk_toggle_button_set_active((GtkToggleButton *)planetCheckButton2, showPlanets2);
  g_signal_connect (G_OBJECT(planetCheckButton2), "clicked",
		    G_CALLBACK(checkButtonCallback), "showPlanets2");
  
  cometCheckButton1 = (GtkCheckButton *)gtk_check_button_new_with_label ("Display Comets");
  gtk_toggle_button_set_active((GtkToggleButton *)cometCheckButton1, showComets1);
  g_signal_connect (G_OBJECT(cometCheckButton1), "clicked",
		    G_CALLBACK(checkButtonCallback), "showComets1");
  
  cometCheckButton2 = (GtkCheckButton *)gtk_check_button_new_with_label ("Display Comets");
  gtk_toggle_button_set_active((GtkToggleButton *)cometCheckButton2, showComets2);
  g_signal_connect (G_OBJECT(cometCheckButton2), "clicked",
		    G_CALLBACK(checkButtonCallback), "showComets2");
  
  meteorCheckButton1 = (GtkCheckButton *)gtk_check_button_new_with_label ("Meteor Radiants");
  gtk_toggle_button_set_active((GtkToggleButton *)meteorCheckButton1, showMeteors1);
  g_signal_connect (G_OBJECT(meteorCheckButton1), "clicked",
		    G_CALLBACK(checkButtonCallback), "showMeteors1");
  
  meteorCheckButton2 = (GtkCheckButton *)gtk_check_button_new_with_label ("Meteor Radiants");
  gtk_toggle_button_set_active((GtkToggleButton *)meteorCheckButton2, showMeteors2);
  g_signal_connect (G_OBJECT(meteorCheckButton2), "clicked",
		    G_CALLBACK(checkButtonCallback), "showMeteors2");
  
  deepSkyCheckButton1 = (GtkCheckButton *)gtk_check_button_new_with_label ("Deep Sky Obs");
  gtk_toggle_button_set_active((GtkToggleButton *)deepSkyCheckButton1, showDeepSky1);
  g_signal_connect (G_OBJECT(deepSkyCheckButton1), "clicked",
		    G_CALLBACK(checkButtonCallback), "showDeepSky1");
  
  deepSkyCheckButton2 = (GtkCheckButton *)gtk_check_button_new_with_label ("Deep Sky Obs");
  gtk_toggle_button_set_active((GtkToggleButton *)deepSkyCheckButton2, showDeepSky2);
  g_signal_connect (G_OBJECT(deepSkyCheckButton2), "clicked",
		    G_CALLBACK(checkButtonCallback), "showDeepSky2");
  
  bayerButton1 = (GtkCheckButton *)gtk_check_button_new_with_label ("Bayer Designations");
  gtk_toggle_button_set_active((GtkToggleButton *)bayerButton1, showBayer1);
  g_signal_connect (G_OBJECT(bayerButton1), "clicked",
		    G_CALLBACK(checkButtonCallback), "showBayer1");
  
  bayerButton2 = (GtkCheckButton *)gtk_check_button_new_with_label ("Bayer Designations");
  gtk_toggle_button_set_active((GtkToggleButton *)bayerButton2, showBayer2);
  g_signal_connect (G_OBJECT(bayerButton2), "clicked",
		    G_CALLBACK(checkButtonCallback), "showBayer2");
  
  greatCirclesCheckButton1 = (GtkCheckButton *)gtk_check_button_new_with_label ("Great Circles");
  gtk_toggle_button_set_active((GtkToggleButton *)greatCirclesCheckButton1, showGreatCircles1);
  g_signal_connect (G_OBJECT(greatCirclesCheckButton1), "clicked",
		    G_CALLBACK(checkButtonCallback), "showGreatCircles1");
  
  greatCirclesCheckButton2 = (GtkCheckButton *)gtk_check_button_new_with_label ("Great Circles");
  gtk_toggle_button_set_active((GtkToggleButton *)greatCirclesCheckButton2, showGreatCircles2);
  g_signal_connect (G_OBJECT(greatCirclesCheckButton2), "clicked",
		    G_CALLBACK(checkButtonCallback), "showGreatCircles2");
  
  useAsterismsCheckButton = (GtkCheckButton *)gtk_check_button_new_with_label ("Use Asterisms");
  gtk_toggle_button_set_active((GtkToggleButton *)useAsterismsCheckButton, useAsterisms);
  g_signal_connect (G_OBJECT(useAsterismsCheckButton), "clicked",
		    G_CALLBACK(checkButtonCallback), "useAsterisms");
  
  gtk_table_attach(GTK_TABLE(itemsTable), column1Label, 0, 1, row, row+1,
		   GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  gtk_table_attach(GTK_TABLE(itemsTable), column2Label, 1, 2, row, row+1,
		   GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0); row++;
  gtk_table_attach(GTK_TABLE(itemsTable), magButton1, 0, 1, row, row+1,
		   GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  gtk_table_attach(GTK_TABLE(itemsTable), magButton2, 1, 2, row, row+1,
		   GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  row += 2;
  gtk_table_attach(GTK_TABLE(itemsTable), (GtkWidget *)starCheckButton1, 0, 1, row, row+1,
		   GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  gtk_table_attach(GTK_TABLE(itemsTable), (GtkWidget *)starCheckButton2, 1, 2, row, row+1,
		   GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0); row++;
  gtk_table_attach(GTK_TABLE(itemsTable), (GtkWidget *)starNameCheckButton1, 0, 1, row, row+1,
		   GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  gtk_table_attach(GTK_TABLE(itemsTable), (GtkWidget *)starNameCheckButton2, 1, 2, row, row+1,
		   GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0); row++;
  gtk_table_attach(GTK_TABLE(itemsTable), (GtkWidget *)planetCheckButton1, 0, 1, row, row+1,
		   GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  gtk_table_attach(GTK_TABLE(itemsTable), (GtkWidget *)planetCheckButton2, 1, 2, row, row+1,
		   GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0); row++;
  gtk_table_attach(GTK_TABLE(itemsTable), (GtkWidget *)cometCheckButton1, 0, 1, row, row+1,
		   GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  gtk_table_attach(GTK_TABLE(itemsTable), (GtkWidget *)cometCheckButton2, 1, 2, row, row+1,
		   GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0); row++;
  gtk_table_attach(GTK_TABLE(itemsTable), (GtkWidget *)meteorCheckButton1, 0, 1, row, row+1,
		   GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  gtk_table_attach(GTK_TABLE(itemsTable), (GtkWidget *)meteorCheckButton2, 1, 2, row, row+1,
		   GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0); row++;
  gtk_table_attach(GTK_TABLE(itemsTable), (GtkWidget *)deepSkyCheckButton1, 0, 1, row, row+1,
		   GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  gtk_table_attach(GTK_TABLE(itemsTable), (GtkWidget *)deepSkyCheckButton2, 1, 2, row, row+1,
		   GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0); row++;
  gtk_table_attach(GTK_TABLE(itemsTable), (GtkWidget *)bayerButton1, 0, 1, row, row+1,
		   GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  gtk_table_attach(GTK_TABLE(itemsTable), (GtkWidget *)bayerButton2, 1, 2, row, row+1,
		   GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0); row++;
  gtk_table_attach(GTK_TABLE(itemsTable), (GtkWidget *)greatCirclesCheckButton1, 0, 1, row, row+1,
		   GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  gtk_table_attach(GTK_TABLE(itemsTable), (GtkWidget *)greatCirclesCheckButton2, 1, 2, row, row+1,
		   GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0); row++;
  gtk_table_attach(GTK_TABLE(itemsTable), (GtkWidget *)useAsterismsCheckButton, 1, 2, row, row+1,
		   GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0); row++;

  itemsStackable = hildon_stackable_window_new();
  g_signal_connect(G_OBJECT(itemsStackable), "destroy",
		   G_CALLBACK(checkItems), NULL);
  gtk_container_add(GTK_CONTAINER(itemsStackable), itemsTable);
  gtk_widget_show_all(itemsStackable);
}

void readLocationNameBox(void)
{
  char *newName;

  newName = (char *)gtk_entry_get_text((GtkEntry *)locationNameText);
  if (strcmp(locationName, newName) != 0) {
    optionsModified = TRUE;
    strcpy(locationName, newName);
  }
}

/*
  This function does garbage collection on a locationClass object
  that is no longer needed.
*/
locationClass *killLocationClass(locationClass *victim)
{
  locationClass *retValue;
  locationEntry *lastLoc, *killLoc;

  free(victim->name);
  lastLoc = victim->entry;
  while (lastLoc != NULL) {
    free(lastLoc->name);
    killLoc = lastLoc;
    lastLoc = lastLoc->next;
    free(killLoc);
  }
  if (victim->itemFactoryEntry != NULL) {
    free(victim->itemFactoryEntry[0].path);
    free(victim->itemFactoryEntry[0].item_type);
    free(victim->itemFactoryEntry);
  }
  if (victim->accelGroup != NULL)
    g_object_unref(victim->accelGroup);
  retValue = victim->next;
  free(victim);
  return(retValue);
}

void getCustomLocation(void)
{
  int longDD, longMM, longSS, latDD, latMM, latSS;
  int longSign, latSign;
  char *longResult, *latResult;

  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(westButton)))
    longSign = -1;
  else
    longSign = 1;
  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(southButton)))
    latSign = -1;
  else
    latSign = 1;
  longResult = (char *)hildon_button_get_value(HILDON_BUTTON(longButton));
  latResult  = (char *)hildon_button_get_value(HILDON_BUTTON(latButton));
  sscanf(longResult, "%d:%d:%d", &longDD, &longMM, &longSS);
  longDD *= longSign;
  longMM *= longSign;
  longSS *= longSign;
  longitude = (double)longDD + ((double)longMM)/60.0 +((double)longSS)/3600.0;
  sscanf(latResult, "%d:%d:%d", &latDD, &latMM, &latSS);
  latDD *= latSign;
  latMM *= latSign;
  latSS *= latSign;
  latitude = (double)latDD + ((double)latMM)/60.0 +((double)latSS)/3600.0;
  longitude *= DEGREES_TO_RADIANS;
  latitude  *= DEGREES_TO_RADIANS;
  readLocationNameBox();
}

/*
  The following routine can insert or delete an entry in the user's private
catalog.   If insert == FALSE, delete is performed.   If insert is specified,
any existing entry with the same name is replaced with the new position
information.
*/
void modifyPrivateCatalog(char *name, int insert)
{
  int i, nRead;
  int oldMenuExists;
  int haveWrittenNewEntry = FALSE;
  double tLat, tLong;
  char oldFileName[MAX_FILE_NAME_SIZE], newFileName[MAX_FILE_NAME_SIZE],
    nameCopy[100], tName[100];
  FILE *oldMenuFile, *newMenuFile;

  for (i = 0; i < strlen(locationName); i++)
    if (locationName[i] == ' ')
      nameCopy[i] ='_';
    else
      nameCopy[i] = locationName[i];
  nameCopy[strlen(locationName)] = (char)0;
  sprintf(oldFileName, "%s", privateCatalogName);
  oldMenuFile = fopen(oldFileName, "r");
  if (oldMenuFile == NULL)
    oldMenuExists = FALSE;
  else
    oldMenuExists = TRUE;
  sprintf(newFileName, "%s/newPrivate", userDir);
  newMenuFile = fopen(newFileName, "w");
  if (unlikely(newMenuFile == NULL)) {
    perror("Error opening new Private menu");
    exit(ERROR_EXIT);
  }
  if (oldMenuExists) {
    while (!feof(oldMenuFile)) {
      nRead = fscanf(oldMenuFile, "%s %lf %lf", &tName[0], &tLat, &tLong);
      if (nRead == 3) {
	if (insert) {
	  if (strcmp(tName, nameCopy) == 0) {
	    fprintf(newMenuFile, "%s %13.9f %13.9f\n", nameCopy,
		    latitude/DEGREES_TO_RADIANS, longitude/DEGREES_TO_RADIANS);
	    haveWrittenNewEntry = TRUE;
	  } else
	    fprintf(newMenuFile, "%s %13.9f %13.9f\n", tName, tLat, tLong);
	} else {
	  if (strcmp(tName, nameCopy) != 0)
	    fprintf(newMenuFile, "%s %13.9f %13.9f\n", tName, tLat, tLong);
	}
      }
    }
    fclose(oldMenuFile);
  }
  if (!haveWrittenNewEntry && insert)
    fprintf(newMenuFile, "%s %13.9f %13.9f\n", nameCopy,
	    latitude/DEGREES_TO_RADIANS, longitude/DEGREES_TO_RADIANS);
  fclose(newMenuFile);
  rename(newFileName, oldFileName);
}

/*
  This is the callback routine for the "Delete from Private Menu" button
*/
void deletePrivateLocation(GtkButton *button, gpointer userData)
{
  modifyPrivateCatalog(locationName, FALSE);
}

/*
  This is the callback routine for the button that saves a new
  location in the private locations menu.   I can't just append
  the new position to the existing file, because if I do so, we
  could end up with multiple entries with the same name, if the
  user either clicks the button more than once, or tries to
  enter a new position with the same name.   So I will copy
  the old file to a new one, and replace any duplicated entry.
*/
void savePrivateLocation(GtkButton *button, gpointer userData)
{
  if (useCustomLocation)
    getCustomLocation();
  modifyPrivateCatalog(locationName, TRUE);
}

/*
  Call back function for the options page OK button, which
  redraws the sky without saving chosen options.
*/
void optionsOk(GtkButton *button, gpointer userData)
{
  writeConfigFile();
}

void putOptsPage(int page)
{
  aboutScreen = page;
  displayingAnOptsPage = TRUE;
  fullRedraw(FALSE);
}

/*
  Call back function for the options page about button, which
  puts up information about the program.
*/
void about(GtkButton *button, gpointer userData)
{
  putOptsPage(ABOUT_SCREEN);
}

/*
  Call back function for the options page soluni button, which
  puts up sun and moon information.
*/
void soluni(GtkButton *button, gpointer userData)
{
  putOptsPage(SOLUNI_SCREEN);
}

/*
  Call back function for the options page Big Moon Calendar button, which
  draws a moon calendar for 21 months
*/
void bigMooncal(GtkButton *button, gpointer userData)
{
  putOptsPage(BIG_MOONCAL_SCREEN);
}

/*
  Call back function for the options page Small Moon Calendar button, which
  draws a moon calendar for 1 month
*/
void smallMooncal(GtkButton *button, gpointer userData)
{
  putOptsPage(SMALL_MOONCAL_SCREEN);
}

/*
  Call back function for the options page Planet Compass button, which
  shows the location of all planets.
*/
void planetCompass(GtkButton *button, gpointer userData)
{
  putOptsPage(PLANETCOMPASS_SCREEN);
}

/*
  Call back function for the options page Meteor Showers button, which
  shows a table of this year's meteor showers.
*/
void meteorShowersScreen(GtkButton *button, gpointer userData)
{
  putOptsPage(METEOR_SHOWERS_SCREEN);
}

/*
  Call back function for the options page analemma screen
*/
void analemmaScreen(GtkButton *button, gpointer userData)
{
  putOptsPage(ANALEMMA_SCREEN);
}

/*
  Call back function for the options page Astronomical Times page.
  Shows various times (local, UT, LST etc).
*/
void timesPageScreen(GtkButton *button, gpointer userData)
{
  putOptsPage(TIMES_PAGE_SCREEN);
  if (timerID != (guint)0) {
    g_source_remove(timerID);
    timerID = (guint)0;
  }
  scheduleUpdates("timesPageScreen", FAST_UPDATE_RATE);
  fastUpdates = TRUE;
}

/*
  Call back function to select the display of planet elevations as a function
  of time.
*/
void planetElevationScreen(GtkButton *button, gpointer userData)
{
  putOptsPage(PLANET_ELEVATION_SCREEN);
  if (timerID != (guint)0) {
    g_source_remove(timerID);
    timerID = (guint)0;
  }
  scheduleUpdates("planetElevationScreen", DEFAULT_UPDATE_RATE);
  fastUpdates = FALSE;
}

/*
  Call back function for the planet phenomena page.
*/
void planetPhenomenaScreen(GtkButton *button, gpointer userData)
{
  putOptsPage(PLANET_PHENOMENA);
  if (timerID != (guint)0) {
    g_source_remove(timerID);
    timerID = (guint)0;
  }
  scheduleUpdates("plenetPhenomenaScreen", DEFAULT_UPDATE_RATE);
  fastUpdates = FALSE;
}

/*
  Call back function for the Celestial Navigation page.
*/
void celestialNavigationScreen(GtkButton *button, gpointer userData)
{
  putOptsPage(CELESTIAL_NAVIGATION);
  updateCelestialNavigationScreen = TRUE;
  if (timerID != (guint)0) {
    g_source_remove(timerID);
    timerID = (guint)0;
  }
  scheduleUpdates("celestialNavigationScreen", DEFAULT_UPDATE_RATE);
  fastUpdates = FALSE;
}

/*
  Call back function for the Jovian Moons page.
*/
void jovianMoonsScreen(GtkButton *button, gpointer userData)
{
  putOptsPage(JOVIAN_MOONS);
  if (timerID != (guint)0) {
    g_source_remove(timerID);
    timerID = (guint)0;
  }
  scheduleUpdates("jovianMoonsScreen", DEFAULT_UPDATE_RATE);
  fastUpdates = FALSE;
}

/*
  Call back function for the Lunar Eclipses page.
*/
void lunarEclipsesScreen(GtkButton *button, gpointer userData)
{
  putOptsPage(LUNAR_ECLIPSES);
  if (timerID != (guint)0) {
    g_source_remove(timerID);
    timerID = (guint)0;
  }
  fastUpdates = FALSE;
}

/*
  Call back function for the Solar Eclipses page.
*/
void cometScreen(GtkButton *button, gpointer userData)
{
  putOptsPage(COMET_SCREEN);
  if (timerID != (guint)0) {
    g_source_remove(timerID);
    timerID = (guint)0;
  }
  scheduleUpdates("cometScreen", DEFAULT_UPDATE_RATE);
  fastUpdates = FALSE;
}

/*
  Call back function for the options page Solar System button, which
  shows the location of all planets.
*/
void solarSystem(GtkButton *button, gpointer userData)
{
  int arg;

  arg = *((int *)userData);
 if (useCurrentTime)
   savedTimeMode = 0;
 else if (useTextTime)
   savedTimeMode = 1;
 else if (useCalendarTime)
   savedTimeMode = 2;
 else
   savedTimeMode = 3;
 savedTJD = tJD;
 if (arg == 1)
   putOptsPage(SOLAR_SYSTEM_SCHEMATIC_SCREEN);
 else
   putOptsPage(SOLAR_SYSTEM_SCALE_SCREEN);
}

/*
  Call back function for the options page flashlight button, which
  sets the entire screen to one color.
*/
void flashlight(int color)
{
  if (color == 0)
    flashlightGC = gC[OR_WHITE];
  else
    flashlightGC = gC[OR_RED];
  fullscreenStateChanging = TRUE;
  gtk_widget_hide(optionsStackable);
  gtk_widget_destroy(optionsStackable);
  postageModeDisabled = FALSE;
  gtk_window_fullscreen(GTK_WINDOW(window));
  inFlashlightMode = TRUE;
}

void redFlashlight(GtkButton *button, gpointer userData)
{
  flashlight(1);
}

void whiteFlashlight(GtkButton *button, gpointer userData)
{
  flashlight(0);
}

void locationCallback(gpointer callbackData, guint callbackAction, GtkWidget *widget)
{
  if (locationNameIsDisplayed) {
    gtk_widget_hide(displayedLocationName);
    gtk_table_remove((GtkContainer *)locationTable, displayedLocationName);
  }
  strcpy(locationName, ((locationEntry *)callbackData)->name);
  latitude = ((locationEntry *)callbackData)->latitude;
  longitude = ((locationEntry *)callbackData)->longitude;
  displayedLocationName = gtk_label_new(locationName);
  gtk_table_attach(GTK_TABLE(locationTable), displayedLocationName, 2, 3, 5, 6,
		   GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show(displayedLocationName);
  locationNameIsDisplayed = TRUE;
  if (!saveAsPrivateShowing && !privateMenuShowing) {
    gtk_widget_show(saveAsPrivate);
    saveAsPrivateShowing = TRUE;
    if (deleteFromPrivateShowing) {
      gtk_widget_hide(deleteFromPrivate);
      deleteFromPrivateShowing = FALSE;
    }
  } else if (saveAsPrivateShowing && privateMenuShowing) {
    gtk_widget_hide(saveAsPrivate);
    saveAsPrivateShowing = FALSE;
  } else if (privateMenuShowing && !deleteFromPrivateShowing) {
    gtk_widget_show(deleteFromPrivate);
    deleteFromPrivateShowing = TRUE;
  }
  locationChanged = TRUE;
  newPosition();
}

void optionsCallback(gpointer callbackData, guint callbackAction, GtkWidget *widget)
{
}

void categoryCallback(gpointer callbackData, guint callbackAction, GtkWidget *widget)
{
  int found = FALSE;
  locationClass *ptr;

  if (menuShowing) {
    gtk_widget_hide(activeMenu);
    menuShowing = privateMenuShowing = FALSE;
  }
  ptr = locationClassRoot;
  while ((!found) && (ptr != NULL)) {
    if (ptr->key == callbackAction)
      found = TRUE;
    else
      ptr = ptr->next;
  }
  if (found) {
    int dataFD, i;
    int nLocations = 0;
    int eOF = FALSE;
    char *fileName;
    char inLine[100];
    locationEntry *lPtr;
    locationEntry *listEnd = NULL;

    if (!(ptr->menuCreated)) {
      ptr->entry = NULL;
      if (strcmp(ptr->name, "Private")) {
	fileName = malloc(strlen(dataDir)+strlen("/menus/")+strlen(ptr->name)+10);
	if (unlikely(fileName == NULL)) {
	  perror("file name for menu data (1)");
	  exit(ERROR_EXIT);
	}
	sprintf(fileName, "%s/menus/%s", dataDir, ptr->name);
      } else {
	fileName = malloc(strlen(userDir)+strlen(ptr->name)+11);
	if (unlikely(fileName == NULL)) {
	  perror("file name for menu data (2)");
	  exit(ERROR_EXIT);
	}
	sprintf(fileName, "%s/%s", userDir, ptr->name);
      }
      dataFD = open(fileName, O_RDONLY);
      free(fileName);
      if (unlikely(dataFD < 0)) {
	perror("open of menu a file");
	exit(ERROR_EXIT);
      }
      while (!eOF) {
	getLine(dataFD, &inLine[0], &eOF);
	if (!eOF) {
	  int nRead, i;
	  float latitude, longitude;
	  char tempName[100];
	  
	  nRead = sscanf(inLine, "%s %f %f", &tempName[0], &latitude, &longitude);
	  if (nRead == 3) {
	    locationEntry *newLoc;
	    
	    for (i = 0; i < strlen(tempName); i++)
	      if (tempName[i] == '_')
		tempName[i] = ' ';
	    newLoc = (locationEntry *)malloc(sizeof(locationEntry));
	    if (unlikely(newLoc == NULL)) {
	      perror("newLoc");
	      exit(ERROR_EXIT);
	    }
	    newLoc->next = NULL;
	    newLoc->name = malloc(strlen(tempName)+1);
	    if (unlikely(newLoc->name == NULL)) {
	      perror("newLoc->name");
	      exit(ERROR_EXIT);
	    }
	    strcpy(newLoc->name, tempName);
	    newLoc->latitude = latitude*DEGREES_TO_RADIANS;
	    newLoc->longitude = longitude*DEGREES_TO_RADIANS;
	    if (ptr->entry == NULL)
	      ptr->entry = newLoc;
	    else
	      listEnd->next = newLoc;
	    listEnd = newLoc;
	    nLocations++;
	  }
	}
      }
      close(dataFD);
      ptr->nLocations = nLocations;
      ptr->itemFactoryEntry = (GtkItemFactoryEntry *)malloc((1+nLocations)*
							    sizeof(GtkItemFactoryEntry));
      if (unlikely(ptr->itemFactoryEntry == NULL)) {
	perror("ptr->itemFactoryEntry");
	exit(ERROR_EXIT);
      }
      ptr->itemFactoryEntry[0].path = (gchar *)malloc(strlen(ptr->name)+2);
      if (unlikely(ptr->itemFactoryEntry[0].path == NULL)) {
	perror("ptr->itemFactoryEntry[0].path");
	exit(ERROR_EXIT);
      }
      sprintf(ptr->itemFactoryEntry[0].path, "/%s", ptr->name);
      ptr->itemFactoryEntry[0].accelerator = NULL;
      ptr->itemFactoryEntry[0].callback = NULL;
      ptr->itemFactoryEntry[0].callback_action = 0;
      ptr->itemFactoryEntry[0].item_type = (gchar *)malloc(strlen("<Branch>")+1);
      if (unlikely(ptr->itemFactoryEntry[0].item_type == NULL)) {
	perror("ptr->itemFactoryEntry[0].item_type");
	exit(ERROR_EXIT);
      }
      strcpy(ptr->itemFactoryEntry[0].item_type, "<Branch>");
      lPtr = ptr->entry;
      ptr->accelGroup = gtk_accel_group_new();
      sprintf(inLine, "<Orrery%s>", ptr->name);
      ptr->itemFactory = gtk_item_factory_new(GTK_TYPE_MENU_BAR, inLine,
					      ptr->accelGroup);

      gtk_item_factory_create_item (ptr->itemFactory,
                                    &ptr->itemFactoryEntry[0],
                                    lPtr, 1);
      for (i = 1; i <= nLocations; i++) {
	char path[100];
	
	sprintf(path, "/%s/%s", ptr->name, lPtr->name);
	ptr->itemFactoryEntry[i].path = (gchar *)malloc(strlen(path)+1);
	if (unlikely(ptr->itemFactoryEntry[i].path == NULL)) {
	  perror("ptr->itemFactoryEntry[i].path");
	  exit(ERROR_EXIT);
	}
	strcpy(ptr->itemFactoryEntry[i].path, path);
	ptr->itemFactoryEntry[i].accelerator = NULL;
	ptr->itemFactoryEntry[i].callback = locationCallback;
	ptr->itemFactoryEntry[i].item_type = NULL;
        gtk_item_factory_create_item (ptr->itemFactory,
                                      &ptr->itemFactoryEntry[i],
                                      lPtr, 1);
	lPtr = lPtr->next;
      }
      ptr->menu = gtk_item_factory_get_widget(ptr->itemFactory, inLine);
      ptr->menuCreated = TRUE;
      gtk_table_attach(GTK_TABLE(locationTable), (GtkWidget *)ptr->menu, 1, 2, 5, 6,
		       GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
      gtk_widget_set_sensitive(ptr->menu, TRUE);
    }
    gtk_widget_show((GtkWidget *)ptr->menu);
    activeMenu = ptr->menu;
    menuShowing = TRUE;
    if (locationNameIsDisplayed) {
      gtk_widget_hide(displayedLocationName);
      locationNameIsDisplayed = FALSE;
    }
    if (saveAsPrivateShowing) {
      gtk_widget_hide(saveAsPrivate);
      saveAsPrivateShowing = FALSE;
    }
    if (strcmp(ptr->name, "Private") == 0)
      privateMenuShowing = TRUE;
  }
}

/*
  This function is called when the stackable window, containing the
  widgets allowing time to be changed, is destroyed.   It implements the
  time change, if required, and forces the display to be redrawn, so that
  the effects of any change are seen immediately.
*/
void checkTime(void)
{
  checkTimeSpinBoxes();
  fullRedraw(FALSE);
}

/*
  Sets up the page allowing the observation time to be specified.
*/
void timeButtonClicked(GtkButton *button, gpointer userData)
{
  time_t t;
  struct tm *tGMT = NULL;  
  static GtkObject *yearAdj, *monthAdj, *dayAdj, *hourAdj, *minuteAdj, *secondAdj;
  static GtkObject *julAdjustment;
  static GtkWidget *timeStackable;
  static GtkWidget *timeTable;
  static HildonTouchSelector *hildonTimeSelector = NULL;

  postageModeDisabled = TRUE;
  timeTable = gtk_table_new(11, 4, FALSE);

  currentTimeButton = (GtkCheckButton *)gtk_check_button_new_with_label ("Now");
  gtk_table_attach(GTK_TABLE(timeTable), (GtkWidget *)currentTimeButton, 0, 4, 0, 1,
		   GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  gtk_toggle_button_set_active((GtkToggleButton *)currentTimeButton, useCurrentTime);
  g_signal_connect (G_OBJECT(currentTimeButton), "clicked",
		    G_CALLBACK(checkButtonCallback), "currentTimeButton");
  
  calendarTimeButton = (GtkCheckButton *)gtk_check_button_new_with_label ("Calendar");
  gtk_table_attach(GTK_TABLE(timeTable), (GtkWidget *)calendarTimeButton, 0, 4, 1, 2,
		   GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  gtk_toggle_button_set_active((GtkToggleButton *)calendarTimeButton, useCalendarTime);
  g_signal_connect (G_OBJECT(calendarTimeButton), "clicked",
		    G_CALLBACK(checkButtonCallback), "calendarTimeButton");
  
  textTimeButton = (GtkCheckButton *)gtk_check_button_new_with_label ("Text Entry");
  gtk_table_attach(GTK_TABLE(timeTable), (GtkWidget *)textTimeButton, 0, 4, 6, 7,
		   GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  gtk_toggle_button_set_active((GtkToggleButton *)textTimeButton, useTextTime);
  g_signal_connect (G_OBJECT(textTimeButton), "clicked",
		    G_CALLBACK(checkButtonCallback), "textTimeButton");
  
  julianTimeButton = (GtkCheckButton *)gtk_check_button_new_with_label ("Julian Date");
  gtk_table_attach(GTK_TABLE(timeTable), (GtkWidget *)julianTimeButton, 0, 4, 4, 5,
		   GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  gtk_toggle_button_set_active((GtkToggleButton *)julianTimeButton, useJulianTime);
  g_signal_connect (G_OBJECT(julianTimeButton), "clicked",
		    G_CALLBACK(checkButtonCallback), "julianTimeButton");
  
  cDateLabel = gtk_label_new("YYYYMMDD");
  gtk_table_attach(GTK_TABLE(timeTable), cDateLabel, 0, 1, 7, 8,
		   GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_set_sensitive(cDateLabel, useTextTime);
  
  cTimeLabel = gtk_label_new("HHMMSS");
  gtk_table_attach(GTK_TABLE(timeTable), cTimeLabel, 0, 1, 8, 9,
		   GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_set_sensitive(cTimeLabel, useTextTime);
  
  yearAdj = gtk_adjustment_new(cYear, -3000.0, 3000.0, 1.0, 1.0, 0.0);
  yearSpin = gtk_spin_button_new((GtkAdjustment *)yearAdj, 0.5, 0);
  gtk_spin_button_set_numeric((GtkSpinButton *)yearSpin, TRUE);
  gtk_table_attach(GTK_TABLE(timeTable), (GtkWidget *)yearSpin, 1, 2, 7, 8,
		   GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_set_sensitive(yearSpin, useTextTime);
  
  monthAdj = gtk_adjustment_new(cMonth, 1.0, 12.0, 1.0, 1.0, 0.0);
  monthSpin = gtk_spin_button_new((GtkAdjustment *)monthAdj, 0.5, 0);
  gtk_spin_button_set_numeric((GtkSpinButton *)monthSpin, TRUE);
  gtk_table_attach(GTK_TABLE(timeTable), (GtkWidget *)monthSpin, 2, 3, 7, 8,
		   GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_set_sensitive(monthSpin, useTextTime);
  
  dayAdj = gtk_adjustment_new(cDay, 1.0, 31.0, 1.0, 1.0, 0.0);
  daySpin = gtk_spin_button_new((GtkAdjustment *)dayAdj, 0.5, 0);
  gtk_spin_button_set_numeric((GtkSpinButton *)daySpin, TRUE);
  gtk_table_attach(GTK_TABLE(timeTable), (GtkWidget *)daySpin, 3, 4, 7, 8,
		   GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_set_sensitive(daySpin, useTextTime);
  
  hourAdj = gtk_adjustment_new(cHour, 0.0, 23.0, 1.0, 1.0, 0.0);
  hourSpin = gtk_spin_button_new((GtkAdjustment *)hourAdj, 0.5, 0);
  gtk_spin_button_set_numeric((GtkSpinButton *)hourSpin, TRUE);
  gtk_table_attach(GTK_TABLE(timeTable), (GtkWidget *)hourSpin, 1, 2, 8, 9,
		   GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_set_sensitive(hourSpin, useTextTime);
  
  minuteAdj = gtk_adjustment_new(cMinute, 0.0, 59.0, 1.0, 1.0, 0.0);
  minuteSpin = gtk_spin_button_new((GtkAdjustment *)minuteAdj, 0.5, 0);
  gtk_spin_button_set_numeric((GtkSpinButton *)minuteSpin, TRUE);
  gtk_table_attach(GTK_TABLE(timeTable), (GtkWidget *)minuteSpin, 2, 3, 8, 9,
		   GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_set_sensitive(minuteSpin, useTextTime);
  
  secondAdj = gtk_adjustment_new(cSecond, 0.0, 59.0, 1.0, 1.0, 0.0);
  secondSpin = gtk_spin_button_new((GtkAdjustment *)secondAdj, 0.5, 0);
  gtk_spin_button_set_numeric((GtkSpinButton *)secondSpin, TRUE);
  gtk_table_attach(GTK_TABLE(timeTable), (GtkWidget *)secondSpin, 3, 4, 8, 9,
		   GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_set_sensitive(secondSpin, useTextTime);
  
  julSpinLabel = gtk_label_new("Julian Date");
  gtk_widget_set_sensitive(julSpinLabel, useJulianTime);
  gtk_table_attach(GTK_TABLE(timeTable), julSpinLabel, 0, 1, 5, 6,
		   GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  julAdjustment = gtk_adjustment_new(tJD, 628459.0, 2817037.0, 0.001, 1.0, 0.0);
  julSpin = gtk_spin_button_new((GtkAdjustment *)julAdjustment, 0.5, 3);
  gtk_spin_button_set_numeric((GtkSpinButton *)julSpin, TRUE);
  gtk_table_attach(GTK_TABLE(timeTable), (GtkWidget *)julSpin, 1, 4, 5, 6,
		   GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_set_sensitive(julSpin, useJulianTime);
  hildonTimeButton = hildon_time_button_new(HILDON_SIZE_AUTO_WIDTH |
					    HILDON_SIZE_AUTO_HEIGHT,
					    HILDON_BUTTON_ARRANGEMENT_HORIZONTAL);
  hildonTimeSelector = hildon_picker_button_get_selector((HildonPickerButton *)hildonTimeButton);
  /* Force the time selector to be in 24 hour mode, regardless of localization */
  g_object_set (G_OBJECT (hildonTimeSelector),
		"time-format-policy", HILDON_TIME_SELECTOR_FORMAT_POLICY_24H, NULL);
  hildon_button_set_title((HildonButton *)hildonTimeButton, "Universal Time (UT)");
  hildon_time_button_set_time((HildonTimeButton *)hildonTimeButton, 0, 0);
  gtk_table_attach(GTK_TABLE(timeTable), hildonTimeButton, 0, 4, 2, 3,
		   GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  /*
    Fill in the columns of the time picker with the current Universal Time
  */
  t = time(NULL);
  tGMT = gmtime(&t);
  hildon_touch_selector_set_active(HILDON_TOUCH_SELECTOR(hildonTimeSelector), 0, tGMT->tm_hour);
  hildon_touch_selector_set_active(HILDON_TOUCH_SELECTOR(hildonTimeSelector), 1, tGMT->tm_min);

  gtk_widget_set_sensitive(hildonTimeButton, useCalendarTime);
  hildonDateButton = hildon_date_button_new_with_year_range(HILDON_SIZE_AUTO_WIDTH |
							    HILDON_SIZE_AUTO_HEIGHT,
							    HILDON_BUTTON_ARRANGEMENT_HORIZONTAL,
							    1900, 2100);
  hildon_button_set_title((HildonButton *)hildonDateButton, "UT Date");
  /* This should make the release button smaller, so the months aren't truncated, but it
     doesn't.   Maybe when maemo 5 more fully supports portrait mode, it will work.
     hildon_picker_button_set_done_button_text((HildonPickerButton *)hildonDateButton, "OK");
  */
  gtk_table_attach(GTK_TABLE(timeTable), hildonDateButton, 0, 4, 3, 4,
		   GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_set_sensitive(hildonDateButton, useCalendarTime);

  timeStackable = hildon_stackable_window_new();
  g_signal_connect(G_OBJECT(timeStackable), "destroy",
		   G_CALLBACK(checkTime), NULL);
  gtk_container_add(GTK_CONTAINER(timeStackable), timeTable);
  gtk_widget_show_all(timeStackable);
}

/*
  This function is called when the options stackable window is destroyed
*/
void checkOptions(void)
{
  postageModeDisabled = FALSE;
}

/*
  Sets up the page allowing vaious global options to be specified
*/
void optionsButtonClicked(GtkButton *button, gpointer userData)
{
  int row = 0;
  static int one = 1;
  static int two = 2;
  static GtkWidget *whiteFlashlightButton, *redFlashlightButton, *soluniButton, *aboutButton,
    *bigMooncalButton, *smallMooncalButton, *planetCompassButton, *solarSystemSchematicButton,
    *solarSystemScaleButton, *meteorShowersButton, *timesPageButton, *analemmaButton,
    *planetElevationButton, *planetPhenomenaButton, *jovianMoonsButton,
    *celestialNavigationButton, *lunarEclipsesButton, *cometButton;

  postageModeDisabled = TRUE;
  optionsTable = gtk_table_new(9, 2, FALSE);
  
  whiteFlashlightButton = gtk_button_new_with_label ("White Flashlight");
  gtk_table_attach_defaults(GTK_TABLE(optionsTable), whiteFlashlightButton,
			    0, 1, row, row+1);
  g_signal_connect (G_OBJECT(whiteFlashlightButton), "clicked",
		    G_CALLBACK (whiteFlashlight), NULL);
  redFlashlightButton = gtk_button_new_with_label ("Red Flashlight");
  gtk_table_attach_defaults(GTK_TABLE(optionsTable), redFlashlightButton,
			    1, 2, row, row+1);
  g_signal_connect (G_OBJECT(redFlashlightButton), "clicked",
		    G_CALLBACK (redFlashlight), NULL);
  row++;
  
  soluniButton = gtk_button_new_with_label ("Sun and Moon");
  gtk_table_attach_defaults(GTK_TABLE(optionsTable), soluniButton,
			    0, 1, row, row+1);
  g_signal_connect (G_OBJECT(soluniButton), "clicked",
		    G_CALLBACK (soluni), NULL);
  aboutButton = gtk_button_new_with_label ("Symbol Key");
  gtk_table_attach_defaults(GTK_TABLE(optionsTable), aboutButton,
			    1, 2, row, row+1);
  g_signal_connect (G_OBJECT(aboutButton), "clicked",
		    G_CALLBACK (about), NULL);
  row++;
  
  bigMooncalButton = gtk_button_new_with_label ("Big Moon Calendar");
  gtk_table_attach_defaults(GTK_TABLE(optionsTable), bigMooncalButton,
			    0, 1, row, row+1);
  g_signal_connect (G_OBJECT(bigMooncalButton), "clicked",
		    G_CALLBACK (bigMooncal), NULL);
  smallMooncalButton = gtk_button_new_with_label ("This Month's Moons");
  gtk_table_attach_defaults(GTK_TABLE(optionsTable), smallMooncalButton,
			    1, 2, row, row+1);
  g_signal_connect (G_OBJECT(smallMooncalButton), "clicked",
                      G_CALLBACK (smallMooncal), NULL);
  row++;
  
  meteorShowersButton = gtk_button_new_with_label ("Meteor Showers");
  gtk_table_attach_defaults(GTK_TABLE(optionsTable), meteorShowersButton,
			    0, 1, row, row+1);
  g_signal_connect (G_OBJECT(meteorShowersButton), "clicked",
		    G_CALLBACK (meteorShowersScreen), NULL);
  
  planetCompassButton = gtk_button_new_with_label ("Planet Compass");
  gtk_table_attach_defaults(GTK_TABLE(optionsTable), planetCompassButton,
			    1, 2, row, row+1);
  g_signal_connect (G_OBJECT(planetCompassButton), "clicked",
		    G_CALLBACK (planetCompass), NULL);
  row++;
  
  solarSystemSchematicButton = gtk_button_new_with_label ("Schematic Solar Sys.");
  gtk_table_attach_defaults(GTK_TABLE(optionsTable), solarSystemSchematicButton,
			    0, 1, row, row+1);
  g_signal_connect (G_OBJECT(solarSystemSchematicButton), "clicked",
		    G_CALLBACK (solarSystem), &one);
  
  solarSystemScaleButton = gtk_button_new_with_label ("To-Scale Solar Sys.");
  gtk_table_attach_defaults(GTK_TABLE(optionsTable), solarSystemScaleButton,
			    1, 2, row, row+1);
  g_signal_connect (G_OBJECT(solarSystemScaleButton), "clicked",
		    G_CALLBACK (solarSystem), &two);
  row++;

  timesPageButton = gtk_button_new_with_label ("Astronomical Times");
  gtk_table_attach_defaults(GTK_TABLE(optionsTable), timesPageButton,
			    0, 1, row, row+1);
  g_signal_connect (G_OBJECT(timesPageButton), "clicked",
		    G_CALLBACK (timesPageScreen), NULL);

  analemmaButton = gtk_button_new_with_label ("Analemma");
  gtk_table_attach_defaults(GTK_TABLE(optionsTable), analemmaButton,
			    1, 2, row, row+1);
  g_signal_connect (G_OBJECT(analemmaButton), "clicked",
		    G_CALLBACK (analemmaScreen), NULL);
  row++;

  planetElevationButton = gtk_button_new_with_label ("Planet Elevations");
  gtk_table_attach_defaults(GTK_TABLE(optionsTable), planetElevationButton,
			    0, 1, row, row+1);
  g_signal_connect (G_OBJECT(planetElevationButton), "clicked",
		    G_CALLBACK (planetElevationScreen), NULL);

  planetPhenomenaButton = gtk_button_new_with_label ("Planet Phenomena");
  gtk_table_attach_defaults(GTK_TABLE(optionsTable), planetPhenomenaButton,
			    1, 2, row, row+1);
  g_signal_connect (G_OBJECT(planetPhenomenaButton), "clicked",
		    G_CALLBACK (planetPhenomenaScreen), NULL);
  row++;

  jovianMoonsButton = gtk_button_new_with_label ("Jovian Moons");
  gtk_table_attach_defaults(GTK_TABLE(optionsTable), jovianMoonsButton,
			    0, 1, row, row+1);
  g_signal_connect (G_OBJECT(jovianMoonsButton), "clicked",
		    G_CALLBACK (jovianMoonsScreen), NULL);

  celestialNavigationButton = gtk_button_new_with_label ("Celestial Navigation");
  gtk_table_attach_defaults(GTK_TABLE(optionsTable), celestialNavigationButton,
			    1, 2, row, row+1);
  g_signal_connect (G_OBJECT(celestialNavigationButton), "clicked",
		    G_CALLBACK (celestialNavigationScreen), NULL);
  row++;

  lunarEclipsesButton = gtk_button_new_with_label ("Lunar Eclipses");
  gtk_table_attach_defaults(GTK_TABLE(optionsTable), lunarEclipsesButton,
			    0, 1, row, row+1);
  g_signal_connect (G_OBJECT(lunarEclipsesButton), "clicked",
		    G_CALLBACK (lunarEclipsesScreen), NULL);

  cometButton = gtk_button_new_with_label ("Comets");
  gtk_table_attach_defaults(GTK_TABLE(optionsTable), cometButton,
			    1, 2, row, row+1);
  g_signal_connect (G_OBJECT(cometButton), "clicked",
		    G_CALLBACK (cometScreen), NULL);
  optionsStackable = hildon_stackable_window_new();
  g_signal_connect(G_OBJECT(optionsStackable), "destroy",
		   G_CALLBACK(checkOptions), NULL);
  gtk_container_add(GTK_CONTAINER(optionsStackable), optionsTable);
  gtk_widget_show_all(optionsStackable);
}

/*
  Callback for button requesting that the Wiki help page be opened.
*/

void wikiButtonClicked(GtkButton *button, gpointer userData)
{
  system("/usr/bin/dbus-send --system --type=method_call  --dest=\"com.nokia.osso_browser\" /com/nokia/osso_browser/request com.nokia.osso_browser.load_url string:\"wiki.maemo.org/Orrery\"");
}

void tipsGone(void)
{
  postageModeDisabled = FALSE;
}

/*
  Callback for local help - displays contents of a text file in a
  scrollable window.
 */

void tipsButtonClicked(GtkButton *button, gpointer userData)
{
  static int firstCall = TRUE;
  static GtkTextBuffer *tipsBuffer;
  static GtkWidget *tipsTextWidget, *tipsStackable;

  if (firstCall) {
    tipsBuffer = gtk_text_buffer_new(NULL);
    gtk_text_buffer_set_text(tipsBuffer, "Pan:\tTap in the bottom 1/5th of the\n\t\tdisplay.\n\nZoom:\tCircle a region with your finger.\n\nUnzoom: Press the screen for more\n\t\tthan 1/3 second.\n\nToggle:\tTap in the upper 4/5ths of the\n\t\tdisplay to toggle displays.", -1);
    tipsTextWidget = hildon_text_view_new(); 
    ((GtkTextView *)tipsTextWidget)->editable =
      ((GtkTextView *)tipsTextWidget)->cursor_visible = FALSE;
    gtk_widget_ref(tipsTextWidget);
    firstCall = FALSE;
  }
  postageModeDisabled = TRUE;
  hildon_text_view_set_buffer((HildonTextView *)tipsTextWidget, tipsBuffer);
  tipsStackable = hildon_stackable_window_new();
  g_signal_connect(G_OBJECT(tipsStackable), "destroy",
		   G_CALLBACK(tipsGone), NULL);
  gtk_container_add(GTK_CONTAINER(tipsStackable), tipsTextWidget);
  gtk_widget_show_all(tipsStackable);
}

/*
  This function is called when the "Change Location" stackable
  window is destroyed.   It cleans up the data structures which
  were built to implement the location menus.
*/
void checkLocation(void)
{
  locationClass *lastClass;

  postageModeDisabled = FALSE;
  lastClass = locationClassRoot;
  while (lastClass != NULL) {
    lastClass = killLocationClass(lastClass);
  }
  menuShowing = privateMenuShowing = locationNameIsDisplayed = FALSE;
  locationClassRoot = NULL;
  if (useCustomLocation) {
    getCustomLocation();
    locationChanged = TRUE;
    gPSLocation = FALSE;
  }
  writeConfigFile();
  fullRedraw(FALSE);
}

/*
  This function creates the selector for the longitude and latitude "pickers".
*/
static GtkWidget *createTouchSelector(int dMin, int dMax)
{
  int deg;
  char degString[10];
  GtkWidget *selector;
  GtkListStore *model;
  GtkTreeIter iter;
  HildonTouchSelectorColumn *column = NULL;

  selector = hildon_touch_selector_new();
  model = gtk_list_store_new(1, G_TYPE_STRING);
  /* Degrees */
  for (deg = dMin; deg <= dMax; deg++) {
    sprintf(degString, "%d", deg);
    gtk_list_store_append(model, &iter);
    gtk_list_store_set(model, &iter, 0, degString, -1);
  }
  column = hildon_touch_selector_append_text_column(HILDON_TOUCH_SELECTOR(selector),
                                                     GTK_TREE_MODEL(model), TRUE);
  g_object_set(G_OBJECT(column), "text-column", 0, NULL);
  model = gtk_list_store_new(1, G_TYPE_STRING);

  /* Minutes of arc */
  for (deg = 0; deg < 60; deg++) {
    sprintf(degString, "%02d", deg);
    gtk_list_store_append(model, &iter);
    gtk_list_store_set(model, &iter, 0, degString, -1);
  }
  column = hildon_touch_selector_append_text_column(HILDON_TOUCH_SELECTOR(selector),
                                                     GTK_TREE_MODEL(model), TRUE);
  g_object_set(G_OBJECT(column), "text-column", 0, NULL);
  model = gtk_list_store_new(1, G_TYPE_STRING);

  /* Seconds of arc */
  for (deg = 0; deg < 60; deg++) {
    sprintf(degString, "%02d", deg);
    gtk_list_store_append(model, &iter);
    gtk_list_store_set(model, &iter, 0, degString, -1);
  }
  column = hildon_touch_selector_append_text_column(HILDON_TOUCH_SELECTOR(selector),
                                                     GTK_TREE_MODEL(model), TRUE);
  g_object_set(G_OBJECT(column), "text-column", 0, NULL);
  
  return selector;
}

/*
  Sets up the page allowing the observer's location to be specified
*/
void locationButtonClicked(GtkButton *button, gpointer userData)
{
  static GtkWidget *locationStackable;
  static GtkItemFactory *itemFactory;
  static GtkItemFactoryEntry *itemFactoryEntry;
  static GtkAccelGroup *accelGroup;
  static GtkWidget *longSelector, *latSelector;
  char *fileName, inLine[100];
  int west, south;
  int menuFileFD, i;
  int nMenuClasses = 0;
  int eOF = FALSE;
  int latDD, latMM, latSS, longDD, longMM, longSS;
  double tLongitude, tLatitude;
  locationClass *lastClass = NULL;

  postageModeDisabled = TRUE;
  deleteFromPrivateShowing = saveAsPrivateShowing = FALSE;
  locationTable = gtk_table_new(11, 4, FALSE);

  saveAsPrivate = gtk_button_new_with_label ("Save in Private Menu");
  g_signal_connect (G_OBJECT(saveAsPrivate), "clicked",
		    G_CALLBACK (savePrivateLocation), NULL);
  
  deleteFromPrivate = gtk_button_new_with_label ("Delete from Private Menu");
  g_signal_connect (G_OBJECT(deleteFromPrivate), "clicked",
		    G_CALLBACK (deletePrivateLocation), NULL);
  
  gPSButton = (GtkCheckButton *)gtk_check_button_new_with_label ("Phone");
  gtk_table_attach(GTK_TABLE(locationTable), (GtkWidget *)gPSButton, 0, 4, 0, 1,
		   GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  gtk_toggle_button_set_active((GtkToggleButton *)gPSButton, useGPSD);
  g_signal_connect (G_OBJECT(gPSButton), "clicked",
		    G_CALLBACK(checkButtonCallback), "gPSButton");
  
  customLocationButton = (GtkCheckButton *)gtk_check_button_new_with_label ("Custom");
  gtk_table_attach(GTK_TABLE(locationTable), (GtkWidget *)customLocationButton, 0, 4, 1, 2,
		   GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  gtk_toggle_button_set_active((GtkToggleButton *)customLocationButton, useCustomLocation);
  g_signal_connect (G_OBJECT(customLocationButton), "clicked",
		    G_CALLBACK(checkButtonCallback), "customLocationButton");
  
  locationNameLabel = gtk_label_new("Name");
  gtk_table_attach(GTK_TABLE(locationTable), locationNameLabel, 0, 1, 2, 3,
		   GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_set_sensitive(locationNameLabel, useCustomLocation);
  locationNameText = gtk_entry_new();
  gtk_entry_set_text((GtkEntry *)locationNameText, "Location Name");
  gtk_table_attach(GTK_TABLE(locationTable), locationNameText, 1, 4, 2, 3,
		   GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_set_sensitive(locationNameText, useCustomLocation);
  longSelector = createTouchSelector(0, 179);
  longButton = hildon_picker_button_new(HILDON_SIZE_AUTO, HILDON_BUTTON_ARRANGEMENT_VERTICAL);
  hildon_picker_button_set_done_button_text(HILDON_PICKER_BUTTON(longButton),
                                             "Done");
  hildon_button_set_title(HILDON_BUTTON(longButton), "Longitude");
  hildon_picker_button_set_selector(HILDON_PICKER_BUTTON(longButton),
				    HILDON_TOUCH_SELECTOR(longSelector));
  gtk_table_attach(GTK_TABLE(locationTable), longButton, 0, 2, 3, 4,
		   GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_set_sensitive(longButton, useCustomLocation);

  eastButton = gtk_radio_button_new_with_label(NULL, "(+)East");
  westButton = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(eastButton), "(-)West");
  gtk_table_attach(GTK_TABLE(locationTable), westButton, 0, 1, 4, 5,
		   GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_set_sensitive(westButton, useCustomLocation);
  gtk_table_attach(GTK_TABLE(locationTable), eastButton, 1, 2, 4, 5,
		   GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_set_sensitive(eastButton, useCustomLocation);

  latSelector = createTouchSelector(0, 89);
  latButton = hildon_picker_button_new(HILDON_SIZE_AUTO, HILDON_BUTTON_ARRANGEMENT_VERTICAL);
  hildon_picker_button_set_done_button_text(HILDON_PICKER_BUTTON(latButton),
                                             "Done");
  hildon_button_set_title(HILDON_BUTTON(latButton), "Latitude");
  hildon_picker_button_set_selector(HILDON_PICKER_BUTTON(latButton),
                                     HILDON_TOUCH_SELECTOR(latSelector));
  /*
    Here we fill in the longitude picker with the phone's current positon, if
    such a position is available.
   */
  west = south = FALSE;
  if (readPhonePosition(&tLatitude, &tLongitude)) {
    tLatitude /= DEGREES_TO_RADIANS;
    tLongitude /= DEGREES_TO_RADIANS;
    if (tLongitude < 0.0) {
      west = TRUE;
      tLongitude *= -1.0;
      }
    if (tLatitude < 0.0) {
      south = TRUE;
      tLatitude *= -1.0;
    }
    latDD = (int)tLatitude;
    latMM = (int)((tLatitude - (double)latDD) * 60.0);
    latSS = (int)((tLatitude - (double)latDD - ((double)latMM)/60.0) * 3600.0);
    longDD = (int)tLongitude;
    longMM = (int)((tLongitude - (double)longDD) * 60.0);
    longSS = (int)((tLongitude - (double)longDD - ((double)longMM)/60.0) * 3600.0);
    hildon_touch_selector_set_active(HILDON_TOUCH_SELECTOR(latSelector), 0, latDD);
    hildon_touch_selector_set_active(HILDON_TOUCH_SELECTOR(latSelector), 1, latMM);
    hildon_touch_selector_set_active(HILDON_TOUCH_SELECTOR(latSelector), 2, latSS);
    hildon_touch_selector_set_active(HILDON_TOUCH_SELECTOR(longSelector), 0, longDD);
    hildon_touch_selector_set_active(HILDON_TOUCH_SELECTOR(longSelector), 1, longMM);
    hildon_touch_selector_set_active(HILDON_TOUCH_SELECTOR(longSelector), 2, longSS);
  }

  gtk_table_attach(GTK_TABLE(locationTable), latButton, 2, 4, 3, 4,
		   GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_set_sensitive(latButton, useCustomLocation);
  northButton = gtk_radio_button_new_with_label(NULL, "(+)North");
  southButton = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(northButton), "(-)South");
  gtk_table_attach(GTK_TABLE(locationTable), southButton, 2, 3, 4, 5,
		   GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_set_sensitive(northButton, useCustomLocation);
  gtk_widget_set_sensitive(southButton, useCustomLocation);
  gtk_table_attach(GTK_TABLE(locationTable), northButton, 3, 4, 4, 5,
		   GTK_FILL, GTK_FILL, 0, 0);
  if (south)
    gtk_toggle_button_set_active((GtkToggleButton *)southButton, TRUE);
  else
    gtk_toggle_button_set_active((GtkToggleButton *)northButton, TRUE);

  if (west)
    gtk_toggle_button_set_active((GtkToggleButton *)westButton, TRUE);
  else
    gtk_toggle_button_set_active((GtkToggleButton *)eastButton, TRUE);

  menuLocationButton = (GtkCheckButton *)gtk_check_button_new_with_label ("Menu");
  gtk_table_attach(GTK_TABLE(locationTable), (GtkWidget *)menuLocationButton, 0, 4, 5, 6,
		   GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  gtk_toggle_button_set_active((GtkToggleButton *)menuLocationButton, useMenuLocation);
  g_signal_connect (G_OBJECT(menuLocationButton), "clicked",
		    G_CALLBACK(checkButtonCallback), "menuLocationButton");
  
  /*
    Make the menu that holds the classes of locations for the main locations
    menu, such as Europe or Observatory.
  */
  fileName = malloc(strlen(dataDir)+strlen("/menus/regions")+10);
  if (unlikely(fileName == NULL)) {
    perror("regions file");
    exit(ERROR_EXIT);
  }
  sprintf(fileName, "%s/menus/regions", dataDir);
  menuFileFD = open(fileName, O_RDONLY);
  if (unlikely(menuFileFD < 0)) {
    perror(fileName);
    exit(ERROR_EXIT);
  }
  free(fileName);
  while (!eOF) {
    getLine(menuFileFD, &inLine[0], &eOF);
    if (!eOF) {
      int nRead, key;
      char name[50];
      
      nRead = sscanf(inLine, "%s %d", &name[0], &key);
      if (nRead == 2) {
	locationClass *newClass;
	
	newClass = malloc(sizeof(locationClass));
	if (unlikely(newClass == NULL)) {
	  perror("newClass");
	  exit(ERROR_EXIT);
	}
	newClass->name = NULL;
	newClass->menu = NULL;
	newClass->itemFactory = NULL;
	newClass->entry = NULL;
	newClass->itemFactoryEntry = NULL;
	newClass->accelGroup = NULL;
	newClass->nLocations = 0;
	newClass->name = malloc(strlen(name)+1);
	if (unlikely(newClass->name == NULL)) {
	  perror("newClass->name");
	  exit(ERROR_EXIT);
	}
	strcpy(newClass->name, name);
	newClass->key = key;
	newClass->menuCreated = FALSE;
	newClass->next = NULL;
	if (locationClassRoot == NULL)
	  locationClassRoot = newClass;
	else
	  lastClass->next = newClass;
	lastClass = newClass;
	nMenuClasses++;
      }
    }
  }
  close(menuFileFD);
  itemFactoryEntry = (GtkItemFactoryEntry *)malloc((1+nMenuClasses)*
						   sizeof(GtkItemFactoryEntry));
  if (unlikely(itemFactoryEntry == NULL)) {
    perror("itemFactoryEntry");
    exit(ERROR_EXIT);
  }
  itemFactoryEntry[0].path = (gchar *)malloc(strlen("/Region")+1);
  if (unlikely(itemFactoryEntry[0].path == NULL)) {
    perror("(itemFactoryEntry[0].path");
    exit(ERROR_EXIT);
  }
  strcpy(itemFactoryEntry[0].path, "/Region");
  itemFactoryEntry[0].accelerator = NULL;
  itemFactoryEntry[0].callback = NULL;
  itemFactoryEntry[0].callback_action = 0;
  itemFactoryEntry[0].extra_data = NULL;
  itemFactoryEntry[0].item_type = (gchar *)malloc(strlen("<Branch>")+1);
  if (unlikely(itemFactoryEntry[0].item_type == NULL)) {
    perror("itemFactoryEntry[0].item_type");
    exit(ERROR_EXIT);
  }
  strcpy(itemFactoryEntry[0].item_type, "<Branch>");
  lastClass = locationClassRoot;
  for (i = 1; i <= nMenuClasses; i++) {
    int j;
    char path[100];
    
    sprintf(path, "/Region/%s", lastClass->name);
    for (j = 0; j < strlen(path); j++)
      if (path[j] == '_')
	path[j] = ' ';
    itemFactoryEntry[i].path = (gchar *)malloc(strlen(path)+1);
    if (unlikely(itemFactoryEntry[i].path == NULL)) {
      perror("itemFactoryEntry[i].path");
      exit(ERROR_EXIT);
    }
    strcpy(itemFactoryEntry[i].path, path);
    itemFactoryEntry[i].accelerator = NULL;
    itemFactoryEntry[i].callback = categoryCallback;
    itemFactoryEntry[i].callback_action = lastClass->key;
    itemFactoryEntry[i].item_type = NULL;
    lastClass = lastClass->next;
  }
  accelGroup = gtk_accel_group_new();
  itemFactory = gtk_item_factory_new(GTK_TYPE_MENU_BAR, "<OrreryRegions>",
				     accelGroup);
  gtk_item_factory_create_items(itemFactory, nMenuClasses+1, itemFactoryEntry,
				NULL);
  regionMenu = gtk_item_factory_get_widget(itemFactory, "<OrreryRegions>");
  gtk_table_attach(GTK_TABLE(locationTable), (GtkWidget *)regionMenu, 0, 1, 6, 7,
		   GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_set_sensitive(regionMenu, useMenuLocation);
  /* End of making location class menu */

  gtk_table_attach(GTK_TABLE(locationTable), saveAsPrivate, 0, 4, 8, 9,
		   GTK_FILL, GTK_FILL, 0, 0);

  locationStackable = hildon_stackable_window_new();
  g_signal_connect(G_OBJECT(locationStackable), "destroy",
		   G_CALLBACK(checkLocation), NULL);
  gtk_container_add(GTK_CONTAINER(locationStackable), locationTable);
  gtk_widget_show_all(locationStackable);
  gtk_table_attach(GTK_TABLE(locationTable), deleteFromPrivate, 0, 4, 9, 10,
		   GTK_FILL, GTK_FILL, 0, 0);
  if (!useCustomLocation) {
    saveAsPrivateShowing = FALSE;
    gtk_widget_hide(saveAsPrivate);
  } else
    saveAsPrivateShowing = TRUE;
}

/*
  The following function is called as the idle job.   It reads in the constellation
  files, and determines which stars are in the constellations by comparing the
  constelation vertex coordinates to all star coordinates.   This is done so that
  the stars making up a constellation are always plotted, even if they are
  fainter than the magnitude cutoff for the constellation display.   This is a
  time consuming processnewFIle -D sci, so it is done in the background as the
  idle process, in the hope that it will have completed before the user wishes to
  display the constellations.

  This function is executed many times, and each time it does a little more work.
  If there is still work to be done, it exits returning TRUE.   When it is
  finally done, it exits returning FALSE.   This multiple execution strategy
  is used to prevent this function from blocking execution of other routines
  for an extended period.   If it were allowed to execute a single time, from
  start to finish, it would prevent the display from being updated for several
  seconds, which is unacceptable.
*/
static int  initConstellations(gpointer ignored)
{
  static int filesRead = FALSE;
  int i;
  int count = 0;
  char dataDirName[1000], inLine[100];
  DIR *dirPtr;
  struct dirent *nextEnt;
  constellation *thisConst, *lastConst = NULL;
  static objectListEntry *currentEntry;

  if (!filesRead) {
    sprintf(dataDirName, "%s/constellations/", dataDir);
    dirPtr = opendir(dataDirName);
    while ((nextEnt = readdir(dirPtr)) != NULL)
      if (strstr(nextEnt->d_name, ".") == NULL) {
	int nPoints = 0;
	int eof = FALSE;
	short starNeededs[100];
	int rAHH, rAMM, decDD, decMM;
	float nameAngle;
	double rASS, decSS, rA, dec;
	double rAs[100], decs[100], sinDecs[100], cosDecs[100];
	char fileName[MAX_FILE_NAME_SIZE];
	int dataFD, negZero;
	
	sprintf(fileName, "%s/constellations/%s", dataDir, nextEnt->d_name);
	dataFD = open(fileName, O_RDONLY);
	if (dataFD >= 0) {
	  int nRead, ii;
	  
	  thisConst = (constellation *)malloc(sizeof(constellation));
	  if (unlikely(thisConst == NULL)) {
	    perror("malloc (thisConst) returned NULL");
	    exit(ERROR_EXIT);
	  }
	  thisConst->next = NULL;
	  if (constellations == NULL)
	    constellations = thisConst;
	  else
	    lastConst->next = thisConst;
	  lastConst = thisConst;
	  thisConst->name = (char *)malloc(strlen(nextEnt->d_name)+1);
	  if (unlikely(thisConst->name == NULL)) {
	    perror("malloc (thisConst->name) returned NULL");
	    exit(ERROR_EXIT);
	  }
	  sprintf(thisConst->name, "%s", nextEnt->d_name);
	  for (ii = 0; ii < strlen(thisConst->name); ii++)
	    if (thisConst->name[ii] == '_')
	      thisConst->name[ii] = '\n';
	  getLine(dataFD, &inLine[0], &eof);
	  nRead = sscanf(inLine, "%d", &(thisConst->type));
	  getLine(dataFD, &inLine[0], &eof);
	  nRead = sscanf(inLine, "%d:%d:%lf %d:%d:%lf %f", &rAHH, &rAMM, &rASS,
			 &decDD, &decMM, &decSS, &nameAngle);
	  if (nRead != 7) {
	    printf("%s has only %d tokens\n", fileName, nRead);
	    nameAngle = 0.0;
	  }
	  rA = (double)rAHH + ((double)rAMM)/60.0 + rASS/3600.0;
	  rA *= HOURS_TO_RADIANS;
	  if (decDD < 0) {
	    decMM *= -1;
	    decSS *= -1.0;
	  }
	  dec = (double)decDD + ((double)decMM)/60.0 + decSS/3600.0;
	  dec *= DEGREES_TO_RADIANS;
	  thisConst->nameRA = rA;
	  thisConst->nameAngle = -nameAngle * DEGREES_TO_RADIANS;
	  thisConst->sinNameDec = sin(dec);
	  thisConst->cosNameDec = cos(dec);
	  nRead = 6;
	  while ((nRead == 6) && (!eof)) {
	    getLine(dataFD, &inLine[0], &eof);
	    nRead = sscanf(inLine, "%d:%d:%lf %d:%d:%lf", &rAHH, &rAMM, &rASS,
			 &decDD, &decMM, &decSS);
	    if (nRead == 6) {
	      rA = (double)rAHH + ((double)rAMM)/60.0 + rASS/3600.0;
	      rA *= HOURS_TO_RADIANS;
	      if (strstr(inLine, "-00:") == NULL)
		negZero = FALSE;
	      else
		negZero = TRUE;
	      if ((decDD < 0) || negZero) {
		decMM *= -1;
		decSS *= -1.0;
	      }
	      dec = (double)decDD + ((double)decMM)/60.0 + decSS/3600.0;
	      dec *= DEGREES_TO_RADIANS;
	      if (rA == 0.0)
		starNeededs[nPoints] = FALSE;
	      else
		starNeededs[nPoints] = TRUE;
	      rAs[nPoints] = rA;
	      decs[nPoints] = dec;
	      sinDecs[nPoints] = sin(dec);
	      cosDecs[nPoints++] = cos(dec);
	    }
	  }
	  thisConst->nPoints = nPoints;
	  thisConst->starNeeded = (short *)malloc(nPoints*sizeof(short));
          if (unlikely(thisConst->starNeeded == NULL)) {
            perror("malloc of thisConst->starNeeded returned NULL");
            exit(ERROR_EXIT);
          }
	  thisConst->rA = (double *)malloc(nPoints*sizeof(double));
	  if (unlikely(thisConst->rA == NULL)) {
	    perror("malloc of thisConst->rA returned NULL");
	    exit(ERROR_EXIT);
	  }
	  thisConst->dec = (double *)malloc(nPoints*sizeof(double));
	  if (unlikely(thisConst->dec == NULL)) {
	    perror("malloc of thisConst->dec returned NULL");
	    exit(ERROR_EXIT);
	  }
	  thisConst->sinDec = (double *)malloc(nPoints*sizeof(double));
	  if (unlikely(thisConst->sinDec == NULL)) {
	    perror("malloc of thisConst->sinDec returned NULL");
	    exit(ERROR_EXIT);
	  }
	  thisConst->cosDec = (double *)malloc(nPoints*sizeof(double));
	  if (unlikely(thisConst->cosDec == NULL)) {
	    perror("malloc of thisConst->cosDec returned NULL");
	    exit(ERROR_EXIT);
	  }
	  for (i = 0; i < nPoints; i++) {
	    thisConst->starNeeded[i] = starNeededs[i];
	    thisConst->rA[i] = rAs[i];
	    thisConst->dec[i] = decs[i];
	    thisConst->sinDec[i] = sinDecs[i];
	    thisConst->cosDec[i] = cosDecs[i];
	  }
	  close(dataFD);
	} else
	  perror(nextEnt->d_name);
      }
    currentEntry = objectListRoot;
    filesRead = TRUE;
    return(TRUE);
  }
  if (currentEntry != NULL) {
    while (currentEntry != NULL) {
      if (currentEntry->mag <= CONSTELLATION_LIMIT)
	inConstellation(currentEntry);
      currentEntry = currentEntry->forwardPointer;
      if (count++ > 100) {
	return(TRUE);
      }
    }
  }
  constellationsInitialized = TRUE;
  if (shouldSwitchScreens) {
    switchScreens();
    fullRedraw(FALSE);
  }
  return(FALSE);
}

/*
  Signal handler to catch ^C and restore initial correlator state and
  antenna offsets before exiting.
*/
void signalHandler(int signum)
{
  if (signum == SIGPIPE) {
    close(gPSDSocket);
    gPSDSocketOpen = FALSE;
  } else
    fprintf(stderr,"point received unexpected signal #%d\n",signum);
}

void usage(void)
{
  fprintf(stderr, "usage: orrery [-d directory]\n");
  exit(-1);
}

static void window_set_orientation(GtkWindow *window, GtkOrientation orientation) {
  int flags;

  switch (orientation) {
  case GTK_ORIENTATION_VERTICAL:
    flags = HILDON_PORTRAIT_MODE_REQUEST;
    break;
  case GTK_ORIENTATION_HORIZONTAL:
    flags = ~HILDON_PORTRAIT_MODE_REQUEST;
    break;
  }

  hildon_gtk_window_set_portrait_flags(GTK_WINDOW(window),
				       (HildonPortraitFlags)flags);
}

static void makeHildonButtons(void)
{
  HildonSizeType buttonSize = HILDON_SIZE_FINGER_HEIGHT | HILDON_SIZE_AUTO_WIDTH;

  hildonMenu = HILDON_APP_MENU(hildon_app_menu_new());

  /* Now, the options button, which allows different pages to be shown */
  optionsButton = hildon_gtk_button_new(buttonSize);
  gtk_button_set_label(GTK_BUTTON(optionsButton), "Other Pages");
  g_signal_connect(G_OBJECT(optionsButton), "clicked", G_CALLBACK(optionsButtonClicked), NULL);
  hildon_app_menu_append(hildonMenu, GTK_BUTTON(optionsButton));
  /* Now, the items button, which controls what things are plotted on the display */
  itemsButton = hildon_gtk_button_new(buttonSize);
  gtk_button_set_label(GTK_BUTTON(itemsButton), "Displayed Items");
  g_signal_connect(G_OBJECT(itemsButton), "clicked", G_CALLBACK(itemsButtonClicked), NULL);
  hildon_app_menu_append(hildonMenu, GTK_BUTTON(itemsButton));
  /* Now, the time button, which controls what time the program thinks it is. */
  timeButton = hildon_gtk_button_new(buttonSize);
  gtk_button_set_label(GTK_BUTTON(timeButton), "Change Time");
  g_signal_connect(G_OBJECT(timeButton), "clicked", G_CALLBACK(timeButtonClicked), NULL);
  hildon_app_menu_append(hildonMenu, GTK_BUTTON(timeButton));
  /* Now, the location button, which controls where the observer is */
  locationButton = hildon_gtk_button_new(buttonSize);
  gtk_button_set_label(GTK_BUTTON(locationButton), "Change Location");
  g_signal_connect(G_OBJECT(locationButton), "clicked", G_CALLBACK(locationButtonClicked), NULL);
  hildon_app_menu_append(hildonMenu, GTK_BUTTON(locationButton));
  /* Now, a button to launch the browser showing the Wiki page */
  wikiButton = hildon_gtk_button_new(buttonSize);
  gtk_button_set_label(GTK_BUTTON(wikiButton), "Web Help Page");
  g_signal_connect(G_OBJECT(wikiButton), "clicked", G_CALLBACK(wikiButtonClicked), NULL);
  hildon_app_menu_append(hildonMenu, GTK_BUTTON(wikiButton));
  /* Now, a button to show tips page */
  tipsButton = hildon_gtk_button_new(buttonSize);
  gtk_button_set_label(GTK_BUTTON(tipsButton), "Quick Tips");
  g_signal_connect(G_OBJECT(tipsButton), "clicked", G_CALLBACK(tipsButtonClicked), NULL);
  hildon_app_menu_append(hildonMenu, GTK_BUTTON(tipsButton));

  hildon_stackable_window_set_main_menu((HildonStackableWindow *)window, hildonMenu);
  gtk_widget_show_all(GTK_WIDGET(hildonMenu));
}

void setupPrivateLocationsCatalog(void)
{
  char *homeDir;
  FILE *pCFId;

  homeDir = getenv("HOME");
  if (homeDir == NULL) {
    homeDir = malloc(strlen("/usr/share/orrery")+1);
    if (unlikely(homeDir == NULL)) {
      perror("malloc of back-up homeDir");
      exit(ERROR_EXIT);
    }
    sprintf(homeDir, "/usr/share/orrery");
  }
  userDir = malloc(strlen(homeDir)+strlen("/.orrery")+1);
  if (unlikely(userDir == NULL)) {
    perror("malloc of userDir");
    exit(ERROR_EXIT);
  }
  sprintf(userDir, "%s/.orrery", homeDir);
  mkdir(userDir, 0777);
  privateCatalogName = malloc(strlen(userDir)+strlen("/Private")+1);
  if (unlikely(privateCatalogName == NULL)) {
    perror("malloc of privateCatalogName");
    exit(ERROR_EXIT);
  }
  sprintf(privateCatalogName, "%s/Private", userDir);
  pCFId = fopen(privateCatalogName, "r");
  if (pCFId == NULL)
    /* Create an empty private locations catalog, if none already exists */
    pCFId = fopen(privateCatalogName, "w");
  fclose(pCFId);
}

int main(int argc, char **argv)
{
  struct sigaction action, oldAction;
  osso_context_t *oSSOContext;

  oSSOContext = osso_initialize("com.nokia.orrery", orreryVersion, TRUE, NULL);
  if (!oSSOContext) {
    fprintf(stderr, "oss_initialize call failed\n");
    exit(-1);
  }

  /*
    OK, the following is a kludge.  I admit it.  I originally called the
    popt package to parse the command line arguments.   But some Openmoko
    distributions do not include the popt lib.   This became a hassle.
    So I decided not only to eliminate the popt dependancy, but to use no
    command line parsing at all, so that there are no unneeded dependancies.
    there is only one command line option available, after all, so it;s
    not too hare to just handle it in its entirety here.
   */

  if (((argc != 1) && (argc != 3)) ||
      ((argc == 3) && (strcmp(argv[1], "-d") != 0)))
    usage();
  if (argc == 3)
    sprintf(dirName, "%s/", argv[2]);
  action.sa_flags = 0;
  sigemptyset(&action.sa_mask);
  action.sa_handler = signalHandler;
  sigaction(SIGPIPE, &action, &oldAction);
   if (argc != 3)
    strcpy(dataDir, DATA_DIR);
  else
    strcpy(dataDir, argv[2]);

  setupPrivateLocationsCatalog();
  parseConfigFile();
  readHipparcosCatalog();
  newPosition();

  hildon_gtk_init(&argc, &argv);
  /* Initialize main window */
  window = hildon_stackable_window_new();
#ifndef NOT_YET
  window_set_orientation(GTK_WINDOW(window), GTK_ORIENTATION_VERTICAL);
#endif
  gtk_widget_set_size_request (GTK_WIDGET (window), 480, 640);
  gtk_window_set_title (GTK_WINDOW (window), "orrery");
  g_signal_connect (G_OBJECT (window), "delete_event",
		    G_CALLBACK (gtk_main_quit), NULL);

  mainBox = gtk_vbox_new(FALSE, 0);
  gtk_container_add(GTK_CONTAINER(window), mainBox);
  g_object_ref(mainBox); /* This keeps mainBox from being destroyed when not displayed */
  gtk_widget_show(mainBox);

  /* Configure Menu Buttons */
  makeHildonButtons();

  /* Set up window in which to plot the graphics ...*/
  drawingArea = gtk_drawing_area_new();
  gtk_widget_set_size_request (GTK_WIDGET(drawingArea), VGA_WIDTH, VGA_HEIGHT-BUTTON_HEIGHT);
  gtk_box_pack_end(GTK_BOX(mainBox), drawingArea, TRUE, TRUE, 0);
  g_signal_connect(G_OBJECT(drawingArea), "expose_event",
		   G_CALLBACK(exposeEvent), NULL);
  g_signal_connect(G_OBJECT(drawingArea), "configure_event",
		   G_CALLBACK(configureEvent), NULL);
  g_signal_connect(G_OBJECT(drawingArea), "button_release_event",
		   G_CALLBACK(buttonReleaseEvent), NULL);
  g_signal_connect(G_OBJECT(drawingArea), "button_press_event",
		   G_CALLBACK(buttonPressEvent), NULL);
  g_signal_connect(G_OBJECT(drawingArea), "motion_notify_event",
		   G_CALLBACK(motionNotifyEvent), NULL);
  g_signal_connect(G_OBJECT(window), "focus-in-event",
		   G_CALLBACK(focusInEvent), NULL);
  g_signal_connect(G_OBJECT(window), "focus-out-event",
		   G_CALLBACK(focusOutEvent), NULL);
  gtk_widget_show(window);
  makeGraphicContexts(window);
  makeFonts(window);

  gtk_widget_set_events(drawingArea,
			GDK_EXPOSURE_MASK       | GDK_BUTTON_PRESS_MASK  |
			GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK);
  gtk_widget_show(drawingArea);
  centerAzD = (double)centerAz;
  scheduleUpdates("main", DEFAULT_UPDATE_RATE);
  gtk_idle_add(initConstellations, NULL);
  gtk_main();
  osso_deinitialize(oSSOContext);
  return 0;
}
