/* jshint esversion: 8 */

var jsonBri='{"name": "brightness","menu":[{"name": "Maximum", "value": 0, "submenu": null},{"name": "High", "value": 1, "submenu": null},{"name": "Medium", "value": 2, "submenu": null},{"name": "Low", "value": 3, "submenu": null},{"name": "Minimum", "value": 4, "submenu": null}]}';

const EVENT_NOFLAGS      = 0x00;     // Event forces lights to be on during period or whole day
const EVENT_LIGHTSOFF    = 0x01;     // Event forces lights to be off during period or whole day
const EVENT_AUTONOMOUS   = 0x10;     // Event will display with sunset/sunrise switching
const EVENT_DEFAULT      = 0x20;     // Default event to be run when no other event is matched
const EVENT_IMMUTABLE    = 0x40;     // Event cannot be removed/altered
const EVENT_DISABLED     = 0x80;     // Event is not active (don't run)

function clone_row(tbl, id)
{
  // Get our parent table, which is the table we are inserting a new row.
  const tbdy=tbl.getElementsByTagName('tbody')[0];
  const clone=document.querySelector(`#t_${tbl.id}Row`).content.cloneNode(true);
  const divs=clone.querySelectorAll("td");
  tbdy.appendChild(clone);
  /* Set the row id */
  if(id!==255)
  {
    divs[0].parentNode.id=`${tbl.id}_${id}`;
  }
  else
  {
    divs[0].parentNode.id=`${tbl.id}_${tbdy.querySelectorAll("tr").length-1}`;/* -1 as we already added a row */
    divs[0].parentNode.classList.add("new");
  }
  return (divs);
}
function fillTable(type, jsonData)
{
  /** @type  {Object} tbl */
  let tbl=_(type);

  /* Check it is the right data */
  if(jsonData.sched!==type)
  {
    return;
  }

  // Remove the old and introduce the new
  /** @type  {Object} ntbdy */
  const ntbdy=document.createElement('tbody');
  /** @type  {Object} otbdy */
  const otbdy=document.querySelector(`#${tbl.id} > tbody`);
  otbdy.parentNode.replaceChild(ntbdy, otbdy);

  let l=jsonData.items.length;
  for(var i=0; i < l; i++)
  {
    const sched=jsonData.items[i];

    let d=0;
    let divEls=clone_row(tbl, sched.N);
    divEls[d].querySelector("i").className="del";
    divEls[d++].querySelector("i").addEventListener("click", eventHandler);

    let el=divEls[d++].querySelector(".sharedsel");
    if(tbl.id==='annual')
    {
      el.setAttribute('data-value', `0:${sched.M}`);
      append_sharedSel(el, changeHandler);

      el=divEls[d++].querySelector(".sharedsel");
      el.setAttribute('data-value', `2:${Number(sched.sd)-1}`);
      append_sharedSel(el, changeHandler);

      el=divEls[d++].querySelector(".sharedsel");
      el.setAttribute('data-value', `2:${Number(sched.ed)-1}`);
      append_sharedSel(el, changeHandler);
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
  divEls[d].querySelector("i").className="del";
  divEls[d++].querySelector("i").addEventListener("click", eventHandler);

  let el=divEls[d++].querySelector(".sharedsel");
  if(tblId==='annual')
  {
    el.setAttribute('data-value', '0:0');
    append_sharedSel(el, changeHandler);

    el=divEls[d++].querySelector(".sharedsel");
    el.setAttribute('data-value', '2:0');
    append_sharedSel(el, changeHandler);

    el=divEls[d++].querySelector(".sharedsel");
    el.setAttribute('data-value', '2:0');
    append_sharedSel(el, changeHandler);
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
function replace_month(el, month)
{
  let cl=el.classList;
  for(let cssClass of cl)
  {
    if(cssClass.search(/sharedsel|dropdown/) < 0)
    {
      el.classList.remove(cssClass);
    }
  }
  el.classList.add(month);
}
/**
 *
 * @param {Object} _this
 * @param {Object} event
 * @returns {boolean}
 */
function changeHandler(_this, event)
{
  let retVal=true;
  let tgt=event.target;
  /* check we are on target */
  if(tgt.classList.contains('dropdown-item')===true)
  {
    /* Get the type of dropdown menu we are dealing with */
    let type=_this.target.dataset.type;

    /* Ensure it is a string */
    if(typeof type==="string")
    {
      /* If month, we need to modify the days if current days > days in new month */
      if(type==="month")
      {
        /* Add/replace the month class of the parents to prevent selection of N/A days */
        let dsEl=closest(tgt, "[data-type='dayStart']");
        replace_month(dsEl, tgt.textContent);
        let deEl=closest(tgt, "[data-type='dayEnd']");
        replace_month(deEl, tgt.textContent);

        /* Get the max days in the month before selection */
        let curMonth=closest(tgt, '.dropdown-container').dataset.value;
        let curMax=sharedSel.daysInMonth[curMonth]-1;

        /* Check the current days are valid for the selected month */
        let selMax=sharedSel.daysInMonth[tgt.value]-1;
        /* Ccheck if our current day exceeds the number in the selected month */
        if(deEl.dataset.value>selMax)
        {
          _this.setMenuItem(deEl, selMax);
        }
        /* If our current day is the last day, and also not the same as the end day
         * we will make the new month also the last day */
        else if(deEl.dataset.value==curMax&&dsEl.dataset.value<deEl.dataset.value)
        {
          _this.setMenuItem(deEl, selMax);
        }
        /* check start date is on or before the end date */
        if(dsEl.dataset.value>deEl.dataset.value)
        {
          _this.setMenuItem(dsEl, deEl.dataset.value);
        }
      }
      /* If we are modifying the start day, ensure it is <= the end day */
      else if(type==="dayStart")
      {
        let ed=closest(tgt, "[data-type=dayEnd]");
        if(tgt.value>ed.dataset.value)
        {
          retVal=false;
        }
      }
        /* Likewise, check the end day is >= the start day and <= days in the selected month */
      else if(type==="dayEnd")
      {
        let sd = closest(tgt, "[data-type=dayStart]");
        if(tgt.value<sd.dataset.value)
        {
          retVal=false;
        }
      }
    }
  }
  /* Return false if we want to stop the change happening */
  return retVal;
}
function eventHandler(event)
{
  let tgt;
  if(event.type==="click")
  {
    tgt=event.target;
    if(tgt.classList.contains('add')===true)
    {
      // If we don't have 'template' support we don't get a new row.
      if("content" in document.createElement("template"))
      {
        const tbl=closest(tgt, 'table');
        if(typeof tbl==='object')
        {
          set_row_defaults(tbl.id, clone_row(tbl, 0xff));
        }
      }
    }
    else if(tgt.classList.contains('del')===true)
    {
      const tbl=closest(tgt, 'table');
      if(typeof tbl==='object')
      {
        console.log(tbl.id, closest(tgt, 'tr').id);
      }
    }
  }
  else if(event.type==="change")
  {
    console.log("change");
  }
  else
  {
    console.log(event.type);
  }
}
/**
 * @param {string} type
 */
function setFooter(type)
{
  /** @type  {Object} tbl */
  let tbl=_(type);
  if(tbl!==null)
  {
    let foot=tbl.createTFoot(0);
    let nr=foot.insertRow(0);
    let td=document.createElement('td');
    td.classList.add("icon");
    let el=document.createElement('i');
    el.classList.add("add");
    el.addEventListener("click", eventHandler);
    td.appendChild(el);
    nr.appendChild(td);
    td=document.createElement('td');
    td.colSpan=11;
    nr.appendChild(td);
  }
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

