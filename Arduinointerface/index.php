<?php

//*** Setting up variables ***//
$r_cmd = $_GET['cmd'];
$html_head = '<!DOCTYPE html><html lang="en"><head><meta charset=utf-8></head><body>';
$html_footer = '</body></html>';
//****************************//


//***  Generating payload  ***//
if ($r_cmd == 'status')
{
	$returned_data = getPassthru('echo "currentsong" | nc localhost 6600 | grep -e "^Title: " -e "^Name: "');

	$returned_data = str_replace("Title: ","",$returned_data);
	$returned_data = str_replace("Name: ","",$returned_data);
	$returned_data = trim($returned_data);
	$returned_data = str_replace(array("\r\n", "\r", "\n"), "<|>", $returned_data);
	$returned_data = cleanForLcd($returned_data);
	
	/* Limit for Arduino UNO RAM
	$blocks = explode("<|>", $returned_data);	
	if (strlen($blocks[0]) > 25)
			$blocks[0] = substr($blocks[0], 0,25);
	if (strlen($blocks[1]) > 16)
		$blocks[1] = substr($blocks[1], 0,16);		
	$returned_data = implode("<|>", $blocks);
	*/

	$index = strpos($returned_data, "<|>");
	if ($index === false)
	{
		$returned_data .= "<|>";
	}
	$body = "<0>".getPlaystate()."<|||>".getPlaylistLength()."<||>".$returned_data;
}
else if ($r_cmd == 'play')
{
	//2-times play, because first start sometimes lags
	shell_exec('mpc play');
	sleep(1);
	shell_exec('mpc stop');
	sleep(2);
	shell_exec('mpc play');
	shell_exec('mpc play');
	$body = "<2>".getPlaystate()."<|>".getPlaylistLength();
}
else if ($r_cmd == 'stop')
{
	shell_exec('mpc stop');
	$body = "<2>".getPlaystate()."<|>".getPlaylistLength();
}
else if ($r_cmd == 'playstate')
{
	shell_exec('mpc play');
	sleep(1);
	$body = "<2>".getPlaystate()."<|>".getPlaylistLength();
}
else if ($r_cmd == 'pllength')
{
	$body = "<3>".getPlaylistLength();
}
else if ($r_cmd == 'playstation')
{
	$cmd = 'mpc play '.$_GET['i'];
	shell_exec($cmd);	
	$body = "<2>".getPlaystate()."<|>".getPlaylistLength();
}
else if ($r_cmd == 'addradio')
{
	$pstate = getPlaystate();
	shell_exec('mpc add '.$_GET['i']);
	shell_exec('mpc play '.getPlaylistLength());
	shell_exec('mpc stop '.getPlaylistLength());
	shell_exec('mpc play '.getPlaylistLength());
	sleep(2);
	shell_exec('mpc '.$pstate);
	$body = "<5>Radion added successful to playlist.";
}
else if ($r_cmd == 'plitem')
{
	$returned_data = getPassthru('mpc playlist');
	
	$stations = explode("\n", $returned_data);
	for ($i = 0; $i < count($stations); $i++) {
		$index = strpos($stations[$i], ":");
		if ($index !== false)
		{
			$stations[$i] = substr($stations[$i], 0, $index);
		}
		$stations[$i] = $returned_data = cleanForLcd($stations[$i]);
		if ((strlen($stations[$i]) + strlen(($i+1)) > 11))
		{
			$stations[$i] = substr($stations[$i], 0, (11- strlen(($i+1))));
		}
		else
		{
			while ((strlen($stations[$i]) + strlen(($i+1)) < 11))
				$stations[$i] .= ' ';
		}		
		//echo "station: ".$stations[$i]."<br/>";
	}
	//$returned_data = implode("\n", $stations);
	$plnum = $_GET['i'];
	$body = "<4>".$plnum.":".$stations[(intval($plnum)-1)]."";
}
else if ($r_cmd == 'autoplaystop')
{
	if (!strncmp(getPlaystate(), "play", 4))
	{
		shell_exec('mpc stop');
	}
	else
	{
		shell_exec('mpc play');
	}
	$body = "<2>".getPlaystate()."<|>".getPlaylistLength();
}
else if ($r_cmd == 'ipinfo')
{
	$body = '<1>'.getPassthru('ifconfig wlan0 | grep inet | cut -d: -f2 | cut -d" " -f1');
}
else
{	
	$body = 'testIO';
}
//****************************//


//***    Posting output    ***//
$output = $body;

//Converting output from UTF8->ISO for Arduino
$output = html_entity_decode(htmlentities($output, ENT_QUOTES, 'UTF-8'), ENT_QUOTES , 'ISO-8859-15');

header('Expires: 0');   // always already expired
header('Pragma: no-cache');   // HTTP/1.0
header('Cache-Control: private, no-store, no-cache, must-revalidate, max-age=0');   // HTTP/1.1

echo $output;
die;
//****************************//


//***    Function block    ***//
function getPlaystate()
{
	return getPassthru('echo "status" | nc localhost 6600 | grep -e "^state: " | cut -d" " -f2');
}

function getPlaylistLength()
{
	return getPassthru('echo "status" | nc localhost 6600 | grep -e "^playlistlength: " | cut -d" " -f 2');
}

function getPassthru($cmd)
{
	ob_start();
	passthru($cmd);
	$data = ob_get_contents();
	ob_clean();
	return trim($data);
}

function cleanForLcd($returned_data)
{
	$returned_data = preg_replace('/[^a-zA-Z0-9_ %\[\]\.\(\)%&-|äöüÄÖÜ]/s', '', $returned_data);
	$returned_data = str_replace("ß", "ss", $returned_data);
	$returned_data = str_replace("ö", "oe", $returned_data);
	$returned_data = str_replace("ä", "ae", $returned_data);
	$returned_data = str_replace("ü", "ue", $returned_data);
	$returned_data = str_replace("Ö", "OE", $returned_data);
	$returned_data = str_replace("Ä", "AE", $returned_data);
	$returned_data = str_replace("Ü", "OE", $returned_data);
	$returned_data = str_replace("  ", " ", $returned_data);
	return $returned_data;
}
//****************************//

?>
