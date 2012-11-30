#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<math.h>
#include<stdint.h>
#include<string>
using std::string;
typedef uint8_t U8;
typedef uint16_t U16;
typedef uint32_t U32;
typedef uint64_t U64;
template<typename T>
struct Array{
T*data;
size_t n;
Array(size_t sz=0,int ex=0):data(0),n(0){
resize(sz,ex);
}
void resize(size_t sz,int ex=0);
~Array(){
resize(0);
}
size_t size(){
return n;
}
T&operator[](size_t i){
return data[i];
}
T&operator()(size_t i){
return data[i&(n-1)];
}
}
;
template<typename T>
void Array<T>::resize(size_t sz,int ex){
if(n>0)free((char*)data);
n=0;
if(sz==0)return;
n=sz<<ex;
const size_t nb=128+n*sizeof(T);
data=(T*)calloc(nb,1);
}
typedef enum{
NONE,CONS,CM,ICM,MATCH,AVG,MIX2,MIX,ISSE,SSE}
CompType;
const int compsize[256]={
0,2,3,2,3,4,6,6,3,5}
;
struct ZP{
ZP(){
out=0;
clear();
}
void clear();
void run(U32 input){
pc=hbegin;
a=input;
while(execute());
}
int read(FILE*in2);
FILE*out;
Array<U8>hd,m;
Array<U32>h,r;
int cend,hbegin,hend,f,pc;
U32 a,b,c,d;
void init(int hbits,int mbits);
int execute();
void div(U32 x){
if(x)a/=x;
else a=0;
}
void mod(U32 x){
if(x)a%=x;
else a=0;
}
void swap(U32&x){
a^=x;
x^=a;
a^=x;
}
void swap(U8&x){
a^=x;
x^=a;
a^=x;
}
}
;
int ZP::read(FILE*in2){
int hsize=getc(in2);
hsize+=getc(in2)*256;
hd.resize(hsize+300);
cend=hbegin=hend=0;
hd[cend++]=hsize&255;
hd[cend++]=hsize>>8;
while(cend<7)hd[cend++]=getc(in2);
int n=hd[cend-1];
for(int i=0;i<n;++i){
int type=getc(in2);
hd[cend++]=type;
int size=compsize[type];
for(int j=1;j<size;++j)
hd[cend++]=getc(in2);
}
hd[cend++]=getc(in2);
hbegin=hend=cend+128;
while(hend<hsize+129){
int op=getc(in2);
hd[hend++]=op;
}
hd[hend++]=getc(in2);
return cend+hend-hbegin;
}
void ZP::clear(){
cend=hbegin=hend=0;
hd.resize(0);
a=b=c=d=f=pc=0;
h.resize(0);
m.resize(0);
r.resize(0);
}
void ZP::init(int hbits,int mbits){
a=b=c=d=f=pc=0;
h.resize(1,hbits);
m.resize(1,mbits);
r.resize(256);
}
#define C(i,L,R)\
case i:L(a R);break;\
case i+1:L(b R);break;\
case i+2:L(c R);break;\
case i+3:L(d R);break;\
case i+4:L(U32(m(b))R);break;\
case i+5:L(U32(m(c))R);break;\
case i+6:L(h(d) R);break;\
case i+7:L(U32(hd[pc++])R);break;
#define C1(i,x)\
case i:swap(x);break;\
case i+1:++x;break;\
case i+2:--x;break;\
case i+3:x=~x;break;\
case i+4:x=0;break;
int ZP::execute(){
switch(hd[pc++]){
case 7:a=r[hd[pc++]];break;
case 15:b=r[hd[pc++]];break;
case 23:c=r[hd[pc++]];break;
case 31:d=r[hd[pc++]];break;
case 39:if(f)pc+=((hd[pc]+128)&255)-127;else ++pc;break;
case 47:if(!f)pc+=((hd[pc]+128)&255)-127;else ++pc;break;
case 55:r[hd[pc++]]=a;break;
case 56:return 0;
case 57:if(out)putc(a,out);break;
case 59:a=(a+m(b)+512)*773;break;
case 60:h(d)=(h(d)+a+512)*773;break;
case 63:pc+=((hd[pc]+128)&255)-127;break;
C1(0,a)
C1(8,b)
C1(16,c)
C1(24,d)
C1(32,m(b))
C1(40,m(c))
C1(48,h(d))
C(64,a=,)
C(72,b=,)
C(80,c=,)
C(88,d=,)
C(96,m(b)=,)
C(104,m(c)=,)
C(112,h(d)=,)
C(128,a+=,)
C(136,a-=,)
C(144,a*=,)
C(152,div,)
C(160,mod,)
C(168,a&=,)
C(176,a&= ~,)
C(184,a|=,)
C(192,a^=,)
C(200,a<<=,&31)
C(208,a>>=,&31)
C(216,f=,==a)
C(224,f=,>a)
C(232,f=,<a)
}
return 1;
}
struct CP{
U32 limit,cxt,a,b,c;
Array<U32>cm;
Array<U8>ht;
Array<U16>a16;
void init();
CP(){
init();
}
}
;
void CP::init(){
limit=cxt=a=b=c=0;
cm.resize(0);
ht.resize(0);
a16.resize(0);
}
struct ST{
enum{
N=64}
;
int nns(int n0,int n1);
void ds(int&n0);
void nx(int&n0,int&n1,int y);
U8 ns[1024];
int cminit(int state){
return((ns[state*4+3]*2+1)<<22)/(ns[state*4+2]+ns[state*4+3]+1);
}
ST();
}
;
int ST::nns(int n0,int n1){
const int B=6,bound[B]={
20,48,15,8,6,5}
;
if(n0<n1)return nns(n1,n0);
if(n0<0||n1<0||n1>=B||n0>bound[n1])return 0;
return 1+(n1>0&&n0+n1<=17);
}
void ST::ds(int&n0){
n0=(n0>=1)+(n0>=2)+(n0>=3)+(n0>=4)+(n0>=5)+(n0>=7)+(n0>=8);
}
void ST::nx(int&n0,int&n1,int y){
if(n0<n1)
nx(n1,n0,1-y);
else{
if(y){
++n1;
ds(n0);
}
else{
++n0;
ds(n1);
}
while(!nns(n0,n1)){
if(n1<2)--n0;
else{
n0=(n0*(n1-1)+(n1/2))/n1;
--n1;
}
}
}
}
ST::ST(){
const int N=50;
U8 t[N][N][2]={
{
{
0}
}
}
;
int state=0;
for(int i=0;i<N;++i){
for(int n1=0;n1<=i;++n1){
int n0=i-n1;
int n=nns(n0,n1);
if(n){
t[n0][n1][0]=state;
t[n0][n1][1]=state+n-1;
state+=n;
}
}
}
memset(ns,0,sizeof(ns));
for(int n0=0;n0<N;++n0){
for(int n1=0;n1<N;++n1){
for(int y=0;y<nns(n0,n1);++y){
int s=t[n0][n1][y];
int s0=n0,s1=n1;
nx(s0,s1,0);
ns[s*4+0]=t[s0][s1][0];
s0=n0,s1=n1;
nx(s0,s1,1);
ns[s*4+1]=t[s0][s1][1];
ns[s*4+2]=n0;
ns[s*4+3]=n1;
}
}
}
}
struct PR{
PR(ZP&);
void init();
int predict();
void update(int y);
bool im(){
return z.hd[6]!=0;
}
int c8,hmap4,p[256];
U32 h[256];
ZP&z;
CP comp[256];
int dt2k[256];
int dt[1024];
U16 squasht[4096];
short stretcht[32768];
ST st;
void train(CP&cr,int y){
U32&pn=cr.cm(cr.cxt);
U32 count=pn&0x3ff;
int error=y*32767-(cr.cm(cr.cxt)>>17);
pn+=(error*dt[count]&-1024)+(count<cr.limit);
}
int squash(int x){
return squasht[x+2048];
}
int stretch(int x){
return stretcht[x];
}
int clamp2k(int x){
if(x<-2048)return -2048;
else if(x>2047)return 2047;
else return x;
}
int clamp512k(int x){
if(x<-(1<<19))return -(1<<19);
else if(x>=(1<<19))return(1<<19)-1;
else return x;
}
size_t find(Array<U8>&ht,int sizebits,U32 cxt);
}
;
PR::PR(ZP&zr):
c8(1),hmap4(1),z(zr){
dt2k[0]=0;
for(int i=1;i<256;++i)
dt2k[i]=2048/i;
for(int i=0;i<1024;++i)
dt[i]=(1<<17)/(i*2+3)*2;
for(int i=0;i<32768;++i)
stretcht[i]=int(log((i+0.5)/(32767.5-i))*64+0.5+100000)-100000;
for(int i=0;i<4096;++i)
squasht[i]=int(32768.0/(1+exp((i-2048)*(-1.0/64))));
}
void PR::init(){
z.init(z.hd[2],z.hd[3]);
for(int i=0;i<256;++i)h[i]=p[i]=0;
for(int i=0;i<256;++i)comp[i].init();
int n=z.hd[6];
const U8*cp=&z.hd[7];
for(int i=0;i<n;++i){
CP&cr=comp[i];
switch(cp[0]){
case CONS:
p[i]=(cp[1]-128)*4;
break;
case CM:
cr.cm.resize(1,cp[1]);
cr.limit=cp[2]*4;
for(size_t j=0;j<cr.cm.size();++j)
cr.cm[j]=0x80000000;
break;
case ICM:
cr.limit=1023;
cr.cm.resize(256);
cr.ht.resize(64,cp[1]);
for(size_t j=0;j<cr.cm.size();++j)
cr.cm[j]=st.cminit(j);
break;
case MATCH:
cr.cm.resize(1,cp[1]);
cr.ht.resize(1,cp[2]);
cr.ht(0)=1;
break;
case MIX2:
cr.c=(size_t(1)<<cp[1]);
cr.a16.resize(1,cp[1]);
for(size_t j=0;j<cr.a16.size();++j)
cr.a16[j]=32768;
break;
case MIX:{
int m=cp[3];
cr.c=(size_t(1)<<cp[1]);
cr.cm.resize(m,cp[1]);
for(size_t j=0;j<cr.cm.size();++j)
cr.cm[j]=65536/m;
break;
}
case ISSE:
cr.ht.resize(64,cp[1]);
cr.cm.resize(512);
for(int j=0;j<256;++j){
cr.cm[j*2]=1<<15;
cr.cm[j*2+1]=clamp512k(stretch(st.cminit(j)>>8)<<10);
}
break;
case SSE:
cr.cm.resize(32,cp[1]);
cr.limit=cp[4]*4;
for(size_t j=0;j<cr.cm.size();++j)
cr.cm[j]=squash((j&31)*64-992)<<17|cp[3];
break;
}
cp+=compsize[*cp];
}
}
int PR::predict(){
int n=z.hd[6];
const U8*cp=&z.hd[7];
for(int i=0;i<n;++i){
CP&cr=comp[i];
switch(cp[0]){
case CM:
cr.cxt=h[i]^hmap4;
p[i]=stretch(cr.cm(cr.cxt)>>17);
break;
case ICM:
if(c8==1||(c8&0xf0)==16)cr.c=find(cr.ht,cp[1]+2,h[i]+16*c8);
cr.cxt=cr.ht[cr.c+(hmap4&15)];
p[i]=stretch(cr.cm(cr.cxt)>>8);
break;
case MATCH:
if(cr.a==0)p[i]=0;
else{
cr.c=(cr.ht(cr.limit-cr.b)>>(7-cr.cxt))&1;
p[i]=stretch(dt2k[cr.a]*(cr.c*-2+1)&32767);
}
break;
case AVG:
p[i]=(p[cp[1]]*cp[3]+p[cp[2]]*(256-cp[3]))>>8;
break;
case MIX2:{
cr.cxt=((h[i]+(c8&cp[5]))&(cr.c-1));
int w=cr.a16[cr.cxt];
p[i]=(w*p[cp[2]]+(65536-w)*p[cp[3]])>>16;
}
break;
case MIX:{
int m=cp[3];
cr.cxt=h[i]+(c8&cp[5]);
cr.cxt=(cr.cxt&(cr.c-1))*m;
int*wt=(int*)&cr.cm[cr.cxt];
p[i]=0;
for(int j=0;j<m;++j)
p[i]+=(wt[j]>>8)*p[cp[2]+j];
p[i]=clamp2k(p[i]>>8);
}
break;
case ISSE:{
if(c8==1||(c8&0xf0)==16)
cr.c=find(cr.ht,cp[1]+2,h[i]+16*c8);
cr.cxt=cr.ht[cr.c+(hmap4&15)];
int*wt=(int*)&cr.cm[cr.cxt*2];
p[i]=clamp2k((wt[0]*p[cp[2]]+wt[1]*64)>>16);
}
break;
case SSE:{
cr.cxt=(h[i]+c8)*32;
int pq=p[cp[2]]+992;
if(pq<0)pq=0;
if(pq>1983)pq=1983;
int wt=pq&63;
pq>>=6;
cr.cxt+=pq;
p[i]=stretch(((cr.cm(cr.cxt)>>10)*(64-wt)+(cr.cm(cr.cxt+1)>>10)*wt)
>>13);
cr.cxt+=wt>>5;
}
}
cp+=compsize[cp[0]];
}
return squash(p[n-1]);
}
void PR::update(int y){
const U8*cp=&z.hd[7];
int n=z.hd[6];
for(int i=0;i<n;++i){
CP&cr=comp[i];
switch(cp[0]){
case CM:
train(cr,y);
break;
case ICM:{
cr.ht[cr.c+(hmap4&15)]=st.ns[cr.ht[cr.c+(hmap4&15)]*4+y];
U32&pn=cr.cm(cr.cxt);
pn+=int(y*32767-(pn>>8))>>2;
}
break;
case MATCH:
{
if(int(cr.c)!=y)cr.a=0;
cr.ht(cr.limit)+=cr.ht(cr.limit)+y;
if(++cr.cxt==8){
cr.cxt=0;
++cr.limit;
cr.limit&=(1<<cp[2])-1;
if(cr.a==0){
cr.b=cr.limit-cr.cm(h[i]);
if(cr.b&(cr.ht.size()-1))
while(cr.a<255&&cr.ht(cr.limit-cr.a-1)==cr.ht(cr.limit-cr.a-cr.b-1))
++cr.a;
}
else cr.a+=cr.a<255;
cr.cm(h[i])=cr.limit;
}
}
break;
case MIX2:{
int err=(y*32767-squash(p[i]))*cp[4]>>5;
int w=cr.a16[cr.cxt];
w+=(err*(p[cp[2]]-p[cp[3]])+(1<<12))>>13;
if(w<0)w=0;
if(w>65535)w=65535;
cr.a16[cr.cxt]=w;
}
break;
case MIX:{
int m=cp[3];
int err=(y*32767-squash(p[i]))*cp[4]>>4;
int*wt=(int*)&cr.cm[cr.cxt];
for(int j=0;j<m;++j)
wt[j]=clamp512k(wt[j]+((err*p[cp[2]+j]+(1<<12))>>13));
}
break;
case ISSE:{
int err=y*32767-squash(p[i]);
int*wt=(int*)&cr.cm[cr.cxt*2];
wt[0]=clamp512k(wt[0]+((err*p[cp[2]]+(1<<12))>>13));
wt[1]=clamp512k(wt[1]+((err+16)>>5));
cr.ht[cr.c+(hmap4&15)]=st.ns[cr.cxt*4+y];
}
break;
case SSE:
train(cr,y);
}
cp+=compsize[cp[0]];
}
c8+=c8+y;
if(c8>=256){
z.run(c8-256);
hmap4=1;
c8=1;
for(int i=0;i<n;++i)h[i]=z.h(i);
}
else if(c8>=16&&c8<32)
hmap4=(hmap4&0xf)<<5|y<<4|1;
else
hmap4=(hmap4&0x1f0)|(((hmap4&0xf)*2+y)&0xf);
}
size_t PR::find(Array<U8>&ht,int sizebits,U32 cxt){
int chk=cxt>>sizebits&255;
size_t h0=(cxt*16)&(ht.size()-16);
if(ht[h0]==chk)return h0;
size_t h1=h0^16;
if(ht[h1]==chk)return h1;
size_t h2=h0^32;
if(ht[h2]==chk)return h2;
if(ht[h0+1]<=ht[h1+1]&&ht[h0+1]<=ht[h2+1])
return memset(&ht[h0],0,16),ht[h0]=chk,h0;
else if(ht[h1+1]<ht[h2+1])
return memset(&ht[h1],0,16),ht[h1]=chk,h1;
else
return memset(&ht[h2],0,16),ht[h2]=chk,h2;
}
struct DE{
FILE*in;
DE(ZP&z);
int de();
void init();
U32 low,high;
U32 curr;
PR pr;
int decode(int p);
}
;
DE::DE(ZP&z):
in(0),low(1),high(0xFFFFFFFF),curr(0),pr(z){
}
void DE::init(){
pr.init();
if(pr.im())low=1,high=0xFFFFFFFF,curr=0;
else low=high=curr=0;
}
int DE::decode(int p){
U32 mid=low+U32(((high-low)*U64(U32(p)))>>16);
int y=curr<=mid;
if(y)high=mid;
else low=mid+1;
while((high^low)<0x1000000){
high=high<<8|255;
low=low<<8;
low+=(low==0);
curr=curr<<8|getc(in);
}
return y;
}
int DE::de(){
if(pr.im()){
if(curr==0){
for(int i=0;i<4;++i)
curr=curr<<8|getc(in);
}
if(decode(0)){
return -1;
}
else{
int c=1;
while(c<256){
int p=pr.predict()*2+1;
c+=c+decode(p);
pr.update(c&1);
}
return c-256;
}
}
else{
if(curr==0){
for(int i=0;i<4;++i)
curr=curr<<8|getc(in);
if(curr==0)return -1;
}
--curr;
return getc(in);
}
}
struct PP{
int state;
int hsize;
int ph,pm;
ZP z;
PP():state(0),hsize(0),ph(0),pm(0){
}
void init(int h,int m);
int write(int c);
}
;
void PP::init(int h,int m){
state=hsize=0;
ph=h;
pm=m;
z.clear();
}
int PP::write(int c){
switch(state){
case 0:
state=c+1;
if(state==1)z.clear();
break;
case 1:
if(c>=0&&z.out)putc(c,z.out);
break;
case 2:
hsize=c;
state=3;
break;
case 3:
hsize+=c*256;
z.hd.resize(hsize+300);
z.cend=8;
z.hbegin=z.hend=z.cend+128;
z.hd[4]=ph;
z.hd[5]=pm;
state=4;
break;
case 4:
z.hd[z.hend++]=c;
if(z.hend-z.hbegin==hsize){
hsize=z.cend-2+z.hend-z.hbegin;
z.hd[0]=hsize&255;
z.hd[1]=hsize>>8;
z.init(z.hd[4],z.hd[5]);
state=5;
}
break;
case 5:
z.run(c);
break;
}
return state;
}
struct DC{
DC():z(),dec(z),pp(),ds(0){
}
bool fb();
bool ff(string&);
void de();
ZP z;
DE dec;
PP pp;
int ds;
}
;
bool DC::fb(){
U32 h1=0x3D49B113,h2=0x29EB7F93,h3=0x2614BE13,h4=0x3828EB13;
int c;
while((c=getc(dec.in))!=EOF){
h1=h1*12+c;
h2=h2*20+c;
h3=h3*28+c;
h4=h4*44+c;
if(h1==0xB16B88F1&&h2==0xFF5376F1&&h3==0x72AC5BF1&&h4==0x2F909AF1)
break;
}
if(c<0)return false;
getc(dec.in);
getc(dec.in);
z.read(dec.in);
ds=0;
return true;
}
bool DC::ff(string&filename){
int c=getc(dec.in);
if(c==1){
while(true){
c=getc(dec.in);
if(c==0){
return true;
}
filename+=char(c);
}
}
return false;
}
void DC::de(){
while(getc(dec.in));
getc(dec.in);
if(ds==0){
dec.init();
pp.init(z.hd[4],z.hd[5]);
ds=1;
}
while((pp.state&3)!=1)
pp.write(dec.de());
while(true){
int c=dec.de();
pp.write(c);
if(c==-1)break;
}
if(getc(dec.in)==253){
for(int i=1;i<=20;++i)
getc(dec.in);
}
}
int main(int argc,char**argv){
DC d;
if(argc<2||!(d.dec.in=fopen(argv[1],"rb")))return 0;
string sout;
while(d.fb()){
while(d.ff(sout)){
if(sout!=""){
if(d.pp.z.out)fclose(d.pp.z.out);
d.pp.z.out=fopen(sout.c_str(),"wb");
sout="";
}
d.de();
}
}
return 0;
}
