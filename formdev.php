<?php
//For developing and testing the HTML served by network-wifinina.cpp
//Example server behavior implemented in PHP, for development purposes
$timeoutmils = '5000'; //'120000';
$curwssid = "Riley";
$curwpass = "5802301644";
$curwki = "";
$syncstate = '[sync state]';
$version = '0.0.0';
if($_SERVER['REQUEST_METHOD'] == 'POST'){
	sleep(1);
	//if(isset($_POST['wssid'])) echo '['.$_POST['wssid'].'/'.$_POST['wpass'].'/'.$_POST['wki'].']';
	exit();
}
//Actual page source strings need to have no line breaks, '"' replaced with '/"', data fields replaced, and target of POST changed to './' (as opposed to './formdev.php' to use the above server, or the clock IP for the running clock server, for testing purposes)
?>
<!DOCTYPE html><html><head><title>Clock Settings</title><style>body { background-color: #eee; color: #222; font-family: system-ui, -apple-system, sans-serif; font-size: 18px; margin: 1.5em; position: absolute; } a { color: #33a; } ul { padding-left: 9em; text-indent: -9em; list-style: none; } ul li { margin-bottom: 0.8em; } ul li * { text-indent: 0; padding: 0; } ul li label:first-child { display: inline-block; width: 8em; text-align: right; padding-right: 1em; font-weight: bold; } ul li.nolabel { margin-left: 9em; } input[type='text'],input[type='submit'],select { border: 1px solid #999; margin: 0.2em 0; padding: 0.1em 0.3em; font-size: 1em; font-family: system-ui, -apple-system, sans-serif; } @media only screen and (max-width: 550px) { ul { padding-left: 0; text-indent: 0; } ul li label:first-child { display: block; width: auto; text-align: left; padding: 0; } ul li.nolabel { margin-left: 0; }} .saving { color: #66d; } .ok { color: #3a3; } .error { color: #c53; } @media (prefers-color-scheme: dark) { body { background-color: #222; color: #ddd; } a { color: white; } #result { background-color: #373; color: white; } input[type='text'],select { background-color: #444; color: #ddd; } }</style><meta charset='utf-8'><meta name='viewport' content='width=device-width, initial-scale=1'></head><body><h2 style='margin-top: 0;'>Clock Settings</h2><div id='content'><ul>

<li><label>Wi-Fi</label><form id='wform' style='display: inline;' onsubmit='save(this); return false;'><select id='wtype' onchange='wformchg()'><option value=''>None</option><option value='wpa'>WPA</option><option value='wep'>WEP</option></select><span id='wa'><br/><input type='text' id='wssid' name='wssid' placeholder='SSID (Network Name)' autocomplete='off' onchange='wformchg()' onkeyup='wformchg()' value='<?php echo $curwssid; ?>' /><br/><input type='text' id='wpass' name='wpass' placeholder='Password/Key' autocomplete='off' onchange='wformchg()' onkeyup='wformchg()' value='<?php echo $curwpass; ?>' /></span><span id='wb'><br/><input type='text' id='wki' name='wki' placeholder='Key Index' autocomplete='off' onchange='wformchg()' onkeyup='wformchg()' value='<?php echo $curwki; ?>' /></span><br/><input id='wformsubmit' type='submit' value='Save' style='display: none;' /></form></li>

<li><label>Last sync</label>As of page load time: <?php echo $syncstate; ?></li>

<li><label>Sync frequency</label><select id='syncfreq' onchange='save(this)'><option value='min'>Every minute</option><option value='hr'>Every hour (at min :59)</option></select></li>

<li><label>NTP packets</label><select id='ntpok' onchange='save(this)'><option value='y'>Yes (normal)</option><option value='n'>No (for dev/testing)</option></select></li>

<li><label>Brightness</label><select id='bright' onchange='save(this)'><option value='3'>High</option><option value='2'>Medium</option><option value='1'>Low</option></select></li>

<li><label>Version</label><?php echo $version; ?> (<a href='https://github.com/clockspot/arduino-ledclock/tree/v<?php echo $version; ?>' target='_new'>details</a>)</li>

</ul></div><script type='text/javascript'>function e(id){ return document.getElementById(id); } function save(ctrl){ if(ctrl.disabled) return; ctrl.disabled = true; let ind = ctrl.nextSibling; if(ind && ind.tagName==='SPAN') ind.parentNode.removeChild(ind); ind = document.createElement('span'); ind.innerHTML = '&nbsp;<span class="saving">Saving&hellip;</span>'; ctrl.parentNode.insertBefore(ind,ctrl.nextSibling); let xhr = new XMLHttpRequest(); xhr.onreadystatechange = function(){ if(xhr.readyState==4){ ctrl.disabled = false; if(xhr.status==200 && !xhr.responseText){ if(ctrl.id=='wform'){ e('content').innerHTML = '<p class="ok">Wi-Fi changes applied.</p><p>' + (e('wssid').value? 'Now attempting to connect to <strong>'+e('wssid').value+'</strong>.</p><p>If successful, the clock will display its IP address. To access this settings page again, connect to <strong>'+e('wssid').value+'</strong> and visit that IP address.</p><p>If not successful, the clock will display <strong>7777</strong>. ': '') + 'To access this settings page again, connect to Wi-Fi network <strong>Clock</strong> and visit <a href="http://7.7.7.7">7.7.7.7</a>.</p>'; } else { ind.innerHTML = '&nbsp;<span class="ok">OK!</span>'; setTimeout(function(){ if(ind.parentNode) ind.parentNode.removeChild(ind); },1500); } } else ind.innerHTML = '&nbsp;<span class="error">'+xhr.responseText+'</span>'; timer = setTimeout(timedOut, <?php echo $timeoutmils; ?>); } }; xhr.open('POST', './formdev.php', true); xhr.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded'); if(ctrl.id=='wform'){ switch(e('wtype').value){ case '': e('wssid').value = ''; e('wpass').value = ''; case 'wpa': e('wki').value = ''; case 'wep': default: break; } xhr.send('wssid='+e('wssid').value+'&wpass='+e('wpass').value+'&wki='+e('wki').value); } else { xhr.send(ctrl.id+'='+ctrl.value); } } function wformchg(initial){ if(initial) e('wtype').value = (e('wssid').value? (e('wki').value? 'wep': 'wpa'): ''); e('wa').style.display = (e('wtype').value==''?'none':'inline'); e('wb').style.display = (e('wtype').value=='wep'?'inline':'none'); if(!initial) e('wformsubmit').style.display = 'inline'; } function timedOut(){ e('content').innerHTML = 'Clock settings page has timed out. Please hold Select for 5 seconds to reactivate it, then <a href=\"./\">refresh</a>.'; } wformchg(true); let timer = setTimeout(timedOut, <?php echo $timeoutmils; ?>);</script></body></html>


<!--
<li class='nolabel'><a href='#' data-action='hi.html'>Just a test</a></li>

1.2.3 (<a href='https://github.com/clockspot/arduino-ledclock/tree/v1.2.3' target='_new'>details</a>)
-->

