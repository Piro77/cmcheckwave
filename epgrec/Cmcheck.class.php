<?php
include_once('config.php');
include_once( INSTALL_PATH . "/DBRecord.class.php" );
include_once( INSTALL_PATH . "/reclib.php" );
include_once( INSTALL_PATH . "/Settings.class.php" );
include_once( INSTALL_PATH . "/recLog.inc.php" );

// Cmcheckクラス

class Cmcheck {
protected $filename;
protected $recfilename;
protected $basefilename;
protected $settings;
protected $lines;
protected $cmdata;
protected $rrec;
protected $totalline;

protected $dbh;
public $id;

function __construct( $reserve_id = null ) {
	$settings = Settings::factory();
	$this->settings = Settings::factory();

	try {
		$this->rrec = new DBRecord( RESERVE_TBL, "id", $reserve_id );
	}
	catch( Exception $e ) {
		throw $e;
	}

	$this->filename = INSTALL_PATH .$this->settings->spool . "/". $this->rrec->path . "-sh";
	$this->recfilename = INSTALL_PATH .$this->settings->spool . "/". $this->rrec->path;


	if( file_exists( $this->filename )) {
		$this->cmdata = array();
		$itmcnt=0;
		$cmfileflg=0;
		$lines=file($this->filename );

                foreach ($lines as $line_num => $line) {
                        if (strstr($line,"# cmcheckwave")) {
                                $cmfileflg=1;
                                $itm = Explode(" ",$line,3);
                                $this->basefilename = trim($itm[2]);
                                continue;
                        }
                        if ($cmfileflg>1) {
                                if (strstr($line,"# total")) {
					$this->totalline = trim($line);
                                        break;
                                }
                                $itm = explode(" ",$line);
                                $ary = array();
                                $ary['id']=$itmcnt;
				$ary['start']=$itm[1];
                                $ary['end']=$itm[2];
                                $ary['diff']=$itm[4];
                                $ary['cm']=trim($itm[5]);
				$ary['thumb']=$settings->install_url.$settings->spool. "/".$this->rrec->path."-".$itmcnt.".png";
                                array_push($this->cmdata,$ary);
				$itmcnt++;
			}
			if ($cmfileflg==1) $cmfileflg++; //skip blank line

		}

	}
	return;
}
function getlines() {
	return $this->lines;
}
function getdata() {
	return $this->cmdata;
}
function getbasefile() {
	return $this->basefilename;
}
function getrecfilename() {
	return $this->recfilename;
}
function getfilename() {
	return $this->filename;
}
function putdata($newcm)
{
	$newary = explode(",",$newcm);

	if (count($this->cmdata)!=count($newary)) return(0);

	FILE *fp;
	$fp = fopen($this->filename ,"w");
	if (fp==NULL) return(0);
	fprintf($fp,"#!/bin/sh\n");
	fprintf($fp,"# cmcheckwave %s\n#\n",$this->basefilename);

	$cnt=0;
	foreach($this->cmdata as $data) {
		if ($newary[$cnt]=="1")
			$cmval = "CM";
		else
			$cmval = "";
		fprintf($fp,"# %s %s diff %s %s\n",
			$data['start'],$data['end'],$data['diff'],$cmval);
		$cnt++;
	}
	fprintf($fp,"%s\n",$this->totalline);
	fclose($fp);

	return(1);
}

}

?>
