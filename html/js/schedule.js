var strMonthNames = ["January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"];
var last_doy = 0;
function getLastDay(mon, year)
{
  return new Date(year, mon+1, 0);
}
function getFirstDay(mon, year)
{
  return new Date(year, mon, 1);
}
function daysInMonth(mon, year)
{
  return new Date(year, mon + 1, 0).getDate();
}
function page_onload()
{
  set_background();
  fetchData(0);
  console.clear();
  fillCalendar(new Date().getFullYear());
}
function padnum(x) { return String("00" + x).slice(-3); };
function fillCalendar(year)
{
  var now = new Date();
  var cur_dat = now.getDate();
  var cur_mon = now.getMonth();

  // Keep track of Day of Year
  var doy = 1;

  // Iterate through all months
  for ( var mon = 0; mon < 12; mon++ )
  {
    var ul = document.getElementById("m" + mon);
    var ld = daysInMonth(mon, year);
    for ( var date = 1; date <= ld; date++, doy++ )
    {
      cd = new Date(year, mon, date);
      var li = document.createElement('li');
      var today = "";
      if ( date === cur_dat && (mon === cur_mon) )
      {
        today = " today";
      }
      li.innerHTML = "<li class=\"day" + today + "\" data-day=\"" + cd.getDay() + "\" data-date=\"" + date + "\" data-doy=\"" + doy + "\" id=\"d" + padnum(doy) + "\"></li>";
      ul.appendChild(li);
    }
  }
  last_doy = doy - 1;
}
function fillTable(jsonData)
{
  return;
  for (var i = 0; i < jsonData.themes.length; i++)
  {
    var theme = jsonData.themes[i];

    var tbl = document.createElement('Table');
    tbl.id = theme.name; tbl.className = "hdr_table";

    var hdr = tbl.createTHead();
    var row = hdr.insertRow(0);
    var cell = document.createElement('th');
    cell.innerHTML = `<b>${theme.name}</b>`;
    row.appendChild(cell);

    var tbd = document.createElement('tbody');
    tbl.appendChild(tbd);

    var row = document.createElement('tr');

    var cell = document.createElement('td');
    row.appendChild(cell);

    var div = document.createElement('div');
    div.id = "gridcontainer";
    cell.appendChild(div);

    var sect = document.createElement('section');
    sect.className = "mainGrid";
    sect.style = `grid-template-rows:repeat(${theme.palettes.length}, 1fr);`

    let l = theme.palettes.length;
    for (var p = 0; p < l; p++)
    {
      var palette = theme.palettes[p];
      let m = palette[p].length;
      for (var c = 0; c < m; c++)
      {
        var col = palette[p][c];
        var box = document.createElement('div');
        box.className = `colbox`;
        box.id = `t${i}p${p}c${c}`;
        box.style = `background-color: ${col}`
        sect.appendChild(box);
      }
    }
    div.appendChild(sect);
    tbd.appendChild(row);
    scheddiv.appendChild(tbl);

    div = document.createElement('div');
    div.className = 'spacer';
    scheddiv.appendChild(div);
  }

  if (jsonData.next > 0)
  {
    fetchData(jsonData.next);
  }
}
function fetchData(start)
{
  var req = new XMLHttpRequest();
  req.overrideMimeType("application/json");
  req.open("get", "schedule.json?start=" + start, true);

  req.onload = function ()
  {
    var jsonResponse = JSON.parse(req.responseText);
    fillTable(jsonResponse);
  };
  req.send(null);
}
var mousedown = false;
var hasMoved = false;
var setDates = false;
var sPos = {
  target: null,
  doy: 0,
}
var ePos ={
  target: null,
  doy: 0,
}
function drawSelected()
{
  // Set our display start and end dates
  dbot = sPos;
  dtop = ePos;

  // When displaying selected dates, make sure we are ascending
  function swap() { var tmp = dtop; dtop = dbot; dbot = tmp; };
  if (dtop.doy < dbot.doy)
  {
    swap();
  }

  // Remove any previously selected items
  [...document.getElementsByClassName('day-selected')].forEach(el => {
    let x = el.getAttribute("data-doy");
    if ( x < dbot.doy || x > dtop.doy )
    {
      el.classList.remove("day-selected");
    }
  });

  // Iterate through the calendar to display selected dates
  cd = dbot.doy;
  let year = new Date().getFullYear();
  do
  {
    uid = "d" + padnum(cd);
    let cell = document.getElementById(uid);
    if ((setDates === false) && cell.classList.contains("day-selected"))
    {
      cell.classList.remove("day-selected");
    }
    else if (setDates)
    {
      cell.classList.add("day-selected");
    }
  }
  while ( ++cd <= dtop.doy );
}
document.addEventListener("DOMContentLoaded", function (event)
{
  console.log("Ready!");
  document.body.onmousedown = function (el)
  {
    console.log("onmousedown: el.target.id = " + el.target.id);
    if ( el.target.classList.contains("day") )
    {
      mousedown = true;
      hasMoved = false;
      sPos.target = el.target;
      sPos.doy = parseInt(el.target.getAttribute("data-doy"));
      if ( el.target.classList.contains("day-selected") )
      {
        setDates = false;
        el.target.classList.remove("day-selected");
      }
      else
      {
        setDates = true;
        el.target.classList.add("day-selected");
      }
    }
  }
  document.body.onmouseup = function (el)
  {
    mousedown = false;
    if ( el.target.classList.contains("day") )
    {
      console.log("onmouseup: el.target.id = " + el.target.id);
      if ( sPos.target.id === el.target.id )
      {
        // Remove previously selected items outside of our range
        [...document.getElementsByClassName('day-selected')].forEach(el => {
          let x = el.getAttribute("data-doy");
          if ( x != sPos.doy )
          {
            el.classList.remove("day-selected");
          }
        });
      }
    }
    else if ( sPos )
    {
      sPos.target = null;
      ePos.target = null;
      [...document.getElementsByClassName('day-selected')].forEach(el => {
        el.classList.remove("day-selected");
      });
    }
  }
  document.body.ondragstart = function (el)
  {
    console.log("ondragstart: el.target.id = " + el.target.id);
    mousedown = false;
    return false;
  }
  document.body.ondrag = function (el)
  {
    console.log("ondrag: el.target.id = " + el.target.id);
    mousedown = false;
    return false;
  }
  document.body.ondragend = function (el)
  {
    console.log("ondrag: el.target.id = " + el.target.id);
    return false;
  }
  document.body.ondragleave = function (el)
  {
    console.log("ondrag: el.target.id = " + el.target.id);
    return false;
  }
  document.body.onmouseleave = function (el)
  {
    console.log("onmouseleave: el.target.id = " + el.target.id);
    mousedown = false;
  }
  document.body.onmousemove = function (el)
  {
    if ( (mousedown) && el.target.classList.contains("day") )
    {
      // Save our end position
      ePos.target = el.target;
      ePos.doy = parseInt(el.target.getAttribute("data-doy"));

      if ( ePos.target.id != sPos.target.id )
      {
        hasMoved = true;
      }

      if ( hasMoved )
      {
        drawSelected();
      }
    }
  }
});