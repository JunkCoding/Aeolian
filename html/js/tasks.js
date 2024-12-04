var retries;

function createRowForTask(row, task)
{
  row.id = task['pid'];

  const fields = new Array("pid", "bp", "cp", "tn", "hwm", "c8", "c32", "rt", "st", "core", "use");

  fields.forEach((element) => {
    var cell = document.createElement('td');
    cell.className = "nopadding";
    cell.appendChild(document.createTextNode(task[element]));
    row.appendChild(cell);
  });

  return row;
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
    var uri = "/websocket/tasks";
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
        var stats = JSON.parse(evt.data);
        if(stats.proc && stats.proc.length > 0)
        {
          let ntbdy = document.createElement('tbody');
          let l = stats.proc.length;
          for(var i = 0; i < l; i++)
          {
            let nr = ntbdy.insertRow();
            createRowForTask(nr, stats.proc[i]);
          }
          let otbdy = document.querySelector("#taskList > tbody");
          otbdy.parentNode.replaceChild(ntbdy, otbdy);
        }
      }
    };
    retries = 0;
    window.onbeforeunload = function()
    {
      ws.onclose = function () {};
      ws.close(222, "Window closing");
    };
  }
}
function page_onload()
{
  wsOpen();
}

