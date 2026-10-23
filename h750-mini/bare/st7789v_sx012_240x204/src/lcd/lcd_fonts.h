#ifndef __FONTS_H
#define __FONTS_H

#include <stdint.h>


// Font structure definition
typedef struct _pFont
{    
	const uint8_t 		*pTable;  		//	Font data array address
	uint16_t 			Width; 		 	//	Font width of a single character
	uint16_t 			Height; 			//	Font height of a single character
	uint16_t 			Sizes;	 		//	Number of font data bytes per character
	uint16_t				Table_Rows;		// Only used by Chinese font glyphs; number of rows in the 2D array
} pFONT;


/*------------------------------------ Chinese fonts ---------------------------------------------*/

extern	pFONT	CH_Font12 ;		//	12x12 font
extern	pFONT	CH_Font16 ;    //	16x16 font
extern	pFONT	CH_Font20 ;    //	20x20 font
extern	pFONT	CH_Font24 ;    //	24x24 font
extern	pFONT	CH_Font32 ;    //	32x32 font


/*------------------------------------ ASCII fonts ---------------------------------------------*/

extern pFONT ASCII_Font32;		// 32x16 font
extern pFONT ASCII_Font24;		// 24x12 font
extern pFONT ASCII_Font20; 	// 20x10 font
extern pFONT ASCII_Font16; 	// 16x08 font
extern pFONT ASCII_Font12; 	// 12x06 font

#endif 
 
