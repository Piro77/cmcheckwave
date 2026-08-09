#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "tclist.h"

static double delay;

#ifdef __FreeBSD__
static char *FFPROBECMD="/usr/local/bin/ffprobe";
#else
static char *FFPROBECMD="ffprobe";
#endif

typedef struct {
  double start;
  double end;
}KEEPRANGE;

typedef struct {
  double totalcut;
  double nextstart;
  double cutstart; //カット開始位置
  double cutend;   //カット終了位置
}CUTTM;

void usage(char *argv0)
{
  printf("Usage:%s [-d delay] [-r start:end ...] filename.mp4\n",argv0);
  printf("	-r reads actual durations from filename.mp4.N.mp4 using ffprobe\n");
  printf("	Without -r, read CutInfo from filename.mp4.split.log\n");
  printf("	Read AssFile from filename.mp4.ass\n");
  printf("	OutputFixed ASS to stdout\n");
  printf("	option -d delay(delay sec)\n\n");
  exit(1);
}

static char *shellquote(const char *str)
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

static int parse_range(const char *str,KEEPRANGE *range)
{
  char *endptr;

  range->start=strtod(str,&endptr);
  if (endptr==str || *endptr!=':') return -1;
  range->end=strtod(endptr+1,&endptr);
  if (*endptr!='\0' || range->start < 0 || range->end <= range->start) return -1;
  return 0;
}

/* Prefer the longest media stream duration over the rounded container value. */
static int probe_duration(const char *filename,double *duration)
{
  FILE *pp;
  char *cmd,*qfilename;
  char rbuf[1024];
  double stream_duration,format_duration,value;
  long long duration_ts;
  long timebase_num,timebase_den;
  int status;

  qfilename=shellquote(filename);
  asprintf(&cmd,"%s -v error -show_entries stream=codec_type,duration_ts,time_base,duration:format=duration -of compact=p=0:nk=0 %s 2>/dev/null",
      FFPROBECMD,qfilename);
  free(qfilename);
  pp=popen(cmd,"r");
  free(cmd);
  if (pp==NULL) return -1;

  stream_duration=0.0;
  format_duration=0.0;
  while(fgets(rbuf,sizeof(rbuf),pp)!=NULL) {
    char *valueptr=strstr(rbuf,"duration=");
    char *typeptr=strstr(rbuf,"codec_type=");
    if (typeptr && (strncmp(typeptr+11,"video",5)==0 || strncmp(typeptr+11,"audio",5)==0)) {
      char *tsptr=strstr(rbuf,"duration_ts=");
      char *tbptr=strstr(rbuf,"time_base=");
      value=0.0;
      if (tsptr && strncmp(tsptr+12,"N/A",3)!=0 && tbptr &&
          sscanf(tbptr+10,"%ld/%ld",&timebase_num,&timebase_den)==2 && timebase_den!=0) {
        duration_ts=strtoll(tsptr+12,NULL,10);
        value=(double)duration_ts*timebase_num/timebase_den;
      }
      else if (valueptr && strncmp(valueptr+9,"N/A",3)!=0) {
        value=strtod(valueptr+9,NULL);
      }
      if (value > stream_duration) stream_duration=value;
    }
    else if (!typeptr) {
      if (!valueptr || strncmp(valueptr+9,"N/A",3)==0) continue;
      value=strtod(valueptr+9,NULL);
      if (value > format_duration) format_duration=value;
    }
  }
  status=pclose(pp);
  if (status!=0) return -1;
  if (stream_duration > 0.0) *duration=stream_duration;
  else if (format_duration > 0.0) *duration=format_duration;
  else return -1;
  return 0;
}

static TCLIST *readranges(const char *basename,TCLIST *ranges)
{
  TCLIST *cutlist;
  CUTTM cut;
  KEEPRANGE *range;
  double duration,actualstart,nextstart,totalcut;
  int i,sp;
  char *chunkname;

  cutlist=tclistnew();
  nextstart=totalcut=0.0;
  for(i=0;i<tclistnum(ranges);i++) {
    range=(KEEPRANGE *)tclistval(ranges,i,&sp);
    asprintf(&chunkname,"%s.%d.mp4",basename,i);
    if (probe_duration(chunkname,&duration)!=0) {
      fprintf(stderr,"fixass: cannot read split duration: %s\n",chunkname);
      free(chunkname);
      return NULL;
    }
    free(chunkname);

    /* MP4Box -splitx keeps the requested end and moves only the start RAP. */
    actualstart=range->end-duration;
    if (actualstart < 0.0 && actualstart > -0.1) actualstart=0.0;
    if (actualstart < 0.0 || actualstart > range->end) {
      fprintf(stderr,"fixass: invalid split duration %.6f for range %.3f:%.3f\n",
          duration,range->start,range->end);
      return NULL;
    }

    cut.cutstart=nextstart;
    cut.cutend=actualstart;
    totalcut += actualstart-nextstart;
    nextstart=actualstart+duration;
    cut.totalcut=totalcut;
    cut.nextstart=nextstart;
    tclistpush(cutlist,&cut,sizeof(CUTTM));
  }
  return cutlist;
}

char *getfixtimestr(double asstime,TCLIST *cutlist)
{
  int i;
  CUTTM *cuttm;
  double wktime;
  int h,m;
  char *fixedstr;

  cuttm = NULL;
  for(i=0;i<tclistnum(cutlist);i++) {
    int sp;
    cuttm = (CUTTM *)tclistval(cutlist,i,&sp);
    //カット範囲の字幕は捨てる
    if (asstime >= cuttm->cutstart && asstime <= cuttm->cutend) return NULL;
    if (asstime < cuttm->nextstart) break;
  }
  if (cuttm) wktime = asstime - cuttm->totalcut;
  else wktime = asstime;

  if (wktime<0) return NULL;
  h = wktime / 3600;
  m = (wktime - h*3600) / 60;
  wktime = wktime - (h*3600+m*60);
  if (wktime < 10)
    asprintf(&fixedstr,"%d:%02d:0%.2f",h,m,wktime);
  else
    asprintf(&fixedstr,"%d:%02d:%.2f",h,m,wktime);
//fprintf(stderr,"ass %.2f fixed %.2f %s\n",asstime,wktime,fixedstr);
  return fixedstr;
}
char * fixtime(char *timestr,TCLIST *cutlist)
{
  TCLIST *timelist;
  timelist = tcstrsplit(timestr,":"); /* H:MM:SS.XX */
  double asstime,wktime;
  char *fixedtime;
  asstime=0;
  if (tclistnum(timelist)==3) {
    asstime += atoi(tclistval2(timelist,0))*3600;
    asstime += atoi(tclistval2(timelist,1))*60;
    wktime = strtod(tclistval2(timelist,2),NULL);
    asstime += wktime;
    asstime += delay;
  }
  fixedtime = getfixtimestr(asstime,cutlist);
  if (fixedtime) return fixedtime;
  else           return NULL;
}
void cutass(char *assfile,TCLIST *cutlist)
{
  
  FILE *fp;
  char rbuf[2048];
  const char *p;
  TCLIST *splist;
  int i,skip;


  fp = fopen(assfile,"r");

  if (fp == NULL) return;
  while(fgets(rbuf,1024,fp)!=NULL) {
     skip = 0;
     if (strncmp(rbuf,"Dialogue",8)==0) { // sub
        splist = tcstrsplit(rbuf,",");
        for(i=1;i<tclistnum(splist)-1;i++) {
           p = tclistval2(splist,i);
           if ( i==1 ) {
		     p = fixtime((char *)p,cutlist);
             if (p == NULL) { /* */
                 skip = 1;
                 break;
             }
             else {
                 printf("%s,",tclistval2(splist,0));
             }
           }
           if (i==2) p = fixtime((char *)p,cutlist);
           printf("%s,",p);
        }
        if (skip == 0) printf("%s",tclistval2(splist,i));
     }
     else
       printf("%s",rbuf);
  }
  fclose(fp);
}
/*
 *   MP4のsplit.logからカット位置とカット時間を算出
 */
TCLIST *readlog(char *logfile)
{
  FILE *fp;
  TCLIST *cutlist;
  char rbuf[1024];
  int  chkflg,listnum,lcnt;
  char *p;
  double nextstart,totalcut,sttime,duration;
  CUTTM cut,*cuttm;
  
  
  cutlist = tclistnew();
  fp = fopen(logfile,"r");
  if (fp == NULL) return cutlist;
  chkflg=lcnt=0;
  nextstart=totalcut=0.0;
  while(fgets(rbuf,1024,fp)!=NULL) {
    if (lcnt==0) {
      // chk start 0 sec
      p=strstr(rbuf," duration ");
      if (p) {chkflg=1;sttime=0.0;}
    }
    if (!chkflg) {
      // find Adjust Start pos
      p = strstr(rbuf,"previous random access at ");
      if (p) {
        sttime = strtod(p+strlen("previous random access at "),NULL);
        chkflg=1;
        lcnt++;
        continue;
      }
    }
    else {
      // find cut end pos
      p=strstr(rbuf," duration ");
      if (p) {
        duration = strtod(p+strlen(" duration "),NULL); // 本編の時間
        chkflg=0;
        listnum=tclistnum(cutlist);
        if (listnum==0)  {
        cut.cutstart=totalcut;
        cut.cutend = sttime;
           }
        else {
            int sp;
            cuttm = (CUTTM *)tclistval(cutlist,listnum-1,&sp);
            cut.cutstart=cuttm->nextstart;
        cut.cutend = sttime;
        }
	totalcut = totalcut + (sttime - nextstart);
	nextstart = sttime+duration;              //  カット開始位置+本編時間=次のカット開始時間
        /* asprintf(&p,"%.2f,%.2f",nextstart,totalcut); */
        cut.totalcut = totalcut;
        cut.nextstart = nextstart;
	tclistpush(cutlist,&cut,sizeof(CUTTM));
        lcnt++;
        continue;
      }
    }
  }
  fclose(fp);
  return cutlist;
}
int main(int argc,char *argv[])
{
        extern char *optarg;
        extern int optind;
        int ch;
        char *argv0;
        char *assfile,*mp4boxsplitlog;
	TCLIST *cutlist,*ranges;
	KEEPRANGE range;
	char *tmpenv;

	delay = 0;
	ranges = tclistnew();
        argv0 = argv[0];
        while ((ch = getopt(argc, argv, "ad:r:")) != -1){
            switch (ch){
              case 'a':
                break;
              case 'd':
                delay=atof(optarg);
                break;
              case 'r':
                if (parse_range(optarg,&range)!=0) {
                  fprintf(stderr,"fixass: invalid range: %s\n",optarg);
                  return 1;
                }
                tclistpush(ranges,&range,sizeof(KEEPRANGE));
                break;
              default:
                usage(argv0);
            }
        }
        argc -= optind;
        argv += optind;

        if (argc != 1) {
            usage(argv0);
            return 1;
        }
	if ((tmpenv=getenv("FFPROBE"))) FFPROBECMD=tmpenv;

        asprintf(&assfile,"%s.ass",argv[0]);
        asprintf(&mp4boxsplitlog,"%s.split.log",argv[0]);

	if (tclistnum(ranges)>0)
	  cutlist = readranges(argv[0],ranges);
	else
	  cutlist = readlog(mp4boxsplitlog);
	if (cutlist==NULL) return 1;
        cutass(assfile,cutlist);

	exit(0);
}
