#ifndef MISC_H_
#define MISC_H_

#ifdef KILOMBO
#include <kilombo.h>
#include <stdio.h>
#include <assert.h>
#else
#include <kilolib.h>    /* kilobot stand alone library */
#include <avr/io.h>     /* microcontroller registers */
#endif /* KILOMBO */


#ifdef KILOMBO

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

#endif /* KILOMBO */


#ifdef VERBOSE_APP
#define TRACE_APP(...)  TRACE(__VA_ARGS__)
#define TRACE2_APP(...) TRACE2(__VA_ARGS__)
#else
#define TRACE_APP(...)  ;
#define TRACE2_APP(...) ;
#endif

#ifdef VERBOSE_CHAN
#define TRACE_CHAN(...)  TRACE(__VA_ARGS__)
#define TRACE2_CHAN(...) TRACE2(__VA_ARGS__)
#else
#define TRACE_CHAN(...)  ;
#define TRACE2_CHAN(...) ;
#endif

#ifdef VISUAL_APP
#define COLOR_APP(col) set_color(col)
#else
#define COLOR_APP(col) ;
#endif

#ifdef VISUAL_CHAN
#define COLOR_CHAN(col) set_color(col)
#else
#define COLOR_CHAN(col) ;
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

#ifdef KILOMBO
#define ASSERT(cond) assert(cond)
#else
#define ASSERT(cond) ;
#endif

#endif /* MISC_H_ */
