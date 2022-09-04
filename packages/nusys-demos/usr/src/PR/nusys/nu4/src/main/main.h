/*
   main.h

   Copyright (C) 1997-1999, NINTENDO Co,Ltd.	
*/

#ifndef MAIN_H
#define MAIN_H

#ifdef _LANGUAGE_C

/* Definition of the external variable  */
extern NUContData	contdata[1]; /* Read data of the controller  */
extern u8 contPattern;		     /* The pattern of the connected controller  */

extern int ptr_buf[];
extern int tune_buf[];
extern int sfx_buf[];

#endif /* _LANGUAGE_C */
#endif /* MAIN_H */




