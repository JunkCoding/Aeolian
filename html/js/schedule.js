
function fillTable(type, jsonData)
{
  tbl=_(type);

  /* Check it is the right data */
  if(jsonData.sched!==type&&!dd.classList.contains(items))
  {
    return;
  }

  let ntbdy=document.createElement('tbody');
  let l=jsonData.items.length;
  for(var i=0; i < l; i++)
  {
    let sched=jsonData.items[i];
    let nr=ntbdy.insertRow();
    // Weekly / Anmual (repectively)
    // Day, hour, min, hour, min, theme, brightness, flags
    // Day, hour, min, day, hour, min, theme, brightness, flags

    // Start
    let day=document.createElement('td');
    nr.appendChild(day);
    let hour=document.createElement('td');
    nr.appendChild(hour);
    let min=document.createElement('td');
    nr.appendChild(min);

    // End
    if(type==="annual")
    {
      day=document.createElement('td');
      nr.appendChild(day);
    }
    hour=document.createElement('td');
    nr.appendChild(hour);
    min=document.createElement('td');
    nr.appendChild(min);

    // Attributes
    let theme=document.createElement('td');
    nr.appendChild(theme);
    let brightness=document.createElement('td');
    nr.appendChild(brightness);
    let flags=document.createElement('td');
    nr.appendChild(flags);

    // Append to the tbody
    ntbdy.appendChild(nr);
  }

  // Remove the old and insert the new
  let otbdy=document.querySelector(`#${tbl.id} > tbody`);
  otbdy.parentNode.replaceChild(ntbdy, otbdy);

  if(jsonData.next>0)
  {
    fetchData(jsonData.next);
  }
}
function fetchSchedule(type, start)
{
  var req=new XMLHttpRequest();
  req.overrideMimeType("application/json");
  req.open("get", `schedule.json?schedule=${type}&start=${start}`, true);

  req.onload=function()
  {
    var jsonResponse=JSON.parse(req.responseText);
    fillTable(type, jsonResponse);
  };
  req.send(null);
}

function page_onload()
{
  set_background();
  fetchSchedule("weekly", 0);
  fetchSchedule("annual", 0);
}
