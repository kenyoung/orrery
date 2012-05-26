/*
  This file defines the colors used by the orrery program.
 */
#define N_COLORS        (22)

#define MAX16        (65535)
#define DOUBLE_MAX16 (65635.0)
#define OR_BLACK              (0)
#define OR_WHITE              (1)
#define OR_BLUE               (2)
#define OR_FAINT_BLUE         (3)
#define OR_LIGHT_GREY         (4)
#define OR_GREY               (5)
#define OR_DARK_GREY          (6)
#define OR_GREEN              (7)
#define OR_DARK_GREEN         (8)
#define OR_RED                (9)
#define OR_EQUATOR           (10)
#define OR_YELLOW            (11)
#define OR_LIGHT_YELLOW      (12)
#define OR_CREAM             (13)
#define OR_DARK_CREAM        (14)
#define OR_BLUE_GREEN        (15)
#define OR_DARK_BLUE_GREEN   (16)
#define OR_FAINT_GOLD        (17)
#define OR_PINK              (18)
#define OR_FAINT_PINK        (19)
#define OR_FAINT_YELLOW      (20)
#define OR_LIGHT_BLUE        (21)

unsigned short orreryColorRGB[N_COLORS][3] =
  {{    0,     0,     0}, /* Black           */
   {MAX16, MAX16, MAX16}, /* White           */
   {30000, 30000, MAX16}, /* Blue            */
   {21000, 21000, 45875}, /* Faint Blue      */
   {44000, 44000, 44000}, /* Light Grey      */
   {32000, 32000, 32000}, /* Grey            */
   {20000, 20000, 20000}, /* Dark Grey       */
   {    0, MAX16,     0}, /* Green           */
   {    0, 40000,     0}, /* Dark Green      */
   {MAX16,     0,     0}, /* Red             */
   {MAX16, MAX16, 32767}, /* Equator         */
   {MAX16, MAX16,     0}, /* Yellow          */
   {MAX16, MAX16, 32000}, /* Light Yellow    */
   {MAX16, MAX16, 40000}, /* Cream           */
   {39321, 39321, 24000}, /* Dark Cream      */
   {    0, MAX16, MAX16}, /* Blue Green      */
   {    0, 32000, 32000}, /* Dark Blue Green */
   {39321, 33153,     0}, /* Faint Gold      */
   {MAX16, 25000, 35000}, /* Pink            */
   {58982, 13415, 22667}, /* Faint Pink      */
   {52000, 52000, 35000}, /* Faint Yellow    */
   {40000, 40000, MAX16}, /* Light Blue      */
  };
