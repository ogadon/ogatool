/* 
 * 7 line program : Othello by 2ch
 * http://piza2.2ch.net/test/read.cgi?bbs=tech&key=984182993
 */
#include <stdio.h> 
#define R(x,v,w,y,z) r(i,c,x,(v>w?w:v))|r(i,c,-x,(y>z?z:y)) 
char n[]=" \\MD[REJQZBCIUY:<;A@^XP5]=`8cdkVbmFNWelGO9?_gf32>4on0H1i67jha";
int i,c,p,s[8],m[64],y,z;

d()
{
    for(c=0;c<64;c++){
	printf("%s%s","„©\0œ\0›"+m[c]*3,(2-(c%8==7)-(c==63))+"\n\n");
    }
    printf("%cM%cM%cM%cM%cM%cM%cM%cM%cM%cM\n",27,27,27,27,27,27,27,27,27,27);
    usleep(500000);
}

r(int i,int c,int d,int e)
{
    p=0;
    while(e-->0&&(c+=d,m[c])) {
	if(m[c]-i) {
	    s[p++]=c;
	} else{
	    d=p;
	    while(p) {
		m[s[--p]]=i;
	    }
	    return d;
	}
    }
    return 0;
}
q(int i,int c)
{
    y=c%8,z=c/8;
    if(R(1,7-y,7,y,7)|R(7,y,7-z,7-y,z)|R(8,7,7-z,7,z)|R(9,7-y,7-z,y,z))
    {
	m[c]=i;
	d();
    } else {
	q(3-i,c);
    }
}

main()
{
    m[28]=m[35]=1;
    m[27]=m[36]=2;
    d();
    while(i++<60){
	q(2-i%2,n[i]-48);
    }
    printf("\n\n\n\n\n\n\n\n");
} 

