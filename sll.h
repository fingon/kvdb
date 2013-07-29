/*
 * $Id: sll.h $
 *
 * Author: Markus Stenberg <fingon@iki.fi>
 *
 * Copyright (c) 2013 Markus Stenberg
 *
 * Created:       Mon Jul 29 13:53:35 2013 mstenber
 * Last modified: Mon Jul 29 13:55:30 2013 mstenber
 * Edit time:     2 min
 *
 */

#ifndef SLL_H
#define SLL_H

#define SLL_ADD4(h,v,p,n)       \
do {                            \
  if (h)                        \
    h->p = v;                   \
  v->n = h;                     \
  h = v;                        \
 } while(0)
#define SLL_ADD2(h,v) SLL_ADD4(h,v,prev,next)

#define SLL_POP4(h,v,p,n)       \
do {                            \
  if (v->p)                     \
    v->p->n = v->n;             \
  if (v->n)                     \
    v->n->p = v->p;             \
  if (h == v)                   \
    h = v->n;                   \
 } while(0)
#define SLL_POP2(h,v) SLL_POP4(h,v,prev,next)

#define SLL_FOR3(h,v,n) for (v = h ; v ; v = v->n)
#define SLL_FOR2(h,v) SLL_FOR3(h,v,next)

#endif /* SLL_H */
