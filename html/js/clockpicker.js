


function getOffset(el)
{
  var _x=0;
  var _y=0;
  while(el&&!isNaN(el.offsetLeft)&&!isNaN(el.offsetTop))
  {
    _x+=el.offsetLeft-el.scrollLeft;
    _y+=el.offsetTop-el.scrollTop;
    el=el.offsetParent;
  }
  return {top: _y, left: _x};
}
class clockPicker
{
  /* Constants (will fix) */
  static #height=180;
  static #width=180;
  /* Outer radius of minutes/hours */
  static #outerRadius=73;
  /* Inner radius of hours */
  static #innerRadius=50;
  /* Radius of number segment */
  static #offsetToNumCenter=14;
  /* Index of picker arrays */
  static SEL_HOURS=0;
  static SEL_MINS=1;
  static SEL_NONE=2;
  static #FOC_LINE=0;
  static #SEL_LINE=1;
  /* Shared */
  /**
   * @type {HTMLDivElement|undefined} items
   */
  static #pin=undefined;
  /**
   * @type {clockFace} []
   */
  /**
 * @typedef clockFace
 * @type {Array}
 * @property {Array} items List of items around the clock face
 * @property {*} disp Pointer to the div element that is the clock face
 * @property {Number} sel_tgt Current selected item
 * @property {Number} ini_tgt Initial selected item
 */
  /**
   * @type {clockFace}[] Array of available clock faces
   */
  static #clockFace=[
    {items: undefined, disp: undefined, sel_tgt: 0, ini_tgt: 0},
    {items: undefined, disp: undefined, sel_tgt: 0, ini_tgt: 0}];
  /**
   * @type {Array<HTMLHRElement>} [] The clock hands (focus and select lines)
   */
  static #hands;
  /**
   * @type {HTMLDivElement} The "outer shell" of the clock face
   */
  static #wrapper;
  /**
   * @type {Number} Essentially, the radius of the outer number segments
   */
  static #numOffset;
  /**
   * @type {Number}
   */
  static #offsetToParentCenter;
  static #centre;
  /**
   * @type {Boolean} Left mouse button is held down
   */
  static #mousedown;
  /**
   * @type {Boolean} When #mousedown is true, this will chamge state if the mouse is moved
   */
  static #hasMoved;
  /**
   * @type {HTMLDivElement} The div element the mouse pointer was previously over
   */
  static #pre_tgt;
  /**
   * @type {HTMLDivElement} The div element the mouse is currently over
   */
  static #ovr_tgt;
  /**
   * @type {DOMRect} The dimensions of the clock outer div element
   */
  static #parentRect;
  /**
  * @type {Number} The index of the active view in #clockFace
  */
  static #curFaceNum;
  /**
   * @type {object} A direct link to the active face object
   */
  static #curFace;
  /**
 * @type {HTMLDivElement} The current parent element
 */
  static #parent;
  /**
 * @type {Array<clockPicker>} [] An array of clockPicker class objects
 */
  static #picLst = [];
  /**
   * @type {Number} Index of the current, active, picker. A way to access this from outside the object method
   */
  static #curPicker;
  /**
   * @type {Number} The index of this picker in clockFace.#picLst
   */
  #pickerIndexNum;
  /**
   * @type {HTMLDivElement} The HTML Div element associated with 'this' instance
   */
  #tElement;

    /* ======================================================================================================== */

  /**
   *
   * @param {HTMLDivElement} pElement
   */
  constructor(pElement)
  {
    /* Object manipulation */
    clockPicker.#mousedown=false;
    clockPicker.#hasMoved=false;

    /* Set the face to defaults */
    clockPicker.#curFaceNum=clockPicker.SEL_HOURS;
    clockPicker.#curFace=clockPicker.#clockFace[clockPicker.#curFaceNum];

    /* Create the wrapper and faces for our clock on first invocation. */
    if(clockPicker.#wrapper==undefined)
    {
      this.init();
    }

    /* Should really check for duplication */
    this.#pickerIndexNum=clockPicker.#picLst.length;
    clockPicker.#picLst[this.#pickerIndexNum]=this;
    this.#tElement=pElement;

    /* Check if we are being added to an element */
    this.setInstance(this.#pickerIndexNum);
  }
  init()
  {
    clockPicker.#wrapper=document.createElement("div");
    clockPicker.#wrapper.classList.add("clock", "clock-wrapper");
    clockPicker.#wrapper.style.width=`${clockPicker.#width}px`;
    clockPicker.#wrapper.style.height=`${clockPicker.#height}px`;

    /* Configure some basics for later utility */
    clockPicker.#parentRect=clockPicker.#wrapper.getBoundingClientRect();
    if(clockPicker.#parentRect.width>0)
    {
      clockPicker.#offsetToParentCenter=Math.round(clockPicker.#wrapper.offsetWidth/2);
      clockPicker.#centre={x: clockPicker.#parentRect.left+((clockPicker.#parentRect.width-0)/2), y: clockPicker.#parentRect.top+((clockPicker.#parentRect.height-0)/2)};
    }
    else
    {
      clockPicker.#offsetToParentCenter=Math.round(clockPicker.#width/2);
      clockPicker.#centre={x: Math.round(clockPicker.#height/2), y: Math.round(clockPicker.#width/2)};
    }

    clockPicker.#numOffset=clockPicker.#offsetToParentCenter-clockPicker.#offsetToNumCenter;

    /* Add user interaction (we only need to do this once) */
    clockPicker.#wrapper.addEventListener("click", this);
    clockPicker.#wrapper.addEventListener("dblclick", this);
    clockPicker.#wrapper.addEventListener("mousemove", this);
    clockPicker.#wrapper.addEventListener("mousedown", this);
    clockPicker.#wrapper.addEventListener("mouseup", this);
    clockPicker.#wrapper.addEventListener("mouseleave", this);

    /* Create the centre pin */
    if(clockPicker.#pin==undefined)
    {
      this.create_centre_pin();
    }

    /* Create the focus line */
    if(clockPicker.#hands==undefined)
    {
      clockPicker.#hands=new Array();
      console.log("creating foc_line");
      this.foc_line_create();
      console.log("creating sel_line");
      this.sel_line_create();
    }

    /* Initialise the hours clock face */
    if(clockPicker.#clockFace[clockPicker.SEL_HOURS].disp==undefined)
    {
      console.log("creating hour picker");
      this.create_hourPicker();
    }

    /* Initialise the minutes clock face */
    if(clockPicker.#clockFace[clockPicker.SEL_MINS].disp==undefined)
    {
      console.log("creating mins picker");
      this.create_minsPicker();
    }

    /* Let the end user know what's happening */
    console.log("clockPickerinitialised...");
  }
  /**
   *
   * @param {HTMLElement} element
   */
  hideEl(element)
  {
    element.classList.add("hidden");
  }
  /**
   *
   * @param {HTMLElement} element
   */
  showEl(element)
  {
    element.classList.remove("hidden");
  }
  log()
  {
    console.log(`curInt set: ${clockPicker.#curFace===clockPicker.#clockFace[clockPicker.#curFaceNum]}`);
    console.log(`clockPicker.#curFaceNum: ${clockPicker.#curFaceNum}, items: ${clockPicker.#curFace.items.length}`);
    console.log(`hour face hidden: ${clockPicker.#clockFace[0].disp.classList.contains("hidden")}`)
    console.log(`mins face hidden: ${clockPicker.#clockFace[1].disp.classList.contains("hidden")}`)
  }
  /**
   *
   * @param {Number} face
   */
  selectFace(face)
  {
    let l=clockPicker.#clockFace.length;
    if(face>=0&&face<l)
    {
      for(let i=0; i<l; i++)
      {
        console.log(`Hiding: ${i}`);
        this.hideEl(clockPicker.#clockFace[i].disp);
      }
      clockPicker.#curFaceNum=face;
      clockPicker.#curFace=clockPicker.#clockFace[face];
      this.showEl(clockPicker.#clockFace[face].disp);
      console.log(`selectFace(${face}) done`);
      this.log();
    }
  }
  /**
   *
   * @param {String|null} time
   * @returns
   */
  setTimeFromData(time)
  {
    let indie;
    let timeStr;

    if(time==undefined)
    {
      timeStr=clockPicker.#parent.dataset.value;
    }
    else
    {
      timeStr="00:00";
    }

    try
    {
      indie=timeStr.split(":");
    }
    catch
    {
      return;
    }

    clockPicker.#clockFace[clockPicker.SEL_HOURS].sel_tgt=Number(indie[0]);
    clockPicker.#clockFace[clockPicker.SEL_MINS].sel_tgt=Number(indie[1]);
  }
  /**
  * Save the current selected value (hours|minutes) to dataset.value
  */
  setDataFromTime()
  {
    let indie=[];
    indie[0]=("00"+clockPicker.#clockFace[clockPicker.SEL_HOURS].sel_tgt).slice(-2);
    indie[1]=("00"+clockPicker.#clockFace[clockPicker.SEL_MINS].sel_tgt).slice(-2);
    clockPicker.#parent.dataset.value=`${("00"+indie[0]).slice(-2)}:${("00"+indie[1]).slice(-2)}`;
  }
  /**
   * Configure the class for this instance, by setting the appropriate vars, etc.
   * @param {clockPicker|HTMLDivElement|Number} tInstance
   */
  setInstance=function(tInstance)
  {
    /**
     * @type {clockPicker|undefined}
     */
    let tObj = undefined;
    if(typeof tInstance==="number")
    {
      let l=clockPicker.#picLst.length;
      if(tInstance>=0&&tInstance<l)
      {
        tObj=clockPicker.#picLst[tInstance];
      }
    }
    else if(tInstance instanceof clockPicker)
    {
      clockPicker.#picLst.forEach(itm =>
      {
        if(itm===tInstance)
        {
          tObj=itm;
        }
      });
    }
    else if(tInstance instanceof HTMLDivElement)
    {
      clockPicker.#picLst.forEach(itm =>
      {
        if(itm.#tElement===tInstance)
        {
          tObj=itm;
        }
      });
    }
    /* If we found a match, then perform the requested action */
    if(tObj!==undefined)
    {
      clockPicker.#curPicker=tObj.#pickerIndexNum;
      clockPicker.#parent=tObj.#tElement;
      clockPicker.#parent.appendChild(clockPicker.#wrapper);
      this.setTimeFromData();
    }
  }
  /**
   *
   * @returns {HTMLDivElement}
   */
  getParent=function()
  {
    return clockPicker.#parent;
  }
  /**
   *
   * @returns {Boolean}
   */
  isVisible=function()
  {
    return clockPicker.#wrapper.classList.contains("hidden") === false;
  }
  hide=function()
  {
    this.hideEl(clockPicker.#wrapper);
    this.hideEl(clockPicker.#hands[0]);
    this.hideEl(clockPicker.#hands[1]);
    this.hideEl(clockPicker.#clockFace[0].disp);
    this.hideEl(clockPicker.#clockFace[1].disp);
  }
  /**
   *
   * @param {Number} which
   */
  show=function(which)
  {
    if(which>=0&&which<clockPicker.#clockFace.length)
    {
      this.showEl(clockPicker.#wrapper);
      this.selectFace(which);
      this.focusSelItem(this, clockPicker.#clockFace[which].sel_tgt);
    }
  }
    /* ======================================================================================================== */

  /**
   * Set the current "selected" item.
   * Item is either the index of #curFace.items[] or an object.
   * @param {*} _this
   * @param {Number|object} item
   */
  focusSelItem(_this, item)
  {
    [...document.getElementsByClassName('sel')].forEach(el =>
    {
      el.classList.remove("sel");
    });

    let num=-1;
    if(item!=undefined)
    {
      if(typeof item==="number")
      {
        num=item;
      }
      else if(typeof item==="object"&&typeof item.dataset.value==="string")
      {
        num=Number(item.dataset.value);
      }
    }

    if((num>=0)&&num<clockPicker.#curFace.items.length)
    {
      clockPicker.#curFace.sel_tgt=clockPicker.#curFace.items[num].dataset.value;
      this.showEl(clockPicker.#hands[clockPicker.#SEL_LINE]);
      (clockPicker.#curFace.items[num]).classList.add("sel");
      _this.adjustLine(document.getElementById('pin'), clockPicker.#curFace.items[num], clockPicker.#hands[clockPicker.#SEL_LINE]);
    }
    else
    {
      this.hideEl(clockPicker.#hands[clockPicker.#SEL_LINE]);
    }
    console.log(`focusSelItem(${num})`);
    _this.log();
  }
  create_centre_pin()
  {
    clockPicker.#pin=document.createElement("div");
    clockPicker.#pin.classList.add("clock", "pin");
    clockPicker.#pin.id=("pin");
    /* We need to take into account pin dimensions */
    clockPicker.#pin.style.left=`${clockPicker.#offsetToParentCenter}px`;
    clockPicker.#pin.style.top=`${clockPicker.#offsetToParentCenter}px`;
    clockPicker.#wrapper.appendChild(clockPicker.#pin);
  }
    /* ======================================================================================================== */

  foc_line_create()
  {
    clockPicker.#hands[clockPicker.#FOC_LINE]=document.createElement('hr');
    this.hideEl(clockPicker.#hands[clockPicker.#FOC_LINE]);
    clockPicker.#hands[clockPicker.#FOC_LINE].classList.add("clock", "clock_hand", "foc_line");
    clockPicker.#hands[clockPicker.#FOC_LINE].id=("foc_line");
    clockPicker.#wrapper.appendChild(clockPicker.#hands[clockPicker.#FOC_LINE]);
  }
    /* ======================================================================================================== */

  sel_line_create()
  {
    clockPicker.#hands[clockPicker.#SEL_LINE]=document.createElement('hr');
    this.hideEl(clockPicker.#hands[clockPicker.#SEL_LINE]);
    clockPicker.#hands[clockPicker.#SEL_LINE].classList.add("clock", "clock_hand", "sel_line");
    clockPicker.#hands[clockPicker.#SEL_LINE].id=("sel_line");
    clockPicker.#wrapper.appendChild(clockPicker.#hands[clockPicker.#SEL_LINE]);
  }
    /* ======================================================================================================== */

  foc_line_hide()
  {
    [...document.getElementsByClassName('focus')].forEach(el =>
    {
      el.classList.remove("focus");
    });
    let line=document.getElementById('foc_line');
    if(line!=undefined)
    {
      this.hideEl(line);
    }
  }
  /**
   *
   * @param {*} _this
   * @param {Number} target
   */
  foc_line_show(_this, target)
  {
    if(_this!=undefined&&target!=undefined)
    {
      /* Only show the "focus"/"hover" line when not over the selected item */
      if(clockPicker.#curFace.items[clockPicker.#curFace.sel_tgt]!==clockPicker.#ovr_tgt&&!clockPicker.#mousedown)
      {
        (clockPicker.#curFace.items[target]).classList.add("focus");
        _this.adjustLine(document.getElementById('pin'), clockPicker.#curFace.items[target], clockPicker.#hands[clockPicker.#FOC_LINE]);
        _this.showEl(clockPicker.#hands[clockPicker.#FOC_LINE]);
      }
      else
      {
        _this.hideEl(clockPicker.#hands[clockPicker.#FOC_LINE]);
      }
    }
  }
  /**
   * Show "selection" line. Target is the index of clockPicker.#curFace.items[]
   * @param {*} _this
   * @param {Number} target
   */
  sel_line_show(_this, target)
  {
    if(_this!=undefined&&target!=undefined)
    {
      [...document.getElementsByClassName('sel')].forEach(el =>
      {
        el.classList.remove("sel");
      });
      (clockPicker.#curFace.items[target]).classList.add('sel');
      _this.adjustLine(document.getElementById('pin'), clockPicker.#curFace.items[target], clockPicker.#hands[clockPicker.#SEL_LINE]);
      _this.showEl(clockPicker.#hands[clockPicker.#SEL_LINE]);
    }
  }
    /* ======================================================================================================== */

  adjustLine(from, to, line)
  {
    if(!from||!to||!line)
    {
      return;
    }

    let a={x: (from.offsetTop+(from.offsetHeight/2))-1, y: (from.offsetLeft+(from.offsetWidth/2))-2};
    let b={x: (to.offsetTop+(to.offsetHeight/2))-1, y: (to.offsetLeft+(to.offsetWidth/2))-2};

    let CA=Math.abs(b.x-a.x);
    let CO=Math.abs(b.y-a.y);
    let H=Math.sqrt(CA*CA+CO*CO);
    let ANG=180/Math.PI*Math.acos(CA/H);

    let br=to.getBoundingClientRect();
    const x=br.x-clockPicker.#centre.x;
    const y=br.y-clockPicker.#centre.y;
    const nx=x*Math.cos(-90*Math.PI/180)-y*Math.sin(-90*Math.PI/180);
    const ny=x*Math.sin(-90*Math.PI/180)+y*Math.cos(-90*Math.PI/180);
    const angle=Math.atan2(ny, nx)+Math.PI;

    if(b.x>a.x)
    {
      var top=(b.x-a.x)/2+a.x;
    }
    else
    {
      var top=(a.x-b.x)/2+b.x;
    }

    if(b.y>a.y)
    {
      var left=(b.y-a.y)/2+a.y;
    }
    else
    {
      var left=(a.y-b.y)/2+b.y;
    }

    if((a.x<b.x&&a.y<b.y)||(b.x<a.x&&b.y<a.y)||(a.x>b.x&&a.y>b.y)||(b.x>a.x&&b.y>a.y))
    {
      ANG*=-1;
    }

    top-=H/2;

    line.style["-webkit-transform"]='rotate('+ANG+'deg)';
    line.style["-moz-transform"]='rotate('+ANG+'deg)';
    line.style["-ms-transform"]='rotate('+ANG+'deg)';
    line.style["-o-transform"]='rotate('+ANG+'deg)';
    line.style["-transform"]='rotate('+ANG+'deg)';
    line.style.top=top+'px';
    line.style.left=left+'px';
    line.style.height=H+'px';
  }
    /* ======================================================================================================== */

  getDistance(x1, y1, x2, y2)
  {
    let y=x2-x1;
    let x=y2-y1;
    return Math.sqrt(x*x+y*y);
  }
    /* ======================================================================================================== */

  roundNumber(number, digits)
  {
    var multiple=Math.pow(10, digits);
    var rndedNum=Math.round(number*multiple)/multiple;
    return rndedNum;
  }
  /**
   *
   *
   * @param {Number} div
   * @param {Number} i
   * @param {Number} r
   * @returns
   */
  get_seg_xy(div, i, r)
  {
    return {
      x: Math.cos((div*i+-90)*(Math.PI/180))*r,
      y: Math.sin((div*i+-90)*(Math.PI/180))*r
    };
  }
    /* ======================================================================================================== */

  create_minsPicker()
  {
    let div=360/60;
    clockPicker.#clockFace[clockPicker.SEL_MINS].disp=document.createElement("div");
    let wrapper=clockPicker.#clockFace[clockPicker.SEL_MINS].disp;
    wrapper.classList.add("clock", "face-wrapper");
    this.showEl(wrapper);
    clockPicker.#wrapper.append(wrapper);

    clockPicker.#clockFace[clockPicker.SEL_MINS].items=new Array();
    for(let i=0; i<=59; i++)
    {
      let z=((i%5)===0);
      let segment=document.createElement('div');
      segment.classList.add("clock", "seg");
      segment.dataset.value=i;
      segment.innerText=i;
      segment.style.position='absolute';
      let seg=this.get_seg_xy(div, i, clockPicker.#outerRadius);
      segment.style.top=(seg.y+clockPicker.#numOffset).toString()+"px";
      segment.style.left=(seg.x+clockPicker.#numOffset).toString()+"px";
      if(z)
      {
        segment.classList.add("cardinal");
      }
      clockPicker.#clockFace[clockPicker.SEL_MINS].items[i]=segment;
      wrapper.appendChild(segment);
    }
  }
    /* ======================================================================================================== */

  create_hourPicker()
  {
    /* Draw the outer ring for AM in 24 hour notation */
    let div=360/12;
    clockPicker.#clockFace[clockPicker.SEL_HOURS].disp=document.createElement("div");
    let wrapper=clockPicker.#clockFace[clockPicker.SEL_HOURS].disp;
    wrapper.classList.add("clock", "face-wrapper");
    this.hideEl(wrapper);
    clockPicker.#wrapper.append(wrapper);

    clockPicker.#clockFace[clockPicker.SEL_HOURS].items=new Array();
    for(let i=1; i<=12; i++)
    {
      let segment=document.createElement('div');
      segment.classList.add("clock", "seg");
      segment.style.position='absolute';
      segment.dataset.value=i;
      segment.innerText=i;
      let seg=this.get_seg_xy(div, i, clockPicker.#outerRadius);
      segment.style.top=(seg.y+clockPicker.#numOffset).toString()+"px";
      segment.style.left=(seg.x+clockPicker.#numOffset).toString()+"px";
      segment.classList.add("cardinal");
      clockPicker.#clockFace[clockPicker.SEL_HOURS].items[i]=segment;
      wrapper.appendChild(segment);
    }
    /* Draw the inner ring for PM in 24 hour notation */
    for(let i=13; i<=24; i++)
    {
      let v=i;
      if(v===24)
      {
        v=0;
      }
      let segment=document.createElement('div');
      segment.classList.add("clock", "seg");
      segment.style.position='absolute';
      segment.dataset.value=v;
      segment.innerText=v;
      let seg=this.get_seg_xy(div, i, clockPicker.#innerRadius);
      segment.style.top=(seg.y+clockPicker.#numOffset).toString()+"px";
      segment.style.left=(seg.x+clockPicker.#numOffset).toString()+"px";
      segment.classList.add("cardinal");
      clockPicker.#clockFace[clockPicker.SEL_HOURS].items[v]=segment;
      wrapper.appendChild(segment);
    }
  }
  /* ======================================================================================================== */
  onclick(_this, event)
  {
    let el=event.target;

    /* Loose selection (mouse can be clicked anywhere in the clock face)*/
    if(el.classList.contains("clock")&&clockPicker.#ovr_tgt!=undefined)

    /* Focused selection (mouse must be clicked over desired field) */
    /* Note: Best used with double click to perform loose selection */
    //if (el.classList.contains("seg") && clockPicker.#ovr_tgt !== undefined)
    {
      _this.focusSelItem(_this, clockPicker.#ovr_tgt);
      _this.setDataFromTime();
      _this.foc_line_hide();
    }
  }
  /* ======================================================================================================== */

  ondblclick(_this, event)
  {
    let el=event.target;
    if(el.classList.contains("clock")&&clockPicker.#ovr_tgt!=undefined)
    {
      _this.focusSelItem(_this, clockPicker.#ovr_tgt);
      _this.setDataFromTime();
    }
  }
  /* ======================================================================================================== */

  onmousedown(_this, event)
  {
    if(event.target.classList.contains("seg"))
    {
      clockPicker.#mousedown=true;
      clockPicker.#hasMoved=false;
      clockPicker.#curFace.ini_tgt=event.target;
    }
  }
  /* ======================================================================================================== */
  /* On mouse move */
  /* ======================================================================================================== */
  onmousemove(_this, event)
  {
    /* Stop drag and select */
    window.getSelection().removeAllRanges();

    let el=event.target;

    /* Check and mark movement with the mouse down */
    if(clockPicker.#mousedown===true)
    {
      if(clockPicker.#curFace.ini_tgt!==el)
      {
        clockPicker.#hasMoved=true;
      }
    }

    /* No point processing if we don't have any items in the list */
    if(!clockPicker.#curFace.items.length||clockPicker.#curFace.items.length<1)
    {
      return;
    }
    //console.log(`(${event.offsetX},${event.offsetY})(${event.clientX},${event.clientY})(${event.pageX},${event.pageY})(${event.screenX},${event.screenX})`);

    let hover={x: 0, y: 0};
    if(el.classList.contains("clock-wrapper")===false)
    {
      hover={x: el.offsetLeft+event.offsetX, y: el.offsetTop+event.offsetY};
    }
    else
    {
      hover={x: event.offsetX, y: event.offsetY};
    }

    /* Should never not be true, but lets make sure */
    if(el.classList.contains("clock"))
    {
      // Math, something I suck at
      const x=hover.x-clockPicker.#centre.x;
      const y=hover.y-clockPicker.#centre.y;
      const nx=x*Math.cos(-90*Math.PI/180)-y*Math.sin(-90*Math.PI/180);
      const ny=x*Math.sin(-90*Math.PI/180)+y*Math.cos(-90*Math.PI/180);
      const angle=Math.atan2(ny, nx)+Math.PI;
      let z;
      if(clockPicker.#clockFace[clockPicker.#curFaceNum].items.length===60)
      {
        console.log("mins: "+clockPicker.#curFaceNum);
        z=_this.roundNumber(((Math.PI*angle)*3.0), 0);
      }
      else if(clockPicker.#clockFace[clockPicker.#curFaceNum].items.length===24)
      {
        console.log("hours: "+clockPicker.#curFaceNum);
        z=_this.roundNumber(((Math.PI*angle)*.6), 0);
        if(z===0)
        {
          z=12;
        }
        if(z<clockPicker.#clockFace[clockPicker.#curFaceNum].items.length)
        {
          let xPos=hover.x-clockPicker.#parentRect.left;
          let yPos=hover.y-clockPicker.#parentRect.top;
          let dist=_this.getDistance(xPos, yPos, clockPicker.#offsetToParentCenter, clockPicker.#offsetToParentCenter);
          if(dist<65)
          {
            z+=12;
          }
          if(z===24)
          {
            z=0;
          }
        }
      }

      /* Sanity check */
      if(isNaN(z)===true||z>clockPicker.#clockFace[clockPicker.#curFaceNum].items.length)
      {
        return;
      }

      clockPicker.#ovr_tgt=clockPicker.#clockFace[clockPicker.#curFaceNum].items[z];
      if(clockPicker.#pre_tgt!==clockPicker.#ovr_tgt)
      {
        /* Hide the last position */
        _this.foc_line_hide();
        /* Show the new position */
        _this.foc_line_show(_this, z);

        if((clockPicker.#mousedown)&&el.classList.contains("clock"))
        {
          _this.sel_line_show(_this, z);
        }
        clockPicker.#pre_tgt=clockPicker.#ovr_tgt;
      }
    }
  }
  onmouseleave(_this, event)
  {
    if(clockPicker.#mousedown===true)
    {
      _this.focusSelItem(_this, clockPicker.#curFace.ini_tgt);
      clockPicker.#mousedown=false;
      clockPicker.#hasMoved=false;
    }
    _this.foc_line_hide();
  }
    /* ======================================================================================================== */

  handleEvent(event)
  {
    /* Prevent default actions, regardless of what we do next */
    event.preventDefault();

    /* Check if we have a function for this event */
    let obj=this["on"+event.type];
    if(typeof obj==="function")
    {
      obj(this, event);
    }
    /* Else, we run a default command */
    else
    {
      clockPicker.#mousedown=false;
    }
  }
}


/* Initialise with parent div and options (work in progress) */

//let x=new clockPicker(document.getElementById("pdiv"));

