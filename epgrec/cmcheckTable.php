<?php
include_once('config.php');
include_once( INSTALL_PATH . '/Smarty/Smarty.class.php' );
include_once( INSTALL_PATH . "/DBRecord.class.php" );
include_once( INSTALL_PATH . "/reclib.php" );
include_once( INSTALL_PATH . "/Reservation.class.php" );
include_once( INSTALL_PATH . "/Keyword.class.php" );
include_once( INSTALL_PATH . "/Cmcheck.class.php" );



include_once(INSTALL_PATH . "/Settings.class.php" );

$settings = Settings::factory();

if( ! isset( $_GET['reserve_id'] )) jdialog("予約番号が指定されていません", "recordedTable.php");
$reserve_id = $_GET['reserve_id'];

$cmary=array();
try{

	$rrec = new DBRecord( RESERVE_TBL, "id", $reserve_id );

	$start_time = toTimestamp($rrec->starttime);
	$end_time = toTimestamp($rrec->endtime );
	$duration = $end_time - $start_time + $settings->former_time;

	$dh = $duration / 3600;
	$duration = $duration % 3600;
	$dm = $duration / 60;
	$duration = $duration % 60;
	$ds = $duration;
	
	$title = htmlspecialchars(str_replace(array("\r\n","\r","\n"), '', $rrec->title),ENT_QUOTES);
	$abstract = htmlspecialchars(str_replace(array("\r\n","\r","\n"), '', $rrec->description),ENT_QUOTES);
	
	$cm = new Cmcheck( $reserve_id );

	$recfile = $cm->getrecfilename();
	if (file_exists($recfile)) {
		$st=stat($recfile);
		$mp4info=sprintf("(%.2f MB %s)",$st['size']/1024/1024,date("Y/m/d H:i:s",$st['mtime']));
	}
	$recfile = $recfile . "-new.mp4";
	if (file_exists($recfile)) {
		$st=stat($recfile);
		$mp4info2=sprintf("(%.2f MB %s)",$st['size']/1024/1024,date("Y/m/d H:i:s",$st['mtime']));
	}

}
catch(exception $e ) {
	exit( $e->getMessage() );
}

$smarty = new Smarty();

$smarty->assign( "cmary", $cm->getdata() );
$smarty->assign( "fileurl", $cm->getfileurl() );
$smarty->assign( "sitetitle", $title );
$smarty->assign( "reserveid", $reserve_id );
$smarty->assign( 'mp4info' ,  $mp4info);
$smarty->assign( 'mp4info2' , $mp4info2);
$smarty->assign( 'cmcheckbox'  , array('CM' => 'CM')); 

$smarty->display( "cmcheckTable.html" );
?>
