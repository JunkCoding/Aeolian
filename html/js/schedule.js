var jsonBri='{"name": "brightness","menu":[{"name": "Maximum", "value": 0, "submenu": null},{"name": "High", "value": 1, "submenu": null},{"name": "Medium", "value": 2, "submenu": null},{"name": "Low", "value": 3, "submenu": null},{"name": "Minimum", "value": 4, "submenu": null}]}';

const EVENT_NOFLAGS    = 0x00     // Event forces lights to be on during period or whole day
const EVENT_LIGHTSOFF  = 0x01     // Event forces lights to be off during period or whole day
const EVENT_AUTONOMOUS = 0x10     // Event will display with sunset/sunrise switching
const EVENT_DEFAULT    = 0x20     // Default event to be run when no other event is matched
const EVENT_IMMUTABLE  = 0x40     // Event cannot be removed/altered
const EVENT_DISABLED   = 0x80     // Event is not active (don't run)

var mousedown=false;
var hasMoved=false;
var setDates=false;
var sPos={
  target: null,
  doy: 0,
};
var ePos={
  target: null,
  doy: 0,
};
function fillTable(type, jsonData)
{
  tbl=_(type);

  /* Check it is the right data */
  if(jsonData.sched!==type&&!dd.classList.contains(items))
  {
    return;
  }
  function createFlagEl(id, state)
  {
    let td=document.createElement('td');
    td.className="fla_td";
    let div=document.createElement("div");
    div.className="tickbox";
    let el=document.createElement("input");
    el.setAttribute("type", "checkbox");
    el.id=id;
    el.checked=state;
    el.setAttribute("onchange", 'toggleFlag(this.id, this.checked)');
    div.appendChild(el);
    td.appendChild(div);
    return td;
  }
  let ntbdy=document.createElement('tbody');
  let l=jsonData.items.length;
  for(var i=0; i < l; i++)
  {
    let sched=jsonData.items[i];
    let nr=ntbdy.insertRow();

    var td;
    var el;

    td=document.createElement('td');
    td.classList.add("icon");
    el=document.createElement('i');
    el.classList.add("del");
    td.appendChild(el);
    nr.appendChild(td);

    // Annual only
    if(type==="annual")
    {
      td=document.createElement('td');
      el=document.createElement('div');
      el.classList.add("sharedsel", "ts-month");
      el.setAttribute('data-value', `0:${sched.M}`);
      td.appendChild(el);
      append_sharedSel(el);
      nr.appendChild(td);

      td=document.createElement('td');
      el=document.createElement('div');
      el.classList.add("sharedsel", "ts-date");
      el.setAttribute('data-value', `2:${Number(sched.sd)-1}`);
      td.appendChild(el);
      append_sharedSel(el);
      nr.appendChild(td);

      td=document.createElement('td');
      el=document.createElement('div');
      el.classList.add("sharedsel", "ts-date");
      el.setAttribute('data-value', `2:${Number(sched.ed)-1}`);
      td.appendChild(el);
      append_sharedSel(el);
      nr.appendChild(td);
    }
    else
    {
      td=document.createElement('td');
      el=document.createElement('div');
      el.classList.add("sharedsel", "ts-day");
      el.setAttribute('data-value', `1:${sched.D}`);
      append_sharedSel(el);
      td.appendChild(el);
      nr.appendChild(td);
    }

    // The rest is shared
    td=document.createElement('td');
    el=document.createElement('div');
    el.id=`stime_${type}_${sched.N}`;
    el.classList.add("timesel");
    el.setAttribute('data-value', `${sched.SH}:${sched.SM}`);
    td.appendChild(el);
    nr.appendChild(td);
    append_timesel(el);

    td=document.createElement('td');
    el=document.createElement('div');
    el.id=`etime_${type}_${sched.N}`;
    el.classList.add("timesel");
    el.setAttribute('data-value', `${sched.EH}:${sched.EM}`);
    td.appendChild(el);
    nr.appendChild(td);
    append_timesel(el);

    // Attributes
    td=document.createElement('td');
    el=document.createElement('div');
    el.classList.add("nopadding", "sharedsel", "theme");
    el.setAttribute('data-value', `theme:${sched.Th}`);
    append_sharedSel(el);
    td.appendChild(el);
    nr.appendChild(td);

    td=document.createElement('td');
    el=document.createElement('div');
    el.classList.add("nopadding", "sharedsel", "dim");
    el.setAttribute('data-value', `brightness:${sched.Br}`);
    append_sharedSel(el);
    td.appendChild(el);
    nr.appendChild(td);

    nr.appendChild(createFlagEl(`def_${type}_${sched.N}`, ((Number(sched.Fl)&EVENT_DEFAULT)===EVENT_DEFAULT)));
    nr.appendChild(createFlagEl(`off_${type}_${sched.N}`, ((Number(sched.Fl)&EVENT_LIGHTSOFF)===EVENT_LIGHTSOFF)));
    nr.appendChild(createFlagEl(`aut_${type}_${sched.N}`, ((Number(sched.Fl)&EVENT_AUTONOMOUS)===EVENT_AUTONOMOUS)));
    nr.appendChild(createFlagEl(`dis_${type}_${sched.N}`, ((Number(sched.Fl)&EVENT_DISABLED)===EVENT_DISABLED)));
  }

  // Remove the old and insert the new
  let otbdy=document.querySelector(`#${tbl.id} > tbody`);
  otbdy.parentNode.replaceChild(ntbdy, otbdy);

  if(jsonData.next>0)
  {
    fetchData(jsonData.next);
  }
}
// FixMe: Convert to promise...
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
function fetchData(JSONSource, start)
{
  var req=new XMLHttpRequest();
  req.overrideMimeType("application/json");
  req.open("get", JSONSource+".json?terse=1&start="+start, true);

  req.onload=function()
  {
    var jsonResponse;
    try
    {
      jsonResponse=JSON.parse(req.responseText);
    }
    catch(error)
    {
      console.error(error);
    }
    if(typeof jsonResponse==='object')
    {
      fillTable(JSONSource, jsonResponse);
    }
  };
  req.send(null);
}
function extend_sharedSel(jsonStr)
{

  sharedSel.options.push(JSON.parse(jsonStr));
}

async function fetchMenuItems(JSONSource, start)
{
  var doLoop=true;
  var jsonItemsStr='';

  // request options
  const options={
    method: 'GET',
    headers: {
      'Content-Type': 'application/json'
    }
  };
  while(doLoop===true)
  {
    const url=JSONSource+".json?terse=1&start="+start;
    const response=await fetch(url, options).catch(handleError);
    const data=await response.json();
    if(Array.isArray(data.items) === true)
    {
      for(let it of data.items)
      {
        if(jsonItemsStr!=='')
        {
          jsonItemsStr+=',';
        }
        jsonItemsStr+=`{"name":"${it.name}","value":${it.id},"submenu":null}`;
      }
    }
    if(data.next===0)
    {
      doLoop=false;
    }
  }
  extend_sharedSel(`{"name":"${JSONSource}","menu":[${jsonItemsStr}]}`);
}
function eventHandler(event)
{
  if(event.type==="click")
  {
    tgt=event.target;
    if(tgt.classList.contains('add')===true)
    {
      // Get our parent table, which is the table we are inserting a new row.
      let tbl=closest(tgt, 'table');
      let tbdy=tbl.getElementsByTagName('tbody')[0];
      let nr=tbdy.insertRow();
      let td=document.createElement('td');
      td.classList.add("icon");
      el=document.createElement('i');
      el.classList.add("del");
      td.appendChild(el);
      nr.appendChild(td);
      td=document.createElement('td');
      td.colSpan="11";
      nr.appendChild(td);
    }
  }
  else
  {
    console.log(event);
  }
}
function setFooter(type)
{
  tbl=_(type);
  let foot=tbl.createTFoot(0);
  let nr=foot.insertRow(0);
  td=document.createElement('td');
  td.colSpan="11";
  td.classList.add("icon");
  el=document.createElement('i');
  el.classList.add("add");
  el.addEventListener("click", eventHandler);
  td.appendChild(el);
  nr.appendChild(td);
}
function page_onload()
{
  set_background();
  extend_sharedSel(jsonBri);
  fetchMenuItems("theme", 0);
  fetchSchedule("weekly", 0);
  setFooter("weekly");
  fetchSchedule("annual", 0);
  setFooter("annual");
}

