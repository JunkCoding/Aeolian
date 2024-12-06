
var retries;

function wsOpen()
{
  var ws;
  var ansi_up = new AnsiUp;
  var rcvd = document.getElementById("received");

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
    var uri = "/websocket/log";
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
        // Save the current position
        var preTop = rcvd.scrollTop;
        var as = (preTop == (rcvd.scrollHeight - rcvd.offsetHeight))

        var tmpStr = ansi_up.ansi_to_html(evt.data);
        rcvd.innerHTML += tmpStr.replace(new RegExp('\r?\n','g'), '<br />');

        // Autoscroll if we were at the bottom of the log output
        if ( as == true )
        {
          // Use this to see if we want to autoscroll
          rcvd.scrollTop = (rcvd.scrollHeight - rcvd.offsetHeight);
        }
        else
        {
          rcvd.scrollTop = preTop;
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

