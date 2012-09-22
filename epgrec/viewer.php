<?php
header("Expires: Thu, 01 Dec 1994 16:00:00 GMT");
header("Last-Modified: ". gmdate("D, d M Y H:i:s"). " GMT");
header("Cache-Control: no-cache, must-revalidate");
header("Cache-Control: post-check=0, pre-check=0", false);
header("Pragma: no-cache");


include_once("config.php");
include_once(INSTALL_PATH . "/DBRecord.class.php" );
include_once(INSTALL_PATH . "/reclib.php" );
include_once(INSTALL_PATH . "/Settings.class.php" );

$settings = Settings::factory();

if( ! isset( $_GET['reserve_id'] )) jdialog("予約番号が指定されていません", "recordedTable.php");
$reserve_id = $_GET['reserve_id'];
$cmcheckext = $_GET['cmcheckext'];
$fixass = ".ass";
if ($cmcheckext) {
	$cmcheckext="-new.mp4";
	$fixass=".fix.ass";
}


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
	$pathex  = $rrec->path . $cmcheckext;
	$assfile  = $rrec->path . $fixass;
	$asschkfile = "./".$settings->spool."/".$rrec->path.$fixass;
	
	$title = htmlspecialchars(str_replace(array("\r\n","\r","\n"), '', $rrec->title),ENT_QUOTES);
	$abstract = htmlspecialchars(str_replace(array("\r\n","\r","\n"), '', $rrec->description),ENT_QUOTES);

	$location = $settings->install_url.$settings->spool."/".$pathex;
	$asslocation = $settings->install_url.$settings->spool."/".$assfile;
	
	header("Content-type: application/xspf+xml");
	header('Content-Disposition: inline; filename="'.$pathex.'.xspf"');
echo <<< EOF
<?xml version="1.0" encoding="UTF-8"?>
<playlist version="1" xmlns="http://xspf.org/ns/0/" xmlns:vlc="http://www.videolan.org/vlc/playlist/ns/0/">
	<title>playlist</title>
	<trackList>
		<track>
			<title>$title</title>
			<location>$location</location>
			<duration>$duration</duration>
EOF;
if (file_exists($asschkfile)) {
echo <<< EOF
			<extension application="http://www.videolan.org/vlc/playlist/0">
				<vlc:id>0</vlc:id>
				<vlc:option>sub-file=$asslocation</vlc:option>
			</extension>
EOF;
}
echo <<< EOF
		</track>
	</trackList>
	<extension application="http://www.videolan.org/vlc/playlist/0">
			<vlc:item tid="0" />
	</extension>
</playlist>
EOF;
}
catch(exception $e ) {
	exit( $e->getMessage() );
}
?>
