var retries;
var curAP = "";
let xhr = new XMLHttpRequest();

function updateSelSsid(sel)
{
  //$("#sta_ssid").value = sel.value;
  document.getElementById("sta_ssid").value = sel.value;
}

function createRowForAp(row, ap)
{
  row.id = ap.bssid;

  var radio = document.createElement('td');
  radio.className = "radio_tick";
  radio.style.width = '22px';
  /*radio.id = "radio_tick";*/
  var input = document.createElement("input")
  input.style.width = '22px';
  input.type = "radio";
  input.name = "ssid";
  input.value = ap.ssid;
  input.addEventListener('change', function() {updateSelSsid(input)});
  /*if(document.getElementById("sta_ssid").value == ap.ssid) input.checked = "1";*/
  if(curAP == ap.bssid) input.checked = "1";
  input.id = "opt-" + ap.ssid;
  radio.appendChild(input);
  row.appendChild(radio);

  var rssi = document.createElement('td');
  rssi.className = "nopadding";
  rssi.style.width = '32px';

  var rssi_img = document.createElement('div');
  if(ap.rssi > -30)
    rssi_img.className = "signal-bars sizing-box good five-bars";
  else if(ap.rssi > -67)
    rssi_img.className = "signal-bars sizing-box good four-bars";
  else if(ap.rssi > -70)
    rssi_img.className = "signal-bars sizing-box ok three-bars";
  else if(ap.rssi > -80)
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

window.onload = function(e)
{
  const staform = document.getElementById('staform');
  staform.addEventListener('submit', (event) =>
  {
    event.preventDefault();

    xhr.open('POST', '/wifi/setsta');

    let data = new FormData(event.target);
    const formJSON = Object.fromEntries(data.entries());

    xhr.setRequestHeader('Content-Type', 'application/json');
    xhr.send(JSON.stringify(formJSON));
  });
}

function scanAPs()
{
  var ws;

  if ( ws === undefined || ws.readyState != 0 )
  {
    if (retries)
    {
      setMsg("error", "WebSocket timeout, retrying..");
    }
    else
    {
      setMsg("info", "Opening WebSocket..");
    }
    var uri = "/websocket/apscan";
    ws = new WebSocket("ws://" + location.host + uri);
    ws.binaryType = 'arraybuffer';
    ws.onopen = function(evt)
    {
      retries = 0;
      setMsg("done", "WebSocket opened.");
    };
    ws.onerror = function(evt)
    {
      if ( ws.readyState == WebSocket.OPEN )
      {
        setMsg("error", "WebSocket error!");
      }
    };
    ws.close = function(evt)
    {
      console.log("websocket closed.");
      setMsg("done", "WebSocket closed.");
    };
    ws.onmessage = function(evt)
    {
      if ( evt.data )
      {
        var apList = JSON.parse(evt.data);
        //console.log(apList);
        if(apList.curAP)
        {
          curAP = apList.curAP;
        }
        if(apList.APs && apList.APs.length > 0)
        {
          let ntbdy = document.createElement('tbody');
          let l = apList.APs.length;
          for(var i = 0; i < l; i++)
          {
            let nr = ntbdy.insertRow();
            createRowForAp(nr, apList.APs[i]);
          }
          let otbdy = document.querySelector("#apList > tbody");
          otbdy.parentNode.replaceChild(ntbdy, otbdy);
        }
      }
    };
    retries = 0;
    window.onbeforeunload = function()
    {
      console.log("window closing");
      ws.onclose = function(){};
      ws.close(1000, "Window closing");
    };
  };
  function close(evt)
  {
    console.log("closing: user interaction");
    setMsg("done", "WebSocket closed.");
    ws.close(1000, "user interaction");
  };
}

function page_onload()
{
  init_all_dropboxex();
}