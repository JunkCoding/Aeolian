/* jshint esversion: 10 */
"use strict";

/**
 * @type {Array<timesel|undefined>} clkgrp
 */
var clkgrp=[];

/* =========================================================== */
function init_timesel()
{
  /**
 * @type {Array<HTMLDivElement|undefined>} clklst
 */
  var clklst=[];
  // Remove any existing class associations
  // ------------------------------------------------
  let g=clkgrp.length;
  if(g>0)
  {
    for(let i=0; i<g; i++)
    {
      let oldNode=document.getElementById(clkgrp[i].id);
      if(oldNode!=undefined&&oldNode.parentNode!=undefined)
      {
        let newNode=oldNode.cloneNode(true);
        oldNode.parentNode.insertBefore(newNode, oldNode);
        oldNode.parentNode.removeChild(oldNode);
        clkgrp[i]=undefined;
      }
    }
    clkgrp=[];
  }

  clklst=document.getElementsByClassName("timesel");
  let l=clklst.length;
  for(let i=0; i<l; i++)
  {
    clkgrp[i]=new timesel(clklst[i]);
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
  /**
   * @type {Boolean}
   */
  static #initialised=false;
  /**
  * @type {Array<timesel>} [] An array of timesel class objects
  */
  static #selLst=[];
  /**
  * @type {clockPicker}
  */
  #picker;

  constructor(tElement)
  {
    this.curSel=clockPicker.SEL_NONE;
    this.tElement=tElement;
    this.#picker;

    this.selId=timesel.#selLst.length;
    timesel.#selLst[this.selId]=this;
    tElement.dataset.selId=this.selId;

    this.init();
    this.setTimeFromData();

    if(!timesel.#initialised)
    {
      timesel.#initialised=true;
      /* We don't want lots of event handlers */
      document.addEventListener("click", this);
      document.addEventListener("change", this);//, {passive: true});
      document.addEventListener("input", this);
      document.addEventListener("keypress", this);
      document.addEventListener("wheel", this);//, {passive: true});
    }
  }
  init()
  {
    let el=this.tElement;
    this.t=document.createElement("input");
    this.t.classList.add("time");
    this.t.name="hours";
    this.t.value="00";
    el.appendChild(this.t);

    let p=document.createElement("div");
    p.classList.add("time", "divider", "no-select");
    p.innerHTML=":";
    el.appendChild(p);

    this.m=document.createElement("input");
    this.m.classList.add("time");
    this.m.name="mins";
    this.m.value="00";
    el.appendChild(this.m);
  }
  setTimeFromData()
  {
    var st;
    let el=this.tElement;
    try
    {
      st=el.dataset.value.split(":");
    }
    catch
    {
      return false;
    }
    this.t.value=("00"+st[0]).slice(-2);
    this.m.value=("00"+st[1]).slice(-2);

  }
  /**
   *
   * @param {timesel} _this
   * @param {*} event
   * @returns
   */
  validate_time(_this, event)
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
    if(ts!=undefined&&te!=undefined)
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
      /* Set the start inputs and parent dataset values */
      let tmpStrA=("00"+String(Math.floor(tsm/60))).slice(-2);
      let tmpStrB=("00"+String(Math.floor(tsm%60))).slice(-2);
      ts.querySelector("input[name=hours]").value=tmpStrA;
      ts.querySelector("input[name=mins]").value=tmpStrB;
      ts.dataset.value=`${tmpStrA}:${tmpStrB}`;

      /* Set the end inputs and parent dataSET VALUES */
      tmpStrA=("00"+String(Math.floor(tem/60))).slice(-2);
      tmpStrB=("00"+String(Math.floor(tem%60))).slice(-2);
      te.querySelector("input[name=hours]").value=tmpStrA;
      te.querySelector("input[name=mins]").value=tmpStrB;
      te.dataset.value=`${tmpStrA}:${tmpStrB}`;
    }
    else
    {
      /* Deal with time selectos that aren't linked start/end times */
      /* (I don't have any need for this now, so in no rush to implement) */
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
  /**
   *
   * @param {timesel} _this
   * @param {Event} event
   */
  onclick(_this, event)
  {
    /* Define our default end state */
    let closeView=false;

    /* Check we have a picker connection */
    if(_this.#picker==undefined)
    {
      _this.#picker=new clockPicker(_this.tElement);
      _this.curSel=clockPicker.SEL_NONE;
    }
    else if (_this.#picker.getParent()!==_this.tElement)
    {
      /* Need to load values if transferred from elsewhere */
      _this.curSel=clockPicker.SEL_NONE;
      _this.#picker.setInstance(_this.#picker);
    }
    else
    {
      closeView=_this.#picker.isVisible();
    }

    /* We will close the clock picker when closeView is true and we are on the "minutes" picker */
    /* Logic: We will always end on minutes, and never ask for hours without minutes */
    if(closeView&&_this.curSel===clockPicker.SEL_MINS)
    {
      /* ToDo: Update the local minutes from the picker */

      _this.curSel=clockPicker.SEL_NONE;
      _this.#picker.hide();
      _this.setTimeFromData();
    }
    else
    {
      /* We are opening a picker, determine which one it is */
      if(_this.curSel===clockPicker.SEL_HOURS)
      {
        /* ToDo: Update local hours from the picker */
        _this.curSel=clockPicker.SEL_MINS;
      }
      else
      {
        /* ToDo: ... */
        _this.curSel=clockPicker.SEL_HOURS;
      }

      /* Make it so */
      _this.#picker.show(_this.curSel);
    }
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
        tgt.validate_time(tgt, event);
      }
    }
  }
  handleEvent(event)
  {
    let _this;

    /* Is this object interesting for us? */
    if(event.target instanceof HTMLInputElement||event.target instanceof HTMLDivElement)
    {
      if(event.target.classList.contains("time")||event.target.classList.contains("clock"))
      {
        let tsEl=closest(event.target, ".timesel");
        _this=timesel.#selLst[tsEl.dataset.selId];
      }
    }

    /* If we don't have "this", we return to sender */
    if(_this==undefined)
    {
      return;
    }

    event.preventDefault();
    let obj=this["on"+event.type];
    if(typeof obj==="function")
    {
      obj(_this, event);
    }
    else
    {
      _this.validate_time(_this, event);
    }
  }
}
