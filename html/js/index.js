var retries;
function set_switch(switchID, state)
{
  document.getElementById(switchID).checked = state == 1 ? true : false;

  if( document.getElementById(switchID).checked )
  {
    document.getElementById( "hidden_" + switchID ).disabled = true;
  }
}
function switchLeds(switchID)
{
  if ( switchID.checked == true )
  {
    updateControl('led_switch', document.leds.state.value);
  }
}
function page_onload()
{
  /* Add our brightness dropdown box to our custom DropDown */
  init_dropbox(document.getElementById("dim"));

  /* Dynamically fill the other dropdown boxes */
  fetchData("theme", 0);
  fetchData("pattern", 0);

  /* Run after all drop boxes have been configured */
  //init_all_dropboxex();

  /* Open the websocket */
  wsOpen();
}

function wsOpen()
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
    var uri = "/websocket/status";
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
      setMsg("done", "WebSocket closed.");
    };
    ws.onmessage = function(evt)
    {
      var stats = JSON.parse(evt.data);
      document.getElementById('date_time').innerHTML = stats.dt;
      document.getElementById('uptime').innerHTML = stats.ut;
      document.getElementById('memory').innerHTML = stats.me;
      document.getElementById('led_status').innerHTML = ((stats.pa == 0) ? 'On' : 'Off');
      document.getElementById('zone0').innerHTML = ((stats.z0 == 1) ? 'On' : 'Off');
      document.getElementById('zone1').innerHTML = ((stats.z1 == 1) ? 'On' : 'Off');
      document.getElementById('zone2').innerHTML = ((stats.z2 == 1) ? 'On' : 'Off');
      document.getElementById('zone3').innerHTML = ((stats.z3 == 1) ? 'On' : 'Off');

      // See if we have a dim setting and whether our form can handle it
      if ( (stats.di >= 0) && (stats.di < document.getElementById("dim").querySelectorAll("li").length) )
      {
        document.getElementById('selDim').textContent = document.getElementById("dim").querySelectorAll("li")[stats.di].textContent;
      }

      patlist = document.getElementById("pattern").querySelectorAll("li");
      if ( (stats.cp) && patlist.length > 0 )
      {
        for ( var item of patlist )
        {
          if ( stats.cp == item.value )
          {
            document.getElementById('selPattern').textContent = item.textContent;
            break;
          }
        }
      }

      document.leds.state.value=stats.ll;
      set_switch("light_switch", stats.sl);
    };
    retries = 0;
    window.onbeforeunload = function()
    {
      ws.onclose = function () {};
      ws.close(222, "Window closing");
    };
  }
}
function fillTable(JSONSource, jsonData)
{
  var active = -1;

  if ( jsonData.hasOwnProperty('active') )
  {
    active = jsonData.active;
  }

  var dd = document.getElementById(JSONSource);
  var plist = dd.querySelector("ul");
  let l = jsonData.items.length;
  for (var i = 0; i < l; i++)
  {
    var item  = jsonData.items[i];
    var opt   = document.createElement("li");
    opt.value = item.id;
    opt.text  = item.name;
    var lnk   = document.createElement("a");
    lnk.href  = "#";
    lnk.text  = item.name;
    opt.appendChild(lnk);
    plist.appendChild(opt);
    if ( item.id == active )
    {
      document.getElementById(JSONSource).querySelector("span").textContent = item.name;
    }
  }

  if ( jsonData.next > 0 )
  {
    fetchData(JSONSource, jsonData.next);
  }
  else
  {
    /* Run this after the list has been populated */
    init_dropbox(dd);
  }
}
function fetchData(JSONSource, start)
{
  var req = new XMLHttpRequest();
  req.overrideMimeType("application/json");
  req.open("get", JSONSource+".json?terse=1&start="+start, true);

  req.onload = function()
  {
    var jsonResponse;
    try
    {
      jsonResponse = JSON.parse(req.responseText);
    }
    catch (error)
    {
      console.error(error);
    }
    if ( typeof jsonResponse === 'object' )
    {
      fillTable(JSONSource, jsonResponse);
    }
  };
  req.send(null);
}
