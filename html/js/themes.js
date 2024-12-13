
function fillTable(jsonData)
{
  for (var i = 0; i < jsonData.items.length; i++)
  {
    var theme = jsonData.items[i];

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
    sect.style = `grid-template-rows:repeat(${theme.items.length}, 1fr);`

    let l = theme.items.length;
    for (var p = 0; p < l; p++)
    {
      var palette = theme.items[p];
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
    themediv.appendChild(tbl);

    div = document.createElement('div');
    div.className = 'spacer';
    themediv.appendChild(div);
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
  req.open("get", "theme.json?start=" + start, true);

  req.onload = function ()
  {
    var jsonResponse = JSON.parse(req.responseText);
    fillTable(jsonResponse);
  };
  req.send(null);
}

function page_onload()
{
  set_background();
  fetchData(0);
  colourChooser();
}

function colourChooser()
{
  var preInput = '';
  var paletteHTML = generateHTML();
  var selPalette = 0;
  var colbox = false;

  function fetchPalette()
  {
    var palettes = new Array();

    palettes[0] = [
              ["000000","FFFFFF","EA80FC","B388FF","8C9EFF","82B1FF","80D8FF","84FFFF"],
              ["FF5252","FF4081","E040FB","7C4DFF","536DFE","448AFF","40C4FF","18FFFF"],
              ["FF1744","F50057","D500F9","651FFF","3D5AFE","2979FF","00B0FF","00E5FF"],
              ["D50000","C51162","AA00FF","6200EA","304FFE","2962FF","0091EA","00B8D4"],
              ["DD2C00","FF6D00","FFAB00","FFD600","AEEA00","64DD17","00C853","00BFA5"],
              ["FF3D00","FF9100","FFC400","FFEA00","C6FF00","76FF03","00E676","1DE9B6"],
              ["FF6E40","FFAB40","FFD740","FFFF00","EEFF41","B2FF59","69F0AE","64FFDA"],
              ["FF9E80","FFD180","FFE57F","FFFF8D","F4FF81","CCFF90","B9F6CA","A7FFEB"]
            ];

    return palettes;
  };
  function generateHTML()
  {
    var palettes = fetchPalette();
    var html = new Array();

    for ( var palette in palettes )
    {
      html[palette] = '<div class="colPicker-palette"><input type="text" value="#" id="colRGB" class="colPicker"><section class="tileGrid" id="palette-table">';

      for ( var row in palettes[palette] )
      {
        for ( var col in palettes[palette][row] )
        {
          html[palette] += "<div class='tile' style='background:#"+palettes[palette][row][col]+"' id='"+palettes[palette][row][col]+"'></div>";
        }
      }

      html[palette] += '</section>';
    }

    return html;
  };

  function hex(x) {return ("0" + parseInt(x).toString(16)).slice(-2);};

  function displayPicker(selCol)
  {
    selCol.classList.add("colPicker-wrapper");
    let div = document.createElement('div');
    div.id = "colPicker";
    div.innerHTML = paletteHTML[selPalette];
    selCol.appendChild(div);

    let inp = document.getElementById("colRGB");
    var rgb = selCol.style.backgroundColor;
    var hex_rgb = rgb.match(/^rgb\((\d+),\s*(\d+),\s*(\d+)\)$/);
    if (hex_rgb)
    {
      inp.value = "#" + hex(hex_rgb[1]) + hex(hex_rgb[2]) + hex(hex_rgb[3]);
    }

    inp.oninput = function (ev)
    {
      if ( ev.target.value.substr(0,1) != '#' )
      {
        ev.target.value = '#' + ev.target.value;
      }

      var check = /^#[0-9A-Fa-f]*$/;

      if ( !check.test(ev.target.value) )
      {
        ev.target.value = preInput; // if this value is invalid, restore it to what was valid
      }

      if (  ev.target.value.length > 7 )
      {
        ev.target.value = preInput; // if this value is invalid, restore it to what was valid
      }

      // Update the visible colour, but not the device
      if ( ev.target.value != preInput && (ev.target.value.length == 7) )
      {
        selCol.style.backgroundColor = ev.target.value;
        // ToDo: preview colour
      }
    };

    inp.onkeypress = function (ev)
    {
      var code = ev.keyCode || ev.which;
      ev.keyCode = 65;
      ev.which = 65;
      if ( code == 13 )
      {
        if ( ev.target.value.length == 7 )
        {
          selCol.style.backgroundColor = ev.target.value;
          updateControl('set_palette', `${colbox.id}:${ev.target.value.substr(1,7)}`);
          removePicker();
        }
      }
      preInput = ev.target.value; //catch this value for comparison in keyup
      return true;
    };
  };

  function removePicker()
  {
    if ( colbox != false )
    {
      colbox.classList.remove("colPicker-wrapper");
      const element = document.getElementById("colPicker");
      element.remove();
    }
    // Allow another instance to run
    colbox = false;
  };

  document.body.onclick = function(e)
  {
    //console.log("e.target.className = " + e.target.className);

    // Do we have a colour picker open?
    if ( colbox != false )
    {
      // Check if we are clicking somewhere on the picker
      let ce = e.target.closest("#colPicker");
      if ( Boolean(ce) )
      {
        //console.log(e.target.className);
        if ( e.target.className == "tile" )
        {
          // Update our colour
          var rgb = e.target.style.backgroundColor;
          colbox.style.backgroundColor = rgb;

          // Update our device
          var hex_rgb = rgb.match(/^rgb\((\d+),\s*(\d+),\s*(\d+)\)$/);
          var color = hex(hex_rgb[1]) + hex(hex_rgb[2]) + hex(hex_rgb[3]);
          updateControl('set_palette', `${colbox.id}:${color}`);

          // Remove the picker from view
          removePicker();
        }
      }
      else if ( colbox != e.target )
      {
        removePicker();
      }
    }

    // Check if we need to open a colour picker
    if ( colbox == false && e.target.className == "colbox" )
    {
      colbox = e.target;
      preInput = e.target.value;

      displayPicker(colbox);
    }
  };
};

document.addEventListener("DOMContentLoaded", function(event)
{
  console.log("Ready!");
});

