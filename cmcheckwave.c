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
	fprintf(stderr,"Check CM from mp4file(require ffmpeg or faad cmd)\n");
	fprintf(stderr,"%s: filename.mp4\n\n",cmd);
	fprintf(stderr,"Check CM from mp4file and execute cutcmd(create new filename.mp4-new.mp4) \n");
	fprintf(stderr,"%s: -x filename.mp4\n\n",cmd);
	fprintf(stderr,"Adjust audio track delay of filename.mp4-new.mp4 from stream metadata\n");
	fprintf(stderr,"%s: -S filename.mp4\n\n",cmd);
	fprintf(stderr,"Check CM and manual edit\n");
	fprintf(stderr,"%s: -b filename.mp4 filename.mp4 > filename-sh\n",cmd);
	fprintf(stderr,"Edit filename-sh for mis detection and re execute next cmd\n");
	fprintf(stderr,"%s: -x filename-sh\n",cmd);
	exit(1);
}

unsigned char *get_bytes(FILE *f, int n)
{
	static unsigned char s[16];

	assert (n <= (int)sizeof s);
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
	int  audiostsec;
	int  audioedsec;
	char cmflg;
	char honpen;
	int  peak;
	int  stsecfix;
	int  edsecfix;
	char *fixparam;
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
static int basets=0;
static int syncadjust=0;
static int ptsdetect=0;
static double mp4_audio_start=0.0;
static double mp4_video_start=0.0;
static char *SELFEXEC=NULL;

int dumpinfo(int mcnt);

static char *MP4BOXCMDRAPSTR="Adjusting chunk start time to previous random access at ";
#ifdef __FreeBSD__
static char *MP4BOXCMD="/usr/local/bin/MP4Box";
static char *SOXCMD="/usr/local/bin/sox";
static char *FFMPEGCMD="/usr/local/bin/ffmpeg";
static char *FFPROBECMD="/usr/local/bin/ffprobe";
static char *MPLAYERCMD="/usr/local/bin/mplayer";
static char *AACENCCMD="/usr/local/bin/aacplusenc";
static char *AACENCOPT="%s '%s.wav' '%s' 60";
static char *FAADCMD="/usr/local/bin/faad";
static char *FIXASS="/usr/home/piro/bin/fixass";
#else
static char *MP4BOXCMD="MP4Box";
static char *SOXCMD="sox";
static char *FFMPEGCMD="ffmpeg";
static char *FFPROBECMD="ffprobe";
static char *MPLAYERCMD="mplayer";
static char *AACENCCMD="neroAacEnc";
static char *AACENCOPT="%s -br 60 -if '%s.wav' -of '%s'";
static char *FAADCMD=NULL;
static char *FIXASS="fixass";
#endif

static int round_msec(double sec)
{
	if (sec >= 0) return (int)(sec * 1000.0 + 0.5);
	return (int)(sec * 1000.0 - 0.5);
}

static char *shellquote(char *str)
{
	char *buf,*p;
	int i,len;

	len=3;
	for(i=0;str[i];i++) {
		if (str[i]=='\'') len+=4;
		else len++;
	}
	buf=malloc(len);
	p=buf;
	*p++='\'';
	for(i=0;str[i];i++) {
		if (str[i]=='\'') {
			memcpy(p,"'\\''",4);
			p+=4;
		}
		else *p++=str[i];
	}
	*p++='\'';
	*p=0x00;
	return buf;
}

static int read_stream_info(char *filename,char *streamtype,double *starttime,int *trackid)
{
	FILE *pp;
	char *cmd,*qfilename;
	char pbuf[1024];
	int found_start,found_track;

	qfilename = shellquote(filename);
	asprintf(&cmd,"%s -v error -select_streams %s:0 -show_entries stream=id,start_time -of default=noprint_wrappers=1 %s",
	    FFPROBECMD,streamtype,qfilename);
	free(qfilename);

	pp = popen(cmd,"r");
	free(cmd);
	if (pp == NULL) return -1;

	found_start=found_track=0;
	while(fgets(pbuf,sizeof(pbuf),pp)!=NULL){
		if (strncmp(pbuf,"id=",3)==0) {
			*trackid = (int)strtol(pbuf+3,NULL,0);
			found_track=1;
		}
		else if (strncmp(pbuf,"start_time=",11)==0) {
			if (strncmp(pbuf+11,"N/A",3)!=0) {
				*starttime = atof(pbuf+11);
				found_start=1;
			}
		}
	}
	pclose(pp);

	if (!found_track || !found_start) return -1;
	return 0;
}

static int sync_adjust_mp4(char *filename)
{
	char *newfilename,*qnewfilename,*cmd;
	double org_v_start,org_a_start,new_v_start,new_a_start;
	double org_offset,new_offset,target_audio_start;
	int org_v_track,org_a_track,new_v_track,new_a_track;
	int target_delay_ms,current_delay_ms;
	int ret;

	asprintf(&newfilename,"%s-new.mp4",filename);
	if (read_stream_info(filename,"v",&org_v_start,&org_v_track) != 0) {
		fprintf(stderr,"sync adjust: cannot read source video stream: %s\n",filename);
		free(newfilename);
		return 1;
	}
	if (read_stream_info(filename,"a",&org_a_start,&org_a_track) != 0) {
		fprintf(stderr,"sync adjust: cannot read source audio stream: %s\n",filename);
		free(newfilename);
		return 1;
	}
	if (read_stream_info(newfilename,"v",&new_v_start,&new_v_track) != 0) {
		fprintf(stderr,"sync adjust: cannot read output video stream: %s\n",newfilename);
		free(newfilename);
		return 1;
	}
	if (read_stream_info(newfilename,"a",&new_a_start,&new_a_track) != 0) {
		fprintf(stderr,"sync adjust: cannot read output audio stream: %s\n",newfilename);
		free(newfilename);
		return 1;
	}

	org_offset = org_a_start - org_v_start;
	new_offset = new_a_start - new_v_start;
	target_audio_start = new_v_start + org_offset;
	target_delay_ms = round_msec(target_audio_start);
	current_delay_ms = round_msec(new_a_start);

	fprintf(stderr,
	    "sync adjust: original av offset %.3f sec, output av offset %.3f sec, target audio start %.3f sec\n",
	    org_offset,new_offset,target_audio_start);

	if (target_delay_ms == current_delay_ms) {
		fprintf(stderr,"sync adjust: audio delay already matches target: %d ms\n",target_delay_ms);
		free(newfilename);
		return 0;
	}

	qnewfilename = shellquote(newfilename);
	asprintf(&cmd,"%s -quiet -noprog -delay %d=%d %s",
	    MP4BOXCMD,new_a_track,target_delay_ms,qnewfilename);
	free(qnewfilename);

	ret = system(cmd);
	free(cmd);
	if (ret != 0) {
		fprintf(stderr,"sync adjust: MP4Box delay failed: %s\n",newfilename);
		free(newfilename);
		return 1;
	}
	fprintf(stderr,"sync adjust: set audio track %d delay to %d ms\n",new_a_track,target_delay_ms);
	free(newfilename);
	return 0;
}

FILE *checkMP4(FILE *f,char *filename)
{
	char readbuf[20];
	char *cmdbuf;
	FILE *pp;
	char *tmpts;

	memset(readbuf,0,sizeof(readbuf));
	fread(readbuf,sizeof(readbuf),1,f);
	rewind(f);
	if (!strstr(readbuf+4,"ftypisom")) {
		return NULL;
	}
	// mp4ファイルだったら、ffmpegでwaveに変換し読み込む。
	if (FAADCMD) {
		// filename.tmp.ts is exist ?
		asprintf(&tmpts,"%s.tmp.ts",filename);
		if (basets && (pp=fopen(tmpts,"r") )) {
			fclose(pp);
			pp=NULL;
		asprintf(&cmdbuf,"%s -d -w -F 0x3330D -q '%s'",FAADCMD,tmpts);
		}
		else {
	    asprintf(&cmdbuf,"%s -d -w -q '%s'",FAADCMD,filename);
}
		
		free(tmpts);
	}
    else
	    asprintf(&cmdbuf,"%s -v 0 -i '%s' -f wav pipe: 2>/dev/null",FFMPEGCMD,filename);

	pp = popen(cmdbuf,"r");
	if (pp == NULL) return NULL;
	fclose(f);
	wkfilename = strdup(filename);
	return pp;
}

static int is_mp4_file(FILE *f)
{
	char readbuf[20];

	memset(readbuf,0,sizeof(readbuf));
	fread(readbuf,sizeof(readbuf),1,f);
	rewind(f);
	return strstr(readbuf+4,"ftypisom") != NULL;
}

static void classify_cm_marks(int mcnt)
{
	int i,j;
	int cmwork;

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
}

int cmcheckmp4pts(char *filename)
{
	FILE *pp;
	char *cmd,*qfilename;
	char pbuf[1024];
	double silence_start,silence_end,silence_duration,noise;
	int have_start,mcnt,kankaku;
	int atrack,vtrack;
	int status;

	memset(m,0,sizeof(m));
	memset(h,0,sizeof(h));

	mp4_audio_start=0.0;
	mp4_video_start=0.0;
	if (read_stream_info(filename,"a",&mp4_audio_start,&atrack) != 0) {
		fprintf(stderr,"pts detect: cannot read audio stream metadata: %s\n",filename);
		return -1;
	}
	if (read_stream_info(filename,"v",&mp4_video_start,&vtrack) != 0) {
		fprintf(stderr,"pts detect: cannot read video stream metadata: %s\n",filename);
		return -1;
	}

	noise = defmax / 32768.0;
	if (noise <= 0.0) noise = 1.0 / 32768.0;
	qfilename = shellquote(filename);
	asprintf(&cmd,"%s -hide_banner -nostats -i %s -map 0:a:0 -af asetpts=PTS-STARTPTS,silencedetect=noise=%.10f:d=%.3f -f null - 2>&1",
	    FFMPEGCMD,qfilename,noise,defmuon/1000.0);
	free(qfilename);

	pp = popen(cmd,"r");
	free(cmd);
	if (pp == NULL) return -1;

	have_start=0;
	mcnt=0;
	kankaku=round_msec(mp4_audio_start);
	while(fgets(pbuf,sizeof(pbuf),pp)!=NULL){
		char *p;

		p = strstr(pbuf,"silence_start:");
		if (p) {
			silence_start = atof(p + strlen("silence_start:"));
			have_start=1;
			continue;
		}

		p = strstr(pbuf,"silence_end:");
		if (p && have_start && mcnt < (int)(sizeof(m)/sizeof(m[0]))) {
			silence_end = 0.0;
			silence_duration = 0.0;
			sscanf(p,"silence_end: %lf | silence_duration: %lf",&silence_end,&silence_duration);
			if (silence_duration * 1000.0 >= defmuon) {
				m[mcnt].audiostsec = round_msec(silence_start);
				m[mcnt].audioedsec = round_msec(silence_end);
				m[mcnt].stsec = round_msec(mp4_audio_start + silence_start);
				m[mcnt].edsec = round_msec(mp4_audio_start + silence_end);
				m[mcnt].diffs = m[mcnt].edsec - kankaku;
				m[mcnt].cmflg = 0;
				m[mcnt].honpen = 0;

				if ((m[mcnt].diffs > 14500) && (m[mcnt].diffs < 15500)) m[mcnt].cmflg=1;
				if ((m[mcnt].diffs > 29500) && (m[mcnt].diffs < 30500)) m[mcnt].cmflg=1;
				if ((m[mcnt].diffs > 59500) && (m[mcnt].diffs < 60500)) m[mcnt].cmflg=1;

				kankaku=m[mcnt].edsec;
				mcnt++;
			}
			have_start=0;
		}
	}
	status = pclose(pp);
	if (status != 0) return -1;

	ptsdetect=1;
	classify_cm_marks(mcnt);
	return dumpinfo(mcnt);
}

int checkMP4RAP(int stsec,int edsec)
{
	FILE *pp;
	char pbuf[1024];
	char cmdbuf[1024];
	float rap;

	if (wkfilename==NULL) return stsec;

	sprintf(cmdbuf,"%s -quiet -noprog -splitx %.2f:%.2f '%s' -out /dev/null 2>&1",MP4BOXCMD,edsec/1000.0,(edsec+10000)/1000.0,wkfilename);

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
		asprintf(&cmdptr,"mv '%s.fix.ass' '%s.ass'",wkfilename,wkfilename);
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

	asprintf(&cmdptr,"rm -f '%s.fix.ass'",wkfilename);
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

	return 0;
}
int dumpinfo(int mcnt)
{
	int honstart,hcnt,totalsec;
	int i,pre;
	char *cptr,*cptr2,*tfptr;
	char *qptr,*qptr2;
	TCLIST *cmdlist;
	TCLIST *tflist;
	FILE *fp;

	honstart=0;
	hcnt=0;
	totalsec=0;
	printf("#!/bin/sh\n# cmcheckwave %s\n#\n",wkfilename?wkfilename:"");
	//カットするため、一連のCM,本編時間を結合
	for(i=0;i<mcnt;i++) {
		printf("# %.2f %.2f diff %.2f ",m[i].stsec/1000.0,m[i].edsec/1000.0,m[i].diffs/1000.0);
		if (m[i].cmflg==1) printf("CM\n");
		else printf("%s\n",m[i].fixparam?m[i].fixparam:"");

		//本編開始位置をマーク
		if ((m[i].cmflg==0)&&(honstart==0)) {
			honstart=1;
			if (i==0) h[hcnt].stsec = 0;
			else h[hcnt].stsec = m[i-1].edsec+(defmuon*0.5) + m[i].stsecfix;
		}
		else {
			//終了位置をマーク
			if ((m[i].cmflg==1)&&(honstart==1)) {
				honstart=0;
				//h[hcnt].edsec = m[i-1].stsec;
				h[hcnt].edsec = checkMP4RAP(m[i-1].stsec,m[i-1].edsec) + m[i-1].edsecfix;
				totalsec += h[hcnt].edsec - h[hcnt].stsec;
				hcnt++;
			}
		}
	}
	if (honstart==1) {
		h[hcnt].edsec = checkMP4RAP(m[i-1].stsec,m[i-1].edsec) + m[i-1].edsecfix;
		totalsec += h[hcnt].edsec - h[hcnt].stsec;
		hcnt++;
	}

	printf("# total %.2f\n\n",totalsec/1000.0);
	if (ptsdetect) {
		printf("# pts audio_start %.3f video_start %.3f offset %.3f\n",
		    mp4_audio_start,mp4_video_start,mp4_audio_start-mp4_video_start);
		for(i=0;i<mcnt;i++) {
			printf("# pts %d audio %.3f %.3f mp4 %.3f %.3f video_rel %.3f %.3f\n",
			    i,
			    m[i].audiostsec/1000.0,m[i].audioedsec/1000.0,
			    m[i].stsec/1000.0,m[i].edsec/1000.0,
			    m[i].stsec/1000.0-mp4_video_start,
			    m[i].edsec/1000.0-mp4_video_start);
		}
		printf("\n");
	}

	cmdlist = tclistnew();
	tflist = tclistnew();
	if (wkfilename) {
		asprintf(&cptr,"rm -f '%s.split.log'",wkfilename);
		tclistpush2(cmdlist,cptr);
		free(cptr);
	}
	for(i=0;i<hcnt;i++) {
		if (wkfilename) {
			asprintf(&tfptr,"%s.%d%s",wkfilename,i,".mp4");
			asprintf(&cptr,"%s -quiet -noprog -splitx %.2f:%.2f '%s' -out '%s' >> '%s.split.log' 2>&1",MP4BOXCMD,h[i].stsec/1000.0,h[i].edsec/1000.0,wkfilename,tfptr,wkfilename);
			tclistpush2(cmdlist,cptr);
			tclistpush2(tflist,tfptr);
			free(tfptr);
			free(cptr);
		}
		else {
			asprintf(&cptr,"# %s -quiet -noprog -splitx %.2f:%.2f ",MP4BOXCMD,h[i].stsec/1000.0,h[i].edsec/1000.0);
			tclistpush2(cmdlist,cptr);
		}
	}
	if (wkfilename) {
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
		if (noaudioencode) {
			asprintf(&cptr,"%s '%s-new.mp4'",cptr2,wkfilename);
			tclistpush2(cmdlist,cptr);
			free(cptr);
		}
		else {
			asprintf(&tfptr,"%s.%d.mp4",wkfilename,i);
			asprintf(&cptr,"%s '%s'",cptr2,tfptr);
			tclistpush2(cmdlist,cptr);
			tclistpush2(tflist,tfptr);
			free(cptr);
			free(tfptr);

			asprintf(&tfptr,"%s.%d.wav",wkfilename,i);

			if (FAADCMD)
				asprintf(&cptr,"%s -d -q -o '%s' '%s.%d.mp4' ",FAADCMD,tfptr,wkfilename,i);
			else
				asprintf(&cptr,"%s -v 0 -i '%s.%d.mp4' -vn '%s'",FFMPEGCMD,wkfilename,i,tfptr);
			tclistpush2(cmdlist,cptr);
			tclistpush2(tflist,tfptr);
			free(tfptr);
			free(cptr);

			asprintf(&tfptr,"%s.%d.mp4",wkfilename,i);
			asprintf(&cptr,"%s -v 0 -i '%s.%d.mp4' -an -vcodec copy '%s-new.mp4'",FFMPEGCMD,wkfilename,i,wkfilename);
			tclistpush2(cmdlist,cptr);
			tclistpush2(tflist,tfptr);
			free(tfptr);
			free(cptr);

			asprintf(&cptr2,"%s --norm '%s.%d.wav'",SOXCMD,wkfilename,i);
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

			asprintf(&cptr,"%s -quiet -noprog -add '%s.aac' '%s-new.mp4'",MP4BOXCMD,wkfilename,wkfilename);
			tclistpush2(cmdlist,cptr);
			free(cptr);

		}

		qptr = shellquote(SELFEXEC?SELFEXEC:"cmcheckwave");
		qptr2 = shellquote(wkfilename);
		asprintf(&cptr,"%s -S %s",qptr,qptr2);
		free(qptr);
		free(qptr2);
		tclistpush2(cmdlist,cptr);
		free(cptr);

		/* wkfilename.mp4.assファイルがあったらfixassを実施  */
		asprintf(&cptr,"%s.ass",wkfilename);
		fp = fopen(cptr,"r");
		if (fp) {
			free(cptr);
			fclose(fp);
			asprintf(&cptr,"%s '%s' > '%s.fix.ass'",FIXASS,wkfilename,wkfilename);
			tclistpush2(cmdlist,cptr);
			free(cptr);
		}
		else {
			free(cptr);
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

	return 0;
}

int rechecktext(FILE *f)
{
	char rbuf[1024];
	char wk1[100];
	char wk2[100];
	char wk3[100];
	char wk4[100];
	int cnt,pgst;


	rewind(f);
	cnt=0;

	pgst=0;

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
			else {
				m[cnt].cmflg=0;
				/* CMカット位置の調整パラメータ
					-0.1, (開始位置を-0.1秒ずらす、終了位置はそのまま)
					,7    (開始位置そのまま、終了位置は7フレームずらす)
					-0.5,10 (開始位置を-0.5秒、終了位置を10フレームずらす)
					開始位置パラメータが有効なのはCMの直後
					終了位置パラメータが有効なのは次がCMの場合のみとする。
				*/
				if (strlen(wk4)>0) {
					m[cnt].fixparam = strdup(wk4);
					TCLIST *t = tcstrsplit(wk4,",");
					if (tclistnum(t)==2) {
						m[cnt].edsecfix = atoi(tclistval2(t,1))*33;
					}
					if (tclistnum(t)>=1) {
						m[cnt].stsecfix = (int)(atof(tclistval2(t,0))*1000.0);
					}
				}
			}
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
	int i, x, channels, bits;
	unsigned long len;
	unsigned char s[5];
	unsigned long bsec;
	int readed,max,totalsec,muonstartsec,kankaku;
	double diffs;
	int mcnt,rcnt,readbufsz;
	unsigned char *readbuf;
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
	for (i = 16; (unsigned long)i < len; i++)
		fgetc(f);
	while (fread(s, 4, 1, f) == 1) {
		len = get_ulong(f);
		s[4] = 0;
		//fprintf(stderr, "[%s] (%lu bytes)\n", s, len);
		if (memcmp(s, "data", 4) == 0) break;
		for (i = 0; (unsigned long)i < len; i++)
			fgetc(f);
	}

	readed=max=totalsec=kankaku=mcnt=0;
	memset(m,0,sizeof(m));
	memset(h,0,sizeof(h));
	peak=0;
	muonstartsec=-1;
	readbuf=malloc(4096*1000);
	while((rcnt=fread(readbuf,1,4096*1000,f))) {
		for(readbufsz=0;readbufsz<rcnt;) {
			for (i = 0; i < channels; i++) {
				if (bits <= 8) {
					//if ((x = fgetc(f)) == EOF) {break;}
					x = readbuf[readbufsz];
					readed++;
					readbufsz++;
					x -= 128;
				} else {
					//if (fread(s, 2, 1, f) != 1) {break;}
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
	classify_cm_marks(mcnt);
	return dumpinfo(mcnt);
}

int main(int argc, char *argv[])
{
	extern char *optarg;
	extern int optind;
	int ch;
	FILE *f,*p;
	int ret;
	char *tmpenv,*argv0;
	ret = -1;

	argv0 = argv[0];
	SELFEXEC = argv[0];
	while ((ch = getopt(argc, argv, "Sagdtxb:m:v:c:")) != -1){
		switch (ch){
			case 'S':
				syncadjust=1;
				break;
			case 'a':
				noaudioencode=1;
				break;
			case 'g':
				basets=1;
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
				if (checkcomplete <= 0 || checkcomplete > 2) usage(argv0);
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
	if ((tmpenv=getenv("FFMPEG"))) FFMPEGCMD=tmpenv;
	if ((tmpenv=getenv("FFPROBE"))) FFPROBECMD=tmpenv;
	if ((tmpenv=getenv("SOX"))) SOXCMD=tmpenv;
	if ((tmpenv=getenv("MP4BOX"))) MP4BOXCMD=tmpenv;
	if ((tmpenv=getenv("MP4BOXCMDRAPSTR"))) MP4BOXCMDRAPSTR=tmpenv;
	if ((tmpenv=getenv("AACENC"))) AACENCCMD=tmpenv;
	if ((tmpenv=getenv("AACENCPOT"))) AACENCOPT=tmpenv;
	if ((tmpenv=getenv("MPLAYER"))) MPLAYERCMD=tmpenv;
	if ((tmpenv=getenv("FAADCMD"))) FAADCMD=tmpenv;
#ifdef DEBUG
	if ((FAADCMD) && (tmpenv=getenv("FORCEFFMPEGCMD"))) FAADCMD = NULL;
#endif
	if (syncadjust) return sync_adjust_mp4(argv[0]);

	ret=0;
	p=NULL;
	if (strcmp(argv[0],"-")==0)
		f = stdin;
	else {
		f = fopen(argv[0],"rb");
		if (f) {
			if (is_mp4_file(f)) {
				wkfilename = strdup(argv[0]);
				ret = cmcheckmp4pts(argv[0]);
				if (ret == 0) {
					fclose(f);
					return ret;
				}
				ptsdetect=0;
			}
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
