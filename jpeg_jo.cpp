// jpeg_jo.cpp - (JPEG transformation - decoding/encoding Huffman)

#include <stdio.h>
#include <stdlib.h>

//  JPEG header info
static int huf[384], hufsel[2][10], hbuf[2048];
static int mcusize,  rsti;

// actual position and state
static int state, hcode, len, mcupos, rs,  rst_code;

void parse_ht(int p, unsigned char *b, int pos)
{
  int j, end = p + b[p - 2] * 256 + b[p - 1] - 2, cnt = 0;
  while (p < end && end < pos && end < p + 2048 && ++cnt < 10) {
    int tc = b[p] >> 4, th = b[p] & 15;
    if (tc >= 2 || th >= 4) break;
    const int a1 = p + 17, a2 = tc * 64 + th * 16, a3 = a2 * 16;
    for (j = 0; j < 256; j++) hbuf[a3 + j] = b[a1 + j] / 16 + (b[a1 + j] % 16) * 16;
    int t = 0, c = 0;
    for (j = 0; j < 16; j++) {
      huf[a2 + j] = (c *= 2);
      huf[a2 + j + 128] = (c += b[p + j + 1]);
      huf[a2 + j + 256] = a3 + t;
      t += b[p + j + 1];
    }
    p = a1 + t;
  }
}

void parse_st(int sos, int sof, unsigned char *b)
{
  int s1 = b[sos + 4], s2 = b[sof + 9], t = 0, d = 0;
  if (s1 > 4 || s2 > 4) s1 = s2 = 0;
  for (int i = 0; i < s1; i++)
    for (int j = 0; j < s2; j++)
      if (b[sos + 2 * i + 5] == b[sof + 3 * j + 10]) {
        int x = b[sof + 3 * j + 11], y = b[sos + 2 * i + 6];
        int z = b[sof + 3 * j + 12];
        if ((x >> 4) > d) d = x >> 4;
        x = (x & 15) * (x >> 4);
        x *= (t + x <= 10);
        z *= (z < 4);
        while (x--) {
          hufsel[0][t] = (y >> 4) & 3;
          hufsel[1][t] = y & 3;
          t++;
        }
      }
  mcusize = t * 64;
}

void read_header(FILE *f_input, FILE *f_output)
{
  int p = 0, st = 6, ht[8], hts = 0;
  int sos=0, sof=0, dri=0, dqt=0, data=0;
  unsigned char b[65536], *r;
  mcusize = 0;
  while (!mcusize && p < 65536) {
    b[p++] = fgetc(f_input);
    r = &b[p - 5];
    fputc(r[4], f_output);
    if (st>=0) { st--; if (st>0) continue; } 
    if (r[0] == 255 && r[1] == 218 && r[4] >= 0 && r[4] <= 4 &&
        r[2] * 256 + r[3] == 6 + 2 * r[4]) data = (sos = p - 5) + r[2] * 256 + r[3] + 2;
    if (r[1] == 255) {
      if (st==0 && (r[2] > 192 && r[2] < 255)) st = r[3] * 256 + r[4] + 2;
      if (r[2] == 196 && hts < 8) ht[hts++] = p;
      if (r[2] == 192) sof = p - 4;
      if (r[2] == 219) dqt = p - 2;
      if (r[2] == 221) dri = p + 2;
    } 
    if (p == dri) rsti = r[3] * 256 + r[4];
    if (p == data && sof && sos) {
      for (int i = 0; i < hts; i++) parse_ht(ht[i], b, p);
      parse_st(sos, sof, b);
    }
  }
}

void transform(FILE *in, FILE *out) {
  read_header(in, out);
  int c = fgetc(in);
  state = hcode = len = mcupos = 0;
  while (c >= 0) {
    for (int i = 7; i >= 0; i--) {
      int bit=((c >> i) & 1);

      hcode += hcode + bit;
      len++;
      if (!state) {
        if (len > 16) len = 16;
        int ac = (mcupos & 63) > 0;
        int hi = (abs(ac * 64 + hufsel[ac][mcupos >> 6] * 16 + len - 1)) & 127; 
        if (hcode < huf[hi + 128]) {
          int k = huf[hi + 256] + hcode - huf[hi];
          if (k < 0 || k >= 2048) k = 0;
          rs = hbuf[k];
          hcode = 0;
          state = len + (rs / 16);
        } 
      }
      if (state == len) {
        int r=rs % 16, s= rs / 16;
        if ((mcupos & 63) == 0) {
          mcupos++;
        } else {
          if (rs == 0) {
            while ((mcupos & 63) != 0) mcupos++;
          } else {
            mcupos += r + 1;
          }
        }
        if (mcupos >= mcusize) mcupos = 0;
        fputc(r+s*16, out);
        if (s>8) {
          fputc(hcode/256, out);
          fputc(hcode%256, out);
        } else if (s>0) {
          fputc(hcode, out);
        }
        hcode = len = state = 0;
      }
    }
    if ((c = fgetc(in)) == 255) {
      if (c = fgetc(in)) {
        c = ((c & 248) == 208)?fgetc(in):-1;
        state = hcode = len = mcupos = 0;
      } else c = 255;
    }
  }
}

void write_bits(int c, int num, FILE *f_output)
{
  hcode = hcode * (1 << num) + c;
  len += num;
  while (len >= 8) {
    len -= 8;
    int b = hcode >> len;
    fputc(b, f_output);
    if (b == 255) fputc(0, f_output);
    hcode -= (b << len);
  }
}

void write_rst(int c, FILE *f_output)
{
  if (len > 0) write_bits((1 << (8 - len)) - 1, 8 - len, f_output);
  fputc(255, f_output);
  fputc(208 + c, f_output);
}

void itransform(FILE *in, FILE *out) {
  read_header(in, out);
  state = hcode = len = mcupos = 0;
  rst_code = 0;
  while (1) {
    int i=0, x, t, s, rs;
    while (i<64) {
      rs=fgetc(in);
      if (rs==EOF) { write_rst(9, out); return; }
    if (i==0 && rsti && state == rsti) {
      write_rst(rst_code, out);
      rst_code = (rst_code + 1) % 8;
      state = hcode = len = mcupos = 0;
    }
      s=rs/16;
      x=0;
      if (s>0) x=fgetc(in);
      if (s>8) x=x*256+fgetc(in);
      int k = (i!=0) * 4 + hufsel[i!=0][mcupos >> 6];
      const int *h = &huf[k * 16]; 
      int c = 0, u = 0, t = 0;
      for (int j = 255; j >= 0; j--) if (hbuf[k * 256 + j] == rs) c = j;
      while ((t += h[u + 128] - h[u]) <= c && u < 16) u++;
      c += h[u + 128] - t;
      write_bits(c, u + 1, out);
      write_bits(x, s, out);
      if (rs==0 && i>0) i=64; else i+=(rs%16)+1;
    }
    if ((mcupos += 64) >= mcusize) { mcupos = 0; state++; }
  }
}

int main(int argc, char **argv) {
  if (argc<4 || (argv[1][0]!='e' && argv[1][0]!='d')) printf("To transform: jpeg_jo e input output\n To inverse transform jpeg_jo d input output\n"), exit(1);
 
  FILE *in=fopen(argv[2],"rb");
  if (!in) perror(argv[2]), exit(1);
  FILE *out=fopen(argv[3],"wb");
  if (!out) perror(argv[3]), exit(1);
  if (argv[1][0]=='e') {
    // encoding
    transform(in, out);
  } else {
    // decoding
    itransform(in, out);
  }
  fclose(out);
  fclose(in);
  return 0;
}