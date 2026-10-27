#ifndef MEMORIES_PC_TOUCH_PAD_ART_H
#define MEMORIES_PC_TOUCH_PAD_ART_H
/* The on-screen controller's pictures (touch_pad.h): vector buttons drawn
 * with soft edges at the size the layout gives them (touch_pad_art.c), the
 * game's colors for Triangle, Circle, Cross and Square, text from the
 * overlay font. Nothing is read from the disc. Main thread only. */
#include "menu.h"

/* Draws the pad where touch_pad.c has laid it out (nothing while hidden),
 * and gives the bounds it covered (all 0 when nothing). */
void TouchPadArt_Draw(MenuCanvas *canvas, int *x, int *y, int *w, int *h);
/* Drops the pictures; they are drawn again when next needed. */
void TouchPadArt_Reset(void);

#endif
