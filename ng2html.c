/*

     NG2HTML NORTON GUIDE TO HTML DOCUMENT CONVERTER.
     Copyright (C) 1996,1997 David A Pearson
   
     This program is free software; you can redistribute it and/or modify
     it under the terms of the GNU General Public License as published by
     the Free Software Foundation; either version 2 of the license, or 
     (at your option) any later version.
     
     This program is distributed in the hope that it will be useful,
     but WITHOUT ANY WARRANTY; without even the implied warranty of
     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
     GNU General Public License for more details.
     
     You should have received a copy of the GNU General Public License
     along with this program; if not, write to the Free Software
     Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

/*
 * Modification history:
 *
 * $Log: ng2html.c,v $
 * Revision 1.11  1997/01/03 03:32:28  davep
 * Improved the quality of the HTML produced. As well as giving you
 * more "legal" HTML, it should also be more readable to the eye.
 *
 * Revision 1.10  1996/07/21 09:58:13  davep
 * Made a minor fix to StandardButtons(). If you were not producing
 * frames aware HTML it would still include a TARGET directive for
 * the menu button.
 *
 * Revision 1.9  1996/07/05 13:00:50  davep
 * Minor changes to get rid of messages when compiling with -Wall.
 *
 * Revision 1.8  1996/04/08 07:44:47  davep
 * Added support for frames.
 *
 * Revision 1.7  1996/03/28 18:36:07  davep
 * Added a config file setup.
 * Added the ability to specify the <BODY> command, this will allow you to
 * (for example) specify a background image for the body of the guide.
 * Added support for frames.
 *
 * Revision 1.6  1996/03/25 11:41:27  davep
 * Killed one more HTML silly.
 *
 * Revision 1.5  1996/03/22 17:41:17  davep
 * Fixed ReadLong() so that it works with 16bt compilers.
 * Minor fixes to the HTML produced. I was ignoring the & characters, this
 * now correctly produces an "&amp;" in it's place.
 * Fixed Hex2Byte() wich had got broken for some strange reason.
 *
 * Revision 1.4  1996/03/19 09:57:23  davep
 * The HTML for the short and long entries was all to pot. I'd forgot to
 * included the usual <HTML></HTML>, <HEAD></HEAD> and <BODY></BODY>
 * "wrappers". Now fixed.
 *
 * Revision 1.3  1996/03/18 12:33:40  davep
 * Fixed a silly bug where I'd cleaned out a variable I didn't need, but
 * had left some code that refered to it. This will teach me to do a test
 * compile every time I make a change. :-)
 *
 * Revision 1.2  1996/03/16 14:37:08  davep
 * A bit of tidying up and some check during writes was added.
 *
 * Revision 1.1  1996/03/16 09:49:00  davep
 * Initial revision
 *
 *
 */

/* Usual standard header files. */

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>

/* Include file for the cfg*() functions. */

#include "cfgfile.h"

/* Structure of the header record of a Norton Guide database. */

typedef struct 
{
    unsigned short usMagic;
    short          sUnknown1;
    short          sUnknown2;
    unsigned short usMenuCount;
    char           szTitle[ 40 ];
    char           szCredits[ 5 ][ 66 ];
} NGHEADER;

/* Structure to hold a tidy version of the header. */

typedef struct 
{
    unsigned short usMenuCount;
    char           szTitle[ 41 ];
    char           szCredits[ 5 ][ 67 ];
} NGTIDYHEAD;

/* Prototype functions found in this file. */

void WriteInfo( NGTIDYHEAD * );
void WriteMenu( FILE *, NGTIDYHEAD * );
void WriteDropMenu( FILE *, FILE * );
void WriteBody( FILE *, NGTIDYHEAD * );
void WriteShort( FILE *, long, NGTIDYHEAD * );
void WriteLong( FILE *, long, NGTIDYHEAD * );
void SkipSection( FILE * );
void TidyHeader( NGHEADER *, NGTIDYHEAD * );
unsigned char SaneText( unsigned char );
int ReadWord( FILE * );
long ReadLong( FILE * );
void GetStrZ( FILE *, char *, int );
void ExpandText( FILE *, char * );
void StandardButtons( FILE * );
void StandardHeader( FILE *, NGTIDYHEAD *, char * );
void StandardFooter( FILE * );
int Hex2Byte( char * );
void SafePrint( FILE *, char *, ... );
void PrintHtmlChar( FILE *, char );
void LoadConfig( void );

/* A couple of handy things. */

#ifndef TRUE
#define TRUE  1
#define FALSE 0
#endif

/* The version number of ng2html. */

#define NG2HTML_VERSION "1.05"

/* If you don't want my "advert" at the bottom of your pages, uncomment 
   the following: */

/* #define NO_ADVERT */

/* If you are compiling under Dos in 16bit mode, then un-comment the
   following: */

/* #define _16_BIT_COMPILER_ */

/* The bits for making up the pages. */

#define NG_PREFIX "ng"              /* Prefix for the pages in the guides. */
#define NG_ID     "%lx"             /* Used to generate guide page names. */
#define NG_SUFFIX ".html"           /* Suffix for the file names. */
#define NG_MENU   "menu" NG_SUFFIX  /* Name of the menu page. */
#define NG_INFO   "info" NG_SUFFIX  /* Name of the info/about page. */
#define NG_FMENU  "fmenu" NG_SUFFIX /* Name of the frames enabled menu. */

/* Decrypt the NGs "encryption". */

#define NG_DECRYPT( x ) ( (unsigned char) ( x ^ 0x1A ) )

/* Read a byte and decrypt it. */

#define NG_READBYTE( f ) ( NG_DECRYPT( getc( f ) ) )

/* Change this to point to the location of the config file. */

#define CONFIG_FILE    "/usr/local/etc/ng2html.conf"

/* Variable settings that affect the look of the generated HTML. */

char *pszBodyString = "<BODY>";
int  iFrames        = FALSE;
char *pszFrameCols  = "30%,70%";

/*
 */

int main( int argc, char **argv )
{
    FILE       *fleNG;
    NGHEADER   ngHeader;
    NGTIDYHEAD ngTidyHead;
    long       lOffset;
    int        iExit = 0;

    if ( argc > 1 )
    {
	if ( ( fleNG = fopen( argv[ 1 ], "rb" ) ) != NULL )
	{
	    (void) fread( &ngHeader, sizeof( NGHEADER ), 1, fleNG );

	    lOffset = ftell( fleNG );

	    TidyHeader( &ngHeader, &ngTidyHead );

	    LoadConfig();

	    WriteInfo( &ngTidyHead );
	    WriteMenu( fleNG, &ngTidyHead ); 

	    fseek( fleNG, lOffset, SEEK_SET );

	    WriteBody( fleNG, &ngTidyHead );

	    cfgReset();

	    fclose( fleNG );
	}
	else
	{
	    fprintf( stderr, "Error! Can't open %s for reading\n", argv[ 1 ] );
	    iExit = 1;
	}
    }
    else
    {
	fprintf( stderr, "\n%s v" NG2HTML_VERSION " By Dave Pearson\n"
                 "Syntax: %s <NGName>\n"
                 "\nExample:\n\t%s foo.ng\n",
                 argv[ 0 ], argv[ 0 ], argv[ 0 ] );
	iExit = 1;
    }

    return( iExit );
}

/*
 */

void WriteInfo( NGTIDYHEAD *ngHeader )
{
    FILE *f = fopen( NG_INFO, "w" );

    if ( f != NULL )
    {
	SafePrint( f, "<HTML>\n<HEAD>\n<TITLE>%s - Information</TITLE>\n"
                   "</HEAD>\n%s\n<PRE>  Title: %s\nCredits: %s\n"
                   "         %s\n         %s\n         %s\n"
                   "         %s\n</PRE>\n</BODY>\n</HTML>\n", 
                   ngHeader->szTitle,
                   pszBodyString,
                   ngHeader->szTitle,
                   ngHeader->szCredits[ 0 ],
                   ngHeader->szCredits[ 1 ],
                   ngHeader->szCredits[ 2 ],
                   ngHeader->szCredits[ 3 ],
                   ngHeader->szCredits[ 4 ] );
	fclose( f );
	printf( "Found header details, " NG_INFO " written\n" );
    }
    else
    {
	fprintf( stderr, "Error! Can't create " NG_INFO "\n" );
	exit( 1 );
    }
}

/*
 */

void WriteMenu( FILE *fleNG, NGTIDYHEAD *ngHeader )
{
    FILE *fMenu      = fopen( iFrames ? NG_FMENU : NG_MENU, "w" );
    FILE *fFrameMenu = NULL;
    int iID;
    int i = 0;
    long l;

    if ( iFrames )
    {
	fFrameMenu = fopen( NG_MENU, "w" );
    }

    if ( fMenu != NULL && ( fFrameMenu != NULL || !iFrames ) )
    {
	StandardHeader( fMenu, ngHeader, "Menu" );
        SafePrint( fMenu, "<OL>\n" );

	if ( iFrames )
	{
	    SafePrint( fFrameMenu, "<HTML>\n<HEAD><TITLE>%s - Menu</TITLE>"
                       "</HEAD>\n<FRAMESET COLS=\"%s\">\n<NOFRAMES>\n%s\n"
                       "<OL>\n", ngHeader->szTitle, pszFrameCols,
                       pszBodyString );
	}

	do
	{
	    iID = ReadWord( fleNG );
	    
	    switch ( iID )
	    {
	    case 0 :
	    case 1 :
		SkipSection( fleNG );
		break;
	    case 2 :
		l = ftell( fleNG );
		WriteDropMenu( fleNG, fMenu );
		if ( iFrames )
		{
		    fseek( fleNG, l, SEEK_SET );
		    WriteDropMenu( fleNG, fFrameMenu );
		}
		++i;
		break;
	    default :
		iID = 5;
	    }
	} while ( iID != 5 && i != ngHeader->usMenuCount );
	
	SafePrint( fMenu, "</OL>\n" );
	StandardFooter( fMenu );

	if ( iFrames )
	{
	    SafePrint( fFrameMenu, "</OL>\n</NOFRAMES>\n"
                       "<FRAME SRC=\"" NG_FMENU "\" NAME=\"menu\">\n"
                       "<FRAME SRC=\"" NG_INFO  "\" NAME=\"display\">\n"
                       "</FRAMESET>\n</HTML>\n" );
	}
    }
    else
    {
	if ( fMenu == NULL )
	{
	    fprintf( stderr, "Error! Can't create %s!\n",
		    iFrames ? NG_FMENU : NG_MENU );
	}
	if ( fFrameMenu == NULL )
	{
	    fprintf( stderr, "Error! Can't create " NG_MENU "!\n" );
	}
	exit( 1 );
    }
}

/*
 */

void WriteDropMenu( FILE *fleNG, FILE *fleMenu )
{
    int iItems;
    int i;
    long l;
    char szTitle[ 51 ];
    long *lpOffsets;
    
    (void) ReadWord( fleNG );
    iItems = ReadWord( fleNG );
    
    fseek( fleNG, 20L, SEEK_CUR );
    
    lpOffsets = (long *) calloc( iItems, sizeof( long ) );
    
    assert( lpOffsets ); /* Ouch! */
    
    for ( i = 1; i < iItems; i++ )
    {
	lpOffsets[ i - 1 ] = ReadLong( fleNG );
    }
    
    l = ftell( fleNG );
    l += ( 8 * iItems );
    
    fseek( fleNG, l, SEEK_SET );
    
    GetStrZ( fleNG, szTitle, 40 );

    SafePrint( fleMenu, "<H2><LI>%s</LI></H2><P>\n", szTitle );
    SafePrint( fleMenu, "<OL>\n" );
    
    for ( i = 1; i < iItems; i++ )
    {
	GetStrZ( fleNG, szTitle, 50 );

	if ( !*szTitle )
	{
	    strcpy( szTitle, "(No Text For Menu Item)" );
	}
	
	SafePrint( fleMenu, "<H3><LI><A HREF=\"" NG_PREFIX NG_ID
                   NG_SUFFIX "\"%s>%s</H3></A></LI><P>\n", 
                   lpOffsets[ i - 1 ], iFrames ? " TARGET=\"display\" " : "",
                   szTitle );
    }
    
    SafePrint( fleMenu, "</OL>\n" );
    
    (void) getc( fleNG );
    
    free( lpOffsets );

    printf( "Found a drop menu\n" );
}

/*
 */

void WriteBody( FILE *fleNG, NGTIDYHEAD *pHead )
{
    int iID;
    long lOffset;
    long lReset;

    do
    {
	lOffset = ftell( fleNG );
	iID     = ReadWord( fleNG );
	lReset  = ftell( fleNG );
	
	switch ( iID )
	{
	case 0 :
	    WriteShort( fleNG, lOffset, pHead );
	    fseek( fleNG, lReset, SEEK_SET );
	    SkipSection( fleNG );
	    break;
	case 1 :
	    WriteLong( fleNG, lOffset, pHead );
	    fseek( fleNG, lReset, SEEK_SET );
	    SkipSection( fleNG );
	    break;
	case 2 :
	    SkipSection( fleNG );
	    break;
	default :
	    printf( "Done!\n" );
	    iID = 5;
	}
    } while ( iID != 5 );
}

/*
 */

void WriteShort( FILE *fleNG, long lOffset, NGTIDYHEAD *pHead )
{
    FILE     *f;
    char     szName[ 30 ]; /* Overkill!!! */
    int      iItems;
    long     *plOffsets;
    int      i;
    char     szBuffer[ 512 ];
    long     lParent;
    unsigned uParentLine;

    sprintf( szName, NG_PREFIX NG_ID NG_SUFFIX, lOffset );

    printf( "Found a short entry, writing %s\n", szName );

    if ( ( f = fopen( szName, "w" ) ) != NULL )
    {

	StandardHeader( f, pHead, "Short Entry" );

	(void) ReadWord( fleNG );
	iItems = ReadWord( fleNG );
	(void) ReadWord( fleNG );
	
	uParentLine = (unsigned) ReadWord( fleNG );
	lParent     = ReadLong( fleNG );
	
	fseek( fleNG, 12L, SEEK_CUR );
	
	plOffsets = (long *) calloc( iItems, sizeof( long ) );
	
	assert( plOffsets ); /* Ouch! */
	
	for ( i = 0; i < iItems; i++ )
	{
	    ReadWord( fleNG );
	    plOffsets[ i ] = ReadLong( fleNG );
	}
	
	if ( lParent > 0 && uParentLine < 0xFFFF )
	{
	    SafePrint( f, "<A HREF=\"" NG_PREFIX NG_ID NG_SUFFIX "\">"
                       "[^^Up^^]</A>\n", lParent );
	}
	else
	{
	    SafePrint( f, "[^^Up^^]\n" );
	}
	
	StandardButtons( f );
	
	SafePrint( f, "<HR>\n<PRE>\n" );
	
	for ( i = 0; i < iItems; i++ )
	{
	    GetStrZ( fleNG, szBuffer, sizeof( szBuffer ) );
	    if ( plOffsets[ i ] > 0 )
	    {
		SafePrint( f, "<A HREF=\"" NG_PREFIX NG_ID NG_SUFFIX "\">", 
                           plOffsets[ i ] );
		ExpandText( f, szBuffer );
		SafePrint( f, "</A>\n" );
	    }
	    else
	    {
		ExpandText( f, szBuffer );
		SafePrint( f, "\n" );
	    }
	}
	
	free( plOffsets );

	SafePrint( f, "</PRE>\n" );

	StandardFooter( f );

	fclose( f );
    }
    else
    {
	fprintf( stderr, "\nError! Can't create %s!\n", szName );
	exit( 1 );
    }
}

/*
 */

void WriteLong( FILE *fleNG, long lOffset, NGTIDYHEAD *pHead ) 
{ 
    FILE     *f;
    char     szName[ 30 ]; /* Overkill! */
    int      iItems;
    int      iSeeAlsos;
    int      i;
    int      iCnt;
    long     lPrevious;
    long     lNext;
    char     szBuffer[ 512 ];
    long     *plOffsets;
    long     lParent;
    unsigned uParentLine;

    sprintf( szName, NG_PREFIX NG_ID NG_SUFFIX, lOffset );

    printf( "Found a long entry, writing %s\n", szName );

    if ( ( f = fopen( szName, "w" ) ) != NULL )
    {
	StandardHeader( f, pHead, "Long Entry" );

	(void) ReadWord( fleNG );
    
	iItems      = ReadWord( fleNG );
	iSeeAlsos   = ReadWord( fleNG );
	uParentLine = (unsigned) ReadWord( fleNG );
	lParent     = ReadLong( fleNG );
	
	fseek( fleNG, 4L, SEEK_CUR );
	
	lPrevious = ReadLong( fleNG );
	lNext     = ReadLong( fleNG );
	
	lPrevious = ( lPrevious == -1L ? 0L : lPrevious );
	lNext     = ( lNext == -1L ? 0L : lNext );
	
	if ( lPrevious )
	{
	    SafePrint( f, "<A HREF=\"" NG_PREFIX NG_ID NG_SUFFIX 
                       "\">[&lt;&lt;Previous Entry]</A>\n", lPrevious );
	}
	else
	{
	    SafePrint( f, "[&lt;&lt;Previous Entry]\n" );
	}
	
	if ( lParent > 0 && uParentLine < 0xFFFF )
	{
	    SafePrint( f, "<A HREF=\"" NG_PREFIX NG_ID NG_SUFFIX 
                       "\">[^^Up^^]</A>\n", lParent );
	}
	else
	{
	    SafePrint( f, "[^^Up^^]\n" );
	}
	
	if ( lNext )
	{
	    SafePrint( f, "<A HREF=\"" NG_PREFIX NG_ID NG_SUFFIX 
                       "\">[Next Entry&gt;&gt;]</A>\n", lNext );
	}
	else
	{
	    SafePrint( f, "[Next Entry&gt;&gt;]\n" );
	}
	
	StandardButtons( f );
	
	SafePrint( f, "<HR>\n<PRE>\n" );
	
	for ( i = 0; i < iItems; i++ )
	{
	    GetStrZ( fleNG, szBuffer, sizeof( szBuffer ) );
	    ExpandText( f, szBuffer );
	    SafePrint( f, "\n" );
	}

	SafePrint( f, "</PRE>\n" );
        
	if ( iSeeAlsos )
	{
	    SafePrint( f, "<HR>\n<B>See Also:</B>\n" );
	    
	    iCnt      = ReadWord( fleNG );
	    plOffsets = (long *) calloc( iCnt, sizeof( long ) );
	    
	    for ( i = 0; i < iCnt; i++ )
	    {
		/* There is a reason for this < 20 check, but
		   I can't remember what it is, so I'll leave
		   it in for the moment. */
		
		if ( i < 20 )
		{
		    plOffsets[ i ] = ReadLong( fleNG );
		}
		else
		{
		    fseek( fleNG, 4L, SEEK_SET );
		}
	    }
	    
	    for ( i = 0; i < iCnt; i++ )
	    {
		if ( i < 20 )
		{
		    GetStrZ( fleNG, szBuffer, sizeof( szBuffer ) );
		    SafePrint( f, "<A HREF=\"" NG_PREFIX NG_ID NG_SUFFIX 
                               "\">%s</A>\n", plOffsets[ i ], szBuffer );
		}
	    }
	}

	StandardFooter( f );

	fclose( f );
    }
    else
    {
	fprintf( stderr, "\nError! Can't create %s!\n", szName );
	exit( 1 );
    }
} 

/*
 */

void SkipSection( FILE *fleNG )
{
    int iLen = ReadWord( fleNG );
    
    fseek( fleNG, (long) 22L + iLen, SEEK_CUR );
}

/*
 */

int ReadWord( FILE *f )
{
    unsigned char b1 = NG_READBYTE( f );
    unsigned char b2 = NG_READBYTE( f );
    
    return( ( b2 << 8 ) + b1 );
}

/*
 */

long ReadLong( FILE *f )
{
    int i1 = ReadWord( f );
    int i2 = ReadWord( f );

#ifdef _16_BIT_COMPILER_
    long l  = 0;
    int  *i = (int *) &l;

    i[ 0 ] = i1;
    i[ 1 ] = i2;

    return( (long) l );
#else
    return( ( i2 << 16 ) + i1 );
#endif
}

/*
 */

void GetStrZ( FILE *f, char *pszBuffer, int iMax )
{
    long lSavPos = ftell( f );
    int  fEOS = 0;
    int  i;
    
    (void) fread( pszBuffer, iMax, 1, f );
    
    for ( i = 0; i < iMax && !fEOS; i++, pszBuffer++ )
    {
	*pszBuffer = NG_DECRYPT( *pszBuffer );
	fEOS       = ( *pszBuffer == 0 );
    }
    
    fseek( f, (long) lSavPos + i, SEEK_SET );
}

/*
 */

void TidyHeader( NGHEADER *pngHeader, NGTIDYHEAD *pngTidy )
{
    int i;
    int n;

    for ( i = 0; i < 40; i++ )
    {
        pngHeader->szTitle[ i ] = SaneText( pngHeader->szTitle[ i ] );
    }

    for ( i = 0; i < 5; i++ )
    {
        for ( n = 0; n < 66; n++ )
        {
            pngHeader->szCredits[ i ][ n ] = 
                SaneText( pngHeader->szCredits[ i ][ n ] );
        }
    }

    pngTidy->usMenuCount = pngHeader->usMenuCount;
    strncpy( pngTidy->szTitle, pngHeader->szTitle, 40 );

    for ( i = 0; i < 5; i++ )
    {
        strncpy( pngTidy->szCredits[ i ], pngHeader->szCredits[ i ], 66 );
    }
}

/*
 */

unsigned char SaneText( unsigned char bChar )
{
    switch ( bChar )
    {
    case 0xB3 :            /* Vertical graphics. */
    case 0xB4 :
    case 0xB5 :
    case 0xB6 :
    case 0xB9 :
    case 0xBA :
    case 0xC3 :
    case 0xCC :
	bChar = '|';
	break;
    case 0xC4 :            /* Horizontal graphics. */
    case 0xC1 :
    case 0xC2 :
    case 0xC6 :
    case 0xC7 :
    case 0xCA :
    case 0xCB :
    case 0xCD :
    case 0xCF :
    case 0xD0 :
    case 0xD1 :
    case 0xD2 :
	bChar = '-';
	break;
    case 0xB7 :            /* Corner graphics. */
    case 0xB8 :
    case 0xBB :
    case 0xBC :
    case 0xBD :
    case 0xBE :
    case 0xBF :
    case 0xC0 :
    case 0xC5 :
    case 0xC8 :
    case 0xC9 :
    case 0xCE :
    case 0xD3 :
    case 0xD4 :
    case 0xD5 :
    case 0xD6 :
    case 0xD7 :
    case 0xD8 :
    case 0xD9 :
    case 0xDA :
	bChar = '+';
	break;
    case 0xDB :            /* Block graphics */
    case 0xDC :
    case 0xDD :
    case 0xDE :
    case 0xDF :
    case 0xB0 :
    case 0xB1 :
    case 0xB2 :
	bChar = '#';
	break;
    default :
	if ( ( bChar < ' ' || bChar > 0x7E ) && bChar )
	{
	    bChar = '.';
	}
    }
    
    return( bChar );
}

/*
 */

void ExpandText( FILE *f, char *pszText )
{
    char *psz   = pszText;
    int  iBold  = 0;
    int  iUnder = 0;
    int  iSpaces;
    int  i;
    
    while ( *psz )
    {
	switch ( *psz )
	{
	case '^' :
	    ++psz;
	    
	    switch ( *psz )
	    {
	    case 'a' :
	    case 'A' :
		++psz;
		/* I suppose I could use the colour attribute, but for
		   the moment just skip it. */
		(void) Hex2Byte( psz );
		++psz;
		break;
	    case 'b' :
	    case 'B' :
		SafePrint( f, iBold ? "</B>" : "<B>" );
		iBold = !iBold;
		break;
	    case 'c' :
	    case 'C' :
		++psz;
		PrintHtmlChar( f, SaneText( (char) Hex2Byte( psz ) ) );
		++psz;
		break;
	    case 'n' :
	    case 'N' :
		if ( iBold ) SafePrint( f, "</B>" );
		if ( iUnder ) SafePrint( f, "</U>" );
		iBold = 0;
		iUnder = 0;
		break;
	    case 'r' :
	    case 'R' :
		/* Turn on reverse */
		break;
	    case 'u' :
	    case 'U' :
		SafePrint( f, iUnder ? "</U>" : "<U>" );
		iUnder = !iUnder;
		break;
	    case '^' :
		SafePrint( f, "^" );
		break;
	    default :
		--psz;
	    }
	    break;
	case (char) 0xFF :
	    ++psz;
	    
	    iSpaces = *psz;
	    
	    for ( i = 0; i < iSpaces; i++ )
	    {
		SafePrint( f, " " );
	    }
	    break;
	default :
	    PrintHtmlChar( f, (char) SaneText( *psz ) );
	    break;
	}
	
	++psz;
    }
    
    if ( iBold ) SafePrint( f, "</B>" );
    if ( iUnder ) SafePrint( f, "</U>" );
}

/*
 */

void PrintHtmlChar( FILE *f, char c )
{
    switch ( c )
    {
    case '<' :
	SafePrint( f, "&lt;" );
	break;
    case '>' :
	SafePrint( f, "&gt;" );
	break;
    case '&' :
	SafePrint( f, "&amp;" );
	break;
    case 0x0 :
	SafePrint( f, " " );
	break;
    default :
	SafePrint( f, "%c", c );
    }
}

/*
 */

int Hex2Byte( char *psz )
{
    int iByte;
    int i;
    unsigned char bByte = 0;
    
    for ( i = 0; i < 2; i++, psz++ )
    {
	*psz = (char) toupper( *psz );
	
	if ( *psz > '/' && *psz < ':' )
	{
	    iByte = ( ( (int) *psz ) - '0' );
	}
	else if ( *psz > '@' && *psz < 'G' )
	{
	    iByte = ( ( (int) *psz ) - '7' );
	}
        else
        {
            iByte = 0;
	}
	bByte += ( iByte * ( !i ? 16 : 1 ) );
    }
    
    return( (int) bByte );
}

/*
 */

void StandardButtons( FILE *f )
{
    SafePrint( f, "<A HREF=\"%s\" %s>[Menu]</A>\n", 
               iFrames ? NG_FMENU : NG_MENU,
               iFrames ? "TARGET=\"menu\"" : "" );
    SafePrint( f, "<A HREF=\"" NG_INFO "\">[About The Guide]</A>\n" );
}

/*
 */

void StandardHeader( FILE *f, NGTIDYHEAD *pHead, char *pszPageType )
{
    SafePrint( f, "<HTML>\n<HEAD><TITLE>%s - %s</TITLE></HEAD>\n%s\n\n",
               pHead->szTitle, pszPageType, pszBodyString );
}

/*
 */

void StandardFooter( FILE *f )
{
#ifndef NO_ADVERT
    SafePrint( f, "<HR>\nThis page created by ng2html v" NG2HTML_VERSION
               ", the Norton guide to HTML conversion utility.\n"
               "Written by <A HREF=\"http://www.acemake.com/hagbard\">"
               "Dave Pearson</A>\n<HR>" );
#endif
    SafePrint( f, "\n</BODY>\n</HTML>\n" );
}

/*
 */

void SafePrint( FILE *f, char *fmt, ... )
{
    va_list args;

    va_start( args, fmt );

    if ( !vfprintf( f, fmt, args ) )
    {
	fprintf( stderr, "Write error! Out of diskspace perhaps?\n" );
	exit( 1 );
    }

    va_end( args );
}

/*
 */

void LoadConfig( void )
{
    char *p;

    if ( cfgReadFile( CONFIG_FILE ) )
    {
	if ( cfgGetSetting( "BODY" ) )
	{
	    pszBodyString = cfgGetSetting( "BODY" );
	}
	if ( cfgGetSetting( "FRAMECOLS" ) )
	{
	    pszFrameCols  = cfgGetSetting( "FRAMECOLS" );
	}

	if ( ( p = cfgGetSetting( "FRAMES" ) ) )
	{
	    iFrames = ( *p == '1' || *p == 'Y' || *p == 'y' );
	}
    }
}
