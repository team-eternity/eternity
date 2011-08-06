/*
 * SHA-1 implementation in JavaScript
 * Copyright Chris Veness 2002-2010
 * www.movable-type.co.uk (http://www.movable-type.co.uk/scripts/sha1.html)
 * see http://csrc.nist.gov/groups/ST/toolkit/secure_hashing.html
 *     http://csrc.nist.gov/groups/ST/toolkit/examples.html
 */

var Sha1={};Sha1.hash=function(msg,utf8encode){utf8encode=(typeof utf8encode=='undefined')?true:utf8encode;if(utf8encode)msg=Utf8.encode(msg);var K=[0x5a827999,0x6ed9eba1,0x8f1bbcdc,0xca62c1d6];msg+=String.fromCharCode(0x80);var l=msg.length/4+2;var N=Math.ceil(l/16);var M=new Array(N);for(var i=0;i<N;i++){M[i]=new Array(16);for(var j=0;j<16;j++){M[i][j]=(msg.charCodeAt(i*64+j*4)<<24)|(msg.charCodeAt(i*64+j*4+1)<<16)|(msg.charCodeAt(i*64+j*4+2)<<8)|(msg.charCodeAt(i*64+j*4+3));}}
M[N-1][14]=((msg.length-1)*8)/Math.pow(2,32);M[N-1][14]=Math.floor(M[N-1][14])
M[N-1][15]=((msg.length-1)*8)&0xffffffff;var H0=0x67452301;var H1=0xefcdab89;var H2=0x98badcfe;var H3=0x10325476;var H4=0xc3d2e1f0;var W=new Array(80);var a,b,c,d,e;for(var i=0;i<N;i++){for(var t=0;t<16;t++)W[t]=M[i][t];for(var t=16;t<80;t++)W[t]=Sha1.ROTL(W[t-3]^W[t-8]^W[t-14]^W[t-16],1);a=H0;b=H1;c=H2;d=H3;e=H4;for(var t=0;t<80;t++){var s=Math.floor(t/20);var T=(Sha1.ROTL(a,5)+Sha1.f(s,b,c,d)+e+K[s]+W[t])&0xffffffff;e=d;d=c;c=Sha1.ROTL(b,30);b=a;a=T;}
H0=(H0+a)&0xffffffff;H1=(H1+b)&0xffffffff;H2=(H2+c)&0xffffffff;H3=(H3+d)&0xffffffff;H4=(H4+e)&0xffffffff;}
return Sha1.toHexStr(H0)+Sha1.toHexStr(H1)+
Sha1.toHexStr(H2)+Sha1.toHexStr(H3)+Sha1.toHexStr(H4);}
Sha1.f=function(s,x,y,z){switch(s){case 0:return(x&y)^(~x&z);case 1:return x^y^z;case 2:return(x&y)^(x&z)^(y&z);case 3:return x^y^z;}}
Sha1.ROTL=function(x,n){return(x<<n)|(x>>>(32-n));}
Sha1.toHexStr=function(n){var s="",v;for(var i=7;i>=0;i--){v=(n>>>(i*4))&0xf;s+=v.toString(16);}
return s;}
var Utf8={};Utf8.encode=function(strUni){var strUtf=strUni.replace(/[\u0080-\u07ff]/g,function(c){var cc=c.charCodeAt(0);return String.fromCharCode(0xc0|cc>>6,0x80|cc&0x3f);});strUtf=strUtf.replace(/[\u0800-\uffff]/g,function(c){var cc=c.charCodeAt(0);return String.fromCharCode(0xe0|cc>>12,0x80|cc>>6&0x3F,0x80|cc&0x3f);});return strUtf;}
Utf8.decode=function(strUtf){var strUni=strUtf.replace(/[\u00e0-\u00ef][\u0080-\u00bf][\u0080-\u00bf]/g,function(c){var cc=((c.charCodeAt(0)&0x0f)<<12)|((c.charCodeAt(1)&0x3f)<<6)|(c.charCodeAt(2)&0x3f);return String.fromCharCode(cc);});strUni=strUni.replace(/[\u00c0-\u00df][\u0080-\u00bf]/g,function(c){var cc=(c.charCodeAt(0)&0x1f)<<6|c.charCodeAt(1)&0x3f;return String.fromCharCode(cc);});return strUni;}
