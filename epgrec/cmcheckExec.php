<?php
include_once('config.php');
include_once( INSTALL_PATH . "/DBRecord.class.php" );
include_once( INSTALL_PATH . "/Settings.class.php" );
include_once( INSTALL_PATH . "/Cmcheck.class.php" );

$settings = Settings::factory();

if( ! isset( $_POST['reserve_id'] )) {
	exit("Error:idが指定されていません。");
}
$reserve_id = $_POST['reserve_id'];
$cmcheckcomplete = $_POST['cmcheckcomplete'];
$replace = $_POST['replace'];
$newcmcheckval = $_POST['checkval'];
$newcmfixval = $_POST['fixval'];
$cmdvol = $_POST['optvol'];
$cmddif = $_POST['optdif'];


try{
	        $cm = new Cmcheck( $reserve_id );

		//処理判定(再実行・CM判定場所変更・CMチェック完了)
		if ($cmcheckcomplete == 1) {
			if ($replace == "true")
				$replace = 2; //録画ファイル置き換えを行わない
			else
			 	$replace = 1;
			
			$cmdbuf=sprintf("/usr/local/bin/cmcheckwave -t -c %d -x %s\n",$replace,escapeshellarg($cm->getfilename()));
			system($cmdbuf);
		}


		if ($newcmcheckval) {
			if (!$cm->getbasefile()) {
				exit( "Error:"."ファイルがありません");
			}
			if ($cm->putdata($newcmcheckval,$newcmfixval)==1) {
				$cmdbuf=sprintf("/usr/local/bin/cmcheckwave -t -x %s\n",escapeshellarg($cm->getfilename()));
				system($cmdbuf);
			}
			else
				exit("Error:putdatafailed");
		}
		if (($cmdvol>0) && ($cmddif>0)) {
				$cmdbuf=sprintf("/usr/local/bin/cmcheckwave -v %s -m %s -t -x %s > %s\n",escapeshellarg($cmdvol),escapeshellarg($cmddif),escapeshellarg($cm->getrecfilename()),escapeshellarg($cm->getfilename()));
				var_dump($cmdbuf);
				system($cmdbuf);
		}

}
catch( Exception $e ) {
	exit( "Error:".$e->getMessage() );
}
exit( "".$reserve_id );
?>
