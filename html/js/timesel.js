/* jshint esversion: 10 */

var clklst=[];
var clkgrp=[];

/* =========================================================== */
function init_timesel()
{
  // Remove any existing class associations
  // ------------------------------------------------
  let g=clkgrp.length;
  if(g>0)
  {
    for(let i=0; i<g; i++)
    {
      let oldNode=document.getElementById(clkgrp[i].id);
      let newNode=oldNode.cloneNode(true);
      oldNode.parentNode.insertBefore(newNode, oldNode);
      oldNode.parentNode.removeChild(oldNode);
      clkgrp[i]=undefined;
    }
    clkgrp=[];
  }

  clklst=document.getElementsByClassName("timesel");
  let l=clklst.length;
  for(let i=0; i<l; i++)
  {
    clklst[i]=new timesel(clklst[i]);
  }
}

function append_timesel(el)
{
  let l=clkgrp.length;
  for(let i=0; i<l; i++)
  {
    if(clkgrp[i].id===el.id)
    {
      return;
    }
  }
  clkgrp[l]=new timesel(el);
}

class timesel
{
  constructor(ctarget)
  {
    this.ctarget=ctarget;
    this.init();
    ctarget.addEventListener("click", this);
    ctarget.addEventListener("change", this);
    ctarget.addEventListener("input", this);
    ctarget.addEventListener("keypress", this);
    ctarget.addEventListener("wheel", this);
  }
  init()
  {
    var tar;
    let el=this.ctarget;
    try
    {
      tar=el.getAttribute("data-value").split(":");
    }
    catch
    {
      return false;
    }
    let t=document.createElement("input");
    t.classList.add("time");
    t.name="hours";
    t.value=("00"+tar[0]).slice(-2);
    el.appendChild(t);

    let p=document.createElement("div");
    p.classList.add("time", "divider", "no-select");
    p.innerHTML=":";
    el.appendChild(p);

    let m=document.createElement("input");
    m.classList.add("time");
    m.name="mins";
    m.value=("00"+tar[1]).slice(-2);
    el.appendChild(m);
  }
  validate_time(event)
  {
    let el=event.target;

    /* Stotr the current value without leading zeros */
    let tmpStr=el.value.replace(/\b0+/g, "");

    /* Are we actually interested in this element? */
    if(!el.classList.contains("time"))
    {
      return;
    }

    // Check content is numeric */
    let isNum=tmpStr.match(/^[0-9]+$/)!=null;

    /* Check for keypress */
    if(typeof event.keyCode!=="undefined")
    {
      if(event.keyCode<48||event.keyCode>57)
      {
        return;
      }
      if(tmpStr.length<2)
      {
        tmpStr+=event.key;
      }
      else
      {
        tmpStr=event.key;
      }
      el.value=tmpStr;
    }

    /* Check if we are a start or emd time */
    const ts=closest(el, "[data-type='timeStart']");
    const te=closest(el, "[data-type='timeEnd']");

    /* If both are not null, we need to constrain ourselves */
    if(ts!==null&&te!==null)
    {
      let tsm=(Number(ts.querySelector("input[name=hours]").value)*60)+Number(ts.querySelector("input[name=mins]").value);
      let tem=(Number(te.querySelector("input[name=hours]").value)*60)+Number(te.querySelector("input[name=mins]").value);

      /* Check our start time is within bounds */
      if(tsm<0||tsm>1439)
      {
        tsm=0;
      }

      /* Check our end time doesn"t overflow */
      if(tem>=1439)
      {
        tem=0;
      }

      /* Only process further if one our times is *not* zero */
      if(!(!tsm&&!tem))
      {
        /* Now check our times are in the correct order */
        if((tsm>=tem)&&tem)
        {
          /* Check if "ts" is the current element */
          if(ts===el.parentNode)
          {
            if(tem>0)
            {
              tsm=tem-1;
            }
            else
            {
              tsm=0;
            }
          }
          /* "te" is our current element, so modify its value */
          else
          {
            if(tem<0)
            {
              tem=1439;
            }
            else if(tem<1439)
            {
              tem=tsm+1;
            }
          }
        }
      }

      ts.querySelector("input[name=hours]").value=String(Math.floor(tsm/60)).padStart(2, "0");
      ts.querySelector("input[name=mins]").value=String(Math.floor(tsm%60)).padStart(2, "0");

      te.querySelector("input[name=hours]").value=String(Math.floor(tem/60)).padStart(2, "0");
      te.querySelector("input[name=mins]").value=String(Math.floor(tem%60)).padStart(2, "0");
    }
    else
    {
      /* Deal with individual needs of hours/minutes */
      if(el.name==="hours")
      {
        /* ToDo */
      }
      else if(el.name==="mins")
      {
        /* ToDo */
      }
      else
      {
        el.value=("00"+tmpStr).slice(-2);
      }
    }
  }
  onclick(tgt, event)
  {
    //console.log(`event: ${event.type}, ${event.target.id}`);
    //event.target.removeEventListener(event.type, this);
  }
  onwheel(tgt, event)
  {
    const delta=Math.sign(event.deltaY);
    let el=event.target;
    if(el.classList.contains("time"))
    {
      if(el.name==="hours"||el.name==="mins")
      {
        el.value=String(Number(el.value)-delta);
        tgt.validate_time(event);
      }
    }
  }
  handleEvent(event)
  {
    event.preventDefault();
    let obj=this["on"+event.type];
    if(typeof obj==="function")
    {
      //console.log("Function exists for ""+event.type+"" ("+event.target.id+")");
      obj(this, event);
    }
    else
    {
      this.validate_time(event);
    }
  }
}
init_timesel();
