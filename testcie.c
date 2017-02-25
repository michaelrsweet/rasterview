#include <stdio.h>
#include <math.h>


/*
 * Define some math constants that are required...
 */

#ifndef M_PI
#  define M_PI		3.14159265358979323846
#endif /* !M_PI */

#ifndef M_SQRT2
#  define M_SQRT2	1.41421356237309504880
#endif /* !M_SQRT2 */

#ifndef M_SQRT1_2
#  define M_SQRT1_2	0.70710678118654752440
#endif /* !M_SQRT1_2 */

/*
 * CIE XYZ whitepoint...
 */

#define D65_X	(0.412453 + 0.357580 + 0.180423)
#define D65_Y	(0.212671 + 0.715160 + 0.072169)
#define D65_Z	(0.019334 + 0.119193 + 0.950227)


/*
 * 'cielab()' - Map CIE Lab transformation...
 */

static float				/* O - Adjusted color value */
cielab(float x,				/* I - Raw color value */
       float xn)			/* I - Whitepoint color value */
{
  float x_xn;				/* Fraction of whitepoint */


  x_xn = x / xn;

  if (x_xn > 0.008856)
    return (cbrt(x_xn));
  else
    return (7.787 * x_xn + 16.0 / 116.0);
}


static void decode_lab(unsigned char *val, float *lab)
{
  lab[0] = val[0] / 2.55f;
  lab[1] = val[1] - 128.0f;
  lab[2] = val[2] - 128.0f;

  printf("        Lab = %.3f %.3f %.3f\n", lab[0], lab[1], lab[2]);
}


static void decode_lab16(unsigned short *val, float *lab)
{
  lab[0] = val[0] / 655.35f;
  lab[1] = val[1] / 256.0f - 128.0f;
  lab[2] = val[2] / 256.0f - 128.0f;

  printf("        Lab = %.3f %.3f %.3f\n", lab[0], lab[1], lab[2]);
}


static void
decode_rgb(unsigned char *val, float *rgb)
{
  rgb[0] = val[0] / 255.0f;
  rgb[1] = val[1] / 255.0f;
  rgb[2] = val[2] / 255.0f;

  printf("        RGB = %.3f %.3f %.3f\n", rgb[0], rgb[1], rgb[2]);
}


#if 0
static void
decode_rgb16(unsigned short *val, float *rgb)
{
  rgb[0] = val[0] / 65535.0f;
  rgb[1] = val[1] / 65535.0f;
  rgb[2] = val[2] / 65535.0f;

  printf("        RGB = %.3f %.3f %.3f\n", rgb[0], rgb[1], rgb[2]);
}
#endif /* 0 */


static void
decode_xyz(unsigned char *val, float *xyz)
{
  xyz[0] = val[0] / 231.8181f;
  xyz[1] = val[1] / 231.8181f;
  xyz[2] = val[2] / 231.8181f;

  printf("        XYZ = %.3f %.3f %.3f\n", xyz[0], xyz[1], xyz[2]);
}


static void
decode_xyz16(unsigned short *val, float *xyz)
{
  xyz[0] = val[0] / 59577.2727f;
  xyz[1] = val[1] / 59577.2727f;
  xyz[2] = val[2] / 59577.2727f;

  printf("        XYZ = %.3f %.3f %.3f\n", xyz[0], xyz[1], xyz[2]);
}


static void encode_lab(float *lab, unsigned char *val)
{
  float	ciel,				/* CIE L value */
	ciea,				/* CIE a value */
	cieb;				/* CIE b value */

 /*
  * Scale the L value and bias the a and b values by 128 so that all
  * numbers are from 0 to 255.
  */

  ciel = lab[0] * 2.55 + 0.5;
  ciea = lab[1] + 128.5;
  cieb = lab[2] + 128.5;

 /*
  * Output 8-bit values...
  */

  if (ciel < 0.0)
    val[0] = 0;
  else if (ciel < 255.0)
    val[0] = (int)ciel;
  else
    val[0] = 255;

  if (ciea < 0.0)
    val[1] = 0;
  else if (ciea < 255.0)
    val[1] = (int)ciea;
  else
    val[1] = 255;

  if (cieb < 0.0)
    val[2] = 0;
  else if (cieb < 255.0)
    val[2] = (int)cieb;
  else
    val[2] = 255;

  printf("        Lab = %d %d %d\n", val[0], val[1], val[2]);
}


static void encode_lab16(float *lab, unsigned short *val)
{
  float	ciel,				/* CIE L value */
	ciea,				/* CIE a value */
	cieb;				/* CIE b value */

 /*
  * Scale the L value and bias the a and b values by 128 so that all
  * numbers are from 0 to 65535.
  */

  ciel = lab[0] * 655.35 + 0.5;
  ciea = (lab[1] + 128.0) * 256.0 + 0.5;
  cieb = (lab[2] + 128.0) * 256.0 + 0.5;

 /*
  * Output 16-bit values...
  */

  if (ciel < 0.0)
    val[0] = 0;
  else if (ciel < 65535.0)
    val[0] = (int)ciel;
  else
    val[0] = 65535;

  if (ciea < 0.0)
    val[1] = 0;
  else if (ciea < 65535.0)
    val[1] = (int)ciea;
  else
    val[1] = 65535;

  if (cieb < 0.0)
    val[2] = 0;
  else if (cieb < 65535.0)
    val[2] = (int)cieb;
  else
    val[2] = 65535;

  printf("        Lab = %d %d %d\n", val[0], val[1], val[2]);
}


static void
encode_rgb(float *rgb, unsigned char *val)
{
  // Save it...
  if (rgb[0] < 0.0f)
    val[0] = 0;
  else if (rgb[0] < 1.0f)
    val[0] = (int)(255.0f * rgb[0] + 0.5);
  else
    val[0] = 255;

  if (rgb[1] < 0.0f)
    val[1] = 0;
  else if (rgb[1] < 1.0f)
    val[1] = (int)(255.0f * rgb[1] + 0.5);
  else
    val[1] = 255;

  if (rgb[2] < 0.0f)
    val[2] = 0;
  else if (rgb[2] < 1.0f)
    val[2] = (int)(255.0f * rgb[2] + 0.5);
  else
    val[2] = 255;

  printf("        RGB = %d %d %d\n", val[0], val[1], val[2]);
}


#if 0
static void
encode_rgb16(float *rgb, unsigned short *val)
{
  // Save it...
  if (rgb[0] < 0.0f)
    val[0] = 0;
  else if (rgb[0] < 1.0f)
    val[0] = (int)(65535.0f * rgb[0] + 0.5);
  else
    val[0] = 65535;

  if (rgb[1] < 0.0f)
    val[1] = 0;
  else if (rgb[1] < 1.0f)
    val[1] = (int)(65535.0f * rgb[1] + 0.5);
  else
    val[1] = 65535;

  if (rgb[2] < 0.0f)
    val[2] = 0;
  else if (rgb[2] < 1.0f)
    val[2] = (int)(65535.0f * rgb[2] + 0.5);
  else
    val[2] = 65535;

  printf("        RGB = %d %d %d\n", val[0], val[1], val[2]);
}
#endif /* 0 */


static void encode_xyz(float *xyz, unsigned char *val)
{
 /*
  * Output 8-bit values...
  */

  if (xyz[0] < 0.0f)
    val[0] = 0;
  else if (xyz[0] < 1.1f)
    val[0] = (int)(231.8181f * xyz[0] + 0.5);
  else
    val[0] = 255;

  if (xyz[1] < 0.0f)
    val[1] = 0;
  else if (xyz[1] < 1.1f)
    val[1] = (int)(231.8181f * xyz[1] + 0.5);
  else
    val[1] = 255;

  if (xyz[2] < 0.0f)
    val[2] = 0;
  else if (xyz[2] < 1.1f)
    val[2] = (int)(231.8181f * xyz[2] + 0.5);
  else
    val[2] = 255;

  printf("        XYZ = %d %d %d\n", val[0], val[1], val[2]);
}


static void encode_xyz16(float *xyz, unsigned short *val)
{
 /*
  * Output 8-bit values...
  */

  if (xyz[0] < 0.0f)
    val[0] = 0;
  else if (xyz[0] < 1.1f)
    val[0] = (int)(59577.2727f * xyz[0] + 0.5);
  else
    val[0] = 65535;

  if (xyz[1] < 0.0f)
    val[1] = 0;
  else if (xyz[1] < 1.1f)
    val[1] = (int)(59577.2727f * xyz[1] + 0.5);
  else
    val[1] = 65535;

  if (xyz[2] < 0.0f)
    val[2] = 0;
  else if (xyz[2] < 1.1f)
    val[2] = (int)(59577.2727f * xyz[2] + 0.5);
  else
    val[2] = 65535;

  printf("        XYZ = %d %d %d\n", val[0], val[1], val[2]);
}


static void
lab_to_xyz(float *lab, float *xyz)
{
  float	p;


  // Convert Lab to XYZ...
  if (lab[0] < 8)
    p = lab[0] / 903.3;
  else
    p = (lab[0] + 16.0f) / 116.0f;
  xyz[0] = D65_X * pow(p + lab[1] * 0.002, 3.0);
  xyz[1] = D65_Y * pow(p, 3.0);
  xyz[2] = D65_Z * pow(p - lab[2] * 0.005, 3.0);

  printf("        XYZ = %.3f %.3f %.3f\n", xyz[0], xyz[1], xyz[2]);
}


static void
xyz_to_lab(float *xyz, float *lab)
{
  float	ciex,				/* CIE X value */
	ciey,				/* CIE Y value */
	ciez,				/* CIE Z value */
	ciey_yn;			/* Normalized luminance */


 /*
  * Normalize and convert to CIE Lab...
  */

  ciex    = xyz[0];
  ciey    = xyz[1];
  ciez    = xyz[2];
  ciey_yn = ciey / D65_Y;

  if (ciey_yn > 0.008856)
    lab[0] = 116 * cbrt(ciey_yn) - 16;
  else
    lab[0] = 903.3 * ciey_yn;

  lab[1] = 500 * (cielab(ciex, D65_X) - cielab(ciey, D65_Y));
  lab[2] = 200 * (cielab(ciey, D65_Y) - cielab(ciez, D65_Z));

  printf("        Lab = %.3f %.3f %.3f\n", lab[0], lab[1], lab[2]);
}


static void rgb_to_xyz(float *rgb, float *xyz)
{
  float	r,				/* Red value */
	g,				/* Green value */
	b;				/* Blue value */


 /*
  * Convert sRGB to linear RGB...
  */

  r = pow((rgb[0] + 0.055) / 1.055, 2.4);
  g = pow((rgb[1] + 0.055) / 1.055, 2.4);
  b = pow((rgb[2] + 0.055) / 1.055, 2.4);

  printf("       lRGB = %.3f %.3f %.3f\n", r, g, b);

 /*
  * Convert to CIE XYZ...
  */

  xyz[0] = 0.412453 * r + 0.357580 * g + 0.180423 * b; 
  xyz[1] = 0.212671 * r + 0.715160 * g + 0.072169 * b;
  xyz[2] = 0.019334 * r + 0.119193 * g + 0.950227 * b;

  printf("        XYZ = %.3f %.3f %.3f\n", xyz[0], xyz[1], xyz[2]);
}


static void xyz_to_rgb(float *xyz, float *rgb)
{
  // Convert XYZ to sRGB...
  rgb[0] =  3.240479f * xyz[0] - 1.537150f * xyz[1] - 0.498535f * xyz[2];
  rgb[1] = -0.969256f * xyz[0] + 1.875992f * xyz[1] + 0.041556f * xyz[2];
  rgb[2] =  0.055648f * xyz[0] - 0.204043f * xyz[1] + 1.057311f * xyz[2];

  printf("       lRGB = %.3f %.3f %.3f\n", rgb[0], rgb[1], rgb[2]);

  rgb[0] = rgb[0] <= 0.0 ? 0.0 : 1.055f * pow(rgb[0], 0.41666) - 0.055f;
  rgb[1] = rgb[1] <= 0.0 ? 0.0 : 1.055f * pow(rgb[1], 0.41666) - 0.055f;
  rgb[2] = rgb[2] <= 0.0 ? 0.0 : 1.055f * pow(rgb[2], 0.41666) - 0.055f;

  printf("        RGB = %.3f %.3f %.3f\n", rgb[0], rgb[1], rgb[2]);
}


int
main(void)
{
  int		i;
  float		rgb[3],
		xyz[3],
		lab[3],
		rgb2[3],
		xyz2[3];
  unsigned char	rgbval[3],
		xyzval[3],
		labval[3];
  unsigned short xyzval16[3],
		labval16[3];
  static struct
  {
    const char		*name;
    unsigned char	rgb[3];
  }		colors[] =
  {
    { "BLACK",   {   0,   0,   0 } },
    { "RED",     { 255,   0,   0 } },
    { "GREEN",   {   0, 255,   0 } },
    { "YELLOW",  { 255, 255,   0 } },
    { "BLUE",    {   0,   0, 255 } },
    { "MAGENTA", { 255,   0, 255 } },
    { "CYAN",    {   0, 255, 255 } },
    { "WHITE",   { 255, 255, 255 } }
  };


  for (i = 0; i < 8; i ++)
  {
    rgbval[0] = colors[i].rgb[0];
    rgbval[1] = colors[i].rgb[1];
    rgbval[2] = colors[i].rgb[2];

    printf("%-8sRGB = %d %d %d\n", colors[i].name, rgbval[0], rgbval[1],
           rgbval[2]);

    decode_rgb(rgbval, rgb);
    rgb_to_xyz(rgb, xyz);
    encode_xyz(xyz, xyzval);
    encode_xyz16(xyz, xyzval16);
    xyz_to_lab(xyz, lab);
    encode_lab(lab, labval);
    encode_lab16(lab, labval16);

    putchar('\n');

    decode_lab(labval, lab);
    lab_to_xyz(lab, xyz2);
    xyz_to_rgb(xyz2, rgb2);
    encode_rgb(rgb2, rgbval);

    putchar('\n');

    decode_xyz(xyzval, xyz);
    xyz_to_rgb(xyz, rgb);
    encode_rgb(rgb, rgbval);

    putchar('\n');

    decode_lab16(labval16, lab);
    lab_to_xyz(lab, xyz2);
    xyz_to_rgb(xyz2, rgb2);
    encode_rgb(rgb2, rgbval);

    putchar('\n');

    decode_xyz16(xyzval16, xyz);
    xyz_to_rgb(xyz, rgb);
    encode_rgb(rgb, rgbval);

    putchar('\n');
  }

  return (0);
}
