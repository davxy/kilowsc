#ifndef MISC_H_
#define MISC_H_

#ifndef SIM
#define SIM
#endif

#ifdef SIM
#include <kilombo.h>
#include <stdio.h>
#include <assert.h>
#else
#include <kilolib.h>    /* kilobot stand alone library */
#include <avr/io.h>     /* microcontroller registers */
#endif /* SIMULATOR */


//#define DEBUG_CHAN
//#define DEBUG_APP

//#define VERBOSE_CHAN
//#define VERBOSE_DISCOVER

//#define VISUAL_CHAN
#define VISUAL_APP

#ifdef DEBUG_CHAN
#define VISUAL_CHAN
#define VERBOSE_CHAN
#endif

#ifdef DEBUG_APP
#define VISUAL_APP
#define VERBOSE_APP
#endif


#ifdef SIM

#define TRACE2(...) do { \
    fprintf(stdout, __VA_ARGS__); \
    fflush(stdout); \
} while (0)

#define TRACE(...) do { \
    TRACE2("[P%03u] ", kilo_uid); \
    TRACE2(__VA_ARGS__); \
} while (0)

#else

#define TRACE2(...) ;
#define TRACE(...)  ;

#endif /* SIM */


#ifdef VISUAL_APP
#define APP_COLOR(col) set_color(col)
#else
#define APP_COLOR(col) ;
#endif

#ifdef VISUAL_CHAN
#define CHN_COLOR(col) set_color(col)
#else
#define CHN_COLOR(col) ;
#endif


#define KILO_TICKS_PER_SEC  31

#define WHITE   (RGB(3,3,3))
#define RED     (RGB(3,0,0))
#define GREEN   (RGB(0,3,0))
#define BLUE    (RGB(0,0,3))
#define BLACK   (RGB(0,0,0))

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

#ifdef SIM
#define ASSERT(cond) assert(cond)
#else
#define ASSERT(cond) ;
#endif

#endif /* MISC_H_ */
