#include <stdio.h>
#include <string.h>

#include "tclist.h"

static double delay;

typedef struct {
  double totalcut;
  double nextstart;
  double cutstart; //カット開始位置
  double cutend;   //カット終了位置
}CUTTM;

void usage(char *argv0)
{
  printf("Usage:%s -d delay filename.mp4\n",argv0);
  printf("	Read CutInfo from filename.mp4.split.log\n");
  printf("	Read AssFile from filename.mp4.ass\n");
  printf("	OutputFixed ASS to stdout\n");
  printf("	option -d delay(delay sec)\n\n");
  exit;
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
    cuttm = tclistval2(cutlist,i);
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
  char rbuf[2048],*p;
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
		     p = fixtime(p,cutlist);
             if (p == NULL) { /* */
                 skip = 1;
                 break;
             }
             else {
                 printf("%s,",tclistval2(splist,0));
             }
           }
           if (i==2) p = fixtime(p,cutlist);
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
            cuttm = tclistval2(cutlist,listnum-1);
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
main(int argc,char *argv[])
{
        extern char *optarg;
        extern int optind, opterr;
        int i,ch;
        FILE *f,*p;
        int ret;
        char *tmpenv,*argv0;
        ret = -1;
        char *assfile,*mp4boxsplitlog;
	TCLIST *cutlist;

	delay = 0;
        argv0 = argv[0];
        while ((ch = getopt(argc, argv, "ad:")) != -1){
            switch (ch){
              case 'a':
                break;
              case 'd':
                delay=atof(optarg);
		printf("delay %f\n",delay);
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

        asprintf(&assfile,"%s.ass",argv[0]);
        asprintf(&mp4boxsplitlog,"%s.split.log",argv[0]);

	cutlist = readlog(mp4boxsplitlog);
  for(i=0;i<tclistnum(cutlist);i++) {
      CUTTM *cuttm;
    cuttm = tclistval2(cutlist,i);
    //fprintf(stderr,"%d total %f next %f start %f end %f\n",i,cuttm->totalcut,cuttm->nextstart,cuttm->cutstart,cuttm->cutend);
  }

        cutass(assfile,cutlist);

	exit(0);
}
