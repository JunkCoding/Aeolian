var retries;

/* Create a graph */
function createGraph()
{
  return new SmoothieChart(
  {
    millisPerPixel:250,
    grid:
    {
      strokeStyle:'rgba(45,45,45,0.51)',
      millisPerLine:6000,
      verticalSections:6
    },
    labels:
    {
      fontSize:12,
      precision:0
    },
    tooltip:true,
    tooltipLine:
    {
      strokeStyle:'#fff'
    },
    horizontalLines:
    [
      //el x{color:'rgba(250,0,0,0.51)',lineWidth:1,value:0},
      {color:'rgba(250,0,0,0.51)',lineWidth:1,value:33},
      {color:'rgba(250,0,0,0.51)',lineWidth:1,value:66}
    ],
    limitFPS:10,
    minValue:0,
    maxValue:100
  });
}

/* ToDo: Make dynamic */
function wsOpen()
{
  var initialised = false;
  var ws;
  let g = new Array();
  let m = new Array();
  let s = new Array();

  if ( ws === undefined || ws.readyState != 0 )
  {
    if ( retries )
    {
      setMsg("error", "WebSocket timeout, retrying..");
    }
    else
    {
      setMsg("info", "Opening WebSocket..");
    }
    var uri = "/websocket/gdata";
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
    ws.onclose = function(evt)
    {
      setMsg("done", "WebSocket closed.");
    };
    ws.onmessage = function(evt)
    {
      if ( evt.data )
      {
        /* Parse our (expected) JSOn data */
        var gdata = JSON.parse(evt.data);

        /* Initialise graphs and arrays */
        if ( !initialised )
        {
          /* Where to place our graphs */
          const glist = document.getElementById('glist');

          if ( gdata.eng.mov && gdata.eng.mov.length > 0)
          {
            let l = gdata.eng.mov.length;
            for(var i = 0; i < l; i++)
            {
              /* Create and append our canvas element */
              var canvas = document.createElement("canvas");
              canvas.id           = "g" + i;
              canvas.width        = 900;
              canvas.height       = 110;
              canvas.style.border = "1px solid white";
              glist.appendChild(canvas);

              /* Create and attach our graph */
              g[i] = createGraph();
              g[i].streamTo(canvas);

              /* Create lines for moving and stationary gates */
              m[i] = new TimeSeries();
              s[i] = new TimeSeries();

              /* Set our line styles (interpolations: bezier, linear, step) */
              g[i].addTimeSeries(m[i],{strokeStyle:'rgb(0,255,0)',fillStyle:'rgba(0,255,0,0.4)',lineWidth:1,fillToBottom:false,interpolation:'step'});
              g[i].addTimeSeries(s[i],{strokeStyle:'rgb(255,0,255)',fillStyle:'rgba(255,0,255,0.3)',lineWidth:1,fillToBottom:false,interpolation:'step'});
            }
            /* Flag we are initialised */
            initialised = true;
          }
        }
        else
        {
          /* Process moving energy values */
          if ( gdata.eng.mov && gdata.eng.mov.length > 0 )
          {
            let l = gdata.eng.mov.length;
            for(var i = 0; i < l; i++)
            {
              m[i].append(Date.now(), gdata.eng.mov[i]);
            }
          }

          /* Process stationary energy values */
          if ( gdata && gdata.eng.sta.length > 0 )
          {
            let l = gdata.eng.sta.length;
            for(var i = 0; i < l; i++)
            {
              s[i].append(Date.now(), gdata.eng.sta[i]);
            }
          }
        }
      }
    };
    retries = 0;
  }
}
function page_onload()
{
  wsOpen();
}

