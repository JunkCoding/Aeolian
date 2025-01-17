/* jshint esversion: 10 */
"use strict";

/* =========================================================== */
class timesel
{
  /**
   * @type {Array<timesel>} clkgrp
   */
  clkgrp=[];
  /**
   * @type {Boolean}
   */
  static #initialised=false;
  /**
  * @type {Array<timesel>} [] An array of timesel class objects
  */
  static #selLst=[];
  /**
  * @type {clockPicker} Clock Picker class.
  */
  #picker;
  /**
   * @type {Number}
   */
  #curSel;
  /**
   * @type {HTMLDivElement}
   */
  #tElement;

  /**
   *
   * @param {HTMLDivElement} tElement
   * @returns
   */
  constructor(tElement)
  {
    /* Check for and prevent duplicates */
    let l=this.clkgrp.length;
    for(let i=0; i<l; i++)
    {
      if(this.clkgrp[i].#tElement===tElement)
      {
        return;
      }
    }

    this.#curSel=clockPicker.SEL_NONE;
    this.#tElement=tElement;
    this.#picker;

    this.selId=timesel.#selLst.length;
    timesel.#selLst[this.selId]=this;
    tElement.dataset.selId=String(this.selId);

    this.init();
    this.setTimeFromData(this);

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
  /* =========================================================== */
  init()
  {
    let el=this.#tElement;
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
  /* =========================================================== */
  /**
   * @param {timesel} tEl
   * @returns
   */
  setTimeFromData(tEl)
  {
    let timeStr=["00", "00"];

    if(tEl.#tElement!=undefined)
    {
      if(tEl.#tElement.dataset.value)
      {
        try
        {
          timeStr=tEl.#tElement.dataset.value.split(":");
        }
        catch
        {
          console.error("Invalid time string");
        }
      }

      if(tEl.t&&tEl.m)
      {
        tEl.t.value=("00"+timeStr[0]).slice(-2);
        tEl.m.value=("00"+timeStr[1]).slice(-2);
      }
    }
  }
  /* =========================================================== */
  /**
   *
   * @param {HTMLDivElement} tsDataDiv
   * @returns {Number}
   */
  inputStrToMins(tsDataDiv)
  {
    let retVal=0;
    /**
     * @type {HTMLInputElement|null}
     */
    let sH=tsDataDiv.querySelector("input[name=hours]");
    /**
    * @type {HTMLInputElement|null}
    */
    let sM=tsDataDiv.querySelector("input[name=mins]");
    if(sH!=undefined&&sM!=undefined)
    {
      retVal=(Number(sH.value)*60)+Number(sM.value);
    }
    return retVal;
  }
  /* =========================================================== */
  /**
   *
   * @param {HTMLDivElement} tsDataDiv
   * @param {Number} mins
   * @returns {Number}
   */
  minsToInputStr(tsDataDiv, mins)
  {
    let retVal=0;
    /**
     * @type {HTMLInputElement|null}
     */
    let sH=tsDataDiv.querySelector("input[name=hours]");
    /**
    * @type {HTMLInputElement|null}
    */
    let sM=tsDataDiv.querySelector("input[name=mins]");
    if(sH!=undefined&&sM!=undefined)
    {
      /* Set the start inputs and parent dataset values */
      sH.value=("00"+String(Math.floor(mins/60))).slice(-2);
      sM.value=("00"+String(Math.floor(mins%60))).slice(-2);
      tsDataDiv.dataset.value=`${sH.value}:${sM.value}`;
    }
    return retVal;
  }
  /* =========================================================== */
  /**
   *
   * @param {timesel} tsObj
   * @param {HTMLInputElement} inpEl
   * @returns
   */
  validate_time(tsObj, inpEl)
  {
    /* Store the current value without leading zeros */
    let tmpStr=inpEl.value.replace(/\b0+/g, "");

    /* Are we actually interested in this element? */
    if(!inpEl.classList.contains("time"))
    {
      return;
    }

    // Check content is numeric */
    let isNum=tmpStr.match(/^[0-9]+$/)!=null;

    /* Check if we are a start or emd time */
    /**
     * @type {HTMLDivElement} ts
     */
    const ts=closest(inpEl, "[data-type='timeStart']");
    /**
     * @type {HTMLDivElement} te
     */
    const te=closest(inpEl, "[data-type='timeEnd']");

    /* If both are not null, we need to constrain ourselves */
    if(ts!=undefined&&te!=undefined)
    {
      let tsm=this.inputStrToMins(ts);
      let tem=this.inputStrToMins(te);
      console.log(`tsm: ${tsm}, tem: ${tem}`)
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
          if(ts===inpEl.parentNode)
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
      this.minsToInputStr(ts, tsm);
      this.minsToInputStr(te, tem);
    }
    /* ELSE, deal with time selectos that aren't linked start/end times */
    /* (I don't have any need for this now, so in no rush to implement) */
  }
  /* =========================================================== */
  /**
   *
   * @param {timesel} _this
   * @param {*} event
   * @returns
   */
  onkeypress(_this, event)
  {
    let inpEl=event.target;

    /* Store the current value without leading zeros */
    let tmpStr=inpEl.value.replace(/\b0+/g, "");

    /* Are we actually interested in this element? */
    if(!inpEl.classList.contains("time"))
    {
      return;
    }

    // Check content is numeric */
    let isNum=tmpStr.match(/^[0-9]+$/)!=null;

    /* ToDo: Allow 0* & 1* & 2[0-3] */
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
      inpEl.value=tmpStr;
    }

    _this.validate_time(_this, inpEl);
  }
  /* =========================================================== */
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
      _this.#picker=new clockPicker(_this.#tElement);
      _this.#curSel=clockPicker.SEL_NONE;
    }
    else if (_this.#picker.getParent()!==_this.#tElement)
    {
      /* Need to load values if transferred from elsewhere */
      _this.#curSel=clockPicker.SEL_NONE;
      _this.#picker.setInstance(_this.#picker);
    }
    else
    {
      closeView=_this.#picker.isVisible();
    }

    /* We will close the clock picker when closeView is true and we are on the "minutes" picker */
    /* Logic: We will always end on minutes, and never ask for hours without minutes */
    if(closeView&&_this.#curSel===clockPicker.SEL_MINS)
    {
      /* ToDo: Update the local minutes from the picker */

      _this.#curSel=clockPicker.SEL_NONE;
      _this.#picker.hide();
      _this.setTimeFromData(_this);
    }
    else
    {
      /* We are opening a picker, determine which one it is */
      if(_this.#curSel===clockPicker.SEL_HOURS)
      {
        /* ToDo: Update local hours from the picker */
        _this.#curSel=clockPicker.SEL_MINS;
      }
      else
      {
        /* ToDo: ... */
        _this.#curSel=clockPicker.SEL_HOURS;
      }

      /* Make it so */
      _this.#picker.show(_this.#curSel);
    }

    //_this.validate_time(_this, event.target);
  }
  /* =========================================================== */
  onwheel(tgt, event)
  {
    const delta=Math.sign(event.deltaY);
    let inpEl=event.target;
    if(inpEl.classList.contains("time"))
    {
      if(inpEl.name==="hours"||inpEl.name==="mins")
      {
        inpEl.value=String(Number(inpEl.value)-delta);
        tgt.validate_time(tgt, inpEl);
      }
    }
  }
  /* =========================================================== */
  handleEvent(event)
  {
    /**
     * {timesel} _this
     */
    let _this;

    /* Is this object interesting for us? */
    if(event.target instanceof HTMLInputElement||event.target instanceof HTMLDivElement)
    {
      if(event.target.classList.contains("time")||event.target.classList.contains("clock"))
      {
        let tsObj=closest(event.target, ".timesel");
        _this=timesel.#selLst[tsObj.dataset.selId];
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
      _this.validate_time(_this, event.target);
    }
  }
}
