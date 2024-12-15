function APconfig ()
{
  ssid = null
  bssid = null;
}

var curAP = new APconfig();
var selAP = new APconfig();
var fixedAP = false;
let xhr = new XMLHttpRequest();

let _keyStr = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=';
function utf8Encode (string)
{
  //base64编码
  string = string.replace(/\r\n/g, '\n');
  var utftext = '';
  for (var n = 0; n < string.length; n++)
  {
    var c = string.charCodeAt(n);
    if (c < 128)
    {
      utftext += String.fromCharCode(c);
    } else if (c > 127 && c < 2048)
    {
      utftext += String.fromCharCode((c >> 6) | 192);
      utftext += String.fromCharCode((c & 63) | 128);
    } else
    {
      utftext += String.fromCharCode((c >> 12) | 224);
      utftext += String.fromCharCode(((c >> 6) & 63) | 128);
      utftext += String.fromCharCode((c & 63) | 128);
    }
  }
  return utftext;
}
function utf8decode (utftext)
{
  //base64解密
  var string = "";
  var i = 0;
  var c = 0;
  var c2 = 0;
  var c3 = 0;
  while (i < utftext.length)
  {
    c = utftext.charCodeAt(i);
    if (c < 128)
    {
      string += String.fromCharCode(c);
      i++;
    } else if (c > 191 && c < 224)
    {
      c2 = utftext.charCodeAt(i + 1);
      string += String.fromCharCode(((c & 31) << 6) | (c2 & 63));
      i += 2;
    } else
    {
      c2 = utftext.charCodeAt(i + 1);
      c3 = utftext.charCodeAt(i + 2);
      string += String.fromCharCode(
        ((c & 15) << 12) | ((c2 & 63) << 6) | (c3 & 63)
      );
      i += 3;
    }
  }
  return string;
}
function baseEncode (input)
{
  //base64编码
  var output = '';
  var chr1, chr2, chr3, enc1, enc2, enc3, enc4;
  var i = 0;
  input = utf8Encode(input);
  while (i < input.length)
  {
    chr1 = input.charCodeAt(i++);
    chr2 = input.charCodeAt(i++);
    chr3 = input.charCodeAt(i++);
    enc1 = chr1 >> 2;
    enc2 = ((chr1 & 3) << 4) | (chr2 >> 4);
    enc3 = ((chr2 & 15) << 2) | (chr3 >> 6);
    enc4 = chr3 & 63;
    if (isNaN(chr2))
    {
      enc3 = enc4 = 64;
    } else if (isNaN(chr3))
    {
      enc4 = 64;
    }
    output = output + _keyStr.charAt(enc1) + _keyStr.charAt(enc2) + _keyStr.charAt(enc3) + _keyStr.charAt(enc4);
  }
  return output;
}
function baseDecode (input)
{
  //base64解密
  var output = "";
  var chr1, chr2, chr3;
  var enc1, enc2, enc3, enc4;
  var i = 0;
  input = input.replace(/[^A-Za-z0-9+\\=]/g, "");
  while (i < input.length)
  {
    enc1 = _keyStr.indexOf(input.charAt(i++));
    enc2 = _keyStr.indexOf(input.charAt(i++));
    enc3 = _keyStr.indexOf(input.charAt(i++));
    enc4 = _keyStr.indexOf(input.charAt(i++));
    chr1 = (enc1 << 2) | (enc2 >> 4);
    chr2 = ((enc2 & 15) << 4) | (enc3 >> 2);
    chr3 = ((enc3 & 3) << 6) | enc4;
    output = output + String.fromCharCode(chr1);
    if (enc3 != 64)
    {
      output = output + String.fromCharCode(chr2);
    }
    if (enc4 != 64)
    {
      output = output + String.fromCharCode(chr3);
    }
  }
  output = utf8decode(output);
  return output;
}
function updateSelAP (bssid, ssid)
{
  document.getElementById("sta_ssid").value = ssid;
  selAP.bssid = bssid;
  selAP.ssid = ssid;
  let f_ap = document.getElementById("ap_fixed");
  if (selAP.ssid === curAP.ssid && selAP.bssid !== curAP.bssid)
  {
    f_ap.checked = true;
  }
  else
  {
    f_ap.checked = false;
  }
  toggleFixedAP(f_ap);
}

function createRowForAp (row, ap, hasFocus)
{
  row.id = ap.bssid;

  var radio = document.createElement('td');
  radio.className = "radio_tick";
  radio.style.width = '22px';
  /*radio.id = "radio_tick";*/
  var input = document.createElement("input");
  input.style.width = '22px';
  input.type = "radio";
  input.name = "ssid";
  input.value = ap.ssid;
  input.addEventListener('change', function () { updateSelAP(ap.bssid, ap.ssid); });
  /*if(document.getElementById("sta_ssid").value == ap.ssid) input.checked = "1";*/
  if (hasFocus)
  {
    input.checked = "1";
  }
  input.id = "opt-" + ap.ssid;
  radio.appendChild(input);
  row.appendChild(radio);

  var rssi = document.createElement('td');
  rssi.className = "nopadding";
  rssi.style.width = '32px';

  var rssi_img = document.createElement('div');
  if (ap.rssi > -30)
    rssi_img.className = "signal-bars sizing-box good five-bars";
  else if (ap.rssi > -67)
    rssi_img.className = "signal-bars sizing-box good four-bars";
  else if (ap.rssi > -70)
    rssi_img.className = "signal-bars sizing-box ok three-bars";
  else if (ap.rssi > -80)
    rssi_img.className = "signal-bars sizing-box bad two-bars";
  else
    rssi_img.className = "signal-bars sizing-box bad one-bar";

  var bar1 = document.createElement('div');
  bar1.className = "first-bar bar";
  rssi_img.appendChild(bar1);
  var bar2 = document.createElement('div');
  bar2.className = "second-bar bar";
  rssi_img.appendChild(bar2);
  var bar3 = document.createElement('div');
  bar3.className = "third-bar bar";
  rssi_img.appendChild(bar3);
  var bar4 = document.createElement('div');
  bar4.className = "fourth-bar bar";
  rssi_img.appendChild(bar4);
  var bar5 = document.createElement('div');
  bar5.className = "fifth-bar bar";
  rssi_img.appendChild(bar5);

  /*
  rssi_img.className = "nopadding";
  rssi_img.style.width = '32px';
  var rssiVal = -Math.floor( ap.rssi / 51 ) * 32;
  rssi_img.className = "wifi_icon";
  rssi_img.style.backgroundPosition = "0px " + rssiVal + "px";
  rssi_img.style.height = '26px';
  */

  rssi.appendChild(rssi_img);
  row.appendChild(rssi);

  var rssi_val = document.createElement('td');
  rssi_val.className = "nopadding";
  rssi_val.style.width = '85px';
  rssi_val.appendChild(document.createTextNode(ap.rssi));
  row.appendChild(rssi_val);

  var channel = document.createElement('td');
  channel.className = "nopadding";
  channel.style.width = '85px';
  channel.appendChild(document.createTextNode(ap.chan));
  row.appendChild(channel);

  var name = document.createElement('td');
  name.className = "nopadding";
  name.appendChild(document.createTextNode(ap.ssid));
  row.appendChild(name);

  var enc = document.createElement('td');
  enc.className = "nopadding";
  enc.style.width = '125px';
  enc.appendChild(document.createTextNode(ap.enc));
  row.appendChild(enc);

  return row;
}
var scan = (function ()
{
  var o = {};
  var _ws = undefined;
  var _retries = 0;

  o.start = function ()
  {
    if ((_ws === undefined) || _ws.readyState != WebSocket.OPEN)
    {
      if (_retries)
      {
        setMsg("error", "WebSocket timeout, retrying..");
      }
      else
      {
        setMsg("info", "Opening WebSocket..");
      }

      let uri = "/websocket/apscan";
      _ws = new WebSocket("ws://" + location.host + uri);
      _ws.binaryType = 'arraybuffer';
      _ws.onopen = function (evt)
      {
        _retries = 0;
        setMsg("done", "WebSocket opened.");
      };
      _ws.onerror = function (evt)
      {
        if (_ws.readyState == WebSocket.OPEN)
        {
          setMsg("error", "WebSocket error!");
        }
      };
      _ws.onclose = function (evt)
      {
        console.log("websocket closed.");
        setMsg("done", "WebSocket closed.");
      };
      _ws.onmessage = function (evt)
      {
        if (evt.data)
        {
          var apList = JSON.parse(evt.data);

          /* Configure our connected AP */
          if (apList.bssid)
          {
            /* Update the details of the AP we are connected to, if necessary */
            if (curAP.bssid != apList.bssid)
            {
              curAP.bssid = apList.bssid;
              curAP.ssid = apList.ssid
            }
          }

          /* This will be null only during our first run, so we need to set it */
          /* I guess this could be also true if there were no available wireless networks */
          if (selAP.ssid == null && curAP.ssid != null) /* deliberate equivalence and not identical */
          {
            selAP.bssid = curAP.bssid;
            selAP.ssid = curAP.ssid;
          }
          var focusAP = selAP.bssid;

          /* Check for when the SSID text does not match the active/selected connection */
          if (document.getElementById("sta_ssid").value != selAP.ssid)
          {
            /* We are no longer focused on a specific BSSID/SSID */
            focusAP = null;
            selAP.bssid = null;
          }

          if (apList.APs && apList.APs.length > 0)
          {
            let ntbdy = document.createElement('tbody');
            let l = apList.APs.length;
            for (var i = 0; i < l; i++)
            {
              let nr = ntbdy.insertRow();
              createRowForAp(nr, apList.APs[i], (apList.APs[i].bssid === focusAP));
            }
            let otbdy = document.querySelector("#apList > tbody");
            otbdy.parentNode.replaceChild(ntbdy, otbdy);
          }
        }
      };
      _retries = 0;
      return true;
    }
    else
    {
      return false;
    }
  };
  o.running = function ()
  {
    if ((_ws === undefined) || _ws.readyState != WebSocket.OPEN)
    {
      return false;
    }
    else
    {
      return true;
    }
  };
  o.stop = function ()
  {
    if ((_ws === undefined) || _ws.readyState != WebSocket.OPEN)
    {
      return false;
    }
    _ws.close();
    return true;
  };
  return o;
})();
function toggleScan (_this)
{
  if (scan.running() === true)
  {
    scan.stop();
    _this.className = "scan_start";
    _this.value = "Scan Start";
  }
  else
  {
    scan.start();
    _this.className = "scan_stop";
    _this.value = "Scan Stop";
  }
}
var handleError = function (err)
{
  console.warn(err);
  return new Response(JSON.stringify({
    code: 400,
    message: 'Stupid network Error'
  }));
};
async function staAwaitResults (el)
{
  let pwd_el = document.getElementById("sta_password");
  let sid_el = document.getElementById("sta_ssid");

  openBusyMesg("Waiting...");

  const json = {
    command: "2",
    sta_ssid: sid_el.value,
    sta_password: baseEncode(pwd_el.value),
    sta_bssid: (fixedAP ? selAP.bssid : "")
  };
  // request options
  const options = {
    method: 'POST',
    body: JSON.stringify(json),
    headers: {
      'Content-Type': 'application/json'
    }
  };
  const url = '/wifi/cfgsta';
  const response = await fetch(url, options).catch(handleError);
  const data = await response.json();
  if (data.noerr === true)
  {
    if (data.tip === 0)
    {
      let sav_el = document.getElementById("sta_save");
      if (data.test_status === 1)
      {
        sav_el.disabled = false;
      }
      else
      {
        sav_el.disabled = true;
      }
      closeBusyMesg();      
      return data;
    }
  }
  // If not timeout, keep trying
  await sleep(500);
  await staAwaitResults(el);
}
async function staTestConfig (el)
{
  let pwd_el = document.getElementById("sta_password");
  let sid_el = document.getElementById("sta_ssid");

  openBusyMesg("Testing...");

  const json = {
    command: "1",
    sta_ssid: sid_el.value,
    sta_password: baseEncode(pwd_el.value),
    sta_bssid: (fixedAP ? selAP.bssid : "")
  };
  // request options
  const options = {
    method: 'POST',
    body: JSON.stringify(json),
    headers: {
      'Content-Type': 'application/json'
    }
  };
  const url = '/wifi/cfgsta';
  const response = await fetch(url, options).catch(handleError);
  const data = await response.json();
  if (data.noerr === true)
  {
    await sleep(1500);
    await staAwaitResults(el);
  }
  else
  {
    closeBusyMesg();
  }
}
async function staSaveConfig (el)
{
  let pwd_el = document.getElementById("sta_password");
  let sid_el = document.getElementById("sta_ssid");

  openBusyMesg("Saving...");

  const json = {
    command: "3",
    sta_ssid: sid_el.value,
    sta_password: baseEncode(pwd_el.value),
    sta_bssid: (fixedAP ? selAP.bssid : "")
  };
  // request options
  const options = {
    method: 'POST',
    body: JSON.stringify(json),
    headers: {
      'Content-Type': 'application/json'
    }
  };
  const url = '/wifi/cfgsta';
  const response = await fetch(url, options).catch(handleError);
  const data = await response.json();
  if (data.noerr === true)
  {
    await sleep(1500);
    await staAwaitResults(el);
  }
  closeBusyMesg();
}
function pwdShowHide (_this)
{
  let parent = _this.parentNode;
  var pwd_el = null;
  for (var i = 0; i < parent.childNodes.length; i++)
  {
    let tmp_el = parent.childNodes[i];
    if ((tmp_el.tagName != undefined) && tmp_el.tagName.toLowerCase() === 'input')
    {
      if (tmp_el.classList.contains('password'))
      {
        pwd_el = tmp_el;
        break;
      }
    }
  }

  if (pwd_el == null)
  {
    return;
  }

  if (_this.classList.contains('icon-yincang'))
  {
    _this.classList.remove('icon-yincang');
    _this.classList.add('icon-xianshimima');
    pwd_el.type = 'text';
  } else
  {
    _this.classList.add('icon-yincang');
    _this.classList.remove('icon-xianshimima');
    pwd_el.type = 'password';
  }
}
function staTestEnable()
{
  let pwd_el = document.getElementById("sta_password");
  let sid_el = document.getElementById("sta_ssid");
  let con_el = document.getElementById("sta_test");

  if (sid_el.value.length > 0 && (pwd_el.value.length === 0 || pwd_el.value.length >= 8))
  {
    con_el.disabled = false;
    return true;
  }
  con_el.disabled = true;
  return false;
}
function validatePassword (el)
{
  // Fuck WPS, but not open?
  if (el.value.length === 0 || el.value.length >= 8)
  {
    el.classList.remove('error');
  }
  else
  {
    el.classList.add('error');
  }
  staTestEnable();
}
function validateSSID (el)
{
  if (el.value.length > 0)
  {
    selAP.ssid = el.value;
    if (el.value === curAP.ssid)
    {
      /* If the ssid matches our current connected AP, we will use its bssid */
      selAP.bssid = curAP.bssid;
    }
    else
    {
      selAP.bssid = null; /* we moved away from a selected AP */
    }
  }
  staTestEnable();
}
function toggleFixedAP (el)
{
  fixedAP = el.checked;
}
function page_onload ()
{
  set_background();
  init_all_dropboxex();
  staTestEnable();
}