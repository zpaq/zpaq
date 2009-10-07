// exe_jo.cpp - (E8E9 transform from paq8px_v64)

#include <stdio.h>
#include <stdlib.h>

void encode_exe(FILE* in, FILE* out, int len, int begin) {
  const int BLOCK=0x10000;
  unsigned char blk[BLOCK];
  fprintf(out, "%c%c%c%c", begin>>24, begin>>16, begin>>8, begin);

  // Transform
  for (int offset=0; offset<len; offset+=BLOCK) {
    int size=(((len-offset)<BLOCK)?(len-offset):BLOCK);
    int bytesRead=fread(&blk[0], 1, size, in);
    for (int i=bytesRead-1; i>=5; --i) {
      if ((blk[i-4]==0xe8 || blk[i-4]==0xe9 || (blk[i-5]==0x0f && (blk[i-4]&0xf0)==0x80))
         && (blk[i]==0||blk[i]==0xff)) {
        int a=(blk[i-3]|blk[i-2]<<8|blk[i-1]<<16|blk[i]<<24)+offset+begin+i+1;
        a<<=7;
        a>>=7;
        blk[i]=a>>24;
        blk[i-1]=a^176;
        blk[i-2]=(a>>8)^176;
        blk[i-3]=(a>>16)^176;
      }
    }
    fwrite(&blk[0], 1, bytesRead, out);
  }
}

int detect(FILE* in, int n, int type) {
  unsigned int buf1=0, buf0=0;  // last 8 bytes
  long start=ftell(in);

  // For EXE detection
  int abspos[256],  // CALL/JMP abs. addr. low byte -> last offset
    relpos[256];    // CALL/JMP relative addr. low byte -> last offset
  int e8e9count=0;  // number of consecutive CALL/JMPs
  int e8e9pos=0;    // offset of first CALL or JMP instruction
  int e8e9last=0;   // offset of most recent CALL or JMP
  for (int i=0; i<256; ++i) abspos[i]=relpos[i]=0;

  for (int i=0; i<n; ++i) {
    int c=getc(in);
    if (c==EOF) return -1;
    buf1=buf1<<8|buf0>>24;
    buf0=buf0<<8|c;

    // Detect EXE if the low order byte (little-endian) XX is more
    // recently seen (and within 4K) if a relative to absolute address
    // conversion is done in the context CALL/JMP (E8/E9) XX xx xx 00/FF
    // 4 times in a row.  Detect end of EXE at the last
    // place this happens when it does not happen for 64KB.

    if (((buf1&0xfe)==0xe8 || (buf1&0xfff0)==0x0f80) && ((buf0+1)&0xfe)==0) {
      int r=buf0>>24;  // relative address low 8 bits
      int a=((buf0>>24)+i)&0xff;  // absolute address low 8 bits
      int rdist=i-relpos[r];
      int adist=i-abspos[a];
      if (adist<rdist && adist<0x800 && abspos[a]>5) {
        e8e9last=i;
        ++e8e9count;
        if (e8e9pos==0 || e8e9pos>abspos[a]) e8e9pos=abspos[a];
      }
      else e8e9count=0;
      if (type==0 && e8e9count>=4 && e8e9pos>5)
        return fseek(in, start+e8e9pos-5, SEEK_SET), 1;
      abspos[a]=i;
      relpos[r]=i;
    }
    if (i-e8e9last>0x4000) {
      if (type==1) return fseek(in, start+e8e9last, SEEK_SET), 0;
      e8e9count=e8e9pos=0;
    }
  }
}

void transform(FILE *in, FILE *out) {
  fseek(in, 0, SEEK_END);
  long begin=0, n=ftell(in), n0=n;
  int type=0;

  // Transform and test in blocks
  rewind(in);
  while (n>0) {
    int nextType=detect(in, n, type);
    long end=ftell(in);
    fseek(in, begin, SEEK_SET);
    if (end>n0) {  // if some detection reports longer then actual size file is
      end=begin+1;
      type=0;
    }
    int len=int(end-begin);
    if (len>0) {
      putc(type, out);
      putc(len>>24, out);
      putc(len>>16, out);
      putc(len>>8, out);
      putc(len, out);
      if (type==1) {
        encode_exe(in, out, len, begin);
      } else {
        for (int j=0; j<len; ++j) putc(getc(in), out);
      }
    }
    n-=len;
    type=nextType;
    begin=end;
  }
}

void decode_exe(FILE *in, FILE *out, int size, int begin) {
  const int BLOCK=0x10000;  // block size
  int offset=6, a;
  unsigned char c[6];
  for (int i=4; i>=0; i--) c[i]=getc(in);  // Fill queue

  while (offset<size+6) {
    c[5]=c[4]; c[4]=c[3]; c[3]=c[2]; c[2]=c[1]; c[1]=c[0];
    if (offset<=size) c[0]=getc(in);
    // E8E9 transform: E8/E9 xx xx xx 00/FF -> subtract location from x
    if ((c[0]==0x00 || c[0]==0xFF) && (c[4]==0xE8 || c[4]==0xE9 || (c[5]==0x0F && (c[4]&0xF0)==0x80))
     && (((offset-1)^(offset-6))&-BLOCK)==0 && offset<=size) { // not crossing block boundary
      a=((c[1]^176)|(c[2]^176)<<8|(c[3]^176)<<16|c[0]<<24)-offset-begin;
      a<<=7;
      a>>=7;
      c[3]=a;
      c[2]=a>>8;
      c[1]=a>>16;
      c[0]=a>>24;
    }
    putc(c[5], out);
    offset++;
  }
}

void itransform(FILE *in, FILE *out) {
  int type, len;
  long begin;
  while ((type=getc(in))!=EOF) {
    len=getc(in);
    len=(len<<8)+getc(in);
    len=(len<<8)+getc(in);
    len=(len<<8)+getc(in);
    if (type==1) {
      begin=getc(in);
      begin=(begin<<8)+getc(in);
      begin=(begin<<8)+getc(in);
      begin=(begin<<8)+getc(in);
      decode_exe(in, out, len, begin);
    } else {
      for (int j=0; j<len; ++j) putc(getc(in), out);
    }
  }
}

int main(int argc, char **argv) {
  if (argc<4 || (argv[1][0]!='e' && argv[1][0]!='d')) printf("To transform: exe_jo e input output\n To inverse transform exe_jo d input output\n"), exit(1);
 
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