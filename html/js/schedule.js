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
function clone_row(tbl, id)
{
  // Get our parent table, which is the table we are inserting a new row.
  const tbdy=tbl.getElementsByTagName('tbody')[0];
  const clone=document.querySelector(`#t_${tbl.id}Row`).content.cloneNode(true);
  const divs=clone.querySelectorAll("td");
  tbdy.appendChild(clone);
  /* Set the row id */
  divs[0].parentNode.id=`${tbl.id}_${id}`;
  return (divs);
}
function fillTable(type, jsonData)
{
  tbl=_(type);

  /* Check it is the right data */
  if(jsonData.sched!==type&&!dd.classList.contains(items))
  {
    return;
  }

  // Remove the old and introduce the new
  const ntbdy=document.createElement('tbody');
  const otbdy=document.querySelector(`#${tbl.id} > tbody`);
  otbdy.parentNode.replaceChild(ntbdy, otbdy);

  let l=jsonData.items.length;
  for(var i=0; i < l; i++)
  {
    const sched=jsonData.items[i];

    let d=0;
    let divEls=clone_row(tbl, sched.N);
    divEls[d++].querySelector("i").className="del";

    let el=divEls[d++].querySelector(".sharedsel");
    if(tbl.id==='annual')
    {
      el.setAttribute('data-value', `0:${sched.M}`);
      append_sharedSel(el);

      el=divEls[d++].querySelector(".sharedsel");
      el.setAttribute('data-value', `2:${Number(sched.sd)-1}`);
      append_sharedSel(el);

      el=divEls[d++].querySelector(".sharedsel");
      el.setAttribute('data-value', `2:${Number(sched.ed)-1}`);
      append_sharedSel(el);
    }
    else if(tbl.id==='weekly')
    {
      el.setAttribute('data-value', `1:${sched.D}`);
      append_sharedSel(el);
    }
    el=divEls[d++].querySelector(".timesel");
    el.setAttribute('data-value', `${sched.SH}:${sched.SM}`);
    append_timesel(el);

    el=divEls[d++].querySelector(".timesel");
    el.setAttribute('data-value', `${sched.EH}:${sched.EM}`);
    append_timesel(el);

    el=divEls[d++].querySelector(".sharedsel");
    el.setAttribute('data-value', `theme:${sched.Th}`);
    append_sharedSel(el);

    el=divEls[d++].querySelector(".sharedsel");
    el.setAttribute('data-value', `brightness:${sched.Br}`);
    append_sharedSel(el);

    el=divEls[d++].querySelector("input[type=checkbox]");
    el.checked=((Number(sched.Fl)&EVENT_DEFAULT)===EVENT_DEFAULT);
    el=divEls[d++].querySelector("input[type=checkbox]");
    el.checked=((Number(sched.Fl)&EVENT_LIGHTSOFF)===EVENT_LIGHTSOFF);
    el=divEls[d++].querySelector("input[type=checkbox]");
    el.checked=((Number(sched.Fl)&EVENT_AUTONOMOUS)===EVENT_AUTONOMOUS);
    el=divEls[d++].querySelector("input[type=checkbox]");
    el.checked=((Number(sched.Fl)&EVENT_DISABLED)===EVENT_DISABLED);
  }

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
function set_row_defaults(tblId, divEls)
{
  let d=0;
  divEls[d++].querySelector("i").className="del";

  let el=divEls[d++].querySelector(".sharedsel");
  if(tblId==='annual')
  {
    el.setAttribute('data-value', '0:0');
    append_sharedSel(el);

    el=divEls[d++].querySelector(".sharedsel");
    el.setAttribute('data-value', '2:0');
    append_sharedSel(el);

    el=divEls[d++].querySelector(".sharedsel");
    el.setAttribute('data-value', '2:0');
    append_sharedSel(el);
  }
  else if(tblId==='weekly')
  {
    el.setAttribute('data-value', '1:0');
    append_sharedSel(el);
  }
  el=divEls[d++].querySelector(".timesel");
  el.setAttribute('data-value', '00:00');
  append_timesel(el);

  el=divEls[d++].querySelector(".timesel");
  el.setAttribute('data-value', '00:00');
  append_timesel(el);

  el=divEls[d++].querySelector(".sharedsel");
  el.setAttribute('data-value', 'theme:0');
  append_sharedSel(el);

  el=divEls[d++].querySelector(".sharedsel");
  el.setAttribute('data-value', 'brightness:2');
  append_sharedSel(el);

  el=divEls[d++].querySelector("input[type=checkbox]");
  el=divEls[d++].querySelector("input[type=checkbox]");
  el=divEls[d++].querySelector("input[type=checkbox]");
  el=divEls[d++].querySelector("input[type=checkbox]");
}
function eventHandler(event)
{
  if(event.type==="click")
  {
    tgt=event.target;
    if(tgt.classList.contains('add')===true)
    {
      // If we don't have 'template' support we don't get a new row.
      if("content" in document.createElement("template"))
      {
        const tbl=closest(tgt, 'table');
        console.log(tbl.id);
        set_row_defaults(tbl.id,clone_row(tbl, 0xff));
      }
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
  td.classList.add("icon");
  el=document.createElement('i');
  el.classList.add("add");
  el.addEventListener("click", eventHandler);
  td.appendChild(el);
  nr.appendChild(td);
  td=document.createElement('td');
  td.colSpan="11";
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

