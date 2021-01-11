#include "gfx.h"
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

//04/29/2005: Fixed warnings in OpenWatcom 1.3. Fixed move&look up/down speed.
//10/08/2002: Added screen capture to X.PNG using F12.
//   Last touch before today was:
//      CAVE.C 11/17/1994 03:22 AM 15,722
//   Looks like I ported GROUCAVE.BAS->CAVE.C around 04/20/1994 to 04/21/1994

#define MAXPALOOKUPS 64

#define SINTABLE_HALF_PI 512
#define SINTABLE_ENTRIES (SINTABLE_HALF_PI * 4)

#define WORLD_DIM 256

long posx, posy, posz, horiz, xdim, ydim;
long newposz, vel, svel, angvel;
short ang, vidmode;

unsigned char h1[WORLD_DIM * WORLD_DIM], c1[WORLD_DIM * WORLD_DIM];
unsigned char h2[WORLD_DIM * WORLD_DIM], c2[WORLD_DIM * WORLD_DIM];
int16_t sintable[SINTABLE_ENTRIES];
unsigned char scrbuf[128000];
short numpalookups;
unsigned char palookup[MAXPALOOKUPS<<8], palette[768];

// Operation commonly used in this file: akin to modulo operator, but for powers of 2.
#define MOD_PO2(X, P) ((X) & ((P) - 1))

// Assumes angles in range of 0 to 2048. Values are -2^14 to 2^14.
#define sin(X) sintable[MOD_PO2(X, SINTABLE_ENTRIES)]
// Equals to SINTABLE_HALF_PI - X, (like in the trig identity) except for some off-by-1 numbers
#define cos(X) sintable[MOD_PO2((X) + SINTABLE_HALF_PI, SINTABLE_ENTRIES)]

#define POS_TO_GRID(X) (((X) >> 10) & (WORLD_DIM - 1))

volatile char keystatus[256];

long scale(long a, long b, long c)
{
	return (a * b) / c;
}

long mulscale(long a, long b, long c)
{
	return (a * b) >> (c & 0xff);
}

long divscale(long a, long b, long c)
{
	return (a << (c & 0xff)) / b;
}

long groudiv(long a, long b)
{
	return ((a << 12) - posz) / ((b >> 8) & 0xffff);
}

long drawtopslab(long edi, long ecx, long eax)
{
	int al = eax & 0xFF;
	int carry;

	carry = ecx & 1;
	ecx >>= 1;

	if (carry == 0)
		goto skipdraw1a;

	scrbuf[edi] = al;

	edi += 80;

skipdraw1a:
	carry = ecx & 1;
	ecx >>= 1;

	if (carry == 0)
		goto skipdraw2a;

	scrbuf[edi] = al;
	scrbuf[edi + 80] = al;
	edi += 160;

skipdraw2a:
	if (ecx == 0)
		goto skipdraw4a;

startdrawa:
	scrbuf[edi] = al;
	scrbuf[edi + 80] = al;
	scrbuf[edi + 160] = al;
	scrbuf[edi + 240] = al;
	edi += 320;

	ecx--;
	if (ecx != 0)
		goto startdrawa;

skipdraw4a:
	eax = edi;

	return eax;
}

long drawbotslab(long edi, long ecx, long eax)
{
	int al = eax & 0xFF;
	int carry;

	carry = ecx & 1;
	ecx >>= 1;

	if (carry == 0)
		goto skipdraw1b;

	scrbuf[edi] = al;

	edi -= 80;

skipdraw1b:
	carry = ecx & 1;
	ecx >>= 1;

	if (carry == 0)
		goto skipdraw2b;

	scrbuf[edi] = al;
	scrbuf[edi - 80] = al;
	edi -= 160;

skipdraw2b:
	if (ecx == 0)
		goto skipdraw4b;

startdrawb:
	scrbuf[edi] = al;
	scrbuf[edi - 80] = al;
	scrbuf[edi - 160] = al;
	scrbuf[edi - 240] = al;
	edi -= 320;

	ecx--;
	if (ecx != 0)
		goto startdrawb;

skipdraw4b:
	eax = edi;

	return eax;
}

void keydown(char key, int down)
{
	if (key < 0)
		return;

	keystatus[(unsigned char)key] = down;
}

void grouvline (short x, long scandist);
void blast (long gridx, long gridy, long rad, unsigned char blastingcol);
void loadtables ();
void loadpalette ();
void loadboard ();

int main ()
{
	unsigned char blastcol;
	long i, j;

	vidmode = 0;
	xdim = 320;
	ydim = 200;
	blastcol = 0;

	gfx_init(xdim, ydim);

	loadpalette();
	loadtables();
	loadboard();

	blast(POS_TO_GRID(posx), POS_TO_GRID(posy), 8L, blastcol);

	while (gfx_update(keydown))
	{
		for(i=0;i<xdim;i++)
			grouvline((short)i,128L);                 //Draw to non-video memory

		if (keystatus[GFX_ESC])
		{
			break;
		}

		if (keystatus[','] > 0)   // ,< Change blasting color
		{
			keystatus[','] = 0;
			blastcol = ((blastcol+64)&255);
		}
		if (keystatus['.'] > 0)   // .> Change blasting color
		{
			keystatus['.'] = 0;
			blastcol = ((blastcol+192)&255);
		}
		if (keystatus[' '] > 0)
		{
			if (keystatus[GFX_CTRL] > 0)
				blast(POS_TO_GRID(posx),POS_TO_GRID(posy),16L,blastcol);
			else
				blast(POS_TO_GRID(posx),POS_TO_GRID(posy),8L,blastcol);
		}

		vel = 0L;
		svel = 0L;
		angvel = 0;

		if (keystatus['='] > 0) horiz++;
		if (keystatus['-'] > 0) horiz--;
		if (keystatus['a'] > 0)
		{
			posz -= (1<<(keystatus[GFX_SHIFT]+8));
			if (posz < 2048) posz = 2048;
		}
		if (keystatus['z'] > 0)
		{
			posz += (1<<(keystatus[GFX_SHIFT]+8));
			if (posz >= 1048576-4096-2048) posz = 1048575-4096-2048;
		}
		if (keystatus[GFX_CTRL] == 0)
		{
			if (keystatus[GFX_LEFT] > 0) angvel = -16;
			if (keystatus[GFX_RIGHT] > 0) angvel = 16;
		}
		else
		{
			if (keystatus[GFX_LEFT] > 0) svel = 12L;
			if (keystatus[GFX_RIGHT] > 0) svel = -12L;
		}
		if (keystatus[GFX_UP] > 0) vel = 12L;
		if (keystatus[GFX_DOWN] > 0) vel = -12L;
		if (keystatus[GFX_SHIFT] > 0)
		{
			vel <<= 1;
			svel <<= 1;
		}
		if (angvel != 0)
		{
			ang += angvel>>3;
			ang = MOD_PO2(ang, SINTABLE_ENTRIES);
		}
		if ((vel != 0L) || (svel != 0L))
		{
			posx += ((vel*cos(ang))>>12);
			posy += ((vel*sin(ang))>>12);
			posx += ((svel*sin(ang))>>12);
			posy -= ((svel*cos(ang))>>12);
			posx &= 0x3ffffff;
			posy &= 0x3ffffff;
		}

		if ((keystatus['s']|keystatus['d']) > 0)
		{
			if (keystatus['s'] > 0)
			{
				if (vidmode == 0)
				{
					vidmode = 1;
					keystatus['s'] = 0;
					ydim = 400L;
					horiz <<= 1;
				}
			}
			if (keystatus['d'] > 0)
			{
				if (vidmode == 1)
				{
					vidmode = 0;
					keystatus['d'] = 0;
					ydim = 200L;
					horiz >>= 1;
				}
			}
		}

		j = (vidmode + 1) * (320 >> 2) * 200;
		for (int y = 0; y < (vidmode + 1) * 200; ++y)
			for (int x = 0; x < xdim; ++x) {
				int palette_idx = scrbuf[(320 >> 2) * y + (x >> 2) + j * (x & 3)];
				uint8_t r = palette[palette_idx * 3 + 0] * 4,
				        g = palette[palette_idx * 3 + 1] * 4,
				        b = palette[palette_idx * 3 + 2] * 4;
				uint32_t color = r << 16 | g << 8 | b;
				gfx_set_pixel(x, y, color);
			}

		gfx_draw();
	}

	gfx_destroy();

	return 0;
}

void loadboard ()
{
	long i, j;

	posx = 512; posy = 512; posz = ((128-32)<<12); ang = 0;
	horiz = (ydim>>1);
	for(i=0;i<WORLD_DIM;i++)
		for(j=0;j<WORLD_DIM;j++)
		{
			h1[i * WORLD_DIM + j] = 255;
			c1[i * WORLD_DIM + j] = 128;
			h2[i * WORLD_DIM + j] = 0;
			c2[i * WORLD_DIM + j] = 128;
		}
}

void loadpalette ()
{
	long i, fil;

	if ((fil = open("palette.dat",O_RDONLY)) == -1)
	{
		printf("Can't load palette.dat.  Now why could that be?\n");
		exit(0);
	}

	read(fil,&palette[0],768);
	read(fil,&numpalookups,2);
	read(fil,&palookup[0],numpalookups<<8);
	close(fil);
}

void loadtables ()
{
	short fil;

	if ((fil = open("tables.dat",O_RDONLY)) != -1)
	{
		read(fil,sintable,sizeof(sintable));
		close(fil);
	}
}

long ksqrt (long num)
{
	long root, temp;

	root = 128;
	do
	{
		temp = root;
		root = ((root+(num/root))>>1);
	}
	while (labs(temp-root) > 1);
	return(root);
}

void blast (long gridx, long gridy, long rad, unsigned char blastingcol)
{
	short tempshort;
	long i, j, dax, day, daz, dasqr, templong;

	templong = rad+2;
	for(i=-templong;i<=templong;i++)
		for(j=-templong;j<=templong;j++)
		{
			dax = MOD_PO2(gridx + i, WORLD_DIM);
			day = MOD_PO2(gridy + j, WORLD_DIM);
			dasqr = rad*rad-(i*i+j*j);

			if (dasqr >= 0)
				daz = (ksqrt(dasqr)<<1);
			else
				daz = -(ksqrt(-dasqr)<<1);

			if ((posz>>12)-daz < h1[(dax<<8)+day])
			{
				h1[(dax<<8)+day] = (posz>>12)-daz;
				if (((posz>>12)-daz) < 0)
					h1[(dax<<8)+day] = 0;
			}

			if ((posz>>12)+daz > h2[(dax<<8)+day])
			{
				h2[(dax<<8)+day] = (posz>>12)+daz;
				if (((posz>>12)+daz) > 255)
					h2[(dax<<8)+day] = 255;
			}

			tempshort = h1[(dax<<8)+day];
			if (tempshort >= h2[(dax<<8)+day]) tempshort = (posz>>12);
			tempshort = labs(64-(tempshort&127))+(rand()&3)-2;
			if (tempshort < 0) tempshort = 0;
			if (tempshort > 63) tempshort = 63;
			c1[(dax<<8)+day] = (unsigned char)(tempshort+blastingcol);

			tempshort = h2[(dax<<8)+day];
			if (tempshort <= h1[(dax<<8)+day]) tempshort = (posz>>12);
			tempshort = labs(64-(tempshort&127))+(rand()&3)-2;
			if (tempshort < 0) tempshort = 0;
			if (tempshort > 63) tempshort = 63;
			c2[(dax<<8)+day] = (unsigned char)(tempshort+blastingcol);
		}
}

void grouvline (short x, long scandist)
{
	long dist[2], dinc[2], incr[2];
	short grid[2], dir[2];

	unsigned char oh1, oh2;
	short um, dm, h, i;
	long plc1, plc2, cosval, sinval;
	long snx, sny, dax, c, shade, cnt, bufplc;

	plc1 = (0     )*80+(x>>2);
	plc2 = (ydim-1)*80+(x>>2);
	if ((x&2) > 0)
	{
		plc1 += 32000*(vidmode+1);
		plc2 += 32000*(vidmode+1);
	}
	if ((x&1) > 0)
	{
		plc1 += 16000*(vidmode+1);
		plc2 += 16000*(vidmode+1);
	}

	cosval = cos(ang);
	sinval = sin(ang);

	dax = (x<<1)-xdim;

	incr[0] = cosval - scale(sinval,dax,xdim);
	incr[1] = sinval + scale(cosval,dax,xdim);

	if (incr[0] < 0) dir[0] = -1, incr[0] = -incr[0]; else dir[0] = 1;
	if (incr[1] < 0) dir[1] = -1, incr[1] = -incr[1]; else dir[1] = 1;
	snx = (posx&1023); if (dir[0] == 1) snx ^= 1023;
	sny = (posy&1023); if (dir[1] == 1) sny ^= 1023;
	cnt = ((snx*incr[1] - sny*incr[0])>>10);
	grid[0] = POS_TO_GRID(posx);
	grid[1] = POS_TO_GRID(posy);

	if (incr[0] != 0)
	{
		dinc[0] = divscale(65536>>vidmode,incr[0],12);
		dist[0] = mulscale(dinc[0],snx,10);
	}
	if (incr[1] != 0)
	{
		dinc[1] = divscale(65536>>vidmode,incr[1],12);
		dist[1] = mulscale(dinc[1],sny,10);
	}

	um = (0     )-horiz;
	dm = (ydim-1)-horiz;

	i = incr[0]; incr[0] = incr[1]; incr[1] = -i;

	shade = 8;
	while (dist[cnt>=0] <= 8192)
	{
		i = (cnt>=0);

		grid[i] = ((grid[i]+dir[i])&255);
		dist[i] += dinc[i];
		cnt += incr[i];
		shade++;
	}

	bufplc = (grid[0]<<8)+grid[1];

	while (shade < scandist-9)
	{
		i = (cnt>=0);

		oh1 = h1[bufplc], oh2 = h2[bufplc];

		h = groudiv((long)oh1,dist[i]);
		if (um <= h)
		{
			c = palookup[((shade>>1)<<8)+c1[bufplc]];
			if (h > dm) break;
			plc1 = drawtopslab(plc1,h-um+1,c);
			um = h+1;
		}

		h = groudiv((long)oh2,dist[i]);
		if (dm >= h)
		{
			c = palookup[((shade>>1)<<8)+c2[bufplc]];
			if (h < um) break;
			plc2 = drawbotslab(plc2,dm-h+1,c);
			dm = h-1;
		}

		grid[i] = MOD_PO2(grid[i] + dir[i], WORLD_DIM);
		bufplc = (grid[0]<<8)+grid[1];

		if (h1[bufplc] > oh1)
		{
			h = groudiv((long)h1[bufplc],dist[i]);
			if (um <= h)
			{
				c = palookup[(((shade>>1)-(i<<2))<<8)+c1[bufplc]];
				if (h > dm) break;
				plc1 = drawtopslab(plc1,h-um+1,c);
				um = h+1;
			}
		}

		if (h2[bufplc] < oh2)
		{
			h = groudiv((long)h2[bufplc],dist[i]);
			if (dm >= h)
			{
				c = palookup[(((shade>>1)+(i<<2))<<8)+c2[bufplc]];
				if (h < um) break;
				plc2 = drawbotslab(plc2,dm-h+1,c);
				dm = h-1;
			}
		}

		dist[i] += dinc[i];
		cnt += incr[i];
		shade++;
	}

	if (dm >= um)
	{
		if (shade >= scandist-9) c = palookup[(numpalookups-1)<<8];
		drawtopslab(plc1,dm-um+1,c);
	}
}
