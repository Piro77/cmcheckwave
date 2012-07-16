/*********************************************************************
  original base program  http://oku.edu.mie-u.ac.jp/~okumura/dumpwave.c

  cmcheckwave.c

Usage: cmcheckwave filename.wav
 *********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

#include "tclist.h"

static void usage(char *cmd){
	fprintf(stderr,"Check CM from wavefile(output cmd stdout)\n");
	fprintf(stderr,"%s: filename.wav\n\n",cmd);
	fprintf(stderr,"Check CM from mp4file(require ffmpeg cmd)\n");
	fprintf(stderr,"%s: filename.mp4\n\n",cmd);
	fprintf(stderr,"Check CM from mp4file and execute cutcmd(create new filename.mp4-new.mp4) \n");
	fprintf(stderr,"%s: -x filename.mp4\n\n",cmd);
	fprintf(stderr,"Check CM and manual edit\n");
	fprintf(stderr,"%s: -b filename.mp4 filename.mp4 > filename-sh\n",cmd);
	fprintf(stderr,"Edit filename-sh for mis detection and re execute next cmd\n",cmd);
	fprintf(stderr,"%s: -x filename-sh\n",cmd);
	exit(1);
}

unsigned char *get_bytes(FILE *f, int n)
{
	static unsigned char s[16];

	assert (n <= sizeof s);
	if (fread(s, n, 1, f) != 1) {
		fprintf(stderr, "Read error\n");
		exit(1);
	}
	return s;
}

unsigned long get_ulong(FILE *f)
{
	unsigned char *s = get_bytes(f, 4);
	return s[0] + 256LU * (s[1] + 256LU * (s[2] + 256LU * s[3]));
}

unsigned get_ushort(FILE *f)
{
	unsigned char *s = get_bytes(f, 2);
	return s[0] + 256U * s[1];
}

typedef struct {
	int  stsec;
	int  edsec;
	int  diffs;
	char cmflg;
	char honpen;
	int  peak;
}muonst;

static muonst m[1000];
static muonst h[100];
static int verbose=0;
static int noaudioencode=0;
static char *wkfilename=NULL;
static int defmuon=250;
static int defmax=9;
static int thumb=0;
static int txtrecheck=0;
static int cmdexecute=0;
static int checkcomplete=0;

static char *MP4BOXCMDRAPSTR="Adjusting chunk start time to previous random access at ";
#ifdef __FreeBSD__
static char *MP4BOXCMD="/usr/local/bin/MP4Box";
static char *SOXCMD="/usr/local/bin/sox";
static char *FFMPEGCMD="/usr/local/bin/ffmpeg";
static char *MPLAYERCMD="/usr/local/bin/mplayer";
static char *AACENCCMD="/usr/local/bin/aacplusenc";
static char *AACENCOPT="%s '%s.wav' '%s' 60";
static char *FAADCMD=NULL;
#else
static char *MP4BOXCMD="mp4box";
static char *SOXCMD="sox";
static char *FFMPEGCMD="ffmpeg";
static char *MPLAYERCMD="mplayer";
static char *AACENCCMD="neroAacEnc";
static char *AACENCOPT="%s -hev2 -br 60 -if '%s.wav' -of '%s'";
static char *FAADCMD=NULL;
#endif

FILE *checkMP4(FILE *f,char *filename)
{
	char readbuf[20];
	char *cmdbuf;
	FILE *pp;

	memset(readbuf,0,sizeof(readbuf));
	fread(readbuf,sizeof(readbuf),1,f);
	rewind(f);
	if (!strstr(readbuf+4,"ftypisom")) {
		return NULL;
	}
	// mp4ファイルだったら、ffmpegでwaveに変換し読み込む。
	if (FAADCMD)
	    asprintf(&cmdbuf,"%s -d -w -q '%s'",FAADCMD,filename);
    else
	    asprintf(&cmdbuf,"%s -v 0 -i '%s' -f wav pipe: 2>/dev/null",FFMPEGCMD,filename);

	pp = popen(cmdbuf,"r");
	if (pp == NULL) return NULL;
	fclose(f);
	wkfilename = strdup(filename);
	return pp;
}

FILE *openpipeffmpeg(char *filename)
{
	char cmdbuf[1024];
	FILE *pp;



}

int checkMP4RAP(int stsec,int edsec)
{
	FILE *pp;
	char pbuf[1024];
	char cmdbuf[1024];
	float rap;

	if (wkfilename==NULL) return stsec;

	sprintf(cmdbuf,"%s -quiet -noprog -splitx %.2f:%.2f '%s' -out /dev/null",MP4BOXCMD,edsec/1000.0,(edsec+10000)/1000.0,wkfilename);

	rap=0.0;
	pp = popen(cmdbuf,"r");
	if (pp == NULL) return stsec;
	while(fgets(pbuf,1024,pp)!=NULL){
		if (strstr(pbuf,MP4BOXCMDRAPSTR)) {
			sscanf(pbuf+strlen(MP4BOXCMDRAPSTR),"%f",&rap);
			//TODO rapはCM開始フレームの秒数なのでちょっと戻す。
			//     フレームレートとか調べないとだめだな・・・
			rap = rap - 0.04;
		}
	}
	pclose(pp);
	if (rap > 0 && stsec < rap*1000.0) return rap*1000.0;
	else return stsec;

}
int cmpinfo(int mcnt)
{
	int i;
	char *cmdptr,readbuf[20];
	TCLIST *cmdlist;

	if (!wkfilename) return 0;

	cmdlist = tclistnew();

	asprintf(&cmdptr,"#!/bin/sh");
	tclistpush2(cmdlist,cmdptr);
	free(cmdptr);


	for(i=0;i<mcnt;i++) {
		if (thumb) {
			asprintf(&cmdptr,"rm -f '%s-%d.png'",wkfilename,i);
			tclistpush2(cmdlist,cmdptr);
			free(cmdptr);
		}
	}
	// check new mp4
	asprintf(&cmdptr,"%s-new.mp4",wkfilename);
	FILE *fp;
	fp = fopen(cmdptr,"rb");
	if (fp==NULL) {
		//ファイルなし
		return 0;
	}
	fread(readbuf,sizeof(readbuf),1,fp);
	if (!strstr(readbuf+4,"ftypisom")) {
		//mp4じゃない感じ(置き換えないときは無視)
		if (checkcomplete == 1) return 0;
	}
	fclose(fp);
	free(cmdptr);

	if (checkcomplete==1) { //元ファイル置き換え
		asprintf(&cmdptr,"mv '%s-new.mp4' '%s'",wkfilename,wkfilename);
		tclistpush2(cmdlist,cmdptr);
		free(cmdptr);
	}
	if (checkcomplete==2) { //CMカットファイル削除
		asprintf(&cmdptr,"rm -f '%s-new.mp4'",wkfilename);
		tclistpush2(cmdlist,cmdptr);
		free(cmdptr);
	}

	asprintf(&cmdptr,"rm -f '%s-sh'",wkfilename);
	tclistpush2(cmdlist,cmdptr);
	free(cmdptr);

	asprintf(&cmdptr,"rm -f '%s.split.log'",wkfilename);
	tclistpush2(cmdlist,cmdptr);
	free(cmdptr);

	for(i=0;i<tclistnum(cmdlist);i++) {
		if (cmdexecute) {
			FILE *pp;
			char pbuf[1024];
			pp = popen(tclistval2(cmdlist,i),"r");
			if (pp==NULL) {continue;}
			while(fgets(pbuf,1024,pp)!=NULL){
			}
			pclose(pp);
		}
		else
			printf("%s\n",tclistval2(cmdlist,i));
	}

}
int dumpinfo(int mcnt)
{
	int honstart,hcnt,totalsec;
	int i,pre;
	char *cptr,*cptr2,*tfptr;
	TCLIST *cmdlist;
	TCLIST *tflist;

	honstart=0;
	hcnt=0;
	totalsec=0;
	printf("#!/bin/sh\n# cmcheckwave %s\n#\n",wkfilename?wkfilename:"");
	//カットするため、一連のCM,本編時間を結合
	for(i=0;i<mcnt;i++) {
		printf("# %.2f %.2f diff %.2f %s\n",m[i].stsec/1000.0,m[i].edsec/1000.0,m[i].diffs/1000.0,m[i].cmflg?"CM":"");
		//本編開始位置をマーク
		if ((m[i].cmflg==0)&&(honstart==0)) {
			honstart=1;
			if (i==0) h[hcnt].stsec = 0;
			else h[hcnt].stsec = m[i-1].edsec+(defmuon*0.5);
		}
		else {
			//終了位置をマーク
			if ((m[i].cmflg==1)&&(honstart==1)) {
				honstart=0;
				//h[hcnt].edsec = m[i-1].stsec;
				h[hcnt].edsec = checkMP4RAP(m[i-1].stsec,m[i-1].edsec);
				totalsec += h[hcnt].edsec - h[hcnt].stsec;
				hcnt++;
			}
		}
	}
	if (honstart==1) {
		h[hcnt].edsec = checkMP4RAP(m[i-1].stsec,m[i-1].edsec);
		totalsec += h[hcnt].edsec - h[hcnt].stsec;
		hcnt++;
	}

	printf("# total %.2f\n\n",totalsec/1000.0);

	cmdlist = tclistnew();
	tflist = tclistnew();
	if (wkfilename) {
		asprintf(&cptr,"rm -f '%s.split.log'",wkfilename);
		tclistpush2(cmdlist,cptr);
		free(cptr);
	}
	for(i=0;i<hcnt;i++) {
		if (wkfilename) {
			asprintf(&tfptr,"%s.%d%s",wkfilename,i,noaudioencode?".mp4":"");
			asprintf(&cptr,"%s -quiet -noprog -splitx %.2f:%.2f '%s' -out '%s' >> '%s.split.log'",MP4BOXCMD,h[i].stsec/1000.0,h[i].edsec/1000.0,wkfilename,tfptr,wkfilename);
			tclistpush2(cmdlist,cptr);
			tclistpush2(tflist,tfptr);
			free(tfptr);
			free(cptr);

			if (!noaudioencode) {
				asprintf(&tfptr,"%s.%d.wav",wkfilename,i);

				if (FAADCMD)
					asprintf(&cptr,"%s -d -q -o '%s' '%s.%d' ",FAADCMD,tfptr,wkfilename,i);
				else
					asprintf(&cptr,"%s -v 0 -i '%s.%d' -vn '%s'",FFMPEGCMD,wkfilename,i,tfptr);
				tclistpush2(cmdlist,cptr);
				tclistpush2(tflist,tfptr);
				free(tfptr);
				free(cptr);

				asprintf(&tfptr,"%s.%d.mp4",wkfilename,i);
				asprintf(&cptr,"%s -v 0 -i '%s.%d' -an -vcodec copy '%s'",FFMPEGCMD,wkfilename,i,tfptr);
				tclistpush2(cmdlist,cptr);
				tclistpush2(tflist,tfptr);
				free(tfptr);
				free(cptr);
			}
		}
		else {
			asprintf(&cptr,"# %s -quiet -noprog -splitx %.2f:%.2f ",MP4BOXCMD,h[i].stsec/1000.0,h[i].edsec/1000.0);
			tclistpush2(cmdlist,cptr);
		}
	}
	if (wkfilename) {
		if (!noaudioencode) {
			asprintf(&cptr2,"%s --norm ",SOXCMD);
			for(i=0;i<hcnt;i++) {
				asprintf(&tfptr,"%s.%d.wav",wkfilename,i);
				asprintf(&cptr,"%s '%s' ",cptr2,tfptr);
				free(cptr2);
				cptr2=cptr;
				tclistpush2(tflist,tfptr);
				free(tfptr);
			}
			asprintf(&tfptr,"%s.wav",wkfilename);
			asprintf(&cptr,"%s '%s'",cptr2,tfptr);
			free(cptr2);
			tclistpush2(cmdlist,cptr);
			tclistpush2(tflist,tfptr);
			free(tfptr);

			asprintf(&tfptr,"%s.aac",wkfilename);
			asprintf(&cptr,AACENCOPT,AACENCCMD,wkfilename,tfptr);
			tclistpush2(cmdlist,cptr);
			free(cptr);
			tclistpush2(tflist,tfptr);
			free(tfptr);
		}
		//ファイル名-new.mp4ファイルを削除する
		asprintf(&cptr,"rm -f '%s-new.mp4'",wkfilename);
		tclistpush2(cmdlist,cptr);
		free(cptr);

		asprintf(&cptr2,"%s -quiet -noprog ",MP4BOXCMD);
		for(i=0;i<hcnt;i++) {
			asprintf(&cptr,"%s -cat '%s.%d.mp4' ",cptr2,wkfilename,i);
			free(cptr2);
			cptr2=cptr;
		}
		asprintf(&cptr,"%s '%s-new.mp4'",cptr2,wkfilename);
		tclistpush2(cmdlist,cptr);
		free(cptr);

		if (!noaudioencode) {
			asprintf(&cptr,"%s -quiet -noprog -add '%s.aac' '%s-new.mp4'",MP4BOXCMD,wkfilename,wkfilename);
			tclistpush2(cmdlist,cptr);
		}


		for(i=0;i<tclistnum(tflist);i++) {
			asprintf(&cptr,"rm -f '%s'",tclistval2(tflist,i));
			tclistpush2(cmdlist,cptr);
			free(cptr);
		}

		if (thumb) {
			pre=0;
			for(i=0;i<mcnt;i++) {
				asprintf(&cptr,"%s -ao null -ss %.2f -frames 1 -vo png:z=9  '%s' ; mv 00000001.png '%s-%d.png'",MPLAYERCMD,(pre + (m[i].stsec-pre)/2)/1000.0,wkfilename,wkfilename,i);
				tclistpush2(cmdlist,cptr);
				pre = m[i].stsec;
			}
		}
	}

	for (i=0;i<tclistnum(cmdlist);i++) {
		if (cmdexecute && wkfilename) {
			FILE *pp;
			char pbuf[1024];
			pp = popen(tclistval2(cmdlist,i),"r");
			if (pp==NULL) {continue;}
			while(fgets(pbuf,1024,pp)!=NULL){
			}
			pclose(pp);
		}
		else
			printf("%s\n",tclistval2(cmdlist,i));
	}

}

int rechecktext(FILE *f)
{
	char rbuf[1024];
	char fname[1024];
	char cm[100];
	char wk1[100];
	char wk2[100];
	char wk3[100];
	char wk4[100];
	int cnt,pgst,len;
	float in1,in2,in3;


	rewind(f);
	cnt=0;

	pgst=0;
	fname[0]=0x00;

	while(fgets(rbuf,1024,f)!=NULL){
		if (strstr(rbuf,"# cmcheckwave ")) {
			pgst=1;
			if (strlen(rbuf+14)-1>0) {
				wkfilename = malloc(strlen(rbuf+14));
				strncpy(wkfilename,rbuf+14,strlen(rbuf+14)-1);
			}
			fgets(rbuf,1024,f);
			continue;
		}
		if (strstr(rbuf,"# total "))
			pgst=0;
		if (pgst) {
			wk4[0]=0x00;
			sscanf(rbuf,"# %s %s diff %s %s",wk1,wk2,wk3,wk4);

			m[cnt].stsec = (int)(atof(wk1)*1000.0);
			m[cnt].edsec = (int)(atof(wk2)*1000.0);
			m[cnt].diffs = (int)(atof(wk3)*1000.0);
			if (strstr(wk4,"CM")) m[cnt].cmflg=1;
			else m[cnt].cmflg=0;
			m[cnt].honpen = 0;
			cnt++;
		}

	}
	if (checkcomplete > 0) {
		return cmpinfo(cnt);
	}
	txtrecheck=1;
	return dumpinfo(cnt);


}

int cmcheckwave(FILE *f)
{
	int i,j, x, channels, bits;
	unsigned long len;
	long count;
	unsigned char s[5];
	unsigned long bsec;
	int readed,loop,max,totalsec,muonstartsec,kankaku;
	double dul,diffs;
	char cm[10];
	int mcnt,hcnt,rcnt,readbufsz;
	unsigned char *readbuf;
	int honstart;
	int cmwork;
	int peak;

	if (memcmp(get_bytes(f, 4), "RIFF", 4) != 0) {
		//   fprintf(stderr, "Not a 'RIFF' format\n");
		return -1;
	}
	//fprintf(stderr, "[RIFF] (%lu bytes)\n", get_ulong(f));
	get_ulong(f);
	if (memcmp(get_bytes(f, 8), "WAVEfmt ", 8) != 0) {
		// fprintf(stderr, "Not a 'WAVEfmt ' format\n");
		return -1;
	}
	len = get_ulong(f);
	//fprintf(stderr, "[WAVEfmt ] (%lu bytes)\n", len);
	//fprintf(stderr, "  Data type = %u (1 = PCM)\n", get_ushort(f));
	get_ushort(f);
	channels = get_ushort(f);
	//fprintf(stderr, "  Number of channels = %u (1 = mono, 2 = stereo)\n", channels);
	//fprintf(stderr, "  Sampling rate = %luHz\n", get_ulong(f));
	get_ulong(f);
	bsec = get_ulong(f);
	//fprintf(stderr, "  Bytes / second = %lu\n", bsec);
	//fprintf(stderr, "  Bytes x channels = %u\n", get_ushort(f));
	get_ushort(f);
	bits = get_ushort(f);
	//fprintf(stderr, "  Bits / sample = %u\n", bits);
	for (i = 16; i < len; i++)
		fgetc(f);
	while (fread(s, 4, 1, f) == 1) {
		len = get_ulong(f);
		s[4] = 0;
		//fprintf(stderr, "[%s] (%lu bytes)\n", s, len);
		if (memcmp(s, "data", 4) == 0) break;
		for (i = 0; i < len; i++)
			fgetc(f);
	}

	readed=max=totalsec=kankaku=mcnt=0;
	peak=0;
	muonstartsec=-1;
	loop=1;
	readbuf=malloc(4096*1000);
	while(rcnt=fread(readbuf,1,4096*1000,f)) {
		for(readbufsz=0;readbufsz<rcnt;) {
			for (i = 0; i < channels; i++) {
				if (bits <= 8) {
					//if ((x = fgetc(f)) == EOF) {loop=0;break;}
					x = readbuf[readbufsz];
					readed++;
					readbufsz++;
					x -= 128;
				} else {
					//if (fread(s, 2, 1, f) != 1) {loop=0;break;}
					//x = (short)(s[0] + 256 * s[1]);
					x = (short)(readbuf[readbufsz+0] + 256 * readbuf[readbufsz+1]);
					readed+=2;
					readbufsz+=2;
				}
				if (x > max) max = x;
				if (x > peak) peak = x;
				//printf("%d", x);
				//if (i != channels - 1) printf("\t");
			}
			//printf("\n");
			if (readed % (bsec/100) == 0) {
				//無音開始
				if (muonstartsec==-1 && max < defmax) {
					muonstartsec = totalsec;
				}
				else {
					// 無音終わり
					if (muonstartsec>=0 && max >= defmax) {
						// 無音が300(defmuon)msより大きい
						if (totalsec - muonstartsec > defmuon){

							m[mcnt].stsec = muonstartsec;
							m[mcnt].edsec = totalsec;
							m[mcnt].diffs = totalsec - kankaku;
							m[mcnt].cmflg = 0;
							m[mcnt].honpen = 0;

							dul = (totalsec-muonstartsec)/1000.0;
							diffs = (totalsec-kankaku)/1000.0;

							if ((diffs >  14.5) && (diffs < 15.5)) m[mcnt].cmflg=1;
							if ((diffs >  29.5) && (diffs < 30.5)) m[mcnt].cmflg=1;
							if ((diffs >  59.5) && (diffs < 60.5)) m[mcnt].cmflg=1;

							kankaku=totalsec;
							m[mcnt].peak=peak;
							mcnt++;

							peak=0;
						}
						muonstartsec=-1;
					}
				}
				max=0;
				totalsec += 10;
			}
		}
	}
	if (mcnt > 1) {
		//本編前CMチェック
		if ((m[0].cmflg==0) && (m[0].diffs < 15000))
			m[0].cmflg=1;
		//細切れCMのたしこみ
		for(i=1;i<mcnt-1;i++) {
			// 本編で31秒以下が連続だったら、次の31秒以上の本編もしくはCMまでの時間をチェック
			// 足しこみは61秒まで
			// TODO 28+32で60秒CMとかいうのがあるどうするのがいいだろうか・・・
			if (m[i].cmflg==0 && m[i].diffs < 31000  && m[i+1].cmflg==0 && m[i+1].diffs < 31000) {
				cmwork=0;
				for(j=i;j<mcnt;j++) {
					if (m[j].cmflg==1) break;
					if (m[j].diffs > 31000) break;
					if (cmwork + m[j].diffs > 61000) break;
					cmwork = cmwork + m[j].diffs;
				}
				//合計時間を15秒で割ってcm時間っぽいならばCMとする。
				// TODO 30(15)秒以下の条件付けがいる？60秒どうする？
				if (cmwork%15000>14500 || cmwork%15000<500) {
					for(j=i;j<mcnt;j++) {
						if (m[j].cmflg==1) break;
						if (m[j].diffs > 31000) break;
						m[j].cmflg=1;
					}
				}
			}

		}
		// 最終CMチェック
		if ((m[mcnt-1].cmflg==0) && (m[mcnt-1].diffs < 15000))
			m[mcnt-1].cmflg=1;
		//短い本編・提供などの処理
		for(i=1;i<mcnt-1;i++) {
			//本編で46秒以下かつ、前後がCMの場合CM14-16,29-31,44-46秒でもCMとする。
			if (m[i].cmflg==0 && m[i].diffs < 46000 && m[i-1].cmflg==1 && m[i+1].cmflg==1) {
				if ((m[i].diffs > 14000) && (m[i].diffs < 16000)) m[i].cmflg=1;
				if ((m[i].diffs > 29000) && (m[i].diffs < 31000)) m[i].cmflg=1;
				if ((m[i].diffs > 44000) && (m[i].diffs < 46000)) m[i].cmflg=1;
				// 0.9秒以下(おそらく前後ＣＭのあまり時間)
				if (m[i].diffs < 900) m[i].cmflg=1;

				//10,5秒のときは提供とみなし、その前を本編にする。
				//TODO 15秒提供は判別不能・・・
				if ((m[i].diffs > 9500) && (m[i].diffs < 10500)) m[i-1].cmflg=0;
				if ((m[i].diffs > 4500) && (m[i].diffs < 5500)) m[i-1].cmflg=0;
			}
			// TODO 前後が本編で単独でCMの場合は本編とする?
			// 46秒以下のチェックも?
			// if (m[i].cmflg==1 && m[i-1].cmflg==0 && m[i+1].cmflg==0) {
			// 	m[i].cmflg=0;
			// }
		}

	}
	return dumpinfo(mcnt);
}

int main(int argc, char *argv[])
{
	extern char *optarg;
	extern int optind, opterr;
	int i,ch;
	FILE *f,*p;
	int ret;
	char *tmpenv,*argv0;
	ret = -1;

	argv0 = argv[0];
	while ((ch = getopt(argc, argv, "adtxb:m:v:c:")) != -1){
		switch (ch){
			case 'a':
				noaudioencode=1;
				break;
			case 'd':
				verbose=1;
				break;
			case 'b':
				wkfilename=optarg;
				break;
			case 'm':
				defmuon=atoi(optarg);
				break;
			case 'v':
				defmax=atoi(optarg);
				break;
			case 't':
				thumb=1;
				break;
			case 'x':
				cmdexecute=1;
				break;
			case 'c':
				checkcomplete=atoi(optarg);
				if (checkcomplete <= 0 && checkcomplete > 2) usage(argv0);
				break;
			default:
				usage(argv0);
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 1) {
		usage(argv0);
		return 0;
	}
	if (tmpenv=getenv("FFMPEG")) FFMPEGCMD=tmpenv;
	if (tmpenv=getenv("SOX")) SOXCMD=tmpenv;
	if (tmpenv=getenv("MP4BOX")) MP4BOXCMD=tmpenv;
	if (tmpenv=getenv("MP4BOXCMDRAPSTR")) MP4BOXCMDRAPSTR=tmpenv;
	if (tmpenv=getenv("AACENC")) AACENCCMD=tmpenv;
	if (tmpenv=getenv("AACENCPOT")) AACENCOPT=tmpenv;
	if (tmpenv=getenv("MPLAYER")) MPLAYERCMD=tmpenv;
	if (tmpenv=getenv("FAADCMD")) FAADCMD=tmpenv;

	ret=0;
	p=NULL;
	if (strcmp(argv[0],"-")==0)
		f = stdin;
	else {
		f = fopen(argv[0],"rb");
		if (f) {
			p = checkMP4(f,argv[0]);
		}
		else
			return -1;
	}

	if (p)  ret = cmcheckwave(p);
	else    ret = cmcheckwave(f);
	// -1 のときはテキストとして再チェック
	if (ret == -1) rechecktext(f);
	if (p) pclose(p);
	else fclose(f);

	return ret;
}




