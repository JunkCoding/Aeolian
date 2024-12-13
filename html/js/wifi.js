function APconfig ()
{
  ssid = null
  bssid = null;
}

var curAP = new APconfig();
var selAP = new APconfig();
var fixedAP = false;
let xhr = new XMLHttpRequest();

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

  openBusyMesg("Testing...");

  const json = {
    test_result: "matching",
    sta_ssid: sid_el.value,
    sta_bssid: (fixedAP ? selAP.bssid : ""),
    sta_password: pwd_el.value
  };
  // request options
  const options = {
    method: 'POST',
    body: JSON.stringify(json),
    headers: {
      'Content-Type': 'application/json'
    }
  };
  const url = '/wifi/teststa';
  const response = await fetch(url, options).catch(handleError);
  const data = await response.json();
  if (data.noerr === true)
  {
    if (data.tip === 0)
    {
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
    sta_ssid: sid_el.value,
    sta_bssid: (fixedAP ? selAP.bssid : ""),
    sta_password: pwd_el.value
  };
  // request options
  const options = {
    method: 'POST',
    body: JSON.stringify(json),
    headers: {
      'Content-Type': 'application/json'
    }
  };
  const url = '/wifi/teststa';
  const response = await fetch(url, options).catch(handleError);
  const data = await response.json();
  if (data.noerr === true)
  {
    await sleep(1500);
    await staAwaitResults(el);
    console.log("here");
  }
  else
  {
    closeBusyMesg();
  }
}
async function staSaveConfig ()
{
  try
  {
    const response = await fetch('<https://example.com/api/data>');
    if (!response.ok)
    {
      throw new Error('Network response was not ok');
    }
    const data = await response.json();
    console.log(data);
  } catch (error)
  {
    console.error(error);
  }
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