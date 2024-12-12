// test
// --------------------------------------------------------------------
// Fancy Drop Down Box
// --------------------------------------------------------------------
var ddlist = [];
var ddgrp = [];
/*****************************************************************/
/*****************************************************************/
function _ (el)
{
  return document.getElementById(el);
}
/*****************************************************************/
/* Initialise all dropboxes                                      */
/*****************************************************************/
function init_all_dropboxex ()
{
  // Remove any existing dropDown class associations
  // ------------------------------------------------
  let g = ddgrp.length;
  if (g > 0)
  {
    for (let i = 0; i < g; i++)
    {
      let oldNode = document.getElementById(ddgrp[i].id);
      let newNode = oldNode.cloneNode(true);
      oldNode.parentNode.insertBefore(newNode, oldNode);
      oldNode.parentNode.removeChild(oldNode);
      ddgrp[i] = undefined;
    }
    ddgrp = [];
  }

  ddlist = document.getElementsByClassName("ddlist");
  let l = ddlist.length;
  for (let i = 0; i < l; i++)
  {
    ddgrp[i] = new dDropDown(ddlist[i]);
  }
}

/*****************************************************************/
/* Initialise a single dropbox (more effort than above)          */
/*****************************************************************/
function init_dropbox (dd)
{
  /* Check it is the right class */
  if (!dd.classList.contains("ddlist"))
  {
    return;
  }

  let g = ddgrp.length;
  if (g > 0)
  {
    for (let i = 0; i < g; i++)
    {
      if (ddgrp[i].id == dd.id)
      {
        ddgrp[i] = undefined;
        ddgrp.splice(i, 1);
      }
    }
  }

  let l = ddlist.length;
  if (l > 0)
  {
    for (let i = 0; i < l; i++)
    {
      if (ddlist[i].id == dd.id)
      {
        ddlist[i] = undefined;
        ddlist.splice(i, 1);
      }
    }
  }

  ddlist[ddlist.length] = dd;
  ddgrp[ddgrp.length] = new dDropDown(dd);
}

/*****************************************************************/
function getChildrenByTag (el, name)
{
  var itmList = [];
  childs = el.childNodes;
  childs.forEach(itm =>
  {
    if (itm.nodeName === name.toUpperCase())
    {
      itmList.push(itm);
    }
  });
  return itmList;
}
function getDropdownItems (el)
{
  var itmList = [];
  childs = el.childNodes;
  childs.forEach(itm =>
  {
    if (itm.nodeName && itm.nodeName === "UL")
    {
      //itmList = itm.children;
      linodes = itm.childNodes;
      linodes.forEach(itz =>
      {
        if (itz.nodeName && itz.nodeName === "LI")
        {
          itmList.push(itz);
        }
      });
    }
  });
  return itmList;
};
/*****************************************************************/
function dDropDown (el)
{
  this.ddgrp = el;
  this.placeholder = getChildrenByTag(this.ddgrp, 'span');
  this.opts = getDropdownItems(this.ddgrp);
  this.val = '';
  this.index = -1;
  this.id = el.id;
  this.initEvents();
}
/*****************************************************************/
dDropDown.prototype = {
  initEvents: function ()
  {
    var obj = this;

    obj.ddgrp.addEventListener('click', function (event)
    {
      if (this.classList.contains('active'))
      {
        this.classList.remove('active');
      }
      else
      {
        /* Make sure no boxes are 'activw' */
        let l = ddlist.length;
        for (let i = 0; i < l; i++)
        {
          if (ddlist[i].classList.contains('active'))
          {
            ddlist[i].classList.remove('active');
          }
        }
        /* Make this one active */
        this.classList.add('active');
      }
    });

    for (var i = 0; i < obj.opts.length; i++)
    {
      obj.opts[i].addEventListener('click', function ()
      {
        set_dd(this.parentNode.parentNode.id, (this.value));
        updateControl(this.parentNode.parentNode.id, (this.value));
      });
    }
  },
  getValue: function ()
  {
    return this.val;
  }
};
/*****************************************************************/
function set_dd (dl, val)
{
  if (isNaN(val))
  {
    console.log("'val' is not a number");
  }
  else
  {
    list = document.getElementById(dl).querySelectorAll("li");
    if (list.length > 0)
    {
      for (var item of list)
      {
        if (val === item.value)
        {
          document.getElementById(dl).querySelector('span').innerHTML = item.textContent;
          break;
        }
      }
    }
  }
}
function setMsg (cls, text)
{
  sbox = document.getElementById('status_box');
  if (sbox !== null)
  {
    sbox.className = "alert alert-" + cls;
    sbox.innerHTML = text;
  }
}
// --------------------------------------------------------------------
// Ajax support
// --------------------------------------------------------------------
function onClickHandler (id)
{
  var xmlhttp = new XMLHttpRequest();

  xmlhttp.onreadystatechange = function ()
  {
    if (this.readyState === 4 && this.status === 200)
    {
      setControl(this);
    }
  };

  var checkBox = document.getElementById(id);
  var val = checkBox.checked ? 1 : 0;

  var request = "config.cgi?" + encodeURI(id + "=" + val);
  xmlhttp.open("GET", request, true);
  xmlhttp.send();
}

function updateControl (param, val)
{
  var xmlhttp = new XMLHttpRequest();

  xmlhttp.onreadystatechange = function ()
  {
    if (this.readyState === 4 && this.status === 200)
    {
      setControl(this);
    }
  };

  var request = "config.cgi?" + encodeURI(param + "=" + val);
  xmlhttp.open("GET", request, true);
  xmlhttp.send();
}

function setControl (xhttp)
{
  var resp = JSON.parse(xhttp.response);
  /*console.log(resp.count);*/

  var elephant = document.getElementById(resp.param);
  if (typeof (elephant) != 'undefined' && elephant != null)
    elephant.value = resp.value;
}

// --------------------------------------------------------------------
// item helper
// --------------------------------------------------------------------
function itemShowHide (_img, _item, op)
{
  // search for img
  var img = document.getElementById(_img);
  var item = document.getElementById(_item);
  if (op === "toggle")
  {
    if (item.style.display === "none")
    {
      item.style.display = "block";
      img.src = "img/arrow_up.png";
    }
    else
    {
      item.style.display = "none";
      img.src = "img/arrow_down.png";
    }
  }
  else if (op === "show")
  {
    item.style.display = "block";
    img.src = "img/arrow_up.png";
  }
  else if (op === "hide")
  {
    item.style.display = "none";
    img.src = "img/arrow_down.png";
  }

  setCookie(_item, item.style.display === "none" ? "hide" : "show", 365);
}

function select_change (selectID)
{
  var select = document.getElementById(selectID);
  //var option = select.options[select.selectedIndex].text;
  var option = select.options[select.selectedIndex].value;

  updateControl(selectID, option);
}

// --------------------------------------------------------------------
// checkbox support
// --------------------------------------------------------------------

// https://www.annedorko.com/toggle-form-field
function checkboxToggle (checkboxID)
{
  var checkbox = document.getElementById(checkboxID);
  var toggle = document.getElementById("hidden_" + checkboxID);
  var updateToggle = checkbox.checked ? toggle.disabled = true : toggle.disabled = false;

  updateControl(checkboxID, updateToggle ? 1 : 0);
}

// --------------------------------------------------------------------
// navbar function
// --------------------------------------------------------------------

// --------------------------------------------------------------------
// Toggle between adding and removing the "responsive" class to topnav
//  when the user clicks on the icon

function navbar ()
{
  var x = document.getElementById("myTopnav");
  if (x.className === "navbar")
  {
    x.className += " responsive";
  }
  else
  {
    x.className = "navbar";
  }
}

/*****************************************************************/
/* Capture clicks outside our area of interest to close lists    */
/*****************************************************************/
document.addEventListener("DOMContentLoaded", function (event)
{
  console.log("Ready!");
  document.body.onclick = function (e)
  {
    console.log("e.target.className = " + e.target.className);

    if (!e.target.closest('.ddlist'))
    {
      for (var i = 0; i < ddlist.length; i++)
      {
        if (ddlist[i].classList.contains('active'))
        {
          ddlist[i].classList.remove('active');
        }
      }
    }
  };
});
/*****************************************************************/
/*****************************************************************/
function sleep (ms)
{
  return new Promise(resolve => setTimeout(resolve, ms));
}
/*****************************************************************/
/*****************************************************************/
function openBusyMesg (mesg)
{
  _("busy-wrapper").style.display = 'block';
  _("busy-message").innerText = mesg
}
function closeBusyMesg ()
{
  _("busy-wrapper").style.display = 'none';
}