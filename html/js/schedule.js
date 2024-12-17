
var dow=["Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"];
var moy=["January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"];
var ds=["st", "nd", "rd", "th"];

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
    el=document.createElement('input');
    el.type='time';
    el.classList.add("ts-hour");
    el.innerHTML=`${sched.SH}:${sched.SM}`;
    td.appendChild(el);
    nr.appendChild(td);

    td=document.createElement('td');
    el=document.createElement('input');
    el.type='time';
    el.classList.add("ts-hour");
    el.innerHTML=`${sched.EH}:${sched.EM}`;
    td.appendChild(el);
    nr.appendChild(td);

    // Attributes
    el=document.createElement('td');
    el.innerHTML=sched.Th;
    nr.appendChild(el);
    el=document.createElement('td');
    el.innerHTML="";
    nr.appendChild(el);
    el=document.createElement('td');
    el.innerHTML=sched.Fl;
    nr.appendChild(el);

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
