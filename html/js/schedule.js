
var dow=["Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"];
var moy=["January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"];
var ds=["st", "nd", "rd", "th"];

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

  let ntbdy=document.createElement('tbody');
  let l=jsonData.items.length;
  for(var i=0; i < l; i++)
  {
    let sched=jsonData.items[i];
    let nr=ntbdy.insertRow();

    var td;
    var el;
    // Annual only
    if(type==="annual")
    {
      el=document.createElement('td');
      el.classList.add("ts-month");
      el.innerHTML=moy[sched.M];
      nr.appendChild(el);

      el=document.createElement('td');
      el.classList.add("ts-date");
      el.innerHTML=sched.sd;
      nr.appendChild(el);

      el=document.createElement('td');
      el.classList.add("ts-date");
      el.innerHTML=sched.ed;
      nr.appendChild(el);
    }
    else
    {
      el=document.createElement('td');
      el.classList.add("ts-wday");
      el.innerHTML=dow[sched.D];
      nr.appendChild(el);
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
    td.className="nopadding";

    td.innerHTML=sched.Th;
    nr.appendChild(td);
    td=document.createElement('td');
    td.innerHTML="";
    nr.appendChild(td);

    td=document.createElement('td');
    td.className="fla_td";
    let div=document.createElement("div");
    div.className="tickbox";
    el=document.createElement("input");
    el.setAttribute("type", "checkbox");
    el.id=`def_${type}_${sched.N}`;
    el.checked=((Number(sched.Fl)&EVENT_DEFAULT)===EVENT_DEFAULT);
    el.setAttribute("onchange", 'toggleEnabled(this.id, this.checked)');
    div.appendChild(el);
    td.appendChild(div);
    nr.appendChild(td);

    td=document.createElement('td');
    td.className="fla_td";
    div=document.createElement("div");
    div.className="tickbox";
    el=document.createElement("input");
    el.setAttribute("type", "checkbox");
    el.id=`off_${type}_${sched.N}`;
    el.checked=((Number(sched.Fl)&EVENT_LIGHTSOFF)===EVENT_LIGHTSOFF);
    el.setAttribute("onchange", 'toggleEnabled(this.id, this.checked)');
    div.appendChild(el);
    td.appendChild(div);
    nr.appendChild(td);

    td=document.createElement('td');
    td.className="fla_td";
    div=document.createElement("div");
    div.className="tickbox";
    el=document.createElement("input");
    el.setAttribute("type", "checkbox");
    el.id=`aut_${type}_${sched.N}`;
    el.checked=((Number(sched.Fl)&EVENT_AUTONOMOUS)===EVENT_AUTONOMOUS);
    el.setAttribute("onchange", 'toggleEnabled(this.id, this.checked)');
    div.appendChild(el);
    td.appendChild(div);
    nr.appendChild(td);

    td=document.createElement('td');
    td.className="fla_td";
    div=document.createElement("div");
    div.className="tickbox";
    el=document.createElement("input");
    el.setAttribute("type", "checkbox");
    el.id=`dis_${type}_${sched.N}`;
    el.checked=((Number(sched.Fl)&EVENT_DISABLED)===EVENT_DISABLED);
    el.setAttribute("onchange", 'toggleEnabled(this.id, this.checked)');
    div.appendChild(el);
    td.appendChild(div);
    nr.appendChild(td);

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
