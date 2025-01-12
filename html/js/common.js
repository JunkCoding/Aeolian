/* jshint esversion: 8 */

// --------------------------------------------------------------------
// Fancy Drop Down Box
// --------------------------------------------------------------------

/** @type{Object.<object, Node>} */
let ddlist = [];
/** @type{Object.<object, Node>} */
let ddgrp = [];
/*****************************************************************/
/*****************************************************************/
function _(el)
{
  return document.getElementById(el);
}
// https://stackoverflow.com/questions/175739/how-can-i-check-if-a-string-is-a-valid-number
/** @param {string} str */
function isNumeric(str)
{
  if(typeof str!="string") return false; // we only process strings!
  return !isNaN(Number(str))&& // use type coercion to parse the _entirety_ of the string (`parseFloat` alone does not do this)...
    !isNaN(parseFloat(str)); // ...and ensure strings of whitespace fail
}
/*****************************************************************/
/*****************************************************************/
function get_view_dimensions()
{
  let viewportwidth=0;
  let viewportheight=0;
  // the more standards compliant browsers (mozilla/netscape/opera/IE7) use window.innerWidth and window.innerHeight
  if (typeof window.innerWidth != "undefined")
  {
    viewportwidth=window.innerWidth;
    viewportheight = window.innerHeight;
  }
  // IE6 in standards compliant mode (i.e. with a valid doctype as the first line in the document)
  else if (typeof document.documentElement != "undefined" && typeof document.documentElement.clientWidth != "undefined" && document.documentElement.clientWidth != 0)
  {
    viewportwidth=document.documentElement.clientWidth;
    viewportheight = document.documentElement.clientHeight;
  }
  // older versions of IE
  else
  {
    viewportwidth=document.getElementsByTagName("body")[0].clientWidth;
    viewportheight = document.getElementsByTagName("body")[0].clientHeight;
  }
  console.log("viewport size is " + viewportwidth + "x" + viewportheight);
  return ({x: viewportwidth, y: viewportheight});
}
/*****************************************************************/
/*****************************************************************/
function set_background ()
{
  var c = document.createElement("canvas");
  c.width = 1440;
  c.height = 2560;
  let ctx = c.getContext("2d");
  if(ctx!=undefined)
  {
    const grad=ctx.createLinearGradient(0, 0, c.width/2, c.height/2);

    grad.addColorStop(0, "#000000");
    grad.addColorStop(1, "#000000");
    ctx.fillStyle=grad;
    ctx.fillRect(0, 0, c.width, c.height);
    for(var x=17; x<c.width; x=x+38)
    {
      for(var y=17; y<c.height; y=y+38)
      {
        ctx.beginPath();
        ctx.arc(x, y, 14, 0, 2*Math.PI);
        let i=(x+y)%1400;
        if(i<350)
          ctx.fillStyle="#056DAA";
        else if(i<700)
          ctx.fillStyle="#C20772";
        else if(i<1050)
          ctx.fillStyle="#FEFE06";
        else if(i<1400)
          ctx.fillStyle="#554A50";
        else
          ctx.fillStyle="#056DAA";
        ctx.fill();
      }
    }
    const grd=ctx.createRadialGradient(c.width/2, 200, 450, 900, 0, 800);
    grd.addColorStop(0, "rgba(10,10,10,0.01)");
    grd.addColorStop(1, "rgba(10,10,10,0.5");
    ctx.fillStyle=grd;
    ctx.fillRect(0, 0, c.width, c.height);
    document.body.style.background="url("+c.toDataURL()+")";
    document.body.style.backgroundRepeat="no-repeat";
    document.body.style.backgroundSize="cover";
  }
}
/*****************************************************************/
/* Catch Errors                                                  */
/*****************************************************************/
var handleError=function(err)
{
  console.warn(err);
  return new Response(JSON.stringify({
    code: 400,
    message: "Stupid network Error"
  }));
};
/*****************************************************************/
// https://stackoverflow.com/questions/18663941/finding-closest-element-without-jquery
/*****************************************************************/
/*
const closest=(to, selector) =>
{
}*/
function closest(to, selector)
{
  //let currentElement=document.querySelector(to);
  let currentElement=to;
  let returnElement;

  while(currentElement.parentNode&&!returnElement)
  {
    currentElement=currentElement.parentNode;
    returnElement=currentElement.querySelector(selector);
  }

  return returnElement;
}
/*****************************************************************/
/* Initialise all dropboxes                                      */
/*****************************************************************/
function init_all_dropboxes ()
{
  // Remove any existing dropDown class associations
  // ------------------------------------------------
  let g = ddgrp.length;
  if (g > 0)
  {
    for (let i = 0; i < g; i++)
    {
      let oldNode=document.getElementById(ddgrp[i].id);
      if((oldNode!=undefined)&&oldNode.parentNode)
      {
        let newNode=oldNode.cloneNode(true);
        oldNode.parentNode.insertBefore(newNode, oldNode);
        oldNode.parentNode.removeChild(oldNode);
        ddgrp[i]=undefined;
      }
    }
    ddgrp = [];
  }

  ddlist=document.getElementsByClassName("ddlist");

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
  if (!dd.classList.contains("dropdown-menu"))
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
function getDropdownItems (el)
{
  var itmList = [];
  let childs = el.childNodes;
  childs.forEach(itm =>
  {
    if (itm.nodeName && itm.nodeName === "UL")
    {
      //itmList = itm.children;
      let linodes = itm.childNodes;
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
}
/*****************************************************************/
class dDropDown
{
  constructor(el)
  {
    this.ddgrp=el;
    this.opts=getDropdownItems(this.ddgrp);
    this.val="";
    this.index=-1;
    this.id=el.id;

    el.addEventListener("mouseleave", this);
    var clkr=closest(this.ddgrp, ".dropdown-toggle");
    clkr.addEventListener("click", this);

    for(var i=0; i<this.opts.length; i++)
    {
      this.opts[i].addEventListener("click", this);
    }
  }
  onclick(_this, event)
  {
    let tgt=event.target;
    if(tgt.nodeName==="LI")
    {
      set_dd(_this.id, (tgt.value));
      updateControl(_this.id, (tgt.value));
      closeDropdown();
    }
    else if(event.target.classList.contains("click-dropdown")===true)
    {
      let m=event.target.parentNode.querySelectorAll(".dropdown-menu")[0];
      if(event.target.nextElementSibling.classList.contains("dropdown-active")===true)
      {
        event.target.parentNode.classList.remove("dropdown-open");
        event.target.nextElementSibling.classList.remove("dropdown-active");
      }
      else
      {
        /* Close any other open dropdown items */
        closeDropdown();
        /* Show the dropdown for ths element */
        event.target.parentNode.classList.add("dropdown-open");
        event.target.nextElementSibling.classList.add("dropdown-active");
      }
    }
    else
    {
      if(event.target.classList.contains("dropdown-item"))
      {
        let tgt=event.target;
        _this.value=tgt.value;
        _this.header.innerHTML=sharedSel.options[_this.menu].menu[tgt.value].name;
      }
      closeDropdown();
    }
  }
  onmouseleave(_this, event)
  {
    closeDropdown();
  }
  handleEvent(event)
  {
    event.preventDefault();
    let obj=this["on"+event.type];
    if(typeof obj==="function")
    {
      obj(this, event);
    }
  }
}
/*****************************************************************/
function set_dd (dl, val)
{
  if (isNaN(val))
  {
    console.error("'val' is not a number");
  }
  else if(dl==undefined)
  {
    console.error("'dl' is 'null'");
  }
  else
  {
    let list = document.getElementById(dl).querySelectorAll("li");
    if (list.length > 0)
    {
      for (var item of list)
      {
        if (val === item.value)
        {
          closest(document.getElementById(dl), ".dropdown-toggle").textContent=item.textContent;
          break;
        }
      }
    }
  }
}
function setMsg (cls, text)
{
  let sbox = document.getElementById("status_box");
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
  let xmlhttp = new XMLHttpRequest();
  let val=0;

  xmlhttp.onreadystatechange = function ()
  {
    if (this.readyState === 4 && this.status === 200)
    {
      setControl(this);
    }
  };
  /** @type{Object} */
  let checkBox=document.getElementById(id);
  if(checkBox)
  {
    val=checkBox.checked? 1:0;
  }

  let request = "config.cgi?" + encodeURI(id + "=" + val);
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

  var elephant = document.getElementById(resp.param);
  if (typeof (elephant) != "undefined" && elephant != null)
    elephant.value = resp.value;
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
  let x=document.getElementById("myTopnav");
  if(x!=undefined)
  {
    if(x.className==="navbar")
    {
      x.className+=" responsive";
    }
    else
    {
      x.className="navbar";
    }
  }
}
/*****************************************************************/
/* Close any open drop down boxes (shared function)              */
/*****************************************************************/
function closeDropdown()
{
  [...document.getElementsByClassName("dropdown-open")].forEach(el =>
  {
    el.classList.remove("dropdown-open");
  });
  [...document.getElementsByClassName("dropdown-active")].forEach(el =>
  {
    el.classList.remove("dropdown-active");
  });
}
/*****************************************************************/
/* Capture clicks outside our area of interest to close lists    */
/*****************************************************************/
document.addEventListener("DOMContentLoaded", function (event)
{
  document.body.onclick = function (e)
  {
    if (!e.target.closest(".dropdown-container"))
    {
      closeDropdown();
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
  _("busy-wrapper").style.display = "block";
  _("busy-message").innerText=mesg;
}
function closeBusyMesg ()
{
  _("busy-wrapper").style.display = "none";
}